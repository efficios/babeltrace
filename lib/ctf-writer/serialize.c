/*
 * serialize.c
 *
 * Copyright 2010-2011 EfficiOS Inc. and Linux Foundation
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
 *
 * The original author of the serialization functions for Babeltrace 1
 * is Mathieu Desnoyers. Philippe Proulx modified the functions in 2017
 * to use Babeltrace 2 objects.
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

#define BT_LOG_TAG "CTF-WRITER-SERIALIZE"
#include <babeltrace/lib-logging-internal.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <babeltrace/ctf-writer/fields.h>
#include <babeltrace/ctf-writer/field-types.h>
#include <babeltrace/ctf-writer/serialize-internal.h>
#include <babeltrace/ctf-ir/utils-internal.h>
#include <babeltrace/ctf-ir/fields-internal.h>
#include <babeltrace/ctf-ir/field-types-internal.h>
#include <babeltrace/align-internal.h>
#include <babeltrace/mmap-align-internal.h>
#include <babeltrace/endian-internal.h>
#include <babeltrace/bitfield-internal.h>
#include <babeltrace/compat/fcntl-internal.h>
#include <babeltrace/types.h>
#include <babeltrace/common-internal.h>
#include <glib.h>

#if (FLT_RADIX != 2)
# error "Unsupported floating point radix"
#endif

union intval {
	int64_t signd;
	uint64_t unsignd;
};

/*
 * The aligned read/write functions are expected to be faster than the
 * bitfield variants. They will be enabled eventually as an
 * optimisation.
 */
static
int aligned_integer_write(struct bt_ctf_stream_pos *pos, union intval value,
		unsigned int alignment, unsigned int size, bt_bool is_signed,
		enum bt_ctf_byte_order byte_order)
{
	bt_bool rbo = ((int) byte_order != BT_MY_BYTE_ORDER); /* reverse byte order */

	if (!bt_ctf_stream_pos_align(pos, alignment))
		return -EFAULT;

	if (!bt_ctf_stream_pos_access_ok(pos, size))
		return -EFAULT;

	BT_ASSERT(!(pos->offset % CHAR_BIT));
	if (!is_signed) {
		switch (size) {
		case 8:
		{
			uint8_t v = value.unsignd;

			memcpy(bt_ctf_stream_pos_get_addr(pos), &v, sizeof(v));
			break;
		}
		case 16:
		{
			uint16_t v = value.unsignd;

			if (rbo)
				v = GUINT16_SWAP_LE_BE(v);
			memcpy(bt_ctf_stream_pos_get_addr(pos), &v, sizeof(v));
			break;
		}
		case 32:
		{
			uint32_t v = value.unsignd;

			if (rbo)
				v = GUINT32_SWAP_LE_BE(v);
			memcpy(bt_ctf_stream_pos_get_addr(pos), &v, sizeof(v));
			break;
		}
		case 64:
		{
			uint64_t v = value.unsignd;

			if (rbo)
				v = GUINT64_SWAP_LE_BE(v);
			memcpy(bt_ctf_stream_pos_get_addr(pos), &v, sizeof(v));
			break;
		}
		default:
			abort();
		}
	} else {
		switch (size) {
		case 8:
		{
			uint8_t v = value.signd;

			memcpy(bt_ctf_stream_pos_get_addr(pos), &v, sizeof(v));
			break;
		}
		case 16:
		{
			int16_t v = value.signd;

			if (rbo)
				v = GUINT16_SWAP_LE_BE(v);
			memcpy(bt_ctf_stream_pos_get_addr(pos), &v, sizeof(v));
			break;
		}
		case 32:
		{
			int32_t v = value.signd;

			if (rbo)
				v = GUINT32_SWAP_LE_BE(v);
			memcpy(bt_ctf_stream_pos_get_addr(pos), &v, sizeof(v));
			break;
		}
		case 64:
		{
			int64_t v = value.signd;

			if (rbo)
				v = GUINT64_SWAP_LE_BE(v);
			memcpy(bt_ctf_stream_pos_get_addr(pos), &v, sizeof(v));
			break;
		}
		default:
			abort();
		}
	}

	if (!bt_ctf_stream_pos_move(pos, size))
		return -EFAULT;
	return 0;
}

static
int integer_write(struct bt_ctf_stream_pos *pos, union intval value,
	unsigned int alignment, unsigned int size, bt_bool is_signed,
	enum bt_ctf_byte_order byte_order)
{
	if (!(alignment % CHAR_BIT)
	    && !(size % CHAR_BIT)) {
		return aligned_integer_write(pos, value, alignment,
			size, is_signed, byte_order);
	}

