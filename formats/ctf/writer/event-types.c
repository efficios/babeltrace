/*
 * event-types.c
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

#include <babeltrace/ctf-writer/event-types.h>
#include <babeltrace/ctf-writer/event-types-internal.h>
#include <babeltrace/ctf-writer/writer-internal.h>
#include <babeltrace/compiler.h>
#include <babeltrace/endian.h>
#include <float.h>
#include <inttypes.h>
#include <stdlib.h>

struct range_overlap_query {
	int64_t range_start, range_end;
	int overlaps;
	GQuark mapping_name;
};

static
void bt_ctf_field_type_integer_destroy(struct bt_ctf_ref *);
static
void bt_ctf_field_type_enumeration_destroy(struct bt_ctf_ref *);
static
void bt_ctf_field_type_floating_point_destroy(struct bt_ctf_ref *);
static
void bt_ctf_field_type_structure_destroy(struct bt_ctf_ref *);
static
void bt_ctf_field_type_variant_destroy(struct bt_ctf_ref *);
static
void bt_ctf_field_type_array_destroy(struct bt_ctf_ref *);
static
void bt_ctf_field_type_sequence_destroy(struct bt_ctf_ref *);
static
void bt_ctf_field_type_string_destroy(struct bt_ctf_ref *);

static
void (* const type_destroy_funcs[])(struct bt_ctf_ref *) = {
	[CTF_TYPE_INTEGER] = bt_ctf_field_type_integer_destroy,
	[CTF_TYPE_ENUM] =
		bt_ctf_field_type_enumeration_destroy,
	[CTF_TYPE_FLOAT] =
		bt_ctf_field_type_floating_point_destroy,
	[CTF_TYPE_STRUCT] = bt_ctf_field_type_structure_destroy,
	[CTF_TYPE_VARIANT] = bt_ctf_field_type_variant_destroy,
	[CTF_TYPE_ARRAY] = bt_ctf_field_type_array_destroy,
	[CTF_TYPE_SEQUENCE] = bt_ctf_field_type_sequence_destroy,
	[CTF_TYPE_STRING] = bt_ctf_field_type_string_destroy,
};

static
void generic_field_type_freeze(struct bt_ctf_field_type *);
static
void bt_ctf_field_type_enumeration_freeze(struct bt_ctf_field_type *);
static
void bt_ctf_field_type_structure_freeze(struct bt_ctf_field_type *);
static
void bt_ctf_field_type_variant_freeze(struct bt_ctf_field_type *);
static
void bt_ctf_field_type_array_freeze(struct bt_ctf_field_type *);
static
void bt_ctf_field_type_sequence_freeze(struct bt_ctf_field_type *);

static
type_freeze_func const type_freeze_funcs[] = {
	[CTF_TYPE_INTEGER] = generic_field_type_freeze,
	[CTF_TYPE_ENUM] = bt_ctf_field_type_enumeration_freeze,
	[CTF_TYPE_FLOAT] = generic_field_type_freeze,
	[CTF_TYPE_STRUCT] = bt_ctf_field_type_structure_freeze,
	[CTF_TYPE_VARIANT] = bt_ctf_field_type_variant_freeze,
	[CTF_TYPE_ARRAY] = bt_ctf_field_type_array_freeze,
	[CTF_TYPE_SEQUENCE] = bt_ctf_field_type_sequence_freeze,
	[CTF_TYPE_STRING] = generic_field_type_freeze,
};

static
int bt_ctf_field_type_integer_serialize(struct bt_ctf_field_type *,
		struct metadata_context *);
static
int bt_ctf_field_type_enumeration_serialize(struct bt_ctf_field_type *,
		struct metadata_context *);
static
int bt_ctf_field_type_floating_point_serialize(
		struct bt_ctf_field_type *, struct metadata_context *);
static
int bt_ctf_field_type_structure_serialize(struct bt_ctf_field_type *,
		struct metadata_context *);
static
int bt_ctf_field_type_variant_serialize(struct bt_ctf_field_type *,
		struct metadata_context *);
static
int bt_ctf_field_type_array_serialize(struct bt_ctf_field_type *,
		struct metadata_context *);
static
int bt_ctf_field_type_sequence_serialize(struct bt_ctf_field_type *,
		struct metadata_context *);
static
int bt_ctf_field_type_string_serialize(struct bt_ctf_field_type *,
		struct metadata_context *);

static
type_serialize_func const type_serialize_funcs[] = {
	[CTF_TYPE_INTEGER] = bt_ctf_field_type_integer_serialize,
	[CTF_TYPE_ENUM] =
		bt_ctf_field_type_enumeration_serialize,
	[CTF_TYPE_FLOAT] =
		bt_ctf_field_type_floating_point_serialize,
	[CTF_TYPE_STRUCT] =
		bt_ctf_field_type_structure_serialize,
	[CTF_TYPE_VARIANT] = bt_ctf_field_type_variant_serialize,
	[CTF_TYPE_ARRAY] = bt_ctf_field_type_array_serialize,
	[CTF_TYPE_SEQUENCE] = bt_ctf_field_type_sequence_serialize,
	[CTF_TYPE_STRING] = bt_ctf_field_type_string_serialize,
};

static
void bt_ctf_field_type_integer_set_byte_order(struct bt_ctf_field_type *,
		int byte_order);
static
void bt_ctf_field_type_floating_point_set_byte_order(
		struct bt_ctf_field_type *, int byte_order);

static
void (* const set_byte_order_funcs[])(struct bt_ctf_field_type *,
	int) = {
	[CTF_TYPE_INTEGER] =
		bt_ctf_field_type_integer_set_byte_order,
	[CTF_TYPE_FLOAT] =
		bt_ctf_field_type_floating_point_set_byte_order,
	[CTF_TYPE_ENUM ... CTF_TYPE_SEQUENCE] = NULL,
};


static
void destroy_enumeration_mapping(struct enumeration_mapping *mapping)
{
	g_free(mapping);
}

static
void destroy_structure_field(struct structure_field *field)
{
	if (field->type) {
		bt_ctf_field_type_put(field->type);
	}

	g_free(field);
}

static
void check_ranges_overlap(gpointer element, gpointer query)
{
	struct enumeration_mapping *mapping = element;
	struct range_overlap_query *overlap_query = query;

	if (mapping->range_start <= overlap_query->range_end
		&& overlap_query->range_start <= mapping->range_end) {
		overlap_query->overlaps = 1;
		overlap_query->mapping_name = mapping->string;
	}

	overlap_query->overlaps |=
		mapping->string == overlap_query->mapping_name;
}

static
void bt_ctf_field_type_init(struct bt_ctf_field_type *type)
{
	enum ctf_type_id type_id = type->declaration->id;
	int ret;

	assert(type && (type_id > CTF_TYPE_UNKNOWN) &&
		(type_id < NR_CTF_TYPES));

	bt_ctf_ref_init(&type->ref_count);
	type->freeze = type_freeze_funcs[type_id];
	type->serialize = type_serialize_funcs[type_id];
	ret = bt_ctf_field_type_set_byte_order(type, BT_CTF_BYTE_ORDER_NATIVE);
	assert(!ret);
	type->declaration->alignment = 1;
}

static
int add_structure_field(GPtrArray *fields,
		GHashTable *field_name_to_index,
		struct bt_ctf_field_type *field_type,
		const char *field_name)
{
	int ret = 0;
	GQuark name_quark = g_quark_from_string(field_name);
	struct structure_field *field;

	/* Make sure structure does not contain a field of the same name */
	if (g_hash_table_lookup_extended(field_name_to_index,
		GUINT_TO_POINTER(name_quark), NULL, NULL)) {
		ret = -1;
		goto end;
	}

	field = g_new0(struct structure_field, 1);
	if (!field) {
		ret = -1;
		goto end;
	}

	bt_ctf_field_type_get(field_type);
	field->name = name_quark;
	field->type = field_type;
	g_hash_table_insert(field_name_to_index,
		(gpointer) (unsigned long) name_quark,
		(gpointer) (unsigned long) fields->len);
	g_ptr_array_add(fields, field);
	bt_ctf_field_type_freeze(field_type);
