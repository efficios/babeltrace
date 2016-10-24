/*
 * field-types.c
 *
 * Babeltrace CTF IR - Event Types
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

#include <babeltrace/ctf-ir/field-types-internal.h>
#include <babeltrace/ctf-ir/field-path-internal.h>
#include <babeltrace/ctf-ir/fields-internal.h>
#include <babeltrace/ctf-ir/utils.h>
#include <babeltrace/ref.h>
#include <babeltrace/ctf-ir/clock.h>
#include <babeltrace/ctf-ir/clock-internal.h>
#include <babeltrace/ctf-writer/writer-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/compiler.h>
#include <babeltrace/endian.h>
#include <float.h>
#include <inttypes.h>
#include <stdlib.h>

struct range_overlap_query {
	union {
		uint64_t _unsigned;
		int64_t _signed;
	} range_start;

	union {
		uint64_t _unsigned;
		int64_t _signed;
	} range_end;
	int overlaps;
	GQuark mapping_name;
};

static
void bt_ctf_field_type_destroy(struct bt_object *);
static
void bt_ctf_field_type_integer_destroy(struct bt_ctf_field_type *);
static
void bt_ctf_field_type_enumeration_destroy(struct bt_ctf_field_type *);
static
void bt_ctf_field_type_floating_point_destroy(struct bt_ctf_field_type *);
static
void bt_ctf_field_type_structure_destroy(struct bt_ctf_field_type *);
static
void bt_ctf_field_type_variant_destroy(struct bt_ctf_field_type *);
static
void bt_ctf_field_type_array_destroy(struct bt_ctf_field_type *);
static
void bt_ctf_field_type_sequence_destroy(struct bt_ctf_field_type *);
static
void bt_ctf_field_type_string_destroy(struct bt_ctf_field_type *);

static
void (* const type_destroy_funcs[])(struct bt_ctf_field_type *) = {
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
void bt_ctf_field_type_integer_freeze(struct bt_ctf_field_type *);
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
	[CTF_TYPE_INTEGER] = bt_ctf_field_type_integer_freeze,
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
		int byte_order, int set_native);
static
void bt_ctf_field_type_enumeration_set_byte_order(struct bt_ctf_field_type *,
		int byte_order, int set_native);
static
void bt_ctf_field_type_floating_point_set_byte_order(
		struct bt_ctf_field_type *, int byte_order, int set_native);
static
void bt_ctf_field_type_structure_set_byte_order(struct bt_ctf_field_type *,
		int byte_order, int set_native);
static
void bt_ctf_field_type_variant_set_byte_order(struct bt_ctf_field_type *,
		int byte_order, int set_native);
static
void bt_ctf_field_type_array_set_byte_order(struct bt_ctf_field_type *,
		int byte_order, int set_native);
static
void bt_ctf_field_type_sequence_set_byte_order(struct bt_ctf_field_type *,
		int byte_order, int set_native);

/* The set_native flag only set the byte order if it is set to native */
static
void (* const set_byte_order_funcs[])(struct bt_ctf_field_type *,
		int byte_order, int set_native) = {
	[CTF_TYPE_INTEGER] = bt_ctf_field_type_integer_set_byte_order,
	[CTF_TYPE_ENUM] =
		bt_ctf_field_type_enumeration_set_byte_order,
	[CTF_TYPE_FLOAT] =
		bt_ctf_field_type_floating_point_set_byte_order,
	[CTF_TYPE_STRUCT] =
		bt_ctf_field_type_structure_set_byte_order,
	[CTF_TYPE_VARIANT] = bt_ctf_field_type_variant_set_byte_order,
	[CTF_TYPE_ARRAY] = bt_ctf_field_type_array_set_byte_order,
	[CTF_TYPE_SEQUENCE] = bt_ctf_field_type_sequence_set_byte_order,
	[CTF_TYPE_STRING] = NULL,
};

static
struct bt_ctf_field_type *bt_ctf_field_type_integer_copy(
		struct bt_ctf_field_type *);
static
struct bt_ctf_field_type *bt_ctf_field_type_enumeration_copy(
		struct bt_ctf_field_type *);
static
struct bt_ctf_field_type *bt_ctf_field_type_floating_point_copy(
		struct bt_ctf_field_type *);
static
struct bt_ctf_field_type *bt_ctf_field_type_structure_copy(
		struct bt_ctf_field_type *);
static
struct bt_ctf_field_type *bt_ctf_field_type_variant_copy(
		struct bt_ctf_field_type *);
static
struct bt_ctf_field_type *bt_ctf_field_type_array_copy(
		struct bt_ctf_field_type *);
static
struct bt_ctf_field_type *bt_ctf_field_type_sequence_copy(
		struct bt_ctf_field_type *);
static
struct bt_ctf_field_type *bt_ctf_field_type_string_copy(
		struct bt_ctf_field_type *);

static
struct bt_ctf_field_type *(* const type_copy_funcs[])(
		struct bt_ctf_field_type *) = {
	[CTF_TYPE_INTEGER] = bt_ctf_field_type_integer_copy,
	[CTF_TYPE_ENUM] = bt_ctf_field_type_enumeration_copy,
	[CTF_TYPE_FLOAT] = bt_ctf_field_type_floating_point_copy,
	[CTF_TYPE_STRUCT] = bt_ctf_field_type_structure_copy,
	[CTF_TYPE_VARIANT] = bt_ctf_field_type_variant_copy,
	[CTF_TYPE_ARRAY] = bt_ctf_field_type_array_copy,
	[CTF_TYPE_SEQUENCE] = bt_ctf_field_type_sequence_copy,
	[CTF_TYPE_STRING] = bt_ctf_field_type_string_copy,
};

static
int bt_ctf_field_type_integer_compare(struct bt_ctf_field_type *,
		struct bt_ctf_field_type *);
static
int bt_ctf_field_type_floating_point_compare(struct bt_ctf_field_type *,
		struct bt_ctf_field_type *);
static
int bt_ctf_field_type_enumeration_compare(struct bt_ctf_field_type *,
		struct bt_ctf_field_type *);
static
int bt_ctf_field_type_string_compare(struct bt_ctf_field_type *,
		struct bt_ctf_field_type *);
static
int bt_ctf_field_type_structure_compare(struct bt_ctf_field_type *,
		struct bt_ctf_field_type *);
static
int bt_ctf_field_type_variant_compare(struct bt_ctf_field_type *,
		struct bt_ctf_field_type *);
static
int bt_ctf_field_type_array_compare(struct bt_ctf_field_type *,
		struct bt_ctf_field_type *);
static
int bt_ctf_field_type_sequence_compare(struct bt_ctf_field_type *,
		struct bt_ctf_field_type *);

static
int (* const type_compare_funcs[])(struct bt_ctf_field_type *,
		struct bt_ctf_field_type *) = {
	[CTF_TYPE_INTEGER] = bt_ctf_field_type_integer_compare,
	[CTF_TYPE_ENUM] = bt_ctf_field_type_enumeration_compare,
	[CTF_TYPE_FLOAT] = bt_ctf_field_type_floating_point_compare,
	[CTF_TYPE_STRUCT] = bt_ctf_field_type_structure_compare,
	[CTF_TYPE_VARIANT] = bt_ctf_field_type_variant_compare,
	[CTF_TYPE_ARRAY] = bt_ctf_field_type_array_compare,
	[CTF_TYPE_SEQUENCE] = bt_ctf_field_type_sequence_compare,
	[CTF_TYPE_STRING] = bt_ctf_field_type_string_compare,
};

static
int bt_ctf_field_type_integer_validate(struct bt_ctf_field_type *);
static
int bt_ctf_field_type_enumeration_validate(struct bt_ctf_field_type *);
static
int bt_ctf_field_type_structure_validate(struct bt_ctf_field_type *);
static
int bt_ctf_field_type_variant_validate(struct bt_ctf_field_type *);
static
int bt_ctf_field_type_array_validate(struct bt_ctf_field_type *);
static
int bt_ctf_field_type_sequence_validate(struct bt_ctf_field_type *);

static
int (* const type_validate_funcs[])(struct bt_ctf_field_type *) = {
	[CTF_TYPE_INTEGER] = bt_ctf_field_type_integer_validate,
	[CTF_TYPE_FLOAT] = NULL,
	[CTF_TYPE_STRING] = NULL,
	[CTF_TYPE_ENUM] = bt_ctf_field_type_enumeration_validate,
	[CTF_TYPE_STRUCT] = bt_ctf_field_type_structure_validate,
	[CTF_TYPE_VARIANT] = bt_ctf_field_type_variant_validate,
	[CTF_TYPE_ARRAY] = bt_ctf_field_type_array_validate,
	[CTF_TYPE_SEQUENCE] = bt_ctf_field_type_sequence_validate,
};

static
void destroy_enumeration_mapping(struct enumeration_mapping *mapping)
{
	g_free(mapping);
}

static
void destroy_structure_field(struct structure_field *field)
{
	bt_put(field->type);
	g_free(field);
}

static
void check_ranges_overlap(gpointer element, gpointer query)
{
	struct enumeration_mapping *mapping = element;
	struct range_overlap_query *overlap_query = query;

	if (mapping->range_start._signed <= overlap_query->range_end._signed
		&& overlap_query->range_start._signed <=
		mapping->range_end._signed) {
		overlap_query->overlaps = 1;
		overlap_query->mapping_name = mapping->string;
	}

	overlap_query->overlaps |=
		mapping->string == overlap_query->mapping_name;
}

static
void check_ranges_overlap_unsigned(gpointer element, gpointer query)
{
	struct enumeration_mapping *mapping = element;
	struct range_overlap_query *overlap_query = query;

	if (mapping->range_start._unsigned <= overlap_query->range_end._unsigned
		&& overlap_query->range_start._unsigned <=
		mapping->range_end._unsigned) {
		overlap_query->overlaps = 1;
		overlap_query->mapping_name = mapping->string;
	}

	overlap_query->overlaps |=
		mapping->string == overlap_query->mapping_name;
}

static
gint compare_enumeration_mappings_signed(struct enumeration_mapping **a,
		struct enumeration_mapping **b)
{
	return ((*a)->range_start._signed < (*b)->range_start._signed) ? -1 : 1;
}

static
gint compare_enumeration_mappings_unsigned(struct enumeration_mapping **a,
		struct enumeration_mapping **b)
{
	return ((*a)->range_start._unsigned < (*b)->range_start._unsigned) ? -1 : 1;
}

static
void bt_ctf_field_type_init(struct bt_ctf_field_type *type, int init_bo)
{
	enum ctf_type_id type_id = type->declaration->id;

	assert(type && (type_id > CTF_TYPE_UNKNOWN) &&
		(type_id < NR_CTF_TYPES));

	bt_object_init(type, bt_ctf_field_type_destroy);
	type->freeze = type_freeze_funcs[type_id];
	type->serialize = type_serialize_funcs[type_id];

	if (init_bo) {
		int ret = bt_ctf_field_type_set_byte_order(type,
			BT_CTF_BYTE_ORDER_NATIVE);
		assert(!ret);
	}

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

	bt_get(field_type);
	field->name = name_quark;
	field->type = field_type;
	g_hash_table_insert(field_name_to_index,
		(gpointer) (unsigned long) name_quark,
		(gpointer) (unsigned long) fields->len);
	g_ptr_array_add(fields, field);
end:
	return ret;
}

static
void bt_ctf_field_type_destroy(struct bt_object *obj)
{
	struct bt_ctf_field_type *type;
	enum ctf_type_id type_id;

	type = container_of(obj, struct bt_ctf_field_type, base);
	type_id = type->declaration->id;
	if (type_id <= CTF_TYPE_UNKNOWN ||
		type_id >= NR_CTF_TYPES) {
		return;
	}

	type_destroy_funcs[type_id](type);
}

