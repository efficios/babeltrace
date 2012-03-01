#ifndef _BABELTRACE_UUID_H
#define _BABELTRACE_UUID_H

/*
 * Copyright (C) 2011   Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to use or copy this program
 * for any purpose,  provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is granted,
 * provided the above notices are retained, and a notice that the code was
 * modified is included with the above copyright notice.
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
void babeltrace_uuid_unparse(const unsigned char *uuid_in, char *str_out)
{
	return uuid_unparse(uuid_in, str_out);
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
void babeltrace_uuid_unparse(const unsigned char *uuid_in, char *str_out)
{
	uint32_t status;

	uuid_to_string((uuid_t *) uuid_in, str_out, &status);
	if (status == uuid_s_ok)
		return 0;
	else
		return -1;
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
