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
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <strings.h>

#include "netacuity_api.h"

#define MAX_XML_REQUEST_BUFF_SIZE     1500
#define XML_PACKET_NUM_DIGITS            2

/* Connection config for a single client. Immutable after na_client_new();
 * safe to share across threads without locking. */
struct na_client
{
    char server_addr[MAX_IP_LEN];
    int  timeout;      // in microseconds (us)
    int  api_id;
    int  ip_family;    // AF_INET or AF_INET6
};

/* Struct to receive XML response into */
typedef struct api_xml_response_t
{
    char thisPacketNum[XML_PACKET_NUM_DIGITS];        // size of the response including NULL
    char numPacketsToReceive[XML_PACKET_NUM_DIGITS];  // number of fields in response
    char response[MAX_RESPONSE_SIZE];  // response
}api_xml_response;

/***************************** private routines *****************************/

// ****************************************************************************
//
// Function: set_nonblock_flag()
//
// Purpose:  Changes the state of the fd's non blocking flag.
//
// Parameters: desc  - The file descriptor to change.
//             value - The value to change the file descriptor to.
//
// Returns: The result of the calling fcntl
//
// ****************************************************************************
static int set_nonblock_flag (int desc, int value)
{
    int oldflags = fcntl (desc, F_GETFL, 0);

    /* If reading the flags failed, return error indication now. */
    if ( oldflags == -1 )
    {
        return -1;
    }

    /* Set just the flag we want to set. */
    if ( value != 0 )
    {
        oldflags |= O_NONBLOCK;
    }
    else
    {
        oldflags &= ~O_NONBLOCK;
    }

    /* Store modified flag word in the descriptor. */
    return fcntl (desc, F_SETFL, oldflags);
}

// Compares two IP address literals for semantic equality rather than textual
// equality, so a compressed IPv6 form (e.g. "2001:db8::1") and its fully
// expanded equivalent ("2001:0db8:0000:...:0001") are recognized as the same
// address. Parses both into binary via inet_pton() and compares the bytes,
// rather than the strings, via memcmp(). Returns 1 if equal, 0 otherwise
// (including if either string fails to parse as a valid literal).
static int ip_strings_equal(const char *a, const char *b)
{
    struct in6_addr a_bin;
    struct in6_addr b_bin;
    int family;
    size_t addr_len;

    if (inet_pton(AF_INET, a, &a_bin) == 1)
    {
        family = AF_INET;
        addr_len = sizeof(struct in_addr);
    }
    else if (inet_pton(AF_INET6, a, &a_bin) == 1)
    {
        family = AF_INET6;
        addr_len = sizeof(struct in6_addr);
    }
    else
    {
        return 0;
    }

    if (inet_pton(family, b, &b_bin) != 1)
    {
        return 0;
    }

    return memcmp(&a_bin, &b_bin, addr_len) == 0;
}

// Fills out (a buffer of out_size bytes) with a random, NUL-terminated,
// lowercase-hex transaction ID, for callers that omit one (pass NULL) to
// na_query_xml(). Prefers arc4random_buf() where it's known
// to be available (BSD, macOS, and glibc >= 2.36 per BUILD_NOTES.txt's
// target); falls back to /dev/urandom, and as a last resort rand() if even
// that can't be opened.
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || \
    (defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 36)))
#define NA_HAVE_ARC4RANDOM 1
#endif

static void generate_transaction_id(char *out, size_t out_size)
{
    static const char hex_digits[] = "0123456789abcdef";
    unsigned char rand_bytes[8];
    size_t out_len = out_size - 1;
    size_t i;

    if (out_len > sizeof(rand_bytes) * 2)
    {
        out_len = sizeof(rand_bytes) * 2;
    }

#if defined(NA_HAVE_ARC4RANDOM)
    arc4random_buf(rand_bytes, sizeof(rand_bytes));
#else
    {
        size_t got = 0;
        int fd = open("/dev/urandom", O_RDONLY);
        if (fd >= 0)
        {
            while (got < sizeof(rand_bytes))
            {
                ssize_t n = read(fd, rand_bytes + got, sizeof(rand_bytes) - got);
                if (n <= 0)
                {
                    break;
                }
                got += (size_t)n;
            }
            close(fd);
        }
        while (got < sizeof(rand_bytes))
        {
            rand_bytes[got++] = (unsigned char)rand();
        }
    }
#endif

    for (i = 0; i < out_len; i++)
    {
        unsigned char b = rand_bytes[i / 2];
        out[i] = hex_digits[(i % 2 == 0) ? (b >> 4) : (b & 0x0F)];
    }
    out[out_len] = '\0';
}

