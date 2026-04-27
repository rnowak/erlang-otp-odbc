# Copilot Instructions

## Project Overview

Standalone Erlang ODBC application extracted from Erlang/OTP. An Erlang gen_server (`src/odbc.erl`) communicates with a C port program (`c_src/odbcserver.c`) over TCP sockets. The C side links against unixODBC/iODBC and uses POSIX threads.

## Build & Test Commands

```bash
# Build
rebar3 compile

# C unit tests (no database needed)
cd c_src && make test                   # 28 tests
cd c_src && make test-sanitize          # with ASan/UBSan
cd c_src && make test-valgrind          # with Valgrind (Linux only)

# Erlang unit tests (no database needed)
rebar3 ct --suite=test/odbc_string_terminate_SUITE

# Integration tests against real databases (Docker required)
cd test/odbc_SUITE_data
docker compose --profile pg build && docker compose --profile pg up --exit-code-from test-runner-pg
docker compose --profile mssql build && docker compose --profile mssql up --exit-code-from test-runner-mssql
docker compose --profile pg down -v     # cleanup

# Run a specific Erlang test case
rebar3 as test ct --suite test/odbc_SUITE --group postgres --case param_query_longvarchar
```

## Architecture

### Erlang ↔ C Communication

1. Erlang spawns `priv/bin/odbcserver` as a port
2. Port numbers are exchanged over stdin, then communication switches to two TCP sockets (supervisor + database handler)
3. Messages: command byte (1–18) + encoded parameters → C decodes with `ei_*` functions
4. Responses: C encodes Erlang terms via `ei_x_*` → Erlang decodes with `binary_to_term`

The socket switch exists because some ODBC drivers interfere with stdin/stdout.

### C Threading Model

`odbcserver` runs two threads:
- **Supervisor thread**: listens for SHUTDOWN command, kills process on disconnect
- **Database handler thread**: receives commands, calls ODBC API, sends responses

### Parameter Encoding (param_query)

Erlang `fix_params/1` maps user types to `USER_*` constants (defined in both `src/odbc_internal.hrl` and `c_src/odbcserver.h`). The C function `init_param_column` allocates buffers, and `decode_params` fills them from the ei-encoded message.

**String null termination convention** (critical for correctness):
- CHAR lists: Erlang does NOT add `\0` — `ei_decode_string` adds it (1 byte)
- CHAR binaries: Erlang appends `<<0:8>>` — `ei_decode_binary` copies as-is
- WCHAR binaries: Erlang appends `<<0:16>>` — 2-byte SQLWCHAR null terminator

Buffer sizes: CHAR = `Max + 1`, WCHAR = `(Max + 1) * sizeof(SQLWCHAR)`.

### Long Data Retrieval

Types `SQL_LONGVARCHAR`, `SQL_WLONGVARCHAR`, `SQL_LONGVARBINARY` use chunked retrieval via `SQLGetData` with 8192-byte chunks, reassembled into a single buffer. This replaces the old 8KB hard limit.

## Key Conventions

### Adding a New SQL Parameter Type

1. Add `USER_NEWTYPE` constant to `c_src/odbcserver.h` and `src/odbc_internal.hrl`
2. Add case in `init_param_column()` (buffer allocation + type mapping)
3. Add case in `decode_params()` (value decoding from ei buffer)
4. Add `fix_params` clause in `src/odbc.erl`
5. Add C unit test in `c_src/test_decode_params.c` and integration test in `test/odbc_SUITE.erl`

### C Memory Management

- `safe_malloc(size)` — exits process on failure via `DO_EXIT(EXIT_ALLOC)`
- `DO_EXIT(code)` — in production: `_exit(code)`; in test harness: `longjmp`
- Pre-decode bounds checks using `ei_get_type()` before `ei_decode_*` calls

### C Test Harness Pattern

`test_decode_params.c` uses `#include "odbcserver.c"` with `#define TEST_HARNESS` to access static functions. ODBC calls are stubbed via `#define SQLXxx(...) SQL_SUCCESS`. The `TEST_HARNESS` flag gates out `main()`, socket functions, and replaces `DO_EXIT` with `longjmp`.

### Erlang Test Patterns (odbc_SUITE.erl)

- Connection options always include `{binary_strings, on}` and `{tuple_row, off}`
- SQL dialect differences are abstracted via `sql(Operation, Args, Config)` helper
- `?assertRows(Expected, Result)` macro for result comparison
- Tables are created per-test with unique names and dropped in cleanup
- `shared_cases()` run against both PostgreSQL and MSSQL

### CHANGELOG

Uses [Keep a Changelog](https://keepachangelog.com/) format. Update under the current version section with `Added`, `Changed`, or `Fixed` headings.

### Versioning

Version must be consistent across `src/odbc.app.src`, `src/odbc.appup.src`, and the git tag. The release CI workflow (`release.yml`) validates this.