end:
	return ret;
}

BT_HIDDEN
int bt_ctf_field_type_validate(struct bt_ctf_field_type *type)
{
	int ret = 0;

	if (!type) {
		ret = -1;
		goto end;
	}

	if (type->declaration->id == CTF_TYPE_ENUM) {
		struct bt_ctf_field_type_enumeration *enumeration =
			container_of(type, struct bt_ctf_field_type_enumeration,
			parent);

		ret = enumeration->entries->len ? 0 : -1;
	}
end:
	return ret;
}

struct bt_ctf_field_type *bt_ctf_field_type_integer_create(unsigned int size)
{
	struct bt_ctf_field_type_integer *integer =
		g_new0(struct bt_ctf_field_type_integer, 1);

	if (!integer || size > 64) {
		return NULL;
	}

	integer->parent.declaration = &integer->declaration.p;
	integer->parent.declaration->id = CTF_TYPE_INTEGER;
	integer->declaration.len = size;
	integer->declaration.base = BT_CTF_INTEGER_BASE_DECIMAL;
	integer->declaration.encoding = CTF_STRING_NONE;
	bt_ctf_field_type_init(&integer->parent);
	return &integer->parent;
}

int bt_ctf_field_type_integer_set_signed(struct bt_ctf_field_type *type,
		int is_signed)
{
	int ret = 0;
	struct bt_ctf_field_type_integer *integer;

	if (!type || type->frozen ||
		type->declaration->id != CTF_TYPE_INTEGER) {
		ret = -1;
		goto end;
	}

	integer = container_of(type, struct bt_ctf_field_type_integer, parent);
	if (is_signed && integer->declaration.len <= 1) {
		ret = -1;
		goto end;
	}

	integer->declaration.signedness = !!is_signed;
end:
	return ret;
}

int bt_ctf_field_type_integer_set_base(struct bt_ctf_field_type *type,
		enum bt_ctf_integer_base base)
{
	int ret = 0;

	if (!type || type->frozen ||
		type->declaration->id != CTF_TYPE_INTEGER) {
		ret = -1;
		goto end;
	}

	switch (base) {
	case BT_CTF_INTEGER_BASE_BINARY:
	case BT_CTF_INTEGER_BASE_OCTAL:
	case BT_CTF_INTEGER_BASE_DECIMAL:
	case BT_CTF_INTEGER_BASE_HEXADECIMAL:
	{
		struct bt_ctf_field_type_integer *integer = container_of(type,
			struct bt_ctf_field_type_integer, parent);
		integer->declaration.base = base;
		break;
	}
	default:
		ret = -1;
	}
end:
	return ret;
}

