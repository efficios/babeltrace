/*
 * BabelTrace - Sequence Type Converter
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

void sequence_copy(struct stream_pos *dest, const struct format *fdest, 
		   struct stream_pos *src, const struct format *fsrc,
		   const struct type_class *type_class)
{
	struct type_class_sequence *sequence_class =
		container_of(type_class, struct type_class_sequence, p);
	unsigned int i;
	size_t len;

	fsrc->sequence_begin(src, sequence_class);
	fdest->sequence_begin(dest, sequence_class);

	len = fsrc->uint_read(src, sequence_class->len_class);
	fdest->uint_write(dest, sequence_class->len_class, len);

	for (i = 0; i < len; i++) {
		struct type_class *elem_class = sequence_class->elem;
		elem_class->copy(dest, fdest, src, fsrc, &elem_class->p);
	}
	fsrc->sequence_end(src, sequence_class);
	fdest->sequence_end(dest, sequence_class);
}

void sequence_type_free(struct type_class_sequence *sequence_class)
{
	sequence_class->elem->free(&sequence_class->elem->p);
	g_free(sequence_class);
}

static void _sequence_type_free(struct type_class *type_class)
{
	struct type_class_struct *sequence_class =
		container_of(type_class, struct type_class_sequence, p);
	sequence_type_free(sequence_class);
}

struct type_class_sequence *
sequence_type_new(const char *name, struct type_class_integer *len_class,
		  struct type_class *elem)
{
	struct type_class_sequence *sequence_class;
	int ret;

	sequence_class = g_new(struct type_class_sequence, 1);
	type_class = &float_class->p;

	assert(!len_class->signedness);

	sequence_class->len = len;
	type_class->name = g_quark_from_string(name);
	type_class->alignment = max(len_class->p.alignment,
				    elem->p.alignment);
	type_class->copy = sequence_copy;
	type_class->free = _sequence_type_free;

	if (type_class->name) {
		ret = ctf_register_type(type_class);
		if (ret)
			goto error_register;
	}
	return sequence_class;

error_register:
	len_class->p.free(&len_class->p);
	sequence_class->elem->free(&sequence_class->elem->p);
	g_free(sequence_class);
	return NULL;
}
