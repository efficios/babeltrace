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

/*
 * ctf_bitfield_unsigned_read and ctf_bitfield_signed_read are defined by
 * bitfield.h.
 *
 * The write primitives below are provided as wrappers over
 * ctf_bitfield_write_le and ctf_bitfield_write_be to specify per-byte write of
 * signed/unsigned integers through a standard API.
 */

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
