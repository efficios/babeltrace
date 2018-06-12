#ifndef BABELTRACE_CTF_IR_FIELDS_INTERNAL_H
#define BABELTRACE_CTF_IR_FIELDS_INTERNAL_H

/*
 * Babeltrace - CTF IR: Event Fields internal
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
#include <babeltrace/common-internal.h>
#include <babeltrace/ctf-ir/field-types-internal.h>
#include <babeltrace/ctf-ir/utils-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/types.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>
#include <glib.h>

#define BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(_field, _type_id, _name)	\
	BT_ASSERT_PRE((_field)->type->id == ((int) (_type_id)),		\
		_name " has the wrong type ID: expected-type-id=%s, "	\
		"%![field-]+_f",					\
		bt_common_field_type_id_string((int) (_type_id)), (_field))

#define BT_ASSERT_PRE_FIELD_COMMON_IS_SET(_field, _name)		\
	BT_ASSERT_PRE(bt_field_common_is_set_recursive(_field),		\
		_name " is not set: %!+_f", (_field))

#define BT_ASSERT_PRE_FIELD_COMMON_HOT(_field, _name)			\
	BT_ASSERT_PRE_HOT((_field), (_name), ": +%!+_f", (_field))

struct bt_field;
struct bt_field_common;

typedef void (*bt_field_common_method_set_is_frozen)(struct bt_field_common *,
		bool);
typedef int (*bt_field_common_method_validate)(struct bt_field_common *);
typedef struct bt_field_common *(*bt_field_common_method_copy)(
		struct bt_field_common *);
typedef bt_bool (*bt_field_common_method_is_set)(struct bt_field_common *);
typedef void (*bt_field_common_method_reset)(struct bt_field_common *);

struct bt_field_common_methods {
	bt_field_common_method_set_is_frozen set_is_frozen;
	bt_field_common_method_validate validate;
	bt_field_common_method_copy copy;
	bt_field_common_method_is_set is_set;
	bt_field_common_method_reset reset;
};

struct bt_field_common {
	struct bt_object base;
	struct bt_field_type_common *type;
	struct bt_field_common_methods *methods;
	bool payload_set;
	bool frozen;

	/*
	 * Specialized data for either CTF IR or CTF writer APIs.
	 * See comment in `field-types-internal.h` for more details.
	 */
	union {
		struct {
		} ir;
		struct {
			void *serialize_func;
		} writer;
	} spec;
};

struct bt_field_common_integer {
	struct bt_field_common common;
	union {
		int64_t signd;
		uint64_t unsignd;
	} payload;
};

struct bt_field_common_floating_point {
	struct bt_field_common common;
	double payload;
};

struct bt_field_common_structure {
	struct bt_field_common common;

	/* Array of `struct bt_field_common *`, owned by this */
	GPtrArray *fields;
};

struct bt_field_common_variant {
	struct bt_field_common common;

	union {
		uint64_t u;
		int64_t i;
	} tag_value;

	/* Weak: belongs to `choices` below */
	struct bt_field_common *current_field;

	/* Array of `struct bt_field_common *`, owned by this */
	GPtrArray *fields;
};

struct bt_field_common_array {
	struct bt_field_common common;

	/* Array of `struct bt_field_common *`, owned by this */
	GPtrArray *elements;
};

struct bt_field_common_sequence {
	struct bt_field_common common;

	/*
	 * This is the true sequence field's length: its value can be
	 * less than `elements->len` below because we never shrink the
	 * array of elements to avoid reallocation.
	 */
	uint64_t length;

	/* Array of `struct bt_field_common *`, owned by this */
	GPtrArray *elements;
};

struct bt_field_common_string {
	struct bt_field_common common;
	GArray *buf;
	size_t size;
};

struct bt_field_enumeration {
	struct bt_field_common_integer common;
};

BT_HIDDEN
struct bt_field_common *bt_field_common_copy(struct bt_field_common *field);

