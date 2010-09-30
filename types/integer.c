/*
 * BabelTrace - Integer Type Converter
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
#include <babeltrace/align.h>
#include <babeltrace/types.h>
#include <stdint.h>

size_t integer_copy(unsigned char *dest, const struct format *fdest, 
		    const unsigned char *src, const struct format *fsrc,
		    const struct type_class *type_class)
{
	struct type_class_integer *int_class =
		container_of(type_class, struct type_class_integer, p);

	if (fsrc->p.alignment)
		src = PTR_ALIGN(src, fsrc->p.alignment / CHAR_BIT);
	if (fdest->p.alignment)
		dest = PTR_ALIGN(dest, fdest->p.alignment / CHAR_BIT);

	if (!int_class->signedness) {
		uint64_t v;

		v = fsrc->uint_read(src, int_class->len, int_class->byte_order);
		return fdest->uint_write(dest, int_class->len, int_class->byte_order, v);
	} else {
		int64_t v;

		v = fsrc->int_read(src, int_class->len, int_class->byte_order);
		return fdest->int_write(dest, int_class->len, int_class->byte_order, v);
	}
}

struct type_class_integer *integer_type_new(const char *name,
					    size_t start_offset,
					    size_t len, int byte_order,
					    int signedness,
					    size_t alignment)
{
	struct type_class_integer *int_class;
	int ret;

	int_class = g_new(struct type_class_integer, 1);
	int_class->p.name = g_quark_from_string(name);
	int_class->p.alignment = alignment;
	int_class->p.copy = integer_copy;
	int_class->len = len;
	int_class->byte_order = byte_order;
	int_class->signedness = signedness;
	if (int_class->p.name) {
		ret = ctf_register_type(&int_class.p);
		if (ret) {
			g_free(int_class);
			return NULL;
		}
	}
	return int_class;
}

void integer_type_free(struct type_class_integer *int_class)
{
	g_free(int_class);
}