// Test seam: na_query_xml() creates its UDP socket through this function
// pointer rather than calling socket() directly, so test code
// (which #includes this file directly -- see tests/test_netacuity_api.c) can
// reassign it to force a deterministic socket-creation failure. Defaults to
// the real socket() syscall.
static int (*na_socket_create)(int, int, int) = socket;

/******************************* API Routines *******************************/

// ****************************************************************************
//
// Function: na_client_new()
//
// Purpose: Allocates and initializes a new client handle bound to a single
//          NetAcuity Server, api_id, and timeout. The returned handle must
//          be passed explicitly to every na_query_*() call and must be
//          released with na_client_free() when no longer needed.
//
// Parameters: server_ip - The IP address in dotted notation of the
//                          NetAcuity Server to use.
//             api_id    - Integer between 0 and 127. 0 is left as
//                          "unregistered".
//             timeout_us - Maximum time to wait on network calls, in
//                          microseconds (this includes both connect and
//                          query). There is no default -- pass 2000000
//                          (2 seconds) for a typical timeout.
//
// Returns: A new client handle, or NULL if server_ip is not a well-formed
//          IPv4 or IPv6 literal, api_id is not between 0 and 127, or if
//          allocation fails.
//
// ****************************************************************************
na_client_t *na_client_new(const char *server_ip, int api_id, int timeout_us)
{
    struct in6_addr addr_valid_check;
    int family;
    na_client_t *client;

    if ( !server_ip )
    {
        return NULL;
    }

    if ( api_id < 0 || api_id > 127 )
    {
        return NULL;
    }

    if ( strchr(server_ip, ':') == NULL )
    {
        // Assume this one is an IPv4 address.
        family = AF_INET;
    }
    else
    {
       // Assume this one is an IPv6 address.
       family = AF_INET6;
    }
    if ( inet_pton( family, server_ip, &(addr_valid_check) ) <= 0 )
    {
        return NULL;
    }

    client = (na_client_t *)malloc(sizeof(*client));
    if ( !client )
    {
        return NULL;
    }

    client->ip_family = family;
    strncpy(client->server_addr, server_ip, sizeof(client->server_addr) - 1);
    client->server_addr[sizeof(client->server_addr) - 1] = '\0';
    client->api_id  = api_id;
    client->timeout = timeout_us;

    return client;
}

// ****************************************************************************
//
// Function: na_client_new_with_default_api_id()
//
// Purpose: Convenience wrapper around na_client_new() for callers that don't
//          have a registered api_id, defaulting it to 0 ("unregistered").
//
// Parameters: server_ip  - See na_client_new().
//             timeout_us - See na_client_new().
//
// Returns: See na_client_new().
//
// ****************************************************************************
na_client_t *na_client_new_with_default_api_id(const char *server_ip, int timeout_us)
{
    return na_client_new(server_ip, 0, timeout_us);
}

// ****************************************************************************
//
// Function: na_client_new_with_default_timeout()
//
// Purpose: Convenience wrapper around na_client_new() for callers that don't
//          have a specific timeout requirement, defaulting it to
//          NA_DEFAULT_TIMEOUT_US (2 seconds).
//
// Parameters: server_ip - See na_client_new().
//             api_id    - See na_client_new().
//
// Returns: See na_client_new().
//
// ****************************************************************************
na_client_t *na_client_new_with_default_timeout(const char *server_ip, int api_id)
{
    return na_client_new(server_ip, api_id, NA_DEFAULT_TIMEOUT_US);
}

