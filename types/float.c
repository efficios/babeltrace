/*
 * float.c
 *
 * BabelTrace - Float Type Converter
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
#include <babeltrace/types.h>

void float_copy(struct stream_pos *dest, const struct format *fdest, 
		struct stream_pos *src, const struct format *fsrc,
		const struct type_class *type_class)
{
	struct type_class_float *float_class =
		container_of(type_class, struct type_class_float, p);

	if (fsrc->float_copy == fdest->float_copy) {
		fsrc->float_copy(dest, src, float_class);
	} else {
		double v;

		v = fsrc->double_read(src, float_class);
		fdest->double_write(dest, float_class, v);
	}
}

void float_type_free(struct type_class_float *float_class)
{
	integer_type_free(float_class->exp);
	integer_type_free(float_class->mantissa);
	integer_type_free(float_class->sign);
	g_free(float_class);
}

static void _float_type_free(struct type_class *type_class)
{
	struct type_class_float *float_class =
		container_of(type_class, struct type_class_float, p);
	float_type_free(float_class);
}

struct type_class_float *float_type_new(const char *name,
					size_t mantissa_len,
					size_t exp_len, int byte_order,
					size_t alignment)
{
	struct type_class_float *float_class;
	struct type_class_integer *int_class;
	struct type_class *type_class;
	int ret;

	float_class = g_new(struct type_class_float, 1);
	type_class = &float_class->p;

	type_class->name = g_quark_from_string(name);
	type_class->alignment = alignment;
	type_class->copy = float_copy;
	type_class->free = _float_type_free;
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
		ret = ctf_register_type(&float_class->p);
		if (ret)
			goto error_register;
	}
	return float_class;

error_register:
	integer_type_free(float_class->exp);
error_exp:
	integer_type_free(float_class->mantissa);
error_mantissa:
	integer_type_free(float_class->sign);
error_sign:
	g_free(float_class);
	return NULL;
}
