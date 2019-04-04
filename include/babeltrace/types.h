#ifndef _BABELTRACE_TYPES_H
#define _BABELTRACE_TYPES_H

/*
 * BabelTrace
 *
 * Type Header
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
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <babeltrace/align.h>
#include <babeltrace/list.h>
#include <babeltrace/ctf/events.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <glib.h>
#include <assert.h>

/* Preallocate this many fields for structures */
#define DEFAULT_NR_STRUCT_FIELDS 8

struct ctf_stream_definition;
struct bt_stream_pos;
struct bt_format;
struct bt_definition;
struct ctf_clock;

/* type scope */
struct declaration_scope {
	/* Hash table mapping type name GQuark to "struct declaration" */
	/* Used for both typedef and typealias. */
	GHashTable *typedef_declarations;
	/* Hash table mapping struct name GQuark to "struct declaration_struct" */
	GHashTable *struct_declarations;
	/* Hash table mapping variant name GQuark to "struct declaration_variant" */
	GHashTable *variant_declarations;
	/* Hash table mapping enum name GQuark to "struct type_enum" */
	GHashTable *enum_declarations;
	struct declaration_scope *parent_scope;
};

/* definition scope */
struct definition_scope {
	/* Hash table mapping field name GQuark to "struct definition" */
	GHashTable *definitions;
	struct definition_scope *parent_scope;
	/*
	 * Complete "path" leading to this definition scope.
	 * Includes dynamic scope name '.' field name '.' field name '.' ....
	 * Array of GQuark elements (which are each separated by dots).
	 * The dynamic scope name can contain dots, and is encoded into
	 * a single GQuark. Thus, scope_path[0] returns the GQuark
	 * identifying the dynamic scope.
	 */
	GArray *scope_path;	/* array of GQuark */
};

struct bt_declaration {
	enum ctf_type_id id;
	size_t alignment;	/* type alignment, in bits */
	int ref;		/* number of references to the type */
	/*
	 * declaration_free called with declaration ref is decremented to 0.
	 */
	void (*declaration_free)(struct bt_declaration *declaration);
	struct bt_definition *
		(*definition_new)(struct bt_declaration *declaration,
				  struct definition_scope *parent_scope,
				  GQuark field_name, int index,
				  const char *root_name);
	/*
	 * definition_free called with definition ref is decremented to 0.
	 */
	void (*definition_free)(struct bt_definition *definition);
};

struct bt_definition {
	struct bt_declaration *declaration;
	int index;		/* Position of the definition in its container */
	GQuark name;		/* Field name in its container (or 0 if unset) */
	int ref;		/* number of references to the definition */
	GQuark path;
	struct definition_scope *scope;
};

typedef int (*rw_dispatch)(struct bt_stream_pos *pos,
			   struct bt_definition *definition);

/* Parent of per-plugin positions */
struct bt_stream_pos {
	/* read/write dispatch table. Specific to plugin used for stream. */
	rw_dispatch *rw_table;	/* rw dispatch table */
	int (*event_cb)(struct bt_stream_pos *pos,
			struct ctf_stream_definition *stream);
	int (*pre_trace_cb)(struct bt_stream_pos *pos,
			struct bt_trace_descriptor *trace);
	int (*post_trace_cb)(struct bt_stream_pos *pos,
			struct bt_trace_descriptor *trace);
	struct bt_trace_descriptor *trace;
};

static inline
int generic_rw(struct bt_stream_pos *pos, struct bt_definition *definition)
{
	enum ctf_type_id dispatch_id = definition->declaration->id;
	rw_dispatch call;

	assert(pos->rw_table[dispatch_id] != NULL);
	call = pos->rw_table[dispatch_id];
	return call(pos, definition);
}

/*
 * Because we address in bits, bitfields end up being exactly the same as
 * integers, except that their read/write functions must be able to deal with
 * read/write non aligned on CHAR_BIT.
 */
struct declaration_integer {
	struct bt_declaration p;
	size_t len;		/* length, in bits. */
	int byte_order;		/* byte order */
	int signedness;
	int base;		/* Base for pretty-printing: 2, 8, 10, 16 */
	enum ctf_string_encoding encoding;
	struct ctf_clock *clock;
};

#ifdef ENABLE_DEBUG_INFO
struct debug_info_source;
#endif

