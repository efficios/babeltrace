/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2016 Philippe Proulx <pproulx@efficios.com>
 *
 * Define PRINT_PREFIX and PRINT_ERR_STREAM, then include this file.
 */

#ifndef CTF_BTR_PRINT_H
#define CTF_BTR_PRINT_H

#include <stdio.h>

#define PERR(fmt, ...)                                                                             \
    do {                                                                                           \
        if (PRINT_ERR_STREAM) {                                                                    \
            fprintf(PRINT_ERR_STREAM, "Error: " PRINT_PREFIX ": " fmt, ##__VA_ARGS__);             \
        }                                                                                          \
    } while (0)

#define PWARN(fmt, ...)                                                                            \
    do {                                                                                           \
        if (PRINT_ERR_STREAM) {                                                                    \
            fprintf(PRINT_ERR_STREAM, "Warning: " PRINT_PREFIX ": " fmt, ##__VA_ARGS__);           \
        }                                                                                          \
    } while (0)

#define PDBG(fmt, ...)                                                                             \
    do {                                                                                           \
        if (babeltrace_debug) {                                                                    \
            fprintf(stderr, "Debug: " PRINT_PREFIX ": " fmt, ##__VA_ARGS__);                       \
        }                                                                                          \
    } while (0)

#endif /* CTF_BTR_PRINT_H */
