# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.17.0]

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