static
int bt_ctf_field_type_integer_validate(struct bt_ctf_field_type *type)
{
	int ret = 0;

	struct bt_ctf_field_type_integer *integer =
		container_of(type, struct bt_ctf_field_type_integer,
			parent);

	if (integer->mapped_clock && integer->declaration.signedness) {
		ret = -1;
		goto end;
	}

end:
	return ret;
}

static
int bt_ctf_field_type_enumeration_validate(struct bt_ctf_field_type *type)
{
	int ret = 0;

	struct bt_ctf_field_type_enumeration *enumeration =
		container_of(type, struct bt_ctf_field_type_enumeration,
			parent);
	struct bt_ctf_field_type *container_type =
		bt_ctf_field_type_enumeration_get_container_type(type);

	if (!container_type) {
		ret = -1;
		goto end;
	}

	ret = bt_ctf_field_type_validate(container_type);
	if (ret) {
		goto end;
	}

	/* Ensure enum has entries */
	ret = enumeration->entries->len ? 0 : -1;

end:
	BT_PUT(container_type);
	return ret;
}

static
int bt_ctf_field_type_sequence_validate(struct bt_ctf_field_type *type)
{
	int ret = 0;
	struct bt_ctf_field_type *element_type = NULL;
	struct bt_ctf_field_type_sequence *sequence =
		container_of(type, struct bt_ctf_field_type_sequence,
		parent);

	/* Length field name should be set at this point */
	if (sequence->length_field_name->len == 0) {
		ret = -1;
		goto end;
	}

	element_type = bt_ctf_field_type_sequence_get_element_type(type);
	if (!element_type) {
		ret = -1;
		goto end;
	}

	ret = bt_ctf_field_type_validate(element_type);

end:
	BT_PUT(element_type);

	return ret;
}

static
int bt_ctf_field_type_array_validate(struct bt_ctf_field_type *type)
{
	int ret = 0;
	struct bt_ctf_field_type *element_type = NULL;

	element_type = bt_ctf_field_type_array_get_element_type(type);
	if (!element_type) {
		ret = -1;
		goto end;
	}

	ret = bt_ctf_field_type_validate(element_type);

end:
	BT_PUT(element_type);

	return ret;
}

static
int bt_ctf_field_type_structure_validate(struct bt_ctf_field_type *type)
{
	int ret = 0;
	struct bt_ctf_field_type *child_type = NULL;
	int field_count = bt_ctf_field_type_structure_get_field_count(type);
	int i;

	if (field_count < 0) {
		ret = -1;
		goto end;
	}

	for (i = 0; i < field_count; ++i) {
		ret = bt_ctf_field_type_structure_get_field(type,
			NULL, &child_type, i);
		if (ret) {
			goto end;
		}

		ret = bt_ctf_field_type_validate(child_type);
		if (ret) {
			goto end;
		}

		BT_PUT(child_type);
	}

end:
	BT_PUT(child_type);

	return ret;
}

static
int bt_ctf_field_type_variant_validate(struct bt_ctf_field_type *type)
{
	int ret = 0;
	int field_count;
	struct bt_ctf_field_type *child_type = NULL;
	struct bt_ctf_field_type_variant *variant =
		container_of(type, struct bt_ctf_field_type_variant,
			parent);
	int i;
	int tag_mappings_count;

	if (variant->tag_name->len == 0 || !variant->tag) {
		ret = -1;
		goto end;
	}

	tag_mappings_count =
		bt_ctf_field_type_enumeration_get_mapping_count(
			(struct bt_ctf_field_type *) variant->tag);

	if (tag_mappings_count != variant->fields->len) {
		ret = -1;
		goto end;
	}

	for (i = 0; i < tag_mappings_count; ++i) {
		const char *label;
		int64_t range_start, range_end;
		struct bt_ctf_field_type *ft;

		ret = bt_ctf_field_type_enumeration_get_mapping(
			(struct bt_ctf_field_type *) variant->tag,
			i, &label, &range_start, &range_end);
		if (ret) {
			goto end;
		}
		if (!label) {
			ret = -1;
			goto end;
		}

		ft = bt_ctf_field_type_variant_get_field_type_by_name(
			type, label);
		if (!ft) {
			ret = -1;
			goto end;
		}

		BT_PUT(ft);
	}

	field_count = bt_ctf_field_type_variant_get_field_count(type);
	if (field_count < 0) {
		ret = -1;
		goto end;
	}

	for (i = 0; i < field_count; ++i) {
		ret = bt_ctf_field_type_variant_get_field(type,
			NULL, &child_type, i);
		if (ret) {
			goto end;
		}

		ret = bt_ctf_field_type_validate(child_type);
		if (ret) {
			goto end;
		}

		BT_PUT(child_type);
	}

end:
	BT_PUT(child_type);

	return ret;
}

/*
 * This function validates a given field type without considering
 * where this field type is located. It only validates the properties
 * of the given field type and the properties of its children if
 * applicable.
 */
BT_HIDDEN
int bt_ctf_field_type_validate(struct bt_ctf_field_type *type)
{
	int ret = 0;
	enum ctf_type_id id = bt_ctf_field_type_get_type_id(type);

	if (!type) {
		ret = -1;
		goto end;
	}

	if (type->valid) {
		/* Already marked as valid */
		goto end;
	}

	if (type_validate_funcs[id]) {
		ret = type_validate_funcs[id](type);
	}

	if (!ret && type->frozen) {
		/* Field type is valid */
		type->valid = 1;
	}

end:
	return ret;
}

struct bt_ctf_field_type *bt_ctf_field_type_integer_create(unsigned int size)
{
	struct bt_ctf_field_type_integer *integer =
		g_new0(struct bt_ctf_field_type_integer, 1);

	if (!integer || size == 0 || size > 64) {
		return NULL;
	}

	integer->parent.declaration = &integer->declaration.p;
	integer->parent.declaration->id = CTF_TYPE_INTEGER;
	integer->declaration.len = size;
	integer->declaration.base = BT_CTF_INTEGER_BASE_DECIMAL;
	integer->declaration.encoding = BT_CTF_STRING_ENCODING_NONE;
	bt_ctf_field_type_init(&integer->parent, TRUE);
	return &integer->parent;
}

BT_HIDDEN
int bt_ctf_field_type_integer_get_size(struct bt_ctf_field_type *type)
{
	int ret = 0;
	struct bt_ctf_field_type_integer *integer;

	if (!type || type->declaration->id != CTF_TYPE_INTEGER) {
		ret = -1;
		goto end;
	}

	integer = container_of(type, struct bt_ctf_field_type_integer, parent);
	ret = (int) integer->declaration.len;
end:
	return ret;
}

int bt_ctf_field_type_integer_get_signed(struct bt_ctf_field_type *type)
{
	int ret = 0;
	struct bt_ctf_field_type_integer *integer;

	if (!type || type->declaration->id != CTF_TYPE_INTEGER) {
		ret = -1;
		goto end;
	}

	integer = container_of(type, struct bt_ctf_field_type_integer, parent);
	ret = integer->declaration.signedness;
end:
	return ret;
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
	integer->declaration.signedness = !!is_signed;
end:
	return ret;
}

BT_HIDDEN
enum bt_ctf_integer_base bt_ctf_field_type_integer_get_base(
		struct bt_ctf_field_type *type)
{
	enum bt_ctf_integer_base ret = BT_CTF_INTEGER_BASE_UNKNOWN;
	struct bt_ctf_field_type_integer *integer;

	if (!type || type->declaration->id != CTF_TYPE_INTEGER) {
		goto end;
	}

	integer = container_of(type, struct bt_ctf_field_type_integer, parent);
	ret = integer->declaration.base;
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

BT_HIDDEN
enum bt_ctf_string_encoding bt_ctf_field_type_integer_get_encoding(
		struct bt_ctf_field_type *type)
{
	enum bt_ctf_string_encoding ret = BT_CTF_STRING_ENCODING_UNKNOWN;
	struct bt_ctf_field_type_integer *integer;

	if (!type || type->declaration->id != CTF_TYPE_INTEGER) {
		goto end;
	}

	integer = container_of(type, struct bt_ctf_field_type_integer, parent);
	ret = integer->declaration.encoding;
end:
	return ret;
}

int bt_ctf_field_type_integer_set_encoding(struct bt_ctf_field_type *type,
		enum bt_ctf_string_encoding encoding)
{
	int ret = 0;
	struct bt_ctf_field_type_integer *integer;

	if (!type || type->frozen ||
		(type->declaration->id != CTF_TYPE_INTEGER) ||
		(encoding < BT_CTF_STRING_ENCODING_NONE) ||
		(encoding >= BT_CTF_STRING_ENCODING_UNKNOWN)) {
		ret = -1;
		goto end;
	}

	integer = container_of(type, struct bt_ctf_field_type_integer, parent);
	integer->declaration.encoding = encoding;
end:
	return ret;
}

BT_HIDDEN
struct bt_ctf_clock *bt_ctf_field_type_integer_get_mapped_clock(
		struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_type_integer *integer;
	struct bt_ctf_clock *clock = NULL;

	if (!type) {
		goto end;
	}

	integer = container_of(type, struct bt_ctf_field_type_integer, parent);
	clock = integer->mapped_clock;
	bt_get(clock);
end:
	return clock;
}

BT_HIDDEN
int bt_ctf_field_type_integer_set_mapped_clock(
		struct bt_ctf_field_type *type,
		struct bt_ctf_clock *clock)
{
	struct bt_ctf_field_type_integer *integer;
	int ret = 0;

	if (!type || type->frozen) {
		ret = -1;
		goto end;
	}

	integer = container_of(type, struct bt_ctf_field_type_integer, parent);
	bt_put(integer->mapped_clock);
	bt_get(clock);
	integer->mapped_clock = clock;
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

	if (integer_container_type->declaration->id != CTF_TYPE_INTEGER) {
		goto error;
	}

	enumeration = g_new0(struct bt_ctf_field_type_enumeration, 1);
	if (!enumeration) {
		goto error;
	}

	enumeration->parent.declaration = &enumeration->declaration.p;
	enumeration->parent.declaration->id = CTF_TYPE_ENUM;
	bt_get(integer_container_type);
	enumeration->container = integer_container_type;
	enumeration->entries = g_ptr_array_new_with_free_func(
		(GDestroyNotify)destroy_enumeration_mapping);
	bt_ctf_field_type_init(&enumeration->parent, FALSE);
	return &enumeration->parent;
error:
	g_free(enumeration);
	return NULL;
}

BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_field_type_enumeration_get_container_type(
		struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_type *container_type = NULL;
	struct bt_ctf_field_type_enumeration *enumeration_type;

	if (!type) {
		goto end;
	}

	if (type->declaration->id != CTF_TYPE_ENUM) {
		goto end;
	}

	enumeration_type = container_of(type,
		struct bt_ctf_field_type_enumeration, parent);
	container_type = enumeration_type->container;
	bt_get(container_type);
end:
	return container_type;
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
	query = (struct range_overlap_query) {
		.range_start._signed = range_start,
		.range_end._signed = range_end,
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

	*mapping = (struct enumeration_mapping) {
		.range_start._signed = range_start,
		.range_end._signed = range_end, .string = mapping_name};
	g_ptr_array_add(enumeration->entries, mapping);
	g_ptr_array_sort(enumeration->entries,
		(GCompareFunc)compare_enumeration_mappings_signed);
error_free:
	free(escaped_string);
end:
	return ret;
}

BT_HIDDEN
int bt_ctf_field_type_enumeration_add_mapping_unsigned(
		struct bt_ctf_field_type *type, const char *string,
		uint64_t range_start, uint64_t range_end)
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
	query = (struct range_overlap_query) {
		.range_start._unsigned = range_start,
		.range_end._unsigned = range_end,
		.mapping_name = mapping_name,
		.overlaps = 0 };
	enumeration = container_of(type, struct bt_ctf_field_type_enumeration,
		parent);

	/* Check that the range does not overlap with one already present */
	g_ptr_array_foreach(enumeration->entries, check_ranges_overlap_unsigned,
		&query);
	if (query.overlaps) {
		ret = -1;
		goto error_free;
	}

	mapping = g_new(struct enumeration_mapping, 1);
	if (!mapping) {
		ret = -1;
		goto error_free;
	}

	*mapping = (struct enumeration_mapping) {
		.range_start._unsigned = range_start,
		.range_end._unsigned = range_end, .string = mapping_name};
	g_ptr_array_add(enumeration->entries, mapping);
	g_ptr_array_sort(enumeration->entries,
		(GCompareFunc)compare_enumeration_mappings_unsigned);
error_free:
	free(escaped_string);
end:
	return ret;
}

BT_HIDDEN
const char *bt_ctf_field_type_enumeration_get_mapping_name_unsigned(
		struct bt_ctf_field_type_enumeration *enumeration_type,
		uint64_t value)
{
	const char *name = NULL;
	struct range_overlap_query query =
		(struct range_overlap_query) {
		.range_start._unsigned = value,
		.range_end._unsigned = value,
		.overlaps = 0 };

	g_ptr_array_foreach(enumeration_type->entries,
		check_ranges_overlap_unsigned,
		&query);
	if (!query.overlaps) {
		goto end;
	}

	name = g_quark_to_string(query.mapping_name);
end:
	return name;
}

const char *bt_ctf_field_type_enumeration_get_mapping_name_signed(
		struct bt_ctf_field_type_enumeration *enumeration_type,
		int64_t value)
{
	const char *name = NULL;
	struct range_overlap_query query =
		(struct range_overlap_query) {
		.range_start._signed = value,
		.range_end._signed = value,
		.overlaps = 0 };

	g_ptr_array_foreach(enumeration_type->entries, check_ranges_overlap,
		&query);
	if (!query.overlaps) {
		goto end;
	}

	name = g_quark_to_string(query.mapping_name);
end:
	return name;
}

int bt_ctf_field_type_enumeration_get_mapping_count(
		struct bt_ctf_field_type *type)
{
	int ret = 0;
	struct bt_ctf_field_type_enumeration *enumeration;

