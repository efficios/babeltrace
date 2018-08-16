/*
 * fields.c
 *
 * Babeltrace CTF IR - Event Fields
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

#define BT_LOG_TAG "FIELDS"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/ctf-ir/fields.h>
#include <babeltrace/ctf-ir/fields-internal.h>
#include <babeltrace/ctf-ir/field-types-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/compat/fcntl-internal.h>
#include <babeltrace/align-internal.h>
#include <babeltrace/assert-internal.h>
#include <inttypes.h>

#define BT_ASSERT_PRE_FIELD_IS_INT_OR_ENUM(_field, _name)		\
	BT_ASSERT_PRE((_field)->type->id == BT_FIELD_TYPE_ID_INTEGER ||	\
		(_field)->type->id == BT_FIELD_TYPE_ID_ENUM,		\
		_name " is not an integer or an enumeration field: "	\
		"%!+f",	(_field))

static
int bt_field_generic_validate(struct bt_field *field);

static
int bt_field_structure_validate_recursive(struct bt_field *field);

static
int bt_field_variant_validate_recursive(struct bt_field *field);

static
int bt_field_array_validate_recursive(struct bt_field *field);

static
int bt_field_sequence_validate_recursive(struct bt_field *field);

static
void bt_field_generic_reset(struct bt_field *field);

static
void bt_field_structure_reset_recursive(struct bt_field *field);

static
void bt_field_variant_reset_recursive(struct bt_field *field);

static
void bt_field_array_reset_recursive(struct bt_field *field);

static
void bt_field_sequence_reset_recursive(struct bt_field *field);

static
void bt_field_generic_set_is_frozen(struct bt_field *field,
		bool is_frozen);

static
void bt_field_structure_set_is_frozen_recursive(
		struct bt_field *field, bool is_frozen);

static
void bt_field_variant_set_is_frozen_recursive(
		struct bt_field *field, bool is_frozen);

static
void bt_field_array_set_is_frozen_recursive(
		struct bt_field *field, bool is_frozen);

static
void bt_field_sequence_set_is_frozen_recursive(
		struct bt_field *field, bool is_frozen);

static
bt_bool bt_field_generic_is_set(struct bt_field *field);

static
bt_bool bt_field_structure_is_set_recursive(
		struct bt_field *field);

static
bt_bool bt_field_variant_is_set_recursive(struct bt_field *field);

static
bt_bool bt_field_array_is_set_recursive(struct bt_field *field);

static
bt_bool bt_field_sequence_is_set_recursive(struct bt_field *field);

static struct bt_field_methods bt_field_integer_methods = {
	.set_is_frozen = bt_field_generic_set_is_frozen,
	.validate = bt_field_generic_validate,
	.is_set = bt_field_generic_is_set,
	.reset = bt_field_generic_reset,
};

static struct bt_field_methods bt_field_floating_point_methods = {
	.set_is_frozen = bt_field_generic_set_is_frozen,
	.validate = bt_field_generic_validate,
	.is_set = bt_field_generic_is_set,
	.reset = bt_field_generic_reset,
};

static struct bt_field_methods bt_field_enumeration_methods = {
	.set_is_frozen = bt_field_generic_set_is_frozen,
	.validate = bt_field_generic_validate,
	.is_set = bt_field_generic_is_set,
	.reset = bt_field_generic_reset,
};

static struct bt_field_methods bt_field_string_methods = {
	.set_is_frozen = bt_field_generic_set_is_frozen,
	.validate = bt_field_generic_validate,
	.is_set = bt_field_generic_is_set,
	.reset = bt_field_generic_reset,
};

static struct bt_field_methods bt_field_structure_methods = {
	.set_is_frozen = bt_field_structure_set_is_frozen_recursive,
	.validate = bt_field_structure_validate_recursive,
	.is_set = bt_field_structure_is_set_recursive,
	.reset = bt_field_structure_reset_recursive,
};

static struct bt_field_methods bt_field_sequence_methods = {
	.set_is_frozen = bt_field_sequence_set_is_frozen_recursive,
	.validate = bt_field_sequence_validate_recursive,
	.is_set = bt_field_sequence_is_set_recursive,
	.reset = bt_field_sequence_reset_recursive,
};

static struct bt_field_methods bt_field_array_methods = {
	.set_is_frozen = bt_field_array_set_is_frozen_recursive,
	.validate = bt_field_array_validate_recursive,
	.is_set = bt_field_array_is_set_recursive,
	.reset = bt_field_array_reset_recursive,
};

static struct bt_field_methods bt_field_variant_methods = {
	.set_is_frozen = bt_field_variant_set_is_frozen_recursive,
	.validate = bt_field_variant_validate_recursive,
	.is_set = bt_field_variant_is_set_recursive,
	.reset = bt_field_variant_reset_recursive,
};

static
struct bt_field *bt_field_integer_create(struct bt_field_type *);

static
struct bt_field *bt_field_enumeration_create(struct bt_field_type *);

static
struct bt_field *bt_field_floating_point_create(struct bt_field_type *);

static
struct bt_field *bt_field_structure_create(struct bt_field_type *);

static
struct bt_field *bt_field_variant_create(struct bt_field_type *);

static
struct bt_field *bt_field_array_create(struct bt_field_type *);

static
struct bt_field *bt_field_sequence_create(struct bt_field_type *);

static
struct bt_field *bt_field_string_create(struct bt_field_type *);

static
struct bt_field *(* const field_create_funcs[])(struct bt_field_type *) = {
	[BT_FIELD_TYPE_ID_INTEGER] =	bt_field_integer_create,
	[BT_FIELD_TYPE_ID_ENUM] =	bt_field_enumeration_create,
	[BT_FIELD_TYPE_ID_FLOAT] =	bt_field_floating_point_create,
	[BT_FIELD_TYPE_ID_STRUCT] =	bt_field_structure_create,
	[BT_FIELD_TYPE_ID_VARIANT] =	bt_field_variant_create,
	[BT_FIELD_TYPE_ID_ARRAY] =	bt_field_array_create,
	[BT_FIELD_TYPE_ID_SEQUENCE] =	bt_field_sequence_create,
	[BT_FIELD_TYPE_ID_STRING] =	bt_field_string_create,
};

static
void bt_field_integer_destroy(struct bt_field *field);

static
void bt_field_enumeration_destroy(struct bt_field *field);

static
void bt_field_floating_point_destroy(struct bt_field *field);

static
void bt_field_structure_destroy_recursive(struct bt_field *field);

static
void bt_field_variant_destroy_recursive(struct bt_field *field);

static
void bt_field_array_destroy_recursive(struct bt_field *field);

static
void bt_field_sequence_destroy_recursive(struct bt_field *field);

static
void bt_field_string_destroy(struct bt_field *field);

static
void (* const field_destroy_funcs[])(struct bt_field *) = {
	[BT_FIELD_TYPE_ID_INTEGER] =	bt_field_integer_destroy,
	[BT_FIELD_TYPE_ID_ENUM] =	bt_field_enumeration_destroy,
	[BT_FIELD_TYPE_ID_FLOAT] =	bt_field_floating_point_destroy,
	[BT_FIELD_TYPE_ID_STRUCT] =	bt_field_structure_destroy_recursive,
	[BT_FIELD_TYPE_ID_VARIANT] =	bt_field_variant_destroy_recursive,
	[BT_FIELD_TYPE_ID_ARRAY] =	bt_field_array_destroy_recursive,
	[BT_FIELD_TYPE_ID_SEQUENCE] =	bt_field_sequence_destroy_recursive,
	[BT_FIELD_TYPE_ID_STRING] =	bt_field_string_destroy,
};

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
struct bt_field *bt_field_create_recursive(struct bt_field_type *type)
{
	struct bt_field *field = NULL;
	enum bt_field_type_id type_id;

	BT_ASSERT_PRE_NON_NULL(type, "Field type");
	BT_ASSERT(bt_field_type_has_known_id((void *) type));
	BT_ASSERT_PRE(bt_field_type_validate((void *) type) == 0,
		"Field type is invalid: %!+F", type);
	type_id = bt_field_type_get_type_id(type);
	field = field_create_funcs[type_id](type);
	if (!field) {
		goto end;
	}

	bt_field_type_freeze(type);

end:
	return field;
}

struct bt_field_type *bt_field_borrow_type(struct bt_field *field)
{
	struct bt_field_type *ret = NULL;

	BT_ASSERT_PRE_NON_NULL(field, "Field");
	ret = field->type;
	return ret;
}

enum bt_field_type_id bt_field_get_type_id(struct bt_field *field)
{
	BT_ASSERT_PRE_NON_NULL(field, "Field");
	return field->type->id;
}

int64_t bt_field_sequence_get_length(struct bt_field *field)
{
	struct bt_field_sequence *sequence = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "Sequence field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field, BT_FIELD_TYPE_ID_SEQUENCE,
		"Field");
	return (int64_t) sequence->length;
}

int bt_field_sequence_set_length(struct bt_field *field, uint64_t length)
{
	int ret = 0;
	struct bt_field_sequence *sequence = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "Sequence field");
	BT_ASSERT_PRE(((int64_t) length) >= 0,
		"Invalid sequence length (too large): length=%" PRId64,
		length);
	BT_ASSERT_PRE_FIELD_HOT(field, "Sequence field");

	if (unlikely(length > sequence->elements->len)) {
		/* Make more room */
		struct bt_field_type_sequence *sequence_ft;
		uint64_t cur_len = sequence->elements->len;
		uint64_t i;

		g_ptr_array_set_size(sequence->elements, length);
		sequence_ft = (void *) sequence->common.type;

		for (i = cur_len; i < sequence->elements->len; i++) {
			struct bt_field *elem_field =
				bt_field_create_recursive(
					sequence_ft->element_ft);

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

struct bt_field *bt_field_structure_borrow_field_by_index(
		struct bt_field *field, uint64_t index)
{
	struct bt_field_structure *structure = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "Structure field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_STRUCT, "Field");
	BT_ASSERT_PRE(index < structure->fields->len,
		"Index is out of bound: %![struct-field-]+f, "
		"index=%" PRIu64 ", count=%u", field, index,
		structure->fields->len);
	return structure->fields->pdata[index];
}

struct bt_field *bt_field_structure_borrow_field_by_name(
		struct bt_field *field, const char *name)
{
	struct bt_field *ret = NULL;
	GQuark field_quark;
	struct bt_field_type_structure *structure_ft;
	struct bt_field_structure *structure = (void *) field;
	size_t index;
	GHashTable *field_name_to_index;

	BT_ASSERT_PRE_NON_NULL(field, "Structure field");
	BT_ASSERT_PRE_NON_NULL(name, "Field name");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_STRUCT, "Field");
	structure_ft = (void *) field->type;
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

struct bt_field *bt_field_array_borrow_field(
		struct bt_field *field, uint64_t index)
{
	struct bt_field_array *array = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "Array field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field, BT_FIELD_TYPE_ID_ARRAY,
		"Field");
	BT_ASSERT_PRE(index < array->elements->len,
		"Index is out of bound: %![array-field-]+f, "
		"index=%" PRIu64 ", count=%u", field,
		index, array->elements->len);
	return array->elements->pdata[(size_t) index];
}

