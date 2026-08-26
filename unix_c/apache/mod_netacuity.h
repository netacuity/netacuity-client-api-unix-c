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

#ifndef MOD_NETACUITY_H_
#define MOD_NETACUITY_H_

/* Apache runs init routines twice.  Set token to ensure things run only once.
 * (modified from Apache2 modules/generators/mod_cgid.c)
 * (The pool must have the same lifetime as the server (i.e. s->process->pool)
 *  so that when the server is restarted, the userdata set below will be cleaned up
 *  and the module will be reinitialized properly.  A file-scoped global flag
 *  is not suitable for this purpose, unless it acts as a toggle and makes the
 *  assumption that init routines are always run twice, and exactly twice,
 *  (which is currently true AFAIK).
 *  The method below is safer.)
 */
#define RETURN_IF_PREFLIGHT(pool, tag)                                  \
    do {                                                                \
        void *userdata_data;                                            \
        apr_pool_userdata_get(&userdata_data, tag "_preflight", (pool));\
        if (userdata_data == NULL) {                                    \
            apr_pool_userdata_set((const void *)1, tag "_preflight",    \
                                  apr_pool_cleanup_null, (pool));       \
            return OK;                                                  \
        }                                                               \
    } while (0)

#define MAX_FEATURE_CODES       40
#define MAX_FEATURE_CODE_STRING 128
#define MAX_HTTP_HEADER_NAME    128
#define MAX_CSV_IP_ADDR_LEN     255
#define MAX_IGNORE_SUFFIXES_LEN    1024

/* General purpose structure to hold request URL suffixes to be dropped.
*/
typedef struct _match_string
{
    struct _match_string *next;
    char                 *pMatch;        /* The prefix/suffix itself */
    int                  len;            /* And it's length */
} MatchString;

/* Our main configuration context.
*/
typedef struct _na_apache_conf_t
{
    char                serverIP[IP_STRING_LEN];
    char                featureCodes[MAX_FEATURE_CODE_STRING+1];
    char                ignoreSuffixes[MAX_IGNORE_SUFFIXES_LEN];
    char                httpHeaderName[MAX_HTTP_HEADER_NAME + 1];
    short               useHttpHeader;
    short               useUserAgentIP;
    MatchString         *pIgnoreList;
    int                 timeout;
    int                 id;
    short               postOnly;
    short               bEnabled;
    apr_proc_mutex_t    *mutex;
    na_client_t         *client;   /* Created once in na_apache_post_conf(), used by every request. */
} na_apache_conf_t;


/* Prototypes.
*/
static void         *na_apache_server_config(apr_pool_t *p, server_rec *s);
static int          na_apache_post_conf(apr_pool_t *p, apr_pool_t *plog, apr_pool_t *ptemp, server_rec *s);
static int          na_apache_request_handler(request_rec *r);
static void         na_apache_register_hooks(apr_pool_t *p);

static void         na_apache_set_response_vars(request_rec *r, const char *response); 

static const char   *na_apache_get_server(cmd_parms *cmd, void *dummy, const char *arg);
static const char   *na_apache_get_feature_codes(cmd_parms *cmd, void *dummy, const char *arg);
static const char   *na_apache_get_timeout(cmd_parms *cmd, void *dummy, const char *arg);
static const char   *na_apache_get_id(cmd_parms *cmd, void *dummy, const char *arg);
static const char   *na_apache_get_ignore_suffixes(cmd_parms *cmd, void *dummy, const char *arg);
static const char   *na_apache_get_post_only(cmd_parms *cmd, void *dummy, const char *arg);
static const char   *na_apache_get_lockfile(cmd_parms *cmd, void *dummy, const char *arg);
static const char   *na_apache_get_http_header(cmd_parms *cmd, void *dummy, const char *arg);
static const char   *na_apache_get_use_user_agent_ip(cmd_parms *cmd, void *dummy, const char *arg);
static void         na_apache_generate_ignore_list(apr_pool_t *p, na_apache_conf_t *cfg);
static short        na_apache_parse_features(na_apache_conf_t *cfg);
static short        na_apache_match_suffix(const char *pURI, MatchString *pIgnoreList);

#endif