int bt_ctf_field_type_integer_set_encoding(struct bt_ctf_field_type *type,
		enum ctf_string_encoding encoding)
{
	int ret = 0;
	struct bt_ctf_field_type_integer *integer;

	if (!type || type->frozen ||
		(type->declaration->id != CTF_TYPE_INTEGER) ||
		(encoding < CTF_STRING_NONE) ||
		(encoding >= CTF_STRING_UNKNOWN)) {
		ret = -1;
		goto end;
	}

	integer = container_of(type, struct bt_ctf_field_type_integer, parent);
	integer->declaration.encoding = encoding;
end:
	return ret;
}

struct bt_ctf_field_type *bt_ctf_field_type_enumeration_create(
		struct bt_ctf_field_type *integer_container_type)
{
	struct bt_ctf_field_type_enumeration *enumeration = NULL;

	if (!integer_container_type) {
		goto error;
	}

	enumeration = g_new0(struct bt_ctf_field_type_enumeration, 1);
	if (!enumeration) {
		goto error;
	}

	enumeration->parent.declaration = &enumeration->declaration.p;
	enumeration->parent.declaration->id = CTF_TYPE_ENUM;
	bt_ctf_field_type_get(integer_container_type);
	enumeration->container = integer_container_type;
	enumeration->entries = g_ptr_array_new_with_free_func(
		(GDestroyNotify)destroy_enumeration_mapping);
	bt_ctf_field_type_init(&enumeration->parent);
	return &enumeration->parent;
error:
	g_free(enumeration);
	return NULL;
}

int bt_ctf_field_type_enumeration_add_mapping(
		struct bt_ctf_field_type *type, const char *string,
		int64_t range_start, int64_t range_end)
{
	int ret = 0;
	GQuark mapping_name;
	struct enumeration_mapping *mapping;
	struct bt_ctf_field_type_enumeration *enumeration;
	struct range_overlap_query query;
	char *escaped_string;

	if (!type || (type->declaration->id != CTF_TYPE_ENUM) ||
		type->frozen ||
		(range_end < range_start)) {
		ret = -1;
		goto end;
	}

	if (!string || strlen(string) == 0) {
		ret = -1;
		goto end;
	}

	escaped_string = g_strescape(string, NULL);
	if (!escaped_string) {
		ret = -1;
		goto end;
	}

	mapping_name = g_quark_from_string(escaped_string);
	query = (struct range_overlap_query) { .range_start = range_start,
		.range_end = range_end,
		.mapping_name = mapping_name,
		.overlaps = 0 };
	enumeration = container_of(type, struct bt_ctf_field_type_enumeration,
		parent);

	/* Check that the range does not overlap with one already present */
	g_ptr_array_foreach(enumeration->entries, check_ranges_overlap, &query);
	if (query.overlaps) {
		ret = -1;
		goto error_free;
	}

	mapping = g_new(struct enumeration_mapping, 1);
	if (!mapping) {
		ret = -1;
		goto error_free;
	}

	*mapping = (struct enumeration_mapping) {.range_start = range_start,
		.range_end = range_end, .string = mapping_name};
	g_ptr_array_add(enumeration->entries, mapping);
error_free:
	free(escaped_string);
end:
	return ret;
}

struct bt_ctf_field_type *bt_ctf_field_type_floating_point_create(void)
{
	struct bt_ctf_field_type_floating_point *floating_point =
		g_new0(struct bt_ctf_field_type_floating_point, 1);

	if (!floating_point) {
		goto end;
	}

	floating_point->declaration.sign = &floating_point->sign;
	floating_point->declaration.mantissa = &floating_point->mantissa;
	floating_point->declaration.exp = &floating_point->exp;
	floating_point->sign.len = 1;
	floating_point->parent.declaration = &floating_point->declaration.p;
	floating_point->parent.declaration->id = CTF_TYPE_FLOAT;
	floating_point->declaration.exp->len =
		sizeof(float) * CHAR_BIT - FLT_MANT_DIG;
	floating_point->declaration.mantissa->len = FLT_MANT_DIG - 1;
	floating_point->sign.p.alignment = 1;
	floating_point->mantissa.p.alignment = 1;
	floating_point->exp.p.alignment = 1;

	bt_ctf_field_type_init(&floating_point->parent);
end:
	return floating_point ? &floating_point->parent : NULL;
}

int bt_ctf_field_type_floating_point_set_exponent_digits(
		struct bt_ctf_field_type *type,
		unsigned int exponent_digits)
{
	int ret = 0;
	struct bt_ctf_field_type_floating_point *floating_point;

	if (!type || type->frozen ||
		(type->declaration->id != CTF_TYPE_FLOAT)) {
		ret = -1;
		goto end;
	}

	floating_point = container_of(type,
		struct bt_ctf_field_type_floating_point, parent);
	if ((exponent_digits != sizeof(float) * CHAR_BIT - FLT_MANT_DIG) &&
		(exponent_digits != sizeof(double) * CHAR_BIT - DBL_MANT_DIG) &&
		(exponent_digits !=
			sizeof(long double) * CHAR_BIT - LDBL_MANT_DIG)) {
		ret = -1;
		goto end;
	}

	floating_point->declaration.exp->len = exponent_digits;
end:
	return ret;
}

