/*
 * Common Trace Format
 *
 * Integers read/write functions.
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

#include <ctf/ctf-types.h>
#include <ctf/bitfield.h>
#include <stdint.h>
#include <glib.h>
#include <endian.h>

static
uint64_t _aligned_uint_read(struct stream_pos *pos,
		       const struct type_class_integer *int_class)
{
	int rbo = (int_class->byte_order != BYTE_ORDER);	/* reverse byte order */

	align_pos(pos, int_class->p.alignment);
	assert(!(pos->offset % CHAR_BIT));
	switch (int_class->len) {
	case 8:
	{
		uint8_t v;

		v = *(const uint8_t *)pos->base;
		move_pos(pos, int_class->len);
		return v;
	}
	case 16:
	{
		uint16_t v;

		v = *(const uint16_t *)pos->base;
		move_pos(pos, int_class->len);
		return rbo ? GUINT16_SWAP_LE_BE(v) : v;
	}
	case 32:
	{
		uint32_t v;

		v = *(const uint32_t *)pos->base;
		move_pos(pos, int_class->len);
		return rbo ? GUINT32_SWAP_LE_BE(v) : v;
	}
	case 64:
	{
		uint64_t v;

		v = *(const uint64_t *)pos->base;
		move_pos(pos, int_class->len);
		return rbo ? GUINT64_SWAP_LE_BE(v) : v;
	}
	default:
		assert(0);
	}
}

static
int64_t _aligned_int_read(struct stream_pos *pos,
		     const struct type_class_integer *int_class)
{
	int rbo = (int_class->byte_order != BYTE_ORDER);	/* reverse byte order */

	align_pos(pos, int_class->p.alignment);
	assert(!(pos->offset % CHAR_BIT));
	switch (int_class->len) {
	case 8:
	{
		int8_t v;

		v = *(const int8_t *)pos->base;
		move_pos(pos, int_class->len);
		return v;
	}
	case 16:
	{
		int16_t v;

		v = *(const int16_t *)pos->base;
		move_pos(pos, int_class->len);
		return rbo ? GUINT16_SWAP_LE_BE(v) : v;
	}
	case 32:
	{
		int32_t v;

		v = *(const int32_t *)pos->base;
		move_pos(pos, int_class->len);
		return rbo ? GUINT32_SWAP_LE_BE(v) : v;
	}
	case 64:
	{
		int64_t v;

		v = *(const int64_t *)pos->base;
		move_pos(pos, int_class->len);
		return rbo ? GUINT64_SWAP_LE_BE(v) : v;
	}
	default:
		assert(0);
	}
}

static
void _aligned_uint_write(struct stream_pos *pos,
		    const struct type_class_integer *int_class,
		    uint64_t v)
{
	int rbo = (int_class->byte_order != BYTE_ORDER);	/* reverse byte order */

	align_pos(pos, int_class->p.alignment);
	assert(!(pos->offset % CHAR_BIT));
	if (pos->dummy)
		goto end;

	switch (int_class->len) {
	case 8:	*(uint8_t *) get_pos_addr(pos) = (uint8_t) v;
		break;
	case 16:
		*(uint16_t *) get_pos_addr(pos) = rbo ?
					 GUINT16_SWAP_LE_BE((uint16_t) v) :
					 (uint16_t) v;
		break;
	case 32:
		*(uint32_t *) get_pos_addr(pos) = rbo ?
					 GUINT32_SWAP_LE_BE((uint32_t) v) :
					 (uint32_t) v;
		break;
	case 64:
		*(uint64_t *) get_pos_addr(pos) = rbo ?
					 GUINT64_SWAP_LE_BE(v) : v;
		break;
	default:
		assert(0);
	}
end:
	move_pos(pos, int_class->len);
}

static
void _aligned_int_write(struct stream_pos *pos,
		   const struct type_class_integer *int_class,
		   int64_t v)
{
	int rbo = (int_class->byte_order != BYTE_ORDER);	/* reverse byte order */

	align_pos(pos, int_class->p.alignment);
	assert(!(pos->offset % CHAR_BIT));
	if (pos->dummy)
		goto end;

	switch (int_class->len) {
	case 8:	*(int8_t *) get_pos_addr(pos) = (int8_t) v;
		break;
	case 16:
		*(int16_t *) get_pos_addr(pos) = rbo ?
					 GUINT16_SWAP_LE_BE((int16_t) v) :
					 (int16_t) v;
		break;
	case 32:
		*(int32_t *) get_pos_addr(pos) = rbo ?
					 GUINT32_SWAP_LE_BE((int32_t) v) :
					 (int32_t) v;
		break;
	case 64:
		*(int64_t *) get_pos_addr(pos) = rbo ?
					 GUINT64_SWAP_LE_BE(v) : v;
		break;
	default:
		assert(0);
	}
end:
	move_pos(pos, int_class->len);
	return;
}

uint64_t ctf_uint_read(struct stream_pos *pos,
			const struct type_class_bitfield *int_class)
{
	uint64_t v;

	align_pos(pos, int_class->p.alignment);
	if (int_class->byte_order == LITTLE_ENDIAN)
		ctf_bitfield_read_le(pos->base, pos->offset,
				     int_class->len, &v);
	else
		ctf_bitfield_read_be(pos->base, pos->offset,
				     int_class->len, &v);
	move_pos(pos, int_class->len);
	return v;
}

int64_t ctf_int_read(struct stream_pos *pos,
			const struct type_class_bitfield *int_class)
{
	int64_t v;

	align_pos(pos, int_class->p.alignment);
	if (int_class->byte_order == LITTLE_ENDIAN)
		ctf_bitfield_read_le(pos->base, pos->offset,
				     int_class->len, &v);
	else
		ctf_bitfield_read_be(pos->base, pos->offset,
				     int_class->len, &v);
	move_pos(pos, int_class->len);
	return v;
}

void ctf_uint_write(struct stream_pos *pos,
			const struct type_class_bitfield *int_class,
			uint64_t v)
{
	align_pos(pos, int_class->p.alignment);
	if (pos->dummy)
		goto end;
	if (int_class->byte_order == LITTLE_ENDIAN)
		ctf_bitfield_write_le(pos->base, pos->offset,
				      int_class->len, v);
	else
		ctf_bitfield_write_be(pos->base, pos->offset,
				      int_class->len,, v);
end:
	move_pos(pos, int_class->len);
}

void ctf_int_write(struct stream_pos *pos,
			const struct type_class_bitfield *int_class,
			int64_t v)
{
	align_pos(pos, int_class->p.alignment);
	if (pos->dummy)
		goto end;
	if (int_class->byte_order == LITTLE_ENDIAN)
		ctf_bitfield_write_le(pos->base, pos->offset,
				      int_class->len, v);
	else
		ctf_bitfield_write_be(pos->base, pos->offset,
				      int_class->len, v);
end:
	move_pos(pos, int_class->len);
}