BT_HIDDEN
int bt_field_common_structure_initialize(struct bt_field_common *field,
		struct bt_field_type_common *type,
		bool is_shared, bt_object_release_func release_func,
		struct bt_field_common_methods *methods,
		bt_field_common_create_func field_create_func,
		GDestroyNotify field_release_func);

BT_HIDDEN
int bt_field_common_array_initialize(struct bt_field_common *field,
		struct bt_field_type_common *type,
		bool is_shared, bt_object_release_func release_func,
		struct bt_field_common_methods *methods,
		bt_field_common_create_func field_create_func,
		GDestroyNotify field_destroy_func);

BT_HIDDEN
int bt_field_common_sequence_initialize(struct bt_field_common *field,
		struct bt_field_type_common *type,
		bool is_shared, bt_object_release_func release_func,
		struct bt_field_common_methods *methods,
		GDestroyNotify field_destroy_func);

BT_HIDDEN
int bt_field_common_variant_initialize(struct bt_field_common *field,
		struct bt_field_type_common *type,
		bool is_shared, bt_object_release_func release_func,
		struct bt_field_common_methods *methods,
		bt_field_common_create_func field_create_func,
		GDestroyNotify field_release_func);

BT_HIDDEN
int bt_field_common_string_initialize(struct bt_field_common *field,
		struct bt_field_type_common *type,
		bool is_shared, bt_object_release_func release_func,
		struct bt_field_common_methods *methods);

BT_HIDDEN
int bt_field_common_generic_validate(struct bt_field_common *field);

BT_HIDDEN
int bt_field_common_structure_validate_recursive(struct bt_field_common *field);

BT_HIDDEN
int bt_field_common_variant_validate_recursive(struct bt_field_common *field);

BT_HIDDEN
int bt_field_common_array_validate_recursive(struct bt_field_common *field);

BT_HIDDEN
int bt_field_common_sequence_validate_recursive(struct bt_field_common *field);

BT_HIDDEN
void bt_field_common_generic_reset(struct bt_field_common *field);

BT_HIDDEN
void bt_field_common_structure_reset_recursive(struct bt_field_common *field);

BT_HIDDEN
void bt_field_common_variant_reset_recursive(struct bt_field_common *field);

BT_HIDDEN
void bt_field_common_array_reset_recursive(struct bt_field_common *field);

BT_HIDDEN
void bt_field_common_sequence_reset_recursive(struct bt_field_common *field);

BT_HIDDEN
void bt_field_common_generic_set_is_frozen(struct bt_field_common *field,
		bool is_frozen);

BT_HIDDEN
void bt_field_common_structure_set_is_frozen_recursive(
		struct bt_field_common *field, bool is_frozen);

BT_HIDDEN
void bt_field_common_variant_set_is_frozen_recursive(
		struct bt_field_common *field, bool is_frozen);

BT_HIDDEN
void bt_field_common_array_set_is_frozen_recursive(
		struct bt_field_common *field, bool is_frozen);

BT_HIDDEN
void bt_field_common_sequence_set_is_frozen_recursive(
		struct bt_field_common *field, bool is_frozen);

BT_HIDDEN
void _bt_field_common_set_is_frozen_recursive(struct bt_field_common *field,
		bool is_frozen);

BT_HIDDEN
bt_bool bt_field_common_generic_is_set(struct bt_field_common *field);

BT_HIDDEN
bt_bool bt_field_common_structure_is_set_recursive(
		struct bt_field_common *field);

BT_HIDDEN
bt_bool bt_field_common_variant_is_set_recursive(struct bt_field_common *field);

BT_HIDDEN
bt_bool bt_field_common_array_is_set_recursive(struct bt_field_common *field);

BT_HIDDEN
bt_bool bt_field_common_sequence_is_set_recursive(struct bt_field_common *field);

