#ifndef CHECK_H
#define CHECK_H

#include <stdio.h>

static int checks_run = 0, checks_failed = 0;

#define CHECK(cond) do {                                              \
    checks_run++;                                                     \
    if (!(cond)) {                                                    \
        checks_failed++;                                              \
        printf("  FAIL line %d: %s\n", __LINE__, #cond);              \
    }                                                                 \
} while (0)

/* Call at the end of main(): prints a summary, returns exit code. */
static inline int check_report(const char *suite)
{
    printf("%s: %d/%d checks passed\n", suite,
           checks_run - checks_failed, checks_run);
    return checks_failed ? 1 : 0;
}

/* Deterministic known data: byte i of a stream. */
static inline unsigned char pattern(long i) { return (unsigned char)((i * 31 + 7) % 251); }

#endif