struct bt_field *bt_field_sequence_borrow_field(
		struct bt_field *field, uint64_t index)
{
	struct bt_field_sequence *sequence = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "Sequence field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field, BT_FIELD_TYPE_ID_SEQUENCE,
		"Field");
	BT_ASSERT_PRE(index < sequence->length,
		"Index is out of bound: %![seq-field-]+f, "
		"index=%" PRIu64 ", count=%u", field, index,
		sequence->elements->len);
	return sequence->elements->pdata[(size_t) index];
}

struct bt_field *bt_field_variant_borrow_current_field(
		struct bt_field *field)
{
	struct bt_field_variant *variant = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "Variant field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_VARIANT, "Field");
	BT_ASSERT_PRE(variant->current_field,
		"Variant field has no current field: %!+f", field);
	return variant->current_field;
}

static inline
int bt_field_variant_set_tag(struct bt_field *field,
		uint64_t tag_uval, bool is_signed)
{
	int ret = 0;
	int64_t choice_index;
	struct bt_field_variant *variant = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "Variant field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_VARIANT, "Field");

	/* Find matching index in variant field's type */
	choice_index = bt_field_type_variant_find_choice_index(
		field->type, tag_uval, is_signed);
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

int bt_field_variant_set_tag_signed(struct bt_field *variant_field,
		int64_t tag)
{
	return bt_field_variant_set_tag((void *) variant_field,
		(uint64_t) tag, true);
}

int bt_field_variant_set_tag_unsigned(struct bt_field *variant_field,
		uint64_t tag)
{
	return bt_field_variant_set_tag((void *) variant_field,
		(uint64_t) tag, false);
}

int bt_field_variant_get_tag_signed(struct bt_field *field,
		int64_t *tag)
{
	struct bt_field_variant *variant = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "Variant field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_VARIANT, "Field");
	BT_ASSERT_PRE(variant->current_field,
		"Variant field has no current field: %!+f", field);
	*tag = variant->tag_value.i;
	return 0;
}

int bt_field_variant_get_tag_unsigned(struct bt_field *field,
		uint64_t *tag)
{
	struct bt_field_variant *variant = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "Variant field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_VARIANT, "Field");
	BT_ASSERT_PRE(variant->current_field,
		"Variant field has no current field: %!+f", field);
	*tag = variant->tag_value.u;
	return 0;
}

struct bt_field_type_enumeration_mapping_iterator *
bt_field_enumeration_get_mappings(struct bt_field *field)
{
	struct bt_field_enumeration *enum_field = (void *) field;
	struct bt_field_type_enumeration *enum_type = NULL;
	struct bt_field_type_integer *integer_type = NULL;
	struct bt_field_type_enumeration_mapping_iterator *iter = NULL;

	BT_ASSERT(field);
	BT_ASSERT(field->type->id == BT_FIELD_TYPE_ID_ENUM);
	BT_ASSERT(field->payload_set);
	BT_ASSERT_PRE_NON_NULL(field, "Enumeration field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID((struct bt_field *) field,
		BT_FIELD_TYPE_ID_ENUM, "Field");
	BT_ASSERT_PRE_FIELD_IS_SET((struct bt_field *) field,
		"Enumeration field");
	enum_type = (void *) field->type;
	integer_type = enum_type->container_ft;

	if (!integer_type->is_signed) {
		iter = bt_field_type_enumeration_unsigned_find_mappings_by_value(
				field->type,
				enum_field->common.payload.unsignd);
	} else {
		iter = bt_field_type_enumeration_signed_find_mappings_by_value(
				field->type,
				enum_field->common.payload.signd);
	}

	return iter;
}

BT_ASSERT_PRE_FUNC
static inline
struct bt_field_type_integer *get_int_enum_int_ft(
		struct bt_field *field)
{
	struct bt_field_integer *int_field = (void *) field;
	struct bt_field_type_integer *int_ft = NULL;

	if (int_field->common.type->id == BT_FIELD_TYPE_ID_INTEGER) {
		int_ft = (void *) int_field->common.type;
	} else if (int_field->common.type->id == BT_FIELD_TYPE_ID_ENUM) {
		struct bt_field_type_enumeration *enum_ft =
			(void *) int_field->common.type;
		int_ft = enum_ft->container_ft;
	} else {
		abort();
	}

	BT_ASSERT(int_ft);
	return int_ft;
}

int bt_field_integer_signed_get_value(struct bt_field *field, int64_t *value)
{
	struct bt_field_integer *integer = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "Integer/enumeration field");
	BT_ASSERT_PRE_NON_NULL(value, "Value");
	BT_ASSERT_PRE_FIELD_IS_SET(field,
		"Integer/enumeration field");
	BT_ASSERT_PRE_FIELD_IS_INT_OR_ENUM(field, "Field");
	BT_ASSERT_PRE(bt_field_type_integer_is_signed(
		(void *) get_int_enum_int_ft(field)),
		"Field's type is unsigned: %!+f", field);
	*value = integer->payload.signd;
	return 0;
}

