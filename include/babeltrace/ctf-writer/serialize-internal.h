#ifndef BABELTRACE_CTF_WRITER_SERIALIZE_INTERNAL_H
#define BABELTRACE_CTF_WRITER_SERIALIZE_INTERNAL_H

/*
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
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
 *
 * The Common Trace Format (CTF) Specification is available at
 * http://www.efficios.com/ctf
 */

#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <babeltrace/compat/mman-internal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <babeltrace/align-internal.h>
#include <babeltrace/common-internal.h>
#include <babeltrace/mmap-align-internal.h>
#include <babeltrace/types.h>
#include <babeltrace/ctf-writer/fields.h>
#include <babeltrace/ctf-writer/field-types.h>
#include <babeltrace/ctf-ir/fields-internal.h>
#include <babeltrace/assert-internal.h>

#define PACKET_LEN_INCREMENT	(bt_common_get_page_size() * 8 * CHAR_BIT)

struct bt_ctf_stream_pos {
	int fd;
	int prot;		/* mmap protection */
	int flags;		/* mmap flags */

	/* Current position */
	off_t mmap_offset;	/* mmap offset in the file, in bytes */
	off_t mmap_base_offset;	/* offset of start of packet in mmap, in bytes */
	uint64_t packet_size;	/* current packet size, in bits */
	int64_t offset;		/* offset from base, in bits. EOF for end of file. */
	struct mmap_align *base_mma;/* mmap base address */
};

BT_HIDDEN
int bt_ctf_field_integer_write(struct bt_field_common *field,
		struct bt_ctf_stream_pos *pos,
		enum bt_ctf_byte_order native_byte_order);

BT_HIDDEN
int bt_ctf_field_floating_point_write(struct bt_field_common *field,
		struct bt_ctf_stream_pos *pos,
		enum bt_ctf_byte_order native_byte_order);

static inline
int bt_ctf_stream_pos_access_ok(struct bt_ctf_stream_pos *pos, uint64_t bit_len)
{
	uint64_t max_len;

	if (unlikely(pos->offset == EOF))
		return 0;

	if (pos->prot == PROT_READ) {
		/*
		 * Reads may only reach up to the "content_size",
		 * regardless of the packet_size.
		 */
		max_len = pos->offset;
	} else {
		/* Writes may take place up to the end of the packet. */
		max_len = pos->packet_size;
	}
	if (unlikely(pos->offset < 0 || bit_len > INT64_MAX - pos->offset)) {
		return 0;
	}
	if (unlikely(pos->offset + bit_len > max_len))
		return 0;
	return 1;
}

static inline
int bt_ctf_stream_pos_move(struct bt_ctf_stream_pos *pos, uint64_t bit_offset)
{
	int ret = 0;

	ret = bt_ctf_stream_pos_access_ok(pos, bit_offset);
	if (!ret) {
		goto end;
	}
	pos->offset += bit_offset;
end:
	return ret;
}

static inline
int bt_ctf_stream_pos_align(struct bt_ctf_stream_pos *pos, uint64_t bit_offset)
{
	return bt_ctf_stream_pos_move(pos,
		offset_align(pos->offset, bit_offset));
}

static inline
char *bt_ctf_stream_pos_get_addr(struct bt_ctf_stream_pos *pos)
{
	/* Only makes sense to get the address after aligning on CHAR_BIT */
	BT_ASSERT(!(pos->offset % CHAR_BIT));
	return ((char *) mmap_align_addr(pos->base_mma)) +
		pos->mmap_base_offset + (pos->offset / CHAR_BIT);
}

static inline
int bt_ctf_stream_pos_init(struct bt_ctf_stream_pos *pos,
		int fd, int open_flags)
{
	pos->fd = fd;

	switch (open_flags & O_ACCMODE) {
	case O_RDONLY:
		pos->prot = PROT_READ;
		pos->flags = MAP_PRIVATE;
		break;
	case O_RDWR:
		pos->prot = PROT_READ | PROT_WRITE;
		pos->flags = MAP_SHARED;
		break;
	default:
		abort();
	}

	return 0;
}

static inline
int bt_ctf_stream_pos_fini(struct bt_ctf_stream_pos *pos)
{
	if (pos->base_mma) {
		int ret;

		/* unmap old base */
		ret = munmap_align(pos->base_mma);
		if (ret) {
			return -1;
		}
	}

	return 0;
}

BT_HIDDEN
void bt_ctf_stream_pos_packet_seek(struct bt_ctf_stream_pos *pos, size_t index,
	int whence);

#endif /* BABELTRACE_CTF_WRITER_SERIALIZE_INTERNAL_H */
