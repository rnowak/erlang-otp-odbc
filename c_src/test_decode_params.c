/*
 * Test suite for odbcserver decode_params and buffer allocation.
 *
 * Uses the #include "odbcserver.c" pattern to access static functions.
 * ODBC calls are stubbed out since we only test the ei encoding/decoding
 * and buffer management, not actual database operations.
 */

/* ---- Stub ODBC functions before including odbcserver.c ---- */

/* Provide ODBC type definitions without linking the real library */
#include "sql.h"
#include "sqlext.h"

/* Stub all ODBC functions used by odbcserver.c to no-ops / SQL_SUCCESS.
 * We never exercise paths that actually call the database. */
#define SQLDriverConnect(a,b,c,d,e,f,g,h) SQL_SUCCESS
#define SQLDisconnect(a) SQL_SUCCESS
#define SQLFreeHandle(a,b) SQL_SUCCESS
#define SQLEndTran(a,b,c) SQL_SUCCESS
#define SQLAllocHandle(a,b,c) SQL_SUCCESS
#define SQLExecDirect(a,b,c) SQL_SUCCESS
#define SQLNumResultCols(a,b) SQL_SUCCESS
#define SQLRowCount(a,b) SQL_SUCCESS
#define SQLSetConnectAttr(a,b,c,d) SQL_SUCCESS
#define SQLSetEnvAttr(a,b,c,d) SQL_SUCCESS
#define SQLSetStmtAttr(a,b,c,d) SQL_SUCCESS
#define SQLDescribeCol(a,b,c,d,e,f,g,h,i) SQL_SUCCESS
#define SQLBindCol(a,b,c,d,e,f) SQL_SUCCESS
#define SQLFetch(a) SQL_NO_DATA
#define SQLFetchScroll(a,b,c) SQL_NO_DATA
#define SQLPrepare(a,b,c) SQL_SUCCESS
#define SQLBindParameter(a,b,c,d,e,f,g,h,i,j) SQL_SUCCESS
#define SQLExecute(a) SQL_SUCCESS
#define SQLGetInfo(a,b,c,d,e) SQL_SUCCESS
#define SQLMoreResults(a) SQL_NO_DATA
#define SQLGetDiagRec(a,b,c,d,e,f,g,h) SQL_NO_DATA
#define SQLFreeStmt(a,b) SQL_SUCCESS
#define SQLGetData(a,b,c,d,e,f) SQL_NO_DATA

/* Tell odbcserver.c we're in test mode — this skips main(), socket
 * functions, and uses longjmp-based DO_EXIT instead of _exit() */
#define TEST_HARNESS 1

/* These are declared extern in odbcserver.c when TEST_HARNESS is set */
#include <setjmp.h>
jmp_buf test_exit_buf;
int test_exit_code = 0;
int test_expect_exit = 0;

/* Now include the actual implementation */
#include "odbcserver.c"

/* ---- Test framework ---- */
#include "test_harness.h"

/* ---- Helper: create a minimal db_state ---- */
static db_state make_test_state(Boolean binary_strings_mode) {
    db_state state = {NULL, NULL, NULL, NULL, 0, {NULL, 0, 0},
                      FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE};
    state.binary_strings = binary_strings_mode;
    return state;
}

/* ---- Helper: encode a param column header for init_param_column ----
 *
 * The Erlang side sends: {UserType, Max, InOrOut, Values}
 * where UserType is an integer, Max is an integer (for sized types),
 * InOrOut is an integer (0=in, 1=out, 2=inout).
 *
 * For init_param_column, the buffer contains:
 *   - tuple header (already consumed by caller)
 *   - user_type (long)
 *   - [max (long)] for sized types
 *   - in_or_out (long)
 */

/* ====================================================================
 * Tests for decode_params: SQL_C_CHAR list path
 * ==================================================================== */

static int test_decode_char_list_normal(void) {
    /* Encode a normal string "hi" for SQL_C_CHAR, Max=5 */
    db_state state = make_test_state(FALSE);
    param_array param;
    param_array *params = &param;

    /* Set up param as CHAR with Max=5: buffer = 6 bytes */
    memset(&param, 0, sizeof(param));
    param.type.c = SQL_C_CHAR;
    param.type.len = 6;  /* Max(5) + 1 null */
    param.type.strlen_or_indptr_array = NULL;
    param.offset = 0;
    param.values.string = (byte *)calloc(1, param.type.len);
    TEST_ASSERT(param.values.string != NULL, "alloc");

    /* Encode "hi" as ERL_STRING_EXT using ei */
    ei_x_buff buf;
    ei_x_new(&buf);
    ei_x_encode_string(&buf, "hi");

    int index = 0;
    Boolean result = decode_params(&state, buf.buff, &index, &params, 0, 0, 1);

    TEST_ASSERT_EQ_INT(TRUE, result, "decode should succeed");
    TEST_ASSERT_EQ_INT('h', param.values.string[0], "first char");
    TEST_ASSERT_EQ_INT('i', param.values.string[1], "second char");
    TEST_ASSERT_EQ_INT('\0', param.values.string[2], "null terminator");
    TEST_ASSERT_EQ_INT(param.type.len, param.offset, "offset advanced by type.len");

    free(param.values.string);
    ei_x_free(&buf);
    return 1;
}

