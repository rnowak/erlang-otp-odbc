-module(odbc_SUITE).

-include_lib("common_test/include/ct.hrl").
-include_lib("stdlib/include/assert.hrl").

%% CT callbacks
-export([
    suite/0, groups/0, all/0,
    init_per_suite/1, end_per_suite/1,
    init_per_group/2, end_per_group/2,
    init_per_testcase/2, end_per_testcase/2
]).

%% Shared test cases
-export([
    select_integer/1,
    select_string/1,
    select_two_strings/1,
    select_string_and_int/1,
    select_boolean/1,
    select_datetime/1,
    select_swedish_characters/1,
    select_long_string/1,
    select_varchar_max/1,
    select_cast_varchar/1,
    select_multiple_result_sets/1,
    select_multiple_result_sets_mixed/1,
    select_null_integer/1,
    select_null_string/1,
    select_null_boolean/1,
    select_null_datetime/1,
    select_null_float/1,
    select_mixed_nulls/1,
    select_very_long_string/1,
    select_binary/1,
    select_large_binary/1,
    param_query_integer/1,
    param_query_string/1,
    param_query_binary/1,
    param_query_multiple_result_sets/1
]).

%% MSSQL-specific test cases
-export([
    mssql_raiserror/1,
    mssql_throw/1,
    mssql_divide_by_zero/1,
    mssql_long_statement/1
]).

%% PostgreSQL-specific test cases
-export([
    pg_divide_by_zero/1,
    pg_select_float/1
]).

%% ------------------------------------------------------------------
%% Macros
%% ------------------------------------------------------------------

-define(assertRows(Expected, Result),
    (fun() ->
        case Result of
            {selected, _Cols, Rows} ->
                ?assertEqual(Expected, Rows);
            _ when is_list(Result) ->
                lists:foreach(fun({E, RS}) ->
                    {selected, _C, R} = RS,
                    ?assertEqual(E, R)
                end, lists:zip(Expected, Result))
        end
    end)()).

%% ------------------------------------------------------------------
%% CT callbacks
%% ------------------------------------------------------------------

suite() ->
    [{timetrap, {minutes, 5}}].

shared_cases() ->
    [
        select_integer,
        select_string,
        select_two_strings,
        select_string_and_int,
        select_boolean,
        select_datetime,
        select_swedish_characters,
        select_long_string,
        select_varchar_max,
        select_cast_varchar,
        select_multiple_result_sets,
        select_multiple_result_sets_mixed,
        select_null_integer,
        select_null_string,
        select_null_boolean,
        select_null_datetime,
        select_null_float,
        select_mixed_nulls,
        select_very_long_string,
        select_binary,
        select_large_binary,
        param_query_integer,
        param_query_string,
        param_query_binary,
        param_query_multiple_result_sets
    ].

groups() ->
    [
        {mssql, [], shared_cases() ++ [
            mssql_raiserror,
            mssql_throw,
            mssql_divide_by_zero,
            mssql_long_statement
        ]},
        {postgres, [], shared_cases() ++ [
            pg_divide_by_zero,
            pg_select_float
        ]}
    ].

all() ->
    [
        {group, mssql},
        {group, postgres}
    ].

init_per_suite(Config) ->
    application:ensure_all_started(odbc),
    Config.

end_per_suite(_Config) ->
    application:stop(odbc),
    ok.

init_per_group(mssql, Config) ->
    Driver = os:getenv("ODBC_TEST_MSSQL_DRIVER", false),
    case Driver of
        false ->
            {skip, "ODBC_TEST_MSSQL_DRIVER not set"};
        _ ->
            Server = case os:getenv("RUNNING_IN_DOCKER", false) of
                false -> require_env("ODBC_TEST_MSSQL_SERVER");
                _ -> "sql-server"
            end,
            User = require_env("ODBC_TEST_MSSQL_USER"),
            Password = require_env("ODBC_TEST_MSSQL_PASSWORD"),
            ConnStr = "Driver=" ++ Driver ++
                      ";Server=" ++ Server ++
                      ";Database=master" ++
                      ";TrustServerCertificate=yes" ++
                      ";AutoTranslate=no" ++
                      ";Uid=" ++ User ++
                      ";Pwd=" ++ Password,
            case await_connection(ConnStr) of
                ok ->
                    [{connstr, ConnStr}, {db_type, mssql} | Config];
                {error, Reason} ->
                    {skip, Reason}
            end
    end;

