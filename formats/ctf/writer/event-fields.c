/*
 * event-fields.c
 *
 * Babeltrace CTF Writer
 *
 * Copyright 2013 EfficiOS Inc.
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

#include <babeltrace/ctf-writer/event-fields.h>
#include <babeltrace/ctf-writer/event-fields-internal.h>
#include <babeltrace/ctf-writer/event-types-internal.h>
#include <babeltrace/compiler.h>
#include <babeltrace/compat/fcntl.h>

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
void bt_ctf_field_destroy(struct bt_ctf_ref *);
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
int bt_ctf_field_generic_validate(struct bt_ctf_field *field);
static
int bt_ctf_field_structure_validate(struct bt_ctf_field *field);
static
int bt_ctf_field_variant_validate(struct bt_ctf_field *field);
static
int bt_ctf_field_enumeration_validate(struct bt_ctf_field *field);
static
int bt_ctf_field_array_validate(struct bt_ctf_field *field);
static
int bt_ctf_field_sequence_validate(struct bt_ctf_field *field);

static
int bt_ctf_field_integer_serialize(struct bt_ctf_field *,
		struct ctf_stream_pos *);
static
int bt_ctf_field_enumeration_serialize(struct bt_ctf_field *,
		struct ctf_stream_pos *);
static
int bt_ctf_field_floating_point_serialize(struct bt_ctf_field *,
		struct ctf_stream_pos *);
static
int bt_ctf_field_structure_serialize(struct bt_ctf_field *,
		struct ctf_stream_pos *);
static
int bt_ctf_field_variant_serialize(struct bt_ctf_field *,
		struct ctf_stream_pos *);
static
int bt_ctf_field_array_serialize(struct bt_ctf_field *,
		struct ctf_stream_pos *);
static
int bt_ctf_field_sequence_serialize(struct bt_ctf_field *,
		struct ctf_stream_pos *);
static
int bt_ctf_field_string_serialize(struct bt_ctf_field *,
		struct ctf_stream_pos *);

static
int increase_packet_size(struct ctf_stream_pos *pos);

static
struct bt_ctf_field *(*field_create_funcs[])(
		struct bt_ctf_field_type *) = {
	[CTF_TYPE_INTEGER] = bt_ctf_field_integer_create,
	[CTF_TYPE_ENUM] = bt_ctf_field_enumeration_create,
	[CTF_TYPE_FLOAT] =
		bt_ctf_field_floating_point_create,
	[CTF_TYPE_STRUCT] = bt_ctf_field_structure_create,
	[CTF_TYPE_VARIANT] = bt_ctf_field_variant_create,
	[CTF_TYPE_ARRAY] = bt_ctf_field_array_create,
	[CTF_TYPE_SEQUENCE] = bt_ctf_field_sequence_create,
	[CTF_TYPE_STRING] = bt_ctf_field_string_create,
};

static
void (*field_destroy_funcs[])(struct bt_ctf_field *) = {
	[CTF_TYPE_INTEGER] = bt_ctf_field_integer_destroy,
	[CTF_TYPE_ENUM] = bt_ctf_field_enumeration_destroy,
	[CTF_TYPE_FLOAT] =
		bt_ctf_field_floating_point_destroy,
	[CTF_TYPE_STRUCT] = bt_ctf_field_structure_destroy,
	[CTF_TYPE_VARIANT] = bt_ctf_field_variant_destroy,
	[CTF_TYPE_ARRAY] = bt_ctf_field_array_destroy,
	[CTF_TYPE_SEQUENCE] = bt_ctf_field_sequence_destroy,
	[CTF_TYPE_STRING] = bt_ctf_field_string_destroy,
};

static
int (*field_validate_funcs[])(struct bt_ctf_field *) = {
	[CTF_TYPE_INTEGER] = bt_ctf_field_generic_validate,
	[CTF_TYPE_ENUM] = bt_ctf_field_enumeration_validate,
	[CTF_TYPE_FLOAT] = bt_ctf_field_generic_validate,
	[CTF_TYPE_STRUCT] = bt_ctf_field_structure_validate,
	[CTF_TYPE_VARIANT] = bt_ctf_field_variant_validate,
	[CTF_TYPE_ARRAY] = bt_ctf_field_array_validate,
	[CTF_TYPE_SEQUENCE] = bt_ctf_field_sequence_validate,
	[CTF_TYPE_STRING] = bt_ctf_field_generic_validate,
};

static
int (*field_serialize_funcs[])(struct bt_ctf_field *,
		struct ctf_stream_pos *) = {
	[CTF_TYPE_INTEGER] = bt_ctf_field_integer_serialize,
	[CTF_TYPE_ENUM] = bt_ctf_field_enumeration_serialize,
	[CTF_TYPE_FLOAT] =
		bt_ctf_field_floating_point_serialize,
	[CTF_TYPE_STRUCT] = bt_ctf_field_structure_serialize,
	[CTF_TYPE_VARIANT] = bt_ctf_field_variant_serialize,
	[CTF_TYPE_ARRAY] = bt_ctf_field_array_serialize,
	[CTF_TYPE_SEQUENCE] = bt_ctf_field_sequence_serialize,
	[CTF_TYPE_STRING] = bt_ctf_field_string_serialize,
};

struct bt_ctf_field *bt_ctf_field_create(struct bt_ctf_field_type *type)
{
	struct bt_ctf_field *field = NULL;
	enum ctf_type_id type_id;

	if (!type) {
		goto error;
	}

	type_id = bt_ctf_field_type_get_type_id(type);
	if (type_id <= CTF_TYPE_UNKNOWN || type_id >= NR_CTF_TYPES ||
		bt_ctf_field_type_validate(type)) {
		goto error;
	}

	field = field_create_funcs[type_id](type);
	if (!field) {
		goto error;
	}

	/* The type's declaration can't change after this point */
	bt_ctf_field_type_freeze(type);
	bt_ctf_field_type_get(type);
	bt_ctf_ref_init(&field->ref_count);
	field->type = type;
