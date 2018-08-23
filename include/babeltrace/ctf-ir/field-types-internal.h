#ifndef BABELTRACE_CTF_IR_FIELD_TYPES_INTERNAL_H
#define BABELTRACE_CTF_IR_FIELD_TYPES_INTERNAL_H

/*
 * BabelTrace - CTF IR: Event field types internal
 *
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Author: Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/ctf-ir/clock-class.h>
#include <babeltrace/ctf-ir/field-types.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/types.h>
#include <stdint.h>
#include <glib.h>

#define BT_ASSERT_PRE_FT_IS_INT(_ft, _name)				\
	BT_ASSERT_PRE(							\
		((struct bt_field_type *) (_ft))->id == BT_FIELD_TYPE_ID_UNSIGNED_INTEGER || \
		((struct bt_field_type *) (_ft))->id == BT_FIELD_TYPE_ID_SIGNED_INTEGER || \
		((struct bt_field_type *) (_ft))->id == BT_FIELD_TYPE_ID_UNSIGNED_ENUMERATION || \
		((struct bt_field_type *) (_ft))->id == BT_FIELD_TYPE_ID_SIGNED_ENUMERATION, \
		_name " is not an integer field type: %![ft-]+F", (_ft))

#define BT_ASSERT_PRE_FT_IS_UNSIGNED_INT(_ft, _name)			\
	BT_ASSERT_PRE(							\
		((struct bt_field_type *) (_ft))->id == BT_FIELD_TYPE_ID_UNSIGNED_INTEGER || \
		((struct bt_field_type *) (_ft))->id == BT_FIELD_TYPE_ID_UNSIGNED_ENUMERATION, \
		_name " is not an unsigned integer field type: %![ft-]+F", (_ft))

#define BT_ASSERT_PRE_FT_IS_ENUM(_ft, _name)				\
	BT_ASSERT_PRE(							\
		((struct bt_field_type *) (_ft))->id == BT_FIELD_TYPE_ID_UNSIGNED_ENUMERATION || \
		((struct bt_field_type *) (_ft))->id == BT_FIELD_TYPE_ID_SIGNED_ENUMERATION, \
		_name " is not an enumeration field type: %![ft-]+F", (_ft))

#define BT_ASSERT_PRE_FT_IS_ARRAY(_ft, _name)				\
	BT_ASSERT_PRE(							\
		((struct bt_field_type *) (_ft))->id == BT_FIELD_TYPE_ID_STATIC_ARRAY || \
		((struct bt_field_type *) (_ft))->id == BT_FIELD_TYPE_ID_DYNAMIC_ARRAY, \
		_name " is not an array field type: %![ft-]+F", (_ft))

#define BT_ASSERT_PRE_FT_HAS_ID(_ft, _id, _name)			\
	BT_ASSERT_PRE(((struct bt_field_type *) (_ft))->id == (_id), 	\
		_name " has the wrong ID: expected-id=%s, "		\
		"%![ft-]+F", bt_common_field_type_id_string(_id), (_ft))

#define BT_ASSERT_PRE_FT_HOT(_ft, _name)				\
	BT_ASSERT_PRE_HOT((struct bt_field_type *) (_ft),		\
		(_name), ": %!+F", (_ft))

#define BT_FIELD_TYPE_NAMED_FT_AT_INDEX(_ft, _index)		\
	(&g_array_index(((struct bt_field_type_named_field_types_container *) (_ft))->named_fts, \
		struct bt_named_field_type, (_index)))

#define BT_FIELD_TYPE_ENUM_MAPPING_AT_INDEX(_ft, _index)		\
	(&g_array_index(((struct bt_field_type_enumeration *) (_ft))->mappings, \
		struct bt_field_type_enumeration_mapping, (_index)))

#define BT_FIELD_TYPE_ENUM_MAPPING_RANGE_AT_INDEX(_mapping, _index)	\
	(&g_array_index((_mapping)->ranges,				\
		struct bt_field_type_enumeration_mapping_range, (_index)))

struct bt_field;
struct bt_field_type;

struct bt_field_type {
	struct bt_object base;
	enum bt_field_type_id id;
	bool frozen;

	/*
	 * Only used in developer mode, this flag indicates whether or
	 * not this field type is part of a trace.
	 */
	bool part_of_trace;
};