static int test_decode_char_list_max_length(void) {
    /* String exactly at Max length: "hello" with Max=5, buffer=6 */
    db_state state = make_test_state(FALSE);
    param_array param;
    param_array *params = &param;

    memset(&param, 0, sizeof(param));
    param.type.c = SQL_C_CHAR;
    param.type.len = 6;
    param.offset = 0;
    param.values.string = (byte *)calloc(1, param.type.len);
    TEST_ASSERT(param.values.string != NULL, "alloc");

    ei_x_buff buf;
    ei_x_new(&buf);
    ei_x_encode_string(&buf, "hello");

    int index = 0;
    Boolean result = decode_params(&state, buf.buff, &index, &params, 0, 0, 1);

    TEST_ASSERT_EQ_INT(TRUE, result, "decode should succeed for max-length string");
    TEST_ASSERT_EQ_MEM("hello", param.values.string, 5, "string content");
    TEST_ASSERT_EQ_INT('\0', param.values.string[5], "null terminator at end of buffer");

    free(param.values.string);
    ei_x_free(&buf);
    return 1;
}

static int test_decode_char_list_overflow(void) {
    /* String exceeding Max: "toolong" with Max=3, buffer=4 */
    db_state state = make_test_state(FALSE);
    param_array param;
    param_array *params = &param;

    memset(&param, 0, sizeof(param));
    param.type.c = SQL_C_CHAR;
    param.type.len = 4;  /* Max=3 + 1 */
    param.offset = 0;
    param.values.string = (byte *)calloc(1, param.type.len);

    ei_x_buff buf;
    ei_x_new(&buf);
    ei_x_encode_string(&buf, "toolong");

    int index = 0;
    Boolean result = decode_params(&state, buf.buff, &index, &params, 0, 0, 1);

    TEST_ASSERT_EQ_INT(FALSE, result, "decode should reject overflow");

    free(param.values.string);
    ei_x_free(&buf);
    return 1;
}

static int test_decode_char_list_empty(void) {
    /* Empty string "" with Max=0, buffer=1
     * Empty list is encoded as ERL_NIL_EXT by ei */
    db_state state = make_test_state(FALSE);
    param_array param;
    param_array *params = &param;

    memset(&param, 0, sizeof(param));
    param.type.c = SQL_C_CHAR;
    param.type.len = 1;  /* Max=0 + 1 */
    param.offset = 0;
    param.values.string = (byte *)calloc(1, param.type.len);
    param.values.string[0] = 0xFF;  /* sentinel to verify it gets zeroed */

    ei_x_buff buf;
    ei_x_new(&buf);
    ei_x_encode_string(&buf, "");

    int index = 0;
    Boolean result = decode_params(&state, buf.buff, &index, &params, 0, 0, 1);

    TEST_ASSERT_EQ_INT(TRUE, result, "empty string should succeed");
    TEST_ASSERT_EQ_INT('\0', param.values.string[0], "should be null terminator");
    TEST_ASSERT_EQ_INT(param.type.len, param.offset, "offset advanced");

    free(param.values.string);
    ei_x_free(&buf);
    return 1;
}

/* ====================================================================
 * Tests for decode_params: SQL_C_CHAR binary path
 * ==================================================================== */

static int test_decode_char_binary_normal(void) {
    /* Binary <<"hi", 0>> for SQL_C_CHAR in binary_strings mode, Max=5 */
    db_state state = make_test_state(TRUE);
    param_array param;
    param_array *params = &param;

    memset(&param, 0, sizeof(param));
    param.type.c = SQL_C_CHAR;
    param.type.len = 6;  /* Max=5 + 1 */
    param.offset = 0;
    param.values.string = (byte *)calloc(1, param.type.len);

    /* Erlang sends <<"hi", 0:8>> (3 bytes) after string_terminate */
    byte data[] = {'h', 'i', 0};
    ei_x_buff buf;
    ei_x_new(&buf);
    ei_x_encode_binary(&buf, data, 3);

    int index = 0;
    Boolean result = decode_params(&state, buf.buff, &index, &params, 0, 0, 1);

    TEST_ASSERT_EQ_INT(TRUE, result, "decode should succeed");
    TEST_ASSERT_EQ_MEM(data, param.values.string, 3, "binary content");

    free(param.values.string);
    ei_x_free(&buf);
    return 1;
}

static int test_decode_char_binary_max_length(void) {
    /* Binary exactly fitting buffer: 6 bytes with Max=5, buffer=6 */
    db_state state = make_test_state(TRUE);
    param_array param;
    param_array *params = &param;

    memset(&param, 0, sizeof(param));
    param.type.c = SQL_C_CHAR;
    param.type.len = 6;
    param.offset = 0;
    param.values.string = (byte *)calloc(1, param.type.len);

    /* "hello" + 1-byte null = 6 bytes */
    byte data[] = {'h', 'e', 'l', 'l', 'o', 0};
    ei_x_buff buf;
    ei_x_new(&buf);
    ei_x_encode_binary(&buf, data, 6);

    int index = 0;
    Boolean result = decode_params(&state, buf.buff, &index, &params, 0, 0, 1);

    TEST_ASSERT_EQ_INT(TRUE, result, "exact fit should succeed");
    TEST_ASSERT_EQ_MEM(data, param.values.string, 6, "binary content");

    free(param.values.string);
    ei_x_free(&buf);
    return 1;
}

