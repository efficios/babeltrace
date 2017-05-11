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

#include <babeltrace/ctf-ir/fields-internal.h>
#include <babeltrace/ctf-ir/field-types-internal.h>
#include <babeltrace/ctf-writer/serialize-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/compat/fcntl-internal.h>
#include <babeltrace/align-internal.h>

#define PACKET_LEN_INCREMENT	(getpagesize() * 8 * CHAR_BIT)

static
struct bt_ctf_field *bt_ctf_field_integer_create(struct bt_ctf_field_type *);
static
struct bt_ctf_field *bt_ctf_field_enumeration_create(
		struct bt_ctf_field_type *);
static
struct bt_ctf_field *bt_ctf_field_floating_point_create(
		struct bt_ctf_field_type *);
static
struct bt_ctf_field *bt_ctf_field_structure_create(
		struct bt_ctf_field_type *);
static
struct bt_ctf_field *bt_ctf_field_variant_create(
		struct bt_ctf_field_type *);
static
struct bt_ctf_field *bt_ctf_field_array_create(
		struct bt_ctf_field_type *);
static
struct bt_ctf_field *bt_ctf_field_sequence_create(
		struct bt_ctf_field_type *);
static
struct bt_ctf_field *bt_ctf_field_string_create(struct bt_ctf_field_type *);

static
void bt_ctf_field_destroy(struct bt_object *);
static
void bt_ctf_field_integer_destroy(struct bt_ctf_field *);
static
void bt_ctf_field_enumeration_destroy(struct bt_ctf_field *);
static
void bt_ctf_field_floating_point_destroy(struct bt_ctf_field *);
static
void bt_ctf_field_structure_destroy(struct bt_ctf_field *);
static
void bt_ctf_field_variant_destroy(struct bt_ctf_field *);
static
void bt_ctf_field_array_destroy(struct bt_ctf_field *);
static
void bt_ctf_field_sequence_destroy(struct bt_ctf_field *);
static
void bt_ctf_field_string_destroy(struct bt_ctf_field *);

static
int bt_ctf_field_generic_validate(struct bt_ctf_field *);
static
int bt_ctf_field_structure_validate(struct bt_ctf_field *);
static
int bt_ctf_field_variant_validate(struct bt_ctf_field *);
static
int bt_ctf_field_enumeration_validate(struct bt_ctf_field *);
static
int bt_ctf_field_array_validate(struct bt_ctf_field *);
static
int bt_ctf_field_sequence_validate(struct bt_ctf_field *);

static
int bt_ctf_field_generic_reset(struct bt_ctf_field *);
static
int bt_ctf_field_structure_reset(struct bt_ctf_field *);
static
int bt_ctf_field_variant_reset(struct bt_ctf_field *);
static
int bt_ctf_field_enumeration_reset(struct bt_ctf_field *);
static
int bt_ctf_field_array_reset(struct bt_ctf_field *);
static
int bt_ctf_field_sequence_reset(struct bt_ctf_field *);
static
int bt_ctf_field_string_reset(struct bt_ctf_field *);

static
int bt_ctf_field_integer_serialize(struct bt_ctf_field *,
		struct bt_ctf_stream_pos *, enum bt_ctf_byte_order);
static
int bt_ctf_field_enumeration_serialize(struct bt_ctf_field *,
		struct bt_ctf_stream_pos *, enum bt_ctf_byte_order);
static
int bt_ctf_field_floating_point_serialize(struct bt_ctf_field *,
		struct bt_ctf_stream_pos *, enum bt_ctf_byte_order);
static
int bt_ctf_field_structure_serialize(struct bt_ctf_field *,
		struct bt_ctf_stream_pos *, enum bt_ctf_byte_order);
static
int bt_ctf_field_variant_serialize(struct bt_ctf_field *,
		struct bt_ctf_stream_pos *, enum bt_ctf_byte_order);
static
int bt_ctf_field_array_serialize(struct bt_ctf_field *,
		struct bt_ctf_stream_pos *, enum bt_ctf_byte_order);
static
int bt_ctf_field_sequence_serialize(struct bt_ctf_field *,
		struct bt_ctf_stream_pos *, enum bt_ctf_byte_order);
static
int bt_ctf_field_string_serialize(struct bt_ctf_field *,
		struct bt_ctf_stream_pos *, enum bt_ctf_byte_order);

static
int bt_ctf_field_integer_copy(struct bt_ctf_field *, struct bt_ctf_field *);
static
int bt_ctf_field_enumeration_copy(struct bt_ctf_field *, struct bt_ctf_field *);
static
int bt_ctf_field_floating_point_copy(struct bt_ctf_field *,
		struct bt_ctf_field *);
static
int bt_ctf_field_structure_copy(struct bt_ctf_field *, struct bt_ctf_field *);
static
int bt_ctf_field_variant_copy(struct bt_ctf_field *, struct bt_ctf_field *);
static
int bt_ctf_field_array_copy(struct bt_ctf_field *, struct bt_ctf_field *);
static
int bt_ctf_field_sequence_copy(struct bt_ctf_field *, struct bt_ctf_field *);
static
int bt_ctf_field_string_copy(struct bt_ctf_field *, struct bt_ctf_field *);

static
void generic_field_freeze(struct bt_ctf_field *);
static
void bt_ctf_field_enumeration_freeze(struct bt_ctf_field *);
static
void bt_ctf_field_structure_freeze(struct bt_ctf_field *);
static
void bt_ctf_field_variant_freeze(struct bt_ctf_field *);
static
void bt_ctf_field_array_freeze(struct bt_ctf_field *);
static
void bt_ctf_field_sequence_freeze(struct bt_ctf_field *);

static
bt_bool bt_ctf_field_generic_is_set(struct bt_ctf_field *);
static
bt_bool bt_ctf_field_structure_is_set(struct bt_ctf_field *);
static
bt_bool bt_ctf_field_variant_is_set(struct bt_ctf_field *);
static
bt_bool bt_ctf_field_enumeration_is_set(struct bt_ctf_field *);
static
bt_bool bt_ctf_field_array_is_set(struct bt_ctf_field *);
static
bt_bool bt_ctf_field_sequence_is_set(struct bt_ctf_field *);

static
int increase_packet_size(struct bt_ctf_stream_pos *pos);

static
struct bt_ctf_field *(* const field_create_funcs[])(
		struct bt_ctf_field_type *) = {
	[BT_CTF_FIELD_TYPE_ID_INTEGER] = bt_ctf_field_integer_create,
	[BT_CTF_FIELD_TYPE_ID_ENUM] = bt_ctf_field_enumeration_create,
	[BT_CTF_FIELD_TYPE_ID_FLOAT] =
		bt_ctf_field_floating_point_create,
	[BT_CTF_FIELD_TYPE_ID_STRUCT] = bt_ctf_field_structure_create,
	[BT_CTF_FIELD_TYPE_ID_VARIANT] = bt_ctf_field_variant_create,
	[BT_CTF_FIELD_TYPE_ID_ARRAY] = bt_ctf_field_array_create,
	[BT_CTF_FIELD_TYPE_ID_SEQUENCE] = bt_ctf_field_sequence_create,
	[BT_CTF_FIELD_TYPE_ID_STRING] = bt_ctf_field_string_create,
};

static
void (* const field_destroy_funcs[])(struct bt_ctf_field *) = {
	[BT_CTF_FIELD_TYPE_ID_INTEGER] = bt_ctf_field_integer_destroy,
	[BT_CTF_FIELD_TYPE_ID_ENUM] = bt_ctf_field_enumeration_destroy,
	[BT_CTF_FIELD_TYPE_ID_FLOAT] =
		bt_ctf_field_floating_point_destroy,
	[BT_CTF_FIELD_TYPE_ID_STRUCT] = bt_ctf_field_structure_destroy,
	[BT_CTF_FIELD_TYPE_ID_VARIANT] = bt_ctf_field_variant_destroy,
	[BT_CTF_FIELD_TYPE_ID_ARRAY] = bt_ctf_field_array_destroy,
	[BT_CTF_FIELD_TYPE_ID_SEQUENCE] = bt_ctf_field_sequence_destroy,
	[BT_CTF_FIELD_TYPE_ID_STRING] = bt_ctf_field_string_destroy,
};

static
int (* const field_validate_funcs[])(struct bt_ctf_field *) = {
	[BT_CTF_FIELD_TYPE_ID_INTEGER] = bt_ctf_field_generic_validate,
	[BT_CTF_FIELD_TYPE_ID_ENUM] = bt_ctf_field_enumeration_validate,
	[BT_CTF_FIELD_TYPE_ID_FLOAT] = bt_ctf_field_generic_validate,
	[BT_CTF_FIELD_TYPE_ID_STRUCT] = bt_ctf_field_structure_validate,
	[BT_CTF_FIELD_TYPE_ID_VARIANT] = bt_ctf_field_variant_validate,
	[BT_CTF_FIELD_TYPE_ID_ARRAY] = bt_ctf_field_array_validate,
	[BT_CTF_FIELD_TYPE_ID_SEQUENCE] = bt_ctf_field_sequence_validate,
	[BT_CTF_FIELD_TYPE_ID_STRING] = bt_ctf_field_generic_validate,
};

