#ifndef _BABELTRACE_CTF_TYPES_H
#define _BABELTRACE_CTF_TYPES_H

/*
 * Common Trace Format
 *
 * Type header
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

#include <babeltrace/types.h>
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
		const struct type_integer *integer_type);
int64_t ctf_int_read(struct stream_pos *pos,
		const struct type_integer *integer_type);
void ctf_uint_write(struct stream_pos *pos,
		const struct type_integer *integer_type,
		uint64_t v);
void ctf_int_write(struct stream_pos *pos,
		const struct type_integer *integer_type,
		int64_t v);

double ctf_double_read(struct stream_pos *pos,
			const struct type_float *src);
void ctf_double_write(struct stream_pos *pos,
		const struct type_float *dest,
		double v);
long double ctf_ldouble_read(struct stream_pos *pos,
			     const struct type_float *src);
void ctf_ldouble_write(struct stream_pos *pos,
		const struct type_float *dest,
		long double v);
void ctf_float_copy(struct stream_pos *destp,
		struct stream_pos *srcp,
		const struct type_float *float_type);

void ctf_string_copy(struct stream_pos *dest, struct stream_pos *src,
		     const struct type_string *string_type);
void ctf_string_read(char **dest, struct stream_pos *src,
		     const struct type_string *string_type);
void ctf_string_write(struct stream_pos *dest, const char *src,
		      const struct type_string *string_type);
void ctf_string_free_temp(char *string);

GArray *ctf_enum_read(struct stream_pos *pos,
		      const struct type_enum *src);
void ctf_enum_write(struct stream_pos *pos,
		const struct type_enum *dest,
		GQuark q);
void ctf_struct_begin(struct stream_pos *pos,
		      const struct type_struct *struct_type);
void ctf_struct_end(struct stream_pos *pos,
		    const struct type_struct *struct_type);
void ctf_variant_begin(struct stream_pos *pos,
		       const struct type_variant *variant_type);
void ctf_variant_end(struct stream_pos *pos,
		     const struct type_variant *variant_type);
void ctf_array_begin(struct stream_pos *pos,
		     const struct type_array *array_type);
void ctf_array_end(struct stream_pos *pos,
		   const struct type_array *array_type);
void ctf_sequence_begin(struct stream_pos *pos,
			const struct type_sequence *sequence_type);
void ctf_sequence_end(struct stream_pos *pos,
		      const struct type_sequence *sequence_type);

#endif /* _BABELTRACE_CTF_TYPES_H */
