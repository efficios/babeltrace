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
#include <sys/mman.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <glib.h>

struct packet_index {
	off_t offset;		/* offset of the packet in the file, in bytes */
	size_t packet_size;	/* packet size, in bits */
	size_t content_size;	/* content size, in bits */
};

/*
 * Always update stream_pos with move_pos and init_pos.
 */
struct stream_pos {
	int fd;			/* backing file fd. -1 if unset. */
	GArray *packet_index;	/* contains struct packet_index */
	int prot;		/* mmap protection */
	int flags;		/* mmap flags */

	/* Current position */
	off_t mmap_offset;	/* mmap offset in the file, in bytes */
	size_t packet_size;	/* current packet size, in bits */
	size_t content_size;	/* current content size, in bits */
	uint32_t *content_size_loc; /* pointer to current content size */
	char *base;		/* mmap base address */
	size_t offset;		/* offset from base, in bits */
	size_t cur_index;	/* current index in packet index */

	int dummy;		/* dummy position, for length calculation */
};

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

void move_pos_slow(struct stream_pos *pos, size_t offset);

void init_pos(struct stream_pos *pos, int fd);
void fini_pos(struct stream_pos *pos);

/*
 * move_pos - move position of a relative bit offset
 *
 * TODO: allow larger files by updating base too.
 */
static inline
void move_pos(struct stream_pos *pos, size_t bit_offset)
{
	if (pos->fd >= 0) {
		if (((pos->prot == PROT_READ)
		      && (pos->offset + bit_offset >= pos->content_size))
		    || ((pos->prot == PROT_WRITE)
		      && (pos->offset + bit_offset >= pos->packet_size))) {
			move_pos_slow(pos, bit_offset);
			return;
		}
	}
	pos->offset += bit_offset;
}

/*
 * align_pos - align position on a bit offset (> 0)
 *
 * TODO: allow larger files by updating base too.
 */
static inline
void align_pos(struct stream_pos *pos, size_t bit_offset)
{
	move_pos(pos, offset_align(pos->offset, bit_offset));
}

static inline
char *get_pos_addr(struct stream_pos *pos)
{
	/* Only makes sense to get the address after aligning on CHAR_BIT */
	assert(!(pos->offset % CHAR_BIT));
	return pos->base + (pos->offset / CHAR_BIT);
}

static inline
void dummy_pos(struct stream_pos *pos, struct stream_pos *dummy)
{
	memcpy(dummy, pos, sizeof(struct stream_pos));
	dummy->dummy = 1;
	dummy->fd = -1;
}

/*
 * Check if current packet can hold data.
 * Returns 0 for success, negative error otherwise.
 */
static inline
int pos_packet(struct stream_pos *dummy)
{
	if (dummy->offset > dummy->packet_size)
		return -ENOSPC;
	return 0;
}

static inline
void pos_pad_packet(struct stream_pos *pos)
{
	move_pos(pos, pos->packet_size - pos->offset);
}

#endif /* _BABELTRACE_CTF_TYPES_H */
