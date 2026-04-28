%%
%% Tests for string_terminate/1 and wstring_terminate/1 null terminator handling.
%%
-module(odbc_string_terminate_SUITE).

-compile(export_all).

-include_lib("common_test/include/ct.hrl").
-include_lib("stdlib/include/assert.hrl").

suite() -> [].

all() ->
    [string_terminate_list_no_extra_null,
     string_terminate_list_empty,
     string_terminate_binary_single_byte_null,
     string_terminate_binary_empty,
     string_terminate_null_value,
     wstring_terminate_binary_two_byte_null,
     wstring_terminate_binary_empty,
     wstring_terminate_null_value,
     wstring_terminate_rejects_list,
     string_terminate_mixed_values,
     string_terminate_max_length_char_binary,
     wstring_terminate_max_length_wchar_binary,
     %% Buffer sizing tests
     char_list_fits_in_buffer,
     char_list_empty_fits_in_buffer,
     char_binary_fits_in_buffer,
     char_binary_empty_fits_in_buffer,
     wchar_binary_fits_in_buffer,
     wchar_binary_empty_fits_in_buffer,
     wchar_null_terminator_is_one_wide_char].

init_per_suite(Config) ->
    Config.

end_per_suite(_Config) ->
    ok.

%%--------------------------------------------------------------------
%% string_terminate tests (for CHAR types)
%%--------------------------------------------------------------------

string_terminate_list_no_extra_null(_Config) ->
    %% List values should NOT have a null terminator appended,
    %% because ei_decode_string on the C side adds one.
    Result = odbc:string_terminate(["hello"]),
    ?assertEqual(["hello"], Result).

string_terminate_list_empty(_Config) ->
    %% Empty string (list) should remain empty.
    Result = odbc:string_terminate([""]),
    ?assertEqual([""], Result).

string_terminate_binary_single_byte_null(_Config) ->
    %% Binary values for CHAR types should get a 1-byte null terminator.
    Result = odbc:string_terminate([<<"hello">>]),
    ?assertEqual([<<"hello", 0:8>>], Result).

string_terminate_binary_empty(_Config) ->
    %% Empty binary should get just the 1-byte null terminator.
    Result = odbc:string_terminate([<<>>]),
    ?assertEqual([<<0:8>>], Result).

string_terminate_null_value(_Config) ->
    %% null atoms should pass through unchanged.
    Result = odbc:string_terminate([null]),
    ?assertEqual([null], Result).

%%--------------------------------------------------------------------
%% wstring_terminate tests (for WCHAR types)
%%--------------------------------------------------------------------

wstring_terminate_binary_two_byte_null(_Config) ->
    %% Binary values for WCHAR types should get a 2-byte (SQLWCHAR) null terminator.
    Utf16Hello = unicode:characters_to_binary("hello", latin1, {utf16, little}),
    Result = odbc:wstring_terminate([Utf16Hello]),
    Expected = <<Utf16Hello/binary, 0:16>>,
    ?assertEqual([Expected], Result).

wstring_terminate_binary_empty(_Config) ->
    %% Empty binary should get just the 2-byte null terminator.
    Result = odbc:wstring_terminate([<<>>]),
    ?assertEqual([<<0:16>>], Result).

wstring_terminate_null_value(_Config) ->
    %% null atoms should pass through unchanged.
    Result = odbc:wstring_terminate([null]),
    ?assertEqual([null], Result).

wstring_terminate_rejects_list(_Config) ->
    %% WCHAR types require binary values (UTF-16 encoded), not lists.
    ?assertError({badarg, wchar_type_requires_binary}, odbc:wstring_terminate(["hello"])).

%%--------------------------------------------------------------------
%% Integration-style tests
%%--------------------------------------------------------------------

string_terminate_mixed_values(_Config) ->
    %% Multiple values including null should all be handled correctly.
    Result = odbc:string_terminate(["first", null, "third"]),
    ?assertEqual(["first", null, "third"], Result).

string_terminate_max_length_char_binary(_Config) ->
    %% A binary exactly at max size should get a proper 1-byte null.
    %% With Max=5, buffer is 6 bytes. Binary "hello" + 0x00 = 6 bytes. Fits exactly.
    Data = <<"hello">>,
    [Terminated] = odbc:string_terminate([Data]),
    ?assertEqual(<<Data/binary, 0:8>>, Terminated),
    ?assertEqual(6, byte_size(Terminated)).

