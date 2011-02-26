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
struct type_float *_float_type_new(struct type_class *type_class,
				   struct declaration_scope *parent_scope);
static
void _float_type_free(struct type *type);

void float_copy(struct stream_pos *destp,
		const struct format *fdest,
		struct stream_pos *srcp,
		const struct format *fsrc,
		struct type *type)
{
	struct type_float *_float = container_of(type, struct type_float, p);
	struct type_class_float *float_class = _float->_class;

	if (fsrc->float_copy == fdest->float_copy) {
		fsrc->float_copy(destp, srcp, float_class);
	} else {
		double v;

		v = fsrc->double_read(srcp, float_class);
		fdest->double_write(destp, float_class, v);
	}
}

static
void _float_type_class_free(struct type_class *type_class)
{
	struct type_class_float *float_class =
		container_of(type_class, struct type_class_float, p);

	type_class_unref(&float_class->exp->p);
	type_class_unref(&float_class->mantissa->p);
	type_class_unref(&float_class->sign->p);
	g_free(float_class);
}

struct type_class_float *
float_type_class_new(const char *name, size_t mantissa_len,
		     size_t exp_len, int byte_order, size_t alignment)
{
	struct type_class_float *float_class;
	struct type_class *type_class;
	int ret;

	float_class = g_new(struct type_class_float, 1);
	type_class = &float_class->p;
	type_class->name = g_quark_from_string(name);
	type_class->alignment = alignment;
	type_class->copy = float_copy;
	type_class->class_free = _float_type_class_free;
	type_class->type_new = _float_type_new;
	type_class->type_free = _float_type_free;
	type_class->ref = 1;
	float_class->byte_order = byte_order;

	float_class->sign = integer_type_new(NULL, 1,
					     byte_order, false, 1);
	if (!float_class->mantissa)
		goto error_sign;
	float_class->mantissa = integer_type_new(NULL, mantissa_len - 1,
						 byte_order, false, 1);
	if (!float_class->mantissa)
		goto error_mantissa;
	float_class->exp = integer_type_new(NULL, exp_len,
					    byte_order, true, 1);
	if (!float_class->exp)
		goto error_exp;

	if (float_class->p.name) {
		ret = register_type(&float_class->p);
		if (ret)
			goto error_register;
	}
	return float_class;

error_register:
	type_class_unref(&float_class->exp->p);
error_exp:
	type_class_unref(&float_class->mantissa->p);
error_mantissa:
	type_class_unref(&float_class->sign->p);
error_sign:
	g_free(float_class);
	return NULL;
}

static
struct type_float *_float_type_new(struct type_class *type_class,
				   struct declaration_scope *parent_scope)
{
	struct type_class_float *float_class =
		container_of(type_class, struct type_class_float, p);
	struct type_float *_float;

	_float = g_new(struct type_float, 1);
	type_class_ref(&_float_class->p);
	_float->p._class = _float_class;
	_float->p.ref = 1;
	_float->value = 0.0;
	return &_float->p;
}

static
void _float_type_free(struct type *type)
{
	struct type_float *_float = container_of(type, struct type_float, p);

	type_class_unref(_float->p._class);
	g_free(_float);
}
