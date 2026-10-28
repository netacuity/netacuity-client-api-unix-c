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

/*
 * Queries several IP addresses over one persistent connection.
 *
 * na_query_xml() opens and closes a UDP socket for every query. A
 * persistent connection (na_connection_*) keeps one socket open and re-uses
 * it, which suits long-lived processes that make many queries.
 *
 *   ./test_api_persistent <server_ip> <feature_codes> <ip_to_query> [<ip_to_query> ...]
 */

#include <stdio.h>
#include <stdlib.h>

#include <libxml/tree.h>

#include "netacuity_api.h"

// Prints each field of an XML response, one per line.
static void print_fields(const char *response)
{
    xmlDoc *xml_doc = xmlParseDoc((const xmlChar *) response);
    if (xml_doc == NULL)
    {
        printf("  Error: could not parse xml\n");
        return;
    }
    xmlNode *root_node = xmlDocGetRootElement(xml_doc);

    // These attributes correspond to NetAcuity feature fields.
    for (xmlAttr *attr = root_node->properties; attr != NULL; attr = attr->next)
    {
        xmlChar *value = xmlNodeListGetString(root_node->doc, attr->children, 1);
        printf("  %s = %s\n", attr->name, value ? (const char *) value : "");
        xmlFree(value);
    }
    xmlFreeDoc(xml_doc);
}

int main(int argc, char **argv)
{
    LIBXML_TEST_VERSION

    char response_buf[MAX_XML_RESPONSE_SIZE];
    char error_msg_buf[MAX_ERROR_MSG_LEN];
    int failures = 0;

    if ( argc < 4 )
    {
        printf("Usage: %s <server_ip> <feature_codes> <ip_to_query> [<ip_to_query> ...]\n",
               argv[0]);
        exit(-1);
    }
    const char *server_ip = argv[1];
    const char *feature_codes = argv[2];

    na_client_t *client = na_client_new(server_ip, 71, 3000000);  // api id 71, 3 second timeout
    if ( !client )
    {
        printf("Error: invalid server address or API ID\n");
        exit(-1);
    }

    // The connection copies the client's settings, so the client could be
    // freed here. The socket isn't opened until the first query.
    na_connection_t *conn = na_connection_new(client);
    if ( !conn )
    {
        printf("Error: could not allocate a connection\n");
        na_client_free(client);
        exit(-1);
    }

    for (int i = 3; i < argc; i++)
    {
        const char *query_ip = argv[i];
        printf("%s:\n", query_ip);

        // Every query on a connection needs its own transaction ID; NULL has
        // one generated. A failed query closes the socket, and the next query
        // opens a new one, so a failure needn't end the loop.
        if (!na_connection_query_xml(conn, query_ip, feature_codes, NULL,
                                     response_buf, sizeof(response_buf), error_msg_buf))
        {
            printf("  Error: %s\n", error_msg_buf);
            failures++;
            continue;
        }
        print_fields(response_buf);
    }

    na_connection_free(conn);
    na_client_free(client);
    return failures == 0 ? 0 : 1;
}