int bt_ctf_field_type_floating_point_set_mantissa_digits(
		struct bt_ctf_field_type *type,
		unsigned int mantissa_digits)
{
	int ret = 0;
	struct bt_ctf_field_type_floating_point *floating_point;

	if (!type || type->frozen ||
		(type->declaration->id != CTF_TYPE_FLOAT)) {
		ret = -1;
		goto end;
	}

	floating_point = container_of(type,
		struct bt_ctf_field_type_floating_point, parent);

	if ((mantissa_digits != FLT_MANT_DIG) &&
		(mantissa_digits != DBL_MANT_DIG) &&
		(mantissa_digits != LDBL_MANT_DIG)) {
		ret = -1;
		goto end;
	}

	floating_point->declaration.mantissa->len = mantissa_digits - 1;
end:
	return ret;
}

struct bt_ctf_field_type *bt_ctf_field_type_structure_create(void)
{
	struct bt_ctf_field_type_structure *structure =
		g_new0(struct bt_ctf_field_type_structure, 1);

	if (!structure) {
		goto error;
	}

	structure->parent.declaration = &structure->declaration.p;
	structure->parent.declaration->id = CTF_TYPE_STRUCT;
	bt_ctf_field_type_init(&structure->parent);
	structure->fields = g_ptr_array_new_with_free_func(
		(GDestroyNotify)destroy_structure_field);
	structure->field_name_to_index = g_hash_table_new(NULL, NULL);
	return &structure->parent;
error:
	return NULL;
}

int bt_ctf_field_type_structure_add_field(struct bt_ctf_field_type *type,
		struct bt_ctf_field_type *field_type,
		const char *field_name)
{
	int ret = 0;
	struct bt_ctf_field_type_structure *structure;

	if (!type || !field_type || type->frozen ||
		validate_identifier(field_name) ||
		(type->declaration->id != CTF_TYPE_STRUCT) ||
		bt_ctf_field_type_validate(field_type)) {
		ret = -1;
		goto end;
	}

	structure = container_of(type,
		struct bt_ctf_field_type_structure, parent);
	if (add_structure_field(structure->fields,
		structure->field_name_to_index, field_type, field_name)) {
		ret = -1;
		goto end;
	}

	if (type->declaration->alignment < field_type->declaration->alignment) {
		type->declaration->alignment =
			field_type->declaration->alignment;
	}
end:
	return ret;
}

struct bt_ctf_field_type *bt_ctf_field_type_variant_create(
	struct bt_ctf_field_type *enum_tag, const char *tag_name)
{
	struct bt_ctf_field_type_variant *variant = NULL;

	if (!enum_tag || validate_identifier(tag_name) ||
		(enum_tag->declaration->id != CTF_TYPE_ENUM)) {
		goto error;
	}

	variant = g_new0(struct bt_ctf_field_type_variant, 1);
	if (!variant) {
		goto error;
	}

	variant->parent.declaration = &variant->declaration.p;
	variant->parent.declaration->id = CTF_TYPE_VARIANT;
	variant->tag_name = g_string_new(tag_name);
	bt_ctf_field_type_init(&variant->parent);
	variant->field_name_to_index = g_hash_table_new(NULL, NULL);
	variant->fields = g_ptr_array_new_with_free_func(
		(GDestroyNotify)destroy_structure_field);
	bt_ctf_field_type_get(enum_tag);
	variant->tag = container_of(enum_tag,
		struct bt_ctf_field_type_enumeration, parent);
	return &variant->parent;
error:
	return NULL;
}

int bt_ctf_field_type_variant_add_field(struct bt_ctf_field_type *type,
		struct bt_ctf_field_type *field_type,
		const char *field_name)
{
	size_t i;
	int ret = 0;
	int name_found = 0;
	struct bt_ctf_field_type_variant *variant;
	GQuark field_name_quark = g_quark_from_string(field_name);

	if (!type || !field_type || type->frozen ||
		validate_identifier(field_name) ||
		(type->declaration->id != CTF_TYPE_VARIANT) ||
		bt_ctf_field_type_validate(field_type)) {
		ret = -1;
		goto end;
	}

	variant = container_of(type, struct bt_ctf_field_type_variant, parent);
	/* Make sure this name is present in the enum tag */
	for (i = 0; i < variant->tag->entries->len; i++) {
		struct enumeration_mapping *mapping =
			g_ptr_array_index(variant->tag->entries, i);

		if (mapping->string == field_name_quark) {
			name_found = 1;
			break;
		}
	}

	if (!name_found || add_structure_field(variant->fields,
		variant->field_name_to_index, field_type, field_name)) {
		ret = -1;
		goto end;
	}
end:
	return ret;
}

struct bt_ctf_field_type *bt_ctf_field_type_array_create(
		struct bt_ctf_field_type *element_type,
		unsigned int length)
{
	struct bt_ctf_field_type_array *array = NULL;

	if (!element_type || length == 0 ||
		bt_ctf_field_type_validate(element_type)) {
		goto error;
	}

	array = g_new0(struct bt_ctf_field_type_array, 1);
	if (!array) {
		goto error;
	}

	array->parent.declaration = &array->declaration.p;
	array->parent.declaration->id = CTF_TYPE_ARRAY;
	bt_ctf_field_type_init(&array->parent);
	bt_ctf_field_type_get(element_type);
	array->element_type = element_type;
	array->length = length;
	array->parent.declaration->alignment =
		element_type->declaration->alignment;
	return &array->parent;
error:
	return NULL;
}

