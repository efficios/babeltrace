#ifndef _BABELTRACE_UUID_H
#define _BABELTRACE_UUID_H

/*
 * babeltrace/uuid.h
 *
 * Copyright (C) 2011   Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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

#include <config.h>

/* Includes final \0. */
#define BABELTRACE_UUID_STR_LEN		37
#define BABELTRACE_UUID_LEN		16

#ifdef BABELTRACE_HAVE_LIBUUID
#include <uuid/uuid.h>

static inline
int babeltrace_uuid_generate(unsigned char *uuid_out)
{
	uuid_generate(uuid_out);
	return 0;
}

static inline
int babeltrace_uuid_unparse(const unsigned char *uuid_in, char *str_out)
{
	uuid_unparse(uuid_in, str_out);
	return 0;
}

static inline
int babeltrace_uuid_parse(const char *str_in, unsigned char *uuid_out)
{
	return uuid_parse(str_in, uuid_out);
}

static inline
int babeltrace_uuid_compare(const unsigned char *uuid_a,
		const unsigned char *uuid_b)
{
	return uuid_compare(uuid_a, uuid_b);
}

#elif defined(BABELTRACE_HAVE_LIBC_UUID)
#include <uuid.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

static inline
int babeltrace_uuid_generate(unsigned char *uuid_out)
{
	uint32_t status;

	uuid_create((uuid_t *) uuid_out, &status);
	if (status == uuid_s_ok)
		return 0;
	else
		return -1;
}

static inline
int babeltrace_uuid_unparse(const unsigned char *uuid_in, char *str_out)
{
	uint32_t status;
	char *alloc_str;
	int ret;

	uuid_to_string((uuid_t *) uuid_in, &alloc_str, &status);
	if (status == uuid_s_ok) {
		strcpy(str_out, alloc_str);
		ret = 0;
	} else {
		ret = -1;
	}
	free(alloc_str);
	return ret;
}

static inline
int babeltrace_uuid_parse(const char *str_in, unsigned char *uuid_out)
{
	uint32_t status;

	uuid_from_string(str_in, (uuid_t *) uuid_out, &status);
	if (status == uuid_s_ok)
		return 0;
	else
		return -1;
}

static inline
int babeltrace_uuid_compare(const unsigned char *uuid_a,
		const unsigned char *uuid_b)
{
	uint32_t status;

	uuid_compare((uuid_t *) uuid_a, (uuid_t *) uuid_b, &status);
	if (status == uuid_s_ok)
		return 0;
	else
		return -1;
}

#else
#error "Babeltrace needs to have a UUID generator configured."
#endif

#endif /* _BABELTRACE_UUID_H */
