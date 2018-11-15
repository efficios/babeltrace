#ifndef BABELTRACE_TRACE_IR_FIELD_CLASSES_INTERNAL_H
#define BABELTRACE_TRACE_IR_FIELD_CLASSES_INTERNAL_H

/*
 * BabelTrace - Trace IR: Event field classes internal
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
#include <babeltrace/trace-ir/clock-class.h>
#include <babeltrace/trace-ir/field-classes.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/types.h>
#include <stdint.h>
#include <glib.h>

#define BT_ASSERT_PRE_FC_IS_INT(_fc, _name)				\
	BT_ASSERT_PRE(							\
		((struct bt_field_class *) (_fc))->id == BT_FIELD_CLASS_ID_UNSIGNED_INTEGER || \
		((struct bt_field_class *) (_fc))->id == BT_FIELD_CLASS_ID_SIGNED_INTEGER || \
		((struct bt_field_class *) (_fc))->id == BT_FIELD_CLASS_ID_UNSIGNED_ENUMERATION || \
		((struct bt_field_class *) (_fc))->id == BT_FIELD_CLASS_ID_SIGNED_ENUMERATION, \
		_name " is not an integer field class: %![fc-]+F", (_fc))

#define BT_ASSERT_PRE_FC_IS_UNSIGNED_INT(_fc, _name)			\
	BT_ASSERT_PRE(							\
		((struct bt_field_class *) (_fc))->id == BT_FIELD_CLASS_ID_UNSIGNED_INTEGER || \
		((struct bt_field_class *) (_fc))->id == BT_FIELD_CLASS_ID_UNSIGNED_ENUMERATION, \
		_name " is not an unsigned integer field class: %![fc-]+F", (_fc))

#define BT_ASSERT_PRE_FC_IS_ENUM(_fc, _name)				\
	BT_ASSERT_PRE(							\
		((struct bt_field_class *) (_fc))->id == BT_FIELD_CLASS_ID_UNSIGNED_ENUMERATION || \
		((struct bt_field_class *) (_fc))->id == BT_FIELD_CLASS_ID_SIGNED_ENUMERATION, \
		_name " is not an enumeration field class: %![fc-]+F", (_fc))

#define BT_ASSERT_PRE_FC_IS_ARRAY(_fc, _name)				\
	BT_ASSERT_PRE(							\
		((struct bt_field_class *) (_fc))->id == BT_FIELD_CLASS_ID_STATIC_ARRAY || \
		((struct bt_field_class *) (_fc))->id == BT_FIELD_CLASS_ID_DYNAMIC_ARRAY, \
		_name " is not an array field class: %![fc-]+F", (_fc))

#define BT_ASSERT_PRE_FC_HAS_ID(_fc, _id, _name)			\
	BT_ASSERT_PRE(((struct bt_field_class *) (_fc))->id == (_id), 	\
		_name " has the wrong ID: expected-id=%s, "		\
		"%![fc-]+F", bt_common_field_class_id_string(_id), (_fc))

#define BT_ASSERT_PRE_FC_HOT(_fc, _name)				\
	BT_ASSERT_PRE_HOT((struct bt_field_class *) (_fc),		\
		(_name), ": %!+F", (_fc))

#define BT_FIELD_CLASS_NAMED_FC_AT_INDEX(_fc, _index)		\
	(&g_array_index(((struct bt_field_class_named_field_class_container *) (_fc))->named_fcs, \
		struct bt_named_field_class, (_index)))

#define BT_FIELD_CLASS_ENUM_MAPPING_AT_INDEX(_fc, _index)		\
	(&g_array_index(((struct bt_field_class_enumeration *) (_fc))->mappings, \
		struct bt_field_class_enumeration_mapping, (_index)))

#define BT_FIELD_CLASS_ENUM_MAPPING_RANGE_AT_INDEX(_mapping, _index)	\
	(&g_array_index((_mapping)->ranges,				\
		struct bt_field_class_enumeration_mapping_range, (_index)))

struct bt_field;
struct bt_field_class;

struct bt_field_class {
	struct bt_object base;
	enum bt_field_class_id id;
	bool frozen;

	/*
	 * Only used in developer mode, this flag indicates whether or
	 * not this field class is part of a trace.
	 */
	bool part_of_trace;
};