struct bt_ctf_field_type *bt_ctf_field_type_sequence_create(
		struct bt_ctf_field_type *element_type,
		const char *length_field_name)
{
	struct bt_ctf_field_type_sequence *sequence = NULL;

	if (!element_type || validate_identifier(length_field_name) ||
		bt_ctf_field_type_validate(element_type)) {
		goto error;
	}

	sequence = g_new0(struct bt_ctf_field_type_sequence, 1);
	if (!sequence) {
		goto error;
	}

	sequence->parent.declaration = &sequence->declaration.p;
	sequence->parent.declaration->id = CTF_TYPE_SEQUENCE;
	bt_ctf_field_type_init(&sequence->parent);
	bt_ctf_field_type_get(element_type);
	sequence->element_type = element_type;
	sequence->length_field_name = g_string_new(length_field_name);
	sequence->parent.declaration->alignment =
		element_type->declaration->alignment;
	return &sequence->parent;
error:
	return NULL;
}

struct bt_ctf_field_type *bt_ctf_field_type_string_create(void)
{
	struct bt_ctf_field_type_string *string =
		g_new0(struct bt_ctf_field_type_string, 1);

	if (!string) {
		return NULL;
	}

	string->parent.declaration = &string->declaration.p;
	string->parent.declaration->id = CTF_TYPE_STRING;
	bt_ctf_field_type_init(&string->parent);
	string->declaration.encoding = CTF_STRING_UTF8;
	string->parent.declaration->alignment = CHAR_BIT;
	return &string->parent;
}

int bt_ctf_field_type_string_set_encoding(
		struct bt_ctf_field_type *type,
		enum ctf_string_encoding encoding)
{
	int ret = 0;
	struct bt_ctf_field_type_string *string;

	if (!type || type->declaration->id != CTF_TYPE_STRING ||
		(encoding != CTF_STRING_UTF8 &&
		encoding != CTF_STRING_ASCII)) {
		ret = -1;
		goto end;
	}

	string = container_of(type, struct bt_ctf_field_type_string, parent);
	string->declaration.encoding = encoding;
end:
	return ret;
}

int bt_ctf_field_type_set_alignment(struct bt_ctf_field_type *type,
		unsigned int alignment)
{
	int ret = 0;

	/* Alignment must be bit-aligned (1) or byte aligned */
	if (!type || type->frozen || (alignment != 1 && (alignment & 0x7))) {
		ret = -1;
		goto end;
	}

	if (type->declaration->id == CTF_TYPE_STRING &&
		alignment != CHAR_BIT) {
		ret = -1;
		goto end;
	}

	type->declaration->alignment = alignment;
	ret = 0;
end:
	return ret;
}

int bt_ctf_field_type_set_byte_order(struct bt_ctf_field_type *type,
		enum bt_ctf_byte_order byte_order)
{
	int ret = 0;
	int internal_byte_order;
	enum ctf_type_id type_id;

	if (!type || type->frozen) {
		ret = -1;
		goto end;
	}

	type_id = type->declaration->id;
	switch (byte_order) {
	case BT_CTF_BYTE_ORDER_NATIVE:
		internal_byte_order = (G_BYTE_ORDER == G_LITTLE_ENDIAN ?
			LITTLE_ENDIAN : BIG_ENDIAN);
		break;
	case BT_CTF_BYTE_ORDER_LITTLE_ENDIAN:
		internal_byte_order = LITTLE_ENDIAN;
		break;
	case BT_CTF_BYTE_ORDER_BIG_ENDIAN:
	case BT_CTF_BYTE_ORDER_NETWORK:
		internal_byte_order = BIG_ENDIAN;
		break;
	default:
		ret = -1;
		goto end;
	}

	if (set_byte_order_funcs[type_id]) {
		set_byte_order_funcs[type_id](type, internal_byte_order);
	}
end:
	return ret;
}

void bt_ctf_field_type_get(struct bt_ctf_field_type *type)
{
	if (!type) {
		return;
	}

	bt_ctf_ref_get(&type->ref_count);
}

void bt_ctf_field_type_put(struct bt_ctf_field_type *type)
{
	enum ctf_type_id type_id;

	if (!type) {
		return;
	}

	type_id = type->declaration->id;
	assert(type_id > CTF_TYPE_UNKNOWN && type_id < NR_CTF_TYPES);
	bt_ctf_ref_put(&type->ref_count, type_destroy_funcs[type_id]);
}

BT_HIDDEN
void bt_ctf_field_type_freeze(struct bt_ctf_field_type *type)
{
	if (!type) {
		return;
	}

	type->freeze(type);
}

BT_HIDDEN
enum ctf_type_id bt_ctf_field_type_get_type_id(
		struct bt_ctf_field_type *type)
{
	if (!type) {
		return CTF_TYPE_UNKNOWN;
	}

	return type->declaration->id;
}

BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_field_type_structure_get_type(
		struct bt_ctf_field_type_structure *structure,
		const char *name)
{
	struct bt_ctf_field_type *type = NULL;
	struct structure_field *field;
	GQuark name_quark = g_quark_try_string(name);
	size_t index;

	if (!name_quark) {
		goto end;
	}

	if (!g_hash_table_lookup_extended(structure->field_name_to_index,
		GUINT_TO_POINTER(name_quark), NULL, (gpointer *)&index)) {
		goto end;
	}

	field = structure->fields->pdata[index];
	type = field->type;
end:
	return type;
}

BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_field_type_array_get_element_type(
		struct bt_ctf_field_type_array *array)
{
	assert(array);
	return array->element_type;
}

BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_field_type_sequence_get_element_type(
		struct bt_ctf_field_type_sequence *sequence)
{
	assert(sequence);
	return sequence->element_type;
}

BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_field_type_variant_get_field_type(
		struct bt_ctf_field_type_variant *variant,
		int64_t tag_value)
{
	struct bt_ctf_field_type *type = NULL;
	GQuark field_name_quark;
	gpointer index;
	struct structure_field *field_entry;
	struct range_overlap_query query = {.range_start = tag_value,
		.range_end = tag_value, .mapping_name = 0, .overlaps = 0};

	g_ptr_array_foreach(variant->tag->entries, check_ranges_overlap,
		&query);
	if (!query.overlaps) {
		goto end;
	}

	field_name_quark = query.mapping_name;
	if (!g_hash_table_lookup_extended(variant->field_name_to_index,
		GUINT_TO_POINTER(field_name_quark), NULL, &index)) {
		goto end;
	}

	field_entry = g_ptr_array_index(variant->fields, (size_t)index);
	type = field_entry->type;
end:
	return type;
}

BT_HIDDEN
int bt_ctf_field_type_serialize(struct bt_ctf_field_type *type,
		struct metadata_context *context)
{
	int ret;

	if (!type || !context) {
		ret = -1;
		goto end;
	}

	ret = type->serialize(type, context);
end:
	return ret;
}

static
void bt_ctf_field_type_integer_destroy(struct bt_ctf_ref *ref)
{
	struct bt_ctf_field_type_integer *integer;

	if (!ref) {
		return;
	}

	integer = container_of(
		container_of(ref, struct bt_ctf_field_type, ref_count),
		struct bt_ctf_field_type_integer, parent);
	g_free(integer);
}

static
void bt_ctf_field_type_enumeration_destroy(struct bt_ctf_ref *ref)
{
	struct bt_ctf_field_type_enumeration *enumeration;

	if (!ref) {
		return;
	}

	enumeration = container_of(
		container_of(ref, struct bt_ctf_field_type, ref_count),
		struct bt_ctf_field_type_enumeration, parent);
	g_ptr_array_free(enumeration->entries, TRUE);
	bt_ctf_field_type_put(enumeration->container);
	g_free(enumeration);
}

static
void bt_ctf_field_type_floating_point_destroy(struct bt_ctf_ref *ref)
{
	struct bt_ctf_field_type_floating_point *floating_point;

	if (!ref) {
		return;
	}

	floating_point = container_of(
		container_of(ref, struct bt_ctf_field_type, ref_count),
		struct bt_ctf_field_type_floating_point, parent);
	g_free(floating_point);
}

static
void bt_ctf_field_type_structure_destroy(struct bt_ctf_ref *ref)
{
	struct bt_ctf_field_type_structure *structure;

	if (!ref) {
		return;
	}

	structure = container_of(
		container_of(ref, struct bt_ctf_field_type, ref_count),
		struct bt_ctf_field_type_structure, parent);
	g_ptr_array_free(structure->fields, TRUE);
	g_hash_table_destroy(structure->field_name_to_index);
	g_free(structure);
}

static
void bt_ctf_field_type_variant_destroy(struct bt_ctf_ref *ref)
{
	struct bt_ctf_field_type_variant *variant;

	if (!ref) {
		return;
	}

	variant = container_of(
		container_of(ref, struct bt_ctf_field_type, ref_count),
		struct bt_ctf_field_type_variant, parent);
	g_ptr_array_free(variant->fields, TRUE);
	g_hash_table_destroy(variant->field_name_to_index);
	g_string_free(variant->tag_name, TRUE);
	bt_ctf_field_type_put(&variant->tag->parent);
	g_free(variant);
}

static
void bt_ctf_field_type_array_destroy(struct bt_ctf_ref *ref)
{
	struct bt_ctf_field_type_array *array;

	if (!ref) {
		return;
	}

	array = container_of(
		container_of(ref, struct bt_ctf_field_type, ref_count),
		struct bt_ctf_field_type_array, parent);
	bt_ctf_field_type_put(array->element_type);
	g_free(array);
}

static
void bt_ctf_field_type_sequence_destroy(struct bt_ctf_ref *ref)
{
	struct bt_ctf_field_type_sequence *sequence;

	if (!ref) {
		return;
	}

	sequence = container_of(
		container_of(ref, struct bt_ctf_field_type, ref_count),
		struct bt_ctf_field_type_sequence, parent);
	bt_ctf_field_type_put(sequence->element_type);
	g_string_free(sequence->length_field_name, TRUE);
	g_free(sequence);
}

static
void bt_ctf_field_type_string_destroy(struct bt_ctf_ref *ref)
{
	struct bt_ctf_field_type_string *string;

	if (!ref) {
		return;
	}

	string = container_of(
		container_of(ref, struct bt_ctf_field_type, ref_count),
		struct bt_ctf_field_type_string, parent);
	g_free(string);
}

static
void generic_field_type_freeze(struct bt_ctf_field_type *type)
{
	type->frozen = 1;
}

static
void bt_ctf_field_type_enumeration_freeze(struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_type_enumeration *enumeration_type = container_of(
		type, struct bt_ctf_field_type_enumeration, parent);

	generic_field_type_freeze(type);
	bt_ctf_field_type_freeze(enumeration_type->container);
}

