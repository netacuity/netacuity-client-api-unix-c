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
 * Unit tests for the NetAcuity Client API (Unix C)
 *
 * Most of these tests need no NetAcuity server:
 *   - na_client_new() input validation
 *   - ip_strings_equal() address comparison
 *   - get_xml_field() XML attribute extraction
 *   - NULL-client and out-of-range input validation for na_query_xml()
 *     (return paths that fire before any socket call)
 *   - socket() failure handling, via the na_socket_create seam
 *
 * The rest exercise the network paths of na_query_xml() and of the
 * na_connection_*() persistent-connection API against a mock NetAcuity
 * server, forked in-process and listening on 127.0.0.1:API_SERVER_UDP_PORT
 * (they are skipped if that port is unavailable).
 *
 * Compile and run (Linux/Unix):
 *
 *   cd unix_c/tests && make test     (or "make test" from the top level)
 *
 * The netacuity_api.c source file is included directly so that the
 * static (internal) na_socket_create seam can be reassigned without
 * modification to the production source.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>  /* waitpid(), used by the mock servers */
#include <signal.h>    /* signal(), used by test_send_failure() */
#include <sys/resource.h>  /* getrlimit(), used by test_connection_high_fd() */
#include <errno.h>
#include <sys/socket.h>

/*
 * recv() shim for the library: if recv_inject_errno is set, the library's
 * next recv() fails with that errno (once) instead of reading the socket.
 * While recv_fake_datagrams is positive, each recv() instead "receives" a
 * one-byte datagram and decrements it.  Used by test_connection_drain_errors()
 * and test_connection_drain_bound().
 */
static int recv_inject_errno = 0;
static int recv_fake_datagrams = 0;

static ssize_t test_recv(int fd, void *buf, size_t len, int flags)
{
    if (recv_inject_errno)
    {
        errno = recv_inject_errno;
        recv_inject_errno = 0;
        return -1;
    }
    if (recv_fake_datagrams > 0 && len > 0)
    {
        recv_fake_datagrams--;
        ((char *)buf)[0] = 'x';
        return 1;
    }
    return recv(fd, buf, len, flags);
}

/*
 * Pull in the full implementation.  This gives access to static helpers such
 * as na_socket_create, and makes all macros defined in netacuity_api.h
 * (MAX_XML_RESPONSE_SIZE, MAX_ERROR_MSG_LEN, …) available without a separate
 * include.
 */
#define recv test_recv
#include "../netacuity_api.c"
#undef recv

#include "../netacuity_fields.h"  /* field dictionary, used by the nginx module */


/* =========================================================================
 * Minimal test framework
 * ========================================================================= */

static int total_passed = 0;
static int total_failed = 0;

static void run_test(const char *desc, int ok)
{
    if (ok) {
        printf("  PASS  %s\n", desc);
        total_passed++;
    } else {
        printf("  FAIL  %s\n", desc);
        total_failed++;
    }
}

#define CHECK(desc, cond) \
    run_test(desc, (int)(cond))

#define CHECK_INT(desc, expected, actual) \
    run_test(desc, (int)(expected) == (int)(actual))

#define CHECK_STR(desc, expected, actual) \
    run_test(desc, strcmp((expected), (actual)) == 0)

static void section(const char *name)
{
    printf("\n--- %s ---\n", name);
}


/* =========================================================================
 * API_SERVER_UDP_PORT
 *
 * The destination UDP port used to reach the NetAcuity server is fixed by
 * the real NetAcuity Server protocol (external to this client) and must
 * always be 5400.  This check is network-free -- it only inspects the
 * #define'd constant from netacuity_api.h, guarding against silent drift
 * (bad refactor, typo, "clean up magic numbers" pass, etc.).
 * ========================================================================= */

static void test_api_server_port(void)
{
    section("API_SERVER_UDP_PORT");

    CHECK_INT("API_SERVER_UDP_PORT is 5400", 5400, API_SERVER_UDP_PORT);
}


/* =========================================================================
 * Feature codes and the field dictionary (netacuity_fields.h)
 *
 * Every database's field IDs start at its feature code * 100, and each
 * field name appears once.  The newest feature codes are also checked
 * against the server's definitions (de-database/MySQLDB, customer.feature_fields).
 * ========================================================================= */

static const netacuity_field *find_field(const char *field_name)
{
    const netacuity_field *f;
    for (f = netacuity_field_dictionary; f->feature_code != 0; f++)
    {
        if (strcmp(f->field_name, field_name) == 0)
        {
            return f;
        }
    }
    return NULL;
}

static void check_field(unsigned int feature_code, netacuity_field_type field_type, const char *field_name)
{
    char desc[128];
    const netacuity_field *f = find_field(field_name);

    snprintf(desc, sizeof(desc), "FC %u: \"%s\" is in the dictionary", feature_code, field_name);
    CHECK(desc, f != NULL);
    if (f != NULL)
    {
        snprintf(desc, sizeof(desc), "FC %u: \"%s\" has the right feature code and type", feature_code, field_name);
        CHECK(desc, f->feature_code == feature_code && f->field_type == field_type);
    }
}

static void test_field_dictionary(void)
{
    section("feature codes and field dictionary");

    CHECK_INT("NA_DMA is 38",      38, NA_DMA);
    CHECK_INT("NA_DMA_MAX is 39",  39, NA_DMA_MAX);
    CHECK_INT("NA_RMM is 51",      51, NA_RMM);
    CHECK_INT("NA_RMM_MAX is 52",  52, NA_RMM_MAX);

    check_field(NA_DMA,     NA_FLD_TYPE_INT,    "dma-code");
    check_field(NA_DMA,     NA_FLD_TYPE_STRING, "dma-regions");
    check_field(NA_DMA,     NA_FLD_TYPE_STRING, "dma-name");
    check_field(NA_DMA_MAX, NA_FLD_TYPE_INT,    "dmamax-code");
    check_field(NA_DMA_MAX, NA_FLD_TYPE_STRING, "dmamax-regions");
    check_field(NA_DMA_MAX, NA_FLD_TYPE_STRING, "dmamax-name");
    check_field(NA_RMM,     NA_FLD_TYPE_STRING, "rmm-country");
    check_field(NA_RMM,     NA_FLD_TYPE_STRING, "rmm-id");
    check_field(NA_RMM_MAX, NA_FLD_TYPE_STRING, "rmmmax-country");
    check_field(NA_RMM_MAX, NA_FLD_TYPE_STRING, "rmmmax-id");

    int bad_id = 0, duplicate_name = 0;
    const netacuity_field *f, *g;
    for (f = netacuity_field_dictionary; f->feature_code != 0; f++)
    {
        if ((unsigned int)f->field_id / 100 != f->feature_code)
        {
            printf("        field \"%s\" (FC %u) has ID %d\n", f->field_name, f->feature_code, (int)f->field_id);
            bad_id = 1;
        }
        for (g = f + 1; g->feature_code != 0; g++)
        {
            if (strcmp(f->field_name, g->field_name) == 0)
            {
                printf("        field \"%s\" appears more than once\n", f->field_name);
                duplicate_name = 1;
            }
        }
    }
    CHECK("every field ID is in its feature code's range (FC * 100)", !bad_id);
    CHECK("every field name appears once", !duplicate_name);
}


/* =========================================================================
 * na_client_new
 * ========================================================================= */

// Creates a client, asserts whether creation succeeded (non-NULL) or failed
// (NULL) as expected, and frees it if it succeeded.
static void check_client_new(const char *desc, int expect_success, const char *server_ip)
{
    na_client_t *client = na_client_new(server_ip, 0, 2000000);
    run_test(desc, (client != NULL) == expect_success);
    na_client_free(client);
}

