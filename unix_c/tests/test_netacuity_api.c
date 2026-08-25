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
 * Tests all logic that does not require a running NetAcuity server:
 *   - na_client_new() input validation
 *   - get_xml_field() XML attribute extraction
 *   - NULL-client and out-of-range input validation for na_query_xml()
 *     (return paths that fire before any socket call)
 *
 * Compile and run (Linux/Unix):
 *
 *   cd unix_c/tests && make test
 *
 * The netacuity_api.c source file is included directly so that the
 * static (internal) na_socket_create seam can be reassigned without
 * modification to the production source.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>  /* waitpid(), used only by test_xml_success()'s mock server */

/*
 * Pull in the full implementation.  This gives access to static helpers such
 * as na_socket_create, and makes all macros defined in netacuity_api.h
 * (MAX_XML_RESPONSE_SIZE, MAX_ERROR_MSG_LEN, …) available without a separate
 * include.
 */
#include "../netacuity_api.c"


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
 * main
 * ========================================================================= */

int main(void)
{
    printf("NetAcuity Client API Unit Tests\n");
    printf("================================\n");

    test_api_server_port();
    test_client_new();
    test_ip_strings_equal();
    test_get_xml_field();
    test_query_xml_validation();
    test_null_client_validation();
    test_socket_create_failure();
    test_xml_success();

    printf("\n================================\n");
    if (total_failed == 0) {
        printf("All %d tests passed.\n", total_passed);
    } else {
        printf("%d passed, %d FAILED.\n", total_passed, total_failed);
    }
    printf("================================\n");

    return total_failed > 0 ? 1 : 0;
}