error:
	return field;
}

void bt_ctf_field_get(struct bt_ctf_field *field)
{
	if (field) {
		bt_ctf_ref_get(&field->ref_count);
	}
}

void bt_ctf_field_put(struct bt_ctf_field *field)
{
	if (field) {
		bt_ctf_ref_put(&field->ref_count, bt_ctf_field_destroy);
	}
}

int bt_ctf_field_sequence_set_length(struct bt_ctf_field *field,
		struct bt_ctf_field *length_field)
{
	int ret = 0;
	struct bt_ctf_field_type_integer *length_type;
	struct bt_ctf_field_integer *length;
	struct bt_ctf_field_sequence *sequence;
	uint64_t sequence_length;

	if (!field || !length_field) {
		ret = -1;
		goto end;
	}
	if (bt_ctf_field_type_get_type_id(length_field->type) !=
		CTF_TYPE_INTEGER) {
		ret = -1;
		goto end;
	}

	length_type = container_of(length_field->type,
		struct bt_ctf_field_type_integer, parent);
	if (length_type->declaration.signedness) {
		ret = -1;
		goto end;
	}

	length = container_of(length_field, struct bt_ctf_field_integer,
		parent);
	sequence_length = length->definition.value._unsigned;
	sequence = container_of(field, struct bt_ctf_field_sequence, parent);
	if (sequence->elements) {
		g_ptr_array_free(sequence->elements, TRUE);
		bt_ctf_field_put(sequence->length);
	}

	sequence->elements = g_ptr_array_sized_new((size_t)sequence_length);
	if (!sequence->elements) {
		ret = -1;
		goto end;
	}

	g_ptr_array_set_free_func(sequence->elements,
		(GDestroyNotify)bt_ctf_field_put);
	g_ptr_array_set_size(sequence->elements, (size_t)sequence_length);
	bt_ctf_field_get(length_field);
	sequence->length = length_field;
end:
	return ret;
}

struct bt_ctf_field *bt_ctf_field_structure_get_field(
		struct bt_ctf_field *field, const char *name)
{
	struct bt_ctf_field *new_field = NULL;
	GQuark field_quark;
	struct bt_ctf_field_structure *structure;
	struct bt_ctf_field_type_structure *structure_type;
	struct bt_ctf_field_type *field_type;
	size_t index;

