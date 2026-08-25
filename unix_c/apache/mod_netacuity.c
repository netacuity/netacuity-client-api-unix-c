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

#define APR_WANT_STRFUNC
#include "apr.h"
#include "apr_strings.h"
#include "apr_lib.h"
#include "util_filter.h"

#include "httpd.h"
#include "http_connection.h"
#include "http_config.h"
#include "http_core.h"
#include "http_log.h"
#include "http_protocol.h"

#include "ap_mpm.h"
#include "ap_release.h"

#include <libxml/tree.h>

#include "netacuity_api.h"
#include "mod_netacuity.h"

module AP_MODULE_DECLARE_DATA netacuity_module;

#define VERSION_STRING                  "mod_netacuity/1.3"
#define MODULE_NAME                     "netacuity"
#define NA_DEFAULT_TIMEOUT                 1000

#define NA_ENV_VAR_NAME_PREFIX      "NA_"
#define NA_ENV_VAR_NAME_PREFIX_LEN  3
#define NA_ENV_VAR_NAME_MAX_LEN     (MAX_FIELD_NAME_LEN) + (NA_ENV_VAR_NAME_PREFIX_LEN)

/* Locks/Mutexes */
static const char *lockfile;

/***************************** Apache hook routines **************************/

//****************************************************************************
//
// Function: na_apache_server_config()
//
// Purpose: Allocate a context structure to hold our specific variables across
//          all Apache processes/threads.
//
// Parameters: p    - Apache pool to allocate from.
//             s    - The server to allocate this to (not used).
//
// Returns: The allocated structure.
//
//****************************************************************************
static void *na_apache_server_config(apr_pool_t *p, server_rec *s)
{
    srand(time(NULL));
    na_apache_conf_t *cfg = (na_apache_conf_t *)apr_pcalloc(p, sizeof(na_apache_conf_t));
    return cfg;
}

//****************************************************************************
//
// Function: na_apache_post_conf()
//
// Purpose: Validate configuration variables and set up NetAcuity server
//          communication.
//
// Parameters: p    - Apache pool to allocate from.
//             s    - The server to allocate this to (not used).
//
// Returns: OK | AP_FILTER_ERROR.
//
//****************************************************************************
/* Frees the na_client_t handle when the pool is destroyed. */
static apr_status_t na_client_apr_cleanup(void *data)
{
    na_client_free((na_client_t *) data);
    return APR_SUCCESS;
}

static int na_apache_post_conf(apr_pool_t *p, apr_pool_t *plog, apr_pool_t *ptemp, server_rec *s)
{
    na_apache_conf_t    *cfg;

    /* Get our server config context and ignore if this is a preflight read.
     */
    cfg = (na_apache_conf_t *)ap_get_module_config(s->module_config, &netacuity_module);
    RETURN_IF_PREFLIGHT(s->process->pool, MODULE_NAME);

    /* The NetAcuityServer configuration variable must be specified.
     */
    if (cfg->serverIP[0] == '\0')
    {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL,
                     "%s: NetAcuityServer must be specified.",
                     MODULE_NAME);
        return AP_FILTER_ERROR;
    }

    /* Parse and validate the feature codes.
     */
    if (!na_apache_parse_features(cfg))
    {
        return AP_FILTER_ERROR;
    }

    /* Generate a list of URI suffixes to not bother looking up.
     */
    na_apache_generate_ignore_list(p, cfg);

    /* Set the NetAcuity server specifics.
     */
    cfg->client = na_client_new(cfg->serverIP, cfg->id, cfg->timeout * 1000);
    if (!cfg->client)
    {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL,
                     "%s: Error setting NetAcuity server address: %s.",
                     MODULE_NAME, cfg->serverIP);
        return AP_FILTER_ERROR;
    }
    apr_pool_cleanup_register(p, cfg->client, na_client_apr_cleanup, apr_pool_cleanup_null);

    /* Create a process level mutex from the config directive in
     * case it's needed.
     */
    if (apr_proc_mutex_create(&cfg->mutex, lockfile, APR_LOCK_DEFAULT, p) != APR_SUCCESS)
    {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL,
                     "%s: Unable to create process mutex.", MODULE_NAME);
        return AP_FILTER_ERROR;
    }

    cfg->bEnabled = TRUE;
    ap_add_version_component(p, VERSION_STRING) ;
    return OK;
}