static void test_client_new(void)
{
    section("na_client_new");

    /* --- Valid IPv4 addresses --- */
    check_client_new("valid IPv4: 192.0.2.1",           1, "192.0.2.1");
    check_client_new("valid IPv4: 198.51.100.1",        1, "198.51.100.1");
    check_client_new("valid IPv4: 203.0.113.1",         1, "203.0.113.1");
    check_client_new("valid IPv4: 192.0.2.0",           1, "192.0.2.0");
    check_client_new("valid IPv4: 203.0.113.255",       1, "203.0.113.255");
    check_client_new("valid IPv4: 198.51.100.100",      1, "198.51.100.100");

    /* --- Invalid IPv4 addresses --- */
    check_client_new("invalid IPv4: 999.999.999.999",   0, "999.999.999.999");
    check_client_new("invalid IPv4: 256.0.0.1",         0, "256.0.0.1");
    check_client_new("invalid IPv4: 192.0.2",           0, "192.0.2");
    check_client_new("invalid IPv4: not-an-ip",         0, "not-an-ip");
    check_client_new("invalid IPv4: abc.def.ghi.jkl",   0, "abc.def.ghi.jkl");
    check_client_new("invalid IPv4: empty string",      0, "");
    check_client_new("invalid IPv4: spaces only",       0, "   ");
    check_client_new("invalid IPv4: 192.0.2.1.5",       0, "192.0.2.1.5");

    /* --- Valid IPv6 addresses (presence of ':' selects IPv6 path) --- */
    check_client_new("valid IPv6: 2001:db8::2",                   1, "2001:db8::2");
    check_client_new("valid IPv6: 2001:db8::1",                   1, "2001:db8::1");
    check_client_new("valid IPv6: 2001:db8:1::1",                 1, "2001:db8:1::1");
    check_client_new("valid IPv6: full address",                  1,
              "2001:0db8:85a3:0000:0000:8a2e:0370:7334");
    check_client_new("valid IPv6: compressed zeros",              1,
              "2001:db8:0:0:0:0:0:1");

    /* --- Invalid IPv6 addresses --- */
    check_client_new("invalid IPv6: ::gg",              0, "::gg");
    check_client_new("invalid IPv6: zzzz::1",           0, "zzzz::1");
    check_client_new("invalid IPv6: too many groups",   0,
              "2001:db8:85a3:0:0:8a2e:370:7334:extra");
    check_client_new("invalid IPv6: invalid:chars:",    0, "invalid:chars:");

    /* --- NULL server_ip --- */
    check_client_new("NULL server_ip",                  0, NULL);

    /* --- api_id range (0-127); validated at construction time --- */
    na_client_t *api_id_0 = na_client_new("192.0.2.1", 0, 2000000);
    CHECK("valid api_id: 0", api_id_0 != NULL);
    na_client_free(api_id_0);

    na_client_t *api_id_127 = na_client_new("192.0.2.1", 127, 2000000);
    CHECK("valid api_id: 127", api_id_127 != NULL);
    na_client_free(api_id_127);

    CHECK("invalid api_id: -1 returns NULL",  na_client_new("192.0.2.1", -1, 2000000) == NULL);
    CHECK("invalid api_id: 128 returns NULL", na_client_new("192.0.2.1", 128, 2000000) == NULL);

    /* --- timeout_us must be positive --- */
    na_client_t *timeout_1 = na_client_new("192.0.2.1", 0, 1);
    CHECK("valid timeout_us: 1", timeout_1 != NULL);
    na_client_free(timeout_1);

    CHECK("invalid timeout_us: 0 returns NULL", na_client_new("192.0.2.1", 0, 0) == NULL);
    CHECK("invalid timeout_us: 0 returns NULL (default api_id)",
          na_client_new_with_default_api_id("192.0.2.1", 0) == NULL);
    CHECK("invalid timeout_us: -1 returns NULL", na_client_new("192.0.2.1", 0, -1) == NULL);
    CHECK("invalid timeout_us: -1 returns NULL (default api_id)",
          na_client_new_with_default_api_id("192.0.2.1", -1) == NULL);
}


/* =========================================================================
 * ip_strings_equal  (static — accessed via #include "../netacuity_api.c")
 * ========================================================================= */

static void test_ip_strings_equal(void)
{
    section("ip_strings_equal");

    /* --- Same IPv6 address, different textual forms --- */
    CHECK("compressed vs. fully expanded",
          ip_strings_equal("2001:db8::1", "2001:0db8:0000:0000:0000:0000:0000:0001"));
    CHECK("case difference",
          ip_strings_equal("2001:DB8::1", "2001:db8::1"));
    CHECK("loopback, compressed vs. expanded",
          ip_strings_equal("::1", "0:0:0:0:0:0:0:1"));
    CHECK("unspecified address, compressed vs. expanded",
          ip_strings_equal("::", "0:0:0:0:0:0:0:0"));

    /* --- Genuinely different addresses --- */
    CHECK("different IPv6 addresses are not equal",
          !ip_strings_equal("2001:db8::1", "2001:db8::2"));
    CHECK("different IPv6 loopback-like addresses are not equal",
          !ip_strings_equal("::1", "::2"));

    /* --- IPv4 --- */
    CHECK("identical IPv4 literals are equal",
          ip_strings_equal("192.0.2.1", "192.0.2.1"));
    CHECK("different IPv4 literals are not equal",
          !ip_strings_equal("192.0.2.1", "192.0.2.2"));

    /* --- Mixed / malformed input never crashes and is treated as unequal --- */
    CHECK("IPv4 literal is never equal to an IPv6 literal",
          !ip_strings_equal("192.0.2.1", "::ffff:192.0.2.1"));
    CHECK("malformed literal on the left is not equal to anything",
          !ip_strings_equal("not-an-ip", "2001:db8::1"));
    CHECK("malformed literal on the right is not equal to anything",
          !ip_strings_equal("2001:db8::1", "not-an-ip"));
}


/* =========================================================================
 * get_xml_field
 * ========================================================================= */

static void test_get_xml_field(void)
{
    section("get_xml_field");

    char value[256];

    /* --- Basic retrieval --- */
    const char *r1 = "<response ip=\"192.0.2.1\" country=\"usa\" region=\"nc\" />";

    get_xml_field(r1, "ip", value, sizeof(value));
    CHECK_STR("finds 'ip' attribute",      "192.0.2.1", value);

    get_xml_field(r1, "country", value, sizeof(value));
    CHECK_STR("finds 'country' attribute", "usa",     value);

    get_xml_field(r1, "region", value, sizeof(value));
    CHECK_STR("finds 'region' attribute",  "nc",      value);

    /* --- Not found --- */
    get_xml_field(r1, "city", value, sizeof(value));
    CHECK_STR("absent attribute returns FieldNotFound", "FieldNotFound", value);

    get_xml_field(r1, "nonexistent", value, sizeof(value));
    CHECK_STR("unknown attribute returns FieldNotFound", "FieldNotFound", value);

    /* --- Partial name must NOT match (search pattern is " name=\"") --- */
    get_xml_field(r1, "i", value, sizeof(value));
    CHECK_STR("prefix 'i' does not match 'ip'", "FieldNotFound", value);

    get_xml_field(r1, "coun", value, sizeof(value));
    CHECK_STR("prefix 'coun' does not match 'country'", "FieldNotFound", value);

    /* --- Empty attribute value --- */
    const char *r2 = "<response error=\"\" ip=\"192.0.2.1\" />";
    get_xml_field(r2, "error", value, sizeof(value));
    CHECK_STR("empty attribute value returns empty string", "", value);

    /* --- Buffer too small: safely truncated, not discarded ---
     * Use a 30-char value with a 20-byte buffer: the real text is preserved,
     * truncated to size-1=19 bytes and still null-terminated, rather than
     * being replaced with a placeholder. */
    const char *r_long = "<response note=\"this-value-exceeds-buffer-size\" />";
    char buf20[20];
    get_xml_field(r_long, "note", buf20, sizeof(buf20));
    CHECK_STR("value exceeding buffer is truncated, not discarded", "this-value-exceeds-", buf20);

    char tiny[10];  /* "192.0.2.1" is 9 chars + NUL = 10; len=9, size=10: 9 < 10 → fits */
    get_xml_field(r1, "ip", tiny, sizeof(tiny));
    CHECK_STR("value exactly fits buffer returns value", "192.0.2.1", tiny);

    /* --- Attribute in an inner element --- */
    const char *r3 = "<response trans-id=\"txn001\" ip=\"192.0.2.1\" error=\"\">"
                     "<db fc=\"3\" country=\"usa\" city=\"durham\" />"
                     "</response>";
    get_xml_field(r3, "trans-id", value, sizeof(value));
    CHECK_STR("finds trans-id attribute",       "txn001", value);

    get_xml_field(r3, "fc", value, sizeof(value));
    CHECK_STR("finds inner element attribute",  "3",      value);

    get_xml_field(r3, "city", value, sizeof(value));
    CHECK_STR("finds city in inner element",    "durham", value);

    /* --- Error attribute in a real-looking NA error response --- */
    const char *r4 = "<response trans-id=\"t1\" ip=\"192.0.2.1\" "
                     "error=\"DB Not Loaded\" />";
    get_xml_field(r4, "error", value, sizeof(value));
    CHECK_STR("finds error message",            "DB Not Loaded", value);

    get_xml_field(r4, "ip", value, sizeof(value));
    CHECK_STR("finds ip in error response",     "192.0.2.1", value);

    /* --- Presence of api-id must not confuse search for ip --- */
    const char *r5 = "<request trans-id=\"t1\" ip=\"198.51.100.1\" api-id=\"42\" />";
    get_xml_field(r5, "ip", value, sizeof(value));
    CHECK_STR("'ip' found when 'api-id' also present", "198.51.100.1", value);

    get_xml_field(r5, "api-id", value, sizeof(value));
    CHECK_STR("'api-id' found correctly",       "42", value);

    /* --- Multi-line / multi-db XML response --- */
    const char *r6 =
        "<response trans-id=\"abc\" ip=\"203.0.113.1\">"
        "<db fc=\"3\" country=\"can\" region=\"on\" city=\"toronto\" "
        "conn-speed=\"broadband\" country-conf=\"99\" region-conf=\"95\" "
        "city-conf=\"80\" />"
        "<db fc=\"26\" pulse-country=\"can\" pulse-city=\"toronto\" />"
        "</response>";

    get_xml_field(r6, "country", value, sizeof(value));
    CHECK_STR("multi-db: finds first 'country'", "can", value);

    get_xml_field(r6, "city", value, sizeof(value));
    CHECK_STR("multi-db: finds 'city'",          "toronto", value);

    get_xml_field(r6, "country-conf", value, sizeof(value));
    CHECK_STR("multi-db: finds 'country-conf'",  "99", value);

    get_xml_field(r6, "pulse-country", value, sizeof(value));
    CHECK_STR("multi-db: finds 'pulse-country'", "can", value);
}


