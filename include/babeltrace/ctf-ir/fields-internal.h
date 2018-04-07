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
#include <inttypes.h>
#include <stdbool.h>
#include <glib.h>

#define BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(_field, _type_id, _name)	\
	BT_ASSERT_PRE((_field)->type->id == (_type_id),			\
		_name " has the wrong type ID: expected-type-id=%s, "	\
		"%![field-]+_f", bt_common_field_type_id_string(_type_id),	\
		(_field))

#define BT_ASSERT_PRE_FIELD_COMMON_IS_SET(_field, _name)		\
	BT_ASSERT_PRE(bt_field_common_is_set_recursive(_field),		\
		_name " is not set: %!+_f", (_field))

#define BT_ASSERT_PRE_FIELD_COMMON_HOT(_field, _name)			\
	BT_ASSERT_PRE_HOT((_field), (_name), ": +%!+_f", (_field))

struct bt_field;
struct bt_field_common;

typedef void (*bt_field_common_method_freeze)(struct bt_field_common *);
typedef int (*bt_field_common_method_validate)(struct bt_field_common *);
typedef struct bt_field_common *(*bt_field_common_method_copy)(
		struct bt_field_common *);
typedef bt_bool (*bt_field_common_method_is_set)(struct bt_field_common *);
typedef void (*bt_field_common_method_reset)(struct bt_field_common *);

struct bt_field_common_methods {
	bt_field_common_method_freeze freeze;
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

struct bt_field_common_enumeration {
	struct bt_field_common common;
	struct bt_field_common *payload;
};

struct bt_field_common_floating_point {
	struct bt_field_common common;
	double payload;
};

struct bt_field_common_structure {
	struct bt_field_common common;
	GPtrArray *fields; /* Array of pointers to struct bt_field_common */
};

struct bt_field_common_variant {
	struct bt_field_common common;
	struct bt_field_common *tag;
	struct bt_field_common *payload;
};

struct bt_field_common_array {
	struct bt_field_common common;
	GPtrArray *elements; /* Array of pointers to struct bt_field_common */
};

struct bt_field_common_sequence {
	struct bt_field_common common;
	struct bt_field_common *length;
	GPtrArray *elements; /* Array of pointers to struct bt_field_common */
};

struct bt_field_common_string {
	struct bt_field_common common;
	GString *payload;
};

static inline
struct bt_field_type *bt_field_borrow_type(struct bt_field *field)
{
	struct bt_field_common *field_common = (void *) field;