int bt_field_integer_signed_set_value(struct bt_field *field, int64_t value)
{
	int ret = 0;
	struct bt_field_integer *integer = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "Integer field");
	BT_ASSERT_PRE_FIELD_HOT(field, "Integer field");
	BT_ASSERT_PRE_FIELD_IS_INT_OR_ENUM(field, "Field");
	BT_ASSERT_PRE(bt_field_type_integer_is_signed(
		(void *) get_int_enum_int_ft(field)),
		"Field's type is unsigned: %!+f", field);
	BT_ASSERT_PRE(value_is_in_range_signed(
		get_int_enum_int_ft(field)->size, value),
		"Value is out of bounds: value=%" PRId64 ", %![field-]+f",
		value, field);
	integer->payload.signd = value;
	bt_field_set(field, true);
	return ret;
}

int bt_field_integer_unsigned_get_value(struct bt_field *field, uint64_t *value)
{
	struct bt_field_integer *integer = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "Integer field");
	BT_ASSERT_PRE_NON_NULL(value, "Value");
	BT_ASSERT_PRE_FIELD_IS_SET(field, "Integer field");
	BT_ASSERT_PRE_FIELD_IS_INT_OR_ENUM(field, "Field");
	BT_ASSERT_PRE(!bt_field_type_integer_is_signed(
		(void *) get_int_enum_int_ft(field)),
		"Field's type is signed: %!+f", field);
	*value = integer->payload.unsignd;
	return 0;
}