	if (!type || (type->declaration->id != CTF_TYPE_ENUM)) {
		ret = -1;
		goto end;
	}

	enumeration = container_of(type, struct bt_ctf_field_type_enumeration,
		parent);
	ret = (int) enumeration->entries->len;
end:
	return ret;
}

static inline
struct enumeration_mapping *get_enumeration_mapping(
	struct bt_ctf_field_type *type, int index)
{
	struct enumeration_mapping *mapping = NULL;
	struct bt_ctf_field_type_enumeration *enumeration;

	enumeration = container_of(type, struct bt_ctf_field_type_enumeration,
		parent);
	if (index >= enumeration->entries->len) {
		goto end;
	}

	mapping = g_ptr_array_index(enumeration->entries, index);
end:
	return mapping;
}

BT_HIDDEN
int bt_ctf_field_type_enumeration_get_mapping(
		struct bt_ctf_field_type *type, int index,
		const char **string, int64_t *range_start, int64_t *range_end)
{
	struct enumeration_mapping *mapping;
	int ret = 0;

	if (!type || index < 0 || !string || !range_start || !range_end ||
		(type->declaration->id != CTF_TYPE_ENUM)) {
		ret = -1;
		goto end;
	}

	mapping = get_enumeration_mapping(type, index);
	if (!mapping) {
		ret = -1;
		goto end;
	}

	*string = g_quark_to_string(mapping->string);
	*range_start = mapping->range_start._signed;
	*range_end = mapping->range_end._signed;
end:
	return ret;
}

BT_HIDDEN
int bt_ctf_field_type_enumeration_get_mapping_unsigned(
		struct bt_ctf_field_type *type, int index,
		const char **string, uint64_t *range_start, uint64_t *range_end)
{
	struct enumeration_mapping *mapping;
	int ret = 0;

	if (!type || index < 0 || !string || !range_start || !range_end ||
		(type->declaration->id != CTF_TYPE_ENUM)) {
		ret = -1;
		goto end;
	}

	mapping = get_enumeration_mapping(type, index);
	if (!mapping) {
		ret = -1;
		goto end;
	}

	*string = g_quark_to_string(mapping->string);
	*range_start = mapping->range_start._unsigned;
	*range_end = mapping->range_end._unsigned;
end:
	return ret;
}

BT_HIDDEN
int bt_ctf_field_type_enumeration_get_mapping_index_by_name(
		struct bt_ctf_field_type *type, const char *name)
{
	GQuark name_quark;
	struct bt_ctf_field_type_enumeration *enumeration;
	int i, ret = 0;

	if (!type || !name ||
		(type->declaration->id != CTF_TYPE_ENUM)) {
		ret = -1;
		goto end;
	}

	name_quark = g_quark_try_string(name);
	if (!name_quark) {
		ret = -1;
		goto end;
	}

	enumeration = container_of(type,
		struct bt_ctf_field_type_enumeration, parent);
	for (i = 0; i < enumeration->entries->len; i++) {
		struct enumeration_mapping *mapping =
			get_enumeration_mapping(type, i);

		if (mapping->string == name_quark) {
			ret = i;
			goto end;
		}
	}

	ret = -1;
end:
	return ret;
}

BT_HIDDEN
int bt_ctf_field_type_enumeration_get_mapping_index_by_value(
		struct bt_ctf_field_type *type, int64_t value)
{
	struct bt_ctf_field_type_enumeration *enumeration;
	int i, ret = 0;

	if (!type || (type->declaration->id != CTF_TYPE_ENUM)) {
		ret = -1;
		goto end;
	}

	enumeration = container_of(type,
		struct bt_ctf_field_type_enumeration, parent);
	for (i = 0; i < enumeration->entries->len; i++) {
		struct enumeration_mapping *mapping =
			get_enumeration_mapping(type, i);

		if (value >= mapping->range_start._signed &&
			value <= mapping->range_end._signed) {
			ret = i;
			goto end;
		}
	}

	ret = -1;
end:
	return ret;
}

BT_HIDDEN
int bt_ctf_field_type_enumeration_get_mapping_index_by_unsigned_value(
		struct bt_ctf_field_type *type, uint64_t value)
{
	struct bt_ctf_field_type_enumeration *enumeration;
	int i, ret = 0;

	if (!type || (type->declaration->id != CTF_TYPE_ENUM)) {
		ret = -1;
		goto end;
	}

	enumeration = container_of(type,
		struct bt_ctf_field_type_enumeration, parent);
	for (i = 0; i < enumeration->entries->len; i++) {
		struct enumeration_mapping *mapping =
			get_enumeration_mapping(type, i);

		if (value >= mapping->range_start._unsigned &&
			value <= mapping->range_end._unsigned) {
			ret = i;
			goto end;
		}
	}

	ret = -1;
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

	bt_ctf_field_type_init(&floating_point->parent, TRUE);
end:
	return floating_point ? &floating_point->parent : NULL;
}

BT_HIDDEN
int bt_ctf_field_type_floating_point_get_exponent_digits(
		struct bt_ctf_field_type *type)
{
	int ret = 0;
	struct bt_ctf_field_type_floating_point *floating_point;

	if (!type || (type->declaration->id != CTF_TYPE_FLOAT)) {
		ret = -1;
		goto end;
	}

	floating_point = container_of(type,
		struct bt_ctf_field_type_floating_point, parent);
	ret = (int) floating_point->declaration.exp->len;
end:
	return ret;
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

BT_HIDDEN
int bt_ctf_field_type_floating_point_get_mantissa_digits(
		struct bt_ctf_field_type *type)
{
	int ret = 0;
	struct bt_ctf_field_type_floating_point *floating_point;

	if (!type || (type->declaration->id != CTF_TYPE_FLOAT)) {
		ret = -1;
		goto end;
	}

	floating_point = container_of(type,
		struct bt_ctf_field_type_floating_point, parent);
	ret = (int) floating_point->mantissa.len + 1;
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
	structure->fields = g_ptr_array_new_with_free_func(
		(GDestroyNotify)destroy_structure_field);
	structure->field_name_to_index = g_hash_table_new(NULL, NULL);
	bt_ctf_field_type_init(&structure->parent, TRUE);
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
		bt_ctf_validate_identifier(field_name) ||
		(type->declaration->id != CTF_TYPE_STRUCT)) {
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
end:
	return ret;
}

BT_HIDDEN
int bt_ctf_field_type_structure_get_field_count(
		struct bt_ctf_field_type *type)
{
	int ret = 0;
	struct bt_ctf_field_type_structure *structure;

	if (!type || (type->declaration->id != CTF_TYPE_STRUCT)) {
		ret = -1;
		goto end;
	}

	structure = container_of(type, struct bt_ctf_field_type_structure,
		parent);
	ret = (int) structure->fields->len;
end:
	return ret;
}

int bt_ctf_field_type_structure_get_field(struct bt_ctf_field_type *type,
		const char **field_name, struct bt_ctf_field_type **field_type,
		int index)
{
	struct bt_ctf_field_type_structure *structure;
	struct structure_field *field;
	int ret = 0;

	if (!type || index < 0 ||
			(type->declaration->id != CTF_TYPE_STRUCT)) {
		ret = -1;
		goto end;
	}

	structure = container_of(type, struct bt_ctf_field_type_structure,
		parent);
	if (index >= structure->fields->len) {
		ret = -1;
		goto end;
	}

	field = g_ptr_array_index(structure->fields, index);
	if (field_type) {
		*field_type = field->type;
		bt_get(field->type);
	}
	if (field_name) {
		*field_name = g_quark_to_string(field->name);
	}
end:
	return ret;
}

BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_field_type_structure_get_field_type_by_name(
		struct bt_ctf_field_type *type,
		const char *name)
{
	size_t index;
	GQuark name_quark;
	struct structure_field *field;
	struct bt_ctf_field_type_structure *structure;
	struct bt_ctf_field_type *field_type = NULL;

	if (!type || !name) {
		goto end;
	}

	name_quark = g_quark_try_string(name);
	if (!name_quark) {
		goto end;
	}

	structure = container_of(type, struct bt_ctf_field_type_structure,
		parent);
	if (!g_hash_table_lookup_extended(structure->field_name_to_index,
		GUINT_TO_POINTER(name_quark), NULL, (gpointer *)&index)) {
		goto end;
	}

	field = structure->fields->pdata[index];
	field_type = field->type;
	bt_get(field_type);
end:
	return field_type;
}

struct bt_ctf_field_type *bt_ctf_field_type_variant_create(
	struct bt_ctf_field_type *enum_tag, const char *tag_name)
{
	struct bt_ctf_field_type_variant *variant = NULL;

	if (tag_name && bt_ctf_validate_identifier(tag_name)) {
		goto error;
	}

	variant = g_new0(struct bt_ctf_field_type_variant, 1);
	if (!variant) {
		goto error;
	}

	variant->parent.declaration = &variant->declaration.p;
	variant->parent.declaration->id = CTF_TYPE_VARIANT;
	variant->tag_name = g_string_new(tag_name);
	variant->field_name_to_index = g_hash_table_new(NULL, NULL);
	variant->fields = g_ptr_array_new_with_free_func(
		(GDestroyNotify) destroy_structure_field);
	if (enum_tag) {
		bt_get(enum_tag);
		variant->tag = container_of(enum_tag,
			struct bt_ctf_field_type_enumeration, parent);
	}