init_per_group(postgres, Config) ->
    Driver = os:getenv("ODBC_TEST_PG_DRIVER", false),
    case Driver of
        false ->
            {skip, "ODBC_TEST_PG_DRIVER not set"};
        _ ->
            Server = case os:getenv("RUNNING_IN_DOCKER", false) of
                false -> require_env("ODBC_TEST_PG_SERVER");
                _ -> "postgres"
            end,
            Port = os:getenv("ODBC_TEST_PG_PORT", "5432"),
            User = require_env("ODBC_TEST_PG_USER"),
            Password = require_env("ODBC_TEST_PG_PASSWORD"),
            Database = os:getenv("ODBC_TEST_PG_DATABASE", "odbc_test"),
            ConnStr = "Driver=" ++ Driver ++
                      ";Server=" ++ Server ++
                      ";Port=" ++ Port ++
                      ";Database=" ++ Database ++
                      ";Uid=" ++ User ++
                      ";Pwd=" ++ Password,
            case await_connection(ConnStr) of
                ok ->
                    [{connstr, ConnStr}, {db_type, postgres} | Config];
                {error, Reason} ->
                    {skip, Reason}
            end
    end;

init_per_group(_, Config) ->
    Config.

end_per_group(_, _Config) ->
    ok.

init_per_testcase(_, Config) ->
    ConnStr = ?config(connstr, Config),
    {ok, Conn} = odbc:connect(ConnStr, odbc_opts()),
    [{conn, Conn} | Config].

end_per_testcase(_, Config) ->
    Conn = ?config(conn, Config),
    catch odbc:disconnect(Conn),
    ok.

%% ------------------------------------------------------------------
%% Shared test cases
%% ------------------------------------------------------------------

select_integer(Config) ->
    Ret = query("select 1", Config),
    ?assertRows([[1]], Ret).

select_string(Config) ->
    Q = sql(text_literal, "hello, world", Config),
    Ret = query("select " ++ Q, Config),
    ?assertRows([[<<"hello, world">>]], Ret).

select_two_strings(Config) ->
    A = sql(text_literal, "hello", Config),
    B = sql(text_literal, "world", Config),
    Ret = query("select " ++ A ++ ", " ++ B, Config),
    ?assertRows([[<<"hello">>, <<"world">>]], Ret).

select_string_and_int(Config) ->
    S = sql(text_literal, "abc", Config),
    Ret = query("select " ++ S ++ ", 1", Config),
    ?assertRows([[<<"abc">>, 1]], Ret).

select_boolean(Config) ->
    Q = sql(boolean_pair, Config),
    Ret = query(Q, Config),
    Expected = sql(boolean_expected, Config),
    ?assertRows([Expected], Ret).

select_datetime(Config) ->
    Q = sql(timestamp_literal, "2024-01-01 12:13:14", Config),
    Ret = query(Q, Config),
    ?assertRows([[{{2024, 1, 1}, {12, 13, 14}}]], Ret).

select_swedish_characters(Config) ->
    {Q, Expected} = sql(swedish_characters, Config),
    Ret = query(Q, Config),
    ?assertRows([[Expected]], Ret).

select_long_string(Config) ->
    Q = sql(repeat_string, "A", "B", 7998, "C", Config),
    Ret = query(Q, Config),
    Expected = iolist_to_binary(["A", lists:duplicate(7998, $B), "C"]),
    ?assertRows([[Expected]], Ret).

select_varchar_max(Config) ->
    Q = sql(varchar_max, "A", "B", 7998, "C", Config),
    Ret = query(Q, Config),
    Expected = iolist_to_binary(["A", lists:duplicate(7998, $B), "C"]),
    ?assertRows([[Expected]], Ret).

select_cast_varchar(Config) ->
    Ret = query("select cast('abc' as varchar(128)), cast('def' as varchar(16))", Config),
    ?assertRows([[<<"abc">>, <<"def">>]], Ret).

select_multiple_result_sets(Config) ->
    require_feature(multiple_result_sets, Config),
    Ret = query("select 1 select 2", Config),
    ?assertRows([[[1]], [[2]]], Ret).