/* =========================================================================
 * Input validation for na_query_xml
 *
 * These checks all fire BEFORE any socket is created, so no server needed.
 * ========================================================================= */

static void test_query_xml_validation(void)
{
    section("na_query_xml input validation (no server required)");

    char xml_response[MAX_XML_RESPONSE_SIZE];
    char error_msg[MAX_ERROR_MSG_LEN];

    na_client_t *client = na_client_new("192.0.2.1", 0, 2000000);

    /* Invalid feature code (< 3) */
    memset(xml_response, 0, sizeof(xml_response));
    memset(error_msg, 0, sizeof(error_msg));
    CHECK_INT("feature 0: returns 0",
              0, na_query_xml(client, "192.0.2.1", "0", "", xml_response, sizeof(xml_response), error_msg));

    memset(xml_response, 0, sizeof(xml_response));
    memset(error_msg, 0, sizeof(error_msg));
    CHECK_INT("feature 2: returns 0",
              0, na_query_xml(client, "192.0.2.1", "2", "", xml_response, sizeof(xml_response), error_msg));

    /* Invalid feature code (>= 100) */
    memset(xml_response, 0, sizeof(xml_response));
    memset(error_msg, 0, sizeof(error_msg));
    CHECK_INT("feature 100: returns 0",
              0, na_query_xml(client, "192.0.2.1", "100", "", xml_response, sizeof(xml_response), error_msg));

    /* Feature code that was valid under the old maximum (<500) but is now
     * rejected under the current maximum (<100) */
    memset(xml_response, 0, sizeof(xml_response));
    memset(error_msg, 0, sizeof(error_msg));
    CHECK_INT("feature 150: returns 0",
              0, na_query_xml(client, "192.0.2.1", "150", "", xml_response, sizeof(xml_response), error_msg));

    memset(xml_response, 0, sizeof(xml_response));
    memset(error_msg, 0, sizeof(error_msg));
    CHECK_INT("feature 500: returns 0",
              0, na_query_xml(client, "192.0.2.1", "500", "", xml_response, sizeof(xml_response), error_msg));

    memset(xml_response, 0, sizeof(xml_response));
    memset(error_msg, 0, sizeof(error_msg));
    CHECK_INT("feature 999: returns 0",
              0, na_query_xml(client, "192.0.2.1", "999", "", xml_response, sizeof(xml_response), error_msg));

    /* Empty feature string (no databases specified) */
    memset(xml_response, 0, sizeof(xml_response));
    memset(error_msg, 0, sizeof(error_msg));
    CHECK_INT("empty feature string: returns 0",
              0, na_query_xml(client, "192.0.2.1", "", "", xml_response, sizeof(xml_response), error_msg));

    /* A client-side validation failure (here, an invalid feature code) is
     * rejected before anything is ever sent to a server, so na_query_xml()
     * must not fabricate XML response text -- error_msg is the only
     * diagnostic channel, and xml_response is cleared to the empty string. */
    strcpy(xml_response, "poisoned");
    memset(error_msg, 0, sizeof(error_msg));
    CHECK_INT("invalid feature: returns 0",
              0, na_query_xml(client, "192.0.2.1", "2", "mytxn", xml_response, sizeof(xml_response), error_msg));
    CHECK_STR("invalid feature: xml_response is cleared to empty (not fabricated)",
              "", xml_response);
    CHECK_STR("invalid feature: error_msg describes the failure",
              "Request for feature 2 is invalid.", error_msg);

    /* query_ip that isn't a well-formed IPv4/IPv6 literal */
    memset(xml_response, 0, sizeof(xml_response));
    memset(error_msg, 0, sizeof(error_msg));
    CHECK_INT("malformed query_ip: returns 0",
              0, na_query_xml(client, "not-an-ip", "3", "", xml_response, sizeof(xml_response), error_msg));

    /* transaction_id containing '"' would break the hand-built XML attribute syntax */
    memset(xml_response, 0, sizeof(xml_response));
    memset(error_msg, 0, sizeof(error_msg));
    CHECK_INT("transaction_id with quote: returns 0",
              0, na_query_xml(client, "192.0.2.1", "3", "a\"b", xml_response, sizeof(xml_response), error_msg));

    /* transaction_id that won't fit in MAX_TRANSACTION_ID (including NUL) */
    char long_txn[MAX_TRANSACTION_ID + 1];
    memset(long_txn, 'a', sizeof(long_txn) - 1);
    long_txn[MAX_TRANSACTION_ID] = '\0';
    memset(error_msg, 0, sizeof(error_msg));
    CHECK_INT("transaction_id of MAX_TRANSACTION_ID chars: returns 0",
              0, na_query_xml(client, "192.0.2.1", "3", long_txn, xml_response, sizeof(xml_response), error_msg));
    CHECK_STR("transaction_id of MAX_TRANSACTION_ID chars: error_msg",
              "Transaction ID is too long.", error_msg);

    /* api_id outside the valid range (0-127) is now rejected at construction
     * time by na_client_new() itself (see test_client_new()), so there is no
     * longer a query-time api_id check here to exercise. */

    na_client_free(client);
}


/* =========================================================================
 * NULL-client validation for na_query_xml()
 *
 * na_query_xml() checks for a NULL client handle and returns 0 immediately
 * -- before any socket operation -- so this test needs no server. This is
 * the regression test for the na_client_t refactor: a NULL client must
 * never be dereferenced.
 * ========================================================================= */

static void test_null_client_validation(void)
{
    section("na_query_xml: NULL client returns 0 (no server required)");

    char error_msg[MAX_ERROR_MSG_LEN];
    char xml_response[MAX_XML_RESPONSE_SIZE];
    memset(error_msg, 0, sizeof(error_msg));
    CHECK_INT("na_query_xml: NULL client returns 0",
              0, na_query_xml(NULL, "192.0.2.1", "3", "", xml_response, sizeof(xml_response), error_msg));
}


/* =========================================================================
 * na_query_xml: socket() failure is surfaced cleanly
 *
 * na_query_xml() creates its UDP socket through the na_socket_create
 * function pointer (see netacuity_api.c) rather than calling socket()
 * directly, specifically so a test can force a deterministic
 * socket-creation failure without needing to exhaust the process's real
 * file-descriptor table. Confirms the failure is surfaced as a clean error
 * (return 0, error_msg set) rather than a crash.
 * ========================================================================= */

static int fail_socket_create(int domain, int type, int protocol)
{
    (void)domain;
    (void)type;
    (void)protocol;
    errno = EMFILE;
    return -1;
}