static int test_decode_char_binary_overflow(void) {
    /* Binary too large: 7 bytes into 6-byte buffer */
    db_state state = make_test_state(TRUE);
    param_array param;
    param_array *params = &param;

    memset(&param, 0, sizeof(param));
    param.type.c = SQL_C_CHAR;
    param.type.len = 6;
    param.offset = 0;
    param.values.string = (byte *)calloc(1, param.type.len);

    byte data[] = {'t', 'o', 'o', 'l', 'o', 'n', 'g'};
    ei_x_buff buf;
    ei_x_new(&buf);
    ei_x_encode_binary(&buf, data, 7);

    int index = 0;
    Boolean result = decode_params(&state, buf.buff, &index, &params, 0, 0, 1);

    TEST_ASSERT_EQ_INT(FALSE, result, "overflow should be rejected");

    free(param.values.string);
    ei_x_free(&buf);
    return 1;
}

static int test_decode_char_binary_wrong_type(void) {
    /* Send a string term when binary is expected */
    db_state state = make_test_state(TRUE);
    param_array param;
    param_array *params = &param;

    memset(&param, 0, sizeof(param));
    param.type.c = SQL_C_CHAR;
    param.type.len = 6;
    param.offset = 0;
    param.values.string = (byte *)calloc(1, param.type.len);

    ei_x_buff buf;
    ei_x_new(&buf);
    ei_x_encode_string(&buf, "hi");  /* String, not binary */

    int index = 0;
    Boolean result = decode_params(&state, buf.buff, &index, &params, 0, 0, 1);

    TEST_ASSERT_EQ_INT(FALSE, result, "wrong term type should be rejected");

    free(param.values.string);
    ei_x_free(&buf);
    return 1;
}

/* ====================================================================
 * Tests for decode_params: SQL_C_WCHAR path
 * ==================================================================== */

static int test_decode_wchar_normal(void) {
    /* UTF-16LE "hi" + null = {0x68,0x00, 0x69,0x00, 0x00,0x00} */
    db_state state = make_test_state(FALSE);
    param_array param;
    param_array *params = &param;

    memset(&param, 0, sizeof(param));
    param.type.c = SQL_C_WCHAR;
    /* Max=5, buffer = (5+1)*sizeof(SQLWCHAR) = 12 */
    param.type.len = 6 * sizeof(SQLWCHAR);
    param.offset = 0;
    param.values.string = (byte *)calloc(1, param.type.len);

    /* "hi" in UTF-16LE + 2-byte null = 6 bytes */
    byte data[] = {0x68, 0x00, 0x69, 0x00, 0x00, 0x00};
    ei_x_buff buf;
    ei_x_new(&buf);
    ei_x_encode_binary(&buf, data, 6);

    int index = 0;
    Boolean result = decode_params(&state, buf.buff, &index, &params, 0, 0, 1);

    TEST_ASSERT_EQ_INT(TRUE, result, "decode should succeed");
    TEST_ASSERT_EQ_MEM(data, param.values.string, 6, "UTF-16 content");

    free(param.values.string);
    ei_x_free(&buf);
    return 1;
}

static int test_decode_wchar_empty(void) {
    /* Empty WCHAR: just the 2-byte null terminator. Max=0, buffer=2 */
    db_state state = make_test_state(FALSE);
    param_array param;
    param_array *params = &param;

    memset(&param, 0, sizeof(param));
    param.type.c = SQL_C_WCHAR;
    param.type.len = 1 * sizeof(SQLWCHAR);  /* (0+1)*2 = 2 */
    param.offset = 0;
    param.values.string = (byte *)calloc(1, param.type.len);

    /* Just 2-byte null */
    byte data[] = {0x00, 0x00};
    ei_x_buff buf;
    ei_x_new(&buf);
    ei_x_encode_binary(&buf, data, 2);

    int index = 0;
    Boolean result = decode_params(&state, buf.buff, &index, &params, 0, 0, 1);

    TEST_ASSERT_EQ_INT(TRUE, result, "empty wchar should succeed");
    TEST_ASSERT_EQ_MEM(data, param.values.string, 2, "null terminator");

    free(param.values.string);
    ei_x_free(&buf);
    return 1;
}

