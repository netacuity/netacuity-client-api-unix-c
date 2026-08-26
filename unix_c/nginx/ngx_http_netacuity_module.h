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

#ifndef NGX_HTTP_NETACUITY_MODULE_H_
#define NGX_HTTP_NETACUITY_MODULE_H_

#include "../netacuity_fields.h"
#include "../netacuity_api.h"


/// NetAcuity field and NGINX variable-index association struct
typedef struct {
    netacuity_field *netacuity_field;
    ngx_int_t ngx_variable_index;
} netacuity_variable;


/// NGINX NetAcuity Module configuration struct
typedef struct {
    char *netacuity_server_ip;
    char *feature_code_csv;
    size_t feature_code_count;
    int *feature_code_array;
    size_t variable_count;
    netacuity_variable *variable_array;
    ngx_int_t ip_variable_index;
    ngx_int_t error_variable_index;
    char *no_error_value;
    size_t no_error_value_length;
    char *not_queried_value;
    size_t not_queried_value_length;
    na_client_t *client;   /* Created once in na_callback_setup_config(), used by every request. */
} ngx_http_netacuity_conf_t;


ngx_module_t ngx_http_netacuity_module;


#endif  // NGX_HTTP_NETACUITY_MODULE_H_
