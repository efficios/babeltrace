/*
 * BabelTrace - Bitfield Type Converter
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

#include <babeltrace/compiler.h>
#include <babeltrace/types.h>
#include <stdint.h>

/*
 * Shortcut to integer copy if we copy bit-aligned data with 0 start_offset.
 * This skips the bitfield overhead when dealing with enumerations (which use
 * the bitfield copy functions).
 */
extern
size_t integer_copy(unsigned char *dest, const struct format *fdest, 
		    const unsigned char *src, const struct format *fsrc,
		    const struct type_class *type_class);

size_t bitfield_copy(unsigned char *dest, const struct format *fdest, 
		     const unsigned char *src, const struct format *fsrc,
		     const struct type_class *type_class)
{
	struct type_class_bitfield *bitfield_class =
		container_of(type_class, struct type_class_bitfield, p);
	struct type_class_integer *int_class = &bitfield_class->p;

	if (!(int_class->p.alignment % CHAR_BIT)
	    && !(int_class->len % CHAR_BIT)
	    && !(bitfield_class->start_offset % CHAR_BIT)) {
		size_t offset = bitfield_class->start_offset / CHAR_BIT;
		dest += offset;
		src += offset;
		return integer_copy(dest, fdest, src, fsrc, type_class);
	}

	if (!int_class->signedness) {
		uint64_t v;

		v = fsrc->bitfield_unsigned_read(src,
					bitfield_class->start_offset,
					int_class->len,
					int_class->byte_order);
		return fdest->bitfield_unsigned_write(dest,
					bitfield_class->start_offset,
					int_class->len, int_class->byte_order,
					v);
	} else {
		int64_t v;

		v = fsrc->bitfield_signed_read(src,
					bitfield_class->start_offset,
					int_class->len,
					int_class->byte_order);
		return fdest->bitfield_signed_write(dest,
					bitfield_class->start_offset,
					int_class->len, int_class->byte_order,
					v);
	}
}

struct type_class_bitfield *bitfield_type_new(const char *name,
					      size_t start_offset,
					      size_t len, int byte_order,
					      int signedness,
					      size_t alignment)
{
	struct type_class_bitfield *bitfield_class;
	struct type_class_integer *int_class;
	int ret;

	bitfield_class = g_new(struct type_class_bitfield, 1);
	int_class = &bitfield_class->p;
	int_class->p.name = g_quark_from_string(name);
	int_class->p.alignment = alignment;
	int_class->len = len;
	int_class->byte_order = byte_order;
	int_class->signedness = signedness;
	bitfield_class->start_offset = start_offset;
	if (int_class->p.name) {
		ret = ctf_register_type(&int_class->p);
		if (ret) {
			g_free(bitfield_class);
			return NULL;
		}
	}
	return bitfield_class;
}

void bitfield_type_free(struct type_class_bitfield *bitfield_class)
{
	g_free(bitfield_class);
}