static int test_decode_wchar_overflow(void) {
    /* WCHAR data too large for buffer */
    db_state state = make_test_state(FALSE);
    param_array param;
    param_array *params = &param;

    memset(&param, 0, sizeof(param));
    param.type.c = SQL_C_WCHAR;
    param.type.len = 1 * sizeof(SQLWCHAR);  /* Max=0, buffer=2 */
    param.offset = 0;
    param.values.string = (byte *)calloc(1, param.type.len);

    /* "h" in UTF-16LE + null = 4 bytes, but buffer is only 2 */
    byte data[] = {0x68, 0x00, 0x00, 0x00};
    ei_x_buff buf;
    ei_x_new(&buf);
    ei_x_encode_binary(&buf, data, 4);

    int index = 0;
    Boolean result = decode_params(&state, buf.buff, &index, &params, 0, 0, 1);

    TEST_ASSERT_EQ_INT(FALSE, result, "wchar overflow should be rejected");

    free(param.values.string);
    ei_x_free(&buf);
    return 1;
}

static int test_decode_wchar_wrong_type(void) {
    /* Send a string term for WCHAR (should be binary) */
    db_state state = make_test_state(FALSE);
    param_array param;
    param_array *params = &param;

    memset(&param, 0, sizeof(param));
    param.type.c = SQL_C_WCHAR;
    param.type.len = 6 * sizeof(SQLWCHAR);
    param.offset = 0;
    param.values.string = (byte *)calloc(1, param.type.len);

    ei_x_buff buf;
    ei_x_new(&buf);
    ei_x_encode_string(&buf, "hi");

    int index = 0;
    Boolean result = decode_params(&state, buf.buff, &index, &params, 0, 0, 1);

    TEST_ASSERT_EQ_INT(FALSE, result, "string term for wchar should be rejected");

    free(param.values.string);
    ei_x_free(&buf);
    return 1;
}

/* ====================================================================
 * Tests for decode_params: SQL_C_BINARY path
 * ==================================================================== */

static int test_decode_binary_normal(void) {
    db_state state = make_test_state(FALSE);
    param_array param;
    param_array *params = &param;

    memset(&param, 0, sizeof(param));
    param.type.c = SQL_C_BINARY;
    param.type.len = 10;
    param.offset = 0;
    param.values.string = (byte *)calloc(1, param.type.len);

    byte data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    ei_x_buff buf;
    ei_x_new(&buf);
    ei_x_encode_binary(&buf, data, 4);

    int index = 0;
    Boolean result = decode_params(&state, buf.buff, &index, &params, 0, 0, 1);

    TEST_ASSERT_EQ_INT(TRUE, result, "binary decode should succeed");
    TEST_ASSERT_EQ_MEM(data, param.values.string, 4, "binary content");

    free(param.values.string);
    ei_x_free(&buf);
    return 1;
}

static int test_decode_binary_overflow(void) {
    db_state state = make_test_state(FALSE);
    param_array param;
    param_array *params = &param;

    memset(&param, 0, sizeof(param));
    param.type.c = SQL_C_BINARY;
    param.type.len = 3;
    param.offset = 0;
    param.values.string = (byte *)calloc(1, param.type.len);

    byte data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    ei_x_buff buf;
    ei_x_new(&buf);
    ei_x_encode_binary(&buf, data, 4);

    int index = 0;
    Boolean result = decode_params(&state, buf.buff, &index, &params, 0, 0, 1);

    TEST_ASSERT_EQ_INT(FALSE, result, "binary overflow should be rejected");

    free(param.values.string);
    ei_x_free(&buf);
    return 1;
}

/* ====================================================================
 * Tests for decode_params: null values
 * ==================================================================== */

static int test_decode_null_char(void) {
    db_state state = make_test_state(FALSE);
    param_array param;
    param_array *params = &param;

    memset(&param, 0, sizeof(param));
    param.type.c = SQL_C_CHAR;
    param.type.len = 6;
    param.offset = 0;
    param.type.strlen_or_indptr_array = NULL;
    param.values.string = (byte *)calloc(1, param.type.len);

    ei_x_buff buf;
    ei_x_new(&buf);
    ei_x_encode_atom(&buf, "null");

    int index = 0;
    Boolean result = decode_params(&state, buf.buff, &index, &params, 0, 0, 1);

    TEST_ASSERT_EQ_INT(TRUE, result, "null should succeed");
    TEST_ASSERT_EQ_INT(param.type.len, param.offset, "offset advanced for null");
    TEST_ASSERT(param.type.strlen_or_indptr_array != NULL, "indptr array allocated");
    TEST_ASSERT_EQ_INT(SQL_NULL_DATA, param.type.strlen_or_indptr_array[0],
                        "null indicator set");

    free(param.type.strlen_or_indptr_array);
    free(param.values.string);
    ei_x_free(&buf);
    return 1;
}

static int test_decode_null_wchar(void) {
    db_state state = make_test_state(FALSE);
    param_array param;
    param_array *params = &param;

    memset(&param, 0, sizeof(param));
    param.type.c = SQL_C_WCHAR;
    param.type.len = 6 * sizeof(SQLWCHAR);
    param.offset = 0;
    param.type.strlen_or_indptr_array = NULL;
    param.values.string = (byte *)calloc(1, param.type.len);

    ei_x_buff buf;
    ei_x_new(&buf);
    ei_x_encode_atom(&buf, "null");

    int index = 0;
    Boolean result = decode_params(&state, buf.buff, &index, &params, 0, 0, 1);

    TEST_ASSERT_EQ_INT(TRUE, result, "null wchar should succeed");
    TEST_ASSERT_EQ_INT(SQL_NULL_DATA, param.type.strlen_or_indptr_array[0],
                        "null indicator set");

    free(param.type.strlen_or_indptr_array);
    free(param.values.string);
    ei_x_free(&buf);
    return 1;
}