	BT_ASSERT(field);
	return (void *) field_common->type;
}

BT_HIDDEN
int64_t bt_field_sequence_get_int_length(struct bt_field *field);

BT_HIDDEN
struct bt_field_common *bt_field_common_copy(struct bt_field_common *field);

BT_HIDDEN
int bt_field_common_structure_initialize(struct bt_field_common *field,
		struct bt_field_type_common *type,
		bt_object_release_func release_func,
		struct bt_field_common_methods *methods,
		bt_field_common_create_func field_create_func);

BT_HIDDEN
int bt_field_common_array_initialize(struct bt_field_common *field,
		struct bt_field_type_common *type,
		bt_object_release_func release_func,
		struct bt_field_common_methods *methods);

BT_HIDDEN
int bt_field_common_generic_validate(struct bt_field_common *field);

BT_HIDDEN
int bt_field_common_enumeration_validate_recursive(
		struct bt_field_common *field);

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
void bt_field_common_enumeration_reset_recursive(struct bt_field_common *field);

BT_HIDDEN
void bt_field_common_structure_reset_recursive(struct bt_field_common *field);

BT_HIDDEN
void bt_field_common_variant_reset_recursive(struct bt_field_common *field);

BT_HIDDEN
void bt_field_common_array_reset_recursive(struct bt_field_common *field);

BT_HIDDEN
void bt_field_common_sequence_reset_recursive(struct bt_field_common *field);

BT_HIDDEN
void bt_field_common_string_reset(struct bt_field_common *field);

BT_HIDDEN
void bt_field_common_generic_freeze(struct bt_field_common *field);

BT_HIDDEN
void bt_field_common_enumeration_freeze_recursive(struct bt_field_common *field);

BT_HIDDEN
void bt_field_common_structure_freeze_recursive(struct bt_field_common *field);

BT_HIDDEN
void bt_field_common_variant_freeze_recursive(struct bt_field_common *field);

BT_HIDDEN
void bt_field_common_array_freeze_recursive(struct bt_field_common *field);

BT_HIDDEN
void bt_field_common_sequence_freeze_recursive(struct bt_field_common *field);

BT_HIDDEN
void _bt_field_common_freeze_recursive(struct bt_field_common *field);

BT_HIDDEN
bt_bool bt_field_common_generic_is_set(struct bt_field_common *field);

BT_HIDDEN
bt_bool bt_field_common_enumeration_is_set_recursive(
		struct bt_field_common *field);

BT_HIDDEN
bt_bool bt_field_common_structure_is_set_recursive(
		struct bt_field_common *field);

BT_HIDDEN
bt_bool bt_field_common_variant_is_set_recursive(struct bt_field_common *field);

BT_HIDDEN
bt_bool bt_field_common_array_is_set_recursive(struct bt_field_common *field);

BT_HIDDEN
bt_bool bt_field_common_sequence_is_set_recursive(struct bt_field_common *field);

BT_HIDDEN
void bt_field_common_integer_destroy(struct bt_object *obj);

BT_HIDDEN
void bt_field_common_enumeration_destroy_recursive(struct bt_object *obj);

BT_HIDDEN
void bt_field_common_floating_point_destroy(struct bt_object *obj);

BT_HIDDEN
void bt_field_common_structure_destroy_recursive(struct bt_object *obj);

BT_HIDDEN
void bt_field_common_variant_destroy_recursive(struct bt_object *obj);

BT_HIDDEN
void bt_field_common_array_destroy_recursive(struct bt_object *obj);

BT_HIDDEN
void bt_field_common_sequence_destroy_recursive(struct bt_object *obj);

BT_HIDDEN
void bt_field_common_string_destroy(struct bt_object *obj);

#ifdef BT_DEV_MODE
# define bt_field_common_validate_recursive	_bt_field_common_validate_recursive
# define bt_field_common_freeze_recursive	_bt_field_common_freeze_recursive
# define bt_field_common_is_set_recursive	_bt_field_common_is_set_recursive
# define bt_field_common_reset_recursive	_bt_field_common_reset_recursive
# define bt_field_common_set			_bt_field_common_set
# define bt_field_validate_recursive		_bt_field_validate_recursive
# define bt_field_freeze_recursive		_bt_field_freeze_recursive
# define bt_field_is_set_recursive		_bt_field_is_set_recursive
# define bt_field_reset_recursive		_bt_field_reset_recursive
# define bt_field_set				_bt_field_set
#else
# define bt_field_common_validate_recursive(_field)	(-1)
# define bt_field_common_freeze_recursive(_field)
# define bt_field_common_is_set_recursive(_field)	(BT_FALSE)
# define bt_field_common_reset_recursive(_field)	(BT_TRUE)
# define bt_field_common_set(_field, _val)
# define bt_field_validate_recursive(_field)		(-1)
# define bt_field_freeze_recursive(_field)
# define bt_field_is_set_recursive(_field)		(BT_FALSE)
# define bt_field_reset_recursive(_field)		(BT_TRUE)
# define bt_field_set(_field, _val)
#endif

BT_ASSERT_FUNC
static inline bool field_type_common_has_known_id(
		struct bt_field_type_common *ft)
{
	return ft->id > BT_FIELD_TYPE_ID_UNKNOWN ||
		ft->id < BT_FIELD_TYPE_ID_NR;
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
		struct bt_field_type_common *ft,
		bt_object_release_func release_func,
		struct bt_field_common_methods *methods)
{
	BT_ASSERT(field);
	BT_ASSERT(ft);
	bt_object_init(field, release_func);
	field->methods = methods;
	field->type = bt_get(ft);
}

static inline
struct bt_field_type_common *bt_field_common_get_type(
		struct bt_field_common *field)
{
	struct bt_field_type_common *ret = NULL;

