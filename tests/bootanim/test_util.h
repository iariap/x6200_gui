/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 *  Banco de pruebas minimo de los tests de host del arranque (issue #192): sin
 *  dependencias, sin frameworks. Cuenta comprobaciones y fallos y sale != 0 si
 *  algo falla, con el archivo:linea de cada fallo.
 */
#ifndef X6200_BOOTANIM_TEST_UTIL_H
#define X6200_BOOTANIM_TEST_UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int ba_test_checks = 0;
static int ba_test_fails  = 0;

#define BA_CHECK(cond)                                                                      \
    do {                                                                                    \
        ba_test_checks++;                                                                   \
        if (!(cond)) {                                                                      \
            ba_test_fails++;                                                                \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                          \
        }                                                                                   \
    } while (0)

#define BA_CHECK_MSG(cond, ...)                                                             \
    do {                                                                                    \
        ba_test_checks++;                                                                   \
        if (!(cond)) {                                                                      \
            ba_test_fails++;                                                                \
            printf("FAIL %s:%d: %s (", __FILE__, __LINE__, #cond);                          \
            printf(__VA_ARGS__);                                                            \
            printf(")\n");                                                                  \
        }                                                                                   \
    } while (0)

#define BA_CHECK_EQ_INT(actual, expected)                                                   \
    do {                                                                                    \
        long _a = (long) (actual);                                                          \
        long _e = (long) (expected);                                                        \
        ba_test_checks++;                                                                   \
        if (_a != _e) {                                                                     \
            ba_test_fails++;                                                                \
            printf("FAIL %s:%d: %s == %ld, se esperaba %ld\n", __FILE__, __LINE__, #actual,  \
                   (long) _a, (long) _e);                                                   \
        }                                                                                   \
    } while (0)

#define BA_CHECK_STREQ(actual, expected)                                                    \
    do {                                                                                    \
        const char *_a = (actual);                                                          \
        const char *_e = (expected);                                                        \
        ba_test_checks++;                                                                   \
        if (_a == NULL || _e == NULL || strcmp(_a, _e) != 0) {                              \
            ba_test_fails++;                                                                \
            printf("FAIL %s:%d: \"%s\" != \"%s\"\n", __FILE__, __LINE__,                    \
                   _a == NULL ? "(null)" : _a, _e == NULL ? "(null)" : _e);                 \
        }                                                                                   \
    } while (0)

#define BA_TEST_SUMMARY(name)                                                               \
    do {                                                                                    \
        printf("%s: %d comprobaciones, %d fallos\n", (name), ba_test_checks, ba_test_fails); \
        return ba_test_fails == 0 ? 0 : 1;                                                  \
    } while (0)

#endif /* X6200_BOOTANIM_TEST_UTIL_H */