struct definition_integer {
	struct bt_definition p;
	struct declaration_integer *declaration;
	/* Last values read */
	union {
		uint64_t _unsigned;
		int64_t _signed;
	} value;

#ifdef ENABLE_DEBUG_INFO
	/*
	 * Debug infos (NULL if not set).
	 *
	 * This is extended debug informations set by the CTF input plugin
	 * itself when available. If it's set, then this integer definition
	 * is the "_ip" field of the stream event context.
	 */
	struct debug_info_source *debug_info_src;
#endif
};

struct declaration_float {
	struct bt_declaration p;
	struct declaration_integer *sign;
	struct declaration_integer *mantissa;
	struct declaration_integer *exp;
	int byte_order;
	/* TODO: we might want to express more info about NaN, +inf and -inf */
};

struct definition_float {
	struct bt_definition p;
	struct declaration_float *declaration;
	struct definition_integer *sign;
	struct definition_integer *mantissa;
	struct definition_integer *exp;
	/* Last values read */
	double value;
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
	struct bt_list_head node;
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
	struct bt_list_head range_to_quark;	/* (range, GQuark) */
	GHashTable *quark_to_range_set;		/* (GQuark, range GArray) */
};

struct declaration_enum {
	struct bt_declaration p;
	struct declaration_integer *integer_declaration;
	struct enum_table table;
};

struct definition_enum {
	struct bt_definition p;
	struct definition_integer *integer;
	struct declaration_enum *declaration;
	/* Last GQuark values read. Keeping a reference on the GQuark array. */
	GArray *value;
};

struct declaration_string {
	struct bt_declaration p;
	enum ctf_string_encoding encoding;
};

struct definition_string {
	struct bt_definition p;
	struct declaration_string *declaration;
	char *value;	/* freed at definition_string teardown */
	size_t len, alloc_len;
};

struct declaration_field {
	GQuark name;
	struct bt_declaration *declaration;
};

struct declaration_struct {
	struct bt_declaration p;
	GHashTable *fields_by_name;	/* Tuples (field name, field index) */
	struct declaration_scope *scope;
	GArray *fields;			/* Array of declaration_field */
};

struct definition_struct {
	struct bt_definition p;
	struct declaration_struct *declaration;
	GPtrArray *fields;		/* Array of pointers to struct bt_definition */
};

struct declaration_untagged_variant {
	struct bt_declaration p;
	GHashTable *fields_by_tag;	/* Tuples (field tag, field index) */
	struct declaration_scope *scope;
	GArray *fields;			/* Array of declaration_field */
};

struct declaration_variant {
	struct bt_declaration p;
	struct declaration_untagged_variant *untagged_variant;
	GArray *tag_name;		/* Array of GQuark */
};

/* A variant needs to be tagged to be defined. */
struct definition_variant {
	struct bt_definition p;
	struct declaration_variant *declaration;
	struct bt_definition *enum_tag;
	GPtrArray *fields;		/* Array of pointers to struct bt_definition */
	struct bt_definition *current_field;	/* Last field read */
};

struct declaration_array {
	struct bt_declaration p;
	size_t len;
	struct bt_declaration *elem;
	struct declaration_scope *scope;
};

struct definition_array {
	struct bt_definition p;
	struct declaration_array *declaration;
	GPtrArray *elems;		/* Array of pointers to struct bt_definition */
	GString *string;		/* String for encoded integer children */
};

struct declaration_sequence {
	struct bt_declaration p;
	GArray *length_name;		/* Array of GQuark */
	struct bt_declaration *elem;
	struct declaration_scope *scope;
};

struct definition_sequence {
	struct bt_definition p;
	struct declaration_sequence *declaration;
	struct definition_integer *length;
	GPtrArray *elems;		/* Array of pointers to struct bt_definition */
	GString *string;		/* String for encoded integer children */
};

int bt_register_declaration(GQuark declaration_name,
			 struct bt_declaration *declaration,
			 struct declaration_scope *scope);
struct bt_declaration *bt_lookup_declaration(GQuark declaration_name,
				struct declaration_scope *scope);

/*
 * Type scopes also contain a separate registry for struct, variant and
 * enum types. Those register types rather than type definitions, so
 * that a named variant can be declared without specifying its target
 * "choice" tag field immediately.
 */
int bt_register_struct_declaration(GQuark struct_name,
				struct declaration_struct *struct_declaration,
				struct declaration_scope *scope);
