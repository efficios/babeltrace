#ifndef _BABELTRACE_CTF_TYPES_H
#define _BABELTRACE_CTF_TYPES_H

/*
 * Common Trace Format
 *
 * Type header
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

#include <stdint.h>
#include <glib.h>

/*
 * IMPORTANT: All lengths (len) and offsets (start, end) are expressed in bits,
 *            *not* in bytes.
 *
 * All write primitives, as well as read for dynamically sized entities, can
 * receive a NULL ptr/dest parameter. In this case, no write is performed, but
 * the size is returned.
 */

uint64_t ctf_uint_read(struct stream_pos *pos,
		const struct type_class_integer *int_class);
int64_t ctf_int_read(struct stream_pos *pos,
		const struct type_class_integer *int_class);
void ctf_uint_write(struct stream_pos *pos,
		const struct type_class_integer *int_class,
		uint64_t v);
void ctf_int_write(struct stream_pos *pos,
		const struct type_class_integer *int_class,
		int64_t v);

double ctf_double_read(struct stream_pos *pos,
			const struct type_class_float *src);
void ctf_double_write(struct stream_pos *pos,
		const struct type_class_float *dest,
		double v);
long double ctf_ldouble_read(struct stream_pos *pos,
			     const struct type_class_float *src);
void ctf_ldouble_write(struct stream_pos *pos,
		const struct type_class_float *dest,
		long double v);
void ctf_float_copy(struct stream_pos *destp, const struct type_class_float *dest,
		struct stream_pos *srcp, const struct type_class_float *src);

void ctf_string_copy(struct stream_pos *dest, struct stream_pos *src,
		     const struct type_class_string *string_class);
void ctf_string_read(unsigned char **dest, struct stream_pos *src,
		     const struct type_class_string *string_class);
void ctf_string_write(struct stream_pos *dest, const unsigned char *src,
		      const struct type_class_string *string_class);
void ctf_string_free_temp(unsigned char *string);

GQuark ctf_enum_read(struct stream_pos *pos,
		const struct type_class_enum *src);
void ctf_enum_write(struct stream_pos *pos,
		const struct type_class_enum *dest,
		GQuark q);
void ctf_struct_begin(struct stream_pos *pos,
		      const struct type_class_struct *struct_class);
void ctf_struct_end(struct stream_pos *pos,
		    const struct type_class_struct *struct_class);
void ctf_array_begin(struct stream_pos *pos,
		     const struct type_class_array *array_class);
void ctf_array_end(struct stream_pos *pos,
		   const struct type_class_array *array_class);
void ctf_sequence_begin(struct stream_pos *pos,
			const struct type_class_sequence *sequence_class);
void ctf_sequence_end(struct stream_pos *pos,
		      const struct type_class_sequence *sequence_class);

#endif /* _BABELTRACE_CTF_TYPES_H */
