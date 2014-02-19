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
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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
#include <inttypes.h>
#include <babeltrace/mmap-align.h>

#define LAST_OFFSET_POISON	((int64_t) ~0ULL)

struct bt_stream_callbacks;

struct packet_index_time {
	uint64_t timestamp_begin;
	uint64_t timestamp_end;
};

struct packet_index {
	off_t offset;		/* offset of the packet in the file, in bytes */
	int64_t data_offset;	/* offset of data within the packet, in bits */
	uint64_t packet_size;	/* packet size, in bits */
	uint64_t content_size;	/* content size, in bits */
	uint64_t events_discarded;
	uint64_t events_discarded_len;	/* length of the field, in bits */
	struct packet_index_time ts_cycles;	/* timestamp in cycles */
	struct packet_index_time ts_real;	/* realtime timestamp */
};

/*
 * Always update ctf_stream_pos with ctf_move_pos and ctf_init_pos.
 */
struct ctf_stream_pos {
	struct bt_stream_pos parent;
	int fd;			/* backing file fd. -1 if unset. */
	FILE *index_fp;		/* backing index file fp. NULL if unset. */
	GArray *packet_index;	/* contains struct packet_index */
	int prot;		/* mmap protection */
	int flags;		/* mmap flags */

	/* Current position */
	off_t mmap_offset;	/* mmap offset in the file, in bytes */
	off_t mmap_base_offset;	/* offset of start of packet in mmap, in bytes */
	uint64_t packet_size;	/* current packet size, in bits */
	uint64_t content_size;	/* current content size, in bits */
	uint64_t *content_size_loc; /* pointer to current content size */
	struct mmap_align *base_mma;/* mmap base address */
	int64_t offset;		/* offset from base, in bits. EOF for end of file. */
	int64_t last_offset;	/* offset before the last read_event */
	int64_t data_offset;	/* offset of data in current packet */
	uint64_t cur_index;	/* current index in packet index */
	uint64_t last_events_discarded;	/* last known amount of event discarded */
	void (*packet_seek)(struct bt_stream_pos *pos, size_t index,
			int whence); /* function called to switch packet */

	int dummy;		/* dummy position, for length calculation */
	struct bt_stream_callbacks *cb;	/* Callbacks registered for iterator. */
	void *priv;
};

static inline
struct ctf_stream_pos *ctf_pos(struct bt_stream_pos *pos)
{
	return container_of(pos, struct ctf_stream_pos, parent);
}

BT_HIDDEN
int ctf_integer_read(struct bt_stream_pos *pos, struct bt_definition *definition);
BT_HIDDEN
int ctf_integer_write(struct bt_stream_pos *pos, struct bt_definition *definition);
BT_HIDDEN
int ctf_float_read(struct bt_stream_pos *pos, struct bt_definition *definition);
BT_HIDDEN
int ctf_float_write(struct bt_stream_pos *pos, struct bt_definition *definition);
BT_HIDDEN
int ctf_string_read(struct bt_stream_pos *pos, struct bt_definition *definition);
BT_HIDDEN
int ctf_string_write(struct bt_stream_pos *pos, struct bt_definition *definition);
BT_HIDDEN
int ctf_enum_read(struct bt_stream_pos *pos, struct bt_definition *definition);
BT_HIDDEN
int ctf_enum_write(struct bt_stream_pos *pos, struct bt_definition *definition);
BT_HIDDEN
int ctf_struct_rw(struct bt_stream_pos *pos, struct bt_definition *definition);
BT_HIDDEN
int ctf_variant_rw(struct bt_stream_pos *pos, struct bt_definition *definition);
BT_HIDDEN
int ctf_array_read(struct bt_stream_pos *pos, struct bt_definition *definition);
BT_HIDDEN
int ctf_array_write(struct bt_stream_pos *pos, struct bt_definition *definition);
BT_HIDDEN
int ctf_sequence_read(struct bt_stream_pos *pos, struct bt_definition *definition);
BT_HIDDEN
int ctf_sequence_write(struct bt_stream_pos *pos, struct bt_definition *definition);

void ctf_packet_seek(struct bt_stream_pos *pos, size_t index, int whence);

int ctf_init_pos(struct ctf_stream_pos *pos, struct bt_trace_descriptor *trace,
		int fd, int open_flags);
int ctf_fini_pos(struct ctf_stream_pos *pos);

/*
 * move_pos - move position of a relative bit offset
 *
 * Return 1 if OK, 0 if out-of-bound.
 *
 * TODO: allow larger files by updating base too.
 */
static inline
int ctf_move_pos(struct ctf_stream_pos *pos, uint64_t bit_offset)
{
	uint64_t max_len;

	printf_debug("ctf_move_pos test EOF: %" PRId64 "\n", pos->offset);
	if (unlikely(pos->offset == EOF))
		return 0;
	if (pos->prot == PROT_READ)
		max_len = pos->content_size;
	else
		max_len = pos->packet_size;
	if (unlikely(pos->offset + bit_offset > max_len))
		return 0;

	pos->offset += bit_offset;
	printf_debug("ctf_move_pos after increment: %" PRId64 "\n", pos->offset);
	return 1;
}

/*
 * align_pos - align position on a bit offset (> 0)
 *
 * Return 1 if OK, 0 if out-of-bound.
 *
 * TODO: allow larger files by updating base too.
 */
static inline
int ctf_align_pos(struct ctf_stream_pos *pos, uint64_t bit_offset)
{
	return ctf_move_pos(pos, offset_align(pos->offset, bit_offset));
}

static inline
char *ctf_get_pos_addr(struct ctf_stream_pos *pos)
{
	/* Only makes sense to get the address after aligning on CHAR_BIT */
	assert(!(pos->offset % CHAR_BIT));
	return mmap_align_addr(pos->base_mma) +
		pos->mmap_base_offset + (pos->offset / CHAR_BIT);
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
	ctf_packet_seek(&pos->parent, 0, SEEK_CUR);
}

static inline
int ctf_pos_access_ok(struct ctf_stream_pos *pos, uint64_t bit_len)
{
	uint64_t max_len;

	if (unlikely(pos->offset == EOF))
		return 0;
	if (pos->prot == PROT_READ)
		max_len = pos->content_size;
	else
		max_len = pos->packet_size;
	if (unlikely(pos->offset + bit_len > max_len))
		return 0;
	return 1;
}

/*
 * Update the stream position to the current event. This moves to
 * the next packet if we are located at the end of the current packet.
 */
static inline
void ctf_pos_get_event(struct ctf_stream_pos *pos)
{
	assert(pos->offset <= pos->content_size);
	if (pos->offset == pos->content_size) {
		printf_debug("ctf_packet_seek (before call): %" PRId64 "\n",
			     pos->offset);
		pos->packet_seek(&pos->parent, 0, SEEK_CUR);
		printf_debug("ctf_packet_seek (after call): %" PRId64 "\n",
			     pos->offset);
	}
}

void ctf_print_timestamp(FILE *fp, struct ctf_stream_definition *stream,
			uint64_t timestamp);
int ctf_append_trace_metadata(struct bt_trace_descriptor *tdp,
			FILE *metadata_fp);

#endif /* _BABELTRACE_CTF_TYPES_H */
