#pragma once

#include <stdio.h>
#include <string.h>

typedef void (*test_fn_t)(void);
typedef struct { const char *suite; const char *name; test_fn_t fn; } test_entry_t;

#define MAX_TESTS 512
extern test_entry_t  g_tests[MAX_TESTS];
extern int           g_test_count;
extern int           g_pass, g_fail;
extern const char   *g_cur_suite, *g_cur_test;
extern int           g_cur_failed;

#define TEST(suite, name)                                               \
    static void test_##suite##_##name(void);                            \
    __attribute__((constructor)) static void reg_##suite##_##name(void) { \
        g_tests[g_test_count++] = (test_entry_t){#suite, #name, test_##suite##_##name}; \
    }                                                                   \
    static void test_##suite##_##name(void)

#define _FAIL(fmt, ...) do {                                        \
    if (!g_cur_failed) {                                            \
        printf("  FAIL: %s::%s\n", g_cur_suite, g_cur_test);       \
        g_cur_failed = 1;                                           \
    }                                                               \
    printf("    line %d: " fmt "\n", __LINE__, ##__VA_ARGS__);     \
} while (0)

#define EXPECT_EQ(a, b) do { int _a=(int)(a), _b=(int)(b); \
    if (_a != _b) _FAIL("EXPECT_EQ(%s, %s)  got %d != %d", #a, #b, _a, _b); } while(0)

#define EXPECT_NE(a, b) do { int _a=(int)(a), _b=(int)(b); \
    if (_a == _b) _FAIL("EXPECT_NE(%s, %s)  both == %d", #a, #b, _a); } while(0)

#define EXPECT_GT(a, b) do { int _a=(int)(a), _b=(int)(b); \
    if (_a <= _b) _FAIL("EXPECT_GT(%s, %s)  %d <= %d", #a, #b, _a, _b); } while(0)

#define EXPECT_GE(a, b) do { int _a=(int)(a), _b=(int)(b); \
    if (_a < _b) _FAIL("EXPECT_GE(%s, %s)  %d < %d", #a, #b, _a, _b); } while(0)

#define EXPECT_LT(a, b) do { int _a=(int)(a), _b=(int)(b); \
    if (_a >= _b) _FAIL("EXPECT_LT(%s, %s)  %d >= %d", #a, #b, _a, _b); } while(0)

#define EXPECT_TRUE(expr) do { \
    if (!(expr)) _FAIL("EXPECT_TRUE(%s)  was false", #expr); } while(0)

#define EXPECT_FALSE(expr) do { \
    if ((expr)) _FAIL("EXPECT_FALSE(%s)  was true", #expr); } while(0)

#define RUN_ALL_TESTS() do {                                        \
    printf("=== pokered test suite (%d tests) ===\n", g_test_count); \
    const char *last_suite = NULL;                                  \
    for (int _i = 0; _i < g_test_count; _i++) {                   \
        test_entry_t *_t = &g_tests[_i];                           \
        if (!last_suite || strcmp(last_suite, _t->suite) != 0) {   \
            printf("\n[%s]\n", _t->suite);                          \
            last_suite = _t->suite;                                 \
        }                                                           \
        g_cur_suite  = _t->suite;                                   \
        g_cur_test   = _t->name;                                    \
        g_cur_failed = 0;                                           \
        _t->fn();                                                   \
        if (g_cur_failed) g_fail++;                                 \
        else { printf("  PASS: %s\n", _t->name); g_pass++; }       \
    }                                                               \
    printf("\n=== %d passed, %d failed ===\n", g_pass, g_fail);    \
    return g_fail ? 1 : 0;                                         \
} while (0)
