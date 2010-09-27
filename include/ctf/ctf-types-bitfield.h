#ifndef _CTF_TYPES_BITFIELD_H
#define _CTF_TYPES_BITFIELD_H

/*
 * Common Trace Format
 *
 * Bitfields read/write functions.
 *
 * Copyright 2010 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * Dual LGPL v2.1/GPL v2 license.
 */

#include <ctf/bitfield.h>
#include <endian.h>

static inline
uint64_t ctf_bitfield_unsigned_read(const uint8_t *ptr,
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
int64_t ctf_bitfield_signed_read(const uint8_t *ptr,
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
size_t ctf_bitfield_unsigned_write(uint8_t *ptr,
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
size_t ctf_bitfield_signed_write(uint8_t *ptr,
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