struct bt_field_class_integer {
	struct bt_field_class common;

	/*
	 * Value range of fields built from this integer field class:
	 * this is an equivalent integer size in bits. More formally,
	 * `range` is `n` in:
	 *
	 * Unsigned range: [0, 2^n - 1]
	 * Signed range: [-2^(n - 1), 2^(n - 1) - 1]
	 */
	uint64_t range;

	enum bt_field_class_integer_preferred_display_base base;
};

struct bt_field_class_enumeration_mapping_range {
	union {
		uint64_t u;
		int64_t i;
	} lower;

	union {
		uint64_t u;
		int64_t i;
	} upper;
};

struct bt_field_class_enumeration_mapping {
	GString *label;

	/* Array of `struct bt_field_class_enumeration_mapping_range` */
	GArray *ranges;
};

struct bt_field_class_enumeration {
	struct bt_field_class_integer common;

	/* Array of `struct bt_field_class_enumeration_mapping *` */
	GArray *mappings;

	/*
	 * This is an array of `const char *` which acts as a temporary
	 * (potentially growing) buffer for
	 * bt_field_class_unsigned_enumeration_get_mapping_labels_by_value()
	 * and
	 * bt_field_class_signed_enumeration_get_mapping_labels_by_value().
	 *
	 * The actual strings are owned by the mappings above.
	 */
	GPtrArray *label_buf;
};

struct bt_field_class_real {
	struct bt_field_class common;
	bool is_single_precision;
};

struct bt_field_class_string {
	struct bt_field_class common;
};

/* A named field class is a (name, field class) pair */
struct bt_named_field_class {
	GString *name;

	/* Owned by this */
	struct bt_field_class *fc;
};

/*
 * This is the base field class for a container of named field classes.
 * Structure and variant field classes inherit this.
 */
struct bt_field_class_named_field_class_container {
	struct bt_field_class common;

	/*
	 * Key: `const char *`, not owned by this (owned by named field
	 * type objects contained in `named_fcs` below).
	 */
	GHashTable *name_to_index;

	/* Array of `struct bt_named_field_class` */
	GArray *named_fcs;
};

struct bt_field_class_structure {
	struct bt_field_class_named_field_class_container common;
};

struct bt_field_class_array {
	struct bt_field_class common;

	/* Owned by this */
	struct bt_field_class *element_fc;
};

struct bt_field_class_static_array {
	struct bt_field_class_array common;
	uint64_t length;
};

struct bt_field_class_dynamic_array {
	struct bt_field_class_array common;

	/* Weak: never dereferenced, only use to find it elsewhere */
	struct bt_field_class *length_fc;

	/* Owned by this */
	struct bt_field_path *length_field_path;
};

struct bt_field_class_variant {
	struct bt_field_class_named_field_class_container common;

	/* Weak: never dereferenced, only use to find it elsewhere */
	struct bt_field_class *selector_fc;

	/* Owned by this */
	struct bt_field_path *selector_field_path;
};

static inline
bool bt_field_class_has_known_id(struct bt_field_class *fc)
{
	return fc->id >= BT_FIELD_CLASS_ID_UNSIGNED_INTEGER &&
		fc->id <= BT_FIELD_CLASS_ID_VARIANT;
}

BT_HIDDEN
void _bt_field_class_freeze(struct bt_field_class *field_class);

#ifdef BT_DEV_MODE
# define bt_field_class_freeze		_bt_field_class_freeze
#else
# define bt_field_class_freeze(_fc)
#endif

/*
 * This function recursively marks `field_class` and its children as
 * being part of a trace. This is used to validate that all field classes
 * are used at a single location within trace objects even if they are
 * shared objects for other purposes.
 */
BT_HIDDEN
void _bt_field_class_make_part_of_trace(struct bt_field_class *field_class);

#ifdef BT_DEV_MODE
# define bt_field_class_make_part_of_trace	_bt_field_class_make_part_of_trace
#else
# define bt_field_class_make_part_of_trace(_fc)	((void) _fc)
#endif

#endif /* BABELTRACE_TRACE_IR_FIELD_CLASSES_INTERNAL_H */
