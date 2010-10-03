/*
 * Common Trace Format
 *
 * Bitfields read/write functions.
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

#include <ctf/bitfield.h>
#include <endian.h>

uint64_t ctf_bitfield_unsigned_read(struct stream_pos *pos,
			const struct type_class_bitfield *bitfield_class)
{
	uint64_t v;

	align_pos(pos, bitfield_class->p.p.alignment);
	if (bitfield_class->p.byte_order == LITTLE_ENDIAN)
		ctf_bitfield_read_le(pos->base, pos->offset,
				     bitfield_class->p.len, &v);
	else
		ctf_bitfield_read_be(pos->base, pos->offset,
				     bitfield_class->p.len, &v);
	move_pos(pos, bitfield_class->p.len);
	return v;
}

int64_t ctf_bitfield_signed_read(struct stream_pos *pos,
			const struct type_class_bitfield *bitfield_class)
{
	int64_t v;

	align_pos(pos, bitfield_class->p.p.alignment);

	if (bitfield_class->p.byte_order == LITTLE_ENDIAN)
		ctf_bitfield_read_le(pos->base, pos->offset,
				     bitfield_class->p.len, &v);
	else
		ctf_bitfield_read_be(pos->base, pos->offset,
				     bitfield_class->p.len, &v);
	move_pos(pos, bitfield_class->p.len);
	return v;
}

void ctf_bitfield_unsigned_write(struct stream_pos *pos,
			const struct type_class_bitfield *bitfield_class,
			uint64_t v)
{
	align_pos(pos, bitfield_class->p.p.alignment);
	if (pos->dummy)
		goto end;
	if (bitfield_class->p.byte_order == LITTLE_ENDIAN)
		ctf_bitfield_write_le(pos->base, pos->offset,
				      bitfield_class->p.len, v);
	else
		ctf_bitfield_write_be(pos->base, pos->offset,
				      bitfield_class->p.len,, v);
end:
	move_pos(pos, bitfield_class->p.len);
}

void ctf_bitfield_signed_write(struct stream_pos *pos,
			const struct type_class_bitfield *bitfield_class,
			int64_t v)
{
	align_pos(pos, bitfield_class->p.p.alignment);
	if (pos->dummy)
		goto end;
	if (bitfield_class->p.byte_order == LITTLE_ENDIAN)
		ctf_bitfield_write_le(pos->base, pos->offset,
				      bitfield_class->p.len, v);
	else
		ctf_bitfield_write_be(pos->base, pos->offset,
				      bitfield_class->p.len, v);
end:
	move_pos(pos, bitfield_class->p.len);
}