wstring_terminate_max_length_wchar_binary(_Config) ->
    %% A UTF-16 binary exactly at max size should get a proper 2-byte null.
    %% With Max=5, buffer is (5+1)*2 = 12 bytes.
    %% UTF-16 "hello" = 10 bytes + 2-byte null = 12 bytes. Fits exactly.
    Utf16Hello = unicode:characters_to_binary("hello", latin1, {utf16, little}),
    [Terminated] = odbc:wstring_terminate([Utf16Hello]),
    Expected = <<Utf16Hello/binary, 0:16>>,
    ?assertEqual(Expected, Terminated),
    ?assertEqual(12, byte_size(Terminated)).

%%--------------------------------------------------------------------
%% Buffer sizing tests
%%
%% These verify that terminated data fits exactly in the C-side buffer.
%% C buffer sizes:
%%   CHAR:  Max + 1  (bytes)
%%   WCHAR: (Max + 1) * sizeof(SQLWCHAR), where sizeof(SQLWCHAR) = 2
%%--------------------------------------------------------------------

-define(SIZEOF_SQLWCHAR, 2).

char_list_fits_in_buffer(_Config) ->
    %% For CHAR list path: ei_decode_string writes length(String) + 1 bytes.
    %% string_terminate does NOT add a null (C side does).
    %% Buffer = Max + 1.
    Max = 5,
    BufferSize = Max + 1,
    String = "hello",  %% exactly Max chars
    [Terminated] = odbc:string_terminate([String]),
    %% Terminated is still just the string (no null added)
    ?assertEqual(String, Terminated),
    %% ei_decode_string will write length + 1 bytes (adds \0)
    BytesWrittenByC = length(Terminated) + 1,
    ?assertEqual(BufferSize, BytesWrittenByC).

char_list_empty_fits_in_buffer(_Config) ->
    %% Empty string with Max=0: buffer = 1, C writes 1 byte (\0).
    %% Note: empty list is ERL_NIL_EXT, handled specially in C to write \0.
    Max = 0,
    BufferSize = Max + 1,
    [Terminated] = odbc:string_terminate([""]),
    ?assertEqual("", Terminated),
    %% C side handles ERL_NIL_EXT by writing a single \0 byte
    BytesWrittenByC = 1,
    ?assert(BytesWrittenByC =< BufferSize).

char_binary_fits_in_buffer(_Config) ->
    %% For CHAR binary path: ei_decode_binary copies exact bytes.
    %% string_terminate appends <<0:8>> (1 byte null).
    %% Buffer = Max + 1.
    Max = 5,
    BufferSize = Max + 1,
    Data = <<"hello">>,  %% exactly Max bytes
    [Terminated] = odbc:string_terminate([Data]),
    ?assertEqual(<<Data/binary, 0:8>>, Terminated),
    ?assertEqual(BufferSize, byte_size(Terminated)).

char_binary_empty_fits_in_buffer(_Config) ->
    %% Empty binary with Max=0: buffer = 1, terminated = <<0>> = 1 byte.
    Max = 0,
    BufferSize = Max + 1,
    [Terminated] = odbc:string_terminate([<<>>]),
    ?assertEqual(<<0:8>>, Terminated),
    ?assertEqual(BufferSize, byte_size(Terminated)).

wchar_binary_fits_in_buffer(_Config) ->
    %% For WCHAR binary path: ei_decode_binary copies exact bytes.
    %% wstring_terminate appends <<0:16>> (one SQLWCHAR null terminator).
    %% Buffer = (Max + 1) * sizeof(SQLWCHAR).
    Max = 5,
    BufferSize = (Max + 1) * ?SIZEOF_SQLWCHAR,
    Utf16Hello = unicode:characters_to_binary("hello", latin1, {utf16, little}),
    ?assertEqual(Max * ?SIZEOF_SQLWCHAR, byte_size(Utf16Hello)),  %% sanity check
    [Terminated] = odbc:wstring_terminate([Utf16Hello]),
    ?assertEqual(BufferSize, byte_size(Terminated)).

wchar_binary_empty_fits_in_buffer(_Config) ->
    %% Empty WCHAR binary with Max=0: buffer = 1 * 2 = 2 bytes.
    %% Terminated = <<0:16>> = 2 bytes (one SQLWCHAR null).
    Max = 0,
    BufferSize = (Max + 1) * ?SIZEOF_SQLWCHAR,
    [Terminated] = odbc:wstring_terminate([<<>>]),
    ?assertEqual(<<0:16>>, Terminated),
    ?assertEqual(BufferSize, byte_size(Terminated)).

wchar_null_terminator_is_one_wide_char(_Config) ->
    %% Verify that the WCHAR null terminator is exactly one SQLWCHAR (2 bytes),
    %% not two separate null bytes. This is important because ODBC wide strings
    %% are terminated by a single null wide character.
    [Terminated] = odbc:wstring_terminate([<<>>]),
    ?assertEqual(<<0, 0>>, Terminated),
    ?assertEqual(?SIZEOF_SQLWCHAR, byte_size(Terminated)).
