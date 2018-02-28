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

#include <babeltrace/ctf-ir/fields-internal.h>
#include <babeltrace/ctf-ir/field-types-internal.h>
#include <babeltrace/ctf-writer/serialize-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/compat/fcntl-internal.h>
#include <babeltrace/align-internal.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/assert-pre-internal.h>
#include <inttypes.h>

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
void bt_field_destroy(struct bt_object *);
static
void bt_field_integer_destroy(struct bt_field *);
static
void bt_field_enumeration_destroy_recursive(struct bt_field *);
static
void bt_field_floating_point_destroy(struct bt_field *);
static
void bt_field_structure_destroy_recursive(struct bt_field *);
static
void bt_field_variant_destroy_recursive(struct bt_field *);
static
void bt_field_array_destroy_recursive(struct bt_field *);
static
void bt_field_sequence_destroy_recursive(struct bt_field *);
static
void bt_field_string_destroy(struct bt_field *);

static
int bt_field_generic_validate(struct bt_field *);
static
int bt_field_structure_validate_recursive(struct bt_field *);
static
int bt_field_variant_validate_recursive(struct bt_field *);
static
int bt_field_enumeration_validate_recursive(struct bt_field *);
static
int bt_field_array_validate_recursive(struct bt_field *);
static
int bt_field_sequence_validate_recursive(struct bt_field *);

static
void bt_field_generic_reset(struct bt_field *);
static
void bt_field_structure_reset_recursive(struct bt_field *);
static
void bt_field_variant_reset_recursive(struct bt_field *);
static
void bt_field_enumeration_reset_recursive(struct bt_field *);
static
void bt_field_array_reset_recursive(struct bt_field *);
static
void bt_field_sequence_reset_recursive(struct bt_field *);
static
void bt_field_string_reset_recursive(struct bt_field *);

static
int bt_field_integer_serialize(struct bt_field *,
		struct bt_stream_pos *, enum bt_byte_order);
static
int bt_field_enumeration_serialize_recursive(struct bt_field *,
		struct bt_stream_pos *, enum bt_byte_order);
static
int bt_field_floating_point_serialize(struct bt_field *,
		struct bt_stream_pos *, enum bt_byte_order);
static
int bt_field_structure_serialize_recursive(struct bt_field *,
		struct bt_stream_pos *, enum bt_byte_order);
static
int bt_field_variant_serialize_recursive(struct bt_field *,
		struct bt_stream_pos *, enum bt_byte_order);
static
int bt_field_array_serialize_recursive(struct bt_field *,
		struct bt_stream_pos *, enum bt_byte_order);
static
int bt_field_sequence_serialize_recursive(struct bt_field *,
		struct bt_stream_pos *, enum bt_byte_order);
static
int bt_field_string_serialize(struct bt_field *,
		struct bt_stream_pos *, enum bt_byte_order);

static
int bt_field_integer_copy(struct bt_field *, struct bt_field *);
static
int bt_field_enumeration_copy_recursive(struct bt_field *, struct bt_field *);
static
int bt_field_floating_point_copy(struct bt_field *, struct bt_field *);
static
int bt_field_structure_copy_recursive(struct bt_field *, struct bt_field *);
static
int bt_field_variant_copy_recursive(struct bt_field *, struct bt_field *);
static
int bt_field_array_copy_recursive(struct bt_field *, struct bt_field *);
static
int bt_field_sequence_copy_recursive(struct bt_field *, struct bt_field *);
static
int bt_field_string_copy_recursive(struct bt_field *, struct bt_field *);

static
void generic_field_freeze(struct bt_field *);
static
void bt_field_enumeration_freeze_recursive(struct bt_field *);
static
void bt_field_structure_freeze_recursive(struct bt_field *);
static
void bt_field_variant_freeze_recursive(struct bt_field *);
static
void bt_field_array_freeze_recursive(struct bt_field *);
static
void bt_field_sequence_freeze_recursive(struct bt_field *);

static
bt_bool bt_field_generic_is_set(struct bt_field *);
static
bt_bool bt_field_structure_is_set_recursive(struct bt_field *);
static
bt_bool bt_field_variant_is_set_recursive(struct bt_field *);
static
bt_bool bt_field_enumeration_is_set_recursive(struct bt_field *);
static
bt_bool bt_field_array_is_set_recursive(struct bt_field *);
static
bt_bool bt_field_sequence_is_set_recursive(struct bt_field *);

static
int increase_packet_size(struct bt_stream_pos *pos);

static
struct bt_field *(* const field_create_funcs[])(struct bt_field_type *) = {
	[BT_FIELD_TYPE_ID_INTEGER] = bt_field_integer_create,
	[BT_FIELD_TYPE_ID_ENUM] = bt_field_enumeration_create,
	[BT_FIELD_TYPE_ID_FLOAT] = bt_field_floating_point_create,
	[BT_FIELD_TYPE_ID_STRUCT] = bt_field_structure_create,
	[BT_FIELD_TYPE_ID_VARIANT] = bt_field_variant_create,
	[BT_FIELD_TYPE_ID_ARRAY] = bt_field_array_create,
	[BT_FIELD_TYPE_ID_SEQUENCE] = bt_field_sequence_create,
	[BT_FIELD_TYPE_ID_STRING] = bt_field_string_create,
};

static
void (* const field_destroy_funcs[])(struct bt_field *) = {
	[BT_FIELD_TYPE_ID_INTEGER] = bt_field_integer_destroy,
	[BT_FIELD_TYPE_ID_ENUM] = bt_field_enumeration_destroy_recursive,
	[BT_FIELD_TYPE_ID_FLOAT] = bt_field_floating_point_destroy,
	[BT_FIELD_TYPE_ID_STRUCT] = bt_field_structure_destroy_recursive,
	[BT_FIELD_TYPE_ID_VARIANT] = bt_field_variant_destroy_recursive,
	[BT_FIELD_TYPE_ID_ARRAY] = bt_field_array_destroy_recursive,
	[BT_FIELD_TYPE_ID_SEQUENCE] = bt_field_sequence_destroy_recursive,
	[BT_FIELD_TYPE_ID_STRING] = bt_field_string_destroy,
};

static
int (* const field_validate_funcs[])(struct bt_field *) = {
	[BT_FIELD_TYPE_ID_INTEGER] = bt_field_generic_validate,
	[BT_FIELD_TYPE_ID_ENUM] = bt_field_enumeration_validate_recursive,
	[BT_FIELD_TYPE_ID_FLOAT] = bt_field_generic_validate,
	[BT_FIELD_TYPE_ID_STRUCT] = bt_field_structure_validate_recursive,
	[BT_FIELD_TYPE_ID_VARIANT] = bt_field_variant_validate_recursive,
	[BT_FIELD_TYPE_ID_ARRAY] = bt_field_array_validate_recursive,
	[BT_FIELD_TYPE_ID_SEQUENCE] = bt_field_sequence_validate_recursive,
	[BT_FIELD_TYPE_ID_STRING] = bt_field_generic_validate,
};

static
void (* const field_reset_funcs[])(struct bt_field *) = {
	[BT_FIELD_TYPE_ID_INTEGER] = bt_field_generic_reset,
	[BT_FIELD_TYPE_ID_ENUM] = bt_field_enumeration_reset_recursive,
	[BT_FIELD_TYPE_ID_FLOAT] = bt_field_generic_reset,
	[BT_FIELD_TYPE_ID_STRUCT] = bt_field_structure_reset_recursive,
	[BT_FIELD_TYPE_ID_VARIANT] = bt_field_variant_reset_recursive,
	[BT_FIELD_TYPE_ID_ARRAY] = bt_field_array_reset_recursive,
	[BT_FIELD_TYPE_ID_SEQUENCE] = bt_field_sequence_reset_recursive,
	[BT_FIELD_TYPE_ID_STRING] = bt_field_string_reset_recursive,
};

static
int (* const field_serialize_funcs[])(struct bt_field *,
		struct bt_stream_pos *, enum bt_byte_order) = {
	[BT_FIELD_TYPE_ID_INTEGER] = bt_field_integer_serialize,
	[BT_FIELD_TYPE_ID_ENUM] = bt_field_enumeration_serialize_recursive,
	[BT_FIELD_TYPE_ID_FLOAT] = bt_field_floating_point_serialize,
	[BT_FIELD_TYPE_ID_STRUCT] = bt_field_structure_serialize_recursive,
	[BT_FIELD_TYPE_ID_VARIANT] = bt_field_variant_serialize_recursive,
	[BT_FIELD_TYPE_ID_ARRAY] = bt_field_array_serialize_recursive,
	[BT_FIELD_TYPE_ID_SEQUENCE] = bt_field_sequence_serialize_recursive,
	[BT_FIELD_TYPE_ID_STRING] = bt_field_string_serialize,
};

static
int (* const field_copy_funcs[])(struct bt_field *,
		struct bt_field *) = {
	[BT_FIELD_TYPE_ID_INTEGER] = bt_field_integer_copy,
	[BT_FIELD_TYPE_ID_ENUM] = bt_field_enumeration_copy_recursive,
	[BT_FIELD_TYPE_ID_FLOAT] = bt_field_floating_point_copy,
	[BT_FIELD_TYPE_ID_STRUCT] = bt_field_structure_copy_recursive,
	[BT_FIELD_TYPE_ID_VARIANT] = bt_field_variant_copy_recursive,
	[BT_FIELD_TYPE_ID_ARRAY] = bt_field_array_copy_recursive,
	[BT_FIELD_TYPE_ID_SEQUENCE] = bt_field_sequence_copy_recursive,
	[BT_FIELD_TYPE_ID_STRING] = bt_field_string_copy_recursive,
};