static
void freeze_structure_field(struct structure_field *field)
{
	bt_ctf_field_type_freeze(field->type);
}

static
void bt_ctf_field_type_structure_freeze(struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_type_structure *structure_type = container_of(
		type, struct bt_ctf_field_type_structure, parent);

	generic_field_type_freeze(type);
	g_ptr_array_foreach(structure_type->fields, (GFunc)freeze_structure_field,
		NULL);
}

static
void bt_ctf_field_type_variant_freeze(struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_type_variant *variant_type = container_of(
		type, struct bt_ctf_field_type_variant, parent);

	generic_field_type_freeze(type);
	g_ptr_array_foreach(variant_type->fields, (GFunc)freeze_structure_field,
		NULL);
}

static
void bt_ctf_field_type_array_freeze(struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_type_array *array_type = container_of(
		type, struct bt_ctf_field_type_array, parent);

	generic_field_type_freeze(type);
	bt_ctf_field_type_freeze(array_type->element_type);
}

static
void bt_ctf_field_type_sequence_freeze(struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_type_sequence *sequence_type = container_of(
		type, struct bt_ctf_field_type_sequence, parent);

	generic_field_type_freeze(type);
	bt_ctf_field_type_freeze(sequence_type->element_type);
}

static
const char *get_encoding_string(enum ctf_string_encoding encoding)
{
	const char *encoding_string;

	switch (encoding) {
	case CTF_STRING_NONE:
		encoding_string = "none";
		break;
	case CTF_STRING_ASCII:
		encoding_string = "ASCII";
		break;
	case CTF_STRING_UTF8:
		encoding_string = "UTF8";
		break;
	default:
		encoding_string = "unknown";
		break;
	}

	return encoding_string;
}

static
const char *get_integer_base_string(enum bt_ctf_integer_base base)
{
	const char *base_string;

	switch (base) {
	case BT_CTF_INTEGER_BASE_DECIMAL:
		base_string = "decimal";
		break;
	case BT_CTF_INTEGER_BASE_HEXADECIMAL:
		base_string = "hexadecimal";
		break;
	case BT_CTF_INTEGER_BASE_OCTAL:
		base_string = "octal";
		break;
	case BT_CTF_INTEGER_BASE_BINARY:
		base_string = "binary";
		break;
	default:
		base_string = "unknown";
		break;
	}

	return base_string;
}

static
int bt_ctf_field_type_integer_serialize(struct bt_ctf_field_type *type,
		struct metadata_context *context)
{
	struct bt_ctf_field_type_integer *integer = container_of(type,
		struct bt_ctf_field_type_integer, parent);

	g_string_append_printf(context->string,
		"integer { size = %zu; align = %zu; signed = %s; encoding = %s; base = %s; byte_order = %s; }",
		integer->declaration.len, type->declaration->alignment,
		(integer->declaration.signedness ? "true" : "false"),
		get_encoding_string(integer->declaration.encoding),
		get_integer_base_string(integer->declaration.base),
		get_byte_order_string(integer->declaration.byte_order));
	return 0;
}

static
int bt_ctf_field_type_enumeration_serialize(struct bt_ctf_field_type *type,
		struct metadata_context *context)
{
	size_t entry;
	int ret;
	struct bt_ctf_field_type_enumeration *enumeration = container_of(type,
		struct bt_ctf_field_type_enumeration, parent);

	ret = bt_ctf_field_type_validate(type);
	if (ret) {
		goto end;
	}

	g_string_append(context->string, "enum : ");
	ret = bt_ctf_field_type_serialize(enumeration->container, context);
	if (ret) {
		goto end;
	}

	g_string_append(context->string, " { ");
	for (entry = 0; entry < enumeration->entries->len; entry++) {
		struct enumeration_mapping *mapping =
			enumeration->entries->pdata[entry];

		if (mapping->range_start == mapping->range_end) {
			g_string_append_printf(context->string,
				"\"%s\" = %" PRId64,
				g_quark_to_string(mapping->string),
				mapping->range_start);
		} else {
			g_string_append_printf(context->string,
				"\"%s\" = %" PRId64 " ... %" PRId64,
				g_quark_to_string(mapping->string),
				mapping->range_start, mapping->range_end);
		}

		g_string_append(context->string,
			((entry != (enumeration->entries->len - 1)) ?
			", " : " }"));
	}

	if (context->field_name->len) {
		g_string_append_printf(context->string, " %s",
			context->field_name->str);
		g_string_assign(context->field_name, "");
	}
end:
	return ret;
}

static
int bt_ctf_field_type_floating_point_serialize(struct bt_ctf_field_type *type,
		struct metadata_context *context)
{
	struct bt_ctf_field_type_floating_point *floating_point = container_of(
		type, struct bt_ctf_field_type_floating_point, parent);

	g_string_append_printf(context->string,
		"floating_point { exp_dig = %zu; mant_dig = %zu; byte_order = %s; align = %zu; }",
		floating_point->declaration.exp->len,
		floating_point->declaration.mantissa->len + 1,
		get_byte_order_string(floating_point->declaration.byte_order),
		type->declaration->alignment);
	return 0;
}

