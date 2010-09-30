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
};

struct type_class_integer {
	struct type_class p;
	size_t len;		/* length, in bits. */
	int byte_order;		/* byte order */
	int signedness;
};

int integer_type_new(const char *name, size_t len, int byte_order,
		     int signedness);

struct type_class_bitfield {
	struct type_class_integer p;
	size_t start_offset;	/* offset from base address, in bits */
};

int bitfield_type_new(const char *name, size_t start_offset,
		      size_t len, int byte_order, int signedness);

struct type_class_float {
	struct type_class p;
	size_t mantissa_len;
	size_t exp_len;
	int byte_order;
};

struct type_class_enum {
	struct type_class_bitfield;	/* inherit from bitfield */
	struct enum_table *table;
};

struct type_class_struct {
	struct type_class p;
	/* TODO */
};

struct type_class *ctf_lookup_type(GQuark qname);
int ctf_register_type(struct type_class *type_class);

#endif /* _BABELTRACE_TYPES_H */