static
void (* const field_freeze_funcs[])(struct bt_field *) = {
	[BT_FIELD_TYPE_ID_INTEGER] = generic_field_freeze,
	[BT_FIELD_TYPE_ID_FLOAT] = generic_field_freeze,
	[BT_FIELD_TYPE_ID_STRING] = generic_field_freeze,
	[BT_FIELD_TYPE_ID_ENUM] = bt_field_enumeration_freeze_recursive,
	[BT_FIELD_TYPE_ID_STRUCT] = bt_field_structure_freeze_recursive,
	[BT_FIELD_TYPE_ID_VARIANT] = bt_field_variant_freeze_recursive,
	[BT_FIELD_TYPE_ID_ARRAY] = bt_field_array_freeze_recursive,
	[BT_FIELD_TYPE_ID_SEQUENCE] = bt_field_sequence_freeze_recursive,
};

static
bt_bool (* const field_is_set_funcs[])(struct bt_field *) = {
	[BT_FIELD_TYPE_ID_INTEGER] = bt_field_generic_is_set,
	[BT_FIELD_TYPE_ID_ENUM] = bt_field_enumeration_is_set_recursive,
	[BT_FIELD_TYPE_ID_FLOAT] = bt_field_generic_is_set,
	[BT_FIELD_TYPE_ID_STRUCT] = bt_field_structure_is_set_recursive,
	[BT_FIELD_TYPE_ID_VARIANT] = bt_field_variant_is_set_recursive,
	[BT_FIELD_TYPE_ID_ARRAY] = bt_field_array_is_set_recursive,
	[BT_FIELD_TYPE_ID_SEQUENCE] = bt_field_sequence_is_set_recursive,
	[BT_FIELD_TYPE_ID_STRING] = bt_field_generic_is_set,
};

BT_ASSERT_FUNC
static inline bool field_type_has_known_id(struct bt_field_type *ft)
{
	return ft->id > BT_FIELD_TYPE_ID_UNKNOWN ||
		ft->id < BT_FIELD_TYPE_ID_NR;
}

#define BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(_field, _type_id, _name)		\
	BT_ASSERT_PRE((_field)->type->id == (_type_id),			\
		_name " has the wrong type ID: expected-type-id=%s, "	\
		"%![field-]+f", bt_field_type_id_string(_type_id),	\
		(_field))

#define BT_ASSERT_PRE_FIELD_IS_SET(_field, _name)			\
	BT_ASSERT_PRE(bt_field_is_set_recursive(_field),		\
		_name " is not set: %!+f", (_field))

#define BT_ASSERT_PRE_FIELD_HOT(_field, _name)				\
	BT_ASSERT_PRE_HOT((_field), (_name), ": +%!+f", (_field))

struct bt_field *bt_field_create(struct bt_field_type *type)
{
	struct bt_field *field = NULL;
	enum bt_field_type_id type_id;

	BT_ASSERT_PRE_NON_NULL(type, "Field type");
	BT_ASSERT(field_type_has_known_id(type));
	BT_ASSERT_PRE(bt_field_type_validate(type) == 0,
		"Field type is invalid: %!+F", type);

	type_id = bt_field_type_get_type_id(type);
	field = field_create_funcs[type_id](type);
	if (!field) {
		goto error;
	}

	/* The type's declaration can't change after this point */
	bt_field_type_freeze(type);
	bt_get(type);
	bt_object_init(field, bt_field_destroy);
	field->type = type;
error:
	return field;
}

/* Pre-2.0 CTF writer backward compatibility */
void bt_ctf_field_get(struct bt_field *field)
{
	bt_get(field);
}

/* Pre-2.0 CTF writer backward compatibility */
void bt_ctf_field_put(struct bt_field *field)
{
	bt_put(field);
}

struct bt_field_type *bt_field_get_type(struct bt_field *field)
{
	struct bt_field_type *ret = NULL;

	BT_ASSERT_PRE_NON_NULL(field, "Field");
	ret = field->type;
	bt_get(ret);
	return ret;
}

enum bt_field_type_id bt_field_get_type_id(struct bt_field *field)
{
	BT_ASSERT_PRE_NON_NULL(field, "Field");
	return bt_field_type_get_type_id(field->type);
}

bt_bool bt_field_is_integer(struct bt_field *field)
{
	return bt_field_get_type_id(field) == BT_FIELD_TYPE_ID_INTEGER;
}

bt_bool bt_field_is_floating_point(struct bt_field *field)
{
	return bt_field_get_type_id(field) == BT_FIELD_TYPE_ID_FLOAT;
}

bt_bool bt_field_is_enumeration(struct bt_field *field)
{
	return bt_field_get_type_id(field) == BT_FIELD_TYPE_ID_ENUM;
}

bt_bool bt_field_is_string(struct bt_field *field)
{
	return bt_field_get_type_id(field) == BT_FIELD_TYPE_ID_STRING;
}

bt_bool bt_field_is_structure(struct bt_field *field)
{
	return bt_field_get_type_id(field) == BT_FIELD_TYPE_ID_STRUCT;
}

bt_bool bt_field_is_array(struct bt_field *field)
{
	return bt_field_get_type_id(field) == BT_FIELD_TYPE_ID_ARRAY;
}

bt_bool bt_field_is_sequence(struct bt_field *field)
{
	return bt_field_get_type_id(field) == BT_FIELD_TYPE_ID_SEQUENCE;
}

bt_bool bt_field_is_variant(struct bt_field *field)
{
	return bt_field_get_type_id(field) == BT_FIELD_TYPE_ID_VARIANT;
}

BT_HIDDEN
int64_t bt_field_sequence_get_int_length(struct bt_field *field)
{
	struct bt_field_sequence *sequence;
	int64_t ret;

	BT_ASSERT(field);
	BT_ASSERT(bt_field_type_get_type_id(field->type) ==
		BT_FIELD_TYPE_ID_SEQUENCE);
	sequence = container_of(field, struct bt_field_sequence, parent);
	if (!sequence->length) {
		ret = -1;
		goto end;
	}

	ret = (int64_t) sequence->elements->len;

end:
	return ret;
}

struct bt_field *bt_field_sequence_get_length(
		struct bt_field *field)
{
	struct bt_field *ret = NULL;
	struct bt_field_sequence *sequence;

	BT_ASSERT_PRE_NON_NULL(field, "Sequence field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field, BT_FIELD_TYPE_ID_SEQUENCE,
		"Field");
	sequence = container_of(field, struct bt_field_sequence, parent);
	ret = sequence->length;
	bt_get(ret);
	return ret;
}

int bt_field_sequence_set_length(struct bt_field *field,
		struct bt_field *length_field)
{
	int ret = 0;
	struct bt_field_integer *length;
	struct bt_field_sequence *sequence;
	uint64_t sequence_length;

	BT_ASSERT_PRE_NON_NULL(field, "Sequence field");
	BT_ASSERT_PRE_NON_NULL(length_field, "Length field");
	BT_ASSERT_PRE_FIELD_HOT(field, "Sequence field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(length_field, BT_FIELD_TYPE_ID_INTEGER,
		"Length field");
	BT_ASSERT_PRE(!bt_field_type_integer_is_signed(length_field->type),
		"Length field's type is signed: %!+f", length_field);
	BT_ASSERT_PRE_FIELD_IS_SET(length_field, "Length field");

	length = container_of(length_field, struct bt_field_integer,
		parent);
	sequence_length = length->payload.unsignd;
	sequence = container_of(field, struct bt_field_sequence, parent);
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
	bt_field_freeze_recursive(length_field);

end:
	return ret;
}

struct bt_field *bt_field_structure_get_field_by_name(
		struct bt_field *field, const char *name)
{
	struct bt_field *ret = NULL;
	GQuark field_quark;
	struct bt_field_structure *structure;
	size_t index;
	GHashTable *field_name_to_index;