#ifdef BT_DEV_MODE
# define bt_field_common_validate_recursive		_bt_field_common_validate_recursive
# define bt_field_common_set_is_frozen_recursive	_bt_field_common_set_is_frozen_recursive
# define bt_field_common_is_set_recursive		_bt_field_common_is_set_recursive
# define bt_field_common_reset_recursive		_bt_field_common_reset_recursive
# define bt_field_common_set				_bt_field_common_set
# define bt_field_validate_recursive			_bt_field_validate_recursive
# define bt_field_set_is_frozen_recursive		_bt_field_set_is_frozen_recursive
# define bt_field_is_set_recursive			_bt_field_is_set_recursive
# define bt_field_reset_recursive			_bt_field_reset_recursive
# define bt_field_set					_bt_field_set
#else
# define bt_field_common_validate_recursive(_field)	(-1)
# define bt_field_common_set_is_frozen_recursive(_field, _is_frozen)
# define bt_field_common_is_set_recursive(_field)	(BT_FALSE)
# define bt_field_common_reset_recursive(_field)
# define bt_field_common_set(_field, _val)
# define bt_field_validate_recursive(_field)		(-1)
# define bt_field_set_is_frozen_recursive(_field, _is_frozen)
# define bt_field_is_set_recursive(_field)		(BT_FALSE)
# define bt_field_reset_recursive(_field)
# define bt_field_set(_field, _val)
#endif

BT_ASSERT_FUNC
static inline bool field_type_common_has_known_id(
		struct bt_field_type_common *ft)
{
	return (int) ft->id > BT_FIELD_TYPE_ID_UNKNOWN ||
		(int) ft->id < BT_FIELD_TYPE_ID_NR;
}

static inline
int _bt_field_common_validate_recursive(struct bt_field_common *field)
{
	int ret = 0;

	if (!field) {
		BT_ASSERT_PRE_MSG("%s", "Invalid field: field is NULL.");
		ret = -1;
		goto end;
	}

	BT_ASSERT(field_type_common_has_known_id(field->type));

	if (field->methods->validate) {
		ret = field->methods->validate(field);
	}

end:
	return ret;
}

static inline
void _bt_field_common_reset_recursive(struct bt_field_common *field)
{
	BT_ASSERT(field);
	BT_ASSERT(field->methods->reset);
	field->methods->reset(field);
}

static inline
void _bt_field_common_set(struct bt_field_common *field, bool value)
{
	BT_ASSERT(field);
	field->payload_set = value;
}

static inline
bt_bool _bt_field_common_is_set_recursive(struct bt_field_common *field)
{
	bt_bool is_set = BT_FALSE;

	if (!field) {
		goto end;
	}

	BT_ASSERT(field_type_common_has_known_id(field->type));
	BT_ASSERT(field->methods->is_set);
	is_set = field->methods->is_set(field);

end:
	return is_set;
}

static inline
void bt_field_common_initialize(struct bt_field_common *field,
		struct bt_field_type_common *ft, bool is_shared,
		bt_object_release_func release_func,
		struct bt_field_common_methods *methods)
{
	BT_ASSERT(field);
	BT_ASSERT(ft);
	bt_object_init(&field->base, is_shared, release_func);
	field->methods = methods;
	field->type = bt_get(ft);
}

static inline
struct bt_field_type_common *bt_field_common_borrow_type(
		struct bt_field_common *field)
{
	struct bt_field_type_common *ret = NULL;

	BT_ASSERT_PRE_NON_NULL(field, "Field");
	ret = field->type;
	return ret;
}

static inline
int64_t bt_field_common_sequence_get_length(struct bt_field_common *field)
{
	struct bt_field_common_sequence *sequence = BT_FROM_COMMON(field);

	BT_ASSERT_PRE_NON_NULL(field, "Sequence field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(field, BT_FIELD_TYPE_ID_SEQUENCE,
		"Field");
	return (int64_t) sequence->length;
}