//****************************************************************************
//
// Function: na_apache_request_handler()
//
// Purpose: Lookup the client IP in the NetAcuity DB and set environment
//          vars based on what it returns.
//
// Parameters: r    - Apache request structure.
//
// Returns: DECLINED
//
//****************************************************************************
static int na_apache_request_handler(request_rec *r)
{
    char response[MAX_XML_RESPONSE_SIZE];
    char error_msg[MAX_ERROR_MSG_LEN];
    char *query_ip;
    char ipArray[MAX_CSV_IP_ADDR_LEN];
    char transaction_id[MAX_TRANSACTION_ID];
    snprintf(transaction_id, sizeof(transaction_id), "%d", rand() % 1000000000);

    na_apache_conf_t *cfg = ap_get_module_config(r->server->module_config,
                                                 &netacuity_module);
    if (!cfg || !cfg->bEnabled)
    {
        return DECLINED;
    }

    /* If we've already processed this request (i.e. this is a subprocess
     * request from mod-rewrite), then skip it.
     */
    if (apr_table_get(r->headers_in, "HTTP_NETACUITY") != NULL)
    {
        return DECLINED;
    }

    /* If we're only looking up POST requests, then skip it if this
     * isn't a POST.
     */
    if (cfg->postOnly && r->method_number != M_POST)
    {
        return DECLINED;
    }

    /* Use remote_ip for apache version 2.2 and earlier and useragent_ip/client_ip for apache version 2.4 and later
    *
    */
    #if(AP_SERVER_MAJORVERSION_NUMBER < 2 || (AP_SERVER_MINORVERSION_NUMBER <= 2 && AP_SERVER_MAJORVERSION_NUMBER == 2))
        query_ip = r->connection->remote_ip;
    #else
        if(cfg->useUserAgentIP == 1)
        {
            char *userAgentSave;
            apr_cpystrn(ipArray, r->useragent_ip, sizeof(ipArray));
            char *userAgentIpCopy = ipArray;
            //Use a copy of useragent_ip since apr_strtok changes value of the pointer passed in. (r->useragent_ip may need to be used again)
            query_ip = apr_strtok(userAgentIpCopy, ",", &userAgentSave);
        } else
        {
            query_ip = r->connection->client_ip;
        }
    #endif

    /* If we're using http headers, then get the query_ip from the http header's content.
     * Otherwise, continue using the REMOTE_ADDR as the query_ip.
     */
    if (cfg->useHttpHeader)
    {
        char *pIpAddrSave;
        char *pIpAddr;

        //if the http header exists, then parse it to get the query_ip
        pIpAddr = (char *)apr_table_get(r->headers_in, cfg->httpHeaderName);
        if (pIpAddr != NULL)
        {
            //We only want the first address in the comma-separated list; no need to loop.
            // (If we wanted the last address in the list, then we need to rewrite the next line to loop.)
            apr_cpystrn(ipArray, pIpAddr, sizeof(ipArray));
            char *pIpAddrCopy = ipArray;
            query_ip = apr_strtok(pIpAddrCopy, ",", &pIpAddrSave);
        }
    }

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, NULL,
                 "%s: Client IP: %s  URI: %s", MODULE_NAME, query_ip, r->uri);

    /* Skip any URL suffixes (i.e. extensions) we should ignore.
     */
    if (na_apache_match_suffix(r->uri, cfg->pIgnoreList))
    {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, NULL,
                     "%s: Not looking up: %s", MODULE_NAME, r->uri);
        return DECLINED;
    }

    /* Do the lookup, which should return an XML response.
     */
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, NULL,
                 "%s: Querying NetAcuity server for address: %s", MODULE_NAME, query_ip);
    if (!na_query_xml(cfg->client, query_ip, cfg->featureCodes, transaction_id, response, sizeof(response), error_msg))
    {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL,
                     "%s: Error querying NetAcuity server for address: %s -- %s", MODULE_NAME, query_ip, error_msg);
        return DECLINED;
    }
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, NULL,
                 "%s: NetAcuity response: %s", MODULE_NAME, response);

    /* Pull field-names from the XML response and put them
     * into the Apache environment.
     */
    na_apache_set_response_vars(r, response);

    /* Set a fictitious NetAcuity header so we can detect and short
     * circuit any subprocess requests from mod-rewrite.
     */
    apr_table_set(r->headers_in, "HTTP_NETACUITY", "yes");
    return DECLINED;
}