select_multiple_result_sets_mixed(Config) ->
    require_feature(multiple_result_sets, Config),
    Ret = query("select 1,2 select 3,4 union all select 5,6", Config),
    ?assertRows([[[1, 2]], [[3, 4], [5, 6]]], Ret).

param_query_integer(Config) ->
    Conn = ?config(conn, Config),
    Q = sql(param_cast, "integer", Config),
    Ret = odbc:param_query(Conn, Q, [{sql_integer, [42]}]),
    ?assertRows([[42]], Ret).

param_query_string(Config) ->
    Conn = ?config(conn, Config),
    Q = sql(param_cast, "text", Config),
    Ret = odbc:param_query(Conn, Q, [{{sql_varchar, 128}, [<<"hello">>]}]),
    ?assertRows([[<<"hello">>]], Ret).

param_query_multiple_result_sets(Config) ->
    %% param_query only returns the first result set regardless of DB,
    %% so we just verify the first SELECT's result.
    require_feature(multiple_result_sets, Config),
    Conn = ?config(conn, Config),
    Ret = odbc:param_query(Conn, "select ? select ?",
        [{sql_integer, [5]}, {sql_integer, [7]}]),
    ?assertRows([[5]], Ret).

%% -- NULL handling tests (regression for uninitialized strlen_or_indptr) --

select_null_integer(Config) ->
    Ret = query(sql(cast_null, "integer", Config), Config),
    ?assertRows([[null]], Ret).

select_null_string(Config) ->
    Ret = query(sql(cast_null, "text", Config), Config),
    ?assertRows([[null]], Ret).

select_null_boolean(Config) ->
    Ret = query(sql(cast_null, "boolean", Config), Config),
    ?assertRows([[null]], Ret).

select_null_datetime(Config) ->
    Ret = query(sql(cast_null, "datetime", Config), Config),
    ?assertRows([[null]], Ret).

select_null_float(Config) ->
    Ret = query(sql(cast_null, "float", Config), Config),
    ?assertRows([[null]], Ret).

select_mixed_nulls(Config) ->
    %% Mix of NULL and non-NULL values in a single row across multiple
    %% unbound column types.  Exercises the strlen_or_indptr initialisation
    %% because each column goes through encode_column_dyn independently.
    Q = sql(mixed_nulls, Config),
    Ret = query(Q, Config),
    Expected = sql(mixed_nulls_expected, Config),
    ?assertRows([Expected], Ret).

%% -- Multi-chunk retrieval (regression for get_long_data / SQLLEN) --

select_very_long_string(Config) ->
    %% 20000 bytes — larger than LONG_DATA_CHUNK_SIZE (8192), forces
    %% multiple SQLGetData round-trips.
    Q = sql(repeat_string, "X", "Y", 19998, "Z", Config),
    Ret = query(Q, Config),
    Expected = iolist_to_binary(["X", lists:duplicate(19998, $Y), "Z"]),
    ?assertRows([[Expected]], Ret).

%% -- Binary data tests (SQL_C_BINARY / sql_longvarbinary) --

select_binary(Config) ->
    require_feature(native_binary, Config),
    Q = sql(binary_literal, 4, Config),
    Ret = query(Q, Config),
    {selected, _Cols, [[Val]]} = Ret,
    ?assertEqual(4, byte_size(Val)).

select_large_binary(Config) ->
    require_feature(native_binary, Config),
    %% Binary data larger than LONG_DATA_CHUNK_SIZE (8192 bytes).
    Q = sql(large_binary, 16000, Config),
    Ret = query(Q, Config),
    {selected, _Cols, [[Val]]} = Ret,
    ?assertEqual(16000, byte_size(Val)).

param_query_binary(Config) ->
    require_feature(native_binary, Config),
    Conn = ?config(conn, Config),
    Blob = crypto:strong_rand_bytes(256),
    Q = sql(param_binary, Config),
    Ret = odbc:param_query(Conn, Q, [{{sql_longvarbinary, 256}, [Blob]}]),
    ?assertRows([[Blob]], Ret).

%% ------------------------------------------------------------------
%% MSSQL-specific test cases
%% ------------------------------------------------------------------

mssql_raiserror(Config) ->
    Ret = query("raiserror('Test Error', 11, 1)", Config),
    ?assertMatch({error, _}, Ret).

