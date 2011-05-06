#ifndef _BABELTRACE_CTF_TEXT_TYPES_H
#define _BABELTRACE_CTF_TEXT_TYPES_H

/*
 * Common Trace Format (Text Output)
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

#include <sys/mman.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <glib.h>
#include <babeltrace/babeltrace.h>
#include <babeltrace/types.h>

struct ctf_text_stream_pos {
	struct trace_descriptor parent;
	FILE *fp;		/* File pointer. NULL if unset. */
};

/*
 * IMPORTANT: All lengths (len) and offsets (start, end) are expressed in bits,
 *            *not* in bytes.
 *
 * All write primitives, as well as read for dynamically sized entities, can
 * receive a NULL ptr/dest parameter. In this case, no write is performed, but
 * the size is returned.
 */

void ctf_text_uint_write(struct stream_pos *pos,
		const struct declaration_integer *integer_declaration,
		uint64_t v);
void ctf_text_int_write(struct stream_pos *pos,
		const struct declaration_integer *integer_declaration,
		int64_t v);

void ctf_text_double_write(struct stream_pos *pos,
		const struct declaration_float *dest,
		double v);
void ctf_text_ldouble_write(struct stream_pos *pos,
		const struct declaration_float *dest,
		long double v);

void ctf_text_string_write(struct stream_pos *dest, const char *src,
		const struct declaration_string *string_declaration);

void ctf_text_enum_write(struct stream_pos *pos,
		const struct declaration_enum *dest,
		GQuark q);
void ctf_text_struct_begin(struct stream_pos *pos,
		const struct declaration_struct *struct_declaration);
void ctf_text_struct_end(struct stream_pos *pos,
		const struct declaration_struct *struct_declaration);
void ctf_text_variant_begin(struct stream_pos *pos,
		const struct declaration_variant *variant_declaration);
void ctf_text_variant_end(struct stream_pos *pos,
		const struct declaration_variant *variant_declaration);
void ctf_text_array_begin(struct stream_pos *pos,
		const struct declaration_array *array_declaration);
void ctf_text_array_end(struct stream_pos *pos,
		const struct declaration_array *array_declaration);
void ctf_text_sequence_begin(struct stream_pos *pos,
		const struct declaration_sequence *sequence_declaration);
void ctf_text_sequence_end(struct stream_pos *pos,
		const struct declaration_sequence *sequence_declaration);

#endif /* _BABELTRACE_CTF_TEXT_TYPES_H */
