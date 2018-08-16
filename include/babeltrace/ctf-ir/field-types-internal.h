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

#define BT_ASSERT_PRE_FT_HAS_ID(_ft, _type_id, _name)		\
	BT_ASSERT_PRE(((struct bt_field_type *) (_ft))->id == (_type_id), \
		_name " has the wrong type ID: expected-type-id=%s, "	\
		"%![ft-]+F", bt_common_field_type_id_string(_type_id), (_ft))

#define BT_ASSERT_PRE_FT_HOT(_ft, _name)				\
	BT_ASSERT_PRE_HOT((_ft), (_name), ": %!+F", (_ft))

#define BT_FIELD_TYPE_STRUCTURE_FIELD_AT_INDEX(_ft, _index)	\
	(&g_array_index(((struct bt_field_type_structure *) (_ft))->fields, \
		struct bt_field_type_structure_field, (_index)))

#define BT_FIELD_TYPE_VARIANT_CHOICE_AT_INDEX(_ft, _index)	\
	(&g_array_index(((struct bt_field_type_variant *) (_ft))->choices, \
		struct bt_field_type_variant_choice, (_index)))

struct bt_field;
struct bt_field_type;

typedef void (*bt_field_type_method_freeze)(
		struct bt_field_type *);
typedef int (*bt_field_type_method_validate)(
		struct bt_field_type *);
typedef void (*bt_field_type_method_set_byte_order)(
		struct bt_field_type *, enum bt_byte_order);
typedef struct bt_field_type *(*bt_field_type_method_copy)(
		struct bt_field_type *);
typedef int (*bt_field_type_method_compare)(
		struct bt_field_type *,
		struct bt_field_type *);

struct bt_field_type_methods {
	bt_field_type_method_freeze freeze;
	bt_field_type_method_validate validate;
	bt_field_type_method_set_byte_order set_byte_order;
	bt_field_type_method_copy copy;
	bt_field_type_method_compare compare;
};

struct bt_field_type {
	struct bt_object base;
	enum bt_field_type_id id;
	unsigned int alignment;

	/* Virtual table */
	struct bt_field_type_methods *methods;

	/*
	 * A type can't be modified once it is added to an event or after a
	 * a field has been instanciated from it.
	 */
	int frozen;

	/*
	 * This flag indicates if the field type is valid. A valid
	 * field type is _always_ frozen. All the nested field types of
	 * a valid field type are also valid (and thus frozen).
	 */
	int valid;
};

struct bt_field_type_integer {
	struct bt_field_type common;

	/* Owned by this */
	struct bt_clock_class *mapped_clock_class;

	enum bt_byte_order user_byte_order;
	bt_bool is_signed;
	unsigned int size;
	enum bt_integer_base base;
	enum bt_string_encoding encoding;
};

struct enumeration_mapping {
	union {
		uint64_t _unsigned;
		int64_t _signed;
	} range_start;
	union {
		uint64_t _unsigned;
		int64_t _signed;
	} range_end;
	GQuark string;
};

struct bt_field_type_enumeration {
	struct bt_field_type common;

	/* Owned by this */
	struct bt_field_type_integer *container_ft;

	/* Array of `struct enumeration_mapping *`, owned by this */
	GPtrArray *entries;

	/* Only set during validation */
	bt_bool has_overlapping_ranges;
};

enum bt_field_type_enumeration_mapping_iterator_type {
	ITERATOR_BY_NAME,
	ITERATOR_BY_SIGNED_VALUE,
	ITERATOR_BY_UNSIGNED_VALUE,
};

struct bt_field_type_enumeration_mapping_iterator {
	struct bt_object base;

	/* Owned by this */
	struct bt_field_type_enumeration *enumeration_ft;

	enum bt_field_type_enumeration_mapping_iterator_type type;
	int index;
	union {
		GQuark name_quark;
		int64_t signed_value;
		uint64_t unsigned_value;
	} u;
};

struct bt_field_type_floating_point {
	struct bt_field_type common;
	enum bt_byte_order user_byte_order;
	unsigned int exp_dig;
	unsigned int mant_dig;
};

struct bt_field_type_structure_field {
	GQuark name;

	/* Owned by this */
	struct bt_field_type *type;
};

struct bt_field_type_structure {
	struct bt_field_type common;
	GHashTable *field_name_to_index;

	/*
	 * Array of `struct bt_field_type_structure_field`,
	 * owned by this
	 */
	GArray *fields;
};

struct bt_field_type_variant_choice_range {
	union {
		int64_t i;
		uint64_t u;
	} lower;
	union {
		int64_t i;
		uint64_t u;
	} upper;
};

struct bt_field_type_variant_choice {
	GQuark name;

	/* Owned by this */
	struct bt_field_type *type;

	/* Array of `struct bt_field_type_variant_choice_range` */
	GArray *ranges;
};

struct bt_field_type_variant {
	struct bt_field_type common;
	GString *tag_name;
	bool choices_up_to_date;

	/* Owned by this */
	struct bt_field_type_enumeration *tag_ft;

	/* Owned by this */
	struct bt_field_path *tag_field_path;

	GHashTable *choice_name_to_index;

	/*
	 * Array of `struct bt_field_type_variant_choice`,
	 * owned by this */
	GArray *choices;
};

struct bt_field_type_array {
	struct bt_field_type common;

	/* Owned by this */
	struct bt_field_type *element_ft;

	unsigned int length;
};

struct bt_field_type_sequence {
	struct bt_field_type common;

	/* Owned by this */
	struct bt_field_type *element_ft;

	GString *length_field_name;

	/* Owned by this */
	struct bt_field_path *length_field_path;
};

struct bt_field_type_string {
	struct bt_field_type common;
	enum bt_string_encoding encoding;
};

typedef struct bt_field *(* bt_field_create_func)(
		struct bt_field_type *);

BT_ASSERT_FUNC
static inline bool bt_field_type_has_known_id(
		struct bt_field_type *ft)
{
	return (int) ft->id > BT_FIELD_TYPE_ID_UNKNOWN ||
		(int) ft->id < BT_FIELD_TYPE_ID_NR;
}

BT_HIDDEN
int bt_field_type_variant_update_choices(
		struct bt_field_type *ft);

BT_HIDDEN
void bt_field_type_freeze(struct bt_field_type *ft);

BT_HIDDEN
int bt_field_type_validate(struct bt_field_type *ft);

BT_HIDDEN
int bt_field_type_sequence_set_length_field_path(
		struct bt_field_type *ft, struct bt_field_path *path);

BT_HIDDEN
int bt_field_type_variant_set_tag_field_path(
		struct bt_field_type *ft,
		struct bt_field_path *path);

BT_HIDDEN
int bt_field_type_variant_set_tag_field_type(
		struct bt_field_type *ft,
		struct bt_field_type *tag_ft);

BT_HIDDEN
int64_t bt_field_type_get_field_count(struct bt_field_type *ft);

BT_HIDDEN
struct bt_field_type *bt_field_type_borrow_field_at_index(
		struct bt_field_type *ft, int index);

BT_HIDDEN
int bt_field_type_get_field_index(struct bt_field_type *ft,
		const char *name);

BT_HIDDEN
int bt_field_type_validate_single_clock_class(
		struct bt_field_type *ft,
		struct bt_clock_class **expected_clock_class);

BT_HIDDEN
int64_t bt_field_type_variant_find_choice_index(
		struct bt_field_type *ft, uint64_t uval,
		bool is_signed);

#endif /* BABELTRACE_CTF_IR_FIELD_TYPES_INTERNAL_H */
