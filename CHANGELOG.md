# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). This changelog starts at the initial
public release on GitHub; changes prior to that are not tracked here.

## [7.1.0]

### Added

- Persistent connection API: `na_connection_new()`, `na_connection_query_xml()` and `na_connection_free()`. A connection
  sends all of its queries over one UDP socket instead of opening a new socket per query as `na_query_xml()` does, which
  keeps a busy client from filling the host's connection-tracking (conntrack) table. A connection is not thread-safe;
  use one per thread.
- `test_api_persistent` example program for the persistent connection API.
- Feature codes 38, 39, 51 and 52 (`NA_DMA`, `NA_DMA_MAX`, `NA_RMM` and `NA_RMM_MAX`), their response fields, and their
  entries in the nginx examples.
- `extern "C"` wrapping in `netacuity_api.h`, so the API can be called from C++.
- Unit tests for query and response handling, run against a mock NetAcuity Server, so `make test` in `unix_c/tests`
  needs no running server.

### Changed

- `na_client_new()` returns `NULL` for a `timeout_us` of 0 or less.
- `na_query_xml()` rejects a `transaction_id` longer than 63 characters (`MAX_TRANSACTION_ID - 1`) with "Transaction ID
  is too long.".
- The timeout is rounded up to whole milliseconds.
- The client socket is created close-on-exec, so programs the caller runs (for example Apache CGI children) don't
  inherit it.
- Apache module: `NetAcuityTimeout` and `NetAcuityID` are range-checked when the configuration is read, and Apache
  reports a bad value with its file and line. `NetAcuityTimeout` accepts 0 to 2147483 ms; 0 means the default (1000 ms).
- The error for a failed `send()` now includes the reason (for example "Could not send request to NetAcuity Server:
  Broken pipe.").

### Fixed

- A process with 1024 or more open file descriptors, such as a busy nginx worker, could corrupt its stack while waiting
  for a response. Responses are now waited for with `poll()` instead of `select()`.
- A response datagram that filled the whole receive buffer was left without a terminating NUL, and bytes past it were
  copied into the caller's response.
- Socket setup errors could report the wrong reason.
- Apache module: with `NetAcuityTimeout` omitted, every query failed with "Timeout waiting for response from NetAcuity
  Server.".
- Apache module: an invalid `NetAcuityLockfile` path could crash Apache at startup or produce a garbled error.
- nginx logging example: the Pulse Plus (30) field `na_pulseplus_timezone_name` was misspelled in its label, and the Is
  An ISP (14) header was missing its ` # ` prefix.
- `na_query_xml()` documentation: the error reason is returned in `error_msg`, and `query_ip` may be IPv6.

### Removed

- The BSD-licensed `unix_c/strtok.c`, which was no longer used.

## [7.0.0]

### Added

- Initial public release of the NetAcuity unix-c Client API on GitHub.
- XML UDP query protocol, with query-IP format validation, transaction-ID validation, response-echo verification
  (transaction ID and IP) to reject spoofed or stale replies, and a `connect()`-ed UDP socket so the OS itself rejects
  packets from any other source.
- `na_client_new_with_default_api_id()` and `na_client_new_with_default_timeout()` convenience constructors, the latter
  defaulting to `NA_DEFAULT_TIMEOUT_US` (2 seconds).
- Apache License 2.0 (see [LICENSE](LICENSE) and [NOTICE](NOTICE)).