	BT_ASSERT_PRE_NON_NULL(field, "Field");
	ret = bt_get(field->type);
	return ret;
}

static inline
int64_t bt_field_common_sequence_get_int_length(struct bt_field_common *field)
{
	struct bt_field_common_sequence *sequence = BT_FROM_COMMON(field);
	int64_t ret;

	BT_ASSERT(field);
	BT_ASSERT(field->type->id == BT_FIELD_TYPE_ID_SEQUENCE);
	if (!sequence->length) {
		ret = -1;
		goto end;
	}

	ret = (int64_t) sequence->elements->len;

end:
	return ret;
}

static inline
struct bt_field_common *bt_field_common_sequence_get_length(
		struct bt_field_common *field)
{
	struct bt_field_common_sequence *sequence = BT_FROM_COMMON(field);

	BT_ASSERT_PRE_NON_NULL(field, "Sequence field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(field, BT_FIELD_TYPE_ID_SEQUENCE,
		"Field");
	return bt_get(sequence->length);
}

static inline
int bt_field_common_sequence_set_length(struct bt_field_common *field,
		struct bt_field_common *length_field)
{
	int ret = 0;
	struct bt_field_common_integer *length = BT_FROM_COMMON(length_field);
	struct bt_field_common_sequence *sequence = BT_FROM_COMMON(field);
	uint64_t sequence_length;

	BT_ASSERT_PRE_NON_NULL(field, "Sequence field");
	BT_ASSERT_PRE_NON_NULL(length_field, "Length field");
	BT_ASSERT_PRE_FIELD_COMMON_HOT(field, "Sequence field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(length_field,
		BT_FIELD_TYPE_ID_INTEGER, "Length field");
	BT_ASSERT_PRE(
		!bt_field_type_common_integer_is_signed(length->common.type),
		"Length field's type is signed: %!+_f", length_field);
	BT_ASSERT_PRE_FIELD_COMMON_IS_SET(length_field, "Length field");
	sequence_length = length->payload.unsignd;
	if (sequence->elements) {
		g_ptr_array_free(sequence->elements, TRUE);
		bt_put(sequence->length);
	}

	sequence->elements = g_ptr_array_sized_new((size_t) sequence_length);
	if (!sequence->elements) {
		BT_LOGE_STR("Failed to allocate a GPtrArray.");
		ret = -1;
		goto end;
	}

