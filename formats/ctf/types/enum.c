/*
 * Common Trace Format
 *
 * Enumeration mapping strings (quarks) from/to integers.
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
#include <stdint.h>
#include <glib.h>

GQuark ctf_enum_read(struct stream_pos *pos,
		     const struct type_class_enum *src)
{
	struct type_class_bitfield *bitfield_class = &src->p;
	struct type_class_integer *int_class = &bitfield_class->p;

	if (!int_class->signedness) {
		uint64_t v;

		v = ctf_bitfield_unsigned_read(pos, bitfield_class);
		return enum_uint_to_quark(src, v);
	} else {
		int64_t v;

		v = fsrc->bitfield_signed_read(pos, bitfield_class);
		return enum_int_to_quark(src, v);
	}
}

size_t ctf_enum_write(struct stream_pos *pos,
		      const struct type_class_enum *dest,
		      GQuark q)
{
	struct type_class_bitfield *bitfield_class = &dest->p;
	struct type_class_integer *int_class = &bitfield_class->p;

	if (!int_class->signedness) {
		uint64_t v;

		v = enum_quark_to_uint(dest, q);
		return ctf_bitfield_unsigned_write(pos, bitfield_class, v);
	} else {
		int64_t v;

		v = enum_quark_to_int(dest, q);
		return ctf_bitfield_signed_write(pos, bitfield_class, v);
	}
}