static
int (* const field_reset_funcs[])(struct bt_ctf_field *) = {
	[BT_CTF_FIELD_TYPE_ID_INTEGER] = bt_ctf_field_generic_reset,
	[BT_CTF_FIELD_TYPE_ID_ENUM] = bt_ctf_field_enumeration_reset,
	[BT_CTF_FIELD_TYPE_ID_FLOAT] = bt_ctf_field_generic_reset,
	[BT_CTF_FIELD_TYPE_ID_STRUCT] = bt_ctf_field_structure_reset,
	[BT_CTF_FIELD_TYPE_ID_VARIANT] = bt_ctf_field_variant_reset,
	[BT_CTF_FIELD_TYPE_ID_ARRAY] = bt_ctf_field_array_reset,
	[BT_CTF_FIELD_TYPE_ID_SEQUENCE] = bt_ctf_field_sequence_reset,
	[BT_CTF_FIELD_TYPE_ID_STRING] = bt_ctf_field_string_reset,
};

static
int (* const field_serialize_funcs[])(struct bt_ctf_field *,
		struct bt_ctf_stream_pos *, enum bt_ctf_byte_order) = {
	[BT_CTF_FIELD_TYPE_ID_INTEGER] = bt_ctf_field_integer_serialize,
	[BT_CTF_FIELD_TYPE_ID_ENUM] = bt_ctf_field_enumeration_serialize,
	[BT_CTF_FIELD_TYPE_ID_FLOAT] =
		bt_ctf_field_floating_point_serialize,
	[BT_CTF_FIELD_TYPE_ID_STRUCT] = bt_ctf_field_structure_serialize,
	[BT_CTF_FIELD_TYPE_ID_VARIANT] = bt_ctf_field_variant_serialize,
	[BT_CTF_FIELD_TYPE_ID_ARRAY] = bt_ctf_field_array_serialize,
	[BT_CTF_FIELD_TYPE_ID_SEQUENCE] = bt_ctf_field_sequence_serialize,
	[BT_CTF_FIELD_TYPE_ID_STRING] = bt_ctf_field_string_serialize,
};

static
int (* const field_copy_funcs[])(struct bt_ctf_field *,
		struct bt_ctf_field *) = {
	[BT_CTF_FIELD_TYPE_ID_INTEGER] = bt_ctf_field_integer_copy,
	[BT_CTF_FIELD_TYPE_ID_ENUM] = bt_ctf_field_enumeration_copy,
	[BT_CTF_FIELD_TYPE_ID_FLOAT] = bt_ctf_field_floating_point_copy,
	[BT_CTF_FIELD_TYPE_ID_STRUCT] = bt_ctf_field_structure_copy,
	[BT_CTF_FIELD_TYPE_ID_VARIANT] = bt_ctf_field_variant_copy,
	[BT_CTF_FIELD_TYPE_ID_ARRAY] = bt_ctf_field_array_copy,
	[BT_CTF_FIELD_TYPE_ID_SEQUENCE] = bt_ctf_field_sequence_copy,
	[BT_CTF_FIELD_TYPE_ID_STRING] = bt_ctf_field_string_copy,
};

static
void (* const field_freeze_funcs[])(struct bt_ctf_field *) = {
	[BT_CTF_FIELD_TYPE_ID_INTEGER] = generic_field_freeze,
	[BT_CTF_FIELD_TYPE_ID_FLOAT] = generic_field_freeze,
	[BT_CTF_FIELD_TYPE_ID_STRING] = generic_field_freeze,
	[BT_CTF_FIELD_TYPE_ID_ENUM] = bt_ctf_field_enumeration_freeze,
	[BT_CTF_FIELD_TYPE_ID_STRUCT] = bt_ctf_field_structure_freeze,
	[BT_CTF_FIELD_TYPE_ID_VARIANT] = bt_ctf_field_variant_freeze,
	[BT_CTF_FIELD_TYPE_ID_ARRAY] = bt_ctf_field_array_freeze,
	[BT_CTF_FIELD_TYPE_ID_SEQUENCE] = bt_ctf_field_sequence_freeze,
};

static
bt_bool (* const field_is_set_funcs[])(struct bt_ctf_field *) = {
	[BT_CTF_FIELD_TYPE_ID_INTEGER] = bt_ctf_field_generic_is_set,
	[BT_CTF_FIELD_TYPE_ID_ENUM] = bt_ctf_field_enumeration_is_set,
	[BT_CTF_FIELD_TYPE_ID_FLOAT] = bt_ctf_field_generic_is_set,
	[BT_CTF_FIELD_TYPE_ID_STRUCT] = bt_ctf_field_structure_is_set,
	[BT_CTF_FIELD_TYPE_ID_VARIANT] = bt_ctf_field_variant_is_set,
	[BT_CTF_FIELD_TYPE_ID_ARRAY] = bt_ctf_field_array_is_set,
	[BT_CTF_FIELD_TYPE_ID_SEQUENCE] = bt_ctf_field_sequence_is_set,
	[BT_CTF_FIELD_TYPE_ID_STRING] = bt_ctf_field_generic_is_set,
};

struct bt_ctf_field *bt_ctf_field_create(struct bt_ctf_field_type *type)
{
	struct bt_ctf_field *field = NULL;
	enum bt_ctf_field_type_id type_id;
	int ret;

	if (!type) {
		goto error;
	}

	type_id = bt_ctf_field_type_get_type_id(type);
	if (type_id <= BT_CTF_FIELD_TYPE_ID_UNKNOWN ||
			type_id >= BT_CTF_NR_TYPE_IDS) {
		goto error;
	}

	/* Field class MUST be valid */
	ret = bt_ctf_field_type_validate(type);

	if (ret) {
		/* Invalid */
		goto error;
	}

	field = field_create_funcs[type_id](type);
	if (!field) {
		goto error;
	}

	/* The type's declaration can't change after this point */
	bt_ctf_field_type_freeze(type);
	bt_get(type);
	bt_object_init(field, bt_ctf_field_destroy);
	field->type = type;
error:
	return field;
}

void bt_ctf_field_get(struct bt_ctf_field *field)
{
	bt_get(field);
}

void bt_ctf_field_put(struct bt_ctf_field *field)
{
	bt_put(field);
}

struct bt_ctf_field_type *bt_ctf_field_get_type(struct bt_ctf_field *field)
{
	struct bt_ctf_field_type *ret = NULL;

	if (!field) {
		goto end;
	}

	ret = field->type;
	bt_get(ret);
end:
	return ret;
}

enum bt_ctf_field_type_id bt_ctf_field_get_type_id(struct bt_ctf_field *field)
{
	enum bt_ctf_field_type_id ret = BT_CTF_FIELD_TYPE_ID_UNKNOWN;

	if (!field) {
		goto end;
	}

	ret = bt_ctf_field_type_get_type_id(field->type);
end:
	return ret;
}

int bt_ctf_field_is_integer(struct bt_ctf_field *field)
{
	return bt_ctf_field_get_type_id(field) == BT_CTF_FIELD_TYPE_ID_INTEGER;
}

int bt_ctf_field_is_floating_point(struct bt_ctf_field *field)
{
	return bt_ctf_field_get_type_id(field) == BT_CTF_FIELD_TYPE_ID_FLOAT;
}

int bt_ctf_field_is_enumeration(struct bt_ctf_field *field)
{
	return bt_ctf_field_get_type_id(field) == BT_CTF_FIELD_TYPE_ID_ENUM;
}

int bt_ctf_field_is_string(struct bt_ctf_field *field)
{
	return bt_ctf_field_get_type_id(field) == BT_CTF_FIELD_TYPE_ID_STRING;
}

int bt_ctf_field_is_structure(struct bt_ctf_field *field)
{
	return bt_ctf_field_get_type_id(field) == BT_CTF_FIELD_TYPE_ID_STRUCT;
}

int bt_ctf_field_is_array(struct bt_ctf_field *field)
{
	return bt_ctf_field_get_type_id(field) == BT_CTF_FIELD_TYPE_ID_ARRAY;
}

int bt_ctf_field_is_sequence(struct bt_ctf_field *field)
{
	return bt_ctf_field_get_type_id(field) == BT_CTF_FIELD_TYPE_ID_SEQUENCE;
}

int bt_ctf_field_is_variant(struct bt_ctf_field *field)
{
	return bt_ctf_field_get_type_id(field) == BT_CTF_FIELD_TYPE_ID_VARIANT;
}

struct bt_ctf_field *bt_ctf_field_sequence_get_length(
		struct bt_ctf_field *field)
{
	struct bt_ctf_field *ret = NULL;
	struct bt_ctf_field_sequence *sequence;

	if (!field) {
		goto end;
	}

	if (bt_ctf_field_type_get_type_id(field->type) !=
		BT_CTF_FIELD_TYPE_ID_SEQUENCE) {
		goto end;
	}

	sequence = container_of(field, struct bt_ctf_field_sequence, parent);
	ret = sequence->length;
	bt_get(ret);
end:
	return ret;
}

int bt_ctf_field_sequence_set_length(struct bt_ctf_field *field,
		struct bt_ctf_field *length_field)
{
	int ret = 0;
	struct bt_ctf_field_type_integer *length_type;
	struct bt_ctf_field_integer *length;
	struct bt_ctf_field_sequence *sequence;
	uint64_t sequence_length;