/* ====================================================================
 * Tests for decode_params: multiple values
 * ==================================================================== */

static int test_decode_char_multiple_values(void) {
    /* Decode two values into sequential slots */
    db_state state = make_test_state(FALSE);
    param_array param;
    param_array *params = &param;

    memset(&param, 0, sizeof(param));
    param.type.c = SQL_C_CHAR;
    param.type.len = 4;  /* Max=3 + 1 */
    param.type.strlen_or_indptr_array = (SQLLEN *)calloc(2, sizeof(SQLLEN));
    param.offset = 0;
    param.values.string = (byte *)calloc(2, param.type.len);

    /* First value: "ab" */
    ei_x_buff buf1;
    ei_x_new(&buf1);
    ei_x_encode_string(&buf1, "ab");
    int index1 = 0;
    Boolean r1 = decode_params(&state, buf1.buff, &index1, &params, 0, 0, 2);
    TEST_ASSERT_EQ_INT(TRUE, r1, "first value decode");
    TEST_ASSERT_EQ_INT(param.type.len, param.offset, "offset after first");

    /* Second value: "xy" */
    ei_x_buff buf2;
    ei_x_new(&buf2);
    ei_x_encode_string(&buf2, "xy");
    int index2 = 0;
    Boolean r2 = decode_params(&state, buf2.buff, &index2, &params, 0, 1, 2);
    TEST_ASSERT_EQ_INT(TRUE, r2, "second value decode");
    TEST_ASSERT_EQ_INT(2 * param.type.len, param.offset, "offset after second");

    /* Verify buffer contents */
    TEST_ASSERT_EQ_MEM("ab", &param.values.string[0], 2, "first value content");
    TEST_ASSERT_EQ_INT('\0', param.values.string[2], "first null terminator");
    TEST_ASSERT_EQ_MEM("xy", &param.values.string[4], 2, "second value content");
    TEST_ASSERT_EQ_INT('\0', param.values.string[6], "second null terminator");

    free(param.type.strlen_or_indptr_array);
    free(param.values.string);
    ei_x_free(&buf1);
    ei_x_free(&buf2);
    return 1;
}

/* ====================================================================
 * Tests for init_param_column: buffer allocation sizes
 * ==================================================================== */

static int test_init_param_char_buffer_size(void) {
    /* USER_CHAR with Max=5 should allocate buffer of 6 bytes */
    db_state state = make_test_state(FALSE);
    param_array param;
    memset(&param, 0, sizeof(param));

    /* Encode: {USER_CHAR, 5, ERL_ODBC_IN} */
    ei_x_buff buf;
    ei_x_new(&buf);
    ei_x_encode_long(&buf, USER_CHAR);
    ei_x_encode_long(&buf, 5);  /* Max */
    ei_x_encode_long(&buf, ERL_ODBC_IN);

    int index = 0;
    init_param_column(&param, buf.buff, &index, 1, &state);

    TEST_ASSERT_EQ_INT(SQL_C_CHAR, param.type.c, "C type");
    TEST_ASSERT_EQ_INT(SQL_CHAR, param.type.sql, "SQL type");
    TEST_ASSERT_EQ_INT(6, param.type.len, "buffer len = Max+1");
    TEST_ASSERT_EQ_INT(5, param.type.col_size, "col_size = Max");
    TEST_ASSERT(param.values.string != NULL, "buffer allocated");

    free(param.values.string);
    free(param.type.strlen_or_indptr_array);
    ei_x_free(&buf);
    return 1;
}

static int test_init_param_char_zero_size(void) {
    /* USER_CHAR with Max=0: buffer should be 1 byte (null terminator only) */
    db_state state = make_test_state(FALSE);
    param_array param;
    memset(&param, 0, sizeof(param));

    ei_x_buff buf;
    ei_x_new(&buf);
    ei_x_encode_long(&buf, USER_CHAR);
    ei_x_encode_long(&buf, 0);
    ei_x_encode_long(&buf, ERL_ODBC_IN);

    int index = 0;
    init_param_column(&param, buf.buff, &index, 1, &state);

    TEST_ASSERT_EQ_INT(1, param.type.len, "buffer len = 0+1 = 1");
    TEST_ASSERT_EQ_INT(1, param.type.col_size, "col_size = max(0,1) = 1");

    free(param.values.string);
    free(param.type.strlen_or_indptr_array);
    ei_x_free(&buf);
    return 1;
}