static void test_socket_create_failure(void)
{
    section("na_query_xml: socket() failure (via na_socket_create seam)");

    na_client_t *client = na_client_new("192.0.2.1", 0, 2000000);
    char error_msg[MAX_ERROR_MSG_LEN];

    int (*real_socket_create)(int, int, int) = na_socket_create;
    na_socket_create = fail_socket_create;

    char xml_response[MAX_XML_RESPONSE_SIZE];
    memset(error_msg, 0, sizeof(error_msg));
    CHECK_INT("na_query_xml: socket() failure returns 0",
              0, na_query_xml(client, "192.0.2.1", "3", "", xml_response, sizeof(xml_response), error_msg));
    CHECK("na_query_xml: error_msg reports the socket failure",
          strstr(error_msg, "Could not create socket") != NULL);

    na_socket_create = real_socket_create;
    na_client_free(client);
}


/* =========================================================================
 * na_query_xml: send() failure is reported with its errno
 *
 * The na_socket_create seam hands back a UDP socket whose write side is
 * already shut down, so connect() succeeds but send() fails with EPIPE.
 * SIGPIPE is ignored for the duration, as send() would otherwise raise it.
 * ========================================================================= */

static int write_shutdown_socket_create(int domain, int type, int protocol)
{
    int fd = socket(domain, type, protocol);
    if (fd >= 0)
    {
        shutdown(fd, SHUT_WR);  // fails with ENOTCONN, but still takes effect
    }
    return fd;
}

static void test_send_failure(void)
{
    section("na_query_xml: send() failure (via na_socket_create seam)");

    na_client_t *client = na_client_new("127.0.0.1", 0, 2000000);
    char xml_response[MAX_XML_RESPONSE_SIZE];
    char error_msg[MAX_ERROR_MSG_LEN];

    int (*real_socket_create)(int, int, int) = na_socket_create;
    na_socket_create = write_shutdown_socket_create;
    void (*old_sigpipe)(int) = signal(SIGPIPE, SIG_IGN);

    memset(error_msg, 0, sizeof(error_msg));
    CHECK_INT("na_query_xml: send() failure returns 0",
              0, na_query_xml(client, "127.0.0.1", "3", "", xml_response, sizeof(xml_response), error_msg));
    CHECK_STR("na_query_xml: error_msg reports the send() errno",
              "Could not send request to NetAcuity Server: Broken pipe.", error_msg);

    signal(SIGPIPE, old_sigpipe);
    na_socket_create = real_socket_create;
    na_client_free(client);
}


/* =========================================================================
 * na_query_xml: recv() failure is reported with its errno
 *
 * With nothing listening on 127.0.0.1:API_SERVER_UDP_PORT, the kernel answers
 * the request with ICMP port unreachable, and recv() on the connect()ed
 * socket fails with ECONNREFUSED. Binding the port first checks it is free;
 * it is released again before the query.
 * ========================================================================= */

static void test_recv_failure(void)
{
    section("na_query_xml: recv() failure (nothing listening)");

    int probe_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (probe_fd < 0)
    {
        printf("  SKIP  na_query_xml: recv() failure (socket() failed: %s)\n", strerror(errno));
        return;
    }

    struct sockaddr_in probe_addr;
    memset(&probe_addr, 0, sizeof(probe_addr));
    probe_addr.sin_family = AF_INET;
    probe_addr.sin_port = htons(API_SERVER_UDP_PORT);
    probe_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(probe_fd, (struct sockaddr *)&probe_addr, sizeof(probe_addr)) != 0)
    {
        printf("  SKIP  na_query_xml: recv() failure (port %d unavailable: %s)\n",
               API_SERVER_UDP_PORT, strerror(errno));
        close(probe_fd);
        return;
    }
    close(probe_fd);

    na_client_t *client = na_client_new("127.0.0.1", 0, 2000000);
    char xml_response[MAX_XML_RESPONSE_SIZE];
    char error_msg[MAX_ERROR_MSG_LEN];
    char expected[MAX_ERROR_MSG_LEN];
    snprintf(expected, sizeof(expected),
             "Error receiving response from NetAcuity Server: %s.", strerror(ECONNREFUSED));

    memset(error_msg, 0, sizeof(error_msg));
    CHECK_INT("na_query_xml: recv() failure returns 0",
              0, na_query_xml(client, "127.0.0.1", "3", "", xml_response, sizeof(xml_response), error_msg));
    CHECK_STR("na_query_xml: error_msg reports the recv() errno", expected, error_msg);

    na_client_free(client);
}


/* =========================================================================
 * na_query_xml: success path (mock server)
 *
 * Every other na_query_xml() test in this file only exercises client-side
 * validation failures that never reach the network (they all short-circuit
 * before na_query_xml() ever creates a socket). This is the first test that
 * drives a genuine successful XML query end-to-end, via a one-shot mock
 * NetAcuity server forked in-process.
 * ========================================================================= */

static void test_xml_success(void)
{
    section("na_query_xml: successful query (mock server)");

    int mock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (mock_fd < 0)
    {
        printf("  SKIP  na_query_xml: success (socket() failed: %s)\n", strerror(errno));
        return;
    }

    int reuse = 1;
    setsockopt(mock_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in mock_addr;
    memset(&mock_addr, 0, sizeof(mock_addr));
    mock_addr.sin_family = AF_INET;
    mock_addr.sin_port = htons(API_SERVER_UDP_PORT);
    mock_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(mock_fd, (struct sockaddr *)&mock_addr, sizeof(mock_addr)) != 0)
    {
        printf("  SKIP  na_query_xml: success (port %d unavailable: %s)\n",
               API_SERVER_UDP_PORT, strerror(errno));
        close(mock_fd);
        return;
    }

    pid_t pid = fork();
    if (pid == 0)
    {
        /* Child: one-shot mock NetAcuity server. */
        struct timeval tv = { 5, 0 };
        setsockopt(mock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        char req_buf[1600];
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        ssize_t n = recvfrom(mock_fd, req_buf, sizeof(req_buf) - 1, 0,
                              (struct sockaddr *)&client_addr, &client_len);
        if (n > 0)
        {
            /* XML packet format: [2-char packet-num][2-char total-packets][xml body].
             * No "error" attribute at all -- per PROTOCOL.txt, that attribute is only
             * present when there IS an error. */
            const char *body =
                "<response trans-id=\"xmltest\" ip=\"127.0.0.1\">"
                "<db fc=\"3\" country=\"usa\" region=\"nc\" city=\"durham\" />"
                "</response>";
            char packet[600];
            size_t offset = 0;
            memcpy(packet + offset, "01", 2); offset += 2;
            memcpy(packet + offset, "01", 2); offset += 2;
            memcpy(packet + offset, body, strlen(body)); offset += strlen(body);
            sendto(mock_fd, packet, offset, 0,
                   (struct sockaddr *)&client_addr, client_len);
        }
        close(mock_fd);
        _exit(0);
    }

    close(mock_fd);

    na_client_t *loopback_client = na_client_new("127.0.0.1", 0, 2000000);
    char xml_response[MAX_XML_RESPONSE_SIZE];
    char error_msg[MAX_ERROR_MSG_LEN];
    memset(error_msg, 0, sizeof(error_msg));

    int ret = na_query_xml(loopback_client, "127.0.0.1", "3", "xmltest", xml_response, sizeof(xml_response), error_msg);

    int status;
    waitpid(pid, &status, 0);
    na_client_free(loopback_client);

    CHECK_INT("na_query_xml: round-trip query succeeds", 1, ret);

    char found_country[64];
    get_xml_field(xml_response, "country", found_country, sizeof(found_country));
    CHECK_STR("na_query_xml: response contains the queried field", "usa", found_country);
}


/* =========================================================================
 * Scripted mock NetAcuity server
 *
 * Like the one-shot mock in test_xml_success(), but answers up to
 * max_requests requests on 127.0.0.1:API_SERVER_UDP_PORT, handing each one
 * to a handler (which runs in the forked child) to decide how to reply.
 * The child's exit status is the number of requests it saw.
 * ========================================================================= */

typedef struct mock_request
{
    int                fd;
    struct sockaddr_in peer;
    socklen_t          peer_len;
    int                index;          // 0-based count of requests seen so far
    char               trans_id[MAX_TRANSACTION_ID];
    char               ip[MAX_IP_LEN];
} mock_request;

typedef void (*mock_handler)(const mock_request *req, void *ctx);

// Returns the child's pid, or -1 (after printing a SKIP line) if the mock
// server couldn't be started.
static pid_t mock_server_start(const char *desc, int max_requests, mock_handler handler, void *ctx)
{
    int mock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (mock_fd < 0)
    {
        printf("  SKIP  %s (socket() failed: %s)\n", desc, strerror(errno));
        return -1;
    }

    int reuse = 1;
    setsockopt(mock_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in mock_addr;
    memset(&mock_addr, 0, sizeof(mock_addr));
    mock_addr.sin_family = AF_INET;
    mock_addr.sin_port = htons(API_SERVER_UDP_PORT);
    mock_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(mock_fd, (struct sockaddr *)&mock_addr, sizeof(mock_addr)) != 0)
    {
        printf("  SKIP  %s (port %d unavailable: %s)\n", desc, API_SERVER_UDP_PORT, strerror(errno));
        close(mock_fd);
        return -1;
    }

    fflush(stdout);
    pid_t pid = fork();
    if (pid < 0)
    {
        printf("  SKIP  %s (fork() failed: %s)\n", desc, strerror(errno));
        close(mock_fd);
        return -1;
    }
    if (pid == 0)
    {
        struct timeval tv = { 5, 0 };
        setsockopt(mock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        int seen;
        for (seen = 0; seen < max_requests; seen++)
        {
            char req_buf[MAX_XML_REQUEST_BUFF_SIZE + 1];
            mock_request req;
            memset(&req, 0, sizeof(req));
            req.fd = mock_fd;
            req.peer_len = sizeof(req.peer);
            req.index = seen;

            ssize_t n = recvfrom(mock_fd, req_buf, sizeof(req_buf) - 1, 0,
                                 (struct sockaddr *)&req.peer, &req.peer_len);
            if (n <= 0)
            {
                break;
            }
            req_buf[n] = '\0';
            get_xml_field(req_buf, "trans-id", req.trans_id, sizeof(req.trans_id));
            get_xml_field(req_buf, "ip", req.ip, sizeof(req.ip));
            handler(&req, ctx);
        }
        close(mock_fd);
        _exit(seen);
    }

    close(mock_fd);
    return pid;
}

// Waits for the mock server to exit, and returns the number of requests it saw.
static int mock_server_finish(pid_t pid)
{
    int status;
    if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status))
    {
        return -1;
    }
    return WEXITSTATUS(status);
}

