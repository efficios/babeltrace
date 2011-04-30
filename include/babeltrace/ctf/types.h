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
 * Always update stream_pos with move_pos and init_pos.
 */
struct stream_pos {
	char *base;		/* Base address */
	size_t offset;		/* Offset from base, in bits */
	int dummy;		/* Dummy position, for length calculation */
};

static inline
void init_pos(struct stream_pos *pos, char *base)
{
	pos->base = base;	/* initial base, page-aligned */
	pos->offset = 0;
	pos->dummy = false;
}

/*
 * move_pos - move position of a relative bit offset
 *
 * TODO: allow larger files by updating base too.
 */
static inline
void move_pos(struct stream_pos *pos, size_t offset)
{
	pos->offset = pos->offset + offset;
}

/*
 * align_pos - align position on a bit offset (> 0)
 *
 * TODO: allow larger files by updating base too.
 */
static inline
void align_pos(struct stream_pos *pos, size_t offset)
{
	pos->offset += offset_align(pos->offset, offset);
}

static inline
void copy_pos(struct stream_pos *dest, struct stream_pos *src)
{
	memcpy(dest, src, sizeof(struct stream_pos));
}

static inline
char *get_pos_addr(struct stream_pos *pos)
{
	/* Only makes sense to get the address after aligning on CHAR_BIT */
	assert(!(pos->offset % CHAR_BIT));
	return pos->base + (pos->offset / CHAR_BIT);
}

/*
 * IMPORTANT: All lengths (len) and offsets (start, end) are expressed in bits,
 *            *not* in bytes.
 *
 * All write primitives, as well as read for dynamically sized entities, can
 * receive a NULL ptr/dest parameter. In this case, no write is performed, but
 * the size is returned.
 */

uint64_t ctf_uint_read(struct stream_pos *pos,
		const struct declaration_integer *integer_declaration);
int64_t ctf_int_read(struct stream_pos *pos,
		const struct declaration_integer *integer_declaration);
void ctf_uint_write(struct stream_pos *pos,
		const struct declaration_integer *integer_declaration,
		uint64_t v);
void ctf_int_write(struct stream_pos *pos,
		const struct declaration_integer *integer_declaration,
		int64_t v);

double ctf_double_read(struct stream_pos *pos,
			const struct declaration_float *src);
void ctf_double_write(struct stream_pos *pos,
		const struct declaration_float *dest,
		double v);
long double ctf_ldouble_read(struct stream_pos *pos,
			     const struct declaration_float *src);
void ctf_ldouble_write(struct stream_pos *pos,
		const struct declaration_float *dest,
		long double v);
void ctf_float_copy(struct stream_pos *destp,
		struct stream_pos *srcp,
		const struct declaration_float *float_declaration);

void ctf_string_copy(struct stream_pos *dest, struct stream_pos *src,
		const struct declaration_string *string_declaration);
void ctf_string_read(char **dest, struct stream_pos *src,
		const struct declaration_string *string_declaration);
void ctf_string_write(struct stream_pos *dest, const char *src,
		const struct declaration_string *string_declaration);
void ctf_string_free_temp(char *string);

GArray *ctf_enum_read(struct stream_pos *pos,
		const struct declaration_enum *src);
void ctf_enum_write(struct stream_pos *pos,
		const struct declaration_enum *dest,
		GQuark q);
void ctf_struct_begin(struct stream_pos *pos,
		const struct declaration_struct *struct_declaration);
void ctf_struct_end(struct stream_pos *pos,
		const struct declaration_struct *struct_declaration);
void ctf_variant_begin(struct stream_pos *pos,
		const struct declaration_variant *variant_declaration);
void ctf_variant_end(struct stream_pos *pos,
		const struct declaration_variant *variant_declaration);
void ctf_array_begin(struct stream_pos *pos,
		const struct declaration_array *array_declaration);
void ctf_array_end(struct stream_pos *pos,
		const struct declaration_array *array_declaration);
void ctf_sequence_begin(struct stream_pos *pos,
		const struct declaration_sequence *sequence_declaration);
void ctf_sequence_end(struct stream_pos *pos,
		const struct declaration_sequence *sequence_declaration);

#endif /* _BABELTRACE_CTF_TYPES_H */