//****************************************************************************
//
// Function: na_apache_register_hooks()
//
// Purpose: Register our hook handlers.
//
// Parameters: p    - Apache pool to allocate from.
//
// Returns: void.
//
//****************************************************************************
static void na_apache_register_hooks(apr_pool_t *p)
{
    ap_hook_post_config(na_apache_post_conf, NULL, NULL, APR_HOOK_MIDDLE);
    ap_hook_post_read_request(na_apache_request_handler, NULL, NULL, APR_HOOK_FIRST);
}

static const command_rec na_apache_cmds[] =
{
    AP_INIT_TAKE1("NetAcuityServer", na_apache_get_server, NULL,
                  RSRC_CONF, "Specify the NetAcuity server IP"),
    AP_INIT_TAKE1("NetAcuityDBS", na_apache_get_feature_codes, NULL,
                  RSRC_CONF, "Specify the NetAcuity DBS feature codes"),
    AP_INIT_TAKE1("NetAcuityTimeout", na_apache_get_timeout, NULL,
                  RSRC_CONF, "Specify the NetAcuity server timeout (milliseconds)"),
    AP_INIT_TAKE1("NetAcuityID", na_apache_get_id, NULL,
                  RSRC_CONF, "Specify the NetAcuity ID"),
    AP_INIT_TAKE1("NetAcuityIgnoreSuffixes", na_apache_get_ignore_suffixes, NULL,
                  RSRC_CONF, "Specify the URL suffixes/extensions to not lookup"),
    AP_INIT_TAKE1("NetAcuityPostOnly", na_apache_get_post_only, NULL,
                  RSRC_CONF, "Specify if only POST requests should be looked up"),
    AP_INIT_TAKE1("NetAcuityLockfile", na_apache_get_lockfile, NULL,
                  RSRC_CONF, "Specify the netacuity_module mutex lock file"),
    AP_INIT_TAKE1("NetAcuityUseHttpHeader", na_apache_get_http_header, NULL,
                  RSRC_CONF, "Specify the http header to use for looking up IP addresses"),
    AP_INIT_TAKE1("NetAcuityUseUserAgentIP", na_apache_get_use_user_agent_ip, NULL,
                      RSRC_CONF, "Specify the http header to use for looking up IP addresses"),
    { NULL }
};

module AP_MODULE_DECLARE_DATA netacuity_module =
{
        STANDARD20_MODULE_STUFF,
        NULL,
        NULL,
        na_apache_server_config,
        NULL,
        na_apache_cmds,
        na_apache_register_hooks
};

/******************************* Internal Routines *******************************/

//****************************************************************************
//
// Function: na_apache_get_xxx()
//
// Purpose: Load configuration values from httpd.conf.
//
// Returns: NULL | ap_check_cmd_context() error.
//
//****************************************************************************
static const char *na_apache_get_server(cmd_parms *cmd, void *dummy, const char *arg)
{
    na_apache_conf_t    *cfg;
    const char          *err;

    cfg = (na_apache_conf_t *)ap_get_module_config(cmd->server->module_config,
                                                   &netacuity_module);
    if ((err = ap_check_cmd_context(cmd, GLOBAL_ONLY)) != NULL)
    {
        return err;
    }

    apr_cpystrn(cfg->serverIP, ap_getword_conf(cmd->pool, &arg), sizeof(cfg->serverIP));
    return NULL;
}