	if (!field || !length_field || field->frozen) {
		ret = -1;
		goto end;
	}
	if (bt_ctf_field_type_get_type_id(length_field->type) !=
		BT_CTF_FIELD_TYPE_ID_INTEGER) {
		ret = -1;
		goto end;
	}

	length_type = container_of(length_field->type,
		struct bt_ctf_field_type_integer, parent);
	/* The length field must be unsigned */
	if (length_type->is_signed) {
		ret = -1;
		goto end;
	}

	length = container_of(length_field, struct bt_ctf_field_integer,
		parent);
	sequence_length = length->payload.unsignd;
	sequence = container_of(field, struct bt_ctf_field_sequence, parent);
	if (sequence->elements) {
		g_ptr_array_free(sequence->elements, TRUE);
		bt_put(sequence->length);
	}

	sequence->elements = g_ptr_array_sized_new((size_t)sequence_length);
	if (!sequence->elements) {
		ret = -1;
		goto end;
	}

	g_ptr_array_set_free_func(sequence->elements,
		(GDestroyNotify) bt_put);
	g_ptr_array_set_size(sequence->elements, (size_t) sequence_length);
	bt_get(length_field);
	sequence->length = length_field;
end:
	return ret;
}

struct bt_ctf_field *bt_ctf_field_structure_get_field_by_name(
		struct bt_ctf_field *field, const char *name)
{
	struct bt_ctf_field *new_field = NULL;
	GQuark field_quark;
	struct bt_ctf_field_structure *structure;
	struct bt_ctf_field_type *field_type = NULL;
	size_t index;

	if (!field || !name ||
		bt_ctf_field_type_get_type_id(field->type) !=
			BT_CTF_FIELD_TYPE_ID_STRUCT) {
		goto error;
	}

	field_quark = g_quark_from_string(name);
	structure = container_of(field, struct bt_ctf_field_structure, parent);
	field_type =
		bt_ctf_field_type_structure_get_field_type_by_name(field->type,
		name);
	if (!g_hash_table_lookup_extended(structure->field_name_to_index,
		GUINT_TO_POINTER(field_quark), NULL, (gpointer *)&index)) {
		goto error;
	}

	if (structure->fields->pdata[index]) {
		new_field = structure->fields->pdata[index];
		goto end;
	}

	/* We don't want to modify this field if it's frozen */
	if (field->frozen) {
		goto end;
	}

	new_field = bt_ctf_field_create(field_type);
	if (!new_field) {
		goto error;
	}

	structure->fields->pdata[index] = new_field;
end:
	bt_get(new_field);
error:
	if (field_type) {
		bt_put(field_type);
	}
	return new_field;
}

struct bt_ctf_field *bt_ctf_field_structure_get_field_by_index(
		struct bt_ctf_field *field, uint64_t index)
{
	int ret;
	const char *field_name;
	struct bt_ctf_field_structure *structure;
	struct bt_ctf_field_type *structure_type;
	struct bt_ctf_field_type *field_type = NULL;
	struct bt_ctf_field *ret_field = NULL;

	if (!field ||
		bt_ctf_field_type_get_type_id(field->type) !=
		BT_CTF_FIELD_TYPE_ID_STRUCT) {
		goto end;
	}

	structure = container_of(field, struct bt_ctf_field_structure, parent);
	if (index >= structure->fields->len) {
		goto error;
	}

	ret_field = structure->fields->pdata[index];
	if (ret_field) {
		goto end;
	}

	/* We don't want to modify this field if it's frozen */
	if (field->frozen) {
		goto end;
	}

	/* Field has not been instanciated yet, create it */
	structure_type = bt_ctf_field_get_type(field);
	if (!structure_type) {
		goto error;
	}

	ret = bt_ctf_field_type_structure_get_field(structure_type,
		&field_name, &field_type, index);
	bt_put(structure_type);
	if (ret) {
		goto error;
	}

	ret_field = bt_ctf_field_create(field_type);
	if (!ret_field) {
		goto error;
	}

	structure->fields->pdata[index] = ret_field;
end:
	bt_get(ret_field);
error:
	bt_put(field_type);
	return ret_field;
}

int bt_ctf_field_structure_set_field(struct bt_ctf_field *field,
		const char *name, struct bt_ctf_field *value)
{
	int ret = 0;
	GQuark field_quark;
	struct bt_ctf_field_structure *structure;
	struct bt_ctf_field_type *expected_field_type = NULL;
	size_t index;

	if (!field || !name || !value || field->frozen ||
		bt_ctf_field_type_get_type_id(field->type) !=
			BT_CTF_FIELD_TYPE_ID_STRUCT) {
		ret = -1;
		goto end;
	}

	field_quark = g_quark_from_string(name);
	structure = container_of(field, struct bt_ctf_field_structure, parent);
	expected_field_type =
		bt_ctf_field_type_structure_get_field_type_by_name(field->type,
		name);

	if (bt_ctf_field_type_compare(expected_field_type, value->type)) {
		ret = -1;
		goto end;
	}

	if (!g_hash_table_lookup_extended(structure->field_name_to_index,
		GUINT_TO_POINTER(field_quark), NULL, (gpointer *) &index)) {
		goto end;
	}

	if (structure->fields->pdata[index]) {
		bt_put(structure->fields->pdata[index]);
	}

	structure->fields->pdata[index] = value;
	bt_get(value);
end:
	if (expected_field_type) {
		bt_put(expected_field_type);
	}
	return ret;
}

struct bt_ctf_field *bt_ctf_field_array_get_field(struct bt_ctf_field *field,
		uint64_t index)
{
	struct bt_ctf_field *new_field = NULL;
	struct bt_ctf_field_type *field_type = NULL;
	struct bt_ctf_field_array *array;

	if (!field || bt_ctf_field_type_get_type_id(field->type) !=
		BT_CTF_FIELD_TYPE_ID_ARRAY) {
		goto end;
	}

	array = container_of(field, struct bt_ctf_field_array, parent);
	if (index >= array->elements->len) {
		goto end;
	}

	field_type = bt_ctf_field_type_array_get_element_type(field->type);
	if (array->elements->pdata[(size_t)index]) {
		new_field = array->elements->pdata[(size_t)index];
		goto end;
	}

	/* We don't want to modify this field if it's frozen */
	if (field->frozen) {
		goto end;
	}

	new_field = bt_ctf_field_create(field_type);
	array->elements->pdata[(size_t)index] = new_field;
end:
	if (field_type) {
		bt_put(field_type);
	}
	if (new_field) {
		bt_get(new_field);
	}
	return new_field;
}

struct bt_ctf_field *bt_ctf_field_sequence_get_field(struct bt_ctf_field *field,
		uint64_t index)
{
	struct bt_ctf_field *new_field = NULL;
	struct bt_ctf_field_type *field_type = NULL;
	struct bt_ctf_field_sequence *sequence;

	if (!field || bt_ctf_field_type_get_type_id(field->type) !=
		BT_CTF_FIELD_TYPE_ID_SEQUENCE) {
		goto end;
	}

	sequence = container_of(field, struct bt_ctf_field_sequence, parent);
	if (!sequence->elements || index >= sequence->elements->len) {
		goto end;
	}

	field_type = bt_ctf_field_type_sequence_get_element_type(field->type);
	if (sequence->elements->pdata[(size_t) index]) {
		new_field = sequence->elements->pdata[(size_t) index];
		goto end;
	}

	/* We don't want to modify this field if it's frozen */
	if (field->frozen) {
		goto end;
	}

	new_field = bt_ctf_field_create(field_type);
	sequence->elements->pdata[(size_t) index] = new_field;
end:
	if (field_type) {
		bt_put(field_type);
	}
	if (new_field) {
		bt_get(new_field);
	}
	return new_field;
}

struct bt_ctf_field *bt_ctf_field_variant_get_field(struct bt_ctf_field *field,
		struct bt_ctf_field *tag_field)
{
	struct bt_ctf_field *new_field = NULL;
	struct bt_ctf_field_variant *variant;
	struct bt_ctf_field_type_variant *variant_type;
	struct bt_ctf_field_type *field_type;
	struct bt_ctf_field *tag_enum = NULL;
	struct bt_ctf_field_integer *tag_enum_integer;
	int64_t tag_enum_value;

	if (!field || !tag_field ||
		bt_ctf_field_type_get_type_id(field->type) !=
			BT_CTF_FIELD_TYPE_ID_VARIANT ||
		bt_ctf_field_type_get_type_id(tag_field->type) !=
			BT_CTF_FIELD_TYPE_ID_ENUM) {
		goto end;
	}

	variant = container_of(field, struct bt_ctf_field_variant, parent);
	variant_type = container_of(field->type,
		struct bt_ctf_field_type_variant, parent);
	tag_enum = bt_ctf_field_enumeration_get_container(tag_field);
	if (!tag_enum) {
		goto end;
	}

	tag_enum_integer = container_of(tag_enum, struct bt_ctf_field_integer,
		parent);

	if (bt_ctf_field_validate(tag_field) < 0) {
		goto end;
	}

	tag_enum_value = tag_enum_integer->payload.signd;