// Sends one XML-protocol packet: [2-char packet-num][2-char total-packets][body]
static void mock_send_packet(const mock_request *req, int packet_num, int num_packets,
                             const char *body, size_t body_len)
{
    char packet[2 * MAX_RESPONSE_SIZE];
    snprintf(packet, sizeof(packet), "%02d%02d", packet_num, num_packets);
    memcpy(packet + 4, body, body_len);
    sendto(req->fd, packet, 4 + body_len, 0, (struct sockaddr *)&req->peer, req->peer_len);
}

// Sends a single-packet success response for the given trans-id, echoing
// the request's ip, and reporting the client's source port as "port" so
// tests can tell whether queries share a socket.
static void mock_reply(const mock_request *req, const char *trans_id)
{
    char body[MAX_RESPONSE_SIZE];
    int len = snprintf(body, sizeof(body),
        "<response trans-id=\"%s\" ip=\"%s\" port=\"%d\">"
        "<db fc=\"3\" country=\"usa\" region=\"nc\" city=\"durham\" />"
        "</response>",
        trans_id, req->ip, ntohs(req->peer.sin_port));
    mock_send_packet(req, 1, 1, body, (size_t)len);
}

// Replies to each request normally.
static void h_echo(const mock_request *req, void *ctx)
{
    (void)ctx;
    mock_reply(req, req->trans_id);
}


/* =========================================================================
 * na_query_xml: a datagram that fills the receive buffer is rejected
 *
 * Real packets are at most 1400 bytes (see MAX_XML_RESPONSE_SIZE), so a
 * datagram that fills the whole receive buffer may have been truncated, and
 * there would be no room left to NUL-terminate it before strlen().
 * ========================================================================= */

static void h_oversized(const mock_request *req, void *ctx)
{
    (void)ctx;
    char body[sizeof(api_xml_response) - 4];
    memset(body, 'x', sizeof(body));
    mock_send_packet(req, 1, 1, body, sizeof(body));
}

static void test_xml_oversized_packet(void)
{
    section("na_query_xml: oversized packet (mock server)");

    pid_t pid = mock_server_start("na_query_xml: oversized packet", 1, h_oversized, NULL);
    if (pid < 0)
    {
        return;
    }

    na_client_t *client = na_client_new("127.0.0.1", 0, 2000000);
    char xml_response[MAX_XML_RESPONSE_SIZE];
    char error_msg[MAX_ERROR_MSG_LEN];
    memset(error_msg, 0, sizeof(error_msg));

    int ret = na_query_xml(client, "127.0.0.1", "3", "big", xml_response, sizeof(xml_response), error_msg);

    mock_server_finish(pid);
    na_client_free(client);

    CHECK_INT("na_query_xml: oversized packet returns 0", 0, ret);
    CHECK_STR("na_query_xml: oversized packet error_msg",
              "Received an oversized response from the NetAcuity Server.", error_msg);
}


/* =========================================================================
 * na_query_xml: the longest allowed transaction_id round-trips
 * ========================================================================= */

static void test_xml_max_transaction_id(void)
{
    section("na_query_xml: longest allowed transaction_id (mock server)");

    pid_t pid = mock_server_start("na_query_xml: longest transaction_id", 1, h_echo, NULL);
    if (pid < 0)
    {
        return;
    }

    na_client_t *client = na_client_new("127.0.0.1", 0, 2000000);
    char txn[MAX_TRANSACTION_ID];
    char xml_response[MAX_XML_RESPONSE_SIZE];
    char error_msg[MAX_ERROR_MSG_LEN];
    memset(txn, 'a', sizeof(txn) - 1);
    txn[sizeof(txn) - 1] = '\0';
    memset(error_msg, 0, sizeof(error_msg));

    int ret = na_query_xml(client, "127.0.0.1", "3", txn, xml_response, sizeof(xml_response), error_msg);

    mock_server_finish(pid);
    na_client_free(client);

    CHECK_INT("na_query_xml: MAX_TRANSACTION_ID-1 char transaction_id succeeds", 1, ret);
    CHECK_STR("na_query_xml: MAX_TRANSACTION_ID-1 char transaction_id error_msg", "", error_msg);
}


/* =========================================================================
 * na_query_xml: response handling (mock server)
 * ========================================================================= */

// Runs a single na_query_xml() against a mock server that uses handler.
// Returns na_query_xml()'s result, or -1 if the mock server couldn't start.
static int mock_query_xml(const char *desc, mock_handler handler, int timeout_us,
                          char *xml_response, size_t size, char *error_msg)
{
    pid_t pid = mock_server_start(desc, 1, handler, NULL);
    if (pid < 0)
    {
        return -1;
    }

    na_client_t *client = na_client_new("127.0.0.1", 0, timeout_us);
    memset(error_msg, 0, MAX_ERROR_MSG_LEN);
    int ret = na_query_xml(client, "127.0.0.1", "3", "txn", xml_response, size, error_msg);

    mock_server_finish(pid);
    na_client_free(client);
    return ret;
}

static void h_server_error(const mock_request *req, void *ctx)
{
    (void)ctx;
    char body[MAX_RESPONSE_SIZE];
    int len = snprintf(body, sizeof(body),
        "<response trans-id=\"%s\" ip=\"%s\" error=\"DB Not Loaded\" />", req->trans_id, req->ip);
    mock_send_packet(req, 1, 1, body, (size_t)len);
}

static void h_wrong_trans_id(const mock_request *req, void *ctx)
{
    (void)ctx;
    mock_reply(req, "not-the-right-one");
}

static void h_wrong_ip(const mock_request *req, void *ctx)
{
    (void)ctx;
    char body[MAX_RESPONSE_SIZE];
    int len = snprintf(body, sizeof(body),
        "<response trans-id=\"%s\" ip=\"192.0.2.99\"><db fc=\"3\" country=\"usa\" /></response>",
        req->trans_id);
    mock_send_packet(req, 1, 1, body, (size_t)len);
}

// Sends the response split across three packets.
static void h_multi_packet(const mock_request *req, void *ctx)
{
    (void)ctx;
    char body[MAX_RESPONSE_SIZE];
    int len = snprintf(body, sizeof(body),
        "<response trans-id=\"%s\" ip=\"%s\"><db fc=\"3\" country=\"usa\" city=\"durham\" /></response>",
        req->trans_id, req->ip);
    int third = len / 3;
    mock_send_packet(req, 1, 3, body,             (size_t)third);
    mock_send_packet(req, 2, 3, body + third,     (size_t)third);
    mock_send_packet(req, 3, 3, body + 2 * third, (size_t)(len - 2 * third));
}