	if (!field || !name ||
		bt_ctf_field_type_get_type_id(field->type) !=
			CTF_TYPE_STRUCT) {
		goto error;
	}

	field_quark = g_quark_from_string(name);
	structure = container_of(field, struct bt_ctf_field_structure, parent);
	structure_type = container_of(field->type,
		struct bt_ctf_field_type_structure, parent);
	field_type = bt_ctf_field_type_structure_get_type(structure_type, name);
	if (!g_hash_table_lookup_extended(structure->field_name_to_index,
		GUINT_TO_POINTER(field_quark), NULL, (gpointer *)&index)) {
		goto error;
	}

	if (structure->fields->pdata[index]) {
		new_field = structure->fields->pdata[index];
		goto end;
	}

	new_field = bt_ctf_field_create(field_type);
	if (!new_field) {
		goto error;
	}

	structure->fields->pdata[index] = new_field;
end:
	bt_ctf_field_get(new_field);
error:
	return new_field;
}

BT_HIDDEN
int bt_ctf_field_structure_set_field(struct bt_ctf_field *field,
		const char *name, struct bt_ctf_field *value)
{
	int ret = 0;
	GQuark field_quark;
	struct bt_ctf_field_structure *structure;
	struct bt_ctf_field_type_structure *structure_type;
	struct bt_ctf_field_type *expected_field_type;
	size_t index;

	if (!field || !name || !value ||
		bt_ctf_field_type_get_type_id(field->type) !=
			CTF_TYPE_STRUCT) {
		ret = -1;
		goto end;
	}

	field_quark = g_quark_from_string(name);
	structure = container_of(field, struct bt_ctf_field_structure, parent);
	structure_type = container_of(field->type,
		struct bt_ctf_field_type_structure, parent);
	expected_field_type = bt_ctf_field_type_structure_get_type(
		structure_type, name);
	if (expected_field_type != value->type) {
		ret = -1;
		goto end;
	}

	if (!g_hash_table_lookup_extended(structure->field_name_to_index,
		GUINT_TO_POINTER(field_quark), NULL, (gpointer *) &index)) {
		goto end;
	}

	if (structure->fields->pdata[index]) {
		bt_ctf_field_put(structure->fields->pdata[index]);
	}

	structure->fields->pdata[index] = value;
	bt_ctf_field_get(value);
end:
	return ret;
}

struct bt_ctf_field *bt_ctf_field_array_get_field(struct bt_ctf_field *field,
		uint64_t index)
{
	struct bt_ctf_field *new_field = NULL;
	struct bt_ctf_field_array *array;
	struct bt_ctf_field_type_array *array_type;
	struct bt_ctf_field_type *field_type;

	if (!field || bt_ctf_field_type_get_type_id(field->type) !=
		CTF_TYPE_ARRAY) {
		goto end;
	}

	array = container_of(field, struct bt_ctf_field_array, parent);
	if (index >= array->elements->len) {
		goto end;
	}

	array_type = container_of(field->type, struct bt_ctf_field_type_array,
		parent);
	field_type = bt_ctf_field_type_array_get_element_type(array_type);
	if (array->elements->pdata[(size_t)index]) {
		new_field = array->elements->pdata[(size_t)index];
		goto end;
	}

	new_field = bt_ctf_field_create(field_type);
	bt_ctf_field_get(new_field);
	array->elements->pdata[(size_t)index] = new_field;
end:
	return new_field;
}