mssql_throw(Config) ->
    Ret = query("throw 60000, 'Test Error', 1", Config),
    ?assertMatch({error, _}, Ret).

mssql_divide_by_zero(Config) ->
    Ret = query("select 1/0", Config),
    ?assertMatch({error, _}, Ret).

mssql_long_statement(Config) ->
    Value = lists:flatten(["A", lists:duplicate(7998, $B), "C"]),
    Q = "select '" ++ Value ++ "'",
    Ret = query(Q, Config),
    Expected = list_to_binary(Value),
    ?assertRows([[Expected]], Ret).

%% ------------------------------------------------------------------
%% PostgreSQL-specific test cases
%% ------------------------------------------------------------------

pg_divide_by_zero(Config) ->
    Ret = query("select 1/0", Config),
    ?assertMatch({error, _}, Ret).

pg_select_float(Config) ->
    Ret = query("select 3.14::double precision", Config),
    {selected, _Cols, [[Val]]} = Ret,
    ?assert(is_float(Val)),
    ?assert(abs(Val - 3.14) < 0.001).

%% ------------------------------------------------------------------
%% Helpers
%% ------------------------------------------------------------------

query(SQL, Config) ->
    Conn = ?config(conn, Config),
    ct:pal("Query: ~p", [SQL]),
    Ret = odbc:sql_query(Conn, SQL),
    ct:pal("Result: ~p", [Ret]),
    Ret.

odbc_opts() ->
    [
        {binary_strings, on},
        {tuple_row, off},
        {auto_commit, on},
        {timeout, 5000},
        {scrollable_cursors, off},
        {extended_errors, off}
    ].

require_env(Var) ->
    case os:getenv(Var) of
        false -> ct:fail("Required environment variable ~s not set", [Var]);
        Val -> Val
    end.

await_connection(ConnStr) ->
    await_connection(ConnStr, 60).

await_connection(_ConnStr, 0) ->
    {error, "Timed out waiting for database connection"};
await_connection(ConnStr, N) ->
    ct:pal("Awaiting database connection (~p attempts left)...", [N]),
    case catch odbc:connect(ConnStr, [{timeout, 500}]) of
        {ok, Conn} ->
            catch odbc:disconnect(Conn),
            ok;
        _Err ->
            timer:sleep(500),
            await_connection(ConnStr, N - 1)
    end.

%% ------------------------------------------------------------------
%% SQL dialect helpers
%%
%% Encapsulate RDBMS-specific SQL syntax so test cases stay clean.
%% ------------------------------------------------------------------

db(Config) -> ?config(db_type, Config).

require_feature(multiple_result_sets, Config) ->
    case db(Config) of
        mssql -> ok;
        _ -> throw({skip, "Not supported by this database"})
    end;
require_feature(native_binary, Config) ->
    case db(Config) of
        mssql -> ok;
        _ -> throw({skip, "Native binary type not supported by this driver"})
    end.

%% Cast a string literal to text type
sql(text_literal, Str, Config) ->
    case db(Config) of
        mssql    -> "'" ++ Str ++ "'";
        postgres -> "'" ++ Str ++ "'::text"
    end;

%% "select ?" with an optional type cast for postgres
sql(param_cast, Type, Config) ->
    case db(Config) of
        mssql    -> "select ?";
        postgres -> "select ?::" ++ Type
    end;

%% Timestamp literal
sql(timestamp_literal, Value, Config) ->
    case db(Config) of
        mssql    -> "select cast('" ++ Value ++ "' as datetime)";
        postgres -> "select '" ++ Value ++ "'::timestamp"
    end;

%% Cast NULL to a specific type
sql(cast_null, "boolean", Config) ->
    case db(Config) of
        mssql    -> "select cast(null as bit)";
        postgres -> "select null::boolean"
    end;
sql(cast_null, "datetime", Config) ->
    case db(Config) of
        mssql    -> "select cast(null as datetime)";
        postgres -> "select null::timestamp"
    end;
sql(cast_null, "float", Config) ->
    case db(Config) of
        mssql    -> "select cast(null as float)";
        postgres -> "select null::double precision"
    end;
sql(cast_null, Type, Config) ->
    case db(Config) of
        mssql    -> "select cast(null as " ++ Type ++ ")";
        postgres -> "select null::" ++ Type
    end;