static int test_init_param_wchar_buffer_size(void) {
    /* USER_WCHAR with Max=5: buffer = (5+1)*sizeof(SQLWCHAR) = 12 */
    db_state state = make_test_state(FALSE);
    param_array param;
    memset(&param, 0, sizeof(param));

    ei_x_buff buf;
    ei_x_new(&buf);
    ei_x_encode_long(&buf, USER_WCHAR);
    ei_x_encode_long(&buf, 5);
    ei_x_encode_long(&buf, ERL_ODBC_IN);

    int index = 0;
    init_param_column(&param, buf.buff, &index, 1, &state);

    TEST_ASSERT_EQ_INT(SQL_C_WCHAR, param.type.c, "C type");
    TEST_ASSERT_EQ_INT(SQL_WCHAR, param.type.sql, "SQL type");
    TEST_ASSERT_EQ_INT(6 * (int)sizeof(SQLWCHAR), param.type.len,
                        "buffer len = (Max+1)*sizeof(SQLWCHAR)");
    TEST_ASSERT_EQ_INT(5, param.type.col_size, "col_size = Max");

    free(param.values.string);
    free(param.type.strlen_or_indptr_array);
    ei_x_free(&buf);
    return 1;
}

static int test_init_param_wlongvarchar_buffer_size(void) {
    /* USER_WLONGVARCHAR with Max=5: buffer = 6*sizeof(SQLWCHAR) = 12 */
    db_state state = make_test_state(FALSE);
    param_array param;
    memset(&param, 0, sizeof(param));

    ei_x_buff buf;
    ei_x_new(&buf);
    ei_x_encode_long(&buf, USER_WLONGVARCHAR);
    ei_x_encode_long(&buf, 5);
    ei_x_encode_long(&buf, ERL_ODBC_IN);

    int index = 0;
    init_param_column(&param, buf.buff, &index, 1, &state);

    TEST_ASSERT_EQ_INT(SQL_C_WCHAR, param.type.c, "C type");
    TEST_ASSERT_EQ_INT(SQL_WLONGVARCHAR, param.type.sql, "SQL type");
    TEST_ASSERT_EQ_INT(6 * (int)sizeof(SQLWCHAR), param.type.len,
                        "buffer len = (5+1)*sizeof(SQLWCHAR)");
    TEST_ASSERT_EQ_INT(5, param.type.col_size, "col_size = 5");

    free(param.values.string);
    free(param.type.strlen_or_indptr_array);
    ei_x_free(&buf);
    return 1;
}

static int test_init_param_longvarchar_buffer_size(void) {
    /* USER_LONGVARCHAR with Max=100: buffer = 101 */
    db_state state = make_test_state(FALSE);
    param_array param;
    memset(&param, 0, sizeof(param));

    ei_x_buff buf;
    ei_x_new(&buf);
    ei_x_encode_long(&buf, USER_LONGVARCHAR);
    ei_x_encode_long(&buf, 100);
    ei_x_encode_long(&buf, ERL_ODBC_IN);

    int index = 0;
    init_param_column(&param, buf.buff, &index, 1, &state);

    TEST_ASSERT_EQ_INT(SQL_C_CHAR, param.type.c, "C type");
    TEST_ASSERT_EQ_INT(SQL_LONGVARCHAR, param.type.sql, "SQL type");
    TEST_ASSERT_EQ_INT(101, param.type.len, "buffer len = Max+1");
    TEST_ASSERT_EQ_INT(100, param.type.col_size, "col_size = Max");

    free(param.values.string);
    free(param.type.strlen_or_indptr_array);
    ei_x_free(&buf);
    return 1;
}

static int test_init_param_longvarchar_zero_size(void) {
    /* USER_LONGVARCHAR with Max=0: buffer = 1, col_size = 1 (clamped) */
    db_state state = make_test_state(FALSE);
    param_array param;
    memset(&param, 0, sizeof(param));

    ei_x_buff buf;
    ei_x_new(&buf);
    ei_x_encode_long(&buf, USER_LONGVARCHAR);
    ei_x_encode_long(&buf, 0);
    ei_x_encode_long(&buf, ERL_ODBC_IN);

    int index = 0;
    init_param_column(&param, buf.buff, &index, 1, &state);

    TEST_ASSERT_EQ_INT(SQL_C_CHAR, param.type.c, "C type");
    TEST_ASSERT_EQ_INT(SQL_LONGVARCHAR, param.type.sql, "SQL type");
    TEST_ASSERT_EQ_INT(1, param.type.len, "buffer len = 0+1 = 1");
    TEST_ASSERT_EQ_INT(1, param.type.col_size, "col_size clamped to 1");

    free(param.values.string);
    free(param.type.strlen_or_indptr_array);
    ei_x_free(&buf);
    return 1;
}

static int test_init_param_wlongvarchar_zero_size(void) {
    /* USER_WLONGVARCHAR with Max=0: buffer = 2, col_size = 1 (clamped) */
    db_state state = make_test_state(FALSE);
    param_array param;
    memset(&param, 0, sizeof(param));

    ei_x_buff buf;
    ei_x_new(&buf);
    ei_x_encode_long(&buf, USER_WLONGVARCHAR);
    ei_x_encode_long(&buf, 0);
    ei_x_encode_long(&buf, ERL_ODBC_IN);

    int index = 0;
    init_param_column(&param, buf.buff, &index, 1, &state);

    TEST_ASSERT_EQ_INT(SQL_C_WCHAR, param.type.c, "C type");
    TEST_ASSERT_EQ_INT(SQL_WLONGVARCHAR, param.type.sql, "SQL type");
    TEST_ASSERT_EQ_INT((int)sizeof(SQLWCHAR), param.type.len,
                        "buffer len = 1*sizeof(SQLWCHAR)");
    TEST_ASSERT_EQ_INT(1, param.type.col_size, "col_size clamped to 1");

    free(param.values.string);
    free(param.type.strlen_or_indptr_array);
    ei_x_free(&buf);
    return 1;
}