int bt_field_integer_unsigned_set_value(struct bt_field *field,
		uint64_t value)
{
	struct bt_field_integer *integer = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "Integer field");
	BT_ASSERT_PRE_FIELD_HOT(field, "Integer field");
	BT_ASSERT_PRE_FIELD_IS_INT_OR_ENUM(field, "Field");
	BT_ASSERT_PRE(!bt_field_type_integer_is_signed(
		(void *) get_int_enum_int_ft(field)),
		"Field's type is signed: %!+f", field);
	BT_ASSERT_PRE(value_is_in_range_unsigned(
		get_int_enum_int_ft(field)->size, value),
		"Value is out of bounds: value=%" PRIu64 ", %![field-]+f",
		value, field);
	integer->payload.unsignd = value;
	bt_field_set(field, true);
	return 0;
}

int bt_field_floating_point_get_value(struct bt_field *field,
		double *value)
{
	struct bt_field_floating_point *floating_point = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "Floating point number field");
	BT_ASSERT_PRE_NON_NULL(value, "Value");
	BT_ASSERT_PRE_FIELD_IS_SET(field, "Floating point number field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_FLOAT, "Field");
	*value = floating_point->payload;
	return 0;
}

int bt_field_floating_point_set_value(struct bt_field *field,
		double value)
{
	struct bt_field_floating_point *floating_point = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "Floating point number field");
	BT_ASSERT_PRE_FIELD_HOT(field, "Floating point number field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_FLOAT, "Field");
	floating_point->payload = value;
	bt_field_set(field, true);
	return 0;
}

const char *bt_field_string_get_value(struct bt_field *field)
{
	struct bt_field_string *string = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "String field");
	BT_ASSERT_PRE_FIELD_IS_SET(field, "String field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_STRING, "Field");
	return (const char *) string->buf->data;
}