	bt_ctf_field_type_init(&variant->parent, TRUE);
	/* A variant's alignment is undefined */
	variant->parent.declaration->alignment = 0;
	return &variant->parent;
error:
	return NULL;
}

BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_field_type_variant_get_tag_type(
		struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_type_variant *variant;
	struct bt_ctf_field_type *tag_type = NULL;

	if (!type || (type->declaration->id != CTF_TYPE_VARIANT)) {
		goto end;
	}

	variant = container_of(type, struct bt_ctf_field_type_variant, parent);
	if (!variant->tag) {
		goto end;
	}

	tag_type = &variant->tag->parent;
	bt_get(tag_type);
end:
	return tag_type;
}

BT_HIDDEN
const char *bt_ctf_field_type_variant_get_tag_name(
		struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_type_variant *variant;
	const char *tag_name = NULL;

	if (!type || (type->declaration->id != CTF_TYPE_VARIANT)) {
		goto end;
	}

	variant = container_of(type, struct bt_ctf_field_type_variant, parent);
	if (variant->tag_name->len == 0) {
		goto end;
	}

	tag_name = variant->tag_name->str;
end:
	return tag_name;
}

BT_HIDDEN
int bt_ctf_field_type_variant_set_tag_name(
		struct bt_ctf_field_type *type, const char *name)
{
	int ret = 0;
	struct bt_ctf_field_type_variant *variant;

	if (!type || type->frozen ||
		(type->declaration->id != CTF_TYPE_VARIANT) ||
		bt_ctf_validate_identifier(name)) {
		ret = -1;
		goto end;
	}

	variant = container_of(type, struct bt_ctf_field_type_variant, parent);
	g_string_assign(variant->tag_name, name);
end:
	return ret;
}

int bt_ctf_field_type_variant_add_field(struct bt_ctf_field_type *type,
		struct bt_ctf_field_type *field_type,
		const char *field_name)
{
	size_t i;
	int ret = 0;
	struct bt_ctf_field_type_variant *variant;
	GQuark field_name_quark = g_quark_from_string(field_name);

	if (!type || !field_type || type->frozen ||
		bt_ctf_validate_identifier(field_name) ||
		(type->declaration->id != CTF_TYPE_VARIANT)) {
		ret = -1;
		goto end;
	}

	variant = container_of(type, struct bt_ctf_field_type_variant, parent);

	/* The user has explicitly provided a tag; validate against it. */
	if (variant->tag) {
		int name_found = 0;

		/* Make sure this name is present in the enum tag */
		for (i = 0; i < variant->tag->entries->len; i++) {
			struct enumeration_mapping *mapping =
				g_ptr_array_index(variant->tag->entries, i);

			if (mapping->string == field_name_quark) {
				name_found = 1;
				break;
			}
		}

		if (!name_found) {
			/* Validation failed */
			ret = -1;
			goto end;
		}
	}

	if (add_structure_field(variant->fields, variant->field_name_to_index,
		field_type, field_name)) {
		ret = -1;
		goto end;
	}
end:
	return ret;
}

BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_field_type_variant_get_field_type_by_name(
		struct bt_ctf_field_type *type,
		const char *field_name)
{
	size_t index;
	GQuark name_quark;
	struct structure_field *field;
	struct bt_ctf_field_type_variant *variant;
	struct bt_ctf_field_type *field_type = NULL;

	if (!type || !field_name) {
		goto end;
	}

	name_quark = g_quark_try_string(field_name);
	if (!name_quark) {
		goto end;
	}

	variant = container_of(type, struct bt_ctf_field_type_variant, parent);
	if (!g_hash_table_lookup_extended(variant->field_name_to_index,
		GUINT_TO_POINTER(name_quark), NULL, (gpointer *)&index)) {
		goto end;
	}

	field = g_ptr_array_index(variant->fields, index);
	field_type = field->type;
	bt_get(field_type);
end:
	return field_type;
}

BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_field_type_variant_get_field_type_from_tag(
		struct bt_ctf_field_type *type,
		struct bt_ctf_field *tag)
{
	const char *enum_value;
	struct bt_ctf_field_type *field_type = NULL;

	if (!type || !tag || type->declaration->id != CTF_TYPE_VARIANT) {
		goto end;
	}

	enum_value = bt_ctf_field_enumeration_get_mapping_name(tag);
	if (!enum_value) {
		goto end;
	}

	/* Already increments field_type's reference count */
	field_type = bt_ctf_field_type_variant_get_field_type_by_name(
		type, enum_value);
end:
	return field_type;
}

int bt_ctf_field_type_variant_get_field_count(struct bt_ctf_field_type *type)
{
	int ret = 0;
	struct bt_ctf_field_type_variant *variant;

	if (!type || (type->declaration->id != CTF_TYPE_VARIANT)) {
		ret = -1;
		goto end;
	}

	variant = container_of(type, struct bt_ctf_field_type_variant,
		parent);
	ret = (int) variant->fields->len;
end:
	return ret;

}

BT_HIDDEN
int bt_ctf_field_type_variant_get_field(struct bt_ctf_field_type *type,
		const char **field_name, struct bt_ctf_field_type **field_type,
		int index)
{
	struct bt_ctf_field_type_variant *variant;
	struct structure_field *field;
	int ret = 0;

	if (!type || index < 0 ||
			(type->declaration->id != CTF_TYPE_VARIANT)) {
		ret = -1;
		goto end;
	}

	variant = container_of(type, struct bt_ctf_field_type_variant,
		parent);
	if (index >= variant->fields->len) {
		ret = -1;
		goto end;
	}

	field = g_ptr_array_index(variant->fields, index);
	if (field_type) {
		*field_type = field->type;
		bt_get(field->type);
	}
	if (field_name) {
		*field_name = g_quark_to_string(field->name);
	}
end:
	return ret;
}

struct bt_ctf_field_type *bt_ctf_field_type_array_create(
		struct bt_ctf_field_type *element_type,
		unsigned int length)
{
	struct bt_ctf_field_type_array *array = NULL;

	if (!element_type || length == 0) {
		goto error;
	}

	array = g_new0(struct bt_ctf_field_type_array, 1);
	if (!array) {
		goto error;
	}

	array->parent.declaration = &array->declaration.p;
	array->parent.declaration->id = CTF_TYPE_ARRAY;

	bt_get(element_type);
	array->element_type = element_type;
	array->length = length;
	bt_ctf_field_type_init(&array->parent, FALSE);
	return &array->parent;
error:
	return NULL;
}

BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_field_type_array_get_element_type(
		struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_type *ret = NULL;
	struct bt_ctf_field_type_array *array;

	if (!type || (type->declaration->id != CTF_TYPE_ARRAY)) {
		goto end;
	}

	array = container_of(type, struct bt_ctf_field_type_array, parent);
	ret = array->element_type;
	bt_get(ret);
end:
	return ret;
}

BT_HIDDEN
int bt_ctf_field_type_array_set_element_type(struct bt_ctf_field_type *type,
		struct bt_ctf_field_type *element_type)
{
	int ret = 0;
	struct bt_ctf_field_type_array *array;

	if (!type || !element_type ||
			(type->declaration->id != CTF_TYPE_ARRAY)) {
		ret = -1;
		goto end;
	}

	array = container_of(type, struct bt_ctf_field_type_array, parent);

	if (array->element_type) {
		BT_PUT(array->element_type);
	}

	array->element_type = element_type;
	bt_get(array->element_type);

end:
	return ret;
}

BT_HIDDEN
int64_t bt_ctf_field_type_array_get_length(struct bt_ctf_field_type *type)
{
	int64_t ret;
	struct bt_ctf_field_type_array *array;

	if (!type || (type->declaration->id != CTF_TYPE_ARRAY)) {
		ret = -1;
		goto end;
	}

	array = container_of(type, struct bt_ctf_field_type_array, parent);
	ret = (int64_t) array->length;
end:
	return ret;
}

struct bt_ctf_field_type *bt_ctf_field_type_sequence_create(
		struct bt_ctf_field_type *element_type,
		const char *length_field_name)
{
	struct bt_ctf_field_type_sequence *sequence = NULL;

	if (!element_type || bt_ctf_validate_identifier(length_field_name)) {
		goto error;
	}

	sequence = g_new0(struct bt_ctf_field_type_sequence, 1);
	if (!sequence) {
		goto error;
	}

	sequence->parent.declaration = &sequence->declaration.p;
	sequence->parent.declaration->id = CTF_TYPE_SEQUENCE;
	bt_get(element_type);
	sequence->element_type = element_type;
	sequence->length_field_name = g_string_new(length_field_name);
	bt_ctf_field_type_init(&sequence->parent, FALSE);
	return &sequence->parent;
error:
	return NULL;
}

BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_field_type_sequence_get_element_type(
		struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_type *ret = NULL;
	struct bt_ctf_field_type_sequence *sequence;

	if (!type || (type->declaration->id != CTF_TYPE_SEQUENCE)) {
		goto end;
	}

	sequence = container_of(type, struct bt_ctf_field_type_sequence,
		parent);
	ret = sequence->element_type;
	bt_get(ret);
end:
	return ret;
}

BT_HIDDEN
int bt_ctf_field_type_sequence_set_element_type(struct bt_ctf_field_type *type,
		struct bt_ctf_field_type *element_type)
{
	int ret = 0;
	struct bt_ctf_field_type_sequence *sequence;

	if (!type || !element_type ||
			(type->declaration->id != CTF_TYPE_SEQUENCE)) {
		ret = -1;
		goto end;
	}

	sequence = container_of(type, struct bt_ctf_field_type_sequence, parent);

	if (sequence->element_type) {
		BT_PUT(sequence->element_type);
	}

	sequence->element_type = element_type;
	bt_get(sequence->element_type);

end:
	return ret;
}

BT_HIDDEN
const char *bt_ctf_field_type_sequence_get_length_field_name(
		struct bt_ctf_field_type *type)
{
	const char *ret = NULL;
	struct bt_ctf_field_type_sequence *sequence;

	if (!type || (type->declaration->id != CTF_TYPE_SEQUENCE)) {
		goto end;
	}

	sequence = container_of(type, struct bt_ctf_field_type_sequence,
		parent);
	ret = sequence->length_field_name->str;
end:
	return ret;
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
	bt_ctf_field_type_init(&string->parent, TRUE);
	string->declaration.encoding = BT_CTF_STRING_ENCODING_UTF8;
	string->parent.declaration->alignment = CHAR_BIT;
	return &string->parent;
}

BT_HIDDEN
enum bt_ctf_string_encoding bt_ctf_field_type_string_get_encoding(
		struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_type_string *string;
	enum bt_ctf_string_encoding ret = BT_CTF_STRING_ENCODING_UNKNOWN;

	if (!type || (type->declaration->id != CTF_TYPE_STRING)) {
		goto end;
	}

	string = container_of(type, struct bt_ctf_field_type_string,
		parent);
	ret = string->declaration.encoding;
end:
	return ret;
}

int bt_ctf_field_type_string_set_encoding(struct bt_ctf_field_type *type,
		enum bt_ctf_string_encoding encoding)
{
	int ret = 0;
	struct bt_ctf_field_type_string *string;

	if (!type || type->declaration->id != CTF_TYPE_STRING ||
		(encoding != BT_CTF_STRING_ENCODING_UTF8 &&
		encoding != BT_CTF_STRING_ENCODING_ASCII)) {
		ret = -1;
		goto end;
	}

