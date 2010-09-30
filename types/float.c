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

size_t float_copy(unsigned char *dest, const struct format *fdest, 
		  const unsigned char *src, const struct format *fsrc,
		  const struct type_class *type_class)
{
	struct type_class_float *float_class =
		container_of(type_class, struct type_class_float, p);

	if (fsrc->float_copy == fdest->float_copy) {
		fsrc->float_copy(dest, float_class, src, float_class);
		return float_class->mantissa_len + float_class->exp_len;
	} else {
		double v;

		v = fsrc->double_read(src, fsrc);
		return fdest->double_write(dest, fdest, v);
	}
}

struct type_class_float *float_type_new(const char *name,
					size_t mantissa_len,
					size_t exp_len, int byte_order,
					size_t alignment)
{
	struct type_class_float *float_class;
	int ret;

	/*
	 * Freed when type is unregistered.
	 */
	float_class = g_new(struct type_class_float, 1);
	float_class->p.name = g_quark_from_string(name);
	float_class->p.alignment = alignment;
	float_class->mantissa_len = mantissa_len;
	float_class->exp_len = exp_len;
	float_class->byte_order = byte_order;
	if (float_class->p.name) {
		ret = ctf_register_type(&float_class->p);
		if (ret) {
			g_free(float_class);
			return NULL;
		}
	}
	return float_class;
}

void float_type_free(struct type_class_float *float_class)
{
	if (!float_class->name)
		g_free(float_class);
}
