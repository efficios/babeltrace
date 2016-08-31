#ifndef CTF_FS_PRINT_H
#define CTF_FS_PRINT_H

/*
 * Define PRINT_PREFIX and PRINT_ERR_STREAM, then include this file.
 *
 * Copyright (c) 2016 Philippe Proulx <pproulx@efficios.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>

#define PERR(fmt, ...)							\
	do {								\
		if (PRINT_ERR_STREAM) {					\
			fprintf(PRINT_ERR_STREAM,			\
				"Error: " PRINT_PREFIX ": " fmt,	\
				##__VA_ARGS__);				\
		}							\
	} while (0)

#define PWARN(fmt, ...)							\
	do {								\
		if (PRINT_ERR_STREAM) {					\
			fprintf(PRINT_ERR_STREAM,			\
				"Warning: " PRINT_PREFIX ": " fmt,	\
				##__VA_ARGS__);				\
		}							\
	} while (0)

#define PDBG(fmt, ...)							\
	do { 								\
		if (ctf_fs_debug) {					\
			fprintf(stderr,					\
				"Debug: " PRINT_PREFIX ": " fmt,	\
				##__VA_ARGS__);				\
		}							\
	} while (0)

#endif /* CTF_FS_PRINT_H */
