#ifndef _BABELTRACE_FORMAT_H
#define _BABELTRACE_FORMAT_H

/*
 * BabelTrace
 *
 * Trace Format Header
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

#include <stdint.h>
#include <glib.h>

struct format {
	GQuark name;

	uint64_t (*uint_read)(const uint8_t *ptr, size_t len, int byte_order);
	int64_t (*int_read)(const uint8_t *ptr, size_t len, int byte_order);
	size_t (*uint_write)(uint8_t *ptr, size_t len, int byte_order, uint64_t v);
	size_t (*int_write)(uint8_t *ptr, size_t len, int byte_order, int64_t v);

	uint64_t (*bitfield_unsigned_read)(const unsigned char *ptr,
					    unsigned long start, unsigned long len,
					    int byte_order);
	int64_t (*bitfield_signed_read)(const unsigned char *ptr,
					 unsigned long start, unsigned long len,
					 int byte_order);
	size_t (*bitfield_unsigned_write)(unsigned char *ptr,
					   unsigned long start, unsigned long len,
					   int byte_order, uint64_t v);
	size_t (*bitfield_signed_write)(unsigned char *ptr,
					 unsigned long start, unsigned long len,
					 int byte_order, int64_t v);


	void (*float_copy)(unsigned char *destp, const struct type_class_float *dest,
		    const unsigned char *srcp, const struct type_class_float *src);

	size_t (*string_copy)(unsigned char *dest, const unsigned char *src);

	GQuark (*enum_uint_to_quark)(const struct enum_table *table, uint64_t v);
	GQuark (*enum_int_to_quark)(const struct enum_table *table, uint64_t v);
	uint64_t (*enum_quark_to_uint)(size_t len, int byte_order, GQuark q);
	int64_t (*enum_quark_to_int)(size_t len, int byte_order, GQuark q);
};

struct format *bt_lookup_format(GQuark qname);
int bt_register_format(const struct format *format);

/* TBD: format unregistration */

#endif /* _BABELTRACE_FORMAT_H */