struct bt_ctf_field *bt_ctf_field_sequence_get_field(struct bt_ctf_field *field,
		uint64_t index)
{
	struct bt_ctf_field *new_field = NULL;
	struct bt_ctf_field_sequence *sequence;
	struct bt_ctf_field_type_sequence *sequence_type;
	struct bt_ctf_field_type *field_type;

	if (!field || bt_ctf_field_type_get_type_id(field->type) !=
		CTF_TYPE_SEQUENCE) {
		goto end;
	}

	sequence = container_of(field, struct bt_ctf_field_sequence, parent);
	if (!sequence->elements || sequence->elements->len <= index) {
		goto end;
	}

	sequence_type = container_of(field->type,
		struct bt_ctf_field_type_sequence, parent);
	field_type = bt_ctf_field_type_sequence_get_element_type(sequence_type);
	if (sequence->elements->pdata[(size_t)index]) {
		new_field = sequence->elements->pdata[(size_t)index];
		goto end;
	}

	new_field = bt_ctf_field_create(field_type);
	bt_ctf_field_get(new_field);
	sequence->elements->pdata[(size_t)index] = new_field;
end:
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
			CTF_TYPE_VARIANT ||
		bt_ctf_field_type_get_type_id(tag_field->type) !=
			CTF_TYPE_ENUM) {
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

	tag_enum_value = tag_enum_integer->definition.value._signed;
	field_type = bt_ctf_field_type_variant_get_field_type(variant_type,
		tag_enum_value);
	if (!field_type) {
		goto end;
	}

	new_field = bt_ctf_field_create(field_type);
	if (!new_field) {
		goto end;
	}

	bt_ctf_field_put(variant->tag);
	bt_ctf_field_put(variant->payload);
	bt_ctf_field_get(new_field);
	bt_ctf_field_get(tag_field);
	variant->tag = tag_field;
	variant->payload = new_field;
end:
	bt_ctf_field_put(tag_enum);
	return new_field;
}

struct bt_ctf_field *bt_ctf_field_enumeration_get_container(
	struct bt_ctf_field *field)
{
	struct bt_ctf_field *container = NULL;
	struct bt_ctf_field_enumeration *enumeration;

	if (!field) {
		goto end;
	}

	enumeration = container_of(field, struct bt_ctf_field_enumeration,
		parent);
	if (!enumeration->payload) {
		struct bt_ctf_field_type_enumeration *enumeration_type =
			container_of(field->type,
			struct bt_ctf_field_type_enumeration, parent);
		enumeration->payload =
			bt_ctf_field_create(enumeration_type->container);
	}

	container = enumeration->payload;
	bt_ctf_field_get(container);
end:
	return container;
}

int bt_ctf_field_signed_integer_set_value(struct bt_ctf_field *field,
		int64_t value)
{
	int ret = 0;
	struct bt_ctf_field_integer *integer;
	struct bt_ctf_field_type_integer *integer_type;
	unsigned int size;
	int64_t min_value, max_value;

	if (!field ||
		bt_ctf_field_type_get_type_id(field->type) !=
			CTF_TYPE_INTEGER) {
		ret = -1;
		goto end;
	}

	integer = container_of(field, struct bt_ctf_field_integer, parent);
	integer_type = container_of(field->type,
		struct bt_ctf_field_type_integer, parent);
	if (!integer_type->declaration.signedness) {
		ret = -1;
		goto end;
	}

	size = integer_type->declaration.len;
	min_value = -((int64_t)1 << (size - 1));
	max_value = ((int64_t)1 << (size - 1)) - 1;
	if (value < min_value || value > max_value) {
		ret = -1;
		goto end;
	}

	integer->definition.value._signed = value;
	integer->parent.payload_set = 1;
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

	if (!field ||
		bt_ctf_field_type_get_type_id(field->type) !=
			CTF_TYPE_INTEGER) {
		ret = -1;
		goto end;
	}

	integer = container_of(field, struct bt_ctf_field_integer, parent);
	integer_type = container_of(field->type,
		struct bt_ctf_field_type_integer, parent);
	if (integer_type->declaration.signedness) {
		ret = -1;
		goto end;
	}

	size = integer_type->declaration.len;
	max_value = (size == 64) ? UINT64_MAX : ((uint64_t)1 << size) - 1;
	if (value > max_value) {
		ret = -1;
		goto end;
	}

	integer->definition.value._unsigned = value;
	integer->parent.payload_set = 1;
end:
	return ret;
}