// ****************************************************************************
//
// Function: na_client_free()
//
// Purpose: Releases a client handle allocated by na_client_new(). Passing
//          NULL is a no-op.
//
// Parameters: client - The client handle to release.
//
// ****************************************************************************
void na_client_free(na_client_t *client)
{
    free(client);
}

// ****************************************************************************
//
// Function: na_query_xml()
//
// Purpose: Queries the NetAcuity databases specified in feature_codes for
//          the IP address passed and returns the response in the memory
//          supplied by the caller.  The response will be an XML string
//          that can be passed to your favorite XML parser.
//          Make sure that the buffer "response" is large enough to handle
//          the entire response, otherwise an error will occur and no
//          response data will be returned.
//          If a timeout or error occurs, it returns 0 and the
//          error reason will be returned in "response".
//
// Parameters: ip          - IP address to query in dotted-notation
//             feature_codes - Comma separated list of database features to query
//             transaction_id - A user-supplied transaction ID (can be "", or
//                           NULL to have one generated automatically)
//             response    - Pointer to location response should be returned
//             max_size    - The size of the response memory.  An error will
//                           occur if response exceeds this size.
//
//    Returns:  1 - Success
//              0 - Error
// ****************************************************************************
int na_query_xml(na_client_t *client, const char *query_ip, const char *feature_code, const char *transaction_id,
    char *response, size_t max_size, char *error_msg)
{
    int                sockfd;              // Socket
    int                ret;                 // Hold various function return
                                            // values
    fd_set             fds;                 // Descriptor set for select()
    struct timeval     tv;                  // Time structure
    int                len;                 // Length parameter for send() call
    api_xml_response   server_response;
    unsigned int       dest_len;
    char               feature_string[MAX_FEATURE_CSV_LEN];  // csv of feature-codes
    char               *field;
    char               *temp_buf;
    int                db_number = 0;
    char               request[MAX_XML_REQUEST_BUFF_SIZE];
    int                temp_result;
    unsigned int       request_temp_len = 0;
    int                dbs_found = 0;
    int                num_packets_received = 0;
    int                packets_to_receive = 1;
    int                last_packet_num = 0;
    char               thisPacketNum_str[XML_PACKET_NUM_DIGITS + 1];
    char               numPacketsToReceive_str[XML_PACKET_NUM_DIGITS + 1];
    size_t             response_len;
    struct sockaddr_in6 dest_addr;           // Will hold the destination addr
    struct sockaddr_in dest_addr4;           // Will hold the destination addr
    struct in_addr api_server;
    char               generated_transaction_id[MAX_TRANSACTION_ID];

    if ( !client )
    {
        strcpy(error_msg, "Invalid client handle.");
        return 0;
    }

    // A caller that omits a transaction ID (passes NULL) gets one generated automatically.
    if ( !transaction_id )
    {
        generate_transaction_id(generated_transaction_id, sizeof(generated_transaction_id));
        transaction_id = generated_transaction_id;
    }

    // A '"', '<', '>', or '&' in transaction_id would break the hand-built XML attribute
    // syntax below. Checked first, ahead of every branch that could otherwise embed an
    // unvalidated transaction_id in a response.
    if (strpbrk(transaction_id, "\"<>&") != NULL)
    {
        strcpy(error_msg, "Invalid transaction ID.");
        return 0;
    }

    /* zero buffers */
    bzero(thisPacketNum_str, sizeof(thisPacketNum_str));
    bzero(numPacketsToReceive_str, sizeof(numPacketsToReceive_str));
    strcpy(error_msg, "");

    memset(request, 0, sizeof(request));
    memset(&server_response, 0, sizeof(server_response));

    response[0] = '\0';
    if (strlen(feature_code) >= sizeof(feature_string))
    {
        strcpy(error_msg, "Feature-code list is too long.");
        return 0;
    }
    strncpy(feature_string, feature_code, sizeof(feature_string)-1);
    feature_string[sizeof(feature_string) - 1] = '\0';

    // Make sure query_ip is a well-formed IPv4 or IPv6 literal before it's
    // interpolated into the request string.
    {
        struct in6_addr ip_valid_check;
        if (inet_pton(AF_INET, query_ip, &ip_valid_check) <= 0 &&
            inet_pton(AF_INET6, query_ip, &ip_valid_check) <= 0)
        {
            strcpy(error_msg, "Invalid query IP address.");
            return 0;
        }
    }

    /* Build the request */
    temp_result = snprintf(request, sizeof(request),
        "<request trans-id=\"%s\" ip=\"%s\" api-id=\"%d\" >",
        transaction_id, query_ip, client->api_id);
    if (temp_result < 0) {
        strcpy(error_msg, "Encoding error occurred while building request.");
        return 0;   // an encoding error occurred
    }
    request_temp_len = (unsigned int)temp_result;
    if (request_temp_len > sizeof(request)) {
        //request buffer is too small
        strcpy(error_msg, "Request buffer is too small.");
        return 0;
    }

    dbs_found = 0;
    field     = (char *) strtok_r(feature_string, ",", &temp_buf);
    while ( field )
    {
        db_number = atoi(field);
        /* Make sure the db to be queried is >= 3 and < 100 */
        if ( (db_number >= NA_MAX_DB_FEATURE) || (db_number < NA_MIN_DB_FEATURE) )
        {
            snprintf(error_msg, MAX_ERROR_MSG_LEN, "Request for feature %d is invalid.", db_number);
            return 0;
        }
        // Add the db-specific query tag to the request
        temp_result = snprintf(request + request_temp_len, sizeof(request) - request_temp_len, "<query db=\"%d\" />", db_number);
        if (temp_result < 0) {
            strcpy(error_msg, "Encoding error occurred while building request.");
            return 0;   // an encoding error occurred
        }
        request_temp_len += (unsigned int)temp_result;

        if (request_temp_len > sizeof(request)) {
            //request buffer is too small
            strcpy(error_msg, "Request buffer is too small.");
            return 0;
        }
        field = (char *)strtok_r(NULL, ",", &temp_buf);

        dbs_found++;
    }

    // Add the final closing tag to the request
    temp_result = snprintf(request + request_temp_len, sizeof(request) - request_temp_len, "</request>");
    if (temp_result < 0) {
        strcpy(error_msg, "Encoding error occurred while building request.");
        return 0;   // an encoding error occurred
    }
    request_temp_len += (unsigned int)temp_result;
    if (request_temp_len > sizeof(request)) {
        //request buffer is too small
        strcpy(error_msg, "Request buffer is too small.");
        return 0;
    }

    if ( !dbs_found )
    {
        strcpy(error_msg, "No databases were specified in request.");
        return 0;
    }

    // Create and check socket, uses UDP
    // Initialize IP connect structure

    // Must match client->ip_family: sendto() rejects a sockaddr whose family doesn't
    // match the socket's own address family.
     ret                = sockfd = na_socket_create(client->ip_family == AF_INET ? PF_INET : PF_INET6, SOCK_DGRAM, 0);
     if ( ret == -1 )
     {
         snprintf(error_msg, MAX_ERROR_MSG_LEN, "Could not create socket: %s.", strerror(errno));
         return 0;
     }

     /* make it non-blocking, so we can timeout without using signals */
     ret = set_nonblock_flag(sockfd, 1);
     if ( ret == -1 )
     {
         close(sockfd);
         snprintf(error_msg, MAX_ERROR_MSG_LEN, "Could not set socket to non-blocking: %s.", strerror(errno));
         return 0;
     }

     len = strlen(request);

     if (client->ip_family == AF_INET)
     {
         memset(&dest_addr, 0, sizeof(dest_addr));
         /* it is an INET address */
         dest_addr4.sin_family = client->ip_family;
         dest_addr4.sin_port = htons(API_SERVER_UDP_PORT);

         /* the server IP address, in network byte order */
         api_server.s_addr = inet_addr(client->server_addr);
         dest_addr4.sin_addr   = api_server;
         bzero(&(dest_addr4.sin_zero), sizeof(dest_addr4.sin_zero));       // zero the rest of the struct

          // Restricts the OS to only deliver datagrams from this address on this socket,
          // rejecting spoofed/stray packets from any other source at the kernel level.
          if (connect(sockfd, (struct sockaddr *)&dest_addr4, sizeof(dest_addr4)) == -1)
          {
              close(sockfd);
              snprintf(error_msg, MAX_ERROR_MSG_LEN, "Unable to connect to NetAcuity Server: %s.", strerror(errno));
              return 0;
          }

          dest_len = sizeof(dest_addr4);

          // send request string to socket
          ret=sendto(sockfd, request, len, 0, (struct sockaddr *)&dest_addr4,
              dest_len);
     }
     else
     {
         memset(&dest_addr, 0, sizeof(dest_addr));
         if (inet_pton(client->ip_family, client->server_addr, &dest_addr.sin6_addr) == -1)
         {
             close(sockfd);
             strcpy(error_msg, "Invalid NetAcuity server address.");
             return 0;
         }
         dest_addr.sin6_family = client->ip_family;
         // the port we are going to send to, in network byte order
         dest_addr.sin6_port = htons(API_SERVER_UDP_PORT);

         // Restricts the OS to only deliver datagrams from this address on this socket,
         // rejecting spoofed/stray packets from any other source at the kernel level.
         if (connect(sockfd, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) == -1)
         {
             close(sockfd);
             snprintf(error_msg, MAX_ERROR_MSG_LEN, "Unable to connect to NetAcuity Server: %s.", strerror(errno));
             return 0;
         }

         // send string to socket
         dest_len = sizeof(dest_addr);
         len     = strlen(request);
         ret      = sendto(sockfd, request, len, 0,
                                  (struct sockaddr *)&dest_addr, dest_len);
     }
     if ( ret < len )
     {
         /* this shouldn't happen, bail out */
         close(sockfd);
         snprintf(error_msg, MAX_ERROR_MSG_LEN, "Sent fewer bytes than expected (sent: %d, expected: %d).", ret, len);
         return 0;
     }

    /* make timeout structure */
    tv.tv_sec = 0;
    tv.tv_usec = client->timeout;

    /* Deal with Solaris issue with US being > 1000000 (1 sec) */
    while ( tv.tv_usec >= 1000000 )
    {
        tv.tv_usec -= 1000000;
        tv.tv_sec++;
    }

    num_packets_received = 0;
    packets_to_receive = 1;  // this will get updated on first packet
    last_packet_num = 0;
    response_len = 0;
    while ( num_packets_received < packets_to_receive )
    {
        // clear any stale data from a previous packet before receiving the next one
        memset(&server_response, 0, sizeof(server_response));

        // wait for the response data
        FD_ZERO(&fds);
        FD_SET(sockfd, &fds);

        tv.tv_sec = 0;
        tv.tv_usec = client->timeout;

        /* Deal with Solaris issue with US being > 1000000 (1 sec) */
        while ( tv.tv_usec >= 1000000 )
        {
            tv.tv_usec -= 1000000;
            tv.tv_sec++;
        }

        /* wait until the timeout or we have data from the server */
        ret=select(sockfd+1, &fds, NULL, NULL, &tv);

        /* For error or timeout */
        if ( ret <= 0 )
        {
            close(sockfd);
            if (ret == 0)
            {
                strcpy(error_msg, "Timeout waiting for response from NetAcuity Server.");
            }
            else
            {
                snprintf(error_msg, MAX_ERROR_MSG_LEN, "Error waiting for response from NetAcuity Server: %s.", strerror(errno));
            }
            return 0;
        }

        /* Receive information from server */
        ret = recvfrom(sockfd, &server_response, sizeof(server_response), 0,
            (struct sockaddr *)&dest_addr, &dest_len);

        /* Check to see if got actual good data off socket */
        if ( ret < 6 )
        {
            close(sockfd);
            strcpy(error_msg, "Received an invalid response from the NetAcuity Server.");
            return 0;
        }
        strncpy(thisPacketNum_str, server_response.thisPacketNum, XML_PACKET_NUM_DIGITS);
        strncpy(numPacketsToReceive_str, server_response.numPacketsToReceive, XML_PACKET_NUM_DIGITS);
        num_packets_received = atoi(thisPacketNum_str);
        packets_to_receive = atoi(numPacketsToReceive_str);

        if ( num_packets_received - 1 != last_packet_num )
        {
            close(sockfd);
            snprintf(error_msg, MAX_ERROR_MSG_LEN, "Packets received out of order (got: %d, last: %d).", num_packets_received, last_packet_num);
            return 0;

        }

        /* make sure we don't overflow the buffer passed */
        response_len += strlen(server_response.response);
        if ( response_len >= max_size )
        {
            close(sockfd);
            strcpy(error_msg, "Response will exceed passed buffer size.");
            return 0;
        }

        /* add this packet to the response buffer */
        strcat(response, server_response.response);
        last_packet_num = num_packets_received;
    }
    // close the socket
    close(sockfd);

    get_xml_field(response, "error", error_msg, MAX_ERROR_MSG_LEN);
    if (strcmp(error_msg, "FieldNotFound") == 0)
    {
        strcpy(error_msg, "");
    }
    else
    {
        return 0;
    }

    // Make sure the response is in sync with this request.
    {
        char response_trans_id[MAX_TRANSACTION_ID];
        get_xml_field(response, "trans-id", response_trans_id, sizeof(response_trans_id));
        if (strcmp(response_trans_id, transaction_id) != 0)
        {
            strcpy(error_msg, "Transaction ID mismatch.");
            return 0;
        }
    }
    {
        char response_ip[MAX_IP_LEN];
        get_xml_field(response, "ip", response_ip, sizeof(response_ip));
        if (!ip_strings_equal(response_ip, query_ip))
        {
            strcpy(error_msg, "Response address mismatch.");
            return 0;
        }
    }

    return 1;  // success
}

