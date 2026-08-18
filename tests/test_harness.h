#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_tests_run;
static int g_tests_failed;

#define TEST(name) static void name(void)

#define ASSERT(cond)                                                           \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, "  FAIL %s:%d: assertion failed: %s\n", __FILE__,  \
                    __LINE__, #cond);                                          \
            g_tests_failed++;                                                  \
            return;                                                            \
        }                                                                      \
    } while (0)

#define ASSERT_EQ_INT(a, b)                                                    \
    do {                                                                       \
        const long _a = (long)(a);                                             \
        const long _b = (long)(b);                                             \
        if (_a != _b) {                                                        \
            fprintf(stderr, "  FAIL %s:%d: expected %ld got %ld\n", __FILE__, \
                    __LINE__, _b, _a);                                         \
            g_tests_failed++;                                                  \
            return;                                                            \
        }                                                                      \
    } while (0)

#define ASSERT_STR_EQ(a, b)                                                    \
    do {                                                                       \
        const char *_a = (a);                                                  \
        const char *_b = (b);                                                  \
        if (_a == NULL || _b == NULL || strcmp(_a, _b) != 0) {                 \
            fprintf(stderr, "  FAIL %s:%d: expected \"%s\" got \"%s\"\n",    \
                    __FILE__, __LINE__, _b != NULL ? _b : "(null)",            \
                    _a != NULL ? _a : "(null)");                               \
            g_tests_failed++;                                                  \
            return;                                                            \
        }                                                                      \
    } while (0)

#define RUN(name)                                                              \
    do {                                                                       \
        const int failed_before = g_tests_failed;                              \
        g_tests_run++;                                                         \
        printf("  %s ... ", #name);                                            \
        fflush(stdout);                                                        \
        name();                                                                \
        if (g_tests_failed == failed_before) {                                 \
            printf("ok\n");                                                    \
        } else {                                                               \
            printf("FAIL\n");                                                  \
        }                                                                      \
    } while (0)

static int test_harness_summary(void)
{
    printf("\n%d test(s) run", g_tests_run);
    if (g_tests_failed != 0) {
        printf(", %d failed\n", g_tests_failed);
        return 1;
    }
    printf(", all passed\n");
    return 0;
}

#endif /* TEST_HARNESS_H */
