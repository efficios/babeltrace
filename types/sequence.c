/*
 * sequence.c
 *
 * BabelTrace - Sequence Type Converter
 *
 * Copyright 2010 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 */

#include <babeltrace/compiler.h>
#include <babeltrace/format.h>

#ifndef max
#define max(a, b)	((a) < (b) ? (b) : (a))
#endif

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
		elem_class->copy(dest, fdest, src, fsrc, elem_class);
	}
	fsrc->sequence_end(src, sequence_class);
	fdest->sequence_end(dest, sequence_class);
}

void sequence_type_free(struct type_class_sequence *sequence_class)
{
	sequence_class->elem->free(sequence_class->elem);
	type_unref(&sequence_class->len_class->p);
	type_unref(sequence_class->elem);
	g_free(sequence_class);
}

static void _sequence_type_free(struct type_class *type_class)
{
	struct type_class_sequence *sequence_class =
		container_of(type_class, struct type_class_sequence, p);
	sequence_type_free(sequence_class);
}

struct type_class_sequence *
sequence_type_new(const char *name, struct type_class_integer *len_class,
		  struct type_class *elem)
{
	struct type_class_sequence *sequence_class;
	struct type_class *type_class;
	int ret;

	sequence_class = g_new(struct type_class_sequence, 1);
	type_class = &sequence_class->p;

	assert(!len_class->signedness);

	type_ref(&len_class->p);
	sequence_class->len_class = len_class;
	type_ref(elem);
	sequence_class->elem = elem;
	type_class->name = g_quark_from_string(name);
	type_class->alignment = max(len_class->p.alignment,
				    elem->alignment);
	type_class->copy = sequence_copy;
	type_class->free = _sequence_type_free;
	type_class->ref = 1;

	if (type_class->name) {
		ret = register_type(type_class);
		if (ret)
			goto error_register;
	}
	return sequence_class;

error_register:
	type_unref(&len_class->p);
	type_unref(elem);
	g_free(sequence_class);
	return NULL;
}