int bt_field_string_set_value(struct bt_field *field, const char *value)
{
	BT_ASSERT_PRE_NON_NULL(field, "String field");
	BT_ASSERT_PRE_NON_NULL(value, "Value");
	BT_ASSERT_PRE_FIELD_HOT(field, "String field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_STRING, "Field");
	bt_field_string_clear(field);
	return bt_field_string_append_len(field,
		value, strlen(value));
}

int bt_field_string_append(struct bt_field *field, const char *value)
{
	BT_ASSERT_PRE_NON_NULL(value, "Value");
	return bt_field_string_append_len(field, value,
		strlen(value));
}

int bt_field_string_append_len(struct bt_field *field,
		const char *value, unsigned int length)
{
	struct bt_field_string *string_field = (void *) field;
	char *data;
	size_t new_size;

	BT_ASSERT_PRE_NON_NULL(field, "String field");
	BT_ASSERT_PRE_NON_NULL(value, "Value");
	BT_ASSERT_PRE_FIELD_HOT(field, "String field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_STRING, "Field");

	/* Make sure no null bytes are appended */
	BT_ASSERT_PRE(memchr(value, '\0', length) == NULL,
		"String value to append contains a null character: "
		"partial-value=\"%.32s\", length=%u", value, length);

	new_size = string_field->size + length;

	if (unlikely(new_size + 1 > string_field->buf->len)) {
		g_array_set_size(string_field->buf, new_size + 1);
	}

	data = string_field->buf->data;
	memcpy(data + string_field->size, value, length);
	((char *) string_field->buf->data)[new_size] = '\0';
	string_field->size = new_size;
	bt_field_set(field, true);
	return 0;
}

int bt_field_string_clear(struct bt_field *field)
{
	struct bt_field_string *string_field = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "String field");
	BT_ASSERT_PRE_FIELD_HOT(field, "String field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_STRING, "Field");
	string_field->size = 0;
	bt_field_set(field, true);
	return 0;
}

static inline
void bt_field_finalize(struct bt_field *field)
{
	BT_ASSERT(field);
	BT_LOGD_STR("Putting field's type.");
	bt_put(field->type);
}

static
void bt_field_integer_destroy(struct bt_field *field)
{
	BT_ASSERT(field);
	BT_LOGD("Destroying integer field object: addr=%p", field);
	bt_field_finalize(field);
	g_free(field);
}

static
void bt_field_floating_point_destroy(struct bt_field *field)
{
	BT_ASSERT(field);
	BT_LOGD("Destroying floating point field object: addr=%p", field);
	bt_field_finalize(field);
	g_free(field);
}

static
void bt_field_enumeration_destroy(struct bt_field *field)
{
	BT_LOGD("Destroying enumeration field object: addr=%p", field);
	bt_field_finalize((void *) field);
	g_free(field);
}

static
void bt_field_structure_destroy_recursive(struct bt_field *field)
{
	struct bt_field_structure *structure = (void *) field;

	BT_ASSERT(field);
	BT_LOGD("Destroying structure field object: addr=%p", field);
	bt_field_finalize(field);

	if (structure->fields) {
		g_ptr_array_free(structure->fields, TRUE);
	}

	g_free(field);
}

static
void bt_field_variant_destroy_recursive(struct bt_field *field)
{
	struct bt_field_variant *variant = (void *) field;

	BT_ASSERT(field);
	BT_LOGD("Destroying variant field object: addr=%p", field);
	bt_field_finalize(field);

	if (variant->fields) {
		g_ptr_array_free(variant->fields, TRUE);
	}

	g_free(field);
}

static
void bt_field_array_destroy_recursive(struct bt_field *field)
{
	struct bt_field_array *array = (void *) field;

	BT_ASSERT(field);
	BT_LOGD("Destroying array field object: addr=%p", field);
	bt_field_finalize(field);

	if (array->elements) {
		g_ptr_array_free(array->elements, TRUE);
	}

	g_free(field);
}

static
void bt_field_sequence_destroy_recursive(struct bt_field *field)
{
	struct bt_field_sequence *sequence = (void *) field;

	BT_ASSERT(field);
	BT_LOGD("Destroying sequence field object: addr=%p", field);
	bt_field_finalize(field);

	if (sequence->elements) {
		g_ptr_array_free(sequence->elements, TRUE);
	}
	g_free(field);
}

static
void bt_field_string_destroy(struct bt_field *field)
{
	struct bt_field_string *string = (void *) field;

	BT_LOGD("Destroying string field object: addr=%p", field);
	BT_ASSERT(field);
	bt_field_finalize(field);

	if (string->buf) {
		g_array_free(string->buf, TRUE);
	}

	g_free(field);
}

BT_HIDDEN
void bt_field_destroy_recursive(struct bt_field *field)
{
	if (!field) {
		return;
	}

	BT_ASSERT(bt_field_type_has_known_id((void *) field->type));
	field_destroy_funcs[field->type->id](field);
}

static inline
void bt_field_initialize(struct bt_field *field,
		struct bt_field_type *ft,
		struct bt_field_methods *methods)
{
	BT_ASSERT(field);
	BT_ASSERT(ft);
	bt_object_init_unique(&field->base);
	field->methods = methods;
	field->type = bt_get(ft);
}

static
struct bt_field *bt_field_integer_create(struct bt_field_type *type)
{
	struct bt_field_integer *integer =
		g_new0(struct bt_field_integer, 1);

	BT_LOGD("Creating integer field object: ft-addr=%p", type);

	if (integer) {
		bt_field_initialize((void *) integer, (void *) type,
			&bt_field_integer_methods);
		BT_LOGD("Created integer field object: addr=%p, ft-addr=%p",
			integer, type);
	} else {
		BT_LOGE_STR("Failed to allocate one integer field.");
	}

	return (void *) integer;
}

static
struct bt_field *bt_field_enumeration_create(struct bt_field_type *type)
{
	struct bt_field_enumeration *enumeration = g_new0(
		struct bt_field_enumeration, 1);

	BT_LOGD("Creating enumeration field object: ft-addr=%p", type);

	if (enumeration) {
		bt_field_initialize((void *) enumeration,
			(void *) type, &bt_field_enumeration_methods);
		BT_LOGD("Created enumeration field object: addr=%p, ft-addr=%p",
			enumeration, type);
	} else {
		BT_LOGE_STR("Failed to allocate one enumeration field.");
	}

	return (void *) enumeration;
}

static
struct bt_field *bt_field_floating_point_create(struct bt_field_type *type)
{
	struct bt_field_floating_point *floating_point;

	BT_LOGD("Creating floating point number field object: ft-addr=%p", type);
	floating_point = g_new0(struct bt_field_floating_point, 1);

	if (floating_point) {
		bt_field_initialize((void *) floating_point,
			(void *) type, &bt_field_floating_point_methods);
		BT_LOGD("Created floating point number field object: addr=%p, ft-addr=%p",
			floating_point, type);
	} else {
		BT_LOGE_STR("Failed to allocate one floating point number field.");
	}

	return (void *) floating_point;
}

static inline
int bt_field_structure_initialize(struct bt_field *field,
		struct bt_field_type *type,
		struct bt_field_methods *methods,
		bt_field_create_func field_create_func,
		GDestroyNotify field_release_func)
{
	int ret = 0;
	struct bt_field_type_structure *structure_type = (void *) type;
	struct bt_field_structure *structure = (void *) field;
	size_t i;

	BT_LOGD("Initializing structure field object: ft-addr=%p", type);
	bt_field_initialize(field, type, methods);
	structure->fields = g_ptr_array_new_with_free_func(field_release_func);
	g_ptr_array_set_size(structure->fields, structure_type->fields->len);

	/* Create all fields contained in the structure field. */
	for (i = 0; i < structure_type->fields->len; i++) {
		struct bt_field *field;
		struct bt_field_type_structure_field *struct_field =
			BT_FIELD_TYPE_STRUCTURE_FIELD_AT_INDEX(
				structure_type, i);
		field = field_create_func(struct_field->type);
		if (!field) {
			BT_LOGE("Failed to create structure field's member: name=\"%s\", index=%zu",
				g_quark_to_string(struct_field->name), i);
			ret = -1;
			goto end;
		}

		g_ptr_array_index(structure->fields, i) = field;
	}

	BT_LOGD("Initialized structure field object: addr=%p, ft-addr=%p",
		field, type);

end:
	return ret;
}

static
struct bt_field *bt_field_structure_create(struct bt_field_type *type)
{
	struct bt_field_structure *structure = g_new0(
		struct bt_field_structure, 1);
	int iret;

	BT_LOGD("Creating structure field object: ft-addr=%p", type);

	if (!structure) {
		BT_LOGE_STR("Failed to allocate one structure field.");
		goto end;
	}

	iret = bt_field_structure_initialize((void *) structure,
		(void *) type, &bt_field_structure_methods,
		(bt_field_create_func) bt_field_create_recursive,
		(GDestroyNotify) bt_field_destroy_recursive);
	if (iret) {
		BT_PUT(structure);
		goto end;
	}

	BT_LOGD("Created structure field object: addr=%p, ft-addr=%p",
		structure, type);

end:
	return (void *) structure;
}

static inline
int bt_field_variant_initialize(struct bt_field *field,
		struct bt_field_type *type,
		struct bt_field_methods *methods,
		bt_field_create_func field_create_func,
		GDestroyNotify field_release_func)
{
	int ret = 0;
	struct bt_field_type_variant *variant_type = (void *) type;
	struct bt_field_variant *variant = (void *) field;
	size_t i;

	BT_LOGD("Initializing variant field object: ft-addr=%p", type);
	bt_field_initialize(field, type, methods);
	ret = bt_field_type_variant_update_choices(type);
	if (ret) {
		BT_LOGE("Cannot update variant field type choices: "
			"ret=%d", ret);
		goto end;
	}

	variant->fields = g_ptr_array_new_with_free_func(field_release_func);
	g_ptr_array_set_size(variant->fields, variant_type->choices->len);

	/* Create all fields contained in the variant field. */
	for (i = 0; i < variant_type->choices->len; i++) {
		struct bt_field *field;
		struct bt_field_type_variant_choice *var_choice =
			BT_FIELD_TYPE_VARIANT_CHOICE_AT_INDEX(
				variant_type, i);

		field = field_create_func(var_choice->type);
		if (!field) {
			BT_LOGE("Failed to create variant field's member: name=\"%s\", index=%zu",
				g_quark_to_string(var_choice->name), i);
			ret = -1;
			goto end;
		}

		g_ptr_array_index(variant->fields, i) = field;
	}

	BT_LOGD("Initialized variant field object: addr=%p, ft-addr=%p",
		field, type);

end:
	return ret;
}

static inline
int bt_field_string_initialize(struct bt_field *field,
		struct bt_field_type *type,
		struct bt_field_methods *methods)
{
	int ret = 0;
	struct bt_field_string *string = (void *) field;

	BT_LOGD("Initializing string field object: ft-addr=%p", type);
	bt_field_initialize(field, type, methods);
	string->buf = g_array_sized_new(FALSE, FALSE, sizeof(char), 1);
	if (!string->buf) {
		ret = -1;
		goto end;
	}

	g_array_index(string->buf, char, 0) = '\0';
	BT_LOGD("Initialized string field object: addr=%p, ft-addr=%p",
		field, type);

end:
	return ret;
}

static
struct bt_field *bt_field_variant_create(struct bt_field_type *type)
{
	struct bt_field_variant *variant = g_new0(
		struct bt_field_variant, 1);
	int iret;

	BT_LOGD("Creating variant field object: ft-addr=%p", type);

	if (!variant) {
		BT_LOGE_STR("Failed to allocate one variant field.");
		goto end;
	}

	iret = bt_field_variant_initialize((void *) variant,
		(void *) type, &bt_field_variant_methods,
		(bt_field_create_func) bt_field_create_recursive,
		(GDestroyNotify) bt_field_destroy_recursive);
	if (iret) {
		BT_PUT(variant);
		goto end;
	}

	BT_LOGD("Created variant field object: addr=%p, ft-addr=%p",
		variant, type);

end:
	return (void *) variant;
}

static inline
int bt_field_array_initialize(struct bt_field *field,
		struct bt_field_type *type,
		struct bt_field_methods *methods,
		bt_field_create_func field_create_func,
		GDestroyNotify field_destroy_func)
{
	struct bt_field_type_array *array_type = (void *) type;
	struct bt_field_array *array = (void *) field;
	unsigned int array_length;
	int ret = 0;
	uint64_t i;

	BT_LOGD("Initializing array field object: ft-addr=%p", type);
	BT_ASSERT(type);
	bt_field_initialize(field, type, methods);
	array_length = array_type->length;
	array->elements = g_ptr_array_sized_new(array_length);
	if (!array->elements) {
		ret = -1;
		goto end;
	}

	g_ptr_array_set_free_func(array->elements, field_destroy_func);
	g_ptr_array_set_size(array->elements, array_length);

	for (i = 0; i < array_length; i++) {
		array->elements->pdata[i] = field_create_func(
			array_type->element_ft);
		if (!array->elements->pdata[i]) {
			ret = -1;
			goto end;
		}
	}

	BT_LOGD("Initialized array field object: addr=%p, ft-addr=%p",
		field, type);

end:
	return ret;
}

static
struct bt_field *bt_field_array_create(struct bt_field_type *type)
{
	struct bt_field_array *array =
		g_new0(struct bt_field_array, 1);
	int ret;

	BT_LOGD("Creating array field object: ft-addr=%p", type);
	BT_ASSERT(type);

	if (!array) {
		BT_LOGE_STR("Failed to allocate one array field.");
		goto end;
	}

	ret = bt_field_array_initialize((void *) array,
		(void *) type, &bt_field_array_methods,
		(bt_field_create_func) bt_field_create_recursive,
		(GDestroyNotify) bt_field_destroy_recursive);
	if (ret) {
		BT_PUT(array);
		goto end;
	}

	BT_LOGD("Created array field object: addr=%p, ft-addr=%p",
		array, type);

end:
	return (void *) array;
}

static inline
int bt_field_sequence_initialize(struct bt_field *field,
		struct bt_field_type *type,
		struct bt_field_methods *methods,
		GDestroyNotify field_destroy_func)
{
	struct bt_field_sequence *sequence = (void *) field;
	int ret = 0;

	BT_LOGD("Initializing sequence field object: ft-addr=%p", type);
	BT_ASSERT(type);
	bt_field_initialize(field, type, methods);
	sequence->elements = g_ptr_array_new();
	if (!sequence->elements) {
		ret = -1;
		goto end;
	}

	g_ptr_array_set_free_func(sequence->elements, field_destroy_func);
	BT_LOGD("Initialized sequence field object: addr=%p, ft-addr=%p",
		field, type);

end:
	return ret;
}

static
struct bt_field *bt_field_sequence_create(struct bt_field_type *type)
{
	struct bt_field_sequence *sequence =
		g_new0(struct bt_field_sequence, 1);
	int ret;

	BT_LOGD("Creating sequence field object: ft-addr=%p", type);
	BT_ASSERT(type);

	if (!sequence) {
		BT_LOGE_STR("Failed to allocate one sequence field.");
		goto end;
	}

	ret = bt_field_sequence_initialize((void *) sequence,
		(void *) type, &bt_field_sequence_methods,
		(GDestroyNotify) bt_field_destroy_recursive);
	if (ret) {
		BT_PUT(sequence);
		goto end;
	}

	BT_LOGD("Created sequence field object: addr=%p, ft-addr=%p",
		sequence, type);

end:
	return (void *) sequence;
}

static
struct bt_field *bt_field_string_create(struct bt_field_type *type)
{
	struct bt_field_string *string = g_new0(
		struct bt_field_string, 1);

	BT_LOGD("Creating string field object: ft-addr=%p", type);

	if (string) {
		bt_field_string_initialize((void *) string,
			(void *) type, &bt_field_string_methods);
		BT_LOGD("Created string field object: addr=%p, ft-addr=%p",
			string, type);
	} else {
		BT_LOGE_STR("Failed to allocate one string field.");
	}

	return (void *) string;
}

static
int bt_field_generic_validate(struct bt_field *field)
{
	return (field && field->payload_set) ? 0 : -1;
}

static
int bt_field_structure_validate_recursive(struct bt_field *field)
{
	int64_t i;
	int ret = 0;
	struct bt_field_structure *structure = (void *) field;

	BT_ASSERT(field);

	for (i = 0; i < structure->fields->len; i++) {
		ret = bt_field_validate_recursive(
			(void *) structure->fields->pdata[i]);

		if (ret) {
			int this_ret;
			const char *name;

			this_ret = bt_field_type_structure_borrow_field_by_index(
				field->type, &name, NULL, i);
			BT_ASSERT(this_ret == 0);
			BT_ASSERT_PRE_MSG("Invalid structure field's field: "
				"%![struct-field-]+f, field-name=\"%s\", "
				"index=%" PRId64 ", %![field-]+f",
				field, name, i, structure->fields->pdata[i]);
			goto end;
		}
	}

end:
	return ret;
}

static
int bt_field_variant_validate_recursive(struct bt_field *field)
{
	int ret = 0;
	struct bt_field_variant *variant = (void *) field;

	BT_ASSERT(field);

	if (!variant->current_field) {
		ret = -1;
		goto end;
	}

	ret = bt_field_validate_recursive(variant->current_field);

end:
	return ret;
}

static
int bt_field_array_validate_recursive(struct bt_field *field)
{
	int64_t i;
	int ret = 0;
	struct bt_field_array *array = (void *) field;

	BT_ASSERT(field);

	for (i = 0; i < array->elements->len; i++) {
		ret = bt_field_validate_recursive((void *) array->elements->pdata[i]);
		if (ret) {
			BT_ASSERT_PRE_MSG("Invalid array field's element field: "
				"%![array-field-]+f, " PRId64 ", "
				"%![elem-field-]+f",
				field, i, array->elements->pdata[i]);
			goto end;
		}
	}

end:
	return ret;
}

static
int bt_field_sequence_validate_recursive(struct bt_field *field)
{
	size_t i;
	int ret = 0;
	struct bt_field_sequence *sequence = (void *) field;

	BT_ASSERT(field);

	for (i = 0; i < sequence->elements->len; i++) {
		ret = bt_field_validate_recursive(
			(void *) sequence->elements->pdata[i]);
		if (ret) {
			BT_ASSERT_PRE_MSG("Invalid sequence field's element field: "
				"%![seq-field-]+f, " PRId64 ", "
				"%![elem-field-]+f",
				field, i, sequence->elements->pdata[i]);
			goto end;
		}
	}
end:
	return ret;
}

static
void bt_field_generic_reset(struct bt_field *field)
{
	BT_ASSERT(field);
	field->payload_set = false;
}

static
void bt_field_structure_reset_recursive(struct bt_field *field)
{
	int64_t i;
	struct bt_field_structure *structure = (void *) field;

	BT_ASSERT(field);

	for (i = 0; i < structure->fields->len; i++) {
		struct bt_field *member = structure->fields->pdata[i];

		if (!member) {
			/*
			 * Structure members are lazily initialized;
			 * skip if this member has not been allocated
			 * yet.
			 */
			continue;
		}

		bt_field_reset_recursive(member);
	}
}

static
void bt_field_variant_reset_recursive(struct bt_field *field)
{
	struct bt_field_variant *variant = (void *) field;

	BT_ASSERT(field);
	variant->current_field = NULL;
}

static
void bt_field_array_reset_recursive(struct bt_field *field)
{
	size_t i;
	struct bt_field_array *array = (void *) field;

	BT_ASSERT(field);

	for (i = 0; i < array->elements->len; i++) {
		struct bt_field *member = array->elements->pdata[i];

		if (!member) {
			/*
			 * Array elements are lazily initialized; skip
			 * if this member has not been allocated yet.
			 */
			continue;
		}

		bt_field_reset_recursive(member);
	}
}

static
void bt_field_sequence_reset_recursive(struct bt_field *field)
{
	struct bt_field_sequence *sequence = (void *) field;
	uint64_t i;

	BT_ASSERT(field);

	for (i = 0; i < sequence->elements->len; i++) {
		if (sequence->elements->pdata[i]) {
			bt_field_reset_recursive(
				sequence->elements->pdata[i]);
		}
	}

	sequence->length = 0;
}

static
void bt_field_generic_set_is_frozen(struct bt_field *field,
		bool is_frozen)
{
	field->frozen = is_frozen;
}

static
void bt_field_structure_set_is_frozen_recursive(
		struct bt_field *field, bool is_frozen)
{
	uint64_t i;
	struct bt_field_structure *structure_field = (void *) field;

	BT_LOGD("Freezing structure field object: addr=%p", field);

	for (i = 0; i < structure_field->fields->len; i++) {
		struct bt_field *struct_field =
			g_ptr_array_index(structure_field->fields, i);

		BT_LOGD("Freezing structure field's field: field-addr=%p, index=%" PRId64,
			struct_field, i);
		bt_field_set_is_frozen_recursive(struct_field,
			is_frozen);
	}

	bt_field_generic_set_is_frozen(field, is_frozen);
}

static
void bt_field_variant_set_is_frozen_recursive(
		struct bt_field *field, bool is_frozen)
{
	uint64_t i;
	struct bt_field_variant *variant_field = (void *) field;

	BT_LOGD("Freezing variant field object: addr=%p", field);

	for (i = 0; i < variant_field->fields->len; i++) {
		struct bt_field *var_field =
			g_ptr_array_index(variant_field->fields, i);

		BT_LOGD("Freezing variant field's field: field-addr=%p, index=%" PRId64,
			var_field, i);
		bt_field_set_is_frozen_recursive(var_field, is_frozen);
	}

	bt_field_generic_set_is_frozen(field, is_frozen);
}

static
void bt_field_array_set_is_frozen_recursive(
		struct bt_field *field, bool is_frozen)
{
	int64_t i;
	struct bt_field_array *array_field = (void *) field;

	BT_LOGD("Freezing array field object: addr=%p", field);

	for (i = 0; i < array_field->elements->len; i++) {
		struct bt_field *elem_field =
			g_ptr_array_index(array_field->elements, i);

		BT_LOGD("Freezing array field object's element field: "
			"element-field-addr=%p, index=%" PRId64,
			elem_field, i);
		bt_field_set_is_frozen_recursive(elem_field, is_frozen);
	}

	bt_field_generic_set_is_frozen(field, is_frozen);
}

static
void bt_field_sequence_set_is_frozen_recursive(
		struct bt_field *field, bool is_frozen)
{
	int64_t i;
	struct bt_field_sequence *sequence_field = (void *) field;

	BT_LOGD("Freezing sequence field object: addr=%p", field);

	for (i = 0; i < sequence_field->length; i++) {
		struct bt_field *elem_field =
			g_ptr_array_index(sequence_field->elements, i);

		BT_LOGD("Freezing sequence field object's element field: "
			"element-field-addr=%p, index=%" PRId64,
			elem_field, i);
		bt_field_set_is_frozen_recursive(elem_field, is_frozen);
	}

	bt_field_generic_set_is_frozen(field, is_frozen);
}

BT_HIDDEN
void _bt_field_set_is_frozen_recursive(struct bt_field *field,
		bool is_frozen)
{
	if (!field) {
		goto end;
	}

	BT_LOGD("Setting field object's frozen state: addr=%p, is-frozen=%d",
		field, is_frozen);
	BT_ASSERT(bt_field_type_has_known_id(field->type));
	BT_ASSERT(field->methods->set_is_frozen);
	field->methods->set_is_frozen(field, is_frozen);

end:
	return;
}

static
bt_bool bt_field_generic_is_set(struct bt_field *field)
{
	return field && field->payload_set;
}

static
bt_bool bt_field_structure_is_set_recursive(
		struct bt_field *field)
{
	bt_bool is_set = BT_FALSE;
	size_t i;
	struct bt_field_structure *structure = (void *) field;

	BT_ASSERT(field);

	for (i = 0; i < structure->fields->len; i++) {
		is_set = bt_field_is_set_recursive(
			structure->fields->pdata[i]);
		if (!is_set) {
			goto end;
		}
	}

end:
	return is_set;
}

static
bt_bool bt_field_variant_is_set_recursive(struct bt_field *field)
{
	struct bt_field_variant *variant = (void *) field;
	bt_bool is_set = BT_FALSE;

	BT_ASSERT(field);

	if (variant->current_field) {
		is_set = bt_field_is_set_recursive(
			variant->current_field);
	}

	return is_set;
}

static
bt_bool bt_field_array_is_set_recursive(struct bt_field *field)
{
	size_t i;
	bt_bool is_set = BT_FALSE;
	struct bt_field_array *array = (void *) field;

	BT_ASSERT(field);

	for (i = 0; i < array->elements->len; i++) {
		is_set = bt_field_is_set_recursive(array->elements->pdata[i]);
		if (!is_set) {
			goto end;
		}
	}

end:
	return is_set;
}

static
bt_bool bt_field_sequence_is_set_recursive(struct bt_field *field)
{
	size_t i;
	bt_bool is_set = BT_FALSE;
	struct bt_field_sequence *sequence = (void *) field;

	BT_ASSERT(field);

	if (!sequence->elements) {
		goto end;
	}

	for (i = 0; i < sequence->elements->len; i++) {
		is_set = bt_field_is_set_recursive(
			sequence->elements->pdata[i]);
		if (!is_set) {
			goto end;
		}
	}

end:
	return is_set;
}
