/*
 * Copyright 2026 Digital Envoy, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ngx_core.h>
#include <ngx_config.h>
#include <ngx_http.h>

#include <stdbool.h>
#include <time.h>
#include <sys/socket.h>

#include <libxml/tree.h>

#include "ngx_http_netacuity_module.h"
#include "../netacuity_fields.h"
#include "../netacuity_api.h"


// The common prefix used by all Nginx NetAcuity variables, to avoid collisions with other Nginx module variables.
#define NA_VARIABLE_NAME_PREFIX "na_"
#define NA_VARIABLE_NAME_PREFIX_LENGTH 3

// The max buffer size (including null-term) for Nginx variable names of NetAcuity fields.
#define MAX_NA_VARIABLE_NAME_SIZE (NA_VARIABLE_NAME_PREFIX_LENGTH + MAX_FIELD_NAME_LEN)


/// Creates a copy of the given (null-terminated) value allocated on the given pool.
static char *get_copy_on_pool(ngx_pool_t *pool, const char* value)
{
    size_t value_size = ngx_strlen(value) + 1;  // +1 for null-term
    char *value_buffer = ngx_pnalloc(pool, value_size);
    if (value_buffer == NULL)
    {
        return NULL;
    }
    ngx_memcpy(value_buffer, value, value_size);
    return value_buffer;
}


/// The create_config_main callback is called to allocate the netacuity "main" config struct.
static void *na_callback_create_config_main(ngx_conf_t *cf)
{
    ngx_conf_log_error(NGX_LOG_DEBUG, cf, 0, "=NA= na_callback_create_config_main(...)");
    srand(time(NULL));
    size_t na_config_size = sizeof(ngx_http_netacuity_conf_t);
    ngx_http_netacuity_conf_t *na_config = ngx_pcalloc(cf->pool, na_config_size);
    if(na_config == NULL) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "=NA= failed to allocate %d bytes for NetAcuity configuration", na_config_size);
        return NULL;
    }
    na_config->netacuity_server_ip = "[undefined]";
    na_config->feature_code_csv = "[undefined]";
    na_config->feature_code_count = 0;
    na_config->feature_code_array = NULL;
    na_config->variable_count = 0;
    na_config->variable_array = NULL;
    na_config->ip_variable_index = 0;
    na_config->error_variable_index = 0;
    na_config->no_error_value = get_copy_on_pool(cf->pool, "");   // "(no error)"
    if (na_config->no_error_value == NULL) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "=NA= failed to allocate bytes for NetAcuity configuration no-error message");
        return NULL;
    }
    na_config->no_error_value_length = ngx_strlen(na_config->no_error_value);
    na_config->not_queried_value = get_copy_on_pool(cf->pool, "(not queried)");
    if (na_config->not_queried_value == NULL) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "=NA= failed to allocate bytes for NetAcuity configuration not-queried message");
        return NULL;
    }
    na_config->not_queried_value_length = ngx_strlen(na_config->not_queried_value);
    return na_config;
}


////////////////////////////////////////////////////////////////////////////////


/// Generates an nginx variable name for the given netacuity field, and stores it into the given variable_name_buffer.
static bool generate_variable_name(char *variable_name_buffer, size_t variable_name_buffer_size, netacuity_field *na_field)
{
    char *field_name = na_field->field_name;
    size_t field_name_length = ngx_strlen(field_name);
    const char *prefix = NA_VARIABLE_NAME_PREFIX;
    size_t prefix_length = ngx_strlen(prefix);
    if (prefix_length + field_name_length >= variable_name_buffer_size)
    {
        return false;  // The given variable_name_buffer_size is too small
    }
    // Add prefix to variable_name
    ngx_memcpy(variable_name_buffer, prefix, prefix_length);
    for (size_t i = 0; i < field_name_length; i++)
    {
        char c = field_name[i];
        // Replace hyphens with underscores (netacuity field-names use hyphens, nginx variable_names use underscores)
        if (c == '-')
        {
            c = '_';
        }
        variable_name_buffer[prefix_length + i] = c;
    }
    variable_name_buffer[prefix_length + field_name_length] = '\0';
    return true;
}


/// Assign the given value (by reference) to the given ngx_http_variable_value_t
static void set_variable_value_reference(ngx_http_request_t *r, ngx_http_variable_value_t *v, char* value, size_t value_length)
{
    v->data = (u_char *)value;
    v->len = value_length;
    v->no_cacheable = 0;
    v->not_found = 0;
}


/// Assign the given value (by copying value) into the given ngx_http_variable_value_t
static bool set_variable_value_copy(ngx_http_request_t *r, ngx_http_variable_value_t *v, const char* value)
{
    size_t value_len = ngx_strlen(value);
    char *value_buffer = ngx_pnalloc(r->pool, value_len + 1);  // +1 for null-term
    if (value_buffer == NULL)
    {
        ngx_log_error(NGX_LOG_ERR, (r->connection)->log, 0, "=NA= failed to allocate %d bytes for variable value", value_len + 1);
        return false;
    }
    ngx_memcpy(value_buffer, value, value_len);
    value_buffer[value_len] = '\0';
    set_variable_value_reference(r, v, value_buffer, value_len);
    return true;
}


/// Callback handler for nginx variable getter that does nothing but provide current state.
static ngx_int_t no_op_getter_handler(ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t id_data)
{
    return NGX_OK;
}


/// Callback handler for nginx variable getter for netacuity fields.
static ngx_int_t na_variable_getter_handler(ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t id_data)
{
    if (v->data != NULL) {
        // Variable value has already been set, return current value
        return NGX_OK;
    }
    // Variable value has not been set, so assign the "not queried" value
    ngx_http_netacuity_conf_t *na_config = ngx_http_get_module_main_conf(r, ngx_http_netacuity_module);
    set_variable_value_reference(r, v, na_config->not_queried_value, na_config->not_queried_value_length);
    return NGX_OK;
}


/// The pre_config callback is called before configuration is read from the nginx conf file.
static ngx_int_t na_callback_pre_config(ngx_conf_t *cf)
{
    ngx_conf_log_error(NGX_LOG_DEBUG, cf, 0, "=NA= na_callback_pre_config(...)");

    // Register all the netacuity fields as nginx variables
    netacuity_field *nf_dict = netacuity_field_dictionary;
    char variable_name_buffer[MAX_NA_VARIABLE_NAME_SIZE];
    for (int i = 0; nf_dict[i].field_id != NA_FLD_NULL; i++)
    {
        char *field_name = nf_dict[i].field_name;
        if (!generate_variable_name(variable_name_buffer, MAX_NA_VARIABLE_NAME_SIZE, nf_dict + i))
        {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                    "=NA= failed to generate ngx variable name for field \"%s\"", field_name);
            return NGX_ERROR;
        }
        ngx_str_t var_name;
        var_name.data = (u_char *)variable_name_buffer;
        var_name.len = ngx_strlen(variable_name_buffer);
        ngx_http_variable_t *var = ngx_http_add_variable(cf, &var_name, NGX_HTTP_VAR_NOCACHEABLE | NGX_HTTP_VAR_CHANGEABLE);
        if (var == NULL)
        {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                    "=NA= failed to register ngx variable \"%s\" for field \"%s\"", var_name.data, field_name);
            return NGX_ERROR;
        }
        var->get_handler = (nf_dict[i].feature_code == 1) ? no_op_getter_handler : na_variable_getter_handler;
        var->data = nf_dict[i].field_id;
        ngx_conf_log_error(NGX_LOG_DEBUG, cf, 0,
                "=NA= registered ngx variable \"%s\" for field \"%s\"", var_name.data, field_name);
    }
    return NGX_OK;
}


////////////////////////////////////////////////////////////////////////////////


/// Returns the count of occurrences of the given character c in the given string str.
static size_t count_character(char *str, char c)
{
    size_t count = 0;
    for (int i = 0; str[i] != '\0'; i++)
    {
        if (str[i] == c)
        {
            count++;
        }
    }
    return count;
}


/// Returns whether the given feature_code is configured for inclusion in the given na_config.
static bool is_feature_code_configured(int feature_code, ngx_http_netacuity_conf_t *na_config)
{
    if (feature_code == 1)
    {
        return true;   //always accept feature-code 1
    }
    for (size_t i = 0; i < na_config->feature_code_count; i++)
    {
        if (na_config->feature_code_array[i] == feature_code)
        {
            return true;
        }
    }
    return false;
}


/// Returns the nginx variable index for the given nginx variable name
static ngx_int_t get_variable_index_for_variable_name(ngx_conf_t *cf, const char *variable_name)
{
    ngx_str_t var_name;
    var_name.data = (u_char *)variable_name;
    var_name.len = ngx_strlen(variable_name);
    ngx_int_t var_index = ngx_http_get_variable_index(cf, &var_name);
    ngx_conf_log_error(NGX_LOG_DEBUG, cf, 0, "=NA= index for variable \"%s\" is %d", var_name.data, var_index);
    return var_index;
}


/// Frees the na_client_t handle when the config pool is destroyed.
static void na_client_pool_cleanup(void *data)
{
    na_client_free((na_client_t *) data);
}


/// The setup_config callback is called when configuration is being read from nginx conf file.
static char *na_callback_setup_config(ngx_conf_t *cf, ngx_command_t *cmd, void *na_conf)
{
    ngx_conf_log_error(NGX_LOG_DEBUG, cf, 0, "=NA= na_callback_setup_config(...)");
    // Process the input configuration from the nginx conf file

    ngx_http_netacuity_conf_t *na_config = (ngx_http_netacuity_conf_t *)na_conf;
    ngx_str_t *config_args = cf->args->elts;

    // Parse and prepare the configured server IP
    na_config->netacuity_server_ip = (char *)(config_args[2].data);

    // Create the reusable client handle for this config (also validates the configured IP)
    na_config->client = na_client_new(na_config->netacuity_server_ip, 126, 2000000);
    if ( !na_config->client )
    {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "=NA= failed to set the NetAcuity Server IP: %s", na_config->netacuity_server_ip);
        return NGX_CONF_ERROR;
    }

    ngx_pool_cleanup_t *cln = ngx_pool_cleanup_add(cf->pool, 0);
    if ( cln == NULL )
    {
        na_client_free(na_config->client);
        na_config->client = NULL;
        return NGX_CONF_ERROR;
    }
    cln->handler = na_client_pool_cleanup;
    cln->data    = na_config->client;

    // Parse and prepare the configured feature-codes
    na_config->feature_code_csv = (char *)(config_args[1].data);
    size_t fc_csv_len = ngx_strlen(na_config->feature_code_csv);
    if (fc_csv_len >= MAX_FEATURE_CSV_LEN)
    {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "=NA= invalid configuration for feature-codes is too long");
        return NGX_CONF_ERROR;
    }
    char delimiter = ',';
    size_t fc_count = count_character(na_config->feature_code_csv, delimiter) + 1;
    size_t fc_array_size = sizeof(*(na_config->feature_code_array)) * fc_count;
    na_config->feature_code_array = ngx_pcalloc(cf->pool, fc_array_size);
    if (na_config->feature_code_array == NULL)
    {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "=NA= failed to allocate %d bytes for feature-code array", fc_array_size);
        return NGX_CONF_ERROR;
    }
    char strtok_buf[MAX_FEATURE_CSV_LEN] = {0};
    strncpy(strtok_buf, na_config->feature_code_csv, fc_csv_len);
    char *strtok_init = strtok_buf;
    char *strtok_temp = NULL;
    for (size_t i = 0; i < fc_count; i++)
    {
        char *fc_str = strtok_r(strtok_init, &delimiter, &strtok_temp);
        strtok_init = NULL;
        int fc = strtol(fc_str, (char **)NULL, 10);
        if (fc <= 0) {
            ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "=NA= invalid feature-code \"%s\" in invalid configuration \"%s\"", fc_str, na_config->feature_code_csv);
            return NGX_CONF_ERROR;
        }
        ngx_conf_log_error(NGX_LOG_DEBUG, cf, 0, "=NA= selecting feature-code %d", fc);
        na_config->feature_code_array[i] = fc;
    }
    na_config->feature_code_count = fc_count;
    ngx_conf_log_error(NGX_LOG_DEBUG, cf, 0, "=NA= selected total of %d feature-codes", fc_count);

    // Count the number of fields that will be expected in query response based on the current configuration.
    // Only fields for selected feature-codes should be expected.
    netacuity_field *nf_dict = netacuity_field_dictionary;
    size_t variable_count = 0;
    for (size_t i = 0; nf_dict[i].field_id != NA_FLD_NULL; i++)
    {
        if (is_feature_code_configured(nf_dict[i].feature_code, na_config))
        {
            ngx_conf_log_error(NGX_LOG_DEBUG, cf, 0, "=NA= selecting field %s", nf_dict[i].field_name);
            variable_count++;
        }
    }
    ngx_conf_log_error(NGX_LOG_DEBUG, cf, 0, "=NA= accepted total of %d fields", variable_count);

    // Create the array of mappings of netacuity-field to nginx-variable-index
    size_t variable_array_size = sizeof(*(na_config->variable_array)) * variable_count;
    na_config->variable_array = ngx_pcalloc(cf->pool, variable_array_size);
    if (na_config->variable_array == NULL)
    {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "=NA= failed to allocate %d bytes for variable array data", variable_array_size);
        return NGX_CONF_ERROR;
    }
    char variable_name[MAX_NA_VARIABLE_NAME_SIZE] = {0};
    int j = 0;
    for (int i = 0; nf_dict[i].field_id != NA_FLD_NULL; i++)
    {
        if (is_feature_code_configured(nf_dict[i].feature_code, na_config))
        {
            if (!generate_variable_name(variable_name, MAX_NA_VARIABLE_NAME_SIZE, nf_dict + i))
            {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "=NA= failed to obtain ngx variable index for field \"%s\"", nf_dict[i].field_name);
                return NGX_CONF_ERROR;
            }
            ngx_int_t var_index = get_variable_index_for_variable_name(cf, variable_name);
            if (ngx_strcmp(nf_dict[i].field_name, "ip") == 0)
            {
                // Keep a direct reference for the "ip" variable index
                na_config->ip_variable_index = var_index;
            }
            else if (ngx_strcmp(nf_dict[i].field_name, "error") == 0)
            {
                // Keep a direct reference for the "error" variable index
                na_config->error_variable_index = var_index;
            }
            na_config->variable_array[j].netacuity_field = &nf_dict[i];
            na_config->variable_array[j].ngx_variable_index = var_index;
            j++;
        }
    }
    na_config->variable_count = variable_count;
    return NGX_CONF_OK;
}


////////////////////////////////////////////////////////////////////////////////


static void generate_random(char *buffer, size_t buffer_size)
{
    const char alpha_numeric_set[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    size_t rand_mod = sizeof(alpha_numeric_set) - 1;
    size_t buffer_length = buffer_size - 1;
    for (size_t i = 0; i < buffer_length; i++)
    {
        buffer[i] = alpha_numeric_set[rand() % rand_mod];
    }
    buffer[buffer_length] = '\0';
}


/// The phase handler is called for a specific phase of each HTTP request targeted by the configuration.
static ngx_int_t na_query_phase_handler(ngx_http_request_t *r)
{
    ngx_log_error(NGX_LOG_DEBUG, (r->connection)->log, 0, "=NA= na_query_phase_handler(...)");
    ngx_http_netacuity_conf_t *na_config = ngx_http_get_module_main_conf(r, ngx_http_netacuity_module);
    ngx_http_variable_value_t *ip_v = ngx_http_get_indexed_variable(r, na_config->ip_variable_index);
    if (ip_v->data != NULL)
    {
        ngx_log_error(NGX_LOG_DEBUG, (r->connection)->log, 0, "=NA= query already performed for this request: %s", ip_v->data);
        return NGX_DECLINED;  // The NetAcuity query has already been handled, so forward to the next handler
    }

    size_t var_count = na_config->variable_count;
    netacuity_variable *var_array = na_config->variable_array;

    char query_ip[IP_STRING_LEN] = {0};
    if (r->connection->addr_text.len >= IP_STRING_LEN)
    {
        ngx_log_error(NGX_LOG_ERR, (r->connection)->log, 0, "=NA= query IP is too long");
        return NGX_ERROR;
    }
    strncpy(query_ip, (char *)r->connection->addr_text.data, r->connection->addr_text.len);
    ngx_log_error(NGX_LOG_DEBUG, (r->connection)->log, 0, "=NA= querying for IP %s", query_ip);

    char trans_id[7];  // 6-character trans-id (of base-36)
    generate_random(trans_id, sizeof(trans_id));

    char na_xml_response[MAX_XML_RESPONSE_SIZE];
    char error_msg[MAX_ERROR_MSG_LEN];
    if (!na_query_xml(na_config->client, query_ip, na_config->feature_code_csv, trans_id, na_xml_response, sizeof(na_xml_response), error_msg))
    {
        ngx_log_error(NGX_LOG_ERR, (r->connection)->log, 0, "=NA= query response error: %s", error_msg);
    }

    xmlDoc *xml_doc = xmlParseDoc((unsigned char *)na_xml_response);
    if (xml_doc == NULL) {
        ngx_log_error(NGX_LOG_ERR, (r->connection)->log, 0, "=NA= failed to parse XML response");
        return NGX_ERROR;
    }
    xmlNode *root_node = xmlDocGetRootElement(xml_doc);
    if (root_node == NULL) {
        ngx_log_error(NGX_LOG_ERR, (r->connection)->log, 0, "=NA= XML response has no root element");
        xmlFreeDoc(xml_doc);
        return NGX_ERROR;
    }

    // Search for the values for all of the expected fields
    for (size_t i = 0; i < var_count; i++)
    {
        char *field_name = var_array[i].netacuity_field->field_name;
        ngx_int_t var_index = var_array[i].ngx_variable_index;
        ngx_http_variable_value_t *v = ngx_http_get_indexed_variable(r, var_index);
        if (var_index == na_config->error_variable_index && error_msg[0] == '\0')
        {
            // Set configured "no error" value for non-errors
            set_variable_value_reference(r, v, na_config->no_error_value, na_config->no_error_value_length);
            continue;
        }
        
        xmlChar *xml_value = xmlGetProp(root_node, (xmlChar *) field_name);
        const char *field_value = (char *) xml_value;
        if (field_value == NULL)
        {
            // Field wasn't present in the response; leave the variable unset so its
            // getter callback falls back to the configured "not queried" value.
            ngx_log_error(NGX_LOG_DEBUG, (r->connection)->log, 0,
                    "=NA= field %s not present in response, skipping variable at index %d", field_name, var_index);
            continue;
        }

        ngx_log_error(NGX_LOG_DEBUG, (r->connection)->log, 0,
                "=NA= assigning value \"%s\" of field %s into variable at index %d", field_value, field_name, var_index);
        if (!set_variable_value_copy(r, v, field_value))
        {
            ngx_log_error(NGX_LOG_ERR, (r->connection)->log, 0, "=NA= failed to set value for variable index %d", var_index);
            xmlFree(xml_value);
            xmlFreeDoc(xml_doc);
            return NGX_ERROR;
        }
        xmlFree(xml_value);
    }
    xmlFreeDoc(xml_doc);
    return NGX_DECLINED; // This phase handler does NOT take ownership of the given request, so forward to the next handler
}


/// The post_config callback is called after configuration has been processed from the nginx conf file.
static ngx_int_t na_callback_post_config(ngx_conf_t *cf)
{
    ngx_conf_log_error(NGX_LOG_DEBUG, cf, 0, "=NA= na_callback_post_config(...)");

    // Attach the netacuity phase handler which will make netacuity queries
    ngx_http_core_main_conf_t *cmcf;
    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
    ngx_http_handler_pt *h;

    // Note:
    // In order to query for IP addresses provided in the X-Forwarded-For HTTP header,
    // the NetAcuity query handler requires the "ngx_http_realip_module", which is called
    // during the NGX_HTTP_POST_READ_PHASE.  The NetAcuity handler must therefore be
    // registered to run for some phase that occurs after the NGX_HTTP_POST_READ_PHASE.
    h = ngx_array_push(&cmcf->phases[NGX_HTTP_SERVER_REWRITE_PHASE].handlers);
    if (h == NULL) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "=NA= failed to register phase handler for NetAcuity queries");
        return NGX_ERROR;
    }
    *h = na_query_phase_handler;
    return NGX_OK;
}


/// The NA module directive array, which holds a subarray for each module directive (in the nginx conf file).
/// The value of this is referenced in the ngx_module_t struct.
static ngx_command_t ngx_http_netacuity_commands[] = {
    {
        ngx_string("netacuity_server_module"), // ngx_str_t    name
        NGX_HTTP_MAIN_CONF | NGX_CONF_TAKE2,   // ngx_uint_t   type
        na_callback_setup_config,              // char        *(*setup) (ngx_conf_t *cf, ngx_command_t *cmd, void *config)
        NGX_HTTP_MAIN_CONF_OFFSET,             // ngx_uint_t   conf
        0,                                     // ngx_uint_t   offset
        NULL                                   // void        *post
    },
    ngx_null_command
};


/// The NA module context, which has hooks for processing the configuration.
/// The address of this is referenced in the ngx_module_t struct.
static ngx_http_module_t ngx_http_netacuity_module_ctx = {
    na_callback_pre_config,                     // ngx_int_t  (*preconfiguration)  (ngx_conf_t *cf)
    na_callback_post_config,                    // ngx_int_t  (*postconfiguration) (ngx_conf_t *cf)
    na_callback_create_config_main,             // void      *(*create_main_conf)  (ngx_conf_t *cf)
    NULL,                                       // char      *(*init_main_conf)    (ngx_conf_t *cf, void *conf)
    NULL,                                       // void      *(*create_srv_conf)   (ngx_conf_t *cf)
    NULL,                                       // char      *(*merge_srv_conf)    (ngx_conf_t *cf, void *prev, void *conf)
    NULL,                                       // void      *(*create_loc_conf)   (ngx_conf_t *cf)
    NULL                                        // char      *(*merge_loc_conf)    (ngx_conf_t *cf, void *prev, void *conf)
};


/// The exposed module which binds the NetAcuity context and commands.
ngx_module_t ngx_http_netacuity_module = {
    NGX_MODULE_V1,
    &ngx_http_netacuity_module_ctx,  //module context
    ngx_http_netacuity_commands,     //module directives
    NGX_HTTP_MODULE,                 //module type
    NULL,                    //init master
    NULL,                    //init module
    NULL,                    //init process
    NULL,                    //init thread
    NULL,                    //exit thread
    NULL,                    //exit process
    NULL,                    //exit master
    NGX_MODULE_V1_PADDING
};