/* ====================================================================
 * End-to-end: init + decode combined
 * ==================================================================== */

static int test_init_then_decode_char_exact_fit(void) {
    /* Allocate via init_param_column, then decode a max-length value */
    db_state state = make_test_state(FALSE);
    param_array param;
    param_array *params = &param;
    memset(&param, 0, sizeof(param));

    /* Init with USER_VARCHAR, Max=3 */
    ei_x_buff init_buf;
    ei_x_new(&init_buf);
    ei_x_encode_long(&init_buf, USER_VARCHAR);
    ei_x_encode_long(&init_buf, 3);
    ei_x_encode_long(&init_buf, ERL_ODBC_IN);

    int init_index = 0;
    init_param_column(&param, init_buf.buff, &init_index, 1, &state);

    TEST_ASSERT_EQ_INT(4, param.type.len, "buffer = Max+1 = 4");

    /* Now decode "abc" (exactly Max=3 chars, ei_decode_string writes 4 bytes) */
    ei_x_buff val_buf;
    ei_x_new(&val_buf);
    ei_x_encode_string(&val_buf, "abc");

    int val_index = 0;
    Boolean result = decode_params(&state, val_buf.buff, &val_index, &params, 0, 0, 1);

    TEST_ASSERT_EQ_INT(TRUE, result, "decode should succeed");
    TEST_ASSERT_EQ_MEM("abc", param.values.string, 3, "content");
    TEST_ASSERT_EQ_INT('\0', param.values.string[3], "null terminator");

    free(param.values.string);
    free(param.type.strlen_or_indptr_array);
    ei_x_free(&init_buf);
    ei_x_free(&val_buf);
    return 1;
}

static int test_init_then_decode_wchar_exact_fit(void) {
    /* Allocate via init_param_column for WCHAR, then decode max-length UTF-16 */
    db_state state = make_test_state(FALSE);
    param_array param;
    param_array *params = &param;
    memset(&param, 0, sizeof(param));

    /* Init with USER_WVARCHAR, Max=2 */
    ei_x_buff init_buf;
    ei_x_new(&init_buf);
    ei_x_encode_long(&init_buf, USER_WVARCHAR);
    ei_x_encode_long(&init_buf, 2);
    ei_x_encode_long(&init_buf, ERL_ODBC_IN);

    int init_index = 0;
    init_param_column(&param, init_buf.buff, &init_index, 1, &state);

    /* buffer = (2+1)*2 = 6 bytes */
    TEST_ASSERT_EQ_INT(6, param.type.len, "buffer = (Max+1)*sizeof(SQLWCHAR)");

    /* "hi" in UTF-16LE + 2-byte null = 6 bytes (from wstring_terminate) */
    byte data[] = {0x68, 0x00, 0x69, 0x00, 0x00, 0x00};
    ei_x_buff val_buf;
    ei_x_new(&val_buf);
    ei_x_encode_binary(&val_buf, data, 6);

    int val_index = 0;
    Boolean result = decode_params(&state, val_buf.buff, &val_index, &params, 0, 0, 1);

    TEST_ASSERT_EQ_INT(TRUE, result, "decode should succeed");
    TEST_ASSERT_EQ_MEM(data, param.values.string, 6, "UTF-16 content with null");

    free(param.values.string);
    free(param.type.strlen_or_indptr_array);
    ei_x_free(&init_buf);
    ei_x_free(&val_buf);
    return 1;
}

/* ====================================================================
 * End-to-end: {sql_longvarchar, 0} with "" and {sql_wlongvarchar, 0}
 * ==================================================================== */

static int test_init_then_decode_longvarchar_zero_empty(void) {
    /* Exact repro of: param_query(Ref, Q, [{{sql_longvarchar, 0}, [""]}]) */
    db_state state = make_test_state(FALSE);
    param_array param;
    param_array *params = &param;
    memset(&param, 0, sizeof(param));

    ei_x_buff init_buf;
    ei_x_new(&init_buf);
    ei_x_encode_long(&init_buf, USER_LONGVARCHAR);
    ei_x_encode_long(&init_buf, 0);
    ei_x_encode_long(&init_buf, ERL_ODBC_IN);

    int init_index = 0;
    init_param_column(&param, init_buf.buff, &init_index, 1, &state);

    TEST_ASSERT_EQ_INT(SQL_C_CHAR, param.type.c, "C type");
    TEST_ASSERT_EQ_INT(SQL_LONGVARCHAR, param.type.sql, "SQL type");
    TEST_ASSERT_EQ_INT(1, param.type.len, "buffer = 1");
    TEST_ASSERT_EQ_INT(1, param.type.col_size, "col_size clamped to 1");

    /* Decode empty string "" — encoded as ERL_NIL_EXT */
    ei_x_buff val_buf;
    ei_x_new(&val_buf);
    ei_x_encode_empty_list(&val_buf);

    int val_index = 0;
    Boolean result = decode_params(&state, val_buf.buff, &val_index, &params, 0, 0, 1);

    TEST_ASSERT_EQ_INT(TRUE, result, "decode empty string should succeed");
    TEST_ASSERT_EQ_INT('\0', param.values.string[0], "null terminator at offset 0");

    free(param.values.string);
    free(param.type.strlen_or_indptr_array);
    ei_x_free(&init_buf);
    ei_x_free(&val_buf);
    return 1;
}