static const char *na_apache_get_feature_codes(cmd_parms *cmd, void *dummy, const char *arg)
{
    na_apache_conf_t    *cfg;
    const char          *err;

    cfg = (na_apache_conf_t *)ap_get_module_config(cmd->server->module_config,
                                                   &netacuity_module);
    if ((err = ap_check_cmd_context(cmd, GLOBAL_ONLY)) != NULL)
    {
        return err;
    }

    apr_cpystrn(cfg->featureCodes, ap_getword_conf(cmd->pool, &arg), sizeof(cfg->featureCodes));
    return NULL;
}

static const char *na_apache_get_timeout(cmd_parms *cmd, void *dummy, const char *arg)
{
    na_apache_conf_t    *cfg;
    const char          *err;

    cfg = (na_apache_conf_t *)ap_get_module_config(cmd->server->module_config,
                                                   &netacuity_module);
    if ((err = ap_check_cmd_context(cmd, GLOBAL_ONLY)) != NULL)
    {
        return err;
    }

    cfg->timeout = (int)apr_atoi64(ap_getword_conf(cmd->pool, &arg));
    if (cfg->timeout == 0)
    {
        cfg->timeout = NA_DEFAULT_TIMEOUT;
    }
    return NULL;
}

static const char *na_apache_get_id(cmd_parms *cmd, void *dummy, const char *arg)
{
    na_apache_conf_t    *cfg;
    const char          *err;

    cfg = (na_apache_conf_t *)ap_get_module_config(cmd->server->module_config,
                                                   &netacuity_module);
    if ((err = ap_check_cmd_context(cmd, GLOBAL_ONLY)) != NULL)
    {
        return err;
    }

    cfg->id = (int)apr_atoi64(ap_getword_conf(cmd->pool, &arg));
    return NULL;
}

static const char *na_apache_get_ignore_suffixes(cmd_parms *cmd, void *dummy, const char *arg)
{
    na_apache_conf_t    *cfg;
    const char          *err;

    cfg = (na_apache_conf_t *)ap_get_module_config(cmd->server->module_config,
                                                   &netacuity_module);
    if ((err = ap_check_cmd_context(cmd, GLOBAL_ONLY)) != NULL)
    {
        return err;
    }

    apr_cpystrn(cfg->ignoreSuffixes, ap_getword_conf(cmd->pool, &arg), sizeof(cfg->ignoreSuffixes));
    return NULL;
}

static const char *na_apache_get_post_only(cmd_parms *cmd, void *dummy, const char *arg)
{
    na_apache_conf_t    *cfg;
    const char          *err;

    cfg = (na_apache_conf_t *)ap_get_module_config(cmd->server->module_config,
                                                   &netacuity_module);
    if ((err = ap_check_cmd_context(cmd, GLOBAL_ONLY)) != NULL)
    {
        return err;
    }

    cfg->postOnly = FALSE;
    if (strcasecmp(ap_getword_conf(cmd->pool, &arg), "yes") == 0)
    {
        cfg->postOnly = TRUE;
    }

    return NULL;
}

static const char *na_apache_get_lockfile(cmd_parms *cmd, void *dummy, const char *arg)
{
    const char          *err;

    if ((err = ap_check_cmd_context(cmd, GLOBAL_ONLY)) != NULL)
    {
        return err;
    }

    lockfile = ap_server_root_relative(cmd->pool, arg);
    if (!lockfile)
    {
        return apr_pstrcat(cmd->pool, "Invalid NetAcuityLockfile: ", arg);
    }

    return NULL;
}