	g_ptr_array_set_free_func(sequence->elements,
		(GDestroyNotify) bt_put);
	g_ptr_array_set_size(sequence->elements, (size_t) sequence_length);
	bt_get(length_field);
	sequence->length = length_field;
	bt_field_common_freeze_recursive(length_field);

end:
	return ret;
}

static inline
struct bt_field_common *bt_field_common_structure_get_field_by_name(
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

	ret = bt_get(structure->fields->pdata[index]);
	BT_ASSERT(ret);

error:
	return ret;
}

static inline
struct bt_field_common *bt_field_common_structure_get_field_by_index(
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
	return bt_get(structure->fields->pdata[index]);
}

BT_ASSERT_PRE_FUNC
static inline bool field_to_set_has_expected_type(
		struct bt_field_common *struct_field,
		const char *name, struct bt_field_common *value)
{
	bool ret = true;
	struct bt_field_type_common *expected_field_type = NULL;

	expected_field_type =
		bt_field_type_common_structure_get_field_type_by_name(
			struct_field->type, name);

	if (bt_field_type_common_compare(expected_field_type, value->type)) {
		BT_ASSERT_PRE_MSG("Value field's type is different from the expected field type: "
			"%![value-ft-]+_F, %![expected-ft-]+_F", value->type,
			expected_field_type);
		ret = false;
		goto end;
	}

end:
	bt_put(expected_field_type);
	return ret;
}

static inline
int bt_field_common_structure_set_field_by_name(struct bt_field_common *field,
		const char *name, struct bt_field_common *value)
{
	int ret = 0;
	GQuark field_quark;
	struct bt_field_common_structure *structure = BT_FROM_COMMON(field);
	size_t index;
	GHashTable *field_name_to_index;
	struct bt_field_type_common_structure *structure_ft;

	BT_ASSERT_PRE_NON_NULL(field, "Structure field");
	BT_ASSERT_PRE_NON_NULL(name, "Field name");
	BT_ASSERT_PRE_NON_NULL(value, "Value field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(field, BT_FIELD_TYPE_ID_STRUCT,
		"Parent field");
	BT_ASSERT_PRE(field_to_set_has_expected_type(field, name, value),
		"Value field's type is different from the expected field type.");
	field_quark = g_quark_from_string(name);
	structure_ft = BT_FROM_COMMON(field->type);
	field_name_to_index = structure_ft->field_name_to_index;
	if (!g_hash_table_lookup_extended(field_name_to_index,
			GUINT_TO_POINTER(field_quark), NULL,
			(gpointer *) &index)) {
		BT_LOGV("Invalid parameter: no such field in structure field's type: "
			"struct-field-addr=%p, struct-ft-addr=%p, "
			"field-ft-addr=%p, name=\"%s\"",
			field, field->type, value->type, name);
		ret = -1;
		goto end;
	}
	bt_get(value);
	BT_MOVE(structure->fields->pdata[index], value);

end:
	return ret;
}

static inline
struct bt_field_common *bt_field_common_array_get_field(
		struct bt_field_common *field, uint64_t index,
		bt_field_common_create_func field_create_func)
{
	struct bt_field_common *new_field = NULL;
	struct bt_field_type_common *field_type = NULL;
	struct bt_field_common_array *array = BT_FROM_COMMON(field);

	BT_ASSERT_PRE_NON_NULL(field, "Array field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(field, BT_FIELD_TYPE_ID_ARRAY, "Field");
	BT_ASSERT_PRE(index < array->elements->len,
		"Index is out of bound: %![array-field-]+_f, "
		"index=%" PRIu64 ", count=%u", field,
		index, array->elements->len);

	field_type = bt_field_type_common_array_get_element_field_type(
		field->type);
	if (array->elements->pdata[(size_t) index]) {
		new_field = array->elements->pdata[(size_t) index];
		goto end;
	}

	/* We don't want to modify this field if it's frozen */
	BT_ASSERT_PRE_FIELD_COMMON_HOT(field, "Array field");
	new_field = field_create_func(field_type);
	array->elements->pdata[(size_t) index] = new_field;

end:
	bt_put(field_type);
	bt_get(new_field);
	return new_field;
}

static inline
struct bt_field_common *bt_field_common_sequence_get_field(
		struct bt_field_common *field, uint64_t index,
		bt_field_common_create_func field_create_func)
{
	struct bt_field_common *new_field = NULL;
	struct bt_field_type_common *field_type = NULL;
	struct bt_field_common_sequence *sequence = BT_FROM_COMMON(field);

	BT_ASSERT_PRE_NON_NULL(field, "Sequence field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(field, BT_FIELD_TYPE_ID_SEQUENCE,
		"Field");
	BT_ASSERT_PRE_NON_NULL(sequence->elements, "Sequence field's element array");
	BT_ASSERT_PRE(index < sequence->elements->len,
		"Index is out of bound: %![seq-field-]+_f, "
		"index=%" PRIu64 ", count=%u", field, index,
		sequence->elements->len);
	field_type = bt_field_type_common_sequence_get_element_field_type(
		field->type);
	if (sequence->elements->pdata[(size_t) index]) {
		new_field = sequence->elements->pdata[(size_t) index];
		goto end;
	}

	/* We don't want to modify this field if it's frozen */
	BT_ASSERT_PRE_FIELD_COMMON_HOT(field, "Sequence field");
	new_field = field_create_func(field_type);
	sequence->elements->pdata[(size_t) index] = new_field;

end:
	bt_put(field_type);
	bt_get(new_field);
	return new_field;
}

static inline
struct bt_field_common *bt_field_common_enumeration_get_container(
		struct bt_field_common *field,
		bt_field_common_create_func field_create_func)
{
	struct bt_field_common_enumeration *enumeration = BT_FROM_COMMON(field);

	BT_ASSERT_PRE_NON_NULL(field, "Enumeration field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_ENUM, "Field");

	if (!enumeration->payload) {
		struct bt_field_type_common_enumeration *enumeration_type;

		/* We don't want to modify this field if it's frozen */
		BT_ASSERT_PRE_FIELD_COMMON_HOT(field, "Enumeration field");

		enumeration_type = BT_FROM_COMMON(field->type);
		enumeration->payload =
			field_create_func(
				BT_TO_COMMON(enumeration_type->container_ft));
	}

	return bt_get(enumeration->payload);
}

static inline
struct bt_field_common *bt_field_common_variant_get_field(
		struct bt_field_common *field,
		struct bt_field_common *tag_field,
		bt_field_common_create_func field_create_func)
{
	struct bt_field_common *new_field = NULL;
	struct bt_field_common_variant *variant = BT_FROM_COMMON(field);
	struct bt_field_type_common_variant *variant_type;
	struct bt_field_type_common *field_type;
	struct bt_field_common *tag_enum = NULL;
	struct bt_field_common_integer *tag_enum_integer =
		BT_FROM_COMMON(field);
	int64_t tag_enum_value;

	BT_ASSERT_PRE_NON_NULL(field, "Variant field");
	BT_ASSERT_PRE_NON_NULL(tag_field, "Tag field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(field, BT_FIELD_TYPE_ID_VARIANT,
		"Variant field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(tag_field, BT_FIELD_TYPE_ID_ENUM,
		"Tag field");
	variant_type = BT_FROM_COMMON(field->type);
	tag_enum = bt_field_common_enumeration_get_container(tag_field,
		field_create_func);
	BT_ASSERT_PRE_NON_NULL(tag_enum, "Tag field's container");
	tag_enum_integer = BT_FROM_COMMON(tag_enum);
	BT_ASSERT_PRE(bt_field_common_validate_recursive(tag_field) == 0,
		"Tag field is invalid: %!+_f", tag_field);
	tag_enum_value = tag_enum_integer->payload.signd;

	/*
	 * If the variant currently has a tag and a payload, and if the
	 * requested tag value is the same as the current one, return
	 * the current payload instead of creating a fresh one.
	 */
	if (variant->tag && variant->payload) {
		struct bt_field_common *cur_tag_container = NULL;
		struct bt_field_common_integer *cur_tag_enum_integer;
		int64_t cur_tag_value;

		cur_tag_container =
			bt_field_common_enumeration_get_container(variant->tag,
				field_create_func);
		BT_ASSERT(cur_tag_container);
		cur_tag_enum_integer = BT_FROM_COMMON(cur_tag_container);
		bt_put(cur_tag_container);
		cur_tag_value = cur_tag_enum_integer->payload.signd;

		if (cur_tag_value == tag_enum_value) {
			new_field = variant->payload;
			bt_get(new_field);
			goto end;
		}
	}

	/* We don't want to modify this field if it's frozen */
	BT_ASSERT_PRE_FIELD_COMMON_HOT(field, "Variant field");
	field_type = bt_field_type_common_variant_get_field_type_signed(
		variant_type, tag_enum_value);

	/* It's the caller's job to make sure the tag's value is valid */
	BT_ASSERT_PRE(field_type,
		"Variant field's type does not contain a field type for "
		"this tag value: tag-value-signed=%" PRId64 ", "
		"%![var-ft-]+_F, %![tag-field-]+_f", tag_enum_value,
		variant_type, tag_field);

	new_field = field_create_func(field_type);
	if (!new_field) {
		BT_LOGW("Cannot create field: "
			"variant-field-addr=%p, variant-ft-addr=%p, "
			"field-ft-addr=%p", field, field->type, field_type);
		goto end;
	}

	bt_put(variant->tag);
	bt_put(variant->payload);
	bt_get(new_field);
	bt_get(tag_field);
	variant->tag = tag_field;
	variant->payload = new_field;

end:
	bt_put(tag_enum);
	return new_field;
}

static inline
struct bt_field_common *bt_field_common_variant_get_current_field(
		struct bt_field_common *variant_field)
{
	struct bt_field_common_variant *variant = BT_FROM_COMMON(variant_field);

	BT_ASSERT_PRE_NON_NULL(variant_field, "Variant field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(variant_field,
		BT_FIELD_TYPE_ID_VARIANT, "Field");
	return bt_get(variant->payload);
}

static inline
struct bt_field_common *bt_field_common_variant_get_tag(
		struct bt_field_common *variant_field)
{
	struct bt_field_common_variant *variant = BT_FROM_COMMON(variant_field);

	BT_ASSERT_PRE_NON_NULL(variant_field, "Variant field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(variant_field,
		BT_FIELD_TYPE_ID_VARIANT, "Field");
	return bt_get(variant->tag);
}

static inline
int bt_field_common_integer_signed_get_value(struct bt_field_common *field,
		int64_t *value)
{
	struct bt_field_common_integer *integer = BT_FROM_COMMON(field);

	BT_ASSERT_PRE_NON_NULL(field, "Integer field");
	BT_ASSERT_PRE_NON_NULL(value, "Value");
	BT_ASSERT_PRE_FIELD_COMMON_IS_SET(field, "Integer field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_INTEGER, "Field");
	BT_ASSERT_PRE(bt_field_type_common_integer_is_signed(field->type),
		"Field's type is unsigned: %!+_f", field);
	*value = integer->payload.signd;
	return 0;
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

static inline
int bt_field_common_integer_signed_set_value(struct bt_field_common *field,
		int64_t value)
{
	int ret = 0;
	struct bt_field_common_integer *integer = BT_FROM_COMMON(field);
	struct bt_field_type_common_integer *integer_type;

	BT_ASSERT_PRE_NON_NULL(field, "Integer field");
	BT_ASSERT_PRE_FIELD_COMMON_HOT(field, "Integer field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_INTEGER, "Field");
	integer_type = BT_FROM_COMMON(field->type);
	BT_ASSERT_PRE(bt_field_type_common_integer_is_signed(field->type),
		"Field's type is unsigned: %!+_f", field);
	BT_ASSERT_PRE(value_is_in_range_signed(integer_type->size, value),
		"Value is out of bounds: value=%" PRId64 ", %![field-]+_f",
		value, field);
	integer->payload.signd = value;
	bt_field_common_set(field, true);
	return ret;
}

static inline
int bt_field_common_integer_unsigned_get_value(struct bt_field_common *field,
		uint64_t *value)
{
	struct bt_field_common_integer *integer = BT_FROM_COMMON(field);

	BT_ASSERT_PRE_NON_NULL(field, "Integer field");
	BT_ASSERT_PRE_NON_NULL(value, "Value");
	BT_ASSERT_PRE_FIELD_COMMON_IS_SET(field, "Integer field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_INTEGER, "Field");
	BT_ASSERT_PRE(!bt_field_type_common_integer_is_signed(field->type),
		"Field's type is signed: %!+_f", field);
	*value = integer->payload.unsignd;
	return 0;
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

static inline
int bt_field_common_integer_unsigned_set_value(struct bt_field_common *field,
		uint64_t value)
{
	struct bt_field_common_integer *integer = BT_FROM_COMMON(field);
	struct bt_field_type_common_integer *integer_type;

	BT_ASSERT_PRE_NON_NULL(field, "Integer field");
	BT_ASSERT_PRE_FIELD_COMMON_HOT(field, "Integer field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_INTEGER, "Field");
	integer_type = BT_FROM_COMMON(field->type);
	BT_ASSERT_PRE(!bt_field_type_common_integer_is_signed(field->type),
		"Field's type is signed: %!+_f", field);
	BT_ASSERT_PRE(value_is_in_range_unsigned(integer_type->size, value),
		"Value is out of bounds: value=%" PRIu64 ", %![field-]+_f",
		value, field);
	integer->payload.unsignd = value;
	bt_field_common_set(field, true);
	return 0;
}

static inline
struct bt_field_type_enumeration_mapping_iterator *
bt_field_common_enumeration_get_mappings(struct bt_field_common *field,
		bt_field_common_create_func field_create_func)
{
	int ret;
	struct bt_field_common *container = NULL;
	struct bt_field_type_common_integer *integer_type = NULL;
	struct bt_field_type_enumeration_mapping_iterator *iter = NULL;

	BT_ASSERT_PRE_NON_NULL(field, "Enumeration field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_ENUM, "Field");
	container = bt_field_common_enumeration_get_container(field,
		field_create_func);
	BT_ASSERT_PRE(container,
		"Enumeration field has no container field: %!+_f", field);
	BT_ASSERT(container->type);
	integer_type = BT_FROM_COMMON(container->type);
	BT_ASSERT_PRE_FIELD_COMMON_IS_SET(container,
		"Enumeration field's payload field");

	if (!integer_type->is_signed) {
		uint64_t value;

		ret = bt_field_common_integer_unsigned_get_value(container,
			&value);
		BT_ASSERT(ret == 0);
		iter = bt_field_type_common_enumeration_unsigned_find_mappings_by_value(
				field->type, value);
	} else {
		int64_t value;

		ret = bt_field_common_integer_signed_get_value(container, &value);
		BT_ASSERT(ret == 0);
		iter = bt_field_type_common_enumeration_signed_find_mappings_by_value(
				field->type, value);
	}

	bt_put(container);
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
	return string->payload->str;
}

static inline
int bt_field_common_string_set_value(struct bt_field_common *field,
		const char *value)
{
	struct bt_field_common_string *string = BT_FROM_COMMON(field);

	BT_ASSERT_PRE_NON_NULL(field, "String field");
	BT_ASSERT_PRE_NON_NULL(value, "Value");
	BT_ASSERT_PRE_FIELD_COMMON_HOT(field, "String field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_STRING, "Field");

	if (string->payload) {
		g_string_assign(string->payload, value);
	} else {
		string->payload = g_string_new(value);
	}

	bt_field_common_set(field, true);
	return 0;
}

static inline
int bt_field_common_string_append(struct bt_field_common *field,
		const char *value)
{
	struct bt_field_common_string *string_field = BT_FROM_COMMON(field);

	BT_ASSERT_PRE_NON_NULL(field, "String field");
	BT_ASSERT_PRE_NON_NULL(value, "Value");
	BT_ASSERT_PRE_FIELD_COMMON_HOT(field, "String field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_STRING, "Field");

	if (string_field->payload) {
		g_string_append(string_field->payload, value);
	} else {
		string_field->payload = g_string_new(value);
	}

	bt_field_common_set(field, true);
	return 0;
}

static inline
int bt_field_common_string_append_len(struct bt_field_common *field,
		const char *value, unsigned int length)
{
	struct bt_field_common_string *string_field = BT_FROM_COMMON(field);

	BT_ASSERT_PRE_NON_NULL(field, "String field");
	BT_ASSERT_PRE_NON_NULL(value, "Value");
	BT_ASSERT_PRE_FIELD_COMMON_HOT(field, "String field");
	BT_ASSERT_PRE_FIELD_COMMON_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_STRING, "Field");

	/* make sure no null bytes are appended */
	BT_ASSERT_PRE(memchr(value, '\0', length) == NULL,
		"String value to append contains a null character: "
		"partial-value=\"%.32s\", length=%u", value, length);

	if (string_field->payload) {
		g_string_append_len(string_field->payload, value, length);
	} else {
		string_field->payload = g_string_new_len(value, length);
	}

	bt_field_common_set(field, true);
	return 0;
}

static inline
int _bt_field_validate_recursive(struct bt_field *field)
{
	return _bt_field_common_validate_recursive((void *) field);
}

static inline
void _bt_field_freeze_recursive(struct bt_field *field)
{
	return _bt_field_common_freeze_recursive((void *) field);
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

#endif /* BABELTRACE_CTF_IR_FIELDS_INTERNAL_H */
