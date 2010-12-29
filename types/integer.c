/*
 * integer.c
 *
 * BabelTrace - Integer Type Converter
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
#include <babeltrace/align.h>
#include <babeltrace/format.h>
#include <stdint.h>

void integer_copy(struct stream_pos *dest, const struct format *fdest, 
		  struct stream_pos *src, const struct format *fsrc,
		  const struct type_class *type_class)
{
	struct type_class_integer *int_class =
		container_of(type_class, struct type_class_integer, p);

	if (!int_class->signedness) {
		uint64_t v;

		v = fsrc->uint_read(src, int_class);
		fdest->uint_write(dest, int_class, v);
	} else {
		int64_t v;

		v = fsrc->int_read(src, int_class);
		fdest->int_write(dest, int_class, v);
	}
}

void integer_type_free(struct type_class_integer *int_class)
{
	g_free(int_class);
}

static void _integer_type_free(struct type_class *type_class)
{
	struct type_class_integer *int_class =
		container_of(type_class, struct type_class_integer, p);
	integer_type_free(int_class);
}

struct type_class_integer *integer_type_new(const char *name,
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
	int_class->p.free = _integer_type_free;
	int_class->p.ref = 1;
	int_class->len = len;
	int_class->byte_order = byte_order;
	int_class->signedness = signedness;
	if (int_class->p.name) {
		ret = ctf_register_type(&int_class->p);
		if (ret) {
			g_free(int_class);
			return NULL;
		}
	}
	return int_class;
}
