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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <libxml/tree.h>

#include "netacuity_api.h"

int main(int argc, char **argv)
{
    LIBXML_TEST_VERSION

    char transaction_id[MAX_TRANSACTION_ID];
    char response_buf[MAX_XML_RESPONSE_SIZE];
    char error_msg_buf[MAX_ERROR_MSG_LEN];

    if ( argc < 4 )
    {
        printf("Usage: %s <server_ip> <ip_to_query> <feature_codes>\n",
               argv[0]);
        exit(-1);
    }
    const char *server_ip = argv[1];
    const char *query_ip = argv[2];
    const char *feature_codes = argv[3];

    unsigned int secure_seed = 0;
    FILE *urandom = fopen("/dev/urandom", "rb");
    if ( urandom != NULL )
    {
        if ( fread(&secure_seed, sizeof(secure_seed), 1, urandom) != 1 )
        {
            secure_seed = (unsigned int) time(NULL);  /* fall back if the read somehow fails */
        }
        fclose(urandom);
    }
    else
    {
        secure_seed = (unsigned int) time(NULL);  /* fall back if /dev/urandom is unavailable */
    }
    snprintf(transaction_id, sizeof(transaction_id), "%u", secure_seed % 1000000000);

    na_client_t *client = na_client_new(server_ip, 71, 3000000);  // api id 71, 3 second timeout
    if ( !client )
    {
        printf("Error: invalid server address or API ID\n");
        exit(-1);
    }

    if (!na_query_xml(client, query_ip, feature_codes, transaction_id, response_buf, sizeof(response_buf), error_msg_buf))
    {
        printf("Error: %s\n", error_msg_buf);
        exit(-1);
    }

    // Use libxml2 to parse the xml string into an xmlDoc
    xmlDoc *xml_doc = xmlParseDoc((unsigned char *) response_buf);
    if (xml_doc == NULL) {
        printf("Error: could not parse xml\n");
        exit(-1);
    }
    xmlNode *root_node = xmlDocGetRootElement(xml_doc);

    xmlChar *ip_val = xmlGetProp(root_node, (const xmlChar *) "ip");
    xmlChar *trans_id_val = xmlGetProp(root_node, (const xmlChar *) "trans-id");
    printf("ip = %s\n", ip_val);
    printf("trans-id = %s\n", trans_id_val);
    xmlFree(ip_val);
    xmlFree(trans_id_val);

    // Loop over the attributes in the xml root node.
    // These attributes correspond to NetAcuity feature fields.
    for(xmlAttr *attr = root_node->properties; attr != NULL; attr = attr->next)
    {
        if (xmlStrcmp(attr->name, (const xmlChar *) "ip") == 0 || xmlStrcmp(attr->name, (const xmlChar *) "trans-id") == 0)
        {
            continue;
        }
        xmlChar *value = xmlNodeListGetString(root_node->doc, attr->children, 1);
        printf("%s = %s\n", attr->name, value );
        xmlFree(value);
    }
    xmlFreeDoc(xml_doc);
    printf("raw-response = %s\n", response_buf);

    na_client_free(client);
    return 1;
}