static void h_out_of_order(const mock_request *req, void *ctx)
{
    (void)ctx;
    const char *body = "<response trans-id=\"txn\" ip=\"127.0.0.1\"></response>";
    mock_send_packet(req, 2, 2, body, strlen(body));
    mock_send_packet(req, 1, 2, body, strlen(body));
}

static void h_drop(const mock_request *req, void *ctx)
{
    (void)req;
    (void)ctx;
}

static long elapsed_us(const struct timeval *start)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    return (now.tv_sec - start->tv_sec) * 1000000L + (now.tv_usec - start->tv_usec);
}

static void test_xml_response_handling(void)
{
    section("na_query_xml: response handling (mock server)");

    char xml_response[MAX_XML_RESPONSE_SIZE];
    char error_msg[MAX_ERROR_MSG_LEN];
    char value[64];
    int ret;

    ret = mock_query_xml("server-reported error", h_server_error, 2000000,
                         xml_response, sizeof(xml_response), error_msg);
    if (ret >= 0)
    {
        CHECK_INT("server-reported error: returns 0", 0, ret);
        CHECK_STR("server-reported error: error_msg is the server's", "DB Not Loaded", error_msg);
    }

    ret = mock_query_xml("trans-id mismatch", h_wrong_trans_id, 2000000,
                         xml_response, sizeof(xml_response), error_msg);
    if (ret >= 0)
    {
        CHECK_INT("trans-id mismatch: returns 0", 0, ret);
        CHECK_STR("trans-id mismatch: error_msg", "Transaction ID mismatch.", error_msg);
    }

    ret = mock_query_xml("ip mismatch", h_wrong_ip, 2000000,
                         xml_response, sizeof(xml_response), error_msg);
    if (ret >= 0)
    {
        CHECK_INT("ip mismatch: returns 0", 0, ret);
        CHECK_STR("ip mismatch: error_msg", "Response address mismatch.", error_msg);
    }

    ret = mock_query_xml("multi-packet response", h_multi_packet, 2000000,
                         xml_response, sizeof(xml_response), error_msg);
    if (ret >= 0)
    {
        CHECK_INT("multi-packet response: returns 1", 1, ret);
        get_xml_field(xml_response, "city", value, sizeof(value));
        CHECK_STR("multi-packet response: is reassembled", "durham", value);
    }

    ret = mock_query_xml("multi-packet response, small buffer", h_multi_packet, 2000000,
                         xml_response, 40, error_msg);
    if (ret >= 0)
    {
        CHECK_INT("multi-packet response, small buffer: returns 0", 0, ret);
        CHECK_STR("multi-packet response, small buffer: error_msg",
                  "Response will exceed passed buffer size.", error_msg);
    }

    ret = mock_query_xml("out-of-order packets", h_out_of_order, 2000000,
                         xml_response, sizeof(xml_response), error_msg);
    if (ret >= 0)
    {
        CHECK_INT("out-of-order packets: returns 0", 0, ret);
        CHECK_STR("out-of-order packets: error_msg",
                  "Packets received out of order (got: 2, last: 0).", error_msg);
    }

    struct timeval start;
    gettimeofday(&start, NULL);
    ret = mock_query_xml("timeout", h_drop, 200000,
                         xml_response, sizeof(xml_response), error_msg);
    long took = elapsed_us(&start);
    if (ret >= 0)
    {
        CHECK_INT("timeout: returns 0", 0, ret);
        CHECK_STR("timeout: error_msg", "Timeout waiting for response from NetAcuity Server.", error_msg);
        CHECK("timeout: waits for (roughly) the client's timeout", took >= 200000 && took < 1000000);
    }
}

// Returns the lowest free file descriptor.  If a query leaks its socket, the
// leaked socket takes that descriptor, and this returns a higher one after.
static int lowest_free_fd(void)
{
    int fd = dup(0);
    if (fd >= 0)
    {
        close(fd);
    }
    return fd;
}

static void test_xml_no_fd_leak(void)
{
    section("na_query_xml: socket is closed on every path (mock server)");

    static const struct { mock_handler handler; int timeout_us; int expect; const char *name; } cases[] = {
        { h_echo,           2000000, 1, "success" },
        { h_server_error,   2000000, 0, "server-reported error" },
        { h_wrong_trans_id, 2000000, 0, "trans-id mismatch" },
        { h_drop,            200000, 0, "timeout" },
    };
    char xml_response[MAX_XML_RESPONSE_SIZE];
    char error_msg[MAX_ERROR_MSG_LEN];
    char desc[128];
    size_t i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        int before = lowest_free_fd();
        int ret = mock_query_xml(cases[i].name, cases[i].handler, cases[i].timeout_us,
                                 xml_response, sizeof(xml_response), error_msg);
        if (ret < 0)
        {
            continue;
        }
        snprintf(desc, sizeof(desc), "%s: returns %d", cases[i].name, cases[i].expect);
        CHECK_INT(desc, cases[i].expect, ret);
        snprintf(desc, sizeof(desc), "%s: no file descriptor leaked", cases[i].name);
        CHECK_INT(desc, before, lowest_free_fd());
    }
}


/* =========================================================================
 * na_connection_*: persistent connections
 * ========================================================================= */

static void test_connection_validation(void)
{
    section("na_connection: argument validation (no server required)");

    char xml_response[MAX_XML_RESPONSE_SIZE];
    char error_msg[MAX_ERROR_MSG_LEN];

    CHECK("na_connection_new: NULL client returns NULL", na_connection_new(NULL) == NULL);

    na_connection_free(NULL);
    CHECK("na_connection_free: NULL is a no-op", 1);

    memset(error_msg, 0, sizeof(error_msg));
    CHECK_INT("na_connection_query_xml: NULL conn returns 0",
              0, na_connection_query_xml(NULL, "192.0.2.1", "3", "", xml_response, sizeof(xml_response), error_msg));
    CHECK_STR("na_connection_query_xml: NULL conn error_msg", "Invalid connection handle.", error_msg);

    na_client_t *client = na_client_new("192.0.2.1", 0, 2000000);
    na_connection_t *conn = na_connection_new(client);
    na_client_free(client);
    CHECK("na_connection_new: valid client returns a connection", conn != NULL);
    CHECK("na_connection_new: socket isn't opened yet", conn->sockfd == -1);

    strcpy(xml_response, "poisoned");
    memset(error_msg, 0, sizeof(error_msg));
    CHECK_INT("invalid feature: returns 0",
              0, na_connection_query_xml(conn, "192.0.2.1", "2", "t", xml_response, sizeof(xml_response), error_msg));
    CHECK_STR("invalid feature: xml_response is cleared to empty", "", xml_response);
    CHECK_STR("invalid feature: error_msg describes the failure", "Request for feature 2 is invalid.", error_msg);
    CHECK("invalid feature: no socket was opened", conn->sockfd == -1);

    na_connection_free(conn);
}

// Per-request actions for h_script(), indexed by request number.
enum { ACT_REPLY, ACT_DROP, ACT_REPLY_TWICE, ACT_SERVER_ERROR, ACT_WRONG_TRANS_ID, ACT_STALE_THEN_REPLY,
       ACT_STALE_TO_PREV_THEN_REPLY };

// Replies to each request as directed by the int array in ctx.  Every reply
// reports the client's source port for this request as "port", and for the
// previous request as "prev-port", so tests can tell whether a socket was
// re-used.  ACT_STALE_THEN_REPLY first re-sends the previous reply, as a late
// duplicate of it would arrive.  ACT_STALE_TO_PREV_THEN_REPLY re-sends it to
// the previous request's source address, as a real late duplicate would go.
static void h_script(const mock_request *req, void *ctx)
{
    static int prev_port = 0;   // the handler runs in the mock server process
    static char prev_body[MAX_RESPONSE_SIZE];
    static int prev_len = 0;
    static mock_request prev_req;
    const int *actions = (const int *)ctx;
    int action = actions[req->index];
    int port = ntohs(req->peer.sin_port);
    const char *trans_id = (action == ACT_WRONG_TRANS_ID) ? "not-the-right-one" : req->trans_id;
    char body[MAX_RESPONSE_SIZE];
    int len;

    if (action == ACT_SERVER_ERROR)
    {
        len = snprintf(body, sizeof(body),
            "<response trans-id=\"%s\" ip=\"%s\" port=\"%d\" prev-port=\"%d\" error=\"DB Not Loaded\" />",
            trans_id, req->ip, port, prev_port);
    }
    else
    {
        len = snprintf(body, sizeof(body),
            "<response trans-id=\"%s\" ip=\"%s\" port=\"%d\" prev-port=\"%d\"><db fc=\"3\" country=\"usa\" /></response>",
            trans_id, req->ip, port, prev_port);
    }

    if (action == ACT_STALE_THEN_REPLY && prev_len > 0)
    {
        mock_send_packet(req, 1, 1, prev_body, (size_t)prev_len);
    }
    if (action == ACT_STALE_TO_PREV_THEN_REPLY && prev_len > 0)
    {
        mock_send_packet(&prev_req, 1, 1, prev_body, (size_t)prev_len);
    }
    if (action != ACT_DROP)
    {
        mock_send_packet(req, 1, 1, body, (size_t)len);
    }
    if (action == ACT_REPLY_TWICE)
    {
        mock_send_packet(req, 1, 1, body, (size_t)len);
    }

    prev_port = port;
    memcpy(prev_body, body, (size_t)len);
    prev_len = len;
    prev_req = *req;
}