struct bt_field_type_integer {
	struct bt_field_type common;

	/*
	 * Value range of fields built from this integer field type:
	 * this is an equivalent integer size in bits. More formally,
	 * `range` is `n` in:
	 *
	 * Unsigned range: [0, 2^n - 1]
	 * Signed range: [-2^(n - 1), 2^(n - 1) - 1]
	 */
	uint64_t range;

	enum bt_field_type_integer_preferred_display_base base;
};

struct bt_field_type_enumeration_mapping_range {
	union {
		uint64_t u;
		int64_t i;
	} lower;

	union {
		uint64_t u;
		int64_t i;
	} upper;
};

struct bt_field_type_enumeration_mapping {
	GString *label;

	/* Array of `struct bt_field_type_enumeration_mapping_range` */
	GArray *ranges;
};

struct bt_field_type_enumeration {
	struct bt_field_type_integer common;

	/* Array of `struct bt_field_type_enumeration_mapping *` */
	GArray *mappings;

	/*
	 * This is an array of `const char *` which acts as a temporary
	 * (potentially growing) buffer for
	 * bt_field_type_unsigned_enumeration_get_mapping_labels_by_value()
	 * and
	 * bt_field_type_signed_enumeration_get_mapping_labels_by_value().
	 *
	 * The actual strings are owned by the mappings above.
	 */
	GPtrArray *label_buf;
};

struct bt_field_type_real {
	struct bt_field_type common;
	bool is_single_precision;
};

struct bt_field_type_string {
	struct bt_field_type common;
};

/* A named field type is a (name, field type) pair */
struct bt_named_field_type {
	GString *name;

	/* Owned by this */
	struct bt_field_type *ft;
};

/*
 * This is the base field type for a container of named field types.
 * Structure and variant field types inherit this.
 */
struct bt_field_type_named_field_types_container {
	struct bt_field_type common;

	/*
	 * Key: `const char *`, not owned by this (owned by named field
	 * type objects contained in `named_fts` below).
	 */
	GHashTable *name_to_index;

	/* Array of `struct bt_named_field_type` */
	GArray *named_fts;
};

struct bt_field_type_structure {
	struct bt_field_type_named_field_types_container common;
};

struct bt_field_type_array {
	struct bt_field_type common;

	/* Owned by this */
	struct bt_field_type *element_ft;
};

struct bt_field_type_static_array {
	struct bt_field_type_array common;
	uint64_t length;
};

struct bt_field_type_dynamic_array {
	struct bt_field_type_array common;

	/* Weak: never dereferenced, only use to find it elsewhere */
	struct bt_field_type *length_ft;

	/* Owned by this */
	struct bt_field_path *length_field_path;
};

struct bt_field_type_variant {
	struct bt_field_type_named_field_types_container common;

	/* Weak: never dereferenced, only use to find it elsewhere */
	struct bt_field_type *selector_ft;

	/* Owned by this */
	struct bt_field_path *selector_field_path;
};

static inline
bool bt_field_type_has_known_id(struct bt_field_type *ft)
{
	return ft->id >= BT_FIELD_TYPE_ID_UNSIGNED_INTEGER &&
		ft->id <= BT_FIELD_TYPE_ID_VARIANT;
}

BT_HIDDEN
void _bt_field_type_freeze(struct bt_field_type *field_type);

#ifdef BT_DEV_MODE
# define bt_field_type_freeze		_bt_field_type_freeze
#else
# define bt_field_type_freeze(_ft)
#endif

/*
 * This function recursively marks `field_type` and its children as
 * being part of a trace. This is used to validate that all field types
 * are used at a single location within trace objects even if they are
 * shared objects for other purposes.
 */
BT_HIDDEN
void _bt_field_type_make_part_of_trace(struct bt_field_type *field_type);

#ifdef BT_DEV_MODE
# define bt_field_type_make_part_of_trace	_bt_field_type_make_part_of_trace
#else
# define bt_field_type_make_part_of_trace(_ft)	((void) _ft)
#endif

#endif /* BABELTRACE_CTF_IR_FIELD_TYPES_INTERNAL_H */
