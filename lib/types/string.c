/*
 * Common Trace Format
 *
 * Strings read/write functions.
 *
 * Copyright 2010 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * Dual LGPL v2.1/GPL v2 license.
 */

#include <ctf/ctf-types.h>
#include <string.h>

size_t string_copy(char *dest, const char *src)
{
	size_t len = strlen(src) + 1;

	if (!dest)
		goto end;
	strcpy(dest, src);
end:
	return len * 8;
}