// ****************************************************************************
//
// Function: get_xml_field()
//
//      THIS FUNCTION IS DEPRECATED.
//                It is recommended to use libxml2 instead.
//
// Purpose: Retrieves the value of the field name passed from the
//          response string returned from na_query_xml().
//
// Parameters: response   -  Pointer to the XML response from na_query_xml()
//             field_name -  Name of the field to retrieve
//             value      -  Pointer to location value should be returned --
//                           MUST be at least 17 bytes but large enough to
//                           hold response.
//                           Will contain "FieldNotFound" if requested field
//                           not in response.
//                           If the field's value is longer than will fit in
//                           the supplied buffer, it is safely truncated to
//                           fit (and still null-terminated) rather than
//                           discarded.
//                           All values returned are strings (char[]) -- you must
//                           use atoi() to convert into numeric values
//                           (if appropriate).
//                size    -  The size of the value memory.  An error will
//                           occur if response exceeds this size.
//
// Returns: Pointer to value string.
//
// ****************************************************************************
char *get_xml_field(const char *response, const char *field_name, char *value, size_t size)
{
    //      THIS FUNCTION IS DEPRECATED.
    //                It is recommended to use libxml2 instead.
    const char *fs;
    const char *fe;
    int  len;
    char field_name_str[MAX_FIELD_NAME_LEN];

    snprintf(field_name_str, sizeof(field_name_str), " %s=\"", field_name);

    fs = strstr(response, field_name_str);
    if ( !fs )
    {
        snprintf(value, size, "FieldNotFound");
        return value;
    }

    fs = strstr(fs, "\"");      //find the first (starting) quote char
    if ( !fs )
    {
        snprintf(value, size, "FieldNotFound");
        return value;
    }
    fs++;                       //refer to next char after it
    fe = strstr(fs, "\"");      //find the second (ending) quote char

    if ( !fe )
    {
        snprintf(value, size, "FieldNotFound");
        return value;
    }

    len = fe - fs;
    if ( len < 0 )
    {
        len = 0;
    }
    if ( size > 0 && (size_t)len >= size )
    {
        // Doesn't fit in the caller's buffer -- copy as much as fits (safely
        // truncated to size-1 bytes, still null-terminated) instead of
        // discarding the real value and returning an unrelated placeholder.
        len = (int)size - 1;
    }
    snprintf(value, size, "%*.*s", len, len, fs);

    return value;
}