// Starts a mock server running h_script(actions), and returns a connection
// to it (via a client that has already been freed).
static na_connection_t *start_script(const char *desc, int *actions, int num_actions,
                                     int timeout_us, pid_t *pid)
{
    *pid = mock_server_start(desc, num_actions, h_script, actions);
    if (*pid < 0)
    {
        return NULL;
    }

    na_client_t *client = na_client_new("127.0.0.1", 0, timeout_us);
    na_connection_t *conn = na_connection_new(client);
    na_client_free(client);
    return conn;
}

// Runs one na_connection_query_xml() on conn and returns its result, with
// port and prev_port set from the response (0 if absent).
static int conn_query(na_connection_t *conn, const char *txn, int *port, int *prev_port, char *error_msg)
{
    char xml_response[MAX_XML_RESPONSE_SIZE];
    char value[32];

    memset(error_msg, 0, MAX_ERROR_MSG_LEN);
    int ret = na_connection_query_xml(conn, "127.0.0.1", "3", txn, xml_response, sizeof(xml_response), error_msg);

    get_xml_field(xml_response, "port", value, sizeof(value));
    *port = atoi(value);
    get_xml_field(xml_response, "prev-port", value, sizeof(value));
    *prev_port = atoi(value);
    return ret;
}

static void test_connection_reuse(void)
{
    section("na_connection: socket is re-used across queries (mock server)");

    int actions[] = { ACT_REPLY, ACT_REPLY, ACT_REPLY };
    pid_t pid;
    na_connection_t *conn = start_script("socket re-use", actions, 3, 2000000, &pid);
    if (!conn)
    {
        return;
    }

    char error_msg[MAX_ERROR_MSG_LEN];
    int port, prev_port;

    CHECK_INT("query 1: succeeds", 1, conn_query(conn, "q1", &port, &prev_port, error_msg));
    CHECK_STR("query 1: error_msg", "", error_msg);
    CHECK("query 1: socket stays open", conn->sockfd != -1);
    CHECK("query 1: socket is close-on-exec", (fcntl(conn->sockfd, F_GETFD) & FD_CLOEXEC) != 0);

    CHECK_INT("query 2: succeeds", 1, conn_query(conn, "q2", &port, &prev_port, error_msg));
    CHECK("query 2: sent from the same socket as query 1", port != 0 && port == prev_port);

    CHECK_INT("query 3: succeeds (generated transaction_id)", 1, conn_query(conn, NULL, &port, &prev_port, error_msg));
    CHECK("query 3: sent from the same socket as query 2", port != 0 && port == prev_port);

    na_connection_free(conn);
    CHECK_INT("mock server saw 3 requests", 3, mock_server_finish(pid));
}

static void test_connection_reopen_after_timeout(void)
{
    section("na_connection: socket is re-opened after a timeout (mock server)");

    int actions[] = { ACT_DROP, ACT_REPLY };
    pid_t pid;
    na_connection_t *conn = start_script("re-open after timeout", actions, 2, 200000, &pid);
    if (!conn)
    {
        return;
    }

    char error_msg[MAX_ERROR_MSG_LEN];
    int port, prev_port;

    CHECK_INT("query 1: times out", 0, conn_query(conn, "q1", &port, &prev_port, error_msg));
    CHECK_STR("query 1: error_msg", "Timeout waiting for response from NetAcuity Server.", error_msg);
    CHECK("query 1: socket is closed", conn->sockfd == -1);

    CHECK_INT("query 2: succeeds", 1, conn_query(conn, "q2", &port, &prev_port, error_msg));
    CHECK("query 2: socket is open", conn->sockfd != -1);

    na_connection_free(conn);
    mock_server_finish(pid);
}

static void test_connection_server_error(void)
{
    section("na_connection: socket is re-opened after a server-reported error (mock server)");

    int actions[] = { ACT_SERVER_ERROR, ACT_REPLY };
    pid_t pid;
    na_connection_t *conn = start_script("server-reported error", actions, 2, 2000000, &pid);
    if (!conn)
    {
        return;
    }

    char error_msg[MAX_ERROR_MSG_LEN];
    int port, prev_port;

    CHECK_INT("query 1: returns 0", 0, conn_query(conn, "q1", &port, &prev_port, error_msg));
    CHECK_STR("query 1: error_msg is the server's", "DB Not Loaded", error_msg);
    CHECK("query 1: socket is closed", conn->sockfd == -1);

    CHECK_INT("query 2: succeeds", 1, conn_query(conn, "q2", &port, &prev_port, error_msg));
    CHECK("query 2: socket is open", conn->sockfd != -1);

    na_connection_free(conn);
    mock_server_finish(pid);
}

// Error responses carry no trans-id to echo-check, so a late duplicate of
// query 1's error, arriving during query 2, used to be reported as query 2's
// error.  Closing the socket after an error response means the duplicate goes
// to a closed socket instead.
static void test_connection_late_server_error(void)
{
    section("na_connection: late duplicate of a server-reported error (mock server)");

    int actions[] = { ACT_SERVER_ERROR, ACT_STALE_TO_PREV_THEN_REPLY };
    pid_t pid;
    na_connection_t *conn = start_script("late server error", actions, 2, 2000000, &pid);
    if (!conn)
    {
        return;
    }

    char error_msg[MAX_ERROR_MSG_LEN];
    int port, prev_port;

    CHECK_INT("query 1: returns 0", 0, conn_query(conn, "q1", &port, &prev_port, error_msg));
    CHECK_STR("query 1: error_msg is the server's", "DB Not Loaded", error_msg);

    CHECK_INT("query 2: query 1's late error isn't reported as query 2's", 1,
              conn_query(conn, "q2", &port, &prev_port, error_msg));
    CHECK_STR("query 2: error_msg", "", error_msg);

    na_connection_free(conn);
    mock_server_finish(pid);
}

static void test_connection_echo_mismatch(void)
{
    section("na_connection: socket is re-opened after a trans-id mismatch (mock server)");

    int actions[] = { ACT_WRONG_TRANS_ID, ACT_REPLY };
    pid_t pid;
    na_connection_t *conn = start_script("trans-id mismatch", actions, 2, 2000000, &pid);
    if (!conn)
    {
        return;
    }

    char error_msg[MAX_ERROR_MSG_LEN];
    int port, prev_port;

    CHECK_INT("query 1: returns 0", 0, conn_query(conn, "q1", &port, &prev_port, error_msg));
    CHECK_STR("query 1: error_msg", "Transaction ID mismatch.", error_msg);
    CHECK("query 1: socket is closed", conn->sockfd == -1);

    CHECK_INT("query 2: succeeds", 1, conn_query(conn, "q2", &port, &prev_port, error_msg));

    na_connection_free(conn);
    mock_server_finish(pid);
}

static void test_connection_drain(void)
{
    section("na_connection: stale datagrams are discarded (mock server)");

    int actions[] = { ACT_REPLY_TWICE, ACT_REPLY };
    pid_t pid;
    na_connection_t *conn = start_script("stale datagram", actions, 2, 2000000, &pid);
    if (!conn)
    {
        return;
    }

    char error_msg[MAX_ERROR_MSG_LEN];
    int port, prev_port;

    CHECK_INT("query 1: succeeds", 1, conn_query(conn, "q1", &port, &prev_port, error_msg));

    // give query 1's duplicate response time to arrive
    usleep(100000);

    CHECK_INT("query 2: succeeds, despite query 1's duplicate response", 1,
              conn_query(conn, "q2", &port, &prev_port, error_msg));
    CHECK_STR("query 2: error_msg", "", error_msg);
    CHECK("query 2: sent from the same socket as query 1", port != 0 && port == prev_port);

    na_connection_free(conn);
    mock_server_finish(pid);
}

