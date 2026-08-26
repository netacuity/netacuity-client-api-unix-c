# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
This changelog starts at the initial public release on GitHub; changes prior to that are not tracked here.

## [7.0.0]

### Added
- Initial public release of the NetAcuity unix-c Client API on GitHub.
- XML UDP query protocol, with query-IP format validation, transaction-ID validation, response-echo verification (transaction ID and IP) to reject spoofed or stale replies, and a `connect()`-ed UDP socket so the OS itself rejects packets from any other source.
- `na_client_new_with_default_api_id()` and `na_client_new_with_default_timeout()` convenience constructors, the latter defaulting to `NA_DEFAULT_TIMEOUT_US` (2 seconds).
- Apache License 2.0 (see [LICENSE](LICENSE) and [NOTICE](NOTICE)); LICENSE also reproduces the BSD license notice for the bundled `unix_c/strtok.c`.
