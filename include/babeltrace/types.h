#ifndef _BABELTRACE_TYPES_H
#define _BABELTRACE_TYPES_H

/*
 * BabelTrace
 *
 * Type Header
 *
 * Copyright 2010, 2011 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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
struct declaration;

/* Type declaration scope */
struct declaration_scope {
	/* Hash table mapping type name GQuark to struct type */
	GHashTable *types;
	struct declaration_scope *parent_scope;
};

struct type {
	GQuark name;		/* type name */
	size_t alignment;	/* type alignment, in bits */
	int ref;		/* number of references to the type */
	/*
	 * type_free called with type ref is decremented to 0.
	 */
	void (*type_free)(struct type *type);
	struct declaration *
		(*declaration_new)(struct type *type,
				   struct declaration_scope *parent_scope);
	/*
	 * declaration_free called with declaration ref is decremented to 0.
	 */
	void (*declaration_free)(struct declaration *declaration);
	/*
	 * Declaration copy function. Knows how to find the child declaration
	 * from the parent declaration.
	 */
	void (*copy)(struct stream_pos *dest, const struct format *fdest, 
		     struct stream_pos *src, const struct format *fsrc,
		     struct declaration *declaration);
};

struct declaration {
	struct type *type;
	int ref;		/* number of references to the declaration */
};

/*
 * Because we address in bits, bitfields end up being exactly the same as
 * integers, except that their read/write functions must be able to deal with
 * read/write non aligned on CHAR_BIT.
 */
struct type_integer {
	struct type p;
	size_t len;		/* length, in bits. */
	int byte_order;		/* byte order */
	int signedness;
};

struct declaration_integer {
	struct declaration p;
	struct type_integer *type;
	/* Last values read */
	union {
		uint64_t _unsigned;
		int64_t _signed;
	} value;
};

struct type_float {
	struct type p;
	struct type_integer *sign;
	struct type_integer *mantissa;
	struct type_integer *exp;
	int byte_order;
	/* TODO: we might want to express more info about NaN, +inf and -inf */
};

