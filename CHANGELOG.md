# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.18.0] - unreleased

### Added

- Support for `sql_longvarchar` in parameterized queries (`param_query/3,4`).
  The new `{sql_longvarchar, Size}` type allows inserting and querying
  `SQL_LONGVARCHAR` / `TEXT` columns via `param_query`, complementing the
  existing read-only support. The implementation follows the same pattern as
  `sql_varchar` on both the C and Erlang sides.
- End-to-end tests for `sql_longvarchar` param queries covering single value,
  multi-row insert, NULL handling, and large strings (>8000 bytes), verified
  against both PostgreSQL and MSSQL.
- C unit test harness (`c_src/test_decode_params.c`) with 28 tests exercising
  parameter decoding and buffer allocation for all type paths (CHAR, WCHAR,
  BINARY), including overflow detection, null handling, empty strings, and
  zero-size edge cases. Tests run without a database via ODBC function stubs.
- AddressSanitizer, UndefinedBehaviorSanitizer, and Valgrind targets in the
  C Makefile (`make test-sanitize`, `make test-valgrind`).
- GitHub Actions workflow (`c-tests.yml`) for C unit tests with ASan/UBSan
  and Valgrind on every push and pull request.
- End-to-end regression tests in `odbc_SUITE` for empty string `param_query`
  with Size=0, validated against both PostgreSQL and MSSQL.

### Fixed

- Fix null terminator handling for parameterized string types, preventing
  buffer overflows in `decode_params`. CHAR list values had a double null
  terminator (Erlang and C both added one), and CHAR binary values received
  a 2-byte wide null instead of a 1-byte null. WCHAR binary values were
  already correct.
- Add pre-decode bounds checks in `decode_params` using `ei_get_type()` to
  validate data size before writing into the buffer, and verify term types
  (`ERL_BINARY_EXT`, `ERL_STRING_EXT`) before decoding.
- Handle empty strings (`ERL_NIL_EXT`) in the CHAR list decoding path, which
  were previously rejected.
- Fix `col_size=0` causing `SQLBindParameter` failure when using
  `{sql_longvarchar, 0}`, `{sql_varchar, 0}`, or any CHAR/WCHAR type with
  Size=0. ODBC drivers reject `col_size=0`, which crashed the C port
  (`connection_closed`). The column size is now clamped to a minimum of 1.
  Negative size values are also guarded against.

## [2.17.1] - 2026-04-09

### Fixed

- Initialize `strlen_or_indptr` for unbound columns in `alloc_column_buffer`,
  preventing sporadic null returns for non-null values when the uninitialized
  memory happened to equal `SQL_NULL_DATA`
- Use `SQLCHAR` with `SQL_C_BIT` instead of `SQLINTEGER` with `SQL_C_TINYINT`
  for boolean column retrieval, matching the ODBC spec and fixing portability
  issues on big-endian architectures
- Widen `encode_binary_or_string` result length parameter from `int` to `SQLLEN`
  to avoid silent truncation of values larger than 2 GB
- Map `SQL_BINARY`, `SQL_VARBINARY`, and `SQL_LONGVARBINARY` column types to
  `SQL_C_BINARY` for retrieval, returning raw bytes instead of hex strings.
  Binary data is now always returned as an Erlang binary regardless of the
  `binary_strings` connection option.

### Added

- Regression tests for NULL handling across all column types
- Tests for multi-chunk binary retrieval and `sql_longvarbinary` param queries
- PostgreSQL connections now use `ByteaAsLongVarBinary=1`, enabling native
  binary retrieval for `bytea` columns

## [2.17.0] - 2026-04-09

### Added

- Compatibility with modern ODBC SQL Server drivers (`{ODBC Driver 18 for SQL Server}`)
- Thanks to the support for new SQL Server drivers, the application can now be used towards SQL Server on Linux and MacOS
- Support for Variable-length types of Arbitrary Sizes
  The limitation of 8000 bytes (`MAXCOLSIZE`) has been removed and retrieval of
  variable-length types now supports arbitrary sizes. This uses chunking with a
  default set to 8192 bytes.
- LONG VARBINARY (SQL_LONGVARBINARY) is now supported

- The application now uses [Rebar3](https://github.com/erlang/rebar3) as its build tool
- The Rebar3 plugin [rebar3_ex_doc](https://github.com/jelly-beam/rebar3_ex_doc) is used for the documentation
- The test suite has been expanded that exercises the new capabilities
- A docker-based test runner has been added for MSSQL and PostgreSQL
- GitHub actions have been added for CI/CD

### Fixed

- Compatibility with SQL Server has been restored, following a breaking change in Erlang/OTP 26.

## [2.16.1] - Unreleased

The ODBC application has been extracted from the [Erlang/OTP repository](https://github.com/erlang/otp)
following its deprecation in Erlang/OTP 29 and eventual removal in Erlang/OTP 30.
