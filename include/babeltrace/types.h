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
#include <babeltrace/align.h>
#include <string.h>

/* Preallocate this many fields for structures */
#define DEFAULT_NR_STRUCT_FIELDS 8

/*
 * Always update stream_pos with move_pos and init_pos.
 */
struct stream_pos {
	unsigned char *base;	/* Base address */
	size_t offset;		/* Offset from base, in bits */
	int dummy;		/* Dummy position, for length calculation */
};

static inline
void init_pos(struct stream_pos *pos, unsigned char *base)
{
	pos->base = base;	/* initial base, page-aligned */
	pos->offset = 0;
	pos->dummy = false;
}

/*
 * move_pos - move position of a relative bit offset
 *
 * TODO: allow larger files by updating base too.
 */
static inline
void move_pos(struct stream_pos *pos, size_t offset)
{
	pos->offset = pos->offset + offset;
}

/*
 * align_pos - align position on a bit offset (> 0)
 *
 * TODO: allow larger files by updating base too.
 */
static inline
void align_pos(struct stream_pos *pos, size_t offset)
{
	pos->offset += offset_align(pos->offset, offset);
}

static inline
void copy_pos(struct stream_pos *dest, struct stream_pos *src)
{
	memcpy(dest, src, sizeof(struct stream_pos));
}

static inline
unsigned char *get_pos_addr(struct stream_pos *pos)
{
	/* Only makes sense to get the address after aligning on CHAR_BIT */
	assert(!(pos->alignment % CHAR_BIT));
	return pos->base + (pos->offset / CHAR_BIT);
}

struct type_class {
	GQuark name;		/* type name */
	size_t alignment;	/* type alignment, in bits */
	/*
	 * Type copy function. Knows how to find the child type_class from the
	 * parent type_class.
	 */
	size_t (*copy)(struct stream_pos *dest, const struct format *fdest, 
		       struct stream_pos *src, const struct format *fsrc,
		       const struct type_class *type_class);
	void (*free)(struct type_class *type_class);
};

/*
 * Because we address in bits, bitfields end up being exactly the same as
 * integers, except that their read/write functions must be able to deal with
 * read/write non aligned on CHAR_BIT.
 */
struct type_class_integer {
	struct type_class p;
	size_t len;		/* length, in bits. */
	int byte_order;		/* byte order */
	int signedness;
};

struct type_class_float {
	struct type_class p;
	struct int_class *sign;
	struct int_class *mantissa;
	struct int_class *exp;
	int byte_order;
	/* TODO: we might want to express more info about NaN, +inf and -inf */
};

struct enum_table {
	GHashTable *value_to_quark;	/* Tuples (value, GQuark) */
	GHashTable *quark_to_value;	/* Tuples (GQuark, value) */
};

struct type_class_enum {
	struct type_class_int p;	/* inherit from integer */
	struct enum_table table;
};

struct type_class_string {
	struct type_class p;
};

struct field {
	GQuark name;
	struct type_class *type_class;
};

struct type_class_struct {
	struct type_class p;
	GHashTable *fields_by_name;	/* Tuples (field name, field index) */
	GArray *fields;			/* Array of fields */
};

struct type_class_array {
	struct type_class p;
	size_t len;
	struct type_class *elem;
};

struct type_class_sequence {
	struct type_class p;
	struct type_class_integer *len;
	struct type_class *elem;
};

struct type_class *ctf_lookup_type(GQuark qname);
int ctf_register_type(struct type_class *type_class);

/* Nameless types can be created by passing a NULL name */

struct type_class_integer *integer_type_new(const char *name,
					    size_t len, int byte_order,
					    int signedness,
					    size_t alignment);
void integer_type_free(struct type_class_integer *int_class);

/*
 * mantissa_len is the length of the number of bytes represented by the mantissa
 * (e.g. result of DBL_MANT_DIG). It includes the leading 1.
 */
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
				      size_t len, int byte_order,
				      int signedness,
				      size_t alignment);
void enum_type_free(struct type_class_enum *enum_class);

struct type_class_struct *struct_type_new(const char *name);
void struct_type_free(struct type_class_struct *struct_class);
void struct_type_add_field(struct type_class_struct *struct_class,
			   GQuark field_name,
			   struct type_class *type_class);
/*
 * Returns the index of a field within a structure.
 */
unsigned long
struct_type_lookup_field_index(struct type_class_struct *struct_class,
			       GQuark field_name);
/*
 * field returned only valid as long as the field structure is not appended to.
 */
struct field *
struct_type_get_field_from_index(struct type_class_struct *struct_class,
				 unsigned long index);

struct type_class_array *array_type_new(const char *name,
					size_t len,
					struct type_class *elem_class);
void array_type_free(struct type_class_array *array_class);

struct type_class_sequence *sequence_type_new(const char *name,
					struct type_class_integer *int_class, 
					struct type_class *elem_class);
void array_type_free(struct type_class_array *array_class);

#endif /* _BABELTRACE_TYPES_H */
