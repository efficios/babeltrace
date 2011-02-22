#ifndef _BABELTRACE_TYPES_H
#define _BABELTRACE_TYPES_H

/*
 * BabelTrace
 *
 * Type Header
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

#include <babeltrace/align.h>
#include <babeltrace/list.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <glib.h>
#include <assert.h>

/* Preallocate this many fields for structures */
#define DEFAULT_NR_STRUCT_FIELDS 8

/*
 * Always update stream_pos with move_pos and init_pos.
 */
struct stream_pos {
	char *base;		/* Base address */
	size_t offset;		/* Offset from base, in bits */
	int dummy;		/* Dummy position, for length calculation */
};

static inline
void init_pos(struct stream_pos *pos, char *base)
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
char *get_pos_addr(struct stream_pos *pos)
{
	/* Only makes sense to get the address after aligning on CHAR_BIT */
	assert(!(pos->offset % CHAR_BIT));
	return pos->base + (pos->offset / CHAR_BIT);
}

struct format;

struct type_class {
	GQuark name;		/* type name */
	size_t alignment;	/* type alignment, in bits */
	int ref;		/* number of references to the type */
	/*
	 * Type copy function. Knows how to find the child type_class from the
	 * parent type_class.
	 */
	void (*copy)(struct stream_pos *dest, const struct format *fdest, 
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
	struct type_class_integer *sign;
	struct type_class_integer *mantissa;
	struct type_class_integer *exp;
	int byte_order;
	/* TODO: we might want to express more info about NaN, +inf and -inf */
};

/*
 * enum_val_equal assumes that signed and unsigned memory layout overlap.
 */
struct enum_range {
	union {
		int64_t _signed;
		uint64_t _unsigned;
	} start;	/* lowest range value */
	union {
		int64_t _signed;
		uint64_t _unsigned;
	} end;		/* highest range value */
};

struct enum_range_to_quark {
	struct cds_list_head node;
	struct enum_range range;
	GQuark quark;
};

/*
 * We optimize the common case (range of size 1: single value) by creating a
 * hash table mapping values to quark sets. We then lookup the ranges to
 * complete the quark set.
 *
 * TODO: The proper structure to hold the range to quark set mapping would be an
 * interval tree, with O(n) size, O(n*log(n)) build time and O(log(n)) query
 * time. Using a simple O(n) list search for now for implementation speed and
 * given that we can expect to have a _relatively_ small number of enumeration
 * ranges. This might become untrue if we are fed with symbol tables often
 * required to lookup function names from instruction pointer value.
 */
struct enum_table {
	GHashTable *value_to_quark_set;		/* (value, GQuark GArray) */
	struct cds_list_head range_to_quark;	/* (range, GQuark) */
	GHashTable *quark_to_range_set;		/* (GQuark, range GArray) */
};

struct type_class_enum {
	struct type_class_integer p;	/* inherit from integer */
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
	struct type_class_integer *len_class;
	struct type_class *elem;
};

struct type_class *lookup_type(GQuark qname);
int register_type(struct type_class *type_class);

void type_ref(struct type_class *type_class);
void type_unref(struct type_class *type_class);

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

/*
 * Returns a GArray of GQuark or NULL.
 * Caller must release the GArray with g_array_unref().
 */
GArray *enum_uint_to_quark_set(const struct type_class_enum *enum_class,
			       uint64_t v);

/*
 * Returns a GArray of GQuark or NULL.
 * Caller must release the GArray with g_array_unref().
 */
GArray *enum_int_to_quark_set(const struct type_class_enum *enum_class,
			      uint64_t v);

/*
 * Returns a GArray of struct enum_range or NULL.
 * Callers do _not_ own the returned GArray (and therefore _don't_ need to
 * release it).
 */
GArray *enum_quark_to_range_set(const struct type_class_enum *enum_class,
				GQuark q);
void enum_signed_insert(struct type_class_enum *enum_class,
                        int64_t start, int64_t end, GQuark q);
void enum_unsigned_insert(struct type_class_enum *enum_class,
			  uint64_t start, uint64_t end, GQuark q);

struct type_class_enum *enum_type_new(const char *name,
				      size_t len, int byte_order,
				      int signedness,
				      size_t alignment);
void enum_type_free(struct type_class_enum *enum_class);

struct type_class_struct *struct_type_new(const char *name);
void struct_type_free(struct type_class_struct *struct_class);
void struct_type_add_field(struct type_class_struct *struct_class,
			   const char *field_name,
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

/*
 * elem_class passed as parameter now belongs to the array. No need to free it
 * explicitely.
 */
struct type_class_array *array_type_new(const char *name,
					size_t len,
					struct type_class *elem_class);
void array_type_free(struct type_class_array *array_class);

/*
 * int_class and elem_class passed as parameter now belongs to the sequence. No
 * need to free them explicitely.
 */
struct type_class_sequence *sequence_type_new(const char *name,
					struct type_class_integer *len_class, 
					struct type_class *elem_class);
void sequence_type_free(struct type_class_sequence *sequence_class);

#endif /* _BABELTRACE_TYPES_H */
