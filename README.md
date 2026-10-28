# NetAcuity Client API — Unix C

C client library and server modules for querying the [NetAcuity](https://www.digitalelement.com/solutions/netacuity/) Server for IP geolocation and intelligence data. Supports the XML UDP query protocol.

## Repository Structure

```
unix_c/
├── netacuity_api.c       # Core API library
├── netacuity_api.h       # Public API header
├── netacuity_fields.h    # Field definitions and dictionary
├── PROTOCOL.txt          # Detailed API documentation
├── BUILD_NOTES.txt       # Build/compiler notes
├── apache/               # Apache httpd server module
│   ├── mod_netacuity.c
│   ├── mod_netacuity.h
│   ├── Makefile
│   └── README_apache.txt
├── nginx/                # Nginx server module
│   ├── ngx_http_netacuity_module.c
│   ├── ngx_http_netacuity_module.h
│   ├── config
│   ├── examples/
│   └── README_nginx.txt
├── examples/             # Standalone example programs
│   ├── test_api_xml.c
│   ├── Makefile.test_api_xml
│   ├── test_api_persistent.c
│   └── Makefile.test_api_persistent
└── tests/                # Unit tests (no server required)
    ├── test_netacuity_api.c
    └── Makefile
```

## Requirements

- **gcc** (tested on GCC 15.x, Ubuntu)
- **libxml2** (required for the Apache and Nginx server modules; not required by the core `na_query_xml()`/`get_xml_field()` library, though it's a good alternative to `get_xml_field()` for parsing the XML response yourself)
  ```
  # Red Hat / CentOS
  sudo yum install libxml2 libxml2-devel

  # Debian / Ubuntu
  sudo apt-get install libxml2 libxml2-dev
  ```
- A running **NetAcuity Server** accessible on UDP port 5400
- An **API ID** (customer-provided integer, range 0–127; default 0)

## Installation / Build

```bash
git clone https://github.com/netacuity/netacuity-client-api-unix-c.git
```

Create a Makefile like the following, pointing `API_DIR` at your local clone's `unix_c/` directory:

```makefile
API_DIR = /absolute/path/to/netacuity-client-api-unix-c/unix_c
CC      = gcc
CFLAGS  = -Wall -O2 -std=gnu99 -I$(API_DIR)

netacuity_client_api_unix_c_test: netacuity_client_api_unix_c_test.c $(API_DIR)/netacuity_api.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f netacuity_client_api_unix_c_test
```

(If you parse the XML response yourself with libxml2 instead of `get_xml_field()`, add `-I/usr/include/libxml2` to `CFLAGS` and `-lxml2` to the link step — see `unix_c/examples/Makefile.test_api_xml` for a working example.)

Then build:

```bash
make
```

## Quick Start

### XML UDP Query

The XML UDP protocol supports multiple feature codes in a single query.

```c
#include "netacuity_api.h"
#include <stdio.h>

int main(void) {
    char response[MAX_XML_RESPONSE_SIZE];
    char error_msg[MAX_ERROR_MSG_LEN];

    // api ID 75, 3 second (3000000us) timeout. Returns NULL on an invalid
    // server_ip or api_id.
    na_client_t *client = na_client_new("192.0.2.1", 75, 3000000);
    if (!client) {
        printf("Error: invalid NetAcuity Server address or API ID\n");
        return 1;
    }

    if (na_query_xml(client, "203.0.113.1", "3,8,10", "txn-001", response, sizeof(response), error_msg)) {
        printf("Response: %s\n", response);
    } else {
        printf("Error: %s\n", error_msg);
    }

    na_client_free(client);
    return 0;
}
```

## API Reference

### `na_client_t *na_client_new(const char *server_ip, int api_id, int timeout_us)`

Allocates and initializes a client handle bound to a single NetAcuity Server address, API ID (0-127; 0 is
"unregistered"), and timeout (in microseconds, covering both connect and query, rounded up to whole milliseconds). Must
be passed explicitly to every query function below. Returns `NULL` if `server_ip` is not a well-formed IPv4/IPv6
literal, `api_id` is not between 0 and 127, `timeout_us` is not positive, or allocation fails.

A client handle is immutable once created and holds no per-query state, so a single handle may be shared freely across threads — each query creates and closes its own socket. Create additional handles only when you need a different server, API ID, or timeout.

### `na_client_t *na_client_new_with_default_api_id(const char *server_ip, int timeout_us)`

Convenience wrapper around `na_client_new()` for callers with no registered API ID, defaulting it to 0 ("unregistered").

### `na_client_t *na_client_new_with_default_timeout(const char *server_ip, int api_id)`

Convenience wrapper around `na_client_new()` for callers with no specific timeout requirement, defaulting it to `NA_DEFAULT_TIMEOUT_US` (2 seconds).

### `void na_client_free(na_client_t *client)`

Releases a client handle allocated by `na_client_new()`. Passing `NULL` is a no-op.

### `int na_query_xml(na_client_t *client, const char *query_ip, const char *feature_codes, const char *transaction_id, char *response, size_t max_size, char *error_msg)`

Queries one or more databases (`feature_codes`, a comma-separated list of feature codes) for `query_ip` using the XML
UDP protocol. The response is an XML string — parse it with `libxml2` or your own XML parser.

- `transaction_id`: at most `MAX_TRANSACTION_ID - 1` (63) characters, or `NULL` or `""` to have one generated
  automatically. A longer ID is rejected with an error.
- `response`, `max_size`: the caller's buffer for the response, and its size in bytes. A response that doesn't fit is an
  error. After any error, `response` may hold part of a response, so use it only on success. `MAX_XML_RESPONSE_SIZE`
  fits any response.
- `error_msg`: a buffer of at least `MAX_ERROR_MSG_LEN` bytes, which receives the reason for any error (`""` on
  success).

Returns 1 on success, 0 on error.

### `na_connection_t *na_connection_new(const na_client_t *client)`

Allocates a persistent connection to the client's NetAcuity Server: a copy of the client's settings plus one UDP socket that is kept open across queries, instead of a new socket for every query as `na_query_xml()` uses. Suited to long-lived processes that make many queries. The client may be freed once the connection is created. Returns `NULL` if `client` is `NULL` or allocation fails.

The socket is opened by the first query. A query that fails after sending its request (a timeout, a mismatched response, a server-reported error) closes the socket, and the next query opens a new one.

Unlike a client handle, a connection must not be used by more than one thread at a time — use one connection per thread — and must not be shared between a parent and child process after `fork()`.

### `int na_connection_query_xml(na_connection_t *conn, const char *query_ip, const char *feature_codes, const char *transaction_id, char *response, size_t max_size, char *error_msg)`

Same as `na_query_xml()`, with the same parameters and limits (including the 63-character `transaction_id`), but sends
the query over the connection's socket. Every query on a connection must use a different `transaction_id`, or pass
`NULL` or `""` to have one generated: a late duplicate of an earlier response can arrive on a re-used socket, and only
the transaction ID tells it apart.

Returns 1 on success, 0 on error.

### `void na_connection_free(na_connection_t *conn)`

Closes the connection's socket and releases the connection. Passing `NULL` is a no-op.

### `char *get_xml_field(const char *response, const char *field_name, char *value, size_t size)`

**Deprecated** — prefer parsing the XML response yourself with `libxml2` instead. Retrieves the value of `field_name` from a response returned by `na_query_xml()`, copying it into the caller-supplied `value` buffer (`size` bytes). Returns `"FieldNotFound"` if the field isn't present in the response; if the field's value is longer than fits in `value`, it's safely truncated (and still null-terminated) rather than discarded.

## Feature Codes

For the complete, up-to-date list of feature codes and their response fields, see the [NetAcuity documentation](https://docs.digitalelement.com/).

## Examples

Runnable examples are provided in the `examples/` directory. These require a running NetAcuity Server.

```bash
# XML UDP protocol example (queries one or more feature codes)
cd unix_c/examples
make -f Makefile.test_api_xml
./test_api_xml <server_ip> <query_ip> <feature_codes>

# Persistent-connection example (queries several IPs over one socket)
make -f Makefile.test_api_persistent
./test_api_persistent <server_ip> <feature_codes> <query_ip> [<query_ip> ...]
```

## Running the Tests

The test suite covers the parsing and validation logic, and runs the network tests against a mock server on 127.0.0.1,
so it needs no running NetAcuity Server. It targets Linux.

```bash
cd unix_c/tests
make test
```

## Server Modules

See `unix_c/apache/README_apache.txt` and `unix_c/nginx/README_nginx.txt` for full build and installation instructions for the Apache and Nginx server modules.

## Changelog

See [CHANGELOG.md](CHANGELOG.md) for release history.

## Support

Technical Support is only available to those under active contract with Digital Element. To contact Support, use the contact information provided at contract initiation.

- Documentation: [docs.digitalelement.com](https://docs.digitalelement.com/)
- Issues: [GitHub Issues](https://github.com/netacuity/netacuity-client-api-unix-c/issues)

## License

Copyright 2026 Digital Envoy, Inc.

Licensed under the Apache License, Version 2.0. See [LICENSE](LICENSE) for the full license text.

This repository contains only Digital Envoy source code, and no binaries. Third-party libraries referenced at build time (libxml2, Apache httpd/APR headers, Nginx headers) are supplied by the end developer's own system and are not redistributed by this repository.
