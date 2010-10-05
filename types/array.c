/*
 * BabelTrace - Array Type Converter
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

void array_copy(struct stream_pos *dest, const struct format *fdest, 
		struct stream_pos *src, const struct format *fsrc,
		const struct type_class *type_class)
{
	struct type_class_array *array_class =
		container_of(type_class, struct type_class_array, p);
	unsigned int i;

	fsrc->array_begin(src, array_class);
	fdest->array_begin(dest, array_class);

	for (i = 0; i < array_class->len; i++) {
		struct type_class *elem_class = array_class->elem;
		elem_class->copy(dest, fdest, src, fsrc, &elem_class->p);
	}
	fsrc->array_end(src, array_class);
	fdest->array_end(dest, array_class);
}

void array_type_free(struct type_class_array *array_class)
{
	array_class->elem->free(&array_class->elem->p);
	g_free(array_class);
}

static void _array_type_free(struct type_class *type_class)
{
	struct type_class_struct *array_class =
		container_of(type_class, struct type_class_array, p);
	array_type_free(array_class);
}

struct type_class_array *array_type_new(const char *name, size_t len,
					struct type_class *elem)
{
	struct type_class_array *array_class;
	int ret;

	array_class = g_new(struct type_class_array, 1);
	type_class = &float_class->p;

	array_class->len = len;
	type_class->name = g_quark_from_string(name);
	/* No need to align the array, the first element will align itself */
	type_class->alignment = 1;
	type_class->copy = array_copy;
	type_class->free = _array_type_free;

	if (type_class->name) {
		ret = ctf_register_type(type_class);
		if (ret)
			goto error_register;
	}
	return array_class;

error_register:
	g_free(array_class);
	return NULL;
}
