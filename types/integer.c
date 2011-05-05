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
struct definition *_integer_definition_new(struct declaration *declaration,
			       struct definition_scope *parent_scope,
			       GQuark field_name, int index);
static
void _integer_definition_free(struct definition *definition);

void integer_copy(struct stream_pos *dest, const struct format *fdest, 
		  struct stream_pos *src, const struct format *fsrc,
		  struct definition *definition)
{
	struct definition_integer *integer =
		container_of(definition, struct definition_integer, p);
	struct declaration_integer *integer_declaration = integer->declaration;

	if (!integer_declaration->signedness) {
		uint64_t v;

		v = fsrc->uint_read(src, integer_declaration);
		if (fdest)
			fdest->uint_write(dest, integer_declaration, v);
	} else {
		int64_t v;

		v = fsrc->int_read(src, integer_declaration);
		if (fdest)
			fdest->int_write(dest, integer_declaration, v);
	}
}

static
void _integer_declaration_free(struct declaration *declaration)
{
	struct declaration_integer *integer_declaration =
		container_of(declaration, struct declaration_integer, p);
	g_free(integer_declaration);
}

struct declaration_integer *
	integer_declaration_new(size_t len, int byte_order,
			 int signedness, size_t alignment)
{
	struct declaration_integer *integer_declaration;

	integer_declaration = g_new(struct declaration_integer, 1);
	integer_declaration->p.id = CTF_TYPE_INTEGER;
	integer_declaration->p.alignment = alignment;
	integer_declaration->p.copy = integer_copy;
	integer_declaration->p.declaration_free = _integer_declaration_free;
	integer_declaration->p.definition_free = _integer_definition_free;
	integer_declaration->p.definition_new = _integer_definition_new;
	integer_declaration->p.ref = 1;
	integer_declaration->len = len;
	integer_declaration->byte_order = byte_order;
	integer_declaration->signedness = signedness;
	return integer_declaration;
}

static
struct definition *
	_integer_definition_new(struct declaration *declaration,
				struct definition_scope *parent_scope,
				GQuark field_name, int index)
{
	struct declaration_integer *integer_declaration =
		container_of(declaration, struct declaration_integer, p);
	struct definition_integer *integer;

	integer = g_new(struct definition_integer, 1);
	declaration_ref(&integer_declaration->p);
	integer->p.declaration = declaration;
	integer->declaration = integer_declaration;
	integer->p.ref = 1;
	integer->p.index = index;
	integer->value._unsigned = 0;
	return &integer->p;
}

static
void _integer_definition_free(struct definition *definition)
{
	struct definition_integer *integer =
		container_of(definition, struct definition_integer, p);

	declaration_unref(integer->p.declaration);
	g_free(integer);
}