	string = container_of(type, struct bt_ctf_field_type_string, parent);
	string->declaration.encoding = encoding;
end:
	return ret;
}

BT_HIDDEN
int bt_ctf_field_type_get_alignment(struct bt_ctf_field_type *type)
{
	int ret;
	enum ctf_type_id type_id;

	if (!type) {
		ret = -1;
		goto end;
	}

	if (type->frozen) {
		ret = (int) type->declaration->alignment;
		goto end;
	}

	type_id = bt_ctf_field_type_get_type_id(type);
	switch (type_id) {
	case CTF_TYPE_SEQUENCE:
	{
		struct bt_ctf_field_type *element =
			bt_ctf_field_type_sequence_get_element_type(type);

		if (!element) {
			ret = -1;
			goto end;
		}

		ret = bt_ctf_field_type_get_alignment(element);
		bt_put(element);
		break;
	}
	case CTF_TYPE_ARRAY:
	{
		struct bt_ctf_field_type *element =
			bt_ctf_field_type_array_get_element_type(type);

		if (!element) {
			ret = -1;
			goto end;
		}

		ret = bt_ctf_field_type_get_alignment(element);
		bt_put(element);
		break;
	}
	case CTF_TYPE_STRUCT:
	{
		int i, element_count;

		element_count = bt_ctf_field_type_structure_get_field_count(
			type);
		if (element_count < 0) {
			ret = element_count;
			goto end;
		}

		for (i = 0; i < element_count; i++) {
			struct bt_ctf_field_type *field;
			int field_alignment;

			ret = bt_ctf_field_type_structure_get_field(type, NULL,
				&field, i);
			if (ret) {
				goto end;
			}

			assert(field);
			field_alignment = bt_ctf_field_type_get_alignment(
				field);
			bt_put(field);
			if (field_alignment < 0) {
				ret = field_alignment;
				goto end;
			}

			type->declaration->alignment = MAX(field_alignment,
				type->declaration->alignment);
		}
		ret = (int) type->declaration->alignment;
		break;
	}
	case CTF_TYPE_UNKNOWN:
		ret = -1;
		break;
	default:
		ret = (int) type->declaration->alignment;
		break;
	}
end:
	return ret;
}

static inline
int is_power_of_two(unsigned int value)
{
	return ((value & (value - 1)) == 0) && value > 0;
}

int bt_ctf_field_type_set_alignment(struct bt_ctf_field_type *type,
		unsigned int alignment)
{
	int ret = 0;
	enum ctf_type_id type_id;

	/* Alignment must be a power of two */
	if (!type || type->frozen || !is_power_of_two(alignment)) {
		ret = -1;
		goto end;
	}

	type_id = bt_ctf_field_type_get_type_id(type);
	if (type_id == CTF_TYPE_UNKNOWN) {
		ret = -1;
		goto end;
	}

	if (type->declaration->id == CTF_TYPE_STRING &&
		alignment != CHAR_BIT) {
		ret = -1;
		goto end;
	}

	if (type_id == CTF_TYPE_VARIANT ||
		type_id == CTF_TYPE_SEQUENCE ||
		type_id == CTF_TYPE_ARRAY) {
		/* Setting an alignment on these types makes no sense */
		ret = -1;
		goto end;
	}

	type->declaration->alignment = alignment;
	ret = 0;
end:
	return ret;
}

BT_HIDDEN
enum bt_ctf_byte_order bt_ctf_field_type_get_byte_order(
		struct bt_ctf_field_type *type)
{
	enum bt_ctf_byte_order ret = BT_CTF_BYTE_ORDER_UNKNOWN;

	if (!type) {
		goto end;
	}

	switch (type->declaration->id) {
	case CTF_TYPE_INTEGER:
	{
		struct bt_ctf_field_type_integer *integer = container_of(
			type, struct bt_ctf_field_type_integer, parent);
		ret = integer->user_byte_order;
		break;
	}
	case CTF_TYPE_FLOAT:
	{
		struct bt_ctf_field_type_floating_point *floating_point =
			container_of(type,
				struct bt_ctf_field_type_floating_point,
				parent);
		ret = floating_point->user_byte_order;
		break;
	}
	default:
		goto end;
	}

	assert(ret == BT_CTF_BYTE_ORDER_NATIVE ||
		ret == BT_CTF_BYTE_ORDER_LITTLE_ENDIAN ||
		ret == BT_CTF_BYTE_ORDER_BIG_ENDIAN ||
		ret == BT_CTF_BYTE_ORDER_NETWORK);

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

