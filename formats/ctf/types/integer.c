/*
 * Common Trace Format
 *
 * Integers read/write functions.
 *
 * Copyright 2010-2011 EfficiOS Inc. and Linux Foundation
 *
 * Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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
#include <babeltrace/endian.h>

/*
 * The aligned read/write functions are expected to be faster than the
 * bitfield variants. They will be enabled eventually as an
 * optimisation.
 */

static
int _aligned_integer_read(struct stream_pos *ppos,
			  struct definition *definition)
{
	struct definition_integer *integer_definition =
		container_of(definition, struct definition_integer, p);
	const struct declaration_integer *integer_declaration =
		integer_definition->declaration;
	struct ctf_stream_pos *pos = ctf_pos(ppos);
	int rbo = (integer_declaration->byte_order != BYTE_ORDER);	/* reverse byte order */

	ctf_align_pos(pos, integer_declaration->p.alignment);

	if (!ctf_pos_access_ok(pos, integer_declaration->len))
		return -EFAULT;

	assert(!(pos->offset % CHAR_BIT));
	if (!integer_declaration->signedness) {
		switch (integer_declaration->len) {
		case 8:
		{
			uint8_t v;

			v = *(const uint8_t *) ctf_get_pos_addr(pos);
			integer_definition->value._unsigned = v;
			break;
		}
		case 16:
		{
			uint16_t v;

			v = *(const uint16_t *) ctf_get_pos_addr(pos);
			integer_definition->value._unsigned =
				rbo ? GUINT16_SWAP_LE_BE(v) : v;
			break;
		}
		case 32:
		{
			uint32_t v;

			v = *(const uint32_t *) ctf_get_pos_addr(pos);
			integer_definition->value._unsigned =
				rbo ? GUINT32_SWAP_LE_BE(v) : v;
			break;
		}
		case 64:
		{
			uint64_t v;

			v = *(const uint64_t *) ctf_get_pos_addr(pos);
			integer_definition->value._unsigned =
				rbo ? GUINT64_SWAP_LE_BE(v) : v;
			break;
		}
		default:
			assert(0);
		}
	} else {
		switch (integer_declaration->len) {
		case 8:
		{
			int8_t v;

			v = *(const int8_t *) ctf_get_pos_addr(pos);
			integer_definition->value._signed = v;
			break;
		}
		case 16:
		{
			int16_t v;

			v = *(const int16_t *) ctf_get_pos_addr(pos);
			integer_definition->value._signed =
				rbo ? (int16_t) GUINT16_SWAP_LE_BE(v) : v;
			break;
		}
		case 32:
		{
			int32_t v;

			v = *(const int32_t *) ctf_get_pos_addr(pos);
			integer_definition->value._signed =
				rbo ? (int32_t) GUINT32_SWAP_LE_BE(v) : v;
			break;
		}
		case 64:
		{
			int64_t v;

			v = *(const int64_t *) ctf_get_pos_addr(pos);
			integer_definition->value._signed =
				rbo ? (int64_t) GUINT64_SWAP_LE_BE(v) : v;
			break;
		}
		default:
			assert(0);
		}
	}
	ctf_move_pos(pos, integer_declaration->len);
	return 0;
}

static
int _aligned_integer_write(struct stream_pos *ppos,
			    struct definition *definition)
{
	struct definition_integer *integer_definition =
		container_of(definition, struct definition_integer, p);
	const struct declaration_integer *integer_declaration =
		integer_definition->declaration;
	struct ctf_stream_pos *pos = ctf_pos(ppos);
	int rbo = (integer_declaration->byte_order != BYTE_ORDER);	/* reverse byte order */

	ctf_align_pos(pos, integer_declaration->p.alignment);

	if (!ctf_pos_access_ok(pos, integer_declaration->len))
		return -EFAULT;