static
int bt_ctf_field_type_structure_serialize(struct bt_ctf_field_type *type,
		struct metadata_context *context)
{
	size_t i;
	unsigned int indent;
	int ret = 0;
	struct bt_ctf_field_type_structure *structure = container_of(type,
		struct bt_ctf_field_type_structure, parent);
	GString *structure_field_name = context->field_name;

	context->field_name = g_string_new("");

	context->current_indentation_level++;
	g_string_append(context->string, "struct {\n");

	for (i = 0; i < structure->fields->len; i++) {
		struct structure_field *field;

		for (indent = 0; indent < context->current_indentation_level;
			indent++) {
			g_string_append_c(context->string, '\t');
		}

		field = structure->fields->pdata[i];
		g_string_assign(context->field_name,
			g_quark_to_string(field->name));
		ret = bt_ctf_field_type_serialize(field->type, context);
		if (ret) {
			goto end;
		}

		if (context->field_name->len) {
			g_string_append_printf(context->string, " %s",
				context->field_name->str);
		}
		g_string_append(context->string, ";\n");
	}

	context->current_indentation_level--;
	for (indent = 0; indent < context->current_indentation_level;
		indent++) {
		g_string_append_c(context->string, '\t');
	}

	g_string_append_printf(context->string, "} align(%zu)",
		 type->declaration->alignment);
end:
	g_string_free(context->field_name, TRUE);
	context->field_name = structure_field_name;
	return ret;
}

static
int bt_ctf_field_type_variant_serialize(struct bt_ctf_field_type *type,
		struct metadata_context *context)
{
	size_t i;
	unsigned int indent;
	int ret = 0;
	struct bt_ctf_field_type_variant *variant = container_of(
		type, struct bt_ctf_field_type_variant, parent);
	GString *variant_field_name = context->field_name;

	context->field_name = g_string_new("");
	g_string_append_printf(context->string,
		"variant <%s> {\n", variant->tag_name->str);
	context->current_indentation_level++;
	for (i = 0; i < variant->fields->len; i++) {
		struct structure_field *field = variant->fields->pdata[i];

		g_string_assign(context->field_name,
			g_quark_to_string(field->name));
		for (indent = 0; indent < context->current_indentation_level;
			indent++) {
			g_string_append_c(context->string, '\t');
		}

		g_string_assign(context->field_name,
			g_quark_to_string(field->name));
		ret = bt_ctf_field_type_serialize(field->type, context);
		if (ret) {
			goto end;
		}

		if (context->field_name->len) {
			g_string_append_printf(context->string, " %s;",
				context->field_name->str);
		}

		g_string_append_c(context->string, '\n');
	}

	context->current_indentation_level--;
	for (indent = 0; indent < context->current_indentation_level;
		indent++) {
		g_string_append_c(context->string, '\t');
	}

	g_string_append(context->string, "}");
end:
	g_string_free(context->field_name, TRUE);
	context->field_name = variant_field_name;
	return ret;
}

static
int bt_ctf_field_type_array_serialize(struct bt_ctf_field_type *type,
		struct metadata_context *context)
{
	int ret = 0;
	struct bt_ctf_field_type_array *array = container_of(type,
		struct bt_ctf_field_type_array, parent);

	ret = bt_ctf_field_type_serialize(array->element_type, context);
	if (ret) {
		goto end;
	}

	if (context->field_name->len) {
		g_string_append_printf(context->string, " %s[%u]",
			context->field_name->str, array->length);
		g_string_assign(context->field_name, "");
	} else {
		g_string_append_printf(context->string, "[%u]", array->length);
	}
end:
	return ret;
}

static
int bt_ctf_field_type_sequence_serialize(struct bt_ctf_field_type *type,
		struct metadata_context *context)
{
	int ret = 0;
	struct bt_ctf_field_type_sequence *sequence = container_of(
		type, struct bt_ctf_field_type_sequence, parent);

	ret = bt_ctf_field_type_serialize(sequence->element_type, context);
	if (ret) {
		goto end;
	}

	if (context->field_name->len) {
		g_string_append_printf(context->string, " %s[%s]",
			context->field_name->str,
			sequence->length_field_name->str);
		g_string_assign(context->field_name, "");
	} else {
		g_string_append_printf(context->string, "[%s]",
			sequence->length_field_name->str);
	}
end:
	return ret;
}

static
int bt_ctf_field_type_string_serialize(struct bt_ctf_field_type *type,
		struct metadata_context *context)
{
	struct bt_ctf_field_type_string *string = container_of(
		type, struct bt_ctf_field_type_string, parent);

	g_string_append_printf(context->string,
		"string { encoding = %s; }",
		get_encoding_string(string->declaration.encoding));
	return 0;
}

static
void bt_ctf_field_type_integer_set_byte_order(struct bt_ctf_field_type *type,
		int byte_order)
{
	struct bt_ctf_field_type_integer *integer_type = container_of(type,
		struct bt_ctf_field_type_integer, parent);

	integer_type->declaration.byte_order = byte_order;
}

static
void bt_ctf_field_type_floating_point_set_byte_order(
		struct bt_ctf_field_type *type, int byte_order)
{
	struct bt_ctf_field_type_floating_point *floating_point_type =
		container_of(type, struct bt_ctf_field_type_floating_point,
		parent);

	floating_point_type->declaration.byte_order = byte_order;
	floating_point_type->sign.byte_order = byte_order;
	floating_point_type->mantissa.byte_order = byte_order;
	floating_point_type->exp.byte_order = byte_order;
}
