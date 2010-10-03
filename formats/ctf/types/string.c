/*
 * Common Trace Format
 *
 * Strings read/write functions.
 *
 * Copyright (c) 2010 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <babeltrace/ctf/types.h>
#include <limits.h>		/* C99 limits */
#include <string.h>

void ctf_string_copy(struct stream_pos *dest, struct stream_pos *src,
		     const struct type_class_string *string_class)
{
	size_t len;
	unsigned char *destaddr, *srcaddr;

	align_pos(src, string_class->p.alignment);
	srcaddr = get_pos_addr(src);
	len = strlen(srcaddr) + 1;
	if (!dest)
		goto end;
	align_pos(dest, string_class->p.alignment);
	destaddr = get_pos_addr(dest);
	strcpy(dest, src);
	move_pos(dest, len);
end:
	move_pos(src, len);
}