struct declaration_float {
	struct declaration p;
	struct type_float *type;
	/* Last values read */
	long double value;
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

struct type_enum {
	struct type p;
	struct type_integer *integer_type;
	struct enum_table table;
};

struct declaration_enum {
	struct declaration p;
	struct declaration_integer *integer;
	struct type_enum *type;
	/* Last GQuark values read. Keeping a reference on the GQuark array. */
	GArray *value;
};

struct type_string {
	struct type p;
};

struct declaration_string {
	struct declaration p;
	struct type_string *type;
	char *value;	/* freed at declaration_string teardown */
};

struct type_field {
	GQuark name;
	struct type *type;
};

struct field {
	struct declaration *type;
};

struct type_struct {
	struct type p;
	GHashTable *fields_by_name;	/* Tuples (field name, field index) */
	GArray *fields;			/* Array of type_field */
};

struct declaration_struct {
	struct declaration p;
	struct type_struct *type;
	struct declaration_scope *scope;
	GArray *fields;			/* Array of struct field */
};

struct type_variant {
	struct type p;
	GHashTable *fields_by_tag;	/* Tuples (field tag, field index) */
	GArray *fields;			/* Array of type_field */
};

struct declaration_variant {
	struct declaration p;
	struct type_variant *type;
	struct declaration_scope *scope;
	struct declaration *tag;
	GArray *fields;			/* Array of struct field */
	struct field *current_field;	/* Last field read */
};

struct type_array {
	struct type p;
	size_t len;
	struct type *elem;
};

struct declaration_array {
	struct declaration p;
	struct type_array *type;
	struct declaration_scope *scope;
	struct field current_element;		/* struct field */
};

struct type_sequence {
	struct type p;
	struct type_integer *len_type;
	struct type *elem;
};

struct declaration_sequence {
	struct declaration p;
	struct type_sequence *type;
	struct declaration_scope *scope;
	struct declaration_integer *len;
	struct field current_element;		/* struct field */
};

struct type *lookup_type(GQuark qname, struct declaration_scope *scope);
int register_type(struct type *type, struct declaration_scope *scope);

void type_ref(struct type *type);
void type_unref(struct type *type);

struct declaration_scope *
	new_declaration_scope(struct declaration_scope *parent_scope);
void free_declaration_scope(struct declaration_scope *scope);

void declaration_ref(struct declaration *declaration);
void declaration_unref(struct declaration *declaration);

/* Nameless types can be created by passing a NULL name */

struct type_integer *integer_type_new(const char *name,
				      size_t len, int byte_order,
				      int signedness, size_t alignment);

/*
 * mantissa_len is the length of the number of bytes represented by the mantissa
 * (e.g. result of DBL_MANT_DIG). It includes the leading 1.
 */
struct type_float *float_type_new(const char *name,
				  size_t mantissa_len,
				  size_t exp_len, int byte_order,
				  size_t alignment);

/*
 * A GQuark can be translated to/from strings with g_quark_from_string() and
 * g_quark_to_string().
 */

/*
 * Returns a GArray of GQuark or NULL.
 * Caller must release the GArray with g_array_unref().
 */
GArray *enum_uint_to_quark_set(const struct type_enum *enum_type, uint64_t v);

/*
 * Returns a GArray of GQuark or NULL.
 * Caller must release the GArray with g_array_unref().
 */
GArray *enum_int_to_quark_set(const struct type_enum *enum_type, uint64_t v);

/*
 * Returns a GArray of struct enum_range or NULL.
 * Callers do _not_ own the returned GArray (and therefore _don't_ need to
 * release it).
 */
GArray *enum_quark_to_range_set(const struct type_enum *enum_type, GQuark q);
void enum_signed_insert(struct type_enum *enum_type,
                        int64_t start, int64_t end, GQuark q);
void enum_unsigned_insert(struct type_enum *enum_type,
			  uint64_t start, uint64_t end, GQuark q);
size_t enum_get_nr_enumerators(struct type_enum *enum_type);

struct type_enum *enum_type_new(const char *name,
				struct type_integer *integer_type);

struct type_struct *struct_type_new(const char *name);
void struct_type_add_field(struct type_struct *struct_type,
			   const char *field_name, struct type *field_type);
/*
 * Returns the index of a field within a structure.
 */
unsigned long struct_type_lookup_field_index(struct type_struct *struct_type,
					     GQuark field_name);
/*
 * field returned only valid as long as the field structure is not appended to.
 */
struct type_field *
struct_type_get_field_from_index(struct type_struct *struct_type,
				 unsigned long index);
struct field *
struct_get_field_from_index(struct declaration_struct *struct_declaration,
			    unsigned long index);

/*
 * The tag enumeration is validated to ensure that it contains only mappings
 * from numeric values to a single tag. Overlapping tag value ranges are
 * therefore forbidden.
 */
struct type_variant *variant_type_new(const char *name);
void variant_type_add_field(struct type_variant *variant_type,
			    const char *tag_name, struct type *tag_type);
struct type_field *
variant_type_get_field_from_tag(struct type_variant *variant_type, GQuark tag);
/*
 * Returns 0 on success, -EPERM on error.
 */
int variant_declaration_set_tag(struct declaration_variant *variant,
				struct declaration *enum_tag);
/*
 * Returns the field selected by the current tag value.
 * field returned only valid as long as the variant structure is not appended
 * to.
 */
struct field *
variant_get_current_field(struct declaration_variant *variant);

/*
 * elem_type passed as parameter now belongs to the array. No need to free it
 * explicitly. "len" is the number of elements in the array.
 */
struct type_array *array_type_new(const char *name,
				  size_t len, struct type *elem_type);

/*
 * int_type and elem_type passed as parameter now belong to the sequence. No
 * need to free them explicitly.
 */
struct type_sequence *sequence_type_new(const char *name,
					struct type_integer *len_type, 
					struct type *elem_type);

#endif /* _BABELTRACE_TYPES_H */