struct declaration_struct *
	bt_lookup_struct_declaration(GQuark struct_name,
				  struct declaration_scope *scope);
int bt_register_variant_declaration(GQuark variant_name,
			  struct declaration_untagged_variant *untagged_variant_declaration,
		          struct declaration_scope *scope);
struct declaration_untagged_variant *bt_lookup_variant_declaration(GQuark variant_name,
					 struct declaration_scope *scope);
int bt_register_enum_declaration(GQuark enum_name,
			      struct declaration_enum *enum_declaration,
			      struct declaration_scope *scope);
struct declaration_enum *
	bt_lookup_enum_declaration(GQuark enum_name,
			        struct declaration_scope *scope);

struct declaration_scope *
	bt_new_declaration_scope(struct declaration_scope *parent_scope);
void bt_free_declaration_scope(struct declaration_scope *scope);

/*
 * field_definition is for field definitions. They are registered into
 * definition scopes.
 */
struct bt_definition *
	bt_lookup_path_definition(GArray *cur_path,	/* array of GQuark */
			       GArray *lookup_path,	/* array of GQuark */
			       struct definition_scope *scope);
int bt_register_field_definition(GQuark field_name,
			      struct bt_definition *definition,
			      struct definition_scope *scope);
struct definition_scope *
	bt_new_definition_scope(struct definition_scope *parent_scope,
			     GQuark field_name, const char *root_name);
void bt_free_definition_scope(struct definition_scope *scope);

GQuark bt_new_definition_path(struct definition_scope *parent_scope,
			   GQuark field_name, const char *root_name);

static inline
int compare_definition_path(struct bt_definition *definition, GQuark path)
{
	return definition->path == path;
}

void bt_declaration_ref(struct bt_declaration *declaration);
void bt_declaration_unref(struct bt_declaration *declaration);

void bt_definition_ref(struct bt_definition *definition);
void bt_definition_unref(struct bt_definition *definition);

struct declaration_integer *bt_integer_declaration_new(size_t len, int byte_order,
				  int signedness, size_t alignment,
				  int base, enum ctf_string_encoding encoding,
				  struct ctf_clock *clock);
uint64_t bt_get_unsigned_int(const struct bt_definition *field);
int64_t bt_get_signed_int(const struct bt_definition *field);
int bt_get_int_signedness(const struct bt_definition *field);
int bt_get_int_byte_order(const struct bt_definition *field);
int bt_get_int_base(const struct bt_definition *field);
size_t bt_get_int_len(const struct bt_definition *field);	/* in bits */
bool bt_int_is_char(const struct bt_declaration *field);
enum ctf_string_encoding bt_get_int_encoding(const struct bt_definition *field);

/*
 * mantissa_len is the length of the number of bytes represented by the mantissa
 * (e.g. result of DBL_MANT_DIG). It includes the leading 1.
 */