	assert(!(pos->offset % CHAR_BIT));
	if (pos->dummy)
		goto end;
	if (!integer_declaration->signedness) {
		uint64_t v = integer_definition->value._unsigned;

		switch (integer_declaration->len) {
		case 8:	*(uint8_t *) ctf_get_pos_addr(pos) = (uint8_t) v;
			break;
		case 16:
			*(uint16_t *) ctf_get_pos_addr(pos) = rbo ?
						 GUINT16_SWAP_LE_BE((uint16_t) v) :
						 (uint16_t) v;
			break;
		case 32:
			*(uint32_t *) ctf_get_pos_addr(pos) = rbo ?
						 GUINT32_SWAP_LE_BE((uint32_t) v) :
						 (uint32_t) v;
			break;
		case 64:
			*(uint64_t *) ctf_get_pos_addr(pos) = rbo ?
						 GUINT64_SWAP_LE_BE(v) : v;
			break;
		default:
			assert(0);
		}
	} else {
		int64_t v = integer_definition->value._signed;

		switch (integer_declaration->len) {
		case 8:	*(int8_t *) ctf_get_pos_addr(pos) = (int8_t) v;
			break;
		case 16:
			*(int16_t *) ctf_get_pos_addr(pos) = rbo ?
						 (int16_t) GUINT16_SWAP_LE_BE((int16_t) v) :
						 (int16_t) v;
			break;
		case 32:
			*(int32_t *) ctf_get_pos_addr(pos) = rbo ?
						 (int32_t) GUINT32_SWAP_LE_BE((int32_t) v) :
						 (int32_t) v;
			break;
		case 64:
			*(int64_t *) ctf_get_pos_addr(pos) = rbo ?
						 GUINT64_SWAP_LE_BE(v) : v;
			break;
		default:
			assert(0);
		}
	}
end:
	ctf_move_pos(pos, integer_declaration->len);
	return 0;
}

int ctf_integer_read(struct stream_pos *ppos, struct definition *definition)
{
	struct definition_integer *integer_definition =
		container_of(definition, struct definition_integer, p);
	const struct declaration_integer *integer_declaration =
		integer_definition->declaration;
	struct ctf_stream_pos *pos = ctf_pos(ppos);

	if (!(integer_declaration->p.alignment % CHAR_BIT)
	    && !(integer_declaration->len % CHAR_BIT)) {
		return _aligned_integer_read(ppos, definition);
	}

	ctf_align_pos(pos, integer_declaration->p.alignment);

	if (!ctf_pos_access_ok(pos, integer_declaration->len))
		return -EFAULT;

	if (!integer_declaration->signedness) {
		if (integer_declaration->byte_order == LITTLE_ENDIAN)
			bt_bitfield_read_le(mmap_align_addr(pos->base_mma) +
					pos->mmap_base_offset, unsigned long,
				pos->offset, integer_declaration->len,
				&integer_definition->value._unsigned);
		else
			bt_bitfield_read_be(mmap_align_addr(pos->base_mma) +
					pos->mmap_base_offset, unsigned long,
				pos->offset, integer_declaration->len,
				&integer_definition->value._unsigned);
	} else {
		if (integer_declaration->byte_order == LITTLE_ENDIAN)
			bt_bitfield_read_le(mmap_align_addr(pos->base_mma) +
					pos->mmap_base_offset, unsigned long,
				pos->offset, integer_declaration->len,
				&integer_definition->value._signed);
		else
			bt_bitfield_read_be(mmap_align_addr(pos->base_mma) +
					pos->mmap_base_offset, unsigned long,
				pos->offset, integer_declaration->len,
				&integer_definition->value._signed);
	}
	ctf_move_pos(pos, integer_declaration->len);
	return 0;
}

int ctf_integer_write(struct stream_pos *ppos, struct definition *definition)
{
	struct definition_integer *integer_definition =
		container_of(definition, struct definition_integer, p);
	const struct declaration_integer *integer_declaration =
		integer_definition->declaration;
	struct ctf_stream_pos *pos = ctf_pos(ppos);

	if (!(integer_declaration->p.alignment % CHAR_BIT)
	    && !(integer_declaration->len % CHAR_BIT)) {
		return _aligned_integer_write(ppos, definition);
	}

	ctf_align_pos(pos, integer_declaration->p.alignment);

	if (!ctf_pos_access_ok(pos, integer_declaration->len))
		return -EFAULT;

	if (pos->dummy)
		goto end;
	if (!integer_declaration->signedness) {
		if (integer_declaration->byte_order == LITTLE_ENDIAN)
			bt_bitfield_write_le(mmap_align_addr(pos->base_mma) +
					pos->mmap_base_offset, unsigned long,
				pos->offset, integer_declaration->len,
				integer_definition->value._unsigned);
		else
			bt_bitfield_write_be(mmap_align_addr(pos->base_mma) +
					pos->mmap_base_offset, unsigned long,
				pos->offset, integer_declaration->len,
				integer_definition->value._unsigned);
	} else {
		if (integer_declaration->byte_order == LITTLE_ENDIAN)
			bt_bitfield_write_le(mmap_align_addr(pos->base_mma) +
					pos->mmap_base_offset, unsigned long,
				pos->offset, integer_declaration->len,
				integer_definition->value._signed);
		else
			bt_bitfield_write_be(mmap_align_addr(pos->base_mma) +
					pos->mmap_base_offset, unsigned long,
				pos->offset, integer_declaration->len,
				integer_definition->value._signed);
	}
end:
	ctf_move_pos(pos, integer_declaration->len);
	return 0;
}
