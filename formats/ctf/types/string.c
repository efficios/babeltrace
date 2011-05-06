/*
 * Common Trace Format
 *
 * Strings read/write functions.
 *
 * Copyright 2010 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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
 */

#include <babeltrace/ctf/types.h>
#include <limits.h>		/* C99 limits */
#include <string.h>

void ctf_string_copy(struct stream_pos *pdest, struct stream_pos *psrc,
		     const struct declaration_string *string_declaration)
{
	struct ctf_stream_pos *dest = ctf_pos(pdest);
	struct ctf_stream_pos *src = ctf_pos(psrc);
	size_t len;
	char *destaddr, *srcaddr;

	ctf_align_pos(src, string_declaration->p.alignment);
	srcaddr = ctf_get_pos_addr(src);
	len = strlen(srcaddr) + 1;
	if (dest->dummy)
		goto end;
	ctf_align_pos(dest, string_declaration->p.alignment);
	destaddr = ctf_get_pos_addr(dest);
	strcpy(destaddr, srcaddr);
end:
	ctf_move_pos(dest, len);
	ctf_move_pos(src, len);
}

void ctf_string_read(char **dest, struct stream_pos *psrc,
		     const struct declaration_string *string_declaration)
{
	struct ctf_stream_pos *src = ctf_pos(psrc);
	size_t len;
	char *srcaddr;

	ctf_align_pos(src, string_declaration->p.alignment);
	srcaddr = ctf_get_pos_addr(src);
	len = strlen(srcaddr) + 1;
	*dest = g_realloc(*dest, len);
	strcpy(*dest, srcaddr);
	ctf_move_pos(src, len);
}

void ctf_string_write(struct stream_pos *pdest, const char *src,
		      const struct declaration_string *string_declaration)
{
	struct ctf_stream_pos *dest = ctf_pos(pdest);
	size_t len;
	char *destaddr;

	ctf_align_pos(dest, string_declaration->p.alignment);
	len = strlen(src) + 1;
	if (dest->dummy)
		goto end;
	destaddr = ctf_get_pos_addr(dest);
	strcpy(destaddr, src);
end:
	ctf_move_pos(dest, len);
}

void ctf_string_free_temp(char *string)
{
	g_free(string);
}