	/*
	 * If the variant currently has a tag and a payload, and if the
	 * requested tag value is the same as the current one, return
	 * the current payload instead of creating a fresh one.
	 */
	if (variant->tag && variant->payload) {
		struct bt_ctf_field *cur_tag_container = NULL;
		struct bt_ctf_field_integer *cur_tag_enum_integer;
		int64_t cur_tag_value;

		cur_tag_container =
			bt_ctf_field_enumeration_get_container(variant->tag);
		assert(cur_tag_container);
		cur_tag_enum_integer = container_of(cur_tag_container,
			struct bt_ctf_field_integer, parent);
		bt_put(cur_tag_container);
		cur_tag_value = cur_tag_enum_integer->payload.signd;

		if (cur_tag_value == tag_enum_value) {
			new_field = variant->payload;
			bt_get(new_field);
			goto end;
		}
	}

	/* We don't want to modify this field if it's frozen */
	if (field->frozen) {
		goto end;
	}

	field_type = bt_ctf_field_type_variant_get_field_type_signed(
		variant_type, tag_enum_value);
	if (!field_type) {
		goto end;
	}

	new_field = bt_ctf_field_create(field_type);
	if (!new_field) {
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

struct bt_ctf_field *bt_ctf_field_variant_get_current_field(
		struct bt_ctf_field *variant_field)
{
	struct bt_ctf_field *current_field = NULL;
	struct bt_ctf_field_variant *variant;

	if (!variant_field ||
		bt_ctf_field_type_get_type_id(variant_field->type) !=
			BT_CTF_FIELD_TYPE_ID_VARIANT) {
		goto end;
	}

	variant = container_of(variant_field, struct bt_ctf_field_variant,
		parent);

	if (variant->payload) {
		current_field = variant->payload;
		bt_get(current_field);
		goto end;
	}

end:
	return current_field;
}

struct bt_ctf_field *bt_ctf_field_variant_get_tag(
		struct bt_ctf_field *variant_field)
{
	struct bt_ctf_field *tag = NULL;
	struct bt_ctf_field_variant *variant;

	if (!variant_field ||
			bt_ctf_field_type_get_type_id(variant_field->type) !=
			BT_CTF_FIELD_TYPE_ID_VARIANT) {
		goto end;
	}

	variant = container_of(variant_field, struct bt_ctf_field_variant,
			parent);
	if (variant->tag) {
		tag = bt_get(variant->tag);
	}
end:
	return tag;
}

struct bt_ctf_field *bt_ctf_field_enumeration_get_container(
	struct bt_ctf_field *field)
{
	struct bt_ctf_field *container = NULL;
	struct bt_ctf_field_enumeration *enumeration;

	if (!field || bt_ctf_field_type_get_type_id(field->type) !=
		BT_CTF_FIELD_TYPE_ID_ENUM) {
		goto end;
	}

	enumeration = container_of(field, struct bt_ctf_field_enumeration,
		parent);
	if (!enumeration->payload) {
		/* We don't want to modify this field if it's frozen */
		if (field->frozen) {
			goto end;
		}

		struct bt_ctf_field_type_enumeration *enumeration_type =
			container_of(field->type,
			struct bt_ctf_field_type_enumeration, parent);
		enumeration->payload =
			bt_ctf_field_create(enumeration_type->container);
	}

	container = enumeration->payload;
	bt_get(container);
end:
	return container;
}

struct bt_ctf_field_type_enumeration_mapping_iterator *
bt_ctf_field_enumeration_get_mappings(struct bt_ctf_field *field)
{
	int ret;
	struct bt_ctf_field *container = NULL;
	struct bt_ctf_field_type *container_type = NULL;
	struct bt_ctf_field_type_integer *integer_type = NULL;
	struct bt_ctf_field_type_enumeration_mapping_iterator *iter = NULL;

	container = bt_ctf_field_enumeration_get_container(field);
	if (!container) {
		goto end;
	}

	container_type = bt_ctf_field_get_type(container);
	if (!container_type) {
		goto error_put_container;
	}

	integer_type = container_of(container_type,
		struct bt_ctf_field_type_integer, parent);

	if (!integer_type->is_signed) {
		uint64_t value;

		ret = bt_ctf_field_unsigned_integer_get_value(container,
		      &value);
		if (ret) {
			goto error_put_container_type;
		}
		iter = bt_ctf_field_type_enumeration_find_mappings_by_unsigned_value(
				field->type, value);
	} else {
		int64_t value;

		ret = bt_ctf_field_signed_integer_get_value(container,
		      &value);
		if (ret) {
			goto error_put_container_type;
		}
		iter = bt_ctf_field_type_enumeration_find_mappings_by_signed_value(
				field->type, value);
	}

error_put_container_type:
	bt_put(container_type);
error_put_container:
	bt_put(container);
end:
	return iter;
}

int bt_ctf_field_signed_integer_get_value(struct bt_ctf_field *field,
		int64_t *value)
{
	int ret = 0;
	struct bt_ctf_field_integer *integer;
	struct bt_ctf_field_type_integer *integer_type;

	if (!field || !value || !field->payload_set ||
		bt_ctf_field_type_get_type_id(field->type) !=
			BT_CTF_FIELD_TYPE_ID_INTEGER) {
		ret = -1;
		goto end;
	}

	integer_type = container_of(field->type,
		struct bt_ctf_field_type_integer, parent);
	if (!integer_type->is_signed) {
		ret = -1;
		goto end;
	}

	integer = container_of(field,
		struct bt_ctf_field_integer, parent);
	*value = integer->payload.signd;
end:
	return ret;
}

int bt_ctf_field_signed_integer_set_value(struct bt_ctf_field *field,
		int64_t value)
{
	int ret = 0;
	struct bt_ctf_field_integer *integer;
	struct bt_ctf_field_type_integer *integer_type;
	unsigned int size;
	int64_t min_value, max_value;

	if (!field || field->frozen ||
		bt_ctf_field_type_get_type_id(field->type) !=
			BT_CTF_FIELD_TYPE_ID_INTEGER) {
		ret = -1;
		goto end;
	}

	integer = container_of(field, struct bt_ctf_field_integer, parent);
	integer_type = container_of(field->type,
		struct bt_ctf_field_type_integer, parent);
	if (!integer_type->is_signed) {
		ret = -1;
		goto end;
	}

	size = integer_type->size;
	min_value = -(1ULL << (size - 1));
	max_value = (1ULL << (size - 1)) - 1;
	if (value < min_value || value > max_value) {
		ret = -1;
		goto end;
	}

	integer->payload.signd = value;
	integer->parent.payload_set = 1;
end:
	return ret;
}

int bt_ctf_field_unsigned_integer_get_value(struct bt_ctf_field *field,
		uint64_t *value)
{
	int ret = 0;
	struct bt_ctf_field_integer *integer;
	struct bt_ctf_field_type_integer *integer_type;

	if (!field || !value || !field->payload_set ||
		bt_ctf_field_type_get_type_id(field->type) !=
			BT_CTF_FIELD_TYPE_ID_INTEGER) {
		ret = -1;
		goto end;
	}

	integer_type = container_of(field->type,
		struct bt_ctf_field_type_integer, parent);
	if (integer_type->is_signed) {
		ret = -1;
		goto end;
	}

	integer = container_of(field,
		struct bt_ctf_field_integer, parent);
	*value = integer->payload.unsignd;
end:
	return ret;
}

int bt_ctf_field_unsigned_integer_set_value(struct bt_ctf_field *field,
		uint64_t value)
{
	int ret = 0;
	struct bt_ctf_field_integer *integer;
	struct bt_ctf_field_type_integer *integer_type;
	unsigned int size;
	uint64_t max_value;

	if (!field || field->frozen ||
		bt_ctf_field_type_get_type_id(field->type) !=
			BT_CTF_FIELD_TYPE_ID_INTEGER) {
		ret = -1;
		goto end;
	}

	integer = container_of(field, struct bt_ctf_field_integer, parent);
	integer_type = container_of(field->type,
		struct bt_ctf_field_type_integer, parent);
	if (integer_type->is_signed) {
		ret = -1;
		goto end;
	}

	size = integer_type->size;
	max_value = (size == 64) ? UINT64_MAX : ((uint64_t) 1 << size) - 1;
	if (value > max_value) {
		ret = -1;
		goto end;
	}

	integer->payload.unsignd = value;
	integer->parent.payload_set = 1;
end:
	return ret;
}

int bt_ctf_field_floating_point_get_value(struct bt_ctf_field *field,
		double *value)
{
	int ret = 0;
	struct bt_ctf_field_floating_point *floating_point;

	if (!field || !value || !field->payload_set ||
		bt_ctf_field_type_get_type_id(field->type) !=
			BT_CTF_FIELD_TYPE_ID_FLOAT) {
		ret = -1;
		goto end;
	}

	floating_point = container_of(field,
		struct bt_ctf_field_floating_point, parent);
	*value = floating_point->payload;
end:
	return ret;
}

int bt_ctf_field_floating_point_set_value(struct bt_ctf_field *field,
		double value)
{
	int ret = 0;
	struct bt_ctf_field_floating_point *floating_point;

	if (!field || field->frozen ||
		bt_ctf_field_type_get_type_id(field->type) !=
			BT_CTF_FIELD_TYPE_ID_FLOAT) {
		ret = -1;
		goto end;
	}
	floating_point = container_of(field, struct bt_ctf_field_floating_point,
		parent);
	floating_point->payload = value;
	floating_point->parent.payload_set = 1;
end:
	return ret;
}

const char *bt_ctf_field_string_get_value(struct bt_ctf_field *field)
{
	const char *ret = NULL;
	struct bt_ctf_field_string *string;

	if (!field || !field->payload_set ||
		bt_ctf_field_type_get_type_id(field->type) !=
			BT_CTF_FIELD_TYPE_ID_STRING) {
		goto end;
	}

	string = container_of(field,
		struct bt_ctf_field_string, parent);
	ret = string->payload->str;
end:
	return ret;
}

int bt_ctf_field_string_set_value(struct bt_ctf_field *field,
		const char *value)
{
	int ret = 0;
	struct bt_ctf_field_string *string;

	if (!field || !value || field->frozen ||
		bt_ctf_field_type_get_type_id(field->type) !=
			BT_CTF_FIELD_TYPE_ID_STRING) {
		ret = -1;
		goto end;
	}

	string = container_of(field, struct bt_ctf_field_string, parent);
	if (string->payload) {
		g_string_assign(string->payload, value);
	} else {
		string->payload = g_string_new(value);
	}

	string->parent.payload_set = 1;
end:
	return ret;
}

int bt_ctf_field_string_append(struct bt_ctf_field *field,
		const char *value)
{
	int ret = 0;
	struct bt_ctf_field_string *string_field;

	if (!field || !value || field->frozen ||
		bt_ctf_field_type_get_type_id(field->type) !=
			BT_CTF_FIELD_TYPE_ID_STRING) {
		ret = -1;
		goto end;
	}

	string_field = container_of(field, struct bt_ctf_field_string, parent);

	if (string_field->payload) {
		g_string_append(string_field->payload, value);
	} else {
		string_field->payload = g_string_new(value);
	}

	string_field->parent.payload_set = 1;

end:
	return ret;
}

int bt_ctf_field_string_append_len(struct bt_ctf_field *field,
		const char *value, unsigned int length)
{
	int i;
	int ret = 0;
	unsigned int effective_length = length;
	struct bt_ctf_field_string *string_field;

	if (!field || !value || field->frozen ||
		bt_ctf_field_type_get_type_id(field->type) !=
			BT_CTF_FIELD_TYPE_ID_STRING) {
		ret = -1;
		goto end;
	}

	string_field = container_of(field, struct bt_ctf_field_string, parent);

	/* make sure no null bytes are appended */
	for (i = 0; i < length; ++i) {
		if (value[i] == '\0') {
			effective_length = i;
			break;
		}
	}

	if (string_field->payload) {
		g_string_append_len(string_field->payload, value,
			effective_length);
	} else {
		string_field->payload = g_string_new_len(value,
			effective_length);
	}

	string_field->parent.payload_set = 1;

end:
	return ret;
}

BT_HIDDEN
int bt_ctf_field_validate(struct bt_ctf_field *field)
{
	int ret = 0;
	enum bt_ctf_field_type_id type_id;

	if (!field) {
		ret = -1;
		goto end;
	}

	type_id = bt_ctf_field_type_get_type_id(field->type);
	if (type_id <= BT_CTF_FIELD_TYPE_ID_UNKNOWN || type_id >= BT_CTF_NR_TYPE_IDS) {
		ret = -1;
		goto end;
	}

	ret = field_validate_funcs[type_id](field);
end:
	return ret;
}

BT_HIDDEN
int bt_ctf_field_reset(struct bt_ctf_field *field)
{
	int ret = 0;
	enum bt_ctf_field_type_id type_id;

	if (!field) {
		ret = -1;
		goto end;
	}

	type_id = bt_ctf_field_type_get_type_id(field->type);
	if (type_id <= BT_CTF_FIELD_TYPE_ID_UNKNOWN || type_id >= BT_CTF_NR_TYPE_IDS) {
		ret = -1;
		goto end;
	}

	ret = field_reset_funcs[type_id](field);
end:
	return ret;
}

BT_HIDDEN
int bt_ctf_field_serialize(struct bt_ctf_field *field,
		struct bt_ctf_stream_pos *pos,
		enum bt_ctf_byte_order native_byte_order)
{
	int ret = 0;
	enum bt_ctf_field_type_id type_id;

	if (!field || !pos) {
		ret = -1;
		goto end;
	}

	type_id = bt_ctf_field_type_get_type_id(field->type);
	if (type_id <= BT_CTF_FIELD_TYPE_ID_UNKNOWN || type_id >= BT_CTF_NR_TYPE_IDS) {
		ret = -1;
		goto end;
	}

	ret = field_serialize_funcs[type_id](field, pos, native_byte_order);
end:
	return ret;
}


BT_HIDDEN
bt_bool bt_ctf_field_is_set(struct bt_ctf_field *field)
{
	bt_bool is_set = BT_FALSE;
	enum bt_ctf_field_type_id type_id;

	if (!field) {
		goto end;
	}

	type_id = bt_ctf_field_type_get_type_id(field->type);
	if (type_id <= BT_CTF_FIELD_TYPE_ID_UNKNOWN || type_id >= BT_CTF_NR_TYPE_IDS) {
		goto end;
	}

	is_set = field_is_set_funcs[type_id](field);
end:
	return is_set;
}

struct bt_ctf_field *bt_ctf_field_copy(struct bt_ctf_field *field)
{
	int ret;
	struct bt_ctf_field *copy = NULL;
	enum bt_ctf_field_type_id type_id;

	if (!field) {
		goto end;
	}

	type_id = bt_ctf_field_type_get_type_id(field->type);
	if (type_id <= BT_CTF_FIELD_TYPE_ID_UNKNOWN || type_id >= BT_CTF_NR_TYPE_IDS) {
		goto end;
	}

	copy = bt_ctf_field_create(field->type);
	if (!copy) {
		goto end;
	}

	copy->payload_set = field->payload_set;
	ret = field_copy_funcs[type_id](field, copy);
	if (ret) {
		bt_put(copy);
		copy = NULL;
	}
end:
	return copy;
}

static
struct bt_ctf_field *bt_ctf_field_integer_create(struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_integer *integer = g_new0(
		struct bt_ctf_field_integer, 1);

	return integer ? &integer->parent : NULL;
}

static
struct bt_ctf_field *bt_ctf_field_enumeration_create(
	struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_enumeration *enumeration = g_new0(
		struct bt_ctf_field_enumeration, 1);

	return enumeration ? &enumeration->parent : NULL;
}

static
struct bt_ctf_field *bt_ctf_field_floating_point_create(
	struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_floating_point *floating_point;

	floating_point = g_new0(struct bt_ctf_field_floating_point, 1);
	return floating_point ? &floating_point->parent : NULL;
}

static
struct bt_ctf_field *bt_ctf_field_structure_create(
	struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_type_structure *structure_type = container_of(type,
		struct bt_ctf_field_type_structure, parent);
	struct bt_ctf_field_structure *structure = g_new0(
		struct bt_ctf_field_structure, 1);
	struct bt_ctf_field *field = NULL;

	if (!structure) {
		goto end;
	}

	structure->field_name_to_index = structure_type->field_name_to_index;
	structure->fields = g_ptr_array_new_with_free_func(
		(GDestroyNotify)bt_ctf_field_put);
	g_ptr_array_set_size(structure->fields,
		g_hash_table_size(structure->field_name_to_index));
	field = &structure->parent;
end:
	return field;
}

static
struct bt_ctf_field *bt_ctf_field_variant_create(struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_variant *variant = g_new0(
		struct bt_ctf_field_variant, 1);
	return variant ? &variant->parent : NULL;
}

static
struct bt_ctf_field *bt_ctf_field_array_create(struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_array *array = g_new0(struct bt_ctf_field_array, 1);
	struct bt_ctf_field_type_array *array_type;
	unsigned int array_length;

	if (!array || !type) {
		goto error;
	}

	array_type = container_of(type, struct bt_ctf_field_type_array, parent);
	array_length = array_type->length;
	array->elements = g_ptr_array_sized_new(array_length);
	if (!array->elements) {
		goto error;
	}

	g_ptr_array_set_free_func(array->elements,
		(GDestroyNotify)bt_ctf_field_put);
	g_ptr_array_set_size(array->elements, array_length);
	return &array->parent;
error:
	g_free(array);
	return NULL;
}

static
struct bt_ctf_field *bt_ctf_field_sequence_create(
	struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_sequence *sequence = g_new0(
		struct bt_ctf_field_sequence, 1);
	return sequence ? &sequence->parent : NULL;
}

static
struct bt_ctf_field *bt_ctf_field_string_create(struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_string *string = g_new0(
		struct bt_ctf_field_string, 1);
	return string ? &string->parent : NULL;
}

static
void bt_ctf_field_destroy(struct bt_object *obj)
{
	struct bt_ctf_field *field;
	struct bt_ctf_field_type *type;
	enum bt_ctf_field_type_id type_id;

	field = container_of(obj, struct bt_ctf_field, base);
	type = field->type;
	type_id = bt_ctf_field_type_get_type_id(type);
	if (type_id <= BT_CTF_FIELD_TYPE_ID_UNKNOWN ||
		type_id >= BT_CTF_NR_TYPE_IDS) {
		return;
	}

	field_destroy_funcs[type_id](field);
	bt_put(type);
}

static
void bt_ctf_field_integer_destroy(struct bt_ctf_field *field)
{
	struct bt_ctf_field_integer *integer;

	if (!field) {
		return;
	}

	integer = container_of(field, struct bt_ctf_field_integer, parent);
	g_free(integer);
}

static
void bt_ctf_field_enumeration_destroy(struct bt_ctf_field *field)
{
	struct bt_ctf_field_enumeration *enumeration;

	if (!field) {
		return;
	}

	enumeration = container_of(field, struct bt_ctf_field_enumeration,
		parent);
	bt_put(enumeration->payload);
	g_free(enumeration);
}

static
void bt_ctf_field_floating_point_destroy(struct bt_ctf_field *field)
{
	struct bt_ctf_field_floating_point *floating_point;

	if (!field) {
		return;
	}

	floating_point = container_of(field, struct bt_ctf_field_floating_point,
		parent);
	g_free(floating_point);
}

static
void bt_ctf_field_structure_destroy(struct bt_ctf_field *field)
{
	struct bt_ctf_field_structure *structure;

	if (!field) {
		return;
	}

	structure = container_of(field, struct bt_ctf_field_structure, parent);
	g_ptr_array_free(structure->fields, TRUE);
	g_free(structure);
}

static
void bt_ctf_field_variant_destroy(struct bt_ctf_field *field)
{
	struct bt_ctf_field_variant *variant;

	if (!field) {
		return;
	}

	variant = container_of(field, struct bt_ctf_field_variant, parent);
	bt_put(variant->tag);
	bt_put(variant->payload);
	g_free(variant);
}

static
void bt_ctf_field_array_destroy(struct bt_ctf_field *field)
{
	struct bt_ctf_field_array *array;

	if (!field) {
		return;
	}

	array = container_of(field, struct bt_ctf_field_array, parent);
	g_ptr_array_free(array->elements, TRUE);
	g_free(array);
}

static
void bt_ctf_field_sequence_destroy(struct bt_ctf_field *field)
{
	struct bt_ctf_field_sequence *sequence;

	if (!field) {
		return;
	}

	sequence = container_of(field, struct bt_ctf_field_sequence, parent);
	if (sequence->elements) {
		g_ptr_array_free(sequence->elements, TRUE);
	}
	bt_put(sequence->length);
	g_free(sequence);
}

static
void bt_ctf_field_string_destroy(struct bt_ctf_field *field)
{
	struct bt_ctf_field_string *string;
	if (!field) {
		return;
	}

	string = container_of(field, struct bt_ctf_field_string, parent);
	if (string->payload) {
		g_string_free(string->payload, TRUE);
	}
	g_free(string);
}

static
int bt_ctf_field_generic_validate(struct bt_ctf_field *field)
{
	return (field && field->payload_set) ? 0 : -1;
}

static
int bt_ctf_field_enumeration_validate(struct bt_ctf_field *field)
{
	int ret;
	struct bt_ctf_field_enumeration *enumeration;

	if (!field) {
		ret = -1;
		goto end;
	}

	enumeration = container_of(field, struct bt_ctf_field_enumeration,
		parent);
	if (!enumeration->payload) {
		ret = -1;
		goto end;
	}

	ret = bt_ctf_field_validate(enumeration->payload);
end:
	return ret;
}

static
int bt_ctf_field_structure_validate(struct bt_ctf_field *field)
{
	size_t i;
	int ret = 0;
	struct bt_ctf_field_structure *structure;

	if (!field) {
		ret = -1;
		goto end;
	}

	structure = container_of(field, struct bt_ctf_field_structure, parent);
	for (i = 0; i < structure->fields->len; i++) {
		ret = bt_ctf_field_validate(structure->fields->pdata[i]);
		if (ret) {
			const char *name;
			struct bt_ctf_field_type *field_type =
					bt_ctf_field_get_type(field);

			(void) bt_ctf_field_type_structure_get_field(field_type,
					&name, NULL, i);
			fprintf(stderr, "Field %s failed validation\n",
					name ? name : "NULL");
			bt_put(field_type);
			goto end;
		}
	}
end:
	return ret;
}

static
int bt_ctf_field_variant_validate(struct bt_ctf_field *field)
{
	int ret = 0;
	struct bt_ctf_field_variant *variant;

	if (!field) {
		ret = -1;
		goto end;
	}

	variant = container_of(field, struct bt_ctf_field_variant, parent);
	ret = bt_ctf_field_validate(variant->payload);
end:
	return ret;
}

static
int bt_ctf_field_array_validate(struct bt_ctf_field *field)
{
	size_t i;
	int ret = 0;
	struct bt_ctf_field_array *array;

	if (!field) {
		ret = -1;
		goto end;
	}

	array = container_of(field, struct bt_ctf_field_array, parent);
	for (i = 0; i < array->elements->len; i++) {
		ret = bt_ctf_field_validate(array->elements->pdata[i]);
		if (ret) {
			fprintf(stderr, "Failed to validate array field #%zu\n", i);
			goto end;
		}
	}
end:
	return ret;
}

static
int bt_ctf_field_sequence_validate(struct bt_ctf_field *field)
{
	size_t i;
	int ret = 0;
	struct bt_ctf_field_sequence *sequence;

	if (!field) {
		ret = -1;
		goto end;
	}

	sequence = container_of(field, struct bt_ctf_field_sequence, parent);
	for (i = 0; i < sequence->elements->len; i++) {
		ret = bt_ctf_field_validate(sequence->elements->pdata[i]);
		if (ret) {
			fprintf(stderr, "Failed to validate sequence field #%zu\n", i);
			goto end;
		}
	}
end:
	return ret;
}

static
int bt_ctf_field_generic_reset(struct bt_ctf_field *field)
{
	int ret = 0;

	if (!field) {
		ret = -1;
		goto end;
	}

	field->payload_set = 0;
end:
	return ret;
}

static
int bt_ctf_field_enumeration_reset(struct bt_ctf_field *field)
{
	int ret = 0;
	struct bt_ctf_field_enumeration *enumeration;

	if (!field) {
		ret = -1;
		goto end;
	}

	enumeration = container_of(field, struct bt_ctf_field_enumeration,
		parent);
	if (!enumeration->payload) {
		goto end;
	}

	ret = bt_ctf_field_reset(enumeration->payload);
end:
	return ret;
}

static
int bt_ctf_field_structure_reset(struct bt_ctf_field *field)
{
	size_t i;
	int ret = 0;
	struct bt_ctf_field_structure *structure;

	if (!field) {
		ret = -1;
		goto end;
	}

	structure = container_of(field, struct bt_ctf_field_structure, parent);
	for (i = 0; i < structure->fields->len; i++) {
		struct bt_ctf_field *member = structure->fields->pdata[i];

		if (!member) {
			/*
			 * Structure members are lazily initialized; skip if
			 * this member has not been allocated yet.
			 */
			continue;
		}

		ret = bt_ctf_field_reset(member);
		if (ret) {
			goto end;
		}
	}
end:
	return ret;
}

static
int bt_ctf_field_variant_reset(struct bt_ctf_field *field)
{
	int ret = 0;
	struct bt_ctf_field_variant *variant;

	if (!field) {
		ret = -1;
		goto end;
	}

	variant = container_of(field, struct bt_ctf_field_variant, parent);
	if (variant->payload) {
		ret = bt_ctf_field_reset(variant->payload);
	}
end:
	return ret;
}

static
int bt_ctf_field_array_reset(struct bt_ctf_field *field)
{
	size_t i;
	int ret = 0;
	struct bt_ctf_field_array *array;

	if (!field) {
		ret = -1;
		goto end;
	}

	array = container_of(field, struct bt_ctf_field_array, parent);
	for (i = 0; i < array->elements->len; i++) {
		struct bt_ctf_field *member = array->elements->pdata[i];

		if (!member) {
			/*
			 * Array elements are lazily initialized; skip if
			 * this member has not been allocated yet.
			 */
			continue;
		}

		ret = bt_ctf_field_reset(member);
		if (ret) {
			goto end;
		}
	}
end:
	return ret;
}

static
int bt_ctf_field_sequence_reset(struct bt_ctf_field *field)
{
	size_t i;
	int ret = 0;
	struct bt_ctf_field_sequence *sequence;

	if (!field) {
		ret = -1;
		goto end;
	}

	sequence = container_of(field, struct bt_ctf_field_sequence, parent);
	for (i = 0; i < sequence->elements->len; i++) {
		struct bt_ctf_field *member = sequence->elements->pdata[i];

		if (!member) {
			/*
			 * Sequence elements are lazily initialized; skip if
			 * this member has not been allocated yet.
			 */
			continue;
		}

		ret = bt_ctf_field_reset(member);
		if (ret) {
			goto end;
		}
	}
end:
	return ret;
}

static
int bt_ctf_field_string_reset(struct bt_ctf_field *field)
{
	int ret = 0;
	struct bt_ctf_field_string *string;

	if (!field) {
		ret = -1;
		goto end;
	}

	ret = bt_ctf_field_generic_reset(field);
	if (ret) {
		goto end;
	}

	string = container_of(field, struct bt_ctf_field_string, parent);
	if (string->payload) {
		g_string_truncate(string->payload, 0);
	}
end:
	return ret;
}

static
int bt_ctf_field_integer_serialize(struct bt_ctf_field *field,
		struct bt_ctf_stream_pos *pos,
		enum bt_ctf_byte_order native_byte_order)
{
	int ret = 0;
	struct bt_ctf_field_integer *integer = container_of(field,
		struct bt_ctf_field_integer, parent);

	if (!bt_ctf_field_generic_is_set(field)) {
		ret = -1;
		goto end;
	}
retry:
	ret = bt_ctf_field_integer_write(integer, pos, native_byte_order);
	if (ret == -EFAULT) {
		/*
		 * The field is too large to fit in the current packet's
		 * remaining space. Bump the packet size and retry.
		 */
		ret = increase_packet_size(pos);
		if (ret) {
			goto end;
		}
		goto retry;
	}
end:
	return ret;
}

static
int bt_ctf_field_enumeration_serialize(struct bt_ctf_field *field,
		struct bt_ctf_stream_pos *pos,
		enum bt_ctf_byte_order native_byte_order)
{
	struct bt_ctf_field_enumeration *enumeration = container_of(
		field, struct bt_ctf_field_enumeration, parent);

	return bt_ctf_field_serialize(enumeration->payload, pos,
		native_byte_order);
}

static
int bt_ctf_field_floating_point_serialize(struct bt_ctf_field *field,
		struct bt_ctf_stream_pos *pos,
		enum bt_ctf_byte_order native_byte_order)
{
	int ret = 0;
	struct bt_ctf_field_floating_point *floating_point = container_of(field,
		struct bt_ctf_field_floating_point, parent);

	if (!bt_ctf_field_generic_is_set(field)) {
		ret = -1;
		goto end;
	}
retry:
	ret = bt_ctf_field_floating_point_write(floating_point, pos,
		native_byte_order);
	if (ret == -EFAULT) {
		/*
		 * The field is too large to fit in the current packet's
		 * remaining space. Bump the packet size and retry.
		 */
		ret = increase_packet_size(pos);
		if (ret) {
			goto end;
		}
		goto retry;
	}
end:
	return ret;
}

static
int bt_ctf_field_structure_serialize(struct bt_ctf_field *field,
		struct bt_ctf_stream_pos *pos,
		enum bt_ctf_byte_order native_byte_order)
{
	size_t i;
	int ret = 0;
	struct bt_ctf_field_structure *structure = container_of(
		field, struct bt_ctf_field_structure, parent);

	while (!bt_ctf_stream_pos_access_ok(pos,
		offset_align(pos->offset, field->type->alignment))) {
		ret = increase_packet_size(pos);
		if (ret) {
			goto end;
		}
	}

	if (!bt_ctf_stream_pos_align(pos, field->type->alignment)) {
		ret = -1;
		goto end;
	}

	for (i = 0; i < structure->fields->len; i++) {
		struct bt_ctf_field *member = g_ptr_array_index(
			structure->fields, i);

		ret = bt_ctf_field_serialize(member, pos, native_byte_order);
		if (ret) {
			const char *name;
			struct bt_ctf_field_type *structure_type =
					bt_ctf_field_get_type(field);

			(void) bt_ctf_field_type_structure_get_field(
					structure_type, &name, NULL, i);
			fprintf(stderr, "Field %s failed to serialize\n",
					name ? name : "NULL");
			bt_put(structure_type);
			break;
		}
	}
end:
	return ret;
}

static
int bt_ctf_field_variant_serialize(struct bt_ctf_field *field,
		struct bt_ctf_stream_pos *pos,
		enum bt_ctf_byte_order native_byte_order)
{
	struct bt_ctf_field_variant *variant = container_of(
		field, struct bt_ctf_field_variant, parent);

	return bt_ctf_field_serialize(variant->payload, pos,
		native_byte_order);
}

static
int bt_ctf_field_array_serialize(struct bt_ctf_field *field,
		struct bt_ctf_stream_pos *pos,
		enum bt_ctf_byte_order native_byte_order)
{
	size_t i;
	int ret = 0;
	struct bt_ctf_field_array *array = container_of(
		field, struct bt_ctf_field_array, parent);

	for (i = 0; i < array->elements->len; i++) {
		ret = bt_ctf_field_serialize(
			g_ptr_array_index(array->elements, i), pos,
			native_byte_order);
		if (ret) {
			fprintf(stderr, "Failed to serialize array element #%zu\n", i);
			goto end;
		}
	}
end:
	return ret;
}

static
int bt_ctf_field_sequence_serialize(struct bt_ctf_field *field,
		struct bt_ctf_stream_pos *pos,
		enum bt_ctf_byte_order native_byte_order)
{
	size_t i;
	int ret = 0;
	struct bt_ctf_field_sequence *sequence = container_of(
		field, struct bt_ctf_field_sequence, parent);

	for (i = 0; i < sequence->elements->len; i++) {
		ret = bt_ctf_field_serialize(
			g_ptr_array_index(sequence->elements, i), pos,
			native_byte_order);
		if (ret) {
			fprintf(stderr, "Failed to serialize sequence element #%zu\n", i);
			goto end;
		}
	}
end:
	return ret;
}

static
int bt_ctf_field_string_serialize(struct bt_ctf_field *field,
		struct bt_ctf_stream_pos *pos,
		enum bt_ctf_byte_order native_byte_order)
{
	size_t i;
	int ret = 0;
	struct bt_ctf_field_string *string = container_of(field,
		struct bt_ctf_field_string, parent);
	struct bt_ctf_field_type *character_type =
		get_field_type(FIELD_TYPE_ALIAS_UINT8_T);
	struct bt_ctf_field *character = bt_ctf_field_create(character_type);

	for (i = 0; i < string->payload->len + 1; i++) {
		ret = bt_ctf_field_unsigned_integer_set_value(character,
			(uint64_t) string->payload->str[i]);
		if (ret) {
			goto end;
		}

		ret = bt_ctf_field_integer_serialize(character, pos,
			native_byte_order);
		if (ret) {
			goto end;
		}
	}
end:
	bt_put(character);
	bt_put(character_type);
	return ret;
}

static
int bt_ctf_field_integer_copy(struct bt_ctf_field *src,
		struct bt_ctf_field *dst)
{
	struct bt_ctf_field_integer *integer_src, *integer_dst;

	integer_src = container_of(src, struct bt_ctf_field_integer, parent);
	integer_dst = container_of(dst, struct bt_ctf_field_integer, parent);
	integer_dst->payload = integer_src->payload;
	return 0;
}

static
int bt_ctf_field_enumeration_copy(struct bt_ctf_field *src,
		struct bt_ctf_field *dst)
{
	int ret = 0;
	struct bt_ctf_field_enumeration *enum_src, *enum_dst;

	enum_src = container_of(src, struct bt_ctf_field_enumeration, parent);
	enum_dst = container_of(dst, struct bt_ctf_field_enumeration, parent);

	if (enum_src->payload) {
		enum_dst->payload = bt_ctf_field_copy(enum_src->payload);
		if (!enum_dst->payload) {
			ret = -1;
			goto end;
		}
	}
end:
	return ret;
}

static
int bt_ctf_field_floating_point_copy(
		struct bt_ctf_field *src, struct bt_ctf_field *dst)
{
	struct bt_ctf_field_floating_point *float_src, *float_dst;

	float_src = container_of(src, struct bt_ctf_field_floating_point,
		parent);
	float_dst = container_of(dst, struct bt_ctf_field_floating_point,
		parent);
	float_dst->payload = float_src->payload;
	return 0;
}

static
int bt_ctf_field_structure_copy(struct bt_ctf_field *src,
		struct bt_ctf_field *dst)
{
	int ret = 0, i;
	struct bt_ctf_field_structure *struct_src, *struct_dst;

	struct_src = container_of(src, struct bt_ctf_field_structure, parent);
	struct_dst = container_of(dst, struct bt_ctf_field_structure, parent);

	/* This field_name_to_index HT is owned by the structure field type */
	struct_dst->field_name_to_index = struct_src->field_name_to_index;
	g_ptr_array_set_size(struct_dst->fields, struct_src->fields->len);

	for (i = 0; i < struct_src->fields->len; i++) {
		struct bt_ctf_field *field =
			g_ptr_array_index(struct_src->fields, i);
		struct bt_ctf_field *field_copy = NULL;

		if (field) {
			field_copy = bt_ctf_field_copy(field);

			if (!field_copy) {
				ret = -1;
				goto end;
			}
		}

		g_ptr_array_index(struct_dst->fields, i) = field_copy;
	}
end:
	return ret;
}

static
int bt_ctf_field_variant_copy(struct bt_ctf_field *src,
		struct bt_ctf_field *dst)
{
	int ret = 0;
	struct bt_ctf_field_variant *variant_src, *variant_dst;

	variant_src = container_of(src, struct bt_ctf_field_variant, parent);
	variant_dst = container_of(dst, struct bt_ctf_field_variant, parent);

	if (variant_src->tag) {
		variant_dst->tag = bt_ctf_field_copy(variant_src->tag);
		if (!variant_dst->tag) {
			ret = -1;
			goto end;
		}
	}
	if (variant_src->payload) {
		variant_dst->payload = bt_ctf_field_copy(variant_src->payload);
		if (!variant_dst->payload) {
			ret = -1;
			goto end;
		}
	}
end:
	return ret;
}

static
int bt_ctf_field_array_copy(struct bt_ctf_field *src,
		struct bt_ctf_field *dst)
{
	int ret = 0, i;
	struct bt_ctf_field_array *array_src, *array_dst;

	array_src = container_of(src, struct bt_ctf_field_array, parent);
	array_dst = container_of(dst, struct bt_ctf_field_array, parent);

	g_ptr_array_set_size(array_dst->elements, array_src->elements->len);
	for (i = 0; i < array_src->elements->len; i++) {
		struct bt_ctf_field *field =
			g_ptr_array_index(array_src->elements, i);
		struct bt_ctf_field *field_copy = NULL;

		if (field) {
			field_copy = bt_ctf_field_copy(field);

			if (!field_copy) {
				ret = -1;
				goto end;
			}
		}

		g_ptr_array_index(array_dst->elements, i) = field_copy;
	}
end:
	return ret;
}

static
int bt_ctf_field_sequence_copy(struct bt_ctf_field *src,
		struct bt_ctf_field *dst)
{
	int ret = 0, i;
	struct bt_ctf_field_sequence *sequence_src, *sequence_dst;
	struct bt_ctf_field *src_length;
	struct bt_ctf_field *dst_length;

	sequence_src = container_of(src, struct bt_ctf_field_sequence, parent);
	sequence_dst = container_of(dst, struct bt_ctf_field_sequence, parent);

	src_length = bt_ctf_field_sequence_get_length(src);

	if (!src_length) {
		/* no length set yet: keep destination sequence empty */
		goto end;
	}

	/* copy source length */
	dst_length = bt_ctf_field_copy(src_length);
	bt_put(src_length);

	if (!dst_length) {
		ret = -1;
		goto end;
	}

	/* this will initialize the destination sequence's internal array */
	ret = bt_ctf_field_sequence_set_length(dst, dst_length);
	bt_put(dst_length);

	if (ret) {
		goto end;
	}

	assert(sequence_dst->elements->len == sequence_src->elements->len);

	for (i = 0; i < sequence_src->elements->len; i++) {
		struct bt_ctf_field *field =
			g_ptr_array_index(sequence_src->elements, i);
		struct bt_ctf_field *field_copy = NULL;

		if (field) {
			field_copy = bt_ctf_field_copy(field);

			if (!field_copy) {
				ret = -1;
				goto end;
			}
		}

		g_ptr_array_index(sequence_dst->elements, i) = field_copy;
	}
end:
	return ret;
}

static
int bt_ctf_field_string_copy(struct bt_ctf_field *src,
		struct bt_ctf_field *dst)
{
	int ret = 0;
	struct bt_ctf_field_string *string_src, *string_dst;

	string_src = container_of(src, struct bt_ctf_field_string, parent);
	string_dst = container_of(dst, struct bt_ctf_field_string, parent);

	if (string_src->payload) {
		string_dst->payload = g_string_new(string_src->payload->str);
		if (!string_dst->payload) {
			ret = -1;
			goto end;
		}
	}
end:
	return ret;
}

static
int increase_packet_size(struct bt_ctf_stream_pos *pos)
{
	int ret;

	assert(pos);
	ret = munmap_align(pos->base_mma);
	if (ret) {
		goto end;
	}

	pos->packet_size += PACKET_LEN_INCREMENT;
	do {
		ret = bt_posix_fallocate(pos->fd, pos->mmap_offset,
			pos->packet_size / CHAR_BIT);
	} while (ret == EINTR);
	if (ret) {
		errno = EINTR;
		ret = -1;
		goto end;
	}

	pos->base_mma = mmap_align(pos->packet_size / CHAR_BIT, pos->prot,
		pos->flags, pos->fd, pos->mmap_offset);
	if (pos->base_mma == MAP_FAILED) {
		ret = -1;
	}
end:
	return ret;
}

static
void generic_field_freeze(struct bt_ctf_field *field)
{
	field->frozen = 1;
}

static
void bt_ctf_field_enumeration_freeze(struct bt_ctf_field *field)
{
	struct bt_ctf_field_enumeration *enum_field =
		container_of(field, struct bt_ctf_field_enumeration, parent);

	bt_ctf_field_freeze(enum_field->payload);
	generic_field_freeze(field);
}

static
void bt_ctf_field_structure_freeze(struct bt_ctf_field *field)
{
	int i;
	struct bt_ctf_field_structure *structure_field =
		container_of(field, struct bt_ctf_field_structure, parent);

	for (i = 0; i < structure_field->fields->len; i++) {
		struct bt_ctf_field *field =
			g_ptr_array_index(structure_field->fields, i);

		bt_ctf_field_freeze(field);
	}

	generic_field_freeze(field);
}

static
void bt_ctf_field_variant_freeze(struct bt_ctf_field *field)
{
	struct bt_ctf_field_variant *variant_field =
		container_of(field, struct bt_ctf_field_variant, parent);

	bt_ctf_field_freeze(variant_field->tag);
	bt_ctf_field_freeze(variant_field->payload);
	generic_field_freeze(field);
}

static
void bt_ctf_field_array_freeze(struct bt_ctf_field *field)
{
	int i;
	struct bt_ctf_field_array *array_field =
		container_of(field, struct bt_ctf_field_array, parent);

	for (i = 0; i < array_field->elements->len; i++) {
		struct bt_ctf_field *field =
			g_ptr_array_index(array_field->elements, i);

		bt_ctf_field_freeze(field);
	}

	generic_field_freeze(field);
}

static
void bt_ctf_field_sequence_freeze(struct bt_ctf_field *field)
{
	int i;
	struct bt_ctf_field_sequence *sequence_field =
		container_of(field, struct bt_ctf_field_sequence, parent);

	bt_ctf_field_freeze(sequence_field->length);

	for (i = 0; i < sequence_field->elements->len; i++) {
		struct bt_ctf_field *field =
			g_ptr_array_index(sequence_field->elements, i);

		bt_ctf_field_freeze(field);
	}

	generic_field_freeze(field);
}

BT_HIDDEN
void bt_ctf_field_freeze(struct bt_ctf_field *field)
{
	enum bt_ctf_field_type_id type_id;

	if (!field) {
		goto end;
	}

	type_id = bt_ctf_field_get_type_id(field);
	if (type_id <= BT_CTF_FIELD_TYPE_ID_UNKNOWN ||
			type_id >= BT_CTF_NR_TYPE_IDS) {
		goto end;
	}

	field_freeze_funcs[type_id](field);
end:
	return;
}

static
bt_bool bt_ctf_field_generic_is_set(struct bt_ctf_field *field)
{
	return field && field->payload_set;
}

static
bt_bool bt_ctf_field_enumeration_is_set(struct bt_ctf_field *field)
{
	bt_bool is_set = BT_FALSE;
	struct bt_ctf_field_enumeration *enumeration;

	if (!field) {
		goto end;
	}

	enumeration = container_of(field, struct bt_ctf_field_enumeration,
			parent);
	if (!enumeration->payload) {
		goto end;
	}

	is_set = bt_ctf_field_is_set(enumeration->payload);
end:
	return is_set;
}

static
bt_bool bt_ctf_field_structure_is_set(struct bt_ctf_field *field)
{
	bt_bool is_set = BT_FALSE;
	size_t i;
	struct bt_ctf_field_structure *structure;

	if (!field) {
		goto end;
	}

	structure = container_of(field, struct bt_ctf_field_structure, parent);
	for (i = 0; i < structure->fields->len; i++) {
		is_set = bt_ctf_field_is_set(structure->fields->pdata[i]);
		if (!is_set) {
			goto end;
		}
	}
end:
	return is_set;
}

static
bt_bool bt_ctf_field_variant_is_set(struct bt_ctf_field *field)
{
	bt_bool is_set = BT_FALSE;
	struct bt_ctf_field_variant *variant;

	if (!field) {
		goto end;
	}

	variant = container_of(field, struct bt_ctf_field_variant, parent);
	is_set = bt_ctf_field_is_set(variant->payload);
end:
	return is_set;
}

static
bt_bool bt_ctf_field_array_is_set(struct bt_ctf_field *field)
{
	size_t i;
	bt_bool is_set = BT_FALSE;
	struct bt_ctf_field_array *array;

	if (!field) {
		goto end;
	}

	array = container_of(field, struct bt_ctf_field_array, parent);
	for (i = 0; i < array->elements->len; i++) {
		is_set = bt_ctf_field_is_set(array->elements->pdata[i]);
		if (!is_set) {
			goto end;
		}
	}
end:
	return is_set;
}

static
bt_bool bt_ctf_field_sequence_is_set(struct bt_ctf_field *field)
{
	size_t i;
	bt_bool is_set = BT_FALSE;
	struct bt_ctf_field_sequence *sequence;

	if (!field) {
		goto end;
	}

	sequence = container_of(field, struct bt_ctf_field_sequence, parent);
	for (i = 0; i < sequence->elements->len; i++) {
		is_set = bt_ctf_field_validate(sequence->elements->pdata[i]);
		if (!is_set) {
			goto end;
		}
	}
end:
	return is_set;
}
