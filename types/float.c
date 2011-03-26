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
struct declaration *_float_declaration_new(struct type *type,
				   struct declaration_scope *parent_scope);
static
void _float_declaration_free(struct declaration *declaration);

void float_copy(struct stream_pos *destp,
		const struct format *fdest,
		struct stream_pos *srcp,
		const struct format *fsrc,
		struct declaration *declaration)
{
	struct declaration_float *_float =
		container_of(declaration, struct declaration_float, p);
	struct type_float *float_type = _float->type;

	if (fsrc->float_copy == fdest->float_copy) {
		fsrc->float_copy(destp, srcp, float_type);
	} else {
		double v;

		v = fsrc->double_read(srcp, float_type);
		fdest->double_write(destp, float_type, v);
	}
}

static
void _float_type_free(struct type *type)
{
	struct type_float *float_type =
		container_of(type, struct type_float, p);

	type_unref(&float_type->exp->p);
	type_unref(&float_type->mantissa->p);
	type_unref(&float_type->sign->p);
	g_free(float_type);
}

struct type_float *
	float_type_new(const char *name, size_t mantissa_len,
		       size_t exp_len, int byte_order, size_t alignment)
{
	struct type_float *float_type;
	struct type *type;

	float_type = g_new(struct type_float, 1);
	type = &float_type->p;
	type->id = CTF_TYPE_FLOAT;
	type->name = g_quark_from_string(name);
	type->alignment = alignment;
	type->copy = float_copy;
	type->type_free = _float_type_free;
	type->declaration_new = _float_declaration_new;
	type->declaration_free = _float_declaration_free;
	type->ref = 1;
	float_type->byte_order = byte_order;

	float_type->sign = integer_type_new(NULL, 1,
					    byte_order, false, 1);
	float_type->mantissa = integer_type_new(NULL, mantissa_len - 1,
						byte_order, false, 1);
	float_type->exp = integer_type_new(NULL, exp_len,
					   byte_order, true, 1);
	return float_type;
}

static
struct declaration *
	_float_declaration_new(struct type *type,
			       struct declaration_scope *parent_scope)
{
	struct type_float *float_type =
		container_of(type, struct type_float, p);
	struct declaration_float *_float;

	_float = g_new(struct declaration_float, 1);
	type_ref(&float_type->p);
	_float->p.type= type;
	_float->type= float_type;
	_float->p.ref = 1;
	_float->value = 0.0;
	return &_float->p;
}

static
void _float_declaration_free(struct declaration *declaration)
{
	struct declaration_float *_float =
		container_of(declaration, struct declaration_float, p);

	type_unref(_float->p.type);
	g_free(_float);
}
