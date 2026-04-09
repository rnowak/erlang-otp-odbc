# Erlang/OTP ODBC Application

A continuation of the Erlang/OTP ODBC application. Extracted from the
[Erlang/OTP repository](https://github.com/erlang/otp) following its deprecation
in Erlang/OTP 29 and eventual removal in Erlang/OTP 30.

The application now uses [Rebar3](https://github.com/erlang/rebar3) as its build
tool, and the Rebar3 plugin
[rebar3_ex_doc](https://github.com/jelly-beam/rebar3_ex_doc) is used for the
documentation.

## Major Enhancements

### Compatibility with Microsoft SQL Server

Compatibility with SQL Server has been restored, following a breaking change in
Erlang/OTP 26.

### Compatibility with Modern ODBC SQL Server Drivers

`odbcserver` has been made compatible for use with modern ODBC SQL drivers,
such as `{ODBC Driver 18 for SQL Server}`. This makes it possible to use the
application on Linux and MacOS with unixODBC.

Previously, using drivers other than the very old, and very deprecated `{SQL Server}`,
caused silent failures and data corruption. This driver is also limited to Microsoft
Windows.

### Support for Variable-length types of Arbitrary Sizes

The limitation of 8000 bytes (`MAXCOLSIZE`) has been removed and retrieval of
variable-length types now supports arbitrary sizes. This uses chunking with a
default set to 8192 bytes.

#### Support for LONG VARBINARY (SQL_LONGVARBINARY)

LONG VARBINARY (SQL_LONGVARBINARY) is now supported.

## License

Licensed under Apache License 2.0, as Erlang/OTP.

> SPDX-License-Identifier: Apache-2.0
>
> Copyright Ericsson AB 2010-2025. All Rights Reserved.
>
> Licensed under the Apache License, Version 2.0 (the "License"); you may not 
> use this file except in compliance with the License. You may obtain a copy 
> of the License at
>
> http://www.apache.org/licenses/LICENSE-2.0
>
> Unless required by applicable law or agreed to in writing, software 
> distributed under the License is distributed on an "AS IS" BASIS, WITHOUT 
> WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the 
> License for the specific language governing permissions and limitations under
> the License.