%% Small binary literal
sql(binary_literal, Size, Config) ->
    Hex = binary_to_hex(crypto:strong_rand_bytes(Size)),
    case db(Config) of
        mssql    -> "select 0x" ++ Hex;
        postgres -> "select E'\\\\x" ++ Hex ++ "'::bytea"
    end;

%% Large binary (bigger than LONG_DATA_CHUNK_SIZE)
sql(large_binary, Size, Config) ->
    SizeStr = integer_to_list(Size),
    case db(Config) of
        mssql ->
            "select crypt_gen_random(" ++ SizeStr ++ ")";
        postgres ->
            %% Build a deterministic bytea of the desired size using
            %% repeat + decode (no pgcrypto extension required).
            "select decode(repeat('AB', " ++ SizeStr ++ "), 'hex')"
    end.

%% --- sql/2: helpers that take only Config ---

%% Boolean pair: select true, false
sql(boolean_pair, Config) ->
    case db(Config) of
        mssql    -> "select cast(1 as bit), cast(0 as bit)";
        postgres -> "select true, false"
    end;

%% Expected result for boolean pair
sql(boolean_expected, Config) ->
    case db(Config) of
        mssql    -> [true, false];
        postgres -> [<<"1">>, <<"0">>]
    end;

%% Swedish characters: query + expected binary
sql(swedish_characters, Config) ->
    case db(Config) of
        mssql ->
            %% CHAR() produces varchar; with AutoTranslate=no, raw Latin-1 bytes
            {"select CHAR(0xE5)+CHAR(0xE4)+CHAR(0xF6)+CHAR(0xC5)+CHAR(0xC4)+CHAR(0xD6)",
             <<229, 228, 246, 197, 196, 214>>};
        postgres ->
            %% PostgreSQL returns UTF-8 encoded bytes
            {"select E'\\xC3\\xA5\\xC3\\xA4\\xC3\\xB6\\xC3\\x85\\xC3\\x84\\xC3\\x96'::text",
             unicode:characters_to_binary([229, 228, 246, 197, 196, 214])}
    end;

%% param_query that accepts a binary parameter
sql(param_binary, Config) ->
    case db(Config) of
        mssql    -> "select ?";
        postgres -> "select ?::bytea"
    end;

%% Mixed NULL and non-NULL values in a single row
sql(mixed_nulls, Config) ->
    case db(Config) of
        mssql ->
            "select 42, cast(null as varchar(64)), cast(1 as bit), "
            "cast(null as datetime), 3.14, cast(null as int)";
        postgres ->
            "select 42, null::text, true, "
            "null::timestamp, 3.14::double precision, null::integer"
    end;

sql(mixed_nulls_expected, Config) ->
    case db(Config) of
        mssql    -> [42, null, true, null, 3.14, null];
        postgres -> [42, null, <<"1">>, null, 3.14, null]
    end.

%% --- sql/6: long string builders ---

%% Build a long string: Prefix + repeated Char + Suffix
sql(repeat_string, Prefix, Char, N, Suffix, Config) ->
    Count = integer_to_list(N),
    case db(Config) of
        mssql ->
            "select '" ++ Prefix ++ "'+replicate('" ++ Char ++ "', " ++ Count ++ ")+'" ++ Suffix ++ "'";
        postgres ->
            "select '" ++ Prefix ++ "' || repeat('" ++ Char ++ "', " ++ Count ++ ") || '" ++ Suffix ++ "'"
    end;

%% Same but cast to varchar(max) / text
sql(varchar_max, Prefix, Char, N, Suffix, Config) ->
    Count = integer_to_list(N),
    case db(Config) of
        mssql ->
            "select cast('" ++ Prefix ++ "'+replicate('" ++ Char ++ "', " ++ Count ++ ")+'" ++ Suffix ++ "' as varchar(max))";
        postgres ->
            "select cast('" ++ Prefix ++ "' || repeat('" ++ Char ++ "', " ++ Count ++ ") || '" ++ Suffix ++ "' as text)"
    end.

%% ------------------------------------------------------------------
%% Internal helpers
%% ------------------------------------------------------------------

binary_to_hex(Bin) ->
    lists:flatten([io_lib:format("~2.16.0B", [B]) || <<B>> <= Bin]).
