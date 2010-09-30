#ifndef _BABELTRACE_CTF_TYPES_H
#define _BABELTRACE_CTF_TYPES_H

/*
 * Common Trace Format
 *
 * Type header
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

/*
 * IMPORTANT: All lengths (len) and offsets (start, end) are expressed in bits,
 *            *not* in bytes.
 *
 * All write primitives, as well as read for dynamically sized entities, can
 * receive a NULL ptr/dest parameter. In this case, no write is performed, but
 * the size is returned.
 */

uint64_t ctf_uint_read(const unsigned char *ptr, int byte_order, size_t len);
int64_t ctf_int_read(const unsigned char *ptr, int byte_order, size_t len);
size_t ctf_uint_write(unsigned char *ptr, int byte_order, size_t len, uint64_t v);
size_t ctf_int_write(unsigned char *ptr, int byte_order, size_t len, int64_t v);

uint64_t ctf_bitfield_unsigned_read(const unsigned char *ptr,
				    unsigned long start, unsigned long len,
				    int byte_order);
int64_t ctf_bitfield_signed_read(const unsigned char *ptr,
				 unsigned long start, unsigned long len,
				 int byte_order);
size_t ctf_bitfield_unsigned_write(unsigned char *ptr,
				   unsigned long start, unsigned long len,
				   int byte_order, uint64_t v);
size_t ctf_bitfield_signed_write(unsigned char *ptr,
				 unsigned long start, unsigned long len,
				 int byte_order, int64_t v);

double ctf_double_read(const unsigned char *ptr, const struct type_class_float *src)
size_t ctf_double_write(unsigned char *ptr, const struct type_class_float *dest,
		        double v);
long double ctf_ldouble_read(const unsigned char *ptr,
			     const struct type_class_float *src)
size_t ctf_ldouble_write(unsigned char *ptr, const struct type_class_float *dest,
		         long double v);
void ctf_float_copy(unsigned char *destp, const struct type_class_float *dest,
		    const unsigned char *srcp, const struct type_class_float *src);

size_t ctf_string_copy(unsigned char *dest, const unsigned char *src);

/*
 * A GQuark can be translated to/from strings with g_quark_from_string() and
 * g_quark_to_string().
 */
GQuark ctf_enum_uint_to_quark(const struct enum_table *table, uint64_t v);
GQuark ctf_enum_int_to_quark(const struct enum_table *table, uint64_t v);
uint64_t ctf_enum_quark_to_uint(size_t len, int byte_order, GQuark q);
int64_t ctf_enum_quark_to_int(size_t len, int byte_order, GQuark q);
void ctf_enum_signed_insert(struct enum_table *table, int64_t v, GQuark q);
void ctf_enum_unsigned_insert(struct enum_table *table, uint64_t v, GQuark q);
struct enum_table *ctf_enum_new(void);
void ctf_enum_destroy(struct enum_table *table);

#endif /* _BABELTRACE_CTF_TYPES_H */
