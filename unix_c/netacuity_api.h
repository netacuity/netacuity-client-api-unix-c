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

#ifndef NETACUITY_API_H
#define NETACUITY_API_H

#include <stddef.h>

#define   API_SERVER_UDP_PORT  5400

/* DB feature codes */
#define NA_GEO              3
#define NA_EDGE             4
#define NA_ZIP              7
#define NA_ISP              8
#define NA_HOME_BIZ         9
#define NA_ASN             10
#define NA_LANGUAGE        11
#define NA_PROXY           12
#define NA_ISANISP         14
#define NA_COMPANY         15
#define NA_DOMAIN_16       16
#define NA_DEMOGRAPHICS    17
#define NA_NAICS           18
#define NA_CBSA            19
#define NA_PULSE_MAX       21
#define NA_MOBILE_CARRIER  24
#define NA_ORGANIZATION    25
#define NA_PULSE           26
#define NA_PULSE_PLUS      30
#define NA_VPN_PROXY       33
#define NA_IPC             35
#define NA_NODIFY          36
#define NA_DECODE          93
#define NA_OBSERVED_COUNTRIES 37
#define NA_AA_SA1          40
#define NA_AAMAX_SA1       41
#define NA_AA_IRIS         42
#define NA_AA_PLZ8         43
#define NA_AA_ZIP4         44
#define NA_AAMAX_ZIP4      45
#define NA_AAMAX_IRIS      46
#define NA_AAMAX_PLZ8      47

// valid range for a db-feature/db number requested in a query
#define NA_MIN_DB_FEATURE   3
#define NA_MAX_DB_FEATURE  100

// the max char-length (including null-term) for any NA response field-name
#define MAX_FIELD_NAME_LEN   100

// the max char-length (including null-term) of an IP address string buffer
// in the apache/nginx modules
#define IP_STRING_LEN         50

// the max char-length (including null-term) of the NA XML response
//  (99 x (1400 - 4 - 1)) + 1 = 138106
#define MAX_XML_RESPONSE_SIZE   138106

// the max char-length (including null-term) of a single NA XML response packet
#define MAX_RESPONSE_SIZE       1496

// the max char-length (including null-term) of the transaction_id
#define MAX_TRANSACTION_ID        64

// the max char-length (including null-term) of a csv of feature-codes
#define MAX_FEATURE_CSV_LEN      100

// the max char-length (including null-term) of an NA error message
#define MAX_ERROR_MSG_LEN        1024

// the max char-length (including null-term) of an IPv4 or IPv6 address
#define MAX_IP_LEN                46

// default timeout (in microseconds) used by na_client_new_with_default_timeout()
#define NA_DEFAULT_TIMEOUT_US    2000000

/* API Functions */

/* Opaque client handle: holds the NetAcuity Server address, api_id, and
 * timeout for a single client. Its definition is private to
 * netacuity_api.c. Create one with na_client_new() and release it with
 * na_client_free() once no longer needed. Immutable after na_client_new();
 * safe to share across threads without locking. */
typedef struct na_client na_client_t;

/***
   Function:    na_client_new
   Returns:     A new client handle, or NULL if server_ip is not a
                well-formed IPv4/IPv6 literal, api_id is not between 0 and
                127, or allocation fails.
   Parameters:  server_ip  - IP address in dotted notation of the NetAcuity
                             Server to use.
                api_id     - Integer between 0 and 127. 0 is left as
                             "unregistered".
                timeout_us - Maximum time to wait on network calls, in
                             microseconds (this includes both connect and
                             query). Pass NA_DEFAULT_TIMEOUT_US (2000000, i.e.
                             2 seconds) to match this library's own default,
                             or use na_client_new_with_default_timeout() to
                             have it applied automatically.
   Description: Allocates and initializes a client handle. Must be passed
                explicitly to every na_query_*() call, and released with
                na_client_free() when no longer needed.
***/
na_client_t *na_client_new(const char *server_ip, int api_id, int timeout_us);

/***
   Function:    na_client_new_with_default_api_id
   Returns:     See na_client_new().
   Parameters:  server_ip  - See na_client_new().
                timeout_us - See na_client_new().
   Description: Convenience wrapper around na_client_new() for callers with
                no registered api_id, defaulting it to 0 ("unregistered").
***/
na_client_t *na_client_new_with_default_api_id(const char *server_ip, int timeout_us);

/***
   Function:    na_client_new_with_default_timeout
   Returns:     See na_client_new().
   Parameters:  server_ip  - See na_client_new().
                api_id     - See na_client_new().
   Description: Convenience wrapper around na_client_new() for callers
                without a specific timeout requirement, defaulting it to
                NA_DEFAULT_TIMEOUT_US (2 seconds).
***/
na_client_t *na_client_new_with_default_timeout(const char *server_ip, int api_id);

/***
   Function:    na_client_free
   Returns:     void
   Parameters:  client - The client handle to release. NULL is a no-op.
   Description: Releases a client handle allocated by na_client_new().
***/
void na_client_free(na_client_t *client);

/***
   Function:    na_query_xml
   Returns:     1-success, 0=error
   Parameters:  client: Client handle from na_client_new().
                ip: IP address to query in dotted-notation
                feature_codes: Comma separated list of database features to query
                transaction_id:  A user-supplied transaction ID (can be "", or NULL
                          to have one generated automatically)
                response: Pointer to location response should be returned
                max_size: The size of the response memory.  An error will
                          occur if response exceeds this size.
   Description: Queries the NetAcuity databases specified in feature_codes for
                the IP address passed and returns the response in the memory
                supplied by the caller.  The response will be an XML string
                that can be passed to your favorite XML parser or you can
                call the xml_get_field() to retrieve specific fields.
                Make sure that the buffer "response" is large enough to handle
                the entire response, otherwise an error will occur and no
                response data will be returned.
                If a timeout or error occurs, it returns 0 and the
                error reason will be returned in "response".
***/
int na_query_xml(na_client_t *client, const char *query_ip, const char *feature_codes, const char *transaction_id,
                 char *response, size_t max_size, char *error_msg);

/***
   Function:    get_xml_field
   //
   //      THIS FUNCTION IS DEPRECATED.
   //                It is recommended to use libxml2 instead.
   //
   Returns:     pointer to value string
   Parameters:  response:  Pointer to the XML response from na_query_xml()
                field_name:  Name of the field to retrieve
                value:  Pointer to location value should be returned --
                        MUST be at least 17 bytes but large enough to
                        hold response.
                        Will contain "FieldNotFound" if requested field
                        not in response.
                        If the field's value is longer than will fit in the
                        supplied buffer, it is safely truncated to fit
                        (and still null-terminated) rather than discarded.
                        All values returned are strings (char[]) -- you must
                        use atoi() to convert into numeric values
                        (if appropriate).
                size:  The size of the value memory.  An error will
                       occur if response exceeds this size.
   Description: Retrieves the value of the field name passed from the
                response string returned from na_query_xml().
                
***/
char *get_xml_field(const char *response, const char *field_name, char *value, size_t size);

#endif
