/*
 * Common Trace Format
 *
 * Integers read/write functions.
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

#include <babeltrace/ctf/types.h>
#include <babeltrace/bitfield.h>
#include <stdint.h>
#include <glib.h>
#include <endian.h>

static
uint64_t _aligned_uint_read(struct stream_pos *pos,
		       const struct type_integer *integer_type)
{
	int rbo = (integer_type->byte_order != BYTE_ORDER);	/* reverse byte order */

	align_pos(pos, integer_type->p.alignment);
	assert(!(pos->offset % CHAR_BIT));
	switch (integer_type->len) {
	case 8:
	{
		uint8_t v;

		v = *(const uint8_t *)pos->base;
		move_pos(pos, integer_type->len);
		return v;
	}
	case 16:
	{
		uint16_t v;

		v = *(const uint16_t *)pos->base;
		move_pos(pos, integer_type->len);
		return rbo ? GUINT16_SWAP_LE_BE(v) : v;
	}
	case 32:
	{
		uint32_t v;

		v = *(const uint32_t *)pos->base;
		move_pos(pos, integer_type->len);
		return rbo ? GUINT32_SWAP_LE_BE(v) : v;
	}
	case 64:
	{
		uint64_t v;

		v = *(const uint64_t *)pos->base;
		move_pos(pos, integer_type->len);
		return rbo ? GUINT64_SWAP_LE_BE(v) : v;
	}
	default:
		assert(0);
	}
}

static
int64_t _aligned_int_read(struct stream_pos *pos,
		     const struct type_integer *integer_type)
{
	int rbo = (integer_type->byte_order != BYTE_ORDER);	/* reverse byte order */

	align_pos(pos, integer_type->p.alignment);
	assert(!(pos->offset % CHAR_BIT));
	switch (integer_type->len) {
	case 8:
	{
		int8_t v;

		v = *(const int8_t *)pos->base;
		move_pos(pos, integer_type->len);
		return v;
	}
	case 16:
	{
		int16_t v;

		v = *(const int16_t *)pos->base;
		move_pos(pos, integer_type->len);
		return rbo ? GUINT16_SWAP_LE_BE(v) : v;
	}
	case 32:
	{
		int32_t v;

		v = *(const int32_t *)pos->base;
		move_pos(pos, integer_type->len);
		return rbo ? GUINT32_SWAP_LE_BE(v) : v;
	}
	case 64:
	{
		int64_t v;

		v = *(const int64_t *)pos->base;
		move_pos(pos, integer_type->len);
		return rbo ? GUINT64_SWAP_LE_BE(v) : v;
	}
	default:
		assert(0);
	}
}

static
void _aligned_uint_write(struct stream_pos *pos,
		    const struct type_integer *integer_type,
		    uint64_t v)
{
	int rbo = (integer_type->byte_order != BYTE_ORDER);	/* reverse byte order */

	align_pos(pos, integer_type->p.alignment);
	assert(!(pos->offset % CHAR_BIT));
	if (pos->dummy)
		goto end;

	switch (integer_type->len) {
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
	move_pos(pos, integer_type->len);
}

static
void _aligned_int_write(struct stream_pos *pos,
		   const struct type_integer *integer_type,
		   int64_t v)
{
	int rbo = (integer_type->byte_order != BYTE_ORDER);	/* reverse byte order */

	align_pos(pos, integer_type->p.alignment);
	assert(!(pos->offset % CHAR_BIT));
	if (pos->dummy)
		goto end;

	switch (integer_type->len) {
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
	move_pos(pos, integer_type->len);
	return;
}

uint64_t ctf_uint_read(struct stream_pos *pos,
			const struct type_integer *integer_type)
{
	uint64_t v = 0;

	align_pos(pos, integer_type->p.alignment);
	if (integer_type->byte_order == LITTLE_ENDIAN)
		bt_bitfield_read_le(pos->base, unsigned long, pos->offset,
				    integer_type->len, &v);
	else
		bt_bitfield_read_be(pos->base, unsigned long, pos->offset,
				    integer_type->len, &v);
	move_pos(pos, integer_type->len);
	return v;
}

int64_t ctf_int_read(struct stream_pos *pos,
			const struct type_integer *integer_type)
{
	int64_t v = 0;

	align_pos(pos, integer_type->p.alignment);
	if (integer_type->byte_order == LITTLE_ENDIAN)
		bt_bitfield_read_le(pos->base, unsigned long, pos->offset,
				    integer_type->len, &v);
	else
		bt_bitfield_read_be(pos->base, unsigned long, pos->offset,
				    integer_type->len, &v);
	move_pos(pos, integer_type->len);
	return v;
}

void ctf_uint_write(struct stream_pos *pos,
			const struct type_integer *integer_type,
			uint64_t v)
{
	align_pos(pos, integer_type->p.alignment);
	if (pos->dummy)
		goto end;
	if (integer_type->byte_order == LITTLE_ENDIAN)
		bt_bitfield_write_le(pos->base, unsigned long, pos->offset,
				     integer_type->len, v);
	else
		bt_bitfield_write_be(pos->base, unsigned long, pos->offset,
				     integer_type->len, v);
end:
	move_pos(pos, integer_type->len);
}

void ctf_int_write(struct stream_pos *pos,
			const struct type_integer *integer_type,
			int64_t v)
{
	align_pos(pos, integer_type->p.alignment);
	if (pos->dummy)
		goto end;
	if (integer_type->byte_order == LITTLE_ENDIAN)
		bt_bitfield_write_le(pos->base, unsigned long, pos->offset,
				     integer_type->len, v);
	else
		bt_bitfield_write_be(pos->base, unsigned long, pos->offset,
				     integer_type->len, v);
end:
	move_pos(pos, integer_type->len);
}
