/*
 * integer.c
 *
 * BabelTrace - Integer Type Converter
 *
 * Copyright 2010, 2011 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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

static
struct type *_integer_type_new(struct type_class *type_class,
			       struct declaration_scope *parent_scope);
static
void _integer_type_free(struct type *type);

void integer_copy(struct stream_pos *dest, const struct format *fdest, 
		  struct stream_pos *src, const struct format *fsrc,
		  struct type *type)
{
	struct type_integer *integer = container_of(type, struct type_integer,
						    p);
	struct type_class_integer *int_class = integer->_class;

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

static
void _integer_type_class_free(struct type_class *type_class)
{
	struct type_class_integer *integer_class =
		container_of(type_class, struct type_class_integer, p);
	g_free(integer_class);
}

struct type_class_integer *
integer_type_class_new(const char *name, size_t len, int byte_order,
		       int signedness, size_t alignment)
{
	struct type_class_integer *int_class;
	int ret;

	int_class = g_new(struct type_class_integer, 1);
	int_class->p.name = g_quark_from_string(name);
	int_class->p.alignment = alignment;
	int_class->p.copy = integer_copy;
	int_class->p.class_free = _integer_type_class_free;
	int_class->p.type_free = _integer_type_free;
	int_class->p.type_new = _integer_type_new;
	int_class->p.ref = 1;
	int_class->len = len;
	int_class->byte_order = byte_order;
	int_class->signedness = signedness;
	if (int_class->p.name) {
		ret = register_type(&int_class->p);
		if (ret) {
			g_free(int_class);
			return NULL;
		}
	}
	return int_class;
}

static
struct type_integer *_integer_type_new(struct type_class *type_class,
				       struct declaration_scope *parent_scope)
{
	struct type_class_integer *integer_class =
		container_of(type_class, struct type_class_integer, p);
	struct type_integer *integer;

	integer = g_new(struct type_integer, 1);
	type_class_ref(&integer_class->p);
	integer->p._class = integer_class;
	integer->p.ref = 1;
	integer->value._unsigned = 0;
	return &integer->p;
}

static
void _integer_type_free(struct type *type)
{
	struct type_integer *integer =
		container_of(type, struct type_integer, p);

	type_class_unref(integer->p._class);
	g_free(integer);
}