static int test_init_then_decode_wlongvarchar_zero_empty(void) {
    /* Exact repro of: param_query(Ref, Q, [{{sql_wlongvarchar, 0}, [<<>>]}]) */
    db_state state = make_test_state(FALSE);
    param_array param;
    param_array *params = &param;
    memset(&param, 0, sizeof(param));

    ei_x_buff init_buf;
    ei_x_new(&init_buf);
    ei_x_encode_long(&init_buf, USER_WLONGVARCHAR);
    ei_x_encode_long(&init_buf, 0);
    ei_x_encode_long(&init_buf, ERL_ODBC_IN);

    int init_index = 0;
    init_param_column(&param, init_buf.buff, &init_index, 1, &state);

    TEST_ASSERT_EQ_INT(SQL_C_WCHAR, param.type.c, "C type");
    TEST_ASSERT_EQ_INT(SQL_WLONGVARCHAR, param.type.sql, "SQL type");
    TEST_ASSERT_EQ_INT((int)sizeof(SQLWCHAR), param.type.len, "buffer = sizeof(SQLWCHAR)");
    TEST_ASSERT_EQ_INT(1, param.type.col_size, "col_size clamped to 1");

    /* Decode empty WCHAR binary: just the 2-byte null terminator (from wstring_terminate) */
    byte wchar_null[] = {0x00, 0x00};
    ei_x_buff val_buf;
    ei_x_new(&val_buf);
    ei_x_encode_binary(&val_buf, wchar_null, sizeof(SQLWCHAR));

    int val_index = 0;
    Boolean result = decode_params(&state, val_buf.buff, &val_index, &params, 0, 0, 1);

    TEST_ASSERT_EQ_INT(TRUE, result, "decode empty wchar should succeed");
    TEST_ASSERT_EQ_INT(0, param.values.string[0], "null byte 1");
    TEST_ASSERT_EQ_INT(0, param.values.string[1], "null byte 2");

    free(param.values.string);
    free(param.type.strlen_or_indptr_array);
    ei_x_free(&init_buf);
    ei_x_free(&val_buf);
    return 1;
}

/* ====================================================================
 * Main
 * ==================================================================== */

int main(void) {
    printf("odbcserver decode_params & buffer allocation tests\n");
    printf("==================================================\n\n");

    printf("SQL_C_CHAR list path:\n");
    RUN_TEST(test_decode_char_list_normal);
    RUN_TEST(test_decode_char_list_max_length);
    RUN_TEST(test_decode_char_list_overflow);
    RUN_TEST(test_decode_char_list_empty);

    printf("\nSQL_C_CHAR binary path:\n");
    RUN_TEST(test_decode_char_binary_normal);
    RUN_TEST(test_decode_char_binary_max_length);
    RUN_TEST(test_decode_char_binary_overflow);
    RUN_TEST(test_decode_char_binary_wrong_type);

    printf("\nSQL_C_WCHAR path:\n");
    RUN_TEST(test_decode_wchar_normal);
    RUN_TEST(test_decode_wchar_empty);
    RUN_TEST(test_decode_wchar_overflow);
    RUN_TEST(test_decode_wchar_wrong_type);

    printf("\nSQL_C_BINARY path:\n");
    RUN_TEST(test_decode_binary_normal);
    RUN_TEST(test_decode_binary_overflow);

    printf("\nNull values:\n");
    RUN_TEST(test_decode_null_char);
    RUN_TEST(test_decode_null_wchar);

    printf("\nMultiple values:\n");
    RUN_TEST(test_decode_char_multiple_values);

    printf("\ninit_param_column buffer sizes:\n");
    RUN_TEST(test_init_param_char_buffer_size);
    RUN_TEST(test_init_param_char_zero_size);
    RUN_TEST(test_init_param_wchar_buffer_size);
    RUN_TEST(test_init_param_wlongvarchar_buffer_size);
    RUN_TEST(test_init_param_longvarchar_buffer_size);
    RUN_TEST(test_init_param_longvarchar_zero_size);
    RUN_TEST(test_init_param_wlongvarchar_zero_size);

    printf("\nEnd-to-end: init + decode:\n");
    RUN_TEST(test_init_then_decode_char_exact_fit);
    RUN_TEST(test_init_then_decode_wchar_exact_fit);
    RUN_TEST(test_init_then_decode_longvarchar_zero_empty);
    RUN_TEST(test_init_then_decode_wlongvarchar_zero_empty);

    TEST_SUMMARY();
}