	BT_ASSERT_PRE_NON_NULL(field, "Structure field");
	BT_ASSERT_PRE_NON_NULL(name, "Field name");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field, BT_FIELD_TYPE_ID_STRUCT, "Field");
	field_name_to_index =
		container_of(field->type, struct bt_field_type_structure,
			parent)->field_name_to_index;
	field_quark = g_quark_from_string(name);
	structure = container_of(field, struct bt_field_structure, parent);
	if (!g_hash_table_lookup_extended(field_name_to_index,
			GUINT_TO_POINTER(field_quark),
			NULL, (gpointer *)&index)) {
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

struct bt_field *bt_field_structure_get_field_by_index(
		struct bt_field *field, uint64_t index)
{
	struct bt_field_structure *structure;
	struct bt_field *ret = NULL;

	BT_ASSERT_PRE_NON_NULL(field, "Structure field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field, BT_FIELD_TYPE_ID_STRUCT, "Field");
	structure = container_of(field, struct bt_field_structure, parent);
	BT_ASSERT_PRE(index < structure->fields->len,
		"Index is out of bound: %![struct-field-]+f, "
		"index=%" PRIu64 ", count=%u", field, index,
		structure->fields->len);
	ret = bt_get(structure->fields->pdata[index]);
	return ret;
}

BT_ASSERT_PRE_FUNC
static inline bool field_to_set_has_expected_type(struct bt_field *struct_field,
		const char *name, struct bt_field *value)
{
	bool ret = true;
	struct bt_field_type *expected_field_type = NULL;

	expected_field_type =
		bt_field_type_structure_get_field_type_by_name(
			struct_field->type, name);

	if (bt_field_type_compare(expected_field_type, value->type)) {
		BT_ASSERT_PRE_MSG("Value field's type is different from the expected field type: "
			"%![value-ft-]+F, %![expected-ft-]+F", value->type,
			expected_field_type);
		ret = false;
		goto end;
	}

end:
	bt_put(expected_field_type);
	return ret;
}

int bt_field_structure_set_field_by_name(struct bt_field *field,
		const char *name, struct bt_field *value)
{
	int ret = 0;
	GQuark field_quark;
	struct bt_field_structure *structure;
	size_t index;
	GHashTable *field_name_to_index;

	BT_ASSERT_PRE_NON_NULL(field, "Structure field");
	BT_ASSERT_PRE_NON_NULL(name, "Field name");
	BT_ASSERT_PRE_NON_NULL(value, "Value field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field, BT_FIELD_TYPE_ID_STRUCT,
		"Parent field");
	BT_ASSERT_PRE(field_to_set_has_expected_type(field, name, value),
		"Value field's type is different from the expected field type.");
	field_quark = g_quark_from_string(name);
	structure = container_of(field, struct bt_field_structure, parent);
	field_name_to_index =
		container_of(field->type, struct bt_field_type_structure,
			parent)->field_name_to_index;
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

struct bt_field *bt_field_array_get_field(struct bt_field *field,
		uint64_t index)
{
	struct bt_field *new_field = NULL;
	struct bt_field_type *field_type = NULL;
	struct bt_field_array *array;

	BT_ASSERT_PRE_NON_NULL(field, "Array field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field, BT_FIELD_TYPE_ID_ARRAY, "Field");
	array = container_of(field, struct bt_field_array, parent);
	BT_ASSERT_PRE(index < array->elements->len,
		"Index is out of bound: %![array-field-]+f, "
		"index=%" PRIu64 ", count=%u", field,
		index, array->elements->len);

	field_type = bt_field_type_array_get_element_type(field->type);
	if (array->elements->pdata[(size_t) index]) {
		new_field = array->elements->pdata[(size_t) index];
		goto end;
	}

	/* We don't want to modify this field if it's frozen */
	BT_ASSERT_PRE_FIELD_HOT(field, "Array field");
	new_field = bt_field_create(field_type);
	array->elements->pdata[(size_t)index] = new_field;

end:
	bt_put(field_type);
	bt_get(new_field);
	return new_field;
}

struct bt_field *bt_field_sequence_get_field(struct bt_field *field,
		uint64_t index)
{
	struct bt_field *new_field = NULL;
	struct bt_field_type *field_type = NULL;
	struct bt_field_sequence *sequence;

	BT_ASSERT_PRE_NON_NULL(field, "Sequence field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field, BT_FIELD_TYPE_ID_SEQUENCE,
		"Field");
	sequence = container_of(field, struct bt_field_sequence, parent);
	BT_ASSERT_PRE_NON_NULL(sequence->elements, "Sequence field's element array");
	BT_ASSERT_PRE(index < sequence->elements->len,
		"Index is out of bound: %![seq-field-]+f, "
		"index=%" PRIu64 ", count=%u", field, index,
		sequence->elements->len);
	field_type = bt_field_type_sequence_get_element_type(field->type);
	if (sequence->elements->pdata[(size_t) index]) {
		new_field = sequence->elements->pdata[(size_t) index];
		goto end;
	}

	/* We don't want to modify this field if it's frozen */
	BT_ASSERT_PRE_FIELD_HOT(field, "Sequence field");
	new_field = bt_field_create(field_type);
	sequence->elements->pdata[(size_t) index] = new_field;

end:
	bt_put(field_type);
	bt_get(new_field);
	return new_field;
}

struct bt_field *bt_field_variant_get_field(struct bt_field *field,
		struct bt_field *tag_field)
{
	struct bt_field *new_field = NULL;
	struct bt_field_variant *variant;
	struct bt_field_type_variant *variant_type;
	struct bt_field_type *field_type;
	struct bt_field *tag_enum = NULL;
	struct bt_field_integer *tag_enum_integer;
	int64_t tag_enum_value;

	BT_ASSERT_PRE_NON_NULL(field, "Variant field");
	BT_ASSERT_PRE_NON_NULL(tag_field, "Tag field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field, BT_FIELD_TYPE_ID_VARIANT,
		"Variant field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(tag_field, BT_FIELD_TYPE_ID_ENUM,
		"Tag field");
	variant = container_of(field, struct bt_field_variant, parent);
	variant_type = container_of(field->type,
		struct bt_field_type_variant, parent);
	tag_enum = bt_field_enumeration_get_container(tag_field);
	BT_ASSERT_PRE_NON_NULL(tag_enum, "Tag field's container");
	tag_enum_integer = container_of(tag_enum, struct bt_field_integer,
		parent);
	BT_ASSERT_PRE(bt_field_validate_recursive(tag_field) == 0,
		"Tag field is invalid: %!+f", tag_field);
	tag_enum_value = tag_enum_integer->payload.signd;

	/*
	 * If the variant currently has a tag and a payload, and if the
	 * requested tag value is the same as the current one, return
	 * the current payload instead of creating a fresh one.
	 */
	if (variant->tag && variant->payload) {
		struct bt_field *cur_tag_container = NULL;
		struct bt_field_integer *cur_tag_enum_integer;
		int64_t cur_tag_value;

		cur_tag_container =
			bt_field_enumeration_get_container(variant->tag);
		BT_ASSERT(cur_tag_container);
		cur_tag_enum_integer = container_of(cur_tag_container,
			struct bt_field_integer, parent);
		bt_put(cur_tag_container);
		cur_tag_value = cur_tag_enum_integer->payload.signd;

		if (cur_tag_value == tag_enum_value) {
			new_field = variant->payload;
			bt_get(new_field);
			goto end;
		}
	}

	/* We don't want to modify this field if it's frozen */
	BT_ASSERT_PRE_FIELD_HOT(field, "Variant field");
	field_type = bt_field_type_variant_get_field_type_signed(
		variant_type, tag_enum_value);

	/* It's the caller's job to make sure the tag's value is valid */
	BT_ASSERT_PRE(field_type,
		"Variant field's type does not contain a field type for "
		"this tag value: tag-value-signed=%" PRId64 ", "
		"%![var-ft-]+F, %![tag-field-]+f", tag_enum_value,
		variant_type, tag_field);

	new_field = bt_field_create(field_type);
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

struct bt_field *bt_field_variant_get_current_field(
		struct bt_field *variant_field)
{
	struct bt_field_variant *variant;

	BT_ASSERT_PRE_NON_NULL(variant_field, "Variant field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(variant_field, BT_FIELD_TYPE_ID_VARIANT,
		"Field");
	variant = container_of(variant_field, struct bt_field_variant,
		parent);
	return bt_get(variant->payload);
}

struct bt_field *bt_field_variant_get_tag(struct bt_field *variant_field)
{
	struct bt_field_variant *variant;

	BT_ASSERT_PRE_NON_NULL(variant_field, "Variant field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(variant_field, BT_FIELD_TYPE_ID_VARIANT,
		"Field");
	variant = container_of(variant_field, struct bt_field_variant,
			parent);
	return bt_get(variant->tag);
}

struct bt_field *bt_field_enumeration_get_container(struct bt_field *field)
{
	struct bt_field *container = NULL;
	struct bt_field_enumeration *enumeration;

	BT_ASSERT_PRE_NON_NULL(field, "Enumeration field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field, BT_FIELD_TYPE_ID_ENUM, "Field");
	enumeration = container_of(field, struct bt_field_enumeration,
		parent);
	if (!enumeration->payload) {
		struct bt_field_type_enumeration *enumeration_type;

		/* We don't want to modify this field if it's frozen */
		BT_ASSERT_PRE_FIELD_HOT(field, "Enumeration field");

		enumeration_type = container_of(field->type,
			struct bt_field_type_enumeration, parent);
		enumeration->payload =
			bt_field_create(enumeration_type->container);
	}

	container = enumeration->payload;
	return bt_get(container);
}

struct bt_field_type_enumeration_mapping_iterator *
bt_field_enumeration_get_mappings(struct bt_field *field)
{
	int ret;
	struct bt_field *container = NULL;
	struct bt_field_type_integer *integer_type = NULL;
	struct bt_field_type_enumeration_mapping_iterator *iter = NULL;

	BT_ASSERT_PRE_NON_NULL(field, "Enumeration field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field, BT_FIELD_TYPE_ID_ENUM, "Field");
	container = bt_field_enumeration_get_container(field);
	BT_ASSERT_PRE(container,
		"Enumeration field has no container field: %!+f", field);
	BT_ASSERT(container->type);
	integer_type = container_of(container->type,
		struct bt_field_type_integer, parent);
	BT_ASSERT_PRE_FIELD_IS_SET(container,
		"Enumeration field's payload field");

	if (!integer_type->is_signed) {
		uint64_t value;

		ret = bt_field_unsigned_integer_get_value(container, &value);
		BT_ASSERT(ret == 0);
		iter = bt_field_type_enumeration_find_mappings_by_unsigned_value(
				field->type, value);
	} else {
		int64_t value;

		ret = bt_field_signed_integer_get_value(container, &value);
		BT_ASSERT(ret == 0);
		iter = bt_field_type_enumeration_find_mappings_by_signed_value(
				field->type, value);
	}

	bt_put(container);
	return iter;
}

int bt_field_signed_integer_get_value(struct bt_field *field, int64_t *value)
{
	struct bt_field_integer *integer;
	struct bt_field_type_integer *integer_type;

	BT_ASSERT_PRE_NON_NULL(field, "Integer field");
	BT_ASSERT_PRE_NON_NULL(value, "Value");
	BT_ASSERT_PRE_FIELD_IS_SET(field, "Integer field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field, BT_FIELD_TYPE_ID_INTEGER, "Field");
	integer_type = container_of(field->type,
		struct bt_field_type_integer, parent);
	BT_ASSERT_PRE(bt_field_type_integer_is_signed(field->type),
		"Field's type is unsigned: %!+f", field);
	integer = container_of(field,
		struct bt_field_integer, parent);
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

int bt_field_signed_integer_set_value(struct bt_field *field, int64_t value)
{
	int ret = 0;
	struct bt_field_integer *integer;
	struct bt_field_type_integer *integer_type;

	BT_ASSERT_PRE_NON_NULL(field, "Integer field");
	BT_ASSERT_PRE_FIELD_HOT(field, "Integer field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field, BT_FIELD_TYPE_ID_INTEGER, "Field");
	integer = container_of(field, struct bt_field_integer, parent);
	integer_type = container_of(field->type,
		struct bt_field_type_integer, parent);
	BT_ASSERT_PRE(bt_field_type_integer_is_signed(field->type),
		"Field's type is unsigned: %!+f", field);
	BT_ASSERT_PRE(value_is_in_range_signed(integer_type->size, value),
		"Value is out of bounds: value=%" PRId64 ", %![field-]+f",
		value, field);
	integer->payload.signd = value;
	bt_field_set(field, true);
	return ret;
}

int bt_field_unsigned_integer_get_value(struct bt_field *field, uint64_t *value)
{
	struct bt_field_integer *integer;
	struct bt_field_type_integer *integer_type;

	BT_ASSERT_PRE_NON_NULL(field, "Integer field");
	BT_ASSERT_PRE_NON_NULL(value, "Value");
	BT_ASSERT_PRE_FIELD_IS_SET(field, "Integer field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field, BT_FIELD_TYPE_ID_INTEGER, "Field");
	integer_type = container_of(field->type,
		struct bt_field_type_integer, parent);
	BT_ASSERT_PRE(!bt_field_type_integer_is_signed(field->type),
		"Field's type is signed: %!+f", field);
	integer = container_of(field, struct bt_field_integer, parent);
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

int bt_field_unsigned_integer_set_value(struct bt_field *field, uint64_t value)
{
	struct bt_field_integer *integer;
	struct bt_field_type_integer *integer_type;

	BT_ASSERT_PRE_NON_NULL(field, "Integer field");
	BT_ASSERT_PRE_FIELD_HOT(field, "Integer field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field, BT_FIELD_TYPE_ID_INTEGER, "Field");
	integer = container_of(field, struct bt_field_integer, parent);
	integer_type = container_of(field->type,
		struct bt_field_type_integer, parent);
	BT_ASSERT_PRE(!bt_field_type_integer_is_signed(field->type),
		"Field's type is signed: %!+f", field);
	BT_ASSERT_PRE(value_is_in_range_unsigned(integer_type->size, value),
		"Value is out of bounds: value=%" PRIu64 ", %![field-]+f",
		value, field);
	integer->payload.unsignd = value;
	bt_field_set(field, true);
	return 0;
}

int bt_field_floating_point_get_value(struct bt_field *field, double *value)
{
	struct bt_field_floating_point *floating_point;

	BT_ASSERT_PRE_NON_NULL(field, "Floating point number field");
	BT_ASSERT_PRE_NON_NULL(value, "Value");
	BT_ASSERT_PRE_FIELD_IS_SET(field, "Floating point number field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field, BT_FIELD_TYPE_ID_FLOAT, "Field");
	floating_point = container_of(field,
		struct bt_field_floating_point, parent);
	*value = floating_point->payload;
	return 0;
}

int bt_field_floating_point_set_value(struct bt_field *field, double value)
{
	struct bt_field_floating_point *floating_point;

	BT_ASSERT_PRE_NON_NULL(field, "Floating point number field");
	BT_ASSERT_PRE_FIELD_HOT(field, "Floating point number field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field, BT_FIELD_TYPE_ID_FLOAT, "Field");
	floating_point = container_of(field, struct bt_field_floating_point,
		parent);
	floating_point->payload = value;
	bt_field_set(field, true);
	return 0;
}

const char *bt_field_string_get_value(struct bt_field *field)
{
	struct bt_field_string *string;

	BT_ASSERT_PRE_NON_NULL(field, "String field");
	BT_ASSERT_PRE_FIELD_IS_SET(field, "String field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field, BT_FIELD_TYPE_ID_STRING, "Field");
	string = container_of(field, struct bt_field_string, parent);
	return string->payload->str;
}

int bt_field_string_set_value(struct bt_field *field, const char *value)
{
	struct bt_field_string *string;

	BT_ASSERT_PRE_NON_NULL(field, "String field");
	BT_ASSERT_PRE_NON_NULL(value, "Value");
	BT_ASSERT_PRE_FIELD_HOT(field, "String field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field, BT_FIELD_TYPE_ID_STRING, "Field");
	string = container_of(field, struct bt_field_string, parent);
	if (string->payload) {
		g_string_assign(string->payload, value);
	} else {
		string->payload = g_string_new(value);
	}

	bt_field_set(field, true);
	return 0;
}

int bt_field_string_append(struct bt_field *field, const char *value)
{
	struct bt_field_string *string_field;

	BT_ASSERT_PRE_NON_NULL(field, "String field");
	BT_ASSERT_PRE_NON_NULL(value, "Value");
	BT_ASSERT_PRE_FIELD_HOT(field, "String field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field, BT_FIELD_TYPE_ID_STRING, "Field");
	string_field = container_of(field, struct bt_field_string, parent);
	if (string_field->payload) {
		g_string_append(string_field->payload, value);
	} else {
		string_field->payload = g_string_new(value);
	}

	bt_field_set(field, true);
	return 0;
}

int bt_field_string_append_len(struct bt_field *field, const char *value,
		unsigned int length)
{
	struct bt_field_string *string_field;

	BT_ASSERT_PRE_NON_NULL(field, "String field");
	BT_ASSERT_PRE_NON_NULL(value, "Value");
	BT_ASSERT_PRE_FIELD_HOT(field, "String field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field, BT_FIELD_TYPE_ID_STRING, "Field");
	string_field = container_of(field, struct bt_field_string, parent);

	/* make sure no null bytes are appended */
	BT_ASSERT_PRE(memchr(value, '\0', length) == NULL,
		"String value to append contains a null character: "
		"partial-value=\"%.32s\", length=%u", value, length);

	if (string_field->payload) {
		g_string_append_len(string_field->payload, value, length);
	} else {
		string_field->payload = g_string_new_len(value, length);
	}

	bt_field_set(field, true);
	return 0;
}

BT_HIDDEN
int _bt_field_validate_recursive(struct bt_field *field)
{
	int ret = 0;
	enum bt_field_type_id type_id;

	if (!field) {
		BT_ASSERT_PRE_MSG("%s", "Invalid field: field is NULL.");
		ret = -1;
		goto end;
	}

	BT_ASSERT(field_type_has_known_id(field->type));
	type_id = bt_field_type_get_type_id(field->type);
	ret = field_validate_funcs[type_id](field);

end:
	return ret;
}

BT_HIDDEN
void _bt_field_reset_recursive(struct bt_field *field)
{	BT_ASSERT(field);
	field_reset_funcs[field->type->id](field);
}

BT_HIDDEN
void _bt_field_set(struct bt_field *field, bool value)
{
	BT_ASSERT(field);
	field->payload_set = value;
}

BT_HIDDEN
int bt_field_serialize_recursive(struct bt_field *field, struct bt_stream_pos *pos,
		enum bt_byte_order native_byte_order)
{
	enum bt_field_type_id type_id;

	BT_ASSERT(pos);
	BT_ASSERT_PRE_NON_NULL(field, "Field");
	BT_ASSERT(field_type_has_known_id(field->type));
	type_id = bt_field_type_get_type_id(field->type);
	return field_serialize_funcs[type_id](field, pos, native_byte_order);
}

BT_HIDDEN
bt_bool _bt_field_is_set_recursive(struct bt_field *field)
{
	bt_bool is_set = BT_FALSE;
	enum bt_field_type_id type_id;

	if (!field) {
		goto end;
	}

	BT_ASSERT(field_type_has_known_id(field->type));
	type_id = bt_field_type_get_type_id(field->type);
	is_set = field_is_set_funcs[type_id](field);

end:
	return is_set;
}

struct bt_field *bt_field_copy(struct bt_field *field)
{
	int ret;
	struct bt_field *copy = NULL;
	enum bt_field_type_id type_id;

	BT_ASSERT_PRE_NON_NULL(field, "Field");
	BT_ASSERT(field_type_has_known_id(field->type));
	copy = bt_field_create(field->type);
	if (!copy) {
		BT_LOGW("Cannot create field: ft-addr=%p", field->type);
		goto end;
	}

	bt_field_set(copy, field->payload_set);
	type_id = bt_field_type_get_type_id(field->type);
	ret = field_copy_funcs[type_id](field, copy);
	if (ret) {
		BT_PUT(copy);
	}

end:
	return copy;
}

static
struct bt_field *bt_field_integer_create(struct bt_field_type *type)
{
	struct bt_field_integer *integer = g_new0(
		struct bt_field_integer, 1);

	BT_LOGD("Creating integer field object: ft-addr=%p", type);

	if (integer) {
		BT_LOGD("Created integer field object: addr=%p, ft-addr=%p",
			&integer->parent, type);
	} else {
		BT_LOGE_STR("Failed to allocate one integer field.");
	}

	return integer ? &integer->parent : NULL;
}

static
struct bt_field *bt_field_enumeration_create(
	struct bt_field_type *type)
{
	struct bt_field_enumeration *enumeration = g_new0(
		struct bt_field_enumeration, 1);

	BT_LOGD("Creating enumeration field object: ft-addr=%p", type);

	if (enumeration) {
		BT_LOGD("Created enumeration field object: addr=%p, ft-addr=%p",
			&enumeration->parent, type);
	} else {
		BT_LOGE_STR("Failed to allocate one enumeration field.");
	}

	return enumeration ? &enumeration->parent : NULL;
}

static
struct bt_field *bt_field_floating_point_create(
	struct bt_field_type *type)
{
	struct bt_field_floating_point *floating_point;

	BT_LOGD("Creating floating point number field object: ft-addr=%p", type);
	floating_point = g_new0(struct bt_field_floating_point, 1);

	if (floating_point) {
		BT_LOGD("Created floating point number field object: addr=%p, ft-addr=%p",
			&floating_point->parent, type);
	} else {
		BT_LOGE_STR("Failed to allocate one floating point number field.");
	}

	return floating_point ? &floating_point->parent : NULL;
}

static
struct bt_field *bt_field_structure_create(
	struct bt_field_type *type)
{
	struct bt_field_type_structure *structure_type = container_of(type,
		struct bt_field_type_structure, parent);
	struct bt_field_structure *structure = g_new0(
		struct bt_field_structure, 1);
	struct bt_field *ret = NULL;
	size_t i;

	BT_LOGD("Creating structure field object: ft-addr=%p", type);

	if (!structure) {
		BT_LOGE_STR("Failed to allocate one structure field.");
		goto end;
	}

	structure->fields = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_put);
	g_ptr_array_set_size(structure->fields,
		structure_type->fields->len);

	/* Create all fields contained by the structure field. */
	for (i = 0; i < structure_type->fields->len; i++) {
		struct bt_field *field;
		struct structure_field *field_type =
			g_ptr_array_index(structure_type->fields, i);

		field = bt_field_create(field_type->type);
		if (!field) {
			BT_LOGE("Failed to create structure field's member: name=\"%s\", index=%zu",
				g_quark_to_string(field_type->name), i);
			bt_field_structure_destroy_recursive(&structure->parent);
			goto end;
		}

		g_ptr_array_index(structure->fields, i) = field;
	}

	ret = &structure->parent;
	BT_LOGD("Created structure field object: addr=%p, ft-addr=%p", ret,
		type);
end:
	return ret;
}

static
struct bt_field *bt_field_variant_create(struct bt_field_type *type)
{
	struct bt_field_variant *variant = g_new0(
		struct bt_field_variant, 1);

	BT_LOGD("Creating variant field object: ft-addr=%p", type);

	if (variant) {
		BT_LOGD("Created variant field object: addr=%p, ft-addr=%p",
			&variant->parent, type);
	} else {
		BT_LOGE_STR("Failed to allocate one variant field.");
	}

	return variant ? &variant->parent : NULL;
}

static
struct bt_field *bt_field_array_create(struct bt_field_type *type)
{
	struct bt_field_array *array = g_new0(struct bt_field_array, 1);
	struct bt_field_type_array *array_type;
	unsigned int array_length;

	BT_LOGD("Creating array field object: ft-addr=%p", type);
	BT_ASSERT(type);

	if (!array) {
		BT_LOGE_STR("Failed to allocate one array field.");
		goto error;
	}

	array_type = container_of(type, struct bt_field_type_array, parent);
	array_length = array_type->length;
	array->elements = g_ptr_array_sized_new(array_length);
	if (!array->elements) {
		goto error;
	}

	g_ptr_array_set_free_func(array->elements,
		(GDestroyNotify) bt_put);
	g_ptr_array_set_size(array->elements, array_length);
	BT_LOGD("Created array field object: addr=%p, ft-addr=%p",
		&array->parent, type);
	return &array->parent;
error:
	g_free(array);
	return NULL;
}

static
struct bt_field *bt_field_sequence_create(
	struct bt_field_type *type)
{
	struct bt_field_sequence *sequence = g_new0(
		struct bt_field_sequence, 1);

	BT_LOGD("Creating sequence field object: ft-addr=%p", type);

	if (sequence) {
		BT_LOGD("Created sequence field object: addr=%p, ft-addr=%p",
			&sequence->parent, type);
	} else {
		BT_LOGE_STR("Failed to allocate one sequence field.");
	}

	return sequence ? &sequence->parent : NULL;
}

static
struct bt_field *bt_field_string_create(struct bt_field_type *type)
{
	struct bt_field_string *string = g_new0(
		struct bt_field_string, 1);

	BT_LOGD("Creating string field object: ft-addr=%p", type);

	if (string) {
		BT_LOGD("Created string field object: addr=%p, ft-addr=%p",
			&string->parent, type);
	} else {
		BT_LOGE_STR("Failed to allocate one string field.");
	}

	return string ? &string->parent : NULL;
}

static
void bt_field_destroy(struct bt_object *obj)
{
	struct bt_field *field;
	struct bt_field_type *type;
	enum bt_field_type_id type_id;

	BT_ASSERT(obj);
	field = container_of(obj, struct bt_field, base);
	type = field->type;
	BT_ASSERT(field_type_has_known_id(type));
	type_id = bt_field_type_get_type_id(type);
	field_destroy_funcs[type_id](field);
	BT_LOGD_STR("Putting field's type.");
	bt_put(type);
}

static
void bt_field_integer_destroy(struct bt_field *field)
{
	struct bt_field_integer *integer;

	BT_ASSERT(field);
	BT_LOGD("Destroying integer field object: addr=%p", field);
	integer = container_of(field, struct bt_field_integer, parent);
	g_free(integer);
}

static
void bt_field_enumeration_destroy_recursive(struct bt_field *field)
{
	struct bt_field_enumeration *enumeration;

	BT_ASSERT(field);
	BT_LOGD("Destroying enumeration field object: addr=%p", field);
	enumeration = container_of(field, struct bt_field_enumeration,
		parent);
	BT_LOGD_STR("Putting payload field.");
	bt_put(enumeration->payload);
	g_free(enumeration);
}

static
void bt_field_floating_point_destroy(struct bt_field *field)
{
	struct bt_field_floating_point *floating_point;

	BT_ASSERT(field);
	BT_LOGD("Destroying floating point number field object: addr=%p", field);
	floating_point = container_of(field, struct bt_field_floating_point,
		parent);
	g_free(floating_point);
}

static
void bt_field_structure_destroy_recursive(struct bt_field *field)
{
	struct bt_field_structure *structure;

	BT_ASSERT(field);
	BT_LOGD("Destroying structure field object: addr=%p", field);
	structure = container_of(field, struct bt_field_structure, parent);
	g_ptr_array_free(structure->fields, TRUE);
	g_free(structure);
}

static
void bt_field_variant_destroy_recursive(struct bt_field *field)
{
	struct bt_field_variant *variant;

	BT_ASSERT(field);
	BT_LOGD("Destroying variant field object: addr=%p", field);
	variant = container_of(field, struct bt_field_variant, parent);
	BT_LOGD_STR("Putting tag field.");
	bt_put(variant->tag);
	BT_LOGD_STR("Putting payload field.");
	bt_put(variant->payload);
	g_free(variant);
}

static
void bt_field_array_destroy_recursive(struct bt_field *field)
{
	struct bt_field_array *array;

	BT_ASSERT(field);
	BT_LOGD("Destroying array field object: addr=%p", field);
	array = container_of(field, struct bt_field_array, parent);
	g_ptr_array_free(array->elements, TRUE);
	g_free(array);
}

static
void bt_field_sequence_destroy_recursive(struct bt_field *field)
{
	struct bt_field_sequence *sequence;

	BT_ASSERT(field);
	BT_LOGD("Destroying sequence field object: addr=%p", field);
	sequence = container_of(field, struct bt_field_sequence, parent);
	if (sequence->elements) {
		g_ptr_array_free(sequence->elements, TRUE);
	}
	BT_LOGD_STR("Putting length field.");
	bt_put(sequence->length);
	g_free(sequence);
}

static
void bt_field_string_destroy(struct bt_field *field)
{
	struct bt_field_string *string;

	BT_ASSERT(field);
	BT_LOGD("Destroying string field object: addr=%p", field);
	string = container_of(field, struct bt_field_string, parent);
	if (string->payload) {
		g_string_free(string->payload, TRUE);
	}
	g_free(string);
}

static
int bt_field_generic_validate(struct bt_field *field)
{
	return (field && field->payload_set) ? 0 : -1;
}

static
int bt_field_enumeration_validate_recursive(struct bt_field *field)
{
	int ret;
	struct bt_field_enumeration *enumeration;

	BT_ASSERT(field);
	enumeration = container_of(field, struct bt_field_enumeration,
		parent);
	if (!enumeration->payload) {
		BT_ASSERT_PRE_MSG("Invalid enumeration field: payload is not set: "
			"%!+f", field);
		ret = -1;
		goto end;
	}

	ret = bt_field_validate_recursive(enumeration->payload);

end:
	return ret;
}

static
int bt_field_structure_validate_recursive(struct bt_field *field)
{
	int64_t i;
	int ret = 0;
	struct bt_field_structure *structure;

	BT_ASSERT(field);
	structure = container_of(field, struct bt_field_structure, parent);

	for (i = 0; i < structure->fields->len; i++) {
		ret = bt_field_validate_recursive(
			(void *) structure->fields->pdata[i]);

		if (ret) {
			int this_ret;
			const char *name;

			this_ret = bt_field_type_structure_get_field_by_index(
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
	struct bt_field_variant *variant;

	BT_ASSERT(field);
	variant = container_of(field, struct bt_field_variant, parent);
	ret = bt_field_validate_recursive(variant->payload);
	if (ret) {
		BT_ASSERT_PRE_MSG("Invalid variant field's payload field: "
			"%![variant-field-]+f, %![payload-field-]+f",
			field, variant->payload);
	}

	return ret;
}

static
int bt_field_array_validate_recursive(struct bt_field *field)
{
	int64_t i;
	int ret = 0;
	struct bt_field_array *array;

	BT_ASSERT(field);
	array = container_of(field, struct bt_field_array, parent);
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
	struct bt_field_sequence *sequence;

	BT_ASSERT(field);
	sequence = container_of(field, struct bt_field_sequence, parent);
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
void bt_field_enumeration_reset_recursive(struct bt_field *field)
{
	struct bt_field_enumeration *enumeration;

	BT_ASSERT(field);
	enumeration = container_of(field, struct bt_field_enumeration,
		parent);
	if (!enumeration->payload) {
		return;
	}

	bt_field_reset_recursive(enumeration->payload);
}

static
void bt_field_structure_reset_recursive(struct bt_field *field)
{
	int64_t i;
	struct bt_field_structure *structure;

	BT_ASSERT(field);
	structure = container_of(field, struct bt_field_structure, parent);

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
	struct bt_field_variant *variant;

	BT_ASSERT(field);
	variant = container_of(field, struct bt_field_variant, parent);
	BT_PUT(variant->tag);
	BT_PUT(variant->payload);
}

static
void bt_field_array_reset_recursive(struct bt_field *field)
{
	size_t i;
	struct bt_field_array *array;

	BT_ASSERT(field);
	array = container_of(field, struct bt_field_array, parent);

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
	struct bt_field_sequence *sequence;

	BT_ASSERT(field);
	sequence = container_of(field, struct bt_field_sequence, parent);

	if (sequence->elements) {
		g_ptr_array_free(sequence->elements, TRUE);
		sequence->elements = NULL;
	}

	BT_PUT(sequence->length);
}

static
void bt_field_string_reset_recursive(struct bt_field *field)
{
	struct bt_field_string *string;

	BT_ASSERT(field);
	bt_field_generic_reset(field);
	string = container_of(field, struct bt_field_string, parent);
	if (string->payload) {
		g_string_truncate(string->payload, 0);
	}
}

static
int bt_field_integer_serialize(struct bt_field *field,
		struct bt_stream_pos *pos,
		enum bt_byte_order native_byte_order)
{
	int ret = 0;
	struct bt_field_integer *integer = container_of(field,
		struct bt_field_integer, parent);

	BT_ASSERT_PRE_FIELD_IS_SET(field, "Integer field");
	BT_LOGV("Serializing integer field: addr=%p, pos-offset=%" PRId64 ", "
		"native-bo=%s", field, pos->offset,
		bt_byte_order_string(native_byte_order));

retry:
	ret = bt_field_integer_write(integer, pos, native_byte_order);
	if (ret == -EFAULT) {
		/*
		 * The field is too large to fit in the current packet's
		 * remaining space. Bump the packet size and retry.
		 */
		ret = increase_packet_size(pos);
		if (ret) {
			BT_LOGE("Cannot increase packet size: ret=%d", ret);
			goto end;
		}
		goto retry;
	}

end:
	return ret;
}

static
int bt_field_enumeration_serialize_recursive(struct bt_field *field,
		struct bt_stream_pos *pos,
		enum bt_byte_order native_byte_order)
{
	struct bt_field_enumeration *enumeration = container_of(
		field, struct bt_field_enumeration, parent);

	BT_LOGV("Serializing enumeration field: addr=%p, pos-offset=%" PRId64 ", "
		"native-bo=%s", field, pos->offset,
		bt_byte_order_string(native_byte_order));
	BT_LOGV_STR("Serializing enumeration field's payload field.");
	return bt_field_serialize_recursive(enumeration->payload, pos,
		native_byte_order);
}

static
int bt_field_floating_point_serialize(struct bt_field *field,
		struct bt_stream_pos *pos,
		enum bt_byte_order native_byte_order)
{
	int ret = 0;
	struct bt_field_floating_point *floating_point = container_of(field,
		struct bt_field_floating_point, parent);

	BT_ASSERT_PRE_FIELD_IS_SET(field, "Floating point number field");
	BT_LOGV("Serializing floating point number field: addr=%p, pos-offset=%" PRId64 ", "
		"native-bo=%s", field, pos->offset,
		bt_byte_order_string(native_byte_order));

retry:
	ret = bt_field_floating_point_write(floating_point, pos,
		native_byte_order);
	if (ret == -EFAULT) {
		/*
		 * The field is too large to fit in the current packet's
		 * remaining space. Bump the packet size and retry.
		 */
		ret = increase_packet_size(pos);
		if (ret) {
			BT_LOGE("Cannot increase packet size: ret=%d", ret);
			goto end;
		}
		goto retry;
	}

end:
	return ret;
}

static
int bt_field_structure_serialize_recursive(struct bt_field *field,
		struct bt_stream_pos *pos,
		enum bt_byte_order native_byte_order)
{
	int64_t i;
	int ret = 0;
	struct bt_field_structure *structure = container_of(
		field, struct bt_field_structure, parent);

	BT_LOGV("Serializing structure field: addr=%p, pos-offset=%" PRId64 ", "
		"native-bo=%s", field, pos->offset,
		bt_byte_order_string(native_byte_order));

	while (!bt_stream_pos_access_ok(pos,
		offset_align(pos->offset, field->type->alignment))) {
		ret = increase_packet_size(pos);
		if (ret) {
			BT_LOGE("Cannot increase packet size: ret=%d", ret);
			goto end;
		}
	}

	if (!bt_stream_pos_align(pos, field->type->alignment)) {
		BT_LOGE("Cannot align packet's position: pos-offset=%" PRId64 ", "
			"align=%u", pos->offset, field->type->alignment);
		ret = -1;
		goto end;
	}

	for (i = 0; i < structure->fields->len; i++) {
		struct bt_field *member = g_ptr_array_index(
			structure->fields, i);
		const char *field_name = NULL;

		BT_LOGV("Serializing structure field's field: pos-offset=%" PRId64 ", "
			"field-addr=%p, index=%" PRId64,
			pos->offset, member, i);

		if (!member) {
			ret = bt_field_type_structure_get_field_by_index(
				field->type, &field_name, NULL, i);
			BT_ASSERT(ret == 0);
			BT_LOGW("Cannot serialize structure field's field: field is not set: "
				"struct-field-addr=%p, "
				"field-name=\"%s\", index=%" PRId64,
				field, field_name, i);
			ret = -1;
			goto end;
		}

		ret = bt_field_serialize_recursive(member, pos,
			native_byte_order);
		if (ret) {
			ret = bt_field_type_structure_get_field_by_index(
				field->type, &field_name, NULL, i);
			BT_ASSERT(ret == 0);
			BT_LOGW("Cannot serialize structure field's field: "
				"struct-field-addr=%p, field-addr=%p, "
				"field-name=\"%s\", index=%" PRId64,
				field->type, member, field_name, i);
			break;
		}
	}

end:
	return ret;
}

static
int bt_field_variant_serialize_recursive(struct bt_field *field,
		struct bt_stream_pos *pos,
		enum bt_byte_order native_byte_order)
{
	struct bt_field_variant *variant = container_of(
		field, struct bt_field_variant, parent);

	BT_LOGV("Serializing variant field: addr=%p, pos-offset=%" PRId64 ", "
		"native-bo=%s", field, pos->offset,
		bt_byte_order_string(native_byte_order));
	BT_LOGV_STR("Serializing variant field's payload field.");
	return bt_field_serialize_recursive(variant->payload, pos,
		native_byte_order);
}

static
int bt_field_array_serialize_recursive(struct bt_field *field,
		struct bt_stream_pos *pos,
		enum bt_byte_order native_byte_order)
{
	int64_t i;
	int ret = 0;
	struct bt_field_array *array = container_of(
		field, struct bt_field_array, parent);

	BT_LOGV("Serializing array field: addr=%p, pos-offset=%" PRId64 ", "
		"native-bo=%s", field, pos->offset,
		bt_byte_order_string(native_byte_order));

	for (i = 0; i < array->elements->len; i++) {
		struct bt_field *elem_field =
			g_ptr_array_index(array->elements, i);

		BT_LOGV("Serializing array field's element field: "
			"pos-offset=%" PRId64 ", field-addr=%p, index=%" PRId64,
			pos->offset, elem_field, i);
		ret = bt_field_serialize_recursive(elem_field, pos,
			native_byte_order);
		if (ret) {
			BT_LOGW("Cannot serialize array field's element field: "
				"array-field-addr=%p, field-addr=%p, "
				"index=%" PRId64, field, elem_field, i);
			goto end;
		}
	}

end:
	return ret;
}

static
int bt_field_sequence_serialize_recursive(struct bt_field *field,
		struct bt_stream_pos *pos,
		enum bt_byte_order native_byte_order)
{
	int64_t i;
	int ret = 0;
	struct bt_field_sequence *sequence = container_of(
		field, struct bt_field_sequence, parent);

	BT_LOGV("Serializing sequence field: addr=%p, pos-offset=%" PRId64 ", "
		"native-bo=%s", field, pos->offset,
		bt_byte_order_string(native_byte_order));

	for (i = 0; i < sequence->elements->len; i++) {
		struct bt_field *elem_field =
			g_ptr_array_index(sequence->elements, i);

		BT_LOGV("Serializing sequence field's element field: "
			"pos-offset=%" PRId64 ", field-addr=%p, index=%" PRId64,
			pos->offset, elem_field, i);
		ret = bt_field_serialize_recursive(elem_field, pos,
			native_byte_order);
		if (ret) {
			BT_LOGW("Cannot serialize sequence field's element field: "
				"sequence-field-addr=%p, field-addr=%p, "
				"index=%" PRId64, field, elem_field, i);
			goto end;
		}
	}

end:
	return ret;
}

static
int bt_field_string_serialize(struct bt_field *field, struct bt_stream_pos *pos,
		enum bt_byte_order native_byte_order)
{
	int64_t i;
	int ret = 0;
	struct bt_field_string *string = container_of(field,
		struct bt_field_string, parent);
	struct bt_field_type *character_type =
		get_field_type(FIELD_TYPE_ALIAS_UINT8_T);
	struct bt_field *character;

	BT_ASSERT_PRE_FIELD_IS_SET(field, "String field");
	BT_LOGV("Serializing string field: addr=%p, pos-offset=%" PRId64 ", "
		"native-bo=%s", field, pos->offset,
		bt_byte_order_string(native_byte_order));

	BT_LOGV_STR("Creating character field from string field's character field type.");
	character = bt_field_create(character_type);

	for (i = 0; i < string->payload->len + 1; i++) {
		const uint64_t chr = (uint64_t) string->payload->str[i];

		ret = bt_field_unsigned_integer_set_value(character, chr);
		BT_ASSERT(ret == 0);
		BT_LOGV("Serializing string field's character field: "
			"pos-offset=%" PRId64 ", field-addr=%p, "
			"index=%" PRId64 ", char-int=%" PRIu64,
			pos->offset, character, i, chr);
		ret = bt_field_integer_serialize(character, pos,
			native_byte_order);
		if (ret) {
			BT_LOGW_STR("Cannot serialize character field.");
			goto end;
		}
	}

end:
	bt_put(character);
	bt_put(character_type);
	return ret;
}

static
int bt_field_integer_copy(struct bt_field *src, struct bt_field *dst)
{
	struct bt_field_integer *integer_src, *integer_dst;

	BT_LOGD("Copying integer field: src-field-addr=%p, dst-field-addr=%p",
		src, dst);
	integer_src = container_of(src, struct bt_field_integer, parent);
	integer_dst = container_of(dst, struct bt_field_integer, parent);
	integer_dst->payload = integer_src->payload;
	BT_LOGD_STR("Copied integer field.");
	return 0;
}

static
int bt_field_enumeration_copy_recursive(struct bt_field *src,
		struct bt_field *dst)
{
	int ret = 0;
	struct bt_field_enumeration *enum_src, *enum_dst;

	BT_LOGD("Copying enumeration field: src-field-addr=%p, dst-field-addr=%p",
		src, dst);
	enum_src = container_of(src, struct bt_field_enumeration, parent);
	enum_dst = container_of(dst, struct bt_field_enumeration, parent);

	if (enum_src->payload) {
		BT_LOGD_STR("Copying enumeration field's payload field.");
		enum_dst->payload = bt_field_copy(enum_src->payload);
		if (!enum_dst->payload) {
			BT_LOGE_STR("Cannot copy enumeration field's payload field.");
			ret = -1;
			goto end;
		}
	}

	BT_LOGD_STR("Copied enumeration field.");
end:
	return ret;
}

static
int bt_field_floating_point_copy(struct bt_field *src, struct bt_field *dst)
{
	struct bt_field_floating_point *float_src, *float_dst;

	BT_LOGD("Copying floating point number field: src-field-addr=%p, dst-field-addr=%p",
		src, dst);
	float_src = container_of(src, struct bt_field_floating_point,
		parent);
	float_dst = container_of(dst, struct bt_field_floating_point,
		parent);
	float_dst->payload = float_src->payload;
	BT_LOGD_STR("Copied floating point number field.");
	return 0;
}

static
int bt_field_structure_copy_recursive(struct bt_field *src,
		struct bt_field *dst)
{
	int ret = 0;
	int64_t i;
	struct bt_field_structure *struct_src, *struct_dst;

	BT_LOGD("Copying structure field: src-field-addr=%p, dst-field-addr=%p",
		src, dst);
	struct_src = container_of(src, struct bt_field_structure, parent);
	struct_dst = container_of(dst, struct bt_field_structure, parent);

	g_ptr_array_set_size(struct_dst->fields, struct_src->fields->len);

	for (i = 0; i < struct_src->fields->len; i++) {
		struct bt_field *field =
			g_ptr_array_index(struct_src->fields, i);
		struct bt_field *field_copy = NULL;

		if (field) {
			BT_LOGD("Copying structure field's field: src-field-addr=%p"
				"index=%" PRId64, field, i);
			field_copy = bt_field_copy(field);
			if (!field_copy) {
				BT_LOGE("Cannot copy structure field's field: "
					"src-field-addr=%p, index=%" PRId64,
					field, i);
				ret = -1;
				goto end;
			}
		}

		BT_MOVE(g_ptr_array_index(struct_dst->fields, i), field_copy);
	}

	BT_LOGD_STR("Copied structure field.");

end:
	return ret;
}

static
int bt_field_variant_copy_recursive(struct bt_field *src,
		struct bt_field *dst)
{
	int ret = 0;
	struct bt_field_variant *variant_src, *variant_dst;

	BT_LOGD("Copying variant field: src-field-addr=%p, dst-field-addr=%p",
		src, dst);
	variant_src = container_of(src, struct bt_field_variant, parent);
	variant_dst = container_of(dst, struct bt_field_variant, parent);

	if (variant_src->tag) {
		BT_LOGD_STR("Copying variant field's tag field.");
		variant_dst->tag = bt_field_copy(variant_src->tag);
		if (!variant_dst->tag) {
			BT_LOGE_STR("Cannot copy variant field's tag field.");
			ret = -1;
			goto end;
		}
	}
	if (variant_src->payload) {
		BT_LOGD_STR("Copying variant field's payload field.");
		variant_dst->payload = bt_field_copy(variant_src->payload);
		if (!variant_dst->payload) {
			BT_LOGE_STR("Cannot copy variant field's payload field.");
			ret = -1;
			goto end;
		}
	}

	BT_LOGD_STR("Copied variant field.");

end:
	return ret;
}

static
int bt_field_array_copy_recursive(struct bt_field *src,
		struct bt_field *dst)
{
	int ret = 0;
	int64_t i;
	struct bt_field_array *array_src, *array_dst;

	BT_LOGD("Copying array field: src-field-addr=%p, dst-field-addr=%p",
		src, dst);
	array_src = container_of(src, struct bt_field_array, parent);
	array_dst = container_of(dst, struct bt_field_array, parent);

	g_ptr_array_set_size(array_dst->elements, array_src->elements->len);
	for (i = 0; i < array_src->elements->len; i++) {
		struct bt_field *field =
			g_ptr_array_index(array_src->elements, i);
		struct bt_field *field_copy = NULL;

		if (field) {
			BT_LOGD("Copying array field's element field: field-addr=%p, "
				"index=%" PRId64, field, i);
			field_copy = bt_field_copy(field);
			if (!field_copy) {
				BT_LOGE("Cannot copy array field's element field: "
					"src-field-addr=%p, index=%" PRId64,
					field, i);
				ret = -1;
				goto end;
			}
		}

		g_ptr_array_index(array_dst->elements, i) = field_copy;
	}

	BT_LOGD_STR("Copied array field.");

end:
	return ret;
}

static
int bt_field_sequence_copy_recursive(struct bt_field *src,
		struct bt_field *dst)
{
	int ret = 0;
	int64_t i;
	struct bt_field_sequence *sequence_src, *sequence_dst;
	struct bt_field *src_length;
	struct bt_field *dst_length;

	BT_LOGD("Copying sequence field: src-field-addr=%p, dst-field-addr=%p",
		src, dst);
	sequence_src = container_of(src, struct bt_field_sequence, parent);
	sequence_dst = container_of(dst, struct bt_field_sequence, parent);

	src_length = bt_field_sequence_get_length(src);
	if (!src_length) {
		/* no length set yet: keep destination sequence empty */
		goto end;
	}

	/* copy source length */
	BT_LOGD_STR("Copying sequence field's length field.");
	dst_length = bt_field_copy(src_length);
	BT_PUT(src_length);
	if (!dst_length) {
		BT_LOGE_STR("Cannot copy sequence field's length field.");
		ret = -1;
		goto end;
	}

	/* this will initialize the destination sequence's internal array */
	ret = bt_field_sequence_set_length(dst, dst_length);
	bt_put(dst_length);
	if (ret) {
		BT_LOGE("Cannot set sequence field copy's length field: "
			"dst-length-field-addr=%p", dst_length);
		ret = -1;
		goto end;
	}

	BT_ASSERT(sequence_dst->elements->len == sequence_src->elements->len);

	for (i = 0; i < sequence_src->elements->len; i++) {
		struct bt_field *field =
			g_ptr_array_index(sequence_src->elements, i);
		struct bt_field *field_copy = NULL;

		if (field) {
			BT_LOGD("Copying sequence field's element field: field-addr=%p, "
				"index=%" PRId64, field, i);
			field_copy = bt_field_copy(field);
			if (!field_copy) {
				BT_LOGE("Cannot copy sequence field's element field: "
					"src-field-addr=%p, index=%" PRId64,
					field, i);
				ret = -1;
				goto end;
			}
		}

		g_ptr_array_index(sequence_dst->elements, i) = field_copy;
	}

	BT_LOGD_STR("Copied sequence field.");

end:
	return ret;
}

static
int bt_field_string_copy_recursive(struct bt_field *src,
		struct bt_field *dst)
{
	int ret = 0;
	struct bt_field_string *string_src, *string_dst;

	BT_LOGD("Copying string field: src-field-addr=%p, dst-field-addr=%p",
		src, dst);
	string_src = container_of(src, struct bt_field_string, parent);
	string_dst = container_of(dst, struct bt_field_string, parent);

	if (string_src->payload) {
		string_dst->payload = g_string_new(string_src->payload->str);
		if (!string_dst->payload) {
			BT_LOGE_STR("Failed to allocate a GString.");
			ret = -1;
			goto end;
		}
	}

	BT_LOGD_STR("Copied string field.");

end:
	return ret;
}

static
int increase_packet_size(struct bt_stream_pos *pos)
{
	int ret;

	BT_ASSERT(pos);
	BT_LOGV("Increasing packet size: pos-offset=%" PRId64 ", "
		"cur-packet-size=%" PRIu64,
		pos->offset, pos->packet_size);
	ret = munmap_align(pos->base_mma);
	if (ret) {
		BT_LOGE_ERRNO("Failed to perform an aligned memory unmapping",
			": ret=%d", ret);
		goto end;
	}

	pos->packet_size += PACKET_LEN_INCREMENT;
	do {
		ret = bt_posix_fallocate(pos->fd, pos->mmap_offset,
			pos->packet_size / CHAR_BIT);
	} while (ret == EINTR);
	if (ret) {
		BT_LOGE_ERRNO("Failed to preallocate memory space",
			": ret=%d", ret);
		errno = EINTR;
		ret = -1;
		goto end;
	}

	pos->base_mma = mmap_align(pos->packet_size / CHAR_BIT, pos->prot,
		pos->flags, pos->fd, pos->mmap_offset);
	if (pos->base_mma == MAP_FAILED) {
		BT_LOGE_ERRNO("Failed to perform an aligned memory mapping",
			": ret=%d", ret);
		ret = -1;
	}

	BT_LOGV("Increased packet size: pos-offset=%" PRId64 ", "
		"new-packet-size=%" PRIu64,
		pos->offset, pos->packet_size);
	BT_ASSERT(pos->packet_size % 8 == 0);

end:
	return ret;
}

static
void generic_field_freeze(struct bt_field *field)
{
	field->frozen = true;
}

static
void bt_field_enumeration_freeze_recursive(struct bt_field *field)
{
	struct bt_field_enumeration *enum_field =
		container_of(field, struct bt_field_enumeration, parent);

	BT_LOGD("Freezing enumeration field object: addr=%p", field);
	BT_LOGD("Freezing enumeration field object's contained payload field: payload-field-addr=%p", enum_field->payload);
	bt_field_freeze_recursive(enum_field->payload);
	generic_field_freeze(field);
}

static
void bt_field_structure_freeze_recursive(struct bt_field *field)
{
	int64_t i;
	struct bt_field_structure *structure_field =
		container_of(field, struct bt_field_structure, parent);

	BT_LOGD("Freezing structure field object: addr=%p", field);

	for (i = 0; i < structure_field->fields->len; i++) {
		struct bt_field *field =
			g_ptr_array_index(structure_field->fields, i);

		BT_LOGD("Freezing structure field's field: field-addr=%p, index=%" PRId64,
			field, i);
		bt_field_freeze_recursive(field);
	}

	generic_field_freeze(field);
}

static
void bt_field_variant_freeze_recursive(struct bt_field *field)
{
	struct bt_field_variant *variant_field =
		container_of(field, struct bt_field_variant, parent);

	BT_LOGD("Freezing variant field object: addr=%p", field);
	BT_LOGD("Freezing variant field object's tag field: tag-field-addr=%p", variant_field->tag);
	bt_field_freeze_recursive(variant_field->tag);
	BT_LOGD("Freezing variant field object's payload field: payload-field-addr=%p", variant_field->payload);
	bt_field_freeze_recursive(variant_field->payload);
	generic_field_freeze(field);
}

static
void bt_field_array_freeze_recursive(struct bt_field *field)
{
	int64_t i;
	struct bt_field_array *array_field =
		container_of(field, struct bt_field_array, parent);

	BT_LOGD("Freezing array field object: addr=%p", field);

	for (i = 0; i < array_field->elements->len; i++) {
		struct bt_field *elem_field =
			g_ptr_array_index(array_field->elements, i);

		BT_LOGD("Freezing array field object's element field: "
			"element-field-addr=%p, index=%" PRId64,
			elem_field, i);
		bt_field_freeze_recursive(elem_field);
	}

	generic_field_freeze(field);
}

static
void bt_field_sequence_freeze_recursive(struct bt_field *field)
{
	int64_t i;
	struct bt_field_sequence *sequence_field =
		container_of(field, struct bt_field_sequence, parent);

	BT_LOGD("Freezing sequence field object: addr=%p", field);
	BT_LOGD("Freezing sequence field object's length field: length-field-addr=%p",
		sequence_field->length);
	bt_field_freeze_recursive(sequence_field->length);

	for (i = 0; i < sequence_field->elements->len; i++) {
		struct bt_field *elem_field =
			g_ptr_array_index(sequence_field->elements, i);

		BT_LOGD("Freezing sequence field object's element field: "
			"element-field-addr=%p, index=%" PRId64,
			elem_field, i);
		bt_field_freeze_recursive(elem_field);
	}

	generic_field_freeze(field);
}

BT_HIDDEN
void _bt_field_freeze_recursive(struct bt_field *field)
{
	enum bt_field_type_id type_id;

	if (!field) {
		goto end;
	}

	if (field->frozen) {
		goto end;
	}

	BT_LOGD("Freezing field object: addr=%p", field);
	BT_ASSERT(field_type_has_known_id(field->type));
	type_id = bt_field_get_type_id(field);
	field_freeze_funcs[type_id](field);
end:
	return;
}

static
bt_bool bt_field_generic_is_set(struct bt_field *field)
{
	return field && field->payload_set;
}

static
bt_bool bt_field_enumeration_is_set_recursive(struct bt_field *field)
{
	bt_bool is_set = BT_FALSE;
	struct bt_field_enumeration *enumeration;

	BT_ASSERT(field);
	enumeration = container_of(field, struct bt_field_enumeration,
			parent);
	if (!enumeration->payload) {
		goto end;
	}

	is_set = bt_field_is_set_recursive(enumeration->payload);
end:
	return is_set;
}

static
bt_bool bt_field_structure_is_set_recursive(struct bt_field *field)
{
	bt_bool is_set = BT_FALSE;
	size_t i;
	struct bt_field_structure *structure;

	BT_ASSERT(field);
	structure = container_of(field, struct bt_field_structure, parent);
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
	struct bt_field_variant *variant;

	BT_ASSERT(field);
	variant = container_of(field, struct bt_field_variant, parent);
	return bt_field_is_set_recursive(variant->payload);
}

static
bt_bool bt_field_array_is_set_recursive(struct bt_field *field)
{
	size_t i;
	bt_bool is_set = BT_FALSE;
	struct bt_field_array *array;

	BT_ASSERT(field);
	array = container_of(field, struct bt_field_array, parent);
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
	struct bt_field_sequence *sequence;

	BT_ASSERT(field);
	sequence = container_of(field, struct bt_field_sequence, parent);
	if (!sequence->elements) {
		goto end;
	}

	for (i = 0; i < sequence->elements->len; i++) {
		is_set = bt_field_is_set_recursive(sequence->elements->pdata[i]);
		if (!is_set) {
			goto end;
		}
	}
end:
	return is_set;
}
