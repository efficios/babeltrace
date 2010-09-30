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
#include <stdint.h>

size_t copy_integer(unsigned char *dest, const struct format *fdest, 
		    const unsigned char *src, const struct format *fsrc,
		    const struct type_class *type_class)
{
	struct type_class_integer *int_class =
		container_of(type_class, struct type_class_integer, p);

	if (!int_class->signedness) {
		uint64_t v;

		v = fsrc->uint_read(src, int_class->byte_order, int_class->len);
		return fdest->uint_write(dest, int_class->byte_order,
					 int_class->len, v);
	} else {
		int64_t v;

		v = fsrc->int_read(src, int_class->byte_order, int_class->len);
		return fdest->int_write(dest, int_class->byte_order,
					int_class->len, v);
	}
}