int bt_ctf_field_floating_point_set_value(struct bt_ctf_field *field,
		double value)
{
	int ret = 0;
	struct bt_ctf_field_floating_point *floating_point;

	if (!field ||
		bt_ctf_field_type_get_type_id(field->type) !=
			CTF_TYPE_FLOAT) {
		ret = -1;
		goto end;
	}
	floating_point = container_of(field, struct bt_ctf_field_floating_point,
		parent);
	floating_point->definition.value = value;
	floating_point->parent.payload_set = 1;
end:
	return ret;
}

int bt_ctf_field_string_set_value(struct bt_ctf_field *field,
		const char *value)
{
	int ret = 0;
	struct bt_ctf_field_string *string;

	if (!field || !value ||
		bt_ctf_field_type_get_type_id(field->type) !=
			CTF_TYPE_STRING) {
		ret = -1;
		goto end;
	}

	string = container_of(field, struct bt_ctf_field_string, parent);
	if (string->payload) {
		g_string_free(string->payload, TRUE);
	}

	string->payload = g_string_new(value);
	string->parent.payload_set = 1;
end:
	return ret;
}

BT_HIDDEN
int bt_ctf_field_validate(struct bt_ctf_field *field)
{
	int ret = 0;
	enum ctf_type_id type_id;

	if (!field) {
		ret = -1;
		goto end;
	}

	type_id = bt_ctf_field_type_get_type_id(field->type);
	if (type_id <= CTF_TYPE_UNKNOWN || type_id >= NR_CTF_TYPES) {
		ret = -1;
		goto end;
	}

	ret = field_validate_funcs[type_id](field);
end:
	return ret;
}

BT_HIDDEN
int bt_ctf_field_serialize(struct bt_ctf_field *field,
		struct ctf_stream_pos *pos)
{
	int ret = 0;
	enum ctf_type_id type_id;

	if (!field || !pos) {
		ret = -1;
		goto end;
	}

	type_id = bt_ctf_field_type_get_type_id(field->type);
	if (type_id <= CTF_TYPE_UNKNOWN || type_id >= NR_CTF_TYPES) {
		ret = -1;
		goto end;
	}

	ret = field_serialize_funcs[type_id](field, pos);
end:
	return ret;
}

static
struct bt_ctf_field *bt_ctf_field_integer_create(struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_type_integer *integer_type = container_of(type,
		struct bt_ctf_field_type_integer, parent);
	struct bt_ctf_field_integer *integer = g_new0(
		struct bt_ctf_field_integer, 1);

	if (integer) {
		integer->definition.declaration = &integer_type->declaration;
	}

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
	struct bt_ctf_field_type_floating_point *floating_point_type;

	floating_point = g_new0(struct bt_ctf_field_floating_point, 1);
	if (!floating_point) {
		goto end;
	}

	floating_point_type = container_of(type,
		struct bt_ctf_field_type_floating_point, parent);
	floating_point->definition.declaration = container_of(
		type->declaration, struct declaration_float, p);


	floating_point->definition.sign = &floating_point->sign;
	floating_point->sign.declaration = &floating_point_type->sign;
	floating_point->definition.sign->p.declaration =
		&floating_point_type->sign.p;

	floating_point->definition.mantissa = &floating_point->mantissa;
	floating_point->mantissa.declaration = &floating_point_type->mantissa;
	floating_point->definition.mantissa->p.declaration =
		&floating_point_type->mantissa.p;

	floating_point->definition.exp = &floating_point->exp;
	floating_point->exp.declaration = &floating_point_type->exp;
	floating_point->definition.exp->p.declaration =
		&floating_point_type->exp.p;

end:
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

	if (!structure || !structure_type->fields->len) {
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
void bt_ctf_field_destroy(struct bt_ctf_ref *ref)
{
	struct bt_ctf_field *field;
	struct bt_ctf_field_type *type;
	enum ctf_type_id type_id;

	if (!ref) {
		return;
	}

	field = container_of(ref, struct bt_ctf_field, ref_count);
	type = field->type;
	type_id = bt_ctf_field_type_get_type_id(type);
	if (type_id <= CTF_TYPE_UNKNOWN ||
		type_id >= NR_CTF_TYPES) {
		return;
	}

	field_destroy_funcs[type_id](field);
	if (type) {
		bt_ctf_field_type_put(type);
	}
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
	bt_ctf_field_put(enumeration->payload);
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
	bt_ctf_field_put(variant->tag);
	bt_ctf_field_put(variant->payload);
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
	g_ptr_array_free(sequence->elements, TRUE);
	bt_ctf_field_put(sequence->length);
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
	g_string_free(string->payload, TRUE);
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
			goto end;
		}
	}
end:
	return ret;
}