static inline
int bt_field_common_sequence_set_length(struct bt_field_common *field,
		uint64_t length, bt_field_common_create_func field_create_func)
{
	int ret = 0;
	struct bt_field_common_sequence *sequence = BT_FROM_COMMON(field);

	BT_ASSERT_PRE_NON_NULL(field, "Sequence field");
	BT_ASSERT_PRE_FIELD_COMMON_HOT(field, "Sequence field");

	if (length > sequence->elements->len) {
		/* Make more room */
		struct bt_field_type_common_sequence *sequence_ft;
		uint64_t cur_len = sequence->elements->len;
		uint64_t i;

		g_ptr_array_set_size(sequence->elements, length);
		sequence_ft = BT_FROM_COMMON(sequence->common.type);

		for (i = cur_len; i < sequence->elements->len; i++) {
			struct bt_field_common *elem_field =
				field_create_func(sequence_ft->element_ft);

			if (!elem_field) {
				ret = -1;
				goto end;
			}

			BT_ASSERT(!sequence->elements->pdata[i]);
			sequence->elements->pdata[i] = elem_field;
		}
	}

	sequence->length = length;

end:
	return ret;
}

static inline
struct bt_field_common *bt_field_common_structure_borrow_field_by_name(
		struct bt_field_common *field, const char *name)
{
	struct bt_field_common *ret = NULL;
	GQuark field_quark;
	struct bt_field_type_common_structure *structure_ft;
	struct bt_field_common_structure *structure = BT_FROM_COMMON(field);
	size_t index;
	GHashTable *field_name_to_index;

	BT_ASSERT_PRE_NON_NULL(field, "Structure field");
	BT_ASSERT_PRE_NON_NULL(name, "Field name");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_STRUCT, "Field");
	structure_ft = BT_FROM_COMMON(field->type);
	field_name_to_index = structure_ft->field_name_to_index;
	field_quark = g_quark_from_string(name);
	if (!g_hash_table_lookup_extended(field_name_to_index,
			GUINT_TO_POINTER(field_quark),
			NULL, (gpointer *) &index)) {
		BT_LOGV("Invalid parameter: no such field in structure field's type: "
			"struct-field-addr=%p, struct-ft-addr=%p, name=\"%s\"",
			field, field->type, name);
		goto error;
	}

	ret = structure->fields->pdata[index];
	BT_ASSERT(ret);

error:
	return ret;
}

static inline
struct bt_field_common *bt_field_common_structure_borrow_field_by_index(
		struct bt_field_common *field, uint64_t index)
{
	struct bt_field_common_structure *structure = BT_FROM_COMMON(field);

	BT_ASSERT_PRE_NON_NULL(field, "Structure field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_STRUCT, "Field");
	BT_ASSERT_PRE(index < structure->fields->len,
		"Index is out of bound: %![struct-field-]+_f, "
		"index=%" PRIu64 ", count=%u", field, index,
		structure->fields->len);
	return structure->fields->pdata[index];
}

static inline
struct bt_field_common *bt_field_common_array_borrow_field(
		struct bt_field_common *field, uint64_t index)
{
	struct bt_field_common_array *array = BT_FROM_COMMON(field);

	BT_ASSERT_PRE_NON_NULL(field, "Array field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(field, BT_FIELD_TYPE_ID_ARRAY,
		"Field");
	BT_ASSERT_PRE(index < array->elements->len,
		"Index is out of bound: %![array-field-]+_f, "
		"index=%" PRIu64 ", count=%u", field,
		index, array->elements->len);
	return array->elements->pdata[(size_t) index];
}

static inline
struct bt_field_common *bt_field_common_sequence_borrow_field(
		struct bt_field_common *field, uint64_t index)
{
	struct bt_field_common_sequence *sequence = BT_FROM_COMMON(field);