static const char *na_apache_get_http_header(cmd_parms *cmd, void *dummy, const char *arg)
{
    na_apache_conf_t    *cfg;
    const char          *err;

    cfg = (na_apache_conf_t *)ap_get_module_config(cmd->server->module_config,
                                                   &netacuity_module);
    if ((err = ap_check_cmd_context(cmd, GLOBAL_ONLY)) != NULL)
    {
        return err;
    }

    //  If ap_getword_conf() cannot find the &arg, it will return an empty string,
    //  in which case we use the traditional REMOTE_ADDR for the query-IP.
    apr_cpystrn(cfg->httpHeaderName, ap_getword_conf(cmd->pool, &arg), sizeof(cfg->httpHeaderName));
    cfg->useHttpHeader = (strlen(cfg->httpHeaderName) != 0);
    if (cfg->useHttpHeader)
    {
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, NULL, "%s: Client IP addresses will be pulled from http header: %s", MODULE_NAME, cfg->httpHeaderName);
    }

    return NULL;
}

static const char *na_apache_get_use_user_agent_ip(cmd_parms *cmd, void *dummy, const char *arg)
{
    na_apache_conf_t    *cfg;
    const char          *err;

    cfg = (na_apache_conf_t *)ap_get_module_config(cmd->server->module_config,
                                                   &netacuity_module);
    if ((err = ap_check_cmd_context(cmd, GLOBAL_ONLY)) != NULL)
    {
        return err;
    }

    cfg->useUserAgentIP = (int)apr_atoi64(ap_getword_conf(cmd->pool, &arg));
    return NULL;
}

static void na_apache_convert_field_name(char *na_env_var_name, const unsigned char *na_field_name)
{
    /* na_env_var_name is NA_ENV_VAR_NAME_MAX_LEN bytes; the prefix consumes
     * NA_ENV_VAR_NAME_PREFIX_LEN of that, leaving room for this many chars
     * plus a null terminator. */
    const int max_name_len = NA_ENV_VAR_NAME_MAX_LEN - NA_ENV_VAR_NAME_PREFIX_LEN - 1;
    strcpy(na_env_var_name, NA_ENV_VAR_NAME_PREFIX);
    na_env_var_name += NA_ENV_VAR_NAME_PREFIX_LEN;
    int i;
    for (i = 0; na_field_name[i] != '\0' && i < max_name_len; i++)
    {
        char c = na_field_name[i];
        if (isalnum(c))
        {
            na_env_var_name[i] = toupper(c);
        }
        else
        {
            na_env_var_name[i] = '_';
        }
    }
    na_env_var_name[i] = '\0';
}


//****************************************************************************
//
// Function: na_apache_set_response_vars()
//
// Purpose: Set Apache environment variables for each field in the XML response.
//
// Parameters: r        - Apache request record.
//             response - XML response string from a NetAcuity API query.
//
//****************************************************************************
static void na_apache_set_response_vars(request_rec *r, const char *response) 
{
    xmlDoc *xml_doc = xmlParseDoc((unsigned char *)response);
    if (xml_doc == NULL) {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL,
                        "%s: Failed to parse XML response",
                        MODULE_NAME);
        return;
    }
    xmlNode *root_node = xmlDocGetRootElement(xml_doc);
    if (root_node == NULL) {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL,
                        "%s: XML response has no root element",
                        MODULE_NAME);
        xmlFreeDoc(xml_doc);
        return;
    }

    char na_env_var_name[NA_ENV_VAR_NAME_MAX_LEN];
    for(xmlAttr *attr = root_node->properties; attr != NULL; attr = attr->next)
    {
        na_apache_convert_field_name(na_env_var_name, attr->name);
        xmlChar *value = xmlNodeListGetString(root_node->doc, attr->children, 1);
        apr_table_set(r->subprocess_env, na_env_var_name, (char *)value);
        xmlFree(value);
    }
    xmlFreeDoc(xml_doc);
}




