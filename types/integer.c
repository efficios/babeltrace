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
struct declaration *_integer_declaration_new(struct type *type,
			       struct declaration_scope *parent_scope);
static
void _integer_declaration_free(struct declaration *declaration);

void integer_copy(struct stream_pos *dest, const struct format *fdest, 
		  struct stream_pos *src, const struct format *fsrc,
		  struct declaration *declaration)
{
	struct declaration_integer *integer =
		container_of(declaration, struct declaration_integer, p);
	struct type_integer *integer_type = integer->type;

	if (!integer_type->signedness) {
		uint64_t v;

		v = fsrc->uint_read(src, integer_type);
		fdest->uint_write(dest, integer_type, v);
	} else {
		int64_t v;

		v = fsrc->int_read(src, integer_type);
		fdest->int_write(dest, integer_type, v);
	}
}

static
void _integer_type_free(struct type *type)
{
	struct type_integer *integer_type =
		container_of(type, struct type_integer, p);
	g_free(integer_type);
}

struct type_integer *
	integer_type_new(const char *name, size_t len, int byte_order,
			 int signedness, size_t alignment)
{
	struct type_integer *integer_type;

	integer_type = g_new(struct type_integer, 1);
	integer_type->p.id = CTF_TYPE_INTEGER;
	integer_type->p.name = g_quark_from_string(name);
	integer_type->p.alignment = alignment;
	integer_type->p.copy = integer_copy;
	integer_type->p.type_free = _integer_type_free;
	integer_type->p.declaration_free = _integer_declaration_free;
	integer_type->p.declaration_new = _integer_declaration_new;
	integer_type->p.ref = 1;
	integer_type->len = len;
	integer_type->byte_order = byte_order;
	integer_type->signedness = signedness;
	return integer_type;
}

static
struct declaration *
	_integer_declaration_new(struct type *type,
				 struct declaration_scope *parent_scope)
{
	struct type_integer *integer_type =
		container_of(type, struct type_integer, p);
	struct declaration_integer *integer;

	integer = g_new(struct declaration_integer, 1);
	type_ref(&integer_type->p);
	integer->p.type = type;
	integer->type = integer_type;
	integer->p.ref = 1;
	integer->value._unsigned = 0;
	return &integer->p;
}

static
void _integer_declaration_free(struct declaration *declaration)
{
	struct declaration_integer *integer =
		container_of(declaration, struct declaration_integer, p);

	type_unref(integer->p.type);
	g_free(integer);
}