	BT_ASSERT_PRE_NON_NULL(field, "Sequence field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(field, BT_FIELD_TYPE_ID_SEQUENCE,
		"Field");
	BT_ASSERT_PRE(index < sequence->length,
		"Index is out of bound: %![seq-field-]+_f, "
		"index=%" PRIu64 ", count=%u", field, index,
		sequence->elements->len);
	return sequence->elements->pdata[(size_t) index];
}

static inline
int bt_field_variant_common_set_tag(struct bt_field_common *variant_field,
		uint64_t tag_uval, bool is_signed)
{
	int ret = 0;
	int64_t choice_index;
	struct bt_field_common_variant *variant = BT_FROM_COMMON(variant_field);

	BT_ASSERT_PRE_NON_NULL(variant_field, "Variant field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(variant_field,
		BT_FIELD_TYPE_ID_VARIANT, "Field");

	/* Find matching index in variant field's type */
	choice_index = bt_field_type_common_variant_find_choice_index(
		variant_field->type, tag_uval, is_signed);
	if (choice_index < 0) {
		ret = -1;
		goto end;
	}

	/* Select corresponding field */
	BT_ASSERT(choice_index < variant->fields->len);
	variant->current_field = variant->fields->pdata[choice_index];
	variant->tag_value.u = tag_uval;

end:
	return ret;
}

static inline
struct bt_field_common *bt_field_common_variant_borrow_current_field(
		struct bt_field_common *variant_field)
{
	struct bt_field_common_variant *variant = BT_FROM_COMMON(variant_field);

