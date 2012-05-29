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
#include <babeltrace/babeltrace-internal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <glib.h>
#include <stdio.h>
#include <babeltrace/mmap-align.h>

struct bt_stream_callbacks;

struct packet_index {
	off_t offset;		/* offset of the packet in the file, in bytes */
	off_t data_offset;	/* offset of data within the packet, in bits */
	uint64_t packet_size;	/* packet size, in bits */
	uint64_t content_size;	/* content size, in bits */
	uint64_t timestamp_begin;
	uint64_t timestamp_end;
	uint64_t events_discarded;
	size_t events_discarded_len;	/* length of the field, in bits */
};

/*
 * Always update ctf_stream_pos with ctf_move_pos and ctf_init_pos.
 */
struct ctf_stream_pos {
	struct stream_pos parent;
	int fd;			/* backing file fd. -1 if unset. */
	GArray *packet_index;	/* contains struct packet_index */
	int prot;		/* mmap protection */
	int flags;		/* mmap flags */

	/* Current position */
	off_t mmap_offset;	/* mmap offset in the file, in bytes */
	size_t packet_size;	/* current packet size, in bits */
	size_t content_size;	/* current content size, in bits */
	uint32_t *content_size_loc; /* pointer to current content size */
	struct mmap_align *base_mma;/* mmap base address */
	ssize_t offset;		/* offset from base, in bits. EOF for end of file. */
	ssize_t last_offset;	/* offset before the last read_event */
	size_t cur_index;	/* current index in packet index */
	void (*packet_seek)(struct stream_pos *pos, size_t index,
			int whence); /* function called to switch packet */

	int dummy;		/* dummy position, for length calculation */
	struct bt_stream_callbacks *cb;	/* Callbacks registered for iterator. */
};

static inline
struct ctf_stream_pos *ctf_pos(struct stream_pos *pos)
{
	return container_of(pos, struct ctf_stream_pos, parent);
}

int ctf_integer_read(struct stream_pos *pos, struct definition *definition);
int ctf_integer_write(struct stream_pos *pos, struct definition *definition);
int ctf_float_read(struct stream_pos *pos, struct definition *definition);
int ctf_float_write(struct stream_pos *pos, struct definition *definition);
int ctf_string_read(struct stream_pos *pos, struct definition *definition);
int ctf_string_write(struct stream_pos *pos, struct definition *definition);
int ctf_enum_read(struct stream_pos *pos, struct definition *definition);
int ctf_enum_write(struct stream_pos *pos, struct definition *definition);
int ctf_struct_rw(struct stream_pos *pos, struct definition *definition);
int ctf_variant_rw(struct stream_pos *pos, struct definition *definition);
int ctf_array_read(struct stream_pos *pos, struct definition *definition);
int ctf_array_write(struct stream_pos *pos, struct definition *definition);
int ctf_sequence_read(struct stream_pos *pos, struct definition *definition);
int ctf_sequence_write(struct stream_pos *pos, struct definition *definition);

void ctf_packet_seek(struct stream_pos *pos, size_t index, int whence);

void ctf_init_pos(struct ctf_stream_pos *pos, int fd, int open_flags);
void ctf_fini_pos(struct ctf_stream_pos *pos);

/*
 * move_pos - move position of a relative bit offset
 *
 * TODO: allow larger files by updating base too.
 */
static inline
void ctf_move_pos(struct ctf_stream_pos *pos, size_t bit_offset)
{
	printf_debug("ctf_move_pos test EOF: %zd\n", pos->offset);
	if (unlikely(pos->offset == EOF))
		return;

	if (pos->fd >= 0) {
		/*
		 * PROT_READ ctf_packet_seek is called from within
		 * ctf_pos_get_event so end of packet does not change
		 * the packet context on for the last event of the
		 * packet.
		 */
		if ((pos->prot == PROT_WRITE)
		    	&& (unlikely(pos->offset + bit_offset >= pos->packet_size))) {
			printf_debug("ctf_packet_seek (before call): %zd\n",
				     pos->offset);
			ctf_packet_seek(&pos->parent, 0, SEEK_CUR);
			printf_debug("ctf_packet_seek (after call): %zd\n",
				     pos->offset);
			return;
		}
	}
	pos->offset += bit_offset;
	printf_debug("ctf_move_pos after increment: %zd\n", pos->offset);
}

/*
 * align_pos - align position on a bit offset (> 0)
 *
 * TODO: allow larger files by updating base too.
 */
static inline
void ctf_align_pos(struct ctf_stream_pos *pos, size_t bit_offset)
{
	ctf_move_pos(pos, offset_align(pos->offset, bit_offset));
}

static inline
char *ctf_get_pos_addr(struct ctf_stream_pos *pos)
{
	/* Only makes sense to get the address after aligning on CHAR_BIT */
	assert(!(pos->offset % CHAR_BIT));
	return mmap_align_addr(pos->base_mma) + (pos->offset / CHAR_BIT);
}

static inline
void ctf_dummy_pos(struct ctf_stream_pos *pos, struct ctf_stream_pos *dummy)
{
	memcpy(dummy, pos, sizeof(struct ctf_stream_pos));
	dummy->dummy = 1;
	dummy->fd = -1;
}

/*
 * Check if current packet can hold data.
 * Returns 0 for success, negative error otherwise.
 */
static inline
int ctf_pos_packet(struct ctf_stream_pos *dummy)
{
	if (unlikely(dummy->offset > dummy->packet_size))
		return -ENOSPC;
	return 0;
}

static inline
void ctf_pos_pad_packet(struct ctf_stream_pos *pos)
{
	ctf_move_pos(pos, pos->packet_size - pos->offset);
}

static inline
int ctf_pos_access_ok(struct ctf_stream_pos *pos, size_t bit_len)
{
	if (unlikely(pos->offset == EOF))
		return 0;
	if (unlikely(pos->offset + bit_len > pos->packet_size))
		return 0;
	return 1;
}

/*
 * Update the stream position for to the current event. This moves to
 * the next packet if we are located at the end of the current packet.
 */
static inline
void ctf_pos_get_event(struct ctf_stream_pos *pos)
{
	assert(pos->offset <= pos->content_size);
	if (pos->offset == pos->content_size) {
		printf_debug("ctf_packet_seek (before call): %zd\n",
			     pos->offset);
		pos->packet_seek(&pos->parent, 0, SEEK_CUR);
		printf_debug("ctf_packet_seek (after call): %zd\n",
			     pos->offset);
	}
}

void ctf_print_timestamp(FILE *fp, struct ctf_stream_definition *stream,
			uint64_t timestamp);

#endif /* _BABELTRACE_CTF_TYPES_H */
