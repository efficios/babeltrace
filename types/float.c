/*
 * float.c
 *
 * BabelTrace - Float Type Converter
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
#include <babeltrace/format.h>

static
struct definition *_float_definition_new(struct declaration *declaration,
				   struct definition_scope *parent_scope,
				   GQuark field_name, int index);
static
void _float_definition_free(struct definition *definition);

void float_copy(struct stream_pos *destp,
		const struct format *fdest,
		struct stream_pos *srcp,
		const struct format *fsrc,
		struct definition *definition)
{
	struct definition_float *_float =
		container_of(definition, struct definition_float, p);
	struct declaration_float *float_declaration = _float->declaration;
	long double v;

	v = fsrc->ldouble_read(srcp, float_declaration);
	_float->value = v;
	if (fdest)
		fdest->ldouble_write(destp, float_declaration, v);
}

static
void _float_declaration_free(struct declaration *declaration)
{
	struct declaration_float *float_declaration =
		container_of(declaration, struct declaration_float, p);

	declaration_unref(&float_declaration->exp->p);
	declaration_unref(&float_declaration->mantissa->p);
	declaration_unref(&float_declaration->sign->p);
	g_free(float_declaration);
}

struct declaration_float *
	float_declaration_new(size_t mantissa_len,
		       size_t exp_len, int byte_order, size_t alignment)
{
	struct declaration_float *float_declaration;
	struct declaration *declaration;

	float_declaration = g_new(struct declaration_float, 1);
	declaration = &float_declaration->p;
	declaration->id = CTF_TYPE_FLOAT;
	declaration->alignment = alignment;
	declaration->copy = float_copy;
	declaration->declaration_free = _float_declaration_free;
	declaration->definition_new = _float_definition_new;
	declaration->definition_free = _float_definition_free;
	declaration->ref = 1;
	float_declaration->byte_order = byte_order;

	float_declaration->sign = integer_declaration_new(1,
					    byte_order, false, 1);
	float_declaration->mantissa = integer_declaration_new(mantissa_len - 1,
						byte_order, false, 1);
	float_declaration->exp = integer_declaration_new(exp_len,
					   byte_order, true, 1);
	return float_declaration;
}

static
struct definition *
	_float_definition_new(struct declaration *declaration,
			      struct definition_scope *parent_scope,
			      GQuark field_name, int index)
{
	struct declaration_float *float_declaration =
		container_of(declaration, struct declaration_float, p);
	struct definition_float *_float;

	_float = g_new(struct definition_float, 1);
	declaration_ref(&float_declaration->p);
	_float->p.declaration = declaration;
	_float->declaration = float_declaration;
	_float->p.ref = 1;
	_float->p.index = index;
	_float->value = 0.0;
	return &_float->p;
}

static
void _float_definition_free(struct definition *definition)
{
	struct definition_float *_float =
		container_of(definition, struct definition_float, p);

	declaration_unref(_float->p.declaration);
	g_free(_float);
}