	if (!bt_ctf_stream_pos_align(pos, alignment))
		return -EFAULT;

	if (!bt_ctf_stream_pos_access_ok(pos, size))
		return -EFAULT;

	if (!is_signed) {
		if (byte_order == BT_CTF_BYTE_ORDER_LITTLE_ENDIAN)
			bt_bitfield_write_le(mmap_align_addr(pos->base_mma) +
				pos->mmap_base_offset, unsigned char,
				pos->offset, size, value.unsignd);
		else
			bt_bitfield_write_be(mmap_align_addr(pos->base_mma) +
				pos->mmap_base_offset, unsigned char,
				pos->offset, size, value.unsignd);
	} else {
		if (byte_order == BT_CTF_BYTE_ORDER_LITTLE_ENDIAN)
			bt_bitfield_write_le(mmap_align_addr(pos->base_mma) +
				pos->mmap_base_offset, unsigned char,
				pos->offset, size, value.signd);
		else
			bt_bitfield_write_be(mmap_align_addr(pos->base_mma) +
				pos->mmap_base_offset, unsigned char,
				pos->offset, size, value.signd);
	}

	if (!bt_ctf_stream_pos_move(pos, size))
		return -EFAULT;
	return 0;
}

BT_HIDDEN
int bt_ctf_field_integer_write(struct bt_field_common *field,
		struct bt_ctf_stream_pos *pos,
		enum bt_ctf_byte_order native_byte_order)
{
	struct bt_field_type_common_integer *int_type =
		BT_FROM_COMMON(field->type);
	struct bt_field_common_integer *int_field = BT_FROM_COMMON(field);
	enum bt_ctf_byte_order byte_order;
	union intval value;

	byte_order = (int) int_type->user_byte_order;
	if ((int) byte_order == BT_BYTE_ORDER_NATIVE) {
		byte_order = native_byte_order;
	}

	value.signd = int_field->payload.signd;
	value.unsignd = int_field->payload.unsignd;
	return integer_write(pos, value, int_type->common.alignment,
		int_type->size, int_type->is_signed,
		byte_order);
}

BT_HIDDEN
int bt_ctf_field_floating_point_write(
		struct bt_field_common *field,
		struct bt_ctf_stream_pos *pos,
		enum bt_ctf_byte_order native_byte_order)
{
	struct bt_field_type_common_floating_point *flt_type =
		BT_FROM_COMMON(field->type);
	struct bt_field_common_floating_point *flt_field = BT_FROM_COMMON(field);
	enum bt_ctf_byte_order byte_order;
	union intval value;
	unsigned int size;

	byte_order = (int) flt_type->user_byte_order;
	if ((int) byte_order == BT_BYTE_ORDER_NATIVE) {
		byte_order = native_byte_order;
	}

	if (flt_type->mant_dig == FLT_MANT_DIG) {
		union u32f {
			uint32_t u;
			float f;
		} u32f;

		u32f.f = (float) flt_field->payload;
		value.unsignd = u32f.u;
		size = 32;
	} else if (flt_type->mant_dig == DBL_MANT_DIG) {
		union u64d {
			uint64_t u;
			double d;
		} u64d;

		u64d.d = flt_field->payload;
		value.unsignd = u64d.u;
		size = 64;
	} else {
		return -EINVAL;
	}

	return integer_write(pos, value, flt_type->common.alignment,
		size, BT_FALSE, byte_order);
}

BT_HIDDEN
void bt_ctf_stream_pos_packet_seek(struct bt_ctf_stream_pos *pos, size_t index,
	int whence)
{
	int ret;

	BT_ASSERT(whence == SEEK_CUR && index == 0);

	if (pos->base_mma) {
		/* unmap old base */
		ret = munmap_align(pos->base_mma);
		if (ret) {
			// FIXME: this can legitimately fail?
			abort();
		}
		pos->base_mma = NULL;
	}

	/* The writer will add padding */
	pos->mmap_offset += pos->packet_size / CHAR_BIT;
	pos->packet_size = PACKET_LEN_INCREMENT;
	do {
		ret = bt_posix_fallocate(pos->fd, pos->mmap_offset,
			pos->packet_size / CHAR_BIT);
	} while (ret == EINTR);
	BT_ASSERT(ret == 0);
	pos->offset = 0;

	/* map new base. Need mapping length from header. */
	pos->base_mma = mmap_align(pos->packet_size / CHAR_BIT, pos->prot,
		pos->flags, pos->fd, pos->mmap_offset);
	if (pos->base_mma == MAP_FAILED) {
		// FIXME: this can legitimately fail?
		abort();
	}
}