//****************************************************************************
//
// Function: na_apache_generate_ignore_list()
//
// Purpose: Generate a list of strings for matching to each request suffix
//          (i.e. extensions such as .gif, .jpg, etc.).  These
//          must be comma delimited.
//
// Parameters: p        - Apache pool to allocate from.
//             cfg      - Our context structure
//
// Returns: void.
//
//****************************************************************************
static void na_apache_generate_ignore_list(apr_pool_t *p,
                                           na_apache_conf_t *cfg)
{
    MatchString         *pIgnoreItem;
    MatchString         *pLocalList;
    char                ignoreSuffixes[MAX_IGNORE_SUFFIXES_LEN];
    char                *pSuffix;
    char                *pSuffixSave;
    int                 len;

    if (cfg->ignoreSuffixes[0] == '\0')
    {
        return;
    }

    apr_cpystrn(ignoreSuffixes, cfg->ignoreSuffixes, sizeof(ignoreSuffixes));
    pSuffix = ignoreSuffixes;
    while ((pSuffix = apr_strtok(pSuffix, ",", &pSuffixSave)) != NULL)
    {
        len = strlen(pSuffix);

        /* Allocate and place this item at the bottom of the list.
        */
        pIgnoreItem = (MatchString *)apr_pcalloc(p, sizeof(MatchString) + len + 1);
        pIgnoreItem->len = len;
        pIgnoreItem->pMatch = (char *)(pIgnoreItem + 1);
        apr_cpystrn(pIgnoreItem->pMatch, pSuffix, len + 1);

        for (pLocalList = (MatchString *)&cfg->pIgnoreList;
             pLocalList->next;
             pLocalList = pLocalList->next);

        pLocalList->next = pIgnoreItem;
        pSuffix = NULL;
    }
}

//****************************************************************************
//
// Function: na_apache_parse_features()
//
// Purpose: Parse out and validate the NetAcuityDBS feature codes.  These
//          must be in a comma delimited list.
//
// Parameters: cfg      - Our context structure
//
// Returns: TRUE = valid | FALSE = invalid or not present
//
//****************************************************************************
static short na_apache_parse_features(na_apache_conf_t *cfg)
{
    char                featureCodes[MAX_FEATURE_CODE_STRING+1];
    char                *pCodeSave;
    char                *pCode;
    int                 i;

    if (cfg->featureCodes[0] == '\0')
    {
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL,
                     "%s: NetAcuityDBS feature codes must be specified.",
                     MODULE_NAME);
        return FALSE;
    }
    apr_cpystrn(featureCodes, cfg->featureCodes, sizeof(featureCodes));
    pCode = featureCodes;
    for (i = 0; (i < MAX_FEATURE_CODES) && ((pCode = apr_strtok(pCode, ",", &pCodeSave)) != NULL); ++i)
    {
        unsigned int code = (unsigned int)apr_atoi64(pCode);
        if (code <= 0)
        {
            ap_log_error(APLOG_MARK, APLOG_ERR, 0, NULL,
                         "%s: NetAcuityDBS contains an invalid feature-code: %s",
                         MODULE_NAME, pCode);
            return FALSE;
        }
        pCode = NULL;
    }

    return TRUE;
}

//****************************************************************************
//
// Function: na_apache_match_suffix()
//
// Purpose: Check if the specified URI ends with one of the suffixes in the
//          specified ignore list.
//
// Parameters: pURI         - URI to test.
//             pSuffixList  - List of suffixes to compare against.
//
// Returns: TRUE = match | FALSE = no match.
//
//****************************************************************************
static short na_apache_match_suffix(const char *pURI, MatchString *pSuffixList)
{
    register MatchString    *pIgnoreItem;
    int                     uriLen;

    if (!pURI || !pSuffixList)
    {
        return FALSE;
    }

    uriLen = strlen(pURI);
    for (pIgnoreItem = pSuffixList; pIgnoreItem != NULL; pIgnoreItem = pIgnoreItem->next)
    {
        if ((uriLen >= pIgnoreItem->len) &&
            (strncasecmp(&pURI[uriLen-pIgnoreItem->len],
                         pIgnoreItem->pMatch, pIgnoreItem->len) == 0))
        {
            return TRUE;
        }
    }

    return FALSE;
}
