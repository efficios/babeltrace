#ifndef _CTF_TYPES_BITFIELD_H
#define _CTF_TYPES_BITFIELD_H

/*
 * Common Trace Format
 *
 * Bitfields read/write functions.
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

#include <ctf/bitfield.h>
#include <endian.h>

static inline
uint64_t ctf_bitfield_unsigned_read(const unsigned char *ptr,
				    unsigned long start, unsigned long len,
				    int byte_order)
{
	uint64_t v;

	if (byte_order == LITTLE_ENDIAN)
		ctf_bitfield_read_le(ptr, start, len, &v);
	else
		ctf_bitfield_read_be(ptr, start, len, &v);
	return v;
}

static inline
int64_t ctf_bitfield_signed_read(const unsigned char *ptr,
				 unsigned long start, unsigned long len,
				 int byte_order)
{
	int64_t v;

	if (byte_order == LITTLE_ENDIAN)
		ctf_bitfield_read_le(ptr, start, len, &v);
	else
		ctf_bitfield_read_be(ptr, start, len, &v);
	return v;
}

static inline
size_t ctf_bitfield_unsigned_write(unsigned char *ptr,
				   unsigned long start, unsigned long len,
				   int byte_order, uint64_t v)
{
	if (!ptr)
		goto end;
	if (byte_order == LITTLE_ENDIAN)
		ctf_bitfield_write_le(ptr, start, len, v);
	else
		ctf_bitfield_write_be(ptr, start, len, v);
end:
	return len;
}

static inline
size_t ctf_bitfield_signed_write(unsigned char *ptr,
				 unsigned long start, unsigned long len,
				 int byte_order, int64_t v)
{
	if (!ptr)
		goto end;
	if (byte_order == LITTLE_ENDIAN)
		ctf_bitfield_write_le(ptr, start, len, v);
	else
		ctf_bitfield_write_be(ptr, start, len, v);
end:
	return len;
}

#endif /* _CTF_TYPES_BITFIELD_H */
