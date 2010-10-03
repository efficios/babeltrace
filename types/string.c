/*
 * BabelTrace - String Type Converter
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

void string_copy(struct stream_pos *dest, const struct format *fdest, 
		 struct stream_pos *src, const struct format *fsrc,
		 const struct type_class *type_class)
{
	struct type_class_string *string_class =
		container_of(type_class, struct type_class_string, p);

	if (fsrc->string_copy == fdest->string_copy) {
		fsrc->string_copy(dest, src, string_class);
	} else {
		unsigned char *tmp = NULL;

		fsrc->string_read(&tmp, src, string_class);
		fdest->string_write(dest, tmp, string_class);
		fsrc->string_free_temp(tmp);
	}
}

void string_type_free(struct type_class_string *string_class)
{
	g_free(string_class);
}

static void _string_type_free(struct type_class *type_class)
{
	struct type_class_string *string_class =
		container_of(type_class, struct type_class_string, p);
	string_type_free(string_class);
}

struct type_class_string *string_type_new(const char *name)
{
	struct type_class_string *string_class;
	int ret;

	string_class = g_new(struct type_class_string, 1);
	string_class->p.name = g_quark_from_string(name);
	string_class->p.alignment = CHAR_BIT;
	string_class->p.copy = string_copy;
	string_class->p.free = _string_type_free;
	if (string_class->p.name) {
		ret = ctf_register_type(&string_class->p);
		if (ret) {
			g_free(string_class);
			return NULL;
		}
	}
	return string_class;
}