// A pending socket error (left by an ICMP message) can sit in front of a stale
// datagram.  The drain must keep going past it, or the stale datagram is read
// as the next response.  The error is injected through the recv() shim, as
// the drain's first recv() of query 2.
static void test_connection_drain_errors(void)
{
    section("na_connection: drain continues past a pending socket error (mock server)");

    static const struct { int err; const char *name; } cases[] = {
        { ECONNREFUSED, "ECONNREFUSED" },
        { EHOSTUNREACH, "EHOSTUNREACH" },
    };
    size_t i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        int actions[] = { ACT_REPLY_TWICE, ACT_REPLY };
        pid_t pid;
        na_connection_t *conn = start_script(cases[i].name, actions, 2, 2000000, &pid);
        if (!conn)
        {
            return;
        }

        char desc[128];
        char error_msg[MAX_ERROR_MSG_LEN];
        int port, prev_port;

        conn_query(conn, "q1", &port, &prev_port, error_msg);

        // give query 1's duplicate response time to arrive
        usleep(100000);

        recv_inject_errno = cases[i].err;
        snprintf(desc, sizeof(desc), "%s before a stale datagram: query 2 succeeds", cases[i].name);
        CHECK_INT(desc, 1, conn_query(conn, "q2", &port, &prev_port, error_msg));
        snprintf(desc, sizeof(desc), "%s before a stale datagram: error_msg", cases[i].name);
        CHECK_STR(desc, "", error_msg);
        recv_inject_errno = 0;

        na_connection_free(conn);
        mock_server_finish(pid);
    }
}

// The drain gives up after NA_MAX_DRAIN recv() calls, and the query then
// starts over on a new socket.  Datagrams are faked through the recv() shim,
// as the first recv() calls of query 2 (the drain's).
static void test_connection_drain_bound(void)
{
    section("na_connection: drain gives up after NA_MAX_DRAIN datagrams (mock server)");

    static const struct { int datagrams; int new_socket; const char *name; } cases[] = {
        { NA_MAX_DRAIN - 1, 0, "NA_MAX_DRAIN - 1 datagrams" },
        { NA_MAX_DRAIN,     1, "NA_MAX_DRAIN datagrams" },
    };
    size_t i;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        int actions[] = { ACT_REPLY, ACT_REPLY };
        pid_t pid;
        na_connection_t *conn = start_script(cases[i].name, actions, 2, 2000000, &pid);
        if (!conn)
        {
            return;
        }

        char desc[128];
        char error_msg[MAX_ERROR_MSG_LEN];
        int port, prev_port;

        conn_query(conn, "q1", &port, &prev_port, error_msg);

        recv_fake_datagrams = cases[i].datagrams;
        snprintf(desc, sizeof(desc), "%s: query 2 succeeds", cases[i].name);
        CHECK_INT(desc, 1, conn_query(conn, "q2", &port, &prev_port, error_msg));
        snprintf(desc, sizeof(desc), "%s: all were drained", cases[i].name);
        CHECK_INT(desc, 0, recv_fake_datagrams);
        snprintf(desc, sizeof(desc), "%s: query 2 sent from %s socket", cases[i].name,
                 cases[i].new_socket ? "a new" : "the same");
        CHECK(desc, port != 0 && (port != prev_port) == cases[i].new_socket);
        recv_fake_datagrams = 0;

        na_connection_free(conn);
        mock_server_finish(pid);
    }
}

// With "" as the transaction ID, a late duplicate of query 1's response that
// arrives after query 2's drain used to pass query 2's echo check, and was
// returned as query 2's result.  An ID is now generated for "", so the
// duplicate fails the echo check instead.
static void test_connection_empty_trans_id(void)
{
    section("na_connection: late duplicate with an empty transaction ID (mock server)");

    int actions[] = { ACT_REPLY, ACT_STALE_THEN_REPLY };
    pid_t pid;
    na_connection_t *conn = start_script("late duplicate, empty trans-id", actions, 2, 2000000, &pid);
    if (!conn)
    {
        return;
    }

    char error_msg[MAX_ERROR_MSG_LEN];
    int port, prev_port;

    CHECK_INT("query 1: succeeds", 1, conn_query(conn, "", &port, &prev_port, error_msg));

    CHECK_INT("query 2: query 1's late response isn't returned as its result",
              0, conn_query(conn, "", &port, &prev_port, error_msg));
    CHECK_STR("query 2: error_msg", "Transaction ID mismatch.", error_msg);

    na_connection_free(conn);
    mock_server_finish(pid);
}

// select() can't handle descriptors of FD_SETSIZE (1024) or more, which a
// long-lived process with many open files can reach.  The na_socket_create
// seam moves the socket to a descriptor above FD_SETSIZE.
#define HIGH_FD  (FD_SETSIZE + 76)

static int high_fd_socket_create(int domain, int type, int protocol)
{
    int fd = socket(domain, type, protocol);
    if (fd < 0)
    {
        return fd;
    }
    int high = dup2(fd, HIGH_FD);
    close(fd);
    return high;
}

static void test_connection_high_fd(void)
{
    section("na_connection: socket descriptor above FD_SETSIZE (mock server)");

    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) != 0 || rl.rlim_cur <= HIGH_FD)
    {
        printf("  SKIP  descriptor above FD_SETSIZE (open-file limit too low)\n");
        return;
    }

    int actions[] = { ACT_REPLY };
    pid_t pid;
    na_connection_t *conn = start_script("high fd", actions, 1, 2000000, &pid);
    if (!conn)
    {
        return;
    }

    char error_msg[MAX_ERROR_MSG_LEN];
    int port, prev_port;

    int (*real_socket_create)(int, int, int) = na_socket_create;
    na_socket_create = high_fd_socket_create;
    CHECK_INT("query: succeeds", 1, conn_query(conn, "q1", &port, &prev_port, error_msg));
    CHECK_STR("query: error_msg", "", error_msg);
    CHECK_INT("query: socket is above FD_SETSIZE", HIGH_FD, conn->sockfd);
    na_socket_create = real_socket_create;

    na_connection_free(conn);
    mock_server_finish(pid);
}

static void test_connection_socket_failure(void)
{
    section("na_connection: recovers from a socket() failure (mock server)");

    int actions[] = { ACT_REPLY };
    pid_t pid;
    na_connection_t *conn = start_script("socket() failure", actions, 1, 2000000, &pid);
    if (!conn)
    {
        return;
    }

    char error_msg[MAX_ERROR_MSG_LEN];
    int port, prev_port;

    int (*real_socket_create)(int, int, int) = na_socket_create;
    na_socket_create = fail_socket_create;
    CHECK_INT("query 1: returns 0", 0, conn_query(conn, "q1", &port, &prev_port, error_msg));
    CHECK("query 1: error_msg reports the socket failure", strstr(error_msg, "Could not create socket") != NULL);
    CHECK("query 1: socket isn't open", conn->sockfd == -1);
    na_socket_create = real_socket_create;

    CHECK_INT("query 2: succeeds", 1, conn_query(conn, "q2", &port, &prev_port, error_msg));

    na_connection_free(conn);
    mock_server_finish(pid);
}


/* =========================================================================
 * main
 * ========================================================================= */

int main(void)
{
    printf("NetAcuity Client API Unit Tests\n");
    printf("================================\n");

    test_api_server_port();
    test_field_dictionary();
    test_client_new();
    test_ip_strings_equal();
    test_get_xml_field();
    test_query_xml_validation();
    test_null_client_validation();
    test_socket_create_failure();
    test_send_failure();
    test_recv_failure();
    test_xml_success();
    test_xml_oversized_packet();
    test_xml_max_transaction_id();
    test_xml_response_handling();
    test_xml_no_fd_leak();
    test_connection_validation();
    test_connection_reuse();
    test_connection_reopen_after_timeout();
    test_connection_server_error();
    test_connection_late_server_error();
    test_connection_echo_mismatch();
    test_connection_drain();
    test_connection_drain_errors();
    test_connection_drain_bound();
    test_connection_empty_trans_id();
    test_connection_high_fd();
    test_connection_socket_failure();

    printf("\n================================\n");
    if (total_failed == 0) {
        printf("All %d tests passed.\n", total_passed);
    } else {
        printf("%d passed, %d FAILED.\n", total_passed, total_failed);
    }
    printf("================================\n");

    return total_failed > 0 ? 1 : 0;
}