	switch (byte_order) {
	case BT_CTF_BYTE_ORDER_NATIVE:
		/* Leave unset. Will be initialized by parent. */
		internal_byte_order = 0;
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

	type_id = type->declaration->id;
	if (set_byte_order_funcs[type_id]) {
		set_byte_order_funcs[type_id](type, internal_byte_order, 0);
	}
end:
	return ret;
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
int bt_ctf_field_type_is_integer(struct bt_ctf_field_type *type)
{
	return bt_ctf_field_type_get_type_id(type) == CTF_TYPE_INTEGER;
}

BT_HIDDEN
int bt_ctf_field_type_is_floating_point(struct bt_ctf_field_type *type)
{
	return bt_ctf_field_type_get_type_id(type) == CTF_TYPE_FLOAT;
}

BT_HIDDEN
int bt_ctf_field_type_is_enumeration(struct bt_ctf_field_type *type)
{
	return bt_ctf_field_type_get_type_id(type) == CTF_TYPE_ENUM;
}

BT_HIDDEN
int bt_ctf_field_type_is_string(struct bt_ctf_field_type *type)
{
	return bt_ctf_field_type_get_type_id(type) == CTF_TYPE_STRING;
}

BT_HIDDEN
int bt_ctf_field_type_is_structure(struct bt_ctf_field_type *type)
{
	return bt_ctf_field_type_get_type_id(type) == CTF_TYPE_STRUCT;
}

BT_HIDDEN
int bt_ctf_field_type_is_array(struct bt_ctf_field_type *type)
{
	return bt_ctf_field_type_get_type_id(type) == CTF_TYPE_ARRAY;
}

BT_HIDDEN
int bt_ctf_field_type_is_sequence(struct bt_ctf_field_type *type)
{
	return bt_ctf_field_type_get_type_id(type) == CTF_TYPE_SEQUENCE;
}

BT_HIDDEN
int bt_ctf_field_type_is_variant(struct bt_ctf_field_type *type)
{
	return bt_ctf_field_type_get_type_id(type) == CTF_TYPE_VARIANT;
}

void bt_ctf_field_type_get(struct bt_ctf_field_type *type)
{
	bt_get(type);
}

void bt_ctf_field_type_put(struct bt_ctf_field_type *type)
{
	bt_put(type);
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
struct bt_ctf_field_type *bt_ctf_field_type_variant_get_field_type_signed(
		struct bt_ctf_field_type_variant *variant,
		int64_t tag_value)
{
	struct bt_ctf_field_type *type = NULL;
	GQuark field_name_quark;
	gpointer index;
	struct structure_field *field_entry;
	struct range_overlap_query query = {
		.range_start._signed = tag_value,
		.range_end._signed = tag_value,
		.mapping_name = 0, .overlaps = 0};

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

	field_entry = g_ptr_array_index(variant->fields, (size_t) index);
	type = field_entry->type;
end:
	return type;
}

BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_field_type_variant_get_field_type_unsigned(
		struct bt_ctf_field_type_variant *variant,
		uint64_t tag_value)
{
	struct bt_ctf_field_type *type = NULL;
	GQuark field_name_quark;
	gpointer index;
	struct structure_field *field_entry;
	struct range_overlap_query query = {
		.range_start._unsigned = tag_value,
		.range_end._unsigned = tag_value,
		.mapping_name = 0, .overlaps = 0};

	g_ptr_array_foreach(variant->tag->entries,
		check_ranges_overlap_unsigned,
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

	/* Make sure field type is valid before serializing it */
	ret = bt_ctf_field_type_validate(type);

	if (ret) {
		goto end;
	}

	ret = type->serialize(type, context);
end:
	return ret;
}

BT_HIDDEN
void bt_ctf_field_type_set_native_byte_order(struct bt_ctf_field_type *type,
		int byte_order)
{
	if (!type) {
		return;
	}

	assert(byte_order == LITTLE_ENDIAN || byte_order == BIG_ENDIAN);
	if (set_byte_order_funcs[type->declaration->id]) {
		set_byte_order_funcs[type->declaration->id](type,
			byte_order, 1);
	}
}

BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_field_type_copy(struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_type *copy = NULL;

	if (!type) {
		goto end;
	}

	copy = type_copy_funcs[type->declaration->id](type);
end:
	return copy;
}

BT_HIDDEN
int bt_ctf_field_type_structure_get_field_name_index(
		struct bt_ctf_field_type *type, const char *name)
{
	int ret;
	size_t index;
	GQuark name_quark;
	struct bt_ctf_field_type_structure *structure;

	if (!type || !name ||
		bt_ctf_field_type_get_type_id(type) != CTF_TYPE_STRUCT) {
		ret = -1;
		goto end;
	}

	name_quark = g_quark_try_string(name);
	if (!name_quark) {
		ret = -1;
		goto end;
	}

	structure = container_of(type, struct bt_ctf_field_type_structure,
		parent);
	if (!g_hash_table_lookup_extended(structure->field_name_to_index,
		GUINT_TO_POINTER(name_quark), NULL, (gpointer *)&index)) {
		ret = -1;
		goto end;
	}
	ret = (int) index;
end:
	return ret;
}

BT_HIDDEN
int bt_ctf_field_type_structure_set_field_index(struct bt_ctf_field_type *type,
		struct bt_ctf_field_type *field, int index)
{
	int ret = 0;
	struct bt_ctf_field_type_structure *structure;

	if (!type || !field ||
		bt_ctf_field_type_get_type_id(type) != CTF_TYPE_STRUCT) {
		ret = -1;
		goto end;
	}

	structure = container_of(type, struct bt_ctf_field_type_structure,
		parent);
	if (index < 0 || index >= structure->fields->len) {
		ret = -1;
		goto end;
	}

	bt_get(field);
	bt_put(((struct structure_field *)
		g_ptr_array_index(structure->fields, index))->type);
	((struct structure_field *) structure->fields->pdata[index])->type =
		field;
end:
	return ret;
}

BT_HIDDEN
int bt_ctf_field_type_variant_get_field_name_index(
		struct bt_ctf_field_type *type, const char *name)
{
	int ret;
	size_t index;
	GQuark name_quark;
	struct bt_ctf_field_type_variant *variant;

	if (!type || !name ||
		bt_ctf_field_type_get_type_id(type) != CTF_TYPE_VARIANT) {
		ret = -1;
		goto end;
	}

	name_quark = g_quark_try_string(name);
	if (!name_quark) {
		ret = -1;
		goto end;
	}

	variant = container_of(type, struct bt_ctf_field_type_variant,
		parent);
	if (!g_hash_table_lookup_extended(variant->field_name_to_index,
		GUINT_TO_POINTER(name_quark), NULL, (gpointer *)&index)) {
		ret = -1;
		goto end;
	}
	ret = (int) index;
end:
	return ret;
}

BT_HIDDEN
int bt_ctf_field_type_sequence_set_length_field_path(
		struct bt_ctf_field_type *type,
		struct bt_ctf_field_path *path)
{
	int ret = 0;
	struct bt_ctf_field_type_sequence *sequence;

	if (!type || bt_ctf_field_type_get_type_id(type) !=
			CTF_TYPE_SEQUENCE) {
		ret = -1;
		goto end;
	}

	sequence = container_of(type, struct bt_ctf_field_type_sequence,
		parent);
	bt_get(path);
	BT_MOVE(sequence->length_field_path, path);
end:
	return ret;
}

BT_HIDDEN
int bt_ctf_field_type_variant_set_tag_field_path(struct bt_ctf_field_type *type,
		struct bt_ctf_field_path *path)
{
	int ret = 0;
	struct bt_ctf_field_type_variant *variant;

	if (!type || bt_ctf_field_type_get_type_id(type) !=
			CTF_TYPE_VARIANT) {
		ret = -1;
		goto end;
	}

	variant = container_of(type, struct bt_ctf_field_type_variant,
		parent);
	bt_get(path);
	BT_MOVE(variant->tag_field_path, path);
end:
	return ret;
}

BT_HIDDEN
int bt_ctf_field_type_variant_set_tag_field_type(struct bt_ctf_field_type *type,
		struct bt_ctf_field_type *tag)
{
	int ret = 0;
	struct bt_ctf_field_type_variant *variant;

	if (!type || !tag ||
			bt_ctf_field_type_get_type_id(tag) !=
			CTF_TYPE_ENUM) {
		ret = -1;
		goto end;
	}

	variant = container_of(type, struct bt_ctf_field_type_variant,
		parent);
	bt_get(tag);
	if (variant->tag) {
		bt_put(&variant->tag->parent);
	}
	variant->tag = container_of(tag, struct bt_ctf_field_type_enumeration,
		parent);
end:
	return ret;
}

BT_HIDDEN
int bt_ctf_field_type_variant_set_field_index(struct bt_ctf_field_type *type,
		struct bt_ctf_field_type *field, int index)
{
	int ret = 0;
	struct bt_ctf_field_type_variant *variant;

	if (!type || !field ||
		bt_ctf_field_type_get_type_id(type) != CTF_TYPE_VARIANT) {
		ret = -1;
		goto end;
	}

	variant = container_of(type, struct bt_ctf_field_type_variant,
		parent);
	if (index < 0 || index >= variant->fields->len) {
		ret = -1;
		goto end;
	}

	bt_get(field);
	bt_put(((struct structure_field *)
		g_ptr_array_index(variant->fields, index))->type);
	((struct structure_field *) variant->fields->pdata[index])->type =
		field;
end:
	return ret;
}

static
void bt_ctf_field_type_integer_destroy(struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_type_integer *integer =
		(struct bt_ctf_field_type_integer *) type;

	if (!type) {
		return;
	}

	bt_put(integer->mapped_clock);
	g_free(integer);
}

static
void bt_ctf_field_type_enumeration_destroy(struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_type_enumeration *enumeration =
		(struct bt_ctf_field_type_enumeration *) type;

	if (!type) {
		return;
	}

	g_ptr_array_free(enumeration->entries, TRUE);
	bt_put(enumeration->container);
	g_free(enumeration);
}

static
void bt_ctf_field_type_floating_point_destroy(struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_type_floating_point *floating_point =
		(struct bt_ctf_field_type_floating_point *) type;

	if (!type) {
		return;
	}

	g_free(floating_point);
}

static
void bt_ctf_field_type_structure_destroy(struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_type_structure *structure =
		(struct bt_ctf_field_type_structure *) type;

	if (!type) {
		return;
	}

	g_ptr_array_free(structure->fields, TRUE);
	g_hash_table_destroy(structure->field_name_to_index);
	g_free(structure);
}

static
void bt_ctf_field_type_variant_destroy(struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_type_variant *variant =
		(struct bt_ctf_field_type_variant *) type;

	if (!type) {
		return;
	}

	g_ptr_array_free(variant->fields, TRUE);
	g_hash_table_destroy(variant->field_name_to_index);
	g_string_free(variant->tag_name, TRUE);
	bt_put(&variant->tag->parent);
	BT_PUT(variant->tag_field_path);
	g_free(variant);
}

static
void bt_ctf_field_type_array_destroy(struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_type_array *array =
		(struct bt_ctf_field_type_array *) type;

	if (!type) {
		return;
	}

	bt_put(array->element_type);
	g_free(array);
}

static
void bt_ctf_field_type_sequence_destroy(struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_type_sequence *sequence =
		(struct bt_ctf_field_type_sequence *) type;

	if (!type) {
		return;
	}

	bt_put(sequence->element_type);
	g_string_free(sequence->length_field_name, TRUE);
	BT_PUT(sequence->length_field_path);
	g_free(sequence);
}

static
void bt_ctf_field_type_string_destroy(struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_type_string *string =
		(struct bt_ctf_field_type_string *) type;

	if (!type) {
		return;
	}

	g_free(string);
}

static
void generic_field_type_freeze(struct bt_ctf_field_type *type)
{
	type->frozen = 1;
}

static
void bt_ctf_field_type_integer_freeze(struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_type_integer *integer_type = container_of(
		type, struct bt_ctf_field_type_integer, parent);

	if (integer_type->mapped_clock) {
		bt_ctf_clock_freeze(integer_type->mapped_clock);
	}

	generic_field_type_freeze(type);
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

	/* Cache the alignment */
	type->declaration->alignment = bt_ctf_field_type_get_alignment(type);
	generic_field_type_freeze(type);
	g_ptr_array_foreach(structure_type->fields,
		(GFunc) freeze_structure_field, NULL);
}

static
void bt_ctf_field_type_variant_freeze(struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_type_variant *variant_type = container_of(
		type, struct bt_ctf_field_type_variant, parent);

	generic_field_type_freeze(type);
	g_ptr_array_foreach(variant_type->fields,
		(GFunc) freeze_structure_field, NULL);
}

static
void bt_ctf_field_type_array_freeze(struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_type_array *array_type = container_of(
		type, struct bt_ctf_field_type_array, parent);

	/* Cache the alignment */
	type->declaration->alignment = bt_ctf_field_type_get_alignment(type);
	generic_field_type_freeze(type);
	bt_ctf_field_type_freeze(array_type->element_type);
}

static
void bt_ctf_field_type_sequence_freeze(struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_type_sequence *sequence_type = container_of(
		type, struct bt_ctf_field_type_sequence, parent);

	/* Cache the alignment */
	type->declaration->alignment = bt_ctf_field_type_get_alignment(type);
	generic_field_type_freeze(type);
	bt_ctf_field_type_freeze(sequence_type->element_type);
}

static
const char *get_encoding_string(enum bt_ctf_string_encoding encoding)
{
	const char *encoding_string;

	switch (encoding) {
	case BT_CTF_STRING_ENCODING_NONE:
		encoding_string = "none";
		break;
	case BT_CTF_STRING_ENCODING_ASCII:
		encoding_string = "ASCII";
		break;
	case BT_CTF_STRING_ENCODING_UTF8:
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
	int ret = 0;

	g_string_append_printf(context->string,
		"integer { size = %zu; align = %zu; signed = %s; encoding = %s; base = %s; byte_order = %s",
		integer->declaration.len, type->declaration->alignment,
		(integer->declaration.signedness ? "true" : "false"),
		get_encoding_string(integer->declaration.encoding),
		get_integer_base_string(integer->declaration.base),
		get_byte_order_string(integer->declaration.byte_order));
	if (integer->mapped_clock) {
		const char *clock_name = bt_ctf_clock_get_name(
			integer->mapped_clock);

		if (!clock_name) {
			ret = -1;
			goto end;
		}

		g_string_append_printf(context->string,
			"; map = clock.%s.value", clock_name);
	}

	g_string_append(context->string, "; }");
end:
	return ret;
}

static
int bt_ctf_field_type_enumeration_serialize(struct bt_ctf_field_type *type,
		struct metadata_context *context)
{
	size_t entry;
	int ret;
	struct bt_ctf_field_type_enumeration *enumeration = container_of(type,
		struct bt_ctf_field_type_enumeration, parent);
	struct bt_ctf_field_type *container_type;
	int container_signed;

	container_type = bt_ctf_field_type_enumeration_get_container_type(type);
	if (!container_type) {
		ret = -1;
		goto end;
	}

	container_signed = bt_ctf_field_type_integer_get_signed(container_type);
	if (container_signed < 0) {
		ret = container_signed;
		goto error_put_container_type;
	}

	g_string_append(context->string, "enum : ");
	ret = bt_ctf_field_type_serialize(enumeration->container, context);
	if (ret) {
		goto error_put_container_type;
	}

	g_string_append(context->string, " { ");
	for (entry = 0; entry < enumeration->entries->len; entry++) {
		struct enumeration_mapping *mapping =
			enumeration->entries->pdata[entry];

		if (container_signed) {
			if (mapping->range_start._signed ==
				mapping->range_end._signed) {
				g_string_append_printf(context->string,
					"\"%s\" = %" PRId64,
					g_quark_to_string(mapping->string),
					mapping->range_start._signed);
			} else {
				g_string_append_printf(context->string,
					"\"%s\" = %" PRId64 " ... %" PRId64,
					g_quark_to_string(mapping->string),
					mapping->range_start._signed,
					mapping->range_end._signed);
			}
		} else {
			if (mapping->range_start._unsigned ==
				mapping->range_end._unsigned) {
				g_string_append_printf(context->string,
					"\"%s\" = %" PRIu64,
					g_quark_to_string(mapping->string),
					mapping->range_start._unsigned);
			} else {
				g_string_append_printf(context->string,
					"\"%s\" = %" PRIu64 " ... %" PRIu64,
					g_quark_to_string(mapping->string),
					mapping->range_start._unsigned,
					mapping->range_end._unsigned);
			}
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
error_put_container_type:
	bt_put(container_type);
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
	if (variant->tag_name->len > 0) {
		g_string_append_printf(context->string,
			"variant <%s> {\n", variant->tag_name->str);
	} else {
		g_string_append(context->string, "variant {\n");
	}

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
enum bt_ctf_byte_order get_ctf_ir_byte_order(int byte_order) {
	enum bt_ctf_byte_order ret;

	switch (byte_order) {
	case BT_CTF_BYTE_ORDER_LITTLE_ENDIAN:
	case LITTLE_ENDIAN:
		ret = BT_CTF_BYTE_ORDER_LITTLE_ENDIAN;
		break;
	case BT_CTF_BYTE_ORDER_BIG_ENDIAN:
	case BIG_ENDIAN:
		ret = BT_CTF_BYTE_ORDER_BIG_ENDIAN;
		break;
	case BT_CTF_BYTE_ORDER_NETWORK:
		ret = BT_CTF_BYTE_ORDER_NETWORK;
		break;
	case BT_CTF_BYTE_ORDER_NATIVE:
		ret = BT_CTF_BYTE_ORDER_NATIVE;
		break;
	default:
		ret = BT_CTF_BYTE_ORDER_UNKNOWN;
		break;
	}

	return ret;
}

static
void bt_ctf_field_type_integer_set_byte_order(struct bt_ctf_field_type *type,
		int byte_order, int set_native)
{
	struct bt_ctf_field_type_integer *integer_type = container_of(type,
		struct bt_ctf_field_type_integer, parent);

	if (set_native) {
		if (integer_type->user_byte_order == BT_CTF_BYTE_ORDER_NATIVE) {
			/*
			 * User byte order is native, so we can set
			 * the real byte order.
			 */
			integer_type->declaration.byte_order =
				byte_order;
		}
	} else {
		integer_type->user_byte_order =
			get_ctf_ir_byte_order(byte_order);
		integer_type->declaration.byte_order = byte_order;
	}
}

static
void bt_ctf_field_type_enumeration_set_byte_order(
		struct bt_ctf_field_type *type, int byte_order, int set_native)
{
	struct bt_ctf_field_type_enumeration *enum_type = container_of(type,
		struct bt_ctf_field_type_enumeration, parent);

	/* Safe to assume that container is an integer */
	bt_ctf_field_type_integer_set_byte_order(enum_type->container,
		byte_order, set_native);
}

static
void bt_ctf_field_type_floating_point_set_byte_order(
		struct bt_ctf_field_type *type, int byte_order, int set_native)
{
	struct bt_ctf_field_type_floating_point *floating_point_type =
		container_of(type, struct bt_ctf_field_type_floating_point,
		parent);

	if (set_native) {
		if (floating_point_type->user_byte_order ==
				BT_CTF_BYTE_ORDER_NATIVE) {
			/*
			 * User byte order is native, so we can set
			 * the real byte order.
			 */
			floating_point_type->declaration.byte_order =
				byte_order;
			floating_point_type->sign.byte_order =
				byte_order;
			floating_point_type->mantissa.byte_order =
				byte_order;
			floating_point_type->exp.byte_order =
				byte_order;
		}
	} else {
		floating_point_type->user_byte_order =
			get_ctf_ir_byte_order(byte_order);
		floating_point_type->declaration.byte_order = byte_order;
		floating_point_type->sign.byte_order = byte_order;
		floating_point_type->mantissa.byte_order = byte_order;
		floating_point_type->exp.byte_order = byte_order;
	}
}

static
void bt_ctf_field_type_structure_set_byte_order(struct bt_ctf_field_type *type,
		int byte_order, int set_native)
{
	int i;
	struct bt_ctf_field_type_structure *structure_type =
		container_of(type, struct bt_ctf_field_type_structure,
		parent);

	for (i = 0; i < structure_type->fields->len; i++) {
		struct structure_field *field = g_ptr_array_index(
			structure_type->fields, i);
		struct bt_ctf_field_type *field_type = field->type;

		if (set_byte_order_funcs[field_type->declaration->id]) {
			set_byte_order_funcs[field_type->declaration->id](
				field_type, byte_order, set_native);
		}
	}
}

static
void bt_ctf_field_type_variant_set_byte_order(struct bt_ctf_field_type *type,
		int byte_order, int set_native)
{
	int i;
	struct bt_ctf_field_type_variant *variant_type =
		container_of(type, struct bt_ctf_field_type_variant,
		parent);

	for (i = 0; i < variant_type->fields->len; i++) {
		struct structure_field *field = g_ptr_array_index(
			variant_type->fields, i);
		struct bt_ctf_field_type *field_type = field->type;

		if (set_byte_order_funcs[field_type->declaration->id]) {
			set_byte_order_funcs[field_type->declaration->id](
				field_type, byte_order, set_native);
		}
	}
}

static
void bt_ctf_field_type_array_set_byte_order(struct bt_ctf_field_type *type,
		int byte_order, int set_native)
{
	struct bt_ctf_field_type_array *array_type =
		container_of(type, struct bt_ctf_field_type_array,
		parent);

	if (set_byte_order_funcs[array_type->element_type->declaration->id]) {
		set_byte_order_funcs[array_type->element_type->declaration->id](
			array_type->element_type, byte_order, set_native);
	}
}

static
void bt_ctf_field_type_sequence_set_byte_order(struct bt_ctf_field_type *type,
		int byte_order, int set_native)
{
	struct bt_ctf_field_type_sequence *sequence_type =
		container_of(type, struct bt_ctf_field_type_sequence,
		parent);

	if (set_byte_order_funcs[
		sequence_type->element_type->declaration->id]) {
		set_byte_order_funcs[
			sequence_type->element_type->declaration->id](
			sequence_type->element_type, byte_order, set_native);
	}
}

static
struct bt_ctf_field_type *bt_ctf_field_type_integer_copy(
		struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_type *copy;
	struct bt_ctf_field_type_integer *integer, *copy_integer;

	integer = container_of(type, struct bt_ctf_field_type_integer, parent);
	copy = bt_ctf_field_type_integer_create(integer->declaration.len);
	if (!copy) {
		goto end;
	}

	copy_integer = container_of(copy, struct bt_ctf_field_type_integer,
		parent);
	copy_integer->declaration = integer->declaration;
	if (integer->mapped_clock) {
		bt_get(integer->mapped_clock);
		copy_integer->mapped_clock = integer->mapped_clock;
	}

	copy_integer->user_byte_order = integer->user_byte_order;

end:
	return copy;
}

static
struct bt_ctf_field_type *bt_ctf_field_type_enumeration_copy(
		struct bt_ctf_field_type *type)
{
	size_t i;
	struct bt_ctf_field_type *copy = NULL, *copy_container;
	struct bt_ctf_field_type_enumeration *enumeration, *copy_enumeration;

	enumeration = container_of(type, struct bt_ctf_field_type_enumeration,
		parent);

	/* Copy the source enumeration's container */
	copy_container = bt_ctf_field_type_copy(enumeration->container);
	if (!copy_container) {
		goto end;
	}

	copy = bt_ctf_field_type_enumeration_create(copy_container);
	if (!copy) {
		goto end;
	}
	copy_enumeration = container_of(copy,
		struct bt_ctf_field_type_enumeration, parent);

	/* Copy all enumaration entries */
	for (i = 0; i < enumeration->entries->len; i++) {
		struct enumeration_mapping *mapping = g_ptr_array_index(
			enumeration->entries, i);
		struct enumeration_mapping* copy_mapping = g_new0(
			struct enumeration_mapping, 1);

		if (!copy_mapping) {
			goto error;
		}

		*copy_mapping = *mapping;
		g_ptr_array_add(copy_enumeration->entries, copy_mapping);
	}

	copy_enumeration->declaration = enumeration->declaration;
end:
	bt_put(copy_container);
	return copy;
error:
	bt_put(copy_container);
        BT_PUT(copy);
	return copy;
}

static
struct bt_ctf_field_type *bt_ctf_field_type_floating_point_copy(
		struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_type *copy;
	struct bt_ctf_field_type_floating_point *floating_point, *copy_float;

	floating_point = container_of(type,
		struct bt_ctf_field_type_floating_point, parent);
	copy = bt_ctf_field_type_floating_point_create();
	if (!copy) {
		goto end;
	}

	copy_float = container_of(copy,
		struct bt_ctf_field_type_floating_point, parent);
	copy_float->declaration = floating_point->declaration;
	copy_float->sign = floating_point->sign;
	copy_float->mantissa = floating_point->mantissa;
	copy_float->exp = floating_point->exp;
	copy_float->user_byte_order = floating_point->user_byte_order;
	copy_float->declaration.sign = &copy_float->sign;
	copy_float->declaration.mantissa = &copy_float->mantissa;
	copy_float->declaration.exp = &copy_float->exp;
end:
	return copy;
}

static
struct bt_ctf_field_type *bt_ctf_field_type_structure_copy(
		struct bt_ctf_field_type *type)
{
	int i;
	GHashTableIter iter;
	gpointer key, value;
	struct bt_ctf_field_type *copy;
	struct bt_ctf_field_type_structure *structure, *copy_structure;

	structure = container_of(type, struct bt_ctf_field_type_structure,
		parent);
	copy = bt_ctf_field_type_structure_create();
	if (!copy) {
		goto end;
	}

	copy_structure = container_of(copy,
		struct bt_ctf_field_type_structure, parent);

	/* Copy field_name_to_index */
	g_hash_table_iter_init(&iter, structure->field_name_to_index);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		g_hash_table_insert(copy_structure->field_name_to_index,
			key, value);
	}

	for (i = 0; i < structure->fields->len; i++) {
		struct structure_field *entry, *copy_entry;
		struct bt_ctf_field_type *copy_field;

		copy_entry = g_new0(struct structure_field, 1);
		if (!copy_entry) {
			goto error;
		}

		entry = g_ptr_array_index(structure->fields, i);
		copy_field = bt_ctf_field_type_copy(entry->type);
		if (!copy_field) {
			g_free(copy_entry);
			goto error;
		}

		copy_entry->name = entry->name;
		copy_entry->type = copy_field;
		g_ptr_array_add(copy_structure->fields, copy_entry);
	}

	copy_structure->declaration = structure->declaration;
end:
	return copy;
error:
        BT_PUT(copy);
	return copy;
}

static
struct bt_ctf_field_type *bt_ctf_field_type_variant_copy(
		struct bt_ctf_field_type *type)
{
	int i;
	GHashTableIter iter;
	gpointer key, value;
	struct bt_ctf_field_type *copy = NULL, *copy_tag = NULL;
	struct bt_ctf_field_type_variant *variant, *copy_variant;

	variant = container_of(type, struct bt_ctf_field_type_variant,
		parent);
	if (variant->tag) {
		copy_tag = bt_ctf_field_type_copy(&variant->tag->parent);
		if (!copy_tag) {
			goto end;
		}
	}

	copy = bt_ctf_field_type_variant_create(copy_tag,
		variant->tag_name->len ? variant->tag_name->str : NULL);
	if (!copy) {
		goto end;
	}

	copy_variant = container_of(copy, struct bt_ctf_field_type_variant,
		parent);

	/* Copy field_name_to_index */
	g_hash_table_iter_init(&iter, variant->field_name_to_index);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		g_hash_table_insert(copy_variant->field_name_to_index,
			key, value);
	}

	for (i = 0; i < variant->fields->len; i++) {
		struct structure_field *entry, *copy_entry;
		struct bt_ctf_field_type *copy_field;

		copy_entry = g_new0(struct structure_field, 1);
		if (!copy_entry) {
			goto error;
		}

		entry = g_ptr_array_index(variant->fields, i);
		copy_field = bt_ctf_field_type_copy(entry->type);
		if (!copy_field) {
			g_free(copy_entry);
			goto error;
		}

		copy_entry->name = entry->name;
		copy_entry->type = copy_field;
		g_ptr_array_add(copy_variant->fields, copy_entry);
	}

	copy_variant->declaration = variant->declaration;
	if (variant->tag_field_path) {
		copy_variant->tag_field_path = bt_ctf_field_path_copy(
			variant->tag_field_path);
		if (!copy_variant->tag_field_path) {
			goto error;
		}
	}
end:
	bt_put(copy_tag);
	return copy;
error:
	bt_put(copy_tag);
        BT_PUT(copy);
	return copy;
}

static
struct bt_ctf_field_type *bt_ctf_field_type_array_copy(
		struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_type *copy = NULL, *copy_element;
	struct bt_ctf_field_type_array *array, *copy_array;

	array = container_of(type, struct bt_ctf_field_type_array,
		parent);
	copy_element = bt_ctf_field_type_copy(array->element_type);
	if (!copy_element) {
		goto end;
	}

	copy = bt_ctf_field_type_array_create(copy_element, array->length);
	if (!copy) {
		goto end;
	}

	copy_array = container_of(copy, struct bt_ctf_field_type_array,
		parent);
	copy_array->declaration = array->declaration;
end:
	bt_put(copy_element);
	return copy;
}

static
struct bt_ctf_field_type *bt_ctf_field_type_sequence_copy(
		struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_type *copy = NULL, *copy_element;
	struct bt_ctf_field_type_sequence *sequence, *copy_sequence;

	sequence = container_of(type, struct bt_ctf_field_type_sequence,
		parent);
	copy_element = bt_ctf_field_type_copy(sequence->element_type);
	if (!copy_element) {
		goto end;
	}

	copy = bt_ctf_field_type_sequence_create(copy_element,
		sequence->length_field_name->len ?
			sequence->length_field_name->str : NULL);
	if (!copy) {
		goto end;
	}

	copy_sequence = container_of(copy, struct bt_ctf_field_type_sequence,
		parent);
	copy_sequence->declaration = sequence->declaration;
	if (sequence->length_field_path) {
		copy_sequence->length_field_path = bt_ctf_field_path_copy(
			sequence->length_field_path);
		if (!copy_sequence->length_field_path) {
			goto error;
		}
	}
end:
	bt_put(copy_element);
	return copy;
error:
	BT_PUT(copy);
	goto end;
}

static
struct bt_ctf_field_type *bt_ctf_field_type_string_copy(
		struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_type *copy;
	struct bt_ctf_field_type_string *string, *copy_string;

	copy = bt_ctf_field_type_string_create();
	if (!copy) {
		goto end;
	}

	string = container_of(type, struct bt_ctf_field_type_string,
		parent);
	copy_string = container_of(type, struct bt_ctf_field_type_string,
		parent);
	copy_string->declaration = string->declaration;
end:
	return copy;
}

static
int bt_ctf_field_type_integer_compare(struct bt_ctf_field_type *type_a,
		struct bt_ctf_field_type *type_b)
{
	int ret = 1;
	struct bt_ctf_field_type_integer *integer_a;
	struct bt_ctf_field_type_integer *integer_b;
	struct declaration_integer *decl_a;
	struct declaration_integer *decl_b;

	integer_a = container_of(type_a, struct bt_ctf_field_type_integer,
		parent);
	integer_b = container_of(type_b, struct bt_ctf_field_type_integer,
		parent);
	decl_a = &integer_a->declaration;
	decl_b = &integer_b->declaration;

	/* Length */
	if (decl_a->len != decl_b->len) {
		goto end;
	}

	/*
	 * Compare user byte orders only, not the cached,
	 * real byte orders.
	 */
	if (integer_a->user_byte_order != integer_b->user_byte_order) {
		goto end;
	}

	/* Signedness */
	if (decl_a->signedness != decl_b->signedness) {
		goto end;
	}

	/* Base */
	if (decl_a->base != decl_b->base) {
		goto end;
	}

	/* Encoding */
	if (decl_a->encoding != decl_b->encoding) {
		goto end;
	}

	/* Mapped clock */
	if (integer_a->mapped_clock != integer_b->mapped_clock) {
		goto end;
	}

	/* Equal */
	ret = 0;

end:
	return ret;
}

static
int bt_ctf_field_type_floating_point_compare(struct bt_ctf_field_type *type_a,
		struct bt_ctf_field_type *type_b)
{
	int ret = 1;
	struct bt_ctf_field_type_floating_point *float_a;
	struct bt_ctf_field_type_floating_point *float_b;

	float_a = container_of(type_a,
		struct bt_ctf_field_type_floating_point, parent);
	float_b = container_of(type_b,
		struct bt_ctf_field_type_floating_point, parent);

	/* Sign length */
	if (float_a->sign.len != float_b->sign.len) {
		goto end;
	}

	/* Exponent length */
	if (float_a->exp.len != float_b->exp.len) {
		goto end;
	}

	/* Mantissa length */
	if (float_a->mantissa.len != float_b->mantissa.len) {
		goto end;
	}

	/*
	 * Compare user byte orders only, not the cached,
	 * real byte orders.
	 */
	if (float_a->user_byte_order != float_b->user_byte_order) {
		goto end;
	}

	/* Equal */
	ret = 0;

end:
	return ret;
}

static
int compare_enumeration_mappings(struct enumeration_mapping *mapping_a,
		struct enumeration_mapping *mapping_b)
{
	int ret = 1;

	/* Label */
	if (mapping_a->string != mapping_b->string) {
		goto end;
	}

	/* Range start */
	if (mapping_a->range_start._unsigned !=
			mapping_b->range_start._unsigned) {
		goto end;
	}

	/* Range end */
	if (mapping_a->range_end._unsigned !=
			mapping_b->range_end._unsigned) {
		goto end;
	}

	/* Equal */
	ret = 0;

end:
	return ret;
}

static
int bt_ctf_field_type_enumeration_compare(struct bt_ctf_field_type *type_a,
		struct bt_ctf_field_type *type_b)
{
	int ret = 1;
	int i;
	struct bt_ctf_field_type_enumeration *enum_a;
	struct bt_ctf_field_type_enumeration *enum_b;

	enum_a = container_of(type_a,
		struct bt_ctf_field_type_enumeration, parent);
	enum_b = container_of(type_b,
		struct bt_ctf_field_type_enumeration, parent);

	/* Container field type */
	ret = bt_ctf_field_type_compare(enum_a->container, enum_b->container);
	if (ret) {
		goto end;
	}

	ret = 1;

	/* Entries */
	if (enum_a->entries->len != enum_b->entries->len) {
		goto end;
	}

	for (i = 0; i < enum_a->entries->len; ++i) {
		struct enumeration_mapping *mapping_a =
			g_ptr_array_index(enum_a->entries, i);
		struct enumeration_mapping *mapping_b =
			g_ptr_array_index(enum_b->entries, i);

		if (compare_enumeration_mappings(mapping_a, mapping_b)) {
			goto end;
		}
	}

	/* Equal */
	ret = 0;

end:
	return ret;
}

static
int bt_ctf_field_type_string_compare(struct bt_ctf_field_type *type_a,
		struct bt_ctf_field_type *type_b)
{
	int ret = 1;
	struct bt_ctf_field_type_string *string_a;
	struct bt_ctf_field_type_string *string_b;

	string_a = container_of(type_a,
		struct bt_ctf_field_type_string, parent);
	string_b = container_of(type_b,
		struct bt_ctf_field_type_string, parent);

	/* Encoding */
	if (string_a->declaration.encoding != string_b->declaration.encoding) {
		goto end;
	}

	/* Equal */
	ret = 0;

end:
	return ret;
}

static
int compare_structure_fields(struct structure_field *field_a,
		struct structure_field *field_b)
{
	int ret = 1;

	/* Label */
	if (field_a->name != field_b->name) {
		goto end;
	}

	/* Type */
	ret = bt_ctf_field_type_compare(field_a->type, field_b->type);

end:
	return ret;
}

static
int bt_ctf_field_type_structure_compare(struct bt_ctf_field_type *type_a,
		struct bt_ctf_field_type *type_b)
{
	int ret = 1;
	int i;
	struct bt_ctf_field_type_structure *struct_a;
	struct bt_ctf_field_type_structure *struct_b;

	struct_a = container_of(type_a,
		struct bt_ctf_field_type_structure, parent);
	struct_b = container_of(type_b,
		struct bt_ctf_field_type_structure, parent);

	/* Alignment */
	if (bt_ctf_field_type_get_alignment(type_a) !=
			bt_ctf_field_type_get_alignment(type_b)) {
		goto end;
	}

	/* Fields */
	if (struct_a->fields->len != struct_b->fields->len) {
		goto end;
	}

	for (i = 0; i < struct_a->fields->len; ++i) {
		struct structure_field *field_a =
			g_ptr_array_index(struct_a->fields, i);
		struct structure_field *field_b =
			g_ptr_array_index(struct_b->fields, i);

		ret = compare_structure_fields(field_a, field_b);
		if (ret) {
			goto end;
		}
	}

	/* Equal */
	ret = 0;

end:
	return ret;
}

static
int bt_ctf_field_type_variant_compare(struct bt_ctf_field_type *type_a,
		struct bt_ctf_field_type *type_b)
{
	int ret = 1;
	int i;
	struct bt_ctf_field_type_variant *variant_a;
	struct bt_ctf_field_type_variant *variant_b;

	variant_a = container_of(type_a,
		struct bt_ctf_field_type_variant, parent);
	variant_b = container_of(type_b,
		struct bt_ctf_field_type_variant, parent);

	/* Tag name */
	if (strcmp(variant_a->tag_name->str, variant_b->tag_name->str)) {
		goto end;
	}

	/* Tag type */
	ret = bt_ctf_field_type_compare(
		(struct bt_ctf_field_type *) variant_a->tag,
		(struct bt_ctf_field_type *) variant_b->tag);
	if (ret) {
		goto end;
	}

	ret = 1;

	/* Fields */
	if (variant_a->fields->len != variant_b->fields->len) {
		goto end;
	}

	for (i = 0; i < variant_a->fields->len; ++i) {
		struct structure_field *field_a =
			g_ptr_array_index(variant_a->fields, i);
		struct structure_field *field_b =
			g_ptr_array_index(variant_b->fields, i);

		ret = compare_structure_fields(field_a, field_b);
		if (ret) {
			goto end;
		}
	}

	/* Equal */
	ret = 0;

end:
	return ret;
}

static
int bt_ctf_field_type_array_compare(struct bt_ctf_field_type *type_a,
		struct bt_ctf_field_type *type_b)
{
	int ret = 1;
	struct bt_ctf_field_type_array *array_a;
	struct bt_ctf_field_type_array *array_b;

	array_a = container_of(type_a,
		struct bt_ctf_field_type_array, parent);
	array_b = container_of(type_b,
		struct bt_ctf_field_type_array, parent);

	/* Length */
	if (array_a->length != array_b->length) {
		goto end;
	}

	/* Element type */
	ret = bt_ctf_field_type_compare(array_a->element_type,
		array_b->element_type);

end:
	return ret;
}

static
int bt_ctf_field_type_sequence_compare(struct bt_ctf_field_type *type_a,
		struct bt_ctf_field_type *type_b)
{
	int ret = -1;
	struct bt_ctf_field_type_sequence *sequence_a;
	struct bt_ctf_field_type_sequence *sequence_b;

	sequence_a = container_of(type_a,
		struct bt_ctf_field_type_sequence, parent);
	sequence_b = container_of(type_b,
		struct bt_ctf_field_type_sequence, parent);

	/* Length name */
	if (strcmp(sequence_a->length_field_name->str,
			sequence_b->length_field_name->str)) {
		goto end;
	}

	/* Element type */
	ret = bt_ctf_field_type_compare(sequence_a->element_type,
			sequence_b->element_type);

end:
	return ret;
}

BT_HIDDEN
int bt_ctf_field_type_compare(struct bt_ctf_field_type *type_a,
		struct bt_ctf_field_type *type_b)
{
	int ret = 1;

	if (type_a == type_b) {
		/* Same reference: equal (even if both are NULL) */
		ret = 0;
		goto end;
	}

	if (!type_a || !type_b) {
		ret = -1;
		goto end;
	}

	if (type_a->declaration->id != type_b->declaration->id) {
		/* Different type IDs */
		goto end;
	}

	if (type_a->declaration->id == CTF_TYPE_UNKNOWN) {
		/* Both have unknown type IDs */
		goto end;
	}

	ret = type_compare_funcs[type_a->declaration->id](type_a, type_b);

end:
	return ret;
}

BT_HIDDEN
int bt_ctf_field_type_get_field_count(struct bt_ctf_field_type *field_type)
{
	int field_count = -1;
	enum ctf_type_id type_id = bt_ctf_field_type_get_type_id(field_type);

	switch (type_id) {
	case CTF_TYPE_STRUCT:
		field_count =
			bt_ctf_field_type_structure_get_field_count(field_type);
		break;
	case CTF_TYPE_VARIANT:
		field_count =
			bt_ctf_field_type_variant_get_field_count(field_type);
		break;
	case CTF_TYPE_ARRAY:
	case CTF_TYPE_SEQUENCE:
		/*
		 * Array and sequence types always contain a single member
		 * (the element type).
		 */
		field_count = 1;
		break;
	default:
		break;
	}

	return field_count;
}

BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_field_type_get_field_at_index(
		struct bt_ctf_field_type *field_type, int index)
{
	struct bt_ctf_field_type *field = NULL;
	enum ctf_type_id type_id = bt_ctf_field_type_get_type_id(field_type);

	switch (type_id) {
	case CTF_TYPE_STRUCT:
		bt_ctf_field_type_structure_get_field(field_type, NULL, &field,
			index);
		break;
	case CTF_TYPE_VARIANT:
	{
		int ret = bt_ctf_field_type_variant_get_field(field_type, NULL,
			&field, index);
		if (ret) {
			field = NULL;
			goto end;
		}
		break;
	}
	case CTF_TYPE_ARRAY:
		field = bt_ctf_field_type_array_get_element_type(field_type);
		break;
	case CTF_TYPE_SEQUENCE:
		field = bt_ctf_field_type_sequence_get_element_type(field_type);
		break;
	default:
		break;
	}
end:
	return field;
}

BT_HIDDEN
int bt_ctf_field_type_get_field_index(struct bt_ctf_field_type *field_type,
		const char *name)
{
	int field_index = -1;
	enum ctf_type_id type_id = bt_ctf_field_type_get_type_id(field_type);

	switch (type_id) {
	case CTF_TYPE_STRUCT:
		field_index = bt_ctf_field_type_structure_get_field_name_index(
			field_type, name);
		break;
	case CTF_TYPE_VARIANT:
		field_index = bt_ctf_field_type_variant_get_field_name_index(
			field_type, name);
		break;
	default:
		break;
	}

	return field_index;
}

BT_HIDDEN
struct bt_ctf_field_path *bt_ctf_field_type_variant_get_tag_field_path(
		struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_path *field_path = NULL;
	struct bt_ctf_field_type_variant *variant;

	if (!type || !bt_ctf_field_type_is_variant(type)) {
		goto end;
	}

	variant = container_of(type, struct bt_ctf_field_type_variant,
			parent);
	field_path = bt_get(variant->tag_field_path);
end:
	return field_path;
}

BT_HIDDEN
struct bt_ctf_field_path *bt_ctf_field_type_sequence_get_length_field_path(
		struct bt_ctf_field_type *type)
{
	struct bt_ctf_field_path *field_path = NULL;
	struct bt_ctf_field_type_sequence *sequence;

	if (!type || !bt_ctf_field_type_is_sequence(type)) {
		goto end;
	}

	sequence = container_of(type, struct bt_ctf_field_type_sequence,
			parent);
	field_path = bt_get(sequence->length_field_path);
end:
	return field_path;
}
