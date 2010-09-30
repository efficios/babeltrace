/*
 * BabelTrace - Common Trace Format (CTF)
 *
 * Format registration.
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

#include <babeltrace/format.h>
#include <babeltrace/ctf/types.h>

void __attribute__((constructor)) ctf_init(void);

static const struct format ctf_format = {
	.uint_read = ctf_uint_read,
	.int_read = ctf_int_read,
	.uint_write = ctf_uint_write,
	.int_write = ctf_int_write,
	.bitfield_unsigned_read = ctf_bitfield_unsigned_read,
	.bitfield_signed_read = ctf_bitfield_signed_read,
	.bitfield_unsigned_write = ctf_bitfield_unsigned_write,
	.bitfield_signed_write = ctf_bitfield_signed_write,
	.double_read = ctf_double_read,
	.double_write = ctf_double_write,
	.float_copy = ctf_float_copy,
	.string_copy = ctf_string_copy,
	.enum_uint_to_quark = ctf_enum_uint_to_quark,
	.enum_int_to_quark = ctf_enum_int_to_quark,
	.enum_quark_to_uint = ctf_enum_quark_to_uint,
	.enum_quark_to_int = ctf_enum_quark_to_int,
};

void ctf_init(void)
{
	int ret;

	ctf_format->name = g_quark_from_static_string("ctf");
	ret = bt_register_format(&ctf_format);
	assert(!ret);
}

/* TODO: finalize */