	BT_ASSERT_PRE_NON_NULL(variant_field, "Variant field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(variant_field,
		BT_FIELD_TYPE_ID_VARIANT, "Field");
	BT_ASSERT_PRE(variant->current_field,
		"Variant field has no current field: %!+_f", variant_field);
	return variant->current_field;
}

static inline
int bt_field_common_variant_get_tag_signed(struct bt_field_common *variant_field,
	int64_t *tag)
{
	struct bt_field_common_variant *variant = BT_FROM_COMMON(variant_field);

	BT_ASSERT_PRE_NON_NULL(variant_field, "Variant field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(variant_field,
		BT_FIELD_TYPE_ID_VARIANT, "Field");
	BT_ASSERT_PRE(variant->current_field,
		"Variant field has no current field: %!+_f", variant_field);
	*tag = variant->tag_value.i;
	return 0;
}

static inline
int bt_field_common_variant_get_tag_unsigned(struct bt_field_common *variant_field,
	uint64_t *tag)
{
	struct bt_field_common_variant *variant = BT_FROM_COMMON(variant_field);

	BT_ASSERT_PRE_NON_NULL(variant_field, "Variant field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(variant_field,
		BT_FIELD_TYPE_ID_VARIANT, "Field");
	BT_ASSERT_PRE(variant->current_field,
		"Variant field has no current field: %!+_f", variant_field);
	*tag = variant->tag_value.u;
	return 0;
}

static inline
struct bt_field_type_enumeration_mapping_iterator *
bt_field_common_enumeration_get_mappings(struct bt_field_common *field,
		bt_field_common_create_func field_create_func,
		uint64_t uval)
{
	struct bt_field_type_common_enumeration *enum_type = NULL;
	struct bt_field_type_common_integer *integer_type = NULL;
	struct bt_field_type_enumeration_mapping_iterator *iter = NULL;

	BT_ASSERT(field);
	BT_ASSERT(field->type->id == BT_FIELD_TYPE_ID_ENUM);
	BT_ASSERT(field->payload_set);
	enum_type = BT_FROM_COMMON(field->type);
	integer_type = enum_type->container_ft;

	if (!integer_type->is_signed) {
		iter = bt_field_type_common_enumeration_unsigned_find_mappings_by_value(
				field->type, uval);
	} else {
		iter = bt_field_type_common_enumeration_signed_find_mappings_by_value(
				field->type, (int64_t) uval);
	}

	return iter;
}

static inline
int bt_field_common_floating_point_get_value(struct bt_field_common *field,
		double *value)
{
	struct bt_field_common_floating_point *floating_point =
		BT_FROM_COMMON(field);

	BT_ASSERT_PRE_NON_NULL(field, "Floating point number field");
	BT_ASSERT_PRE_NON_NULL(value, "Value");
	BT_ASSERT_PRE_FIELD_COMMON_IS_SET(field, "Floating point number field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_FLOAT, "Field");
	*value = floating_point->payload;
	return 0;
}

static inline
int bt_field_common_floating_point_set_value(struct bt_field_common *field,
		double value)
{
	struct bt_field_common_floating_point *floating_point =
		BT_FROM_COMMON(field);

	BT_ASSERT_PRE_NON_NULL(field, "Floating point number field");
	BT_ASSERT_PRE_FIELD_COMMON_HOT(field, "Floating point number field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_FLOAT, "Field");
	floating_point->payload = value;
	bt_field_common_set(field, true);
	return 0;
}

static inline
const char *bt_field_common_string_get_value(struct bt_field_common *field)
{
	struct bt_field_common_string *string = BT_FROM_COMMON(field);

	BT_ASSERT_PRE_NON_NULL(field, "String field");
	BT_ASSERT_PRE_FIELD_COMMON_IS_SET(field, "String field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_STRING, "Field");
	return (const char *) string->buf->data;
}

static inline
int bt_field_common_string_set_value(struct bt_field_common *field,
		const char *value)
{
	struct bt_field_common_string *string = BT_FROM_COMMON(field);
	size_t str_len;

	BT_ASSERT_PRE_NON_NULL(field, "String field");
	BT_ASSERT_PRE_NON_NULL(value, "Value");
	BT_ASSERT_PRE_FIELD_COMMON_HOT(field, "String field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_STRING, "Field");

	str_len = strlen(value);

	if (str_len + 1 > string->buf->len) {
		g_array_set_size(string->buf, str_len + 1);
	}

	memcpy(string->buf->data, value, str_len);
	((char *) string->buf->data)[str_len] = '\0';
	string->size = str_len;
	bt_field_common_set(field, true);
	return 0;
}

static inline
int bt_field_common_string_append_len(struct bt_field_common *field,
		const char *value, unsigned int length)
{
	struct bt_field_common_string *string_field = BT_FROM_COMMON(field);
	char *data;
	size_t new_size;

	BT_ASSERT_PRE_NON_NULL(field, "String field");
	BT_ASSERT_PRE_NON_NULL(value, "Value");
	BT_ASSERT_PRE_FIELD_COMMON_HOT(field, "String field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_STRING, "Field");

	/* Make sure no null bytes are appended */
	BT_ASSERT_PRE(memchr(value, '\0', length) == NULL,
		"String value to append contains a null character: "
		"partial-value=\"%.32s\", length=%u", value, length);

	new_size = string_field->size + length;

	if (new_size + 1 > string_field->buf->len) {
		g_array_set_size(string_field->buf, new_size + 1);
	}

	data = string_field->buf->data;
	memcpy(data + string_field->size, value, length);
	((char *) string_field->buf->data)[new_size] = '\0';
	string_field->size = new_size;
	bt_field_common_set(field, true);
	return 0;
}

static inline
int bt_field_common_string_append(struct bt_field_common *field,
		const char *value)
{
	BT_ASSERT_PRE_NON_NULL(value, "Value");

	return bt_field_common_string_append_len(field, value,
		strlen(value));
}

static inline
int bt_field_common_string_clear(struct bt_field_common *field)
{
	struct bt_field_common_string *string_field = BT_FROM_COMMON(field);

	BT_ASSERT_PRE_NON_NULL(field, "String field");
	BT_ASSERT_PRE_FIELD_COMMON_HOT(field, "String field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_STRING, "Field");
	string_field->size = 0;
	bt_field_common_set(field, true);
	return 0;
}

static inline
int _bt_field_validate_recursive(struct bt_field *field)
{
	return _bt_field_common_validate_recursive((void *) field);
}

static inline
void _bt_field_set_is_frozen_recursive(struct bt_field *field, bool is_frozen)
{
	return _bt_field_common_set_is_frozen_recursive((void *) field,
			is_frozen);
}

static inline
bt_bool _bt_field_is_set_recursive(struct bt_field *field)
{
	return _bt_field_common_is_set_recursive((void *) field);
}

static inline
void _bt_field_reset_recursive(struct bt_field *field)
{
	_bt_field_common_reset_recursive((void *) field);
}

static inline
void _bt_field_set(struct bt_field *field, bool value)
{
	_bt_field_common_set((void *) field, value);
}

static inline
void bt_field_common_finalize(struct bt_field_common *field)
{
	BT_ASSERT(field);
	BT_LOGD_STR("Putting field's type.");
	bt_put(field->type);
}

static inline
void bt_field_common_integer_finalize(struct bt_field_common *field)
{
	BT_ASSERT(field);
	BT_LOGD("Finalizing common integer field object: addr=%p", field);
	bt_field_common_finalize(field);
}

static inline
void bt_field_common_floating_point_finalize(struct bt_field_common *field)
{
	BT_ASSERT(field);
	BT_LOGD("Finalizing common floating point number field object: addr=%p", field);
	bt_field_common_finalize(field);
}

static inline
void bt_field_common_structure_finalize_recursive(struct bt_field_common *field)
{
	struct bt_field_common_structure *structure = BT_FROM_COMMON(field);

	BT_ASSERT(field);
	BT_LOGD("Finalizing common structure field object: addr=%p", field);
	bt_field_common_finalize(field);

	if (structure->fields) {
		g_ptr_array_free(structure->fields, TRUE);
	}
}

static inline
void bt_field_common_variant_finalize_recursive(struct bt_field_common *field)
{
	struct bt_field_common_variant *variant = BT_FROM_COMMON(field);

	BT_ASSERT(field);
	BT_LOGD("Finalizing common variant field object: addr=%p", field);
	bt_field_common_finalize(field);

	if (variant->fields) {
		g_ptr_array_free(variant->fields, TRUE);
	}
}

static inline
void bt_field_common_array_finalize_recursive(struct bt_field_common *field)
{
	struct bt_field_common_array *array = BT_FROM_COMMON(field);

	BT_ASSERT(field);
	BT_LOGD("Finalizing common array field object: addr=%p", field);
	bt_field_common_finalize(field);

	if (array->elements) {
		g_ptr_array_free(array->elements, TRUE);
	}
}

static inline
void bt_field_common_sequence_finalize_recursive(struct bt_field_common *field)
{
	struct bt_field_common_sequence *sequence = BT_FROM_COMMON(field);

	BT_ASSERT(field);
	BT_LOGD("Finalizing common sequence field object: addr=%p", field);
	bt_field_common_finalize(field);

	if (sequence->elements) {
		g_ptr_array_free(sequence->elements, TRUE);
	}
}

static inline
void bt_field_common_string_finalize(struct bt_field_common *field)
{
	struct bt_field_common_string *string = BT_FROM_COMMON(field);

	BT_ASSERT(field);
	BT_LOGD("Finalizing common string field object: addr=%p", field);
	bt_field_common_finalize(field);

	if (string->buf) {
		g_array_free(string->buf, TRUE);
	}
}

BT_ASSERT_PRE_FUNC
static inline bool value_is_in_range_signed(unsigned int size, int64_t value)
{
	bool ret = true;
	int64_t min_value, max_value;

	min_value = -(1ULL << (size - 1));
	max_value = (1ULL << (size - 1)) - 1;
	if (value < min_value || value > max_value) {
		BT_LOGF("Value is out of bounds: value=%" PRId64 ", "
			"min-value=%" PRId64 ", max-value=%" PRId64,
			value, min_value, max_value);
		ret = false;
	}

	return ret;
}

BT_ASSERT_PRE_FUNC
static inline bool value_is_in_range_unsigned(unsigned int size, uint64_t value)
{
	bool ret = true;
	int64_t max_value;

	max_value = (size == 64) ? UINT64_MAX : ((uint64_t) 1 << size) - 1;
	if (value > max_value) {
		BT_LOGF("Value is out of bounds: value=%" PRIu64 ", "
			"max-value=%" PRIu64,
			value, max_value);
		ret = false;
	}

	return ret;
}

BT_HIDDEN
struct bt_field *bt_field_create_recursive(struct bt_field_type *type);

BT_HIDDEN
void bt_field_destroy_recursive(struct bt_field *field);

#endif /* BABELTRACE_CTF_IR_FIELDS_INTERNAL_H */
