/*
 * BabelTrace - Float Type Converter
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
	struct type_class_bitfield *bitfield_class;
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

	float_class->sign = bitfield_type_new(NULL, 1,
					      byte_order, false, 1);
	if (!float_class->mantissa)
		goto error_sign;
	float_class->mantissa = bitfield_type_new(NULL, mantissa_len - 1,
						  byte_order, false, 1);
	if (!float_class->mantissa)
		goto error_mantissa;
	float_class->exp = bitfield_type_new(NULL, exp_len,
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
	bitfield_type_free(float_class->exp);
error_exp:
	bitfield_type_free(float_class->mantissa);
error_mantissa:
	bitfield_type_free(float_class->sign);
error_sign:
	g_free(float_class);
	return NULL;
}
