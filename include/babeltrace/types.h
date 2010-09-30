#ifndef _BABELTRACE_TYPES_H
#define _BABELTRACE_TYPES_H

/*
 * BabelTrace
 *
 * Type Header
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

struct type_class {
	GQuark name;		/* type name */
	size_t alignment;	/* type alignment, in bits */
	/*
	 * Type copy function. Knows how to find the child type_class from the
	 * parent type_class.
	 */
	size_t (*copy)(unsigned char *dest, const struct format *fdest, 
		       const unsigned char *src, const struct format *fsrc,
		       const struct type_class *type_class);
	void (*free)(struct type_class *type_class);
};

struct type_class_integer {
	struct type_class p;
	size_t len;		/* length, in bits. */
	int byte_order;		/* byte order */
	int signedness;
};

struct type_class_bitfield {
	struct type_class_integer p;
	size_t start_offset;	/* offset from base address, in bits */
};

struct type_class_float {
	struct type_class p;
	size_t mantissa_len;
	size_t exp_len;
	int byte_order;
	/* TODO: we might want to express more info about NaN, +inf and -inf */
};

struct enum_table {
	GHashTable *value_to_quark;	/* Tuples (value, GQuark) */
	GHashTable *quark_to_value;	/* Tuples (GQuark, value) */
};

struct type_class_enum {
	struct type_class_bitfield p;	/* inherit from bitfield */
	struct enum_table table;
};

struct type_class_struct {
	struct type_class p;
	/* TODO */
};

struct type_class *ctf_lookup_type(GQuark qname);
int ctf_register_type(struct type_class *type_class);

/* Nameless types can be created by passing a NULL name */

struct type_class_integer *integer_type_new(const char *name,
					    size_t start_offset,
					    size_t len, int byte_order,
					    int signedness);
void integer_type_free(struct type_class_integer *int_class);

struct type_class_bitfield *bitfield_type_new(const char *name,
					      size_t start_offset,
					      size_t len, int byte_order,
					      int signedness);
void bitfield_type_free(struct type_class_bitfield *bitfield_class);

struct type_class_float *float_type_new(const char *name,
					size_t mantissa_len,
					size_t exp_len, int byte_order,
					size_t alignment);
void float_type_free(struct type_class_float *float_class);

/*
 * A GQuark can be translated to/from strings with g_quark_from_string() and
 * g_quark_to_string().
 */
GQuark enum_uint_to_quark(const struct type_class_enum *enum_class, uint64_t v);
GQuark enum_int_to_quark(const struct type_class_enum *enum_class, uint64_t v);
uint64_t enum_quark_to_uint(const struct type_class_enum *enum_class,
			    size_t len, int byte_order, GQuark q);
int64_t enum_quark_to_int(const struct type_class_enum *enum_class,
			  size_t len, int byte_order, GQuark q);
void enum_signed_insert(struct type_class_enum *enum_class,
			int64_t v, GQuark q);
void enum_unsigned_insert(struct type_class_enum *enum_class,
			  uint64_t v, GQuark q);

struct type_class_enum *enum_type_new(const char *name,
				      size_t start_offset,
				      size_t len, int byte_order,
				      int signedness,
				      size_t alignment);
void enum_type_free(struct type_class_enum *enum_class);

#endif /* _BABELTRACE_TYPES_H */
