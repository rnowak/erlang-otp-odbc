/*
 * Minimal test macros for the odbcserver test harness.
 * No external dependencies — just assert + reporting.
 */

#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg)                                         \
    do {                                                               \
        if (!(cond)) {                                                 \
            fprintf(stderr, "  FAIL: %s (line %d): %s\n",             \
                    __func__, __LINE__, msg);                          \
            return 0;                                                  \
        }                                                              \
    } while (0)

#define TEST_ASSERT_EQ_INT(expected, actual, msg)                      \
    do {                                                               \
        long _exp = (long)(expected);                                  \
        long _act = (long)(actual);                                    \
        if (_exp != _act) {                                            \
            fprintf(stderr, "  FAIL: %s (line %d): %s"                \
                    " (expected %ld, got %ld)\n",                      \
                    __func__, __LINE__, msg, _exp, _act);              \
            return 0;                                                  \
        }                                                              \
    } while (0)

#define TEST_ASSERT_EQ_MEM(expected, actual, len, msg)                 \
    do {                                                               \
        if (memcmp((expected), (actual), (len)) != 0) {                \
            fprintf(stderr, "  FAIL: %s (line %d): %s\n",             \
                    __func__, __LINE__, msg);                          \
            return 0;                                                  \
        }                                                              \
    } while (0)

#define RUN_TEST(test_func)                                            \
    do {                                                               \
        tests_run++;                                                   \
        printf("  %-60s ", #test_func);                                \
        if (test_func()) {                                             \
            tests_passed++;                                            \
            printf("PASS\n");                                          \
        } else {                                                       \
            tests_failed++;                                            \
            printf("FAIL\n");                                          \
        }                                                              \
    } while (0)

#define TEST_SUMMARY()                                                 \
    do {                                                               \
        printf("\n%d tests: %d passed, %d failed\n",                   \
               tests_run, tests_passed, tests_failed);                 \
        return tests_failed > 0 ? 1 : 0;                              \
    } while (0)

#endif /* TEST_HARNESS_H */