static
int bt_ctf_field_integer_serialize(struct bt_ctf_field *field,
		struct ctf_stream_pos *pos)
{
	int ret = 0;
	struct bt_ctf_field_integer *integer = container_of(field,
		struct bt_ctf_field_integer, parent);

retry:
	ret = ctf_integer_write(&pos->parent, &integer->definition.p);
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
		struct ctf_stream_pos *pos)
{
	struct bt_ctf_field_enumeration *enumeration = container_of(
		field, struct bt_ctf_field_enumeration, parent);

	return bt_ctf_field_serialize(enumeration->payload, pos);
}

static
int bt_ctf_field_floating_point_serialize(struct bt_ctf_field *field,
		struct ctf_stream_pos *pos)
{
	int ret = 0;
	struct bt_ctf_field_floating_point *floating_point = container_of(field,
		struct bt_ctf_field_floating_point, parent);

retry:
	ret = ctf_float_write(&pos->parent, &floating_point->definition.p);
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
		struct ctf_stream_pos *pos)
{
	size_t i;
	int ret = 0;
	struct bt_ctf_field_structure *structure = container_of(
		field, struct bt_ctf_field_structure, parent);

	while (!ctf_pos_access_ok(pos,
		offset_align(pos->offset,
			field->type->declaration->alignment))) {
		ret = increase_packet_size(pos);
		if (ret) {
			goto end;
		}
	}

	if (!ctf_align_pos(pos, field->type->declaration->alignment)) {
		ret = -1;
		goto end;
	}

	for (i = 0; i < structure->fields->len; i++) {
		struct bt_ctf_field *field = g_ptr_array_index(
			structure->fields, i);

		ret = bt_ctf_field_serialize(field, pos);
		if (ret) {
			break;
		}
	}
end:
	return ret;
}

static
int bt_ctf_field_variant_serialize(struct bt_ctf_field *field,
		struct ctf_stream_pos *pos)
{
	struct bt_ctf_field_variant *variant = container_of(
		field, struct bt_ctf_field_variant, parent);

	return bt_ctf_field_serialize(variant->payload, pos);
}

static
int bt_ctf_field_array_serialize(struct bt_ctf_field *field,
		struct ctf_stream_pos *pos)
{
	size_t i;
	int ret = 0;
	struct bt_ctf_field_array *array = container_of(
		field, struct bt_ctf_field_array, parent);

	for (i = 0; i < array->elements->len; i++) {
		ret = bt_ctf_field_serialize(
			g_ptr_array_index(array->elements, i), pos);
		if (ret) {
			goto end;
		}
	}
end:
	return ret;
}

static
int bt_ctf_field_sequence_serialize(struct bt_ctf_field *field,
		struct ctf_stream_pos *pos)
{
	size_t i;
	int ret = 0;
	struct bt_ctf_field_sequence *sequence = container_of(
		field, struct bt_ctf_field_sequence, parent);

	for (i = 0; i < sequence->elements->len; i++) {
		ret = bt_ctf_field_serialize(
			g_ptr_array_index(sequence->elements, i), pos);
		if (ret) {
			goto end;
		}
	}
end:
	return ret;
}

static
int bt_ctf_field_string_serialize(struct bt_ctf_field *field,
		struct ctf_stream_pos *pos)
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

		ret = bt_ctf_field_integer_serialize(character, pos);
		if (ret) {
			goto end;
		}
	}
end:
	bt_ctf_field_put(character);
	bt_ctf_field_type_put(character_type);
	return ret;
}

static
int increase_packet_size(struct ctf_stream_pos *pos)
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