struct declaration_float *bt_float_declaration_new(size_t mantissa_len,
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
GArray *bt_enum_uint_to_quark_set(const struct declaration_enum *enum_declaration,
			       uint64_t v);

/*
 * Returns a GArray of GQuark or NULL.
 * Caller must release the GArray with g_array_unref().
 */
GArray *bt_enum_int_to_quark_set(const struct declaration_enum *enum_declaration,
			      int64_t v);

/*
 * Returns a GArray of struct enum_range or NULL.
 * Callers do _not_ own the returned GArray (and therefore _don't_ need to
 * release it).
 */
GArray *bt_enum_quark_to_range_set(const struct declaration_enum *enum_declaration,
				GQuark q);
void bt_enum_signed_insert(struct declaration_enum *enum_declaration,
                        int64_t start, int64_t end, GQuark q);
void bt_enum_unsigned_insert(struct declaration_enum *enum_declaration,
			  uint64_t start, uint64_t end, GQuark q);
size_t bt_enum_get_nr_enumerators(struct declaration_enum *enum_declaration);

struct declaration_enum *
	bt_enum_declaration_new(struct declaration_integer *integer_declaration);

struct declaration_string *
	bt_string_declaration_new(enum ctf_string_encoding encoding);
char *bt_get_string(const struct bt_definition *field);
enum ctf_string_encoding bt_get_string_encoding(const struct bt_definition *field);

double bt_get_float(const struct bt_definition *field);

const struct bt_definition *bt_get_variant_field(struct bt_definition *definition);

struct declaration_struct *
	bt_struct_declaration_new(struct declaration_scope *parent_scope,
			       uint64_t min_align);
void bt_struct_declaration_add_field(struct declaration_struct *struct_declaration,
				  const char *field_name,
				  struct bt_declaration *field_declaration);
/*
 * Returns the index of a field within a structure.
 */
int bt_struct_declaration_lookup_field_index(struct declaration_struct *struct_declaration,
						    GQuark field_name);
/*
 * field returned only valid as long as the field structure is not appended to.
 */
struct declaration_field *
bt_struct_declaration_get_field_from_index(struct declaration_struct *struct_declaration,
					int index);
struct bt_definition *
bt_struct_definition_get_field_from_index(const struct definition_struct *struct_definition,
				       int index);
int bt_struct_rw(struct bt_stream_pos *pos, struct bt_definition *definition);
uint64_t bt_struct_declaration_len(const struct declaration_struct *struct_declaration);

/*
 * The tag enumeration is validated to ensure that it contains only mappings
 * from numeric values to a single tag. Overlapping tag value ranges are
 * therefore forbidden.
 */
struct declaration_untagged_variant *bt_untagged_bt_variant_declaration_new(
		struct declaration_scope *parent_scope);
struct declaration_variant *bt_variant_declaration_new(struct declaration_untagged_variant *untagged_variant,
		const char *tag);

void bt_untagged_variant_declaration_add_field(struct declaration_untagged_variant *untagged_variant_declaration,
		const char *field_name,
		struct bt_declaration *field_declaration);
struct declaration_field *
	bt_untagged_variant_declaration_get_field_from_tag(struct declaration_untagged_variant *untagged_variant_declaration,
		GQuark tag);
/*
 * Returns 0 on success, -EPERM on error.
 */
int variant_definition_set_tag(struct definition_variant *variant,
			       struct bt_definition *enum_tag);
/*
 * Returns the field selected by the current tag value.
 * field returned only valid as long as the variant structure is not appended
 * to.
 */
struct bt_definition *bt_variant_get_current_field(struct definition_variant *variant);
int bt_variant_rw(struct bt_stream_pos *pos, struct bt_definition *definition);

/*
 * elem_declaration passed as parameter now belongs to the array. No
 * need to free it explicitly. "len" is the number of elements in the
 * array.
 */
struct declaration_array *
	bt_array_declaration_new(size_t len, struct bt_declaration *elem_declaration,
		struct declaration_scope *parent_scope);
uint64_t bt_array_len(struct definition_array *array);
struct bt_definition *bt_array_index(struct definition_array *array, uint64_t i);
int bt_array_rw(struct bt_stream_pos *pos, struct bt_definition *definition);
GString *bt_get_char_array(const struct bt_definition *field);
int bt_get_array_len(const struct bt_definition *field);

/*
 * int_declaration and elem_declaration passed as parameter now belong
 * to the sequence. No need to free them explicitly.
 */
struct declaration_sequence *
	bt_sequence_declaration_new(const char *length_name,
		struct bt_declaration *elem_declaration,
		struct declaration_scope *parent_scope);
uint64_t bt_sequence_len(struct definition_sequence *sequence);
struct bt_definition *bt_sequence_index(struct definition_sequence *sequence, uint64_t i);
int bt_sequence_rw(struct bt_stream_pos *pos, struct bt_definition *definition);

/*
 * in: path (dot separated), out: q (GArray of GQuark)
 */
void bt_append_scope_path(const char *path, GArray *q);

/*
 * Lookup helpers.
 */
struct bt_definition *bt_lookup_definition(const struct bt_definition *definition,
				     const char *field_name);
struct bt_definition *bt_lookup_definition_by_quark(const struct bt_definition *definition,
				     GQuark quark);
struct definition_integer *bt_lookup_integer(const struct bt_definition *definition,
					  const char *field_name,
					  int signedness);
struct definition_enum *bt_lookup_enum(const struct bt_definition *definition,
				    const char *field_name,
				    int signedness);
struct bt_definition *bt_lookup_variant(const struct bt_definition *definition,
				  const char *field_name);

static inline
const char *rem_(const char *str)
{
	if (str[0] == '_')
		return &str[1];
	else
		return str;
}

#endif /* _BABELTRACE_TYPES_H */
