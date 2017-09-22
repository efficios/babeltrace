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

#define BT_LOG_TAG "FIELD-TYPES"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/ctf-ir/field-types-internal.h>
#include <babeltrace/ctf-ir/field-path-internal.h>
#include <babeltrace/ctf-ir/utils.h>
#include <babeltrace/ref.h>
#include <babeltrace/ctf-ir/clock-class.h>
#include <babeltrace/ctf-ir/clock-class-internal.h>
#include <babeltrace/ctf-writer/writer-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/endian-internal.h>
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
void bt_field_type_destroy(struct bt_object *);
static
void bt_field_type_integer_destroy(struct bt_field_type *);
static
void bt_field_type_enumeration_destroy(struct bt_field_type *);
static
void bt_field_type_floating_point_destroy(struct bt_field_type *);
static
void bt_field_type_structure_destroy(struct bt_field_type *);
static
void bt_field_type_variant_destroy(struct bt_field_type *);
static
void bt_field_type_array_destroy(struct bt_field_type *);
static
void bt_field_type_sequence_destroy(struct bt_field_type *);
static
void bt_field_type_string_destroy(struct bt_field_type *);

static
void (* const type_destroy_funcs[])(struct bt_field_type *) = {
	[BT_FIELD_TYPE_ID_INTEGER] = bt_field_type_integer_destroy,
	[BT_FIELD_TYPE_ID_ENUM] =
		bt_field_type_enumeration_destroy,
	[BT_FIELD_TYPE_ID_FLOAT] =
		bt_field_type_floating_point_destroy,
	[BT_FIELD_TYPE_ID_STRUCT] = bt_field_type_structure_destroy,
	[BT_FIELD_TYPE_ID_VARIANT] = bt_field_type_variant_destroy,
	[BT_FIELD_TYPE_ID_ARRAY] = bt_field_type_array_destroy,
	[BT_FIELD_TYPE_ID_SEQUENCE] = bt_field_type_sequence_destroy,
	[BT_FIELD_TYPE_ID_STRING] = bt_field_type_string_destroy,
};

static
void generic_field_type_freeze(struct bt_field_type *);
static
void bt_field_type_integer_freeze(struct bt_field_type *);
static
void bt_field_type_enumeration_freeze(struct bt_field_type *);
static
void bt_field_type_structure_freeze(struct bt_field_type *);
static
void bt_field_type_variant_freeze(struct bt_field_type *);
static
void bt_field_type_array_freeze(struct bt_field_type *);
static
void bt_field_type_sequence_freeze(struct bt_field_type *);

static
type_freeze_func const type_freeze_funcs[] = {
	[BT_FIELD_TYPE_ID_INTEGER] = bt_field_type_integer_freeze,
	[BT_FIELD_TYPE_ID_ENUM] = bt_field_type_enumeration_freeze,
	[BT_FIELD_TYPE_ID_FLOAT] = generic_field_type_freeze,
	[BT_FIELD_TYPE_ID_STRUCT] = bt_field_type_structure_freeze,
	[BT_FIELD_TYPE_ID_VARIANT] = bt_field_type_variant_freeze,
	[BT_FIELD_TYPE_ID_ARRAY] = bt_field_type_array_freeze,
	[BT_FIELD_TYPE_ID_SEQUENCE] = bt_field_type_sequence_freeze,
	[BT_FIELD_TYPE_ID_STRING] = generic_field_type_freeze,
};

static
int bt_field_type_integer_serialize(struct bt_field_type *,
		struct metadata_context *);
static
int bt_field_type_enumeration_serialize(struct bt_field_type *,
		struct metadata_context *);
static
int bt_field_type_floating_point_serialize(
		struct bt_field_type *, struct metadata_context *);
static
int bt_field_type_structure_serialize(struct bt_field_type *,
		struct metadata_context *);
static
int bt_field_type_variant_serialize(struct bt_field_type *,
		struct metadata_context *);
static
int bt_field_type_array_serialize(struct bt_field_type *,
		struct metadata_context *);
static
int bt_field_type_sequence_serialize(struct bt_field_type *,
		struct metadata_context *);
static
int bt_field_type_string_serialize(struct bt_field_type *,
		struct metadata_context *);

static
type_serialize_func const type_serialize_funcs[] = {
	[BT_FIELD_TYPE_ID_INTEGER] = bt_field_type_integer_serialize,
	[BT_FIELD_TYPE_ID_ENUM] =
		bt_field_type_enumeration_serialize,
	[BT_FIELD_TYPE_ID_FLOAT] =
		bt_field_type_floating_point_serialize,
	[BT_FIELD_TYPE_ID_STRUCT] =
		bt_field_type_structure_serialize,
	[BT_FIELD_TYPE_ID_VARIANT] = bt_field_type_variant_serialize,
	[BT_FIELD_TYPE_ID_ARRAY] = bt_field_type_array_serialize,
	[BT_FIELD_TYPE_ID_SEQUENCE] = bt_field_type_sequence_serialize,
	[BT_FIELD_TYPE_ID_STRING] = bt_field_type_string_serialize,
};

static
void bt_field_type_integer_set_byte_order(struct bt_field_type *,
		enum bt_byte_order byte_order);
static
void bt_field_type_enumeration_set_byte_order(struct bt_field_type *,
		enum bt_byte_order byte_order);
static
void bt_field_type_floating_point_set_byte_order(
		struct bt_field_type *, enum bt_byte_order byte_order);
static
void bt_field_type_structure_set_byte_order(struct bt_field_type *,
		enum bt_byte_order byte_order);
static
void bt_field_type_variant_set_byte_order(struct bt_field_type *,
		enum bt_byte_order byte_order);
static
void bt_field_type_array_set_byte_order(struct bt_field_type *,
		enum bt_byte_order byte_order);
static
void bt_field_type_sequence_set_byte_order(struct bt_field_type *,
		enum bt_byte_order byte_order);

static
void (* const set_byte_order_funcs[])(struct bt_field_type *,
		enum bt_byte_order) = {
	[BT_FIELD_TYPE_ID_INTEGER] = bt_field_type_integer_set_byte_order,
	[BT_FIELD_TYPE_ID_ENUM] =
		bt_field_type_enumeration_set_byte_order,
	[BT_FIELD_TYPE_ID_FLOAT] =
		bt_field_type_floating_point_set_byte_order,
	[BT_FIELD_TYPE_ID_STRUCT] =
		bt_field_type_structure_set_byte_order,
	[BT_FIELD_TYPE_ID_VARIANT] = bt_field_type_variant_set_byte_order,
	[BT_FIELD_TYPE_ID_ARRAY] = bt_field_type_array_set_byte_order,
	[BT_FIELD_TYPE_ID_SEQUENCE] = bt_field_type_sequence_set_byte_order,
	[BT_FIELD_TYPE_ID_STRING] = NULL,
};

static
struct bt_field_type *bt_field_type_integer_copy(
		struct bt_field_type *);
static
struct bt_field_type *bt_field_type_enumeration_copy(
		struct bt_field_type *);
static
struct bt_field_type *bt_field_type_floating_point_copy(
		struct bt_field_type *);
static
struct bt_field_type *bt_field_type_structure_copy(
		struct bt_field_type *);
static
struct bt_field_type *bt_field_type_variant_copy(
		struct bt_field_type *);
static
struct bt_field_type *bt_field_type_array_copy(
		struct bt_field_type *);
static
struct bt_field_type *bt_field_type_sequence_copy(
		struct bt_field_type *);
static
struct bt_field_type *bt_field_type_string_copy(
		struct bt_field_type *);

static
struct bt_field_type *(* const type_copy_funcs[])(
		struct bt_field_type *) = {
	[BT_FIELD_TYPE_ID_INTEGER] = bt_field_type_integer_copy,
	[BT_FIELD_TYPE_ID_ENUM] = bt_field_type_enumeration_copy,
	[BT_FIELD_TYPE_ID_FLOAT] = bt_field_type_floating_point_copy,
	[BT_FIELD_TYPE_ID_STRUCT] = bt_field_type_structure_copy,
	[BT_FIELD_TYPE_ID_VARIANT] = bt_field_type_variant_copy,
	[BT_FIELD_TYPE_ID_ARRAY] = bt_field_type_array_copy,
	[BT_FIELD_TYPE_ID_SEQUENCE] = bt_field_type_sequence_copy,
	[BT_FIELD_TYPE_ID_STRING] = bt_field_type_string_copy,
};

static
int bt_field_type_integer_compare(struct bt_field_type *,
		struct bt_field_type *);
static
int bt_field_type_floating_point_compare(struct bt_field_type *,
		struct bt_field_type *);
static
int bt_field_type_enumeration_compare(struct bt_field_type *,
		struct bt_field_type *);
static
int bt_field_type_string_compare(struct bt_field_type *,
		struct bt_field_type *);
static
int bt_field_type_structure_compare(struct bt_field_type *,
		struct bt_field_type *);
static
int bt_field_type_variant_compare(struct bt_field_type *,
		struct bt_field_type *);
static
int bt_field_type_array_compare(struct bt_field_type *,
		struct bt_field_type *);
static
int bt_field_type_sequence_compare(struct bt_field_type *,
		struct bt_field_type *);

static
int (* const type_compare_funcs[])(struct bt_field_type *,
		struct bt_field_type *) = {
	[BT_FIELD_TYPE_ID_INTEGER] = bt_field_type_integer_compare,
	[BT_FIELD_TYPE_ID_ENUM] = bt_field_type_enumeration_compare,
	[BT_FIELD_TYPE_ID_FLOAT] = bt_field_type_floating_point_compare,
	[BT_FIELD_TYPE_ID_STRUCT] = bt_field_type_structure_compare,
	[BT_FIELD_TYPE_ID_VARIANT] = bt_field_type_variant_compare,
	[BT_FIELD_TYPE_ID_ARRAY] = bt_field_type_array_compare,
	[BT_FIELD_TYPE_ID_SEQUENCE] = bt_field_type_sequence_compare,
	[BT_FIELD_TYPE_ID_STRING] = bt_field_type_string_compare,
};

static
int bt_field_type_integer_validate(struct bt_field_type *);
static
int bt_field_type_enumeration_validate(struct bt_field_type *);
static
int bt_field_type_structure_validate(struct bt_field_type *);
static
int bt_field_type_variant_validate(struct bt_field_type *);
static
int bt_field_type_array_validate(struct bt_field_type *);
static
int bt_field_type_sequence_validate(struct bt_field_type *);

static
int (* const type_validate_funcs[])(struct bt_field_type *) = {
	[BT_FIELD_TYPE_ID_INTEGER] = bt_field_type_integer_validate,
	[BT_FIELD_TYPE_ID_FLOAT] = NULL,
	[BT_FIELD_TYPE_ID_STRING] = NULL,
	[BT_FIELD_TYPE_ID_ENUM] = bt_field_type_enumeration_validate,
	[BT_FIELD_TYPE_ID_STRUCT] = bt_field_type_structure_validate,
	[BT_FIELD_TYPE_ID_VARIANT] = bt_field_type_variant_validate,
	[BT_FIELD_TYPE_ID_ARRAY] = bt_field_type_array_validate,
	[BT_FIELD_TYPE_ID_SEQUENCE] = bt_field_type_sequence_validate,
};

static
void destroy_enumeration_mapping(struct enumeration_mapping *mapping)
{
	g_free(mapping);
}

static
void destroy_structure_field(struct structure_field *field)
{
	if (!field) {
		return;
	}

	BT_LOGD("Destroying structure/variant field type's field object: "
		"addr=%p, field-ft-addr=%p, field-name=\"%s\"",
		field, field->type, g_quark_to_string(field->name));
	BT_LOGD_STR("Putting field type.");
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

	if (overlap_query->overlaps) {
		BT_LOGV("Overlapping enumeration field type mappings: "
			"mapping-name=\"%s\", "
			"mapping-a-range-start=%" PRId64 ", "
			"mapping-a-range-end=%" PRId64 ", "
			"mapping-b-range-start=%" PRId64 ", "
			"mapping-b-range-end=%" PRId64,
			g_quark_to_string(mapping->string),
			mapping->range_start._signed,
			mapping->range_end._signed,
			overlap_query->range_start._signed,
			overlap_query->range_end._signed);
	}
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

	if (overlap_query->overlaps) {
		BT_LOGW("Overlapping enumeration field type mappings: "
			"mapping-name=\"%s\", "
			"mapping-a-range-start=%" PRIu64 ", "
			"mapping-a-range-end=%" PRIu64 ", "
			"mapping-b-range-start=%" PRIu64 ", "
			"mapping-b-range-end=%" PRIu64,
			g_quark_to_string(mapping->string),
			mapping->range_start._unsigned,
			mapping->range_end._unsigned,
			overlap_query->range_start._unsigned,
			overlap_query->range_end._unsigned);
	}
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
void bt_field_type_init(struct bt_field_type *type, bt_bool init_bo)
{
	assert(type && (type->id > BT_FIELD_TYPE_ID_UNKNOWN) &&
		(type->id < BT_FIELD_TYPE_ID_NR));

	bt_object_init(type, bt_field_type_destroy);
	type->freeze = type_freeze_funcs[type->id];
	type->serialize = type_serialize_funcs[type->id];

	if (init_bo) {
		int ret;
		const enum bt_byte_order bo = BT_BYTE_ORDER_NATIVE;

		BT_LOGD("Setting initial field type's byte order: bo=%s",
			bt_byte_order_string(bo));
		ret = bt_field_type_set_byte_order(type, bo);
		assert(ret == 0);
	}

	type->alignment = 1;
}

static
int add_structure_field(GPtrArray *fields,
		GHashTable *field_name_to_index,
		struct bt_field_type *field_type,
		const char *field_name)
{
	int ret = 0;
	GQuark name_quark = g_quark_from_string(field_name);
	GQuark underscore_name_quark;
	struct structure_field *field;
	GString *underscore_name = g_string_new(NULL);

	if (!underscore_name) {
		BT_LOGE_STR("Failed to allocate a GString.");
		ret = -1;
		goto end;
	}

	g_string_assign(underscore_name, "_");
	g_string_append(underscore_name, field_name);
	underscore_name_quark = g_quark_from_string(underscore_name->str);

	/* Make sure structure does not contain a field of the same name */
	if (g_hash_table_lookup_extended(field_name_to_index,
			GUINT_TO_POINTER(name_quark), NULL, NULL)) {
		BT_LOGW("Structure or variant field type already contains a field type with this name: "
			"field-name=\"%s\"", field_name);
		ret = -1;
		goto end;
	}

	if (g_hash_table_lookup_extended(field_name_to_index,
			GUINT_TO_POINTER(underscore_name_quark), NULL, NULL)) {
		BT_LOGW("Structure or variant field type already contains a field type with this name: "
			"field-name=\"%s\"", underscore_name->str);
		ret = -1;
		goto end;
	}

	field = g_new0(struct structure_field, 1);
	if (!field) {
		BT_LOGE_STR("Failed to allocate one structure/variant field type field.");
		ret = -1;
		goto end;
	}

	bt_get(field_type);
	field->name = name_quark;
	field->type = field_type;
	g_hash_table_insert(field_name_to_index,
		GUINT_TO_POINTER(name_quark),
		GUINT_TO_POINTER(fields->len));
	g_hash_table_insert(field_name_to_index,
		GUINT_TO_POINTER(underscore_name_quark),
		GUINT_TO_POINTER(fields->len));
	g_ptr_array_add(fields, field);
	BT_LOGV("Added structure/variant field type field: field-ft-addr=%p, "
		"field-name=\"%s\"", field_type, field_name);
end:
	g_string_free(underscore_name, TRUE);
	return ret;
}

static
void bt_field_type_destroy(struct bt_object *obj)
{
	struct bt_field_type *type;
	enum bt_field_type_id type_id;

	type = container_of(obj, struct bt_field_type, base);
	type_id = type->id;
	assert(type_id > BT_FIELD_TYPE_ID_UNKNOWN &&
		type_id < BT_FIELD_TYPE_ID_NR);
	type_destroy_funcs[type_id](type);
}

static
int bt_field_type_integer_validate(struct bt_field_type *type)
{
	int ret = 0;

	struct bt_field_type_integer *integer =
		container_of(type, struct bt_field_type_integer,
			parent);

	if (integer->mapped_clock && integer->is_signed) {
		BT_LOGW("Invalid integer field type: cannot be signed and have a mapped clock class: "
			"ft-addr=%p, clock-class-addr=%p, clock-class-name=\"%s\"",
			type, integer->mapped_clock,
			bt_clock_class_get_name(integer->mapped_clock));
		ret = -1;
		goto end;
	}

end:
	return ret;
}

static
struct enumeration_mapping *get_enumeration_mapping(
		struct bt_field_type *type, uint64_t index)
{
	struct enumeration_mapping *mapping = NULL;
	struct bt_field_type_enumeration *enumeration;

	enumeration = container_of(type, struct bt_field_type_enumeration,
		parent);
	if (index >= enumeration->entries->len) {
		BT_LOGW("Invalid parameter: index is out of bounds: "
			"addr=%p, index=%" PRIu64 ", count=%u",
			type, index, enumeration->entries->len);
		goto end;
	}

	mapping = g_ptr_array_index(enumeration->entries, index);
end:
	return mapping;
}

/*
 * Note: This algorithm is O(n^2) vs number of enumeration mappings.
 * Only used when freezing an enumeration.
 */
static
void set_enumeration_range_overlap(
		struct bt_field_type *type)
{
	int64_t i, j, len;
	struct bt_field_type *container_type;
	struct bt_field_type_enumeration *enumeration_type;
	int is_signed;

	BT_LOGV("Setting enumeration field type's overlap flag: addr=%p",
		type);
	enumeration_type = container_of(type,
			struct bt_field_type_enumeration, parent);

	len = enumeration_type->entries->len;
	container_type = enumeration_type->container;
	is_signed = bt_field_type_integer_is_signed(container_type);

	for (i = 0; i < len; i++) {
		for (j = i + 1; j < len; j++) {
			struct enumeration_mapping *mapping[2];

			mapping[0] = get_enumeration_mapping(type, i);
			mapping[1] = get_enumeration_mapping(type, j);
			if (is_signed) {
				if (mapping[0]->range_start._signed
							<= mapping[1]->range_end._signed
						&& mapping[0]->range_end._signed
							>= mapping[1]->range_start._signed) {
					enumeration_type->has_overlapping_ranges = BT_TRUE;
					goto end;
				}
			} else {
				if (mapping[0]->range_start._unsigned
							<= mapping[1]->range_end._unsigned
						&& mapping[0]->range_end._unsigned
							>= mapping[1]->range_start._unsigned) {
					enumeration_type->has_overlapping_ranges = BT_TRUE;
					goto end;
				}
			}
		}
	}

end:
	if (enumeration_type->has_overlapping_ranges) {
		BT_LOGV_STR("Enumeration field type has overlapping ranges.");
	} else {
		BT_LOGV_STR("Enumeration field type has no overlapping ranges.");
	}
}

static
int bt_field_type_enumeration_validate(struct bt_field_type *type)
{
	int ret = 0;

	struct bt_field_type_enumeration *enumeration =
		container_of(type, struct bt_field_type_enumeration,
			parent);
	struct bt_field_type *container_type =
		bt_field_type_enumeration_get_container_type(type);

	assert(container_type);
	ret = bt_field_type_validate(container_type);
	if (ret) {
		BT_LOGW("Invalid enumeration field type: container type is invalid: "
			"enum-ft-addr=%p, int-ft-addr=%p",
			type, container_type);
		goto end;
	}

	/* Ensure enum has entries */
	if (enumeration->entries->len == 0) {
		BT_LOGW("Invalid enumeration field type: no entries: "
			"addr=%p", type);
		ret = -1;
		goto end;
	}

end:
	BT_PUT(container_type);
	return ret;
}

static
int bt_field_type_sequence_validate(struct bt_field_type *type)
{
	int ret = 0;
	struct bt_field_type *element_type = NULL;
	struct bt_field_type_sequence *sequence =
		container_of(type, struct bt_field_type_sequence,
		parent);

	/* Length field name should be set at this point */
	if (sequence->length_field_name->len == 0) {
		BT_LOGW("Invalid sequence field type: no length field name: "
			"addr=%p", type);
		ret = -1;
		goto end;
	}

	element_type = bt_field_type_sequence_get_element_type(type);
	assert(element_type);
	ret = bt_field_type_validate(element_type);
	if (ret) {
		BT_LOGW("Invalid sequence field type: invalid element field type: "
			"seq-ft-addr=%p, element-ft-add=%p",
			type, element_type);
	}

end:
	BT_PUT(element_type);

	return ret;
}

static
int bt_field_type_array_validate(struct bt_field_type *type)
{
	int ret = 0;
	struct bt_field_type *element_type = NULL;

	element_type = bt_field_type_array_get_element_type(type);
	assert(element_type);
	ret = bt_field_type_validate(element_type);
	if (ret) {
		BT_LOGW("Invalid array field type: invalid element field type: "
			"array-ft-addr=%p, element-ft-add=%p",
			type, element_type);
	}

	BT_PUT(element_type);
	return ret;
}

static
int bt_field_type_structure_validate(struct bt_field_type *type)
{
	int ret = 0;
	struct bt_field_type *child_type = NULL;
	int64_t field_count = bt_field_type_structure_get_field_count(type);
	int64_t i;

	assert(field_count >= 0);

	for (i = 0; i < field_count; ++i) {
		const char *field_name;

		ret = bt_field_type_structure_get_field_by_index(type,
			&field_name, &child_type, i);
		assert(ret == 0);
		ret = bt_field_type_validate(child_type);
		if (ret) {
			BT_LOGW("Invalid structure field type: "
				"a contained field type is invalid: "
				"struct-ft-addr=%p, field-ft-addr=%p, "
				"field-name=\"%s\", field-index=%" PRId64,
				type, child_type, field_name, i);
			goto end;
		}

		BT_PUT(child_type);
	}

end:
	BT_PUT(child_type);

	return ret;
}

static
bt_bool bt_field_type_enumeration_has_overlapping_ranges(
		struct bt_field_type_enumeration *enumeration_type)
{
	if (!enumeration_type->parent.frozen) {
		set_enumeration_range_overlap(&enumeration_type->parent);
	}
	return enumeration_type->has_overlapping_ranges;
}

static
int bt_field_type_variant_validate(struct bt_field_type *type)
{
	int ret = 0;
	int64_t field_count;
	struct bt_field_type *child_type = NULL;
	struct bt_field_type_variant *variant =
		container_of(type, struct bt_field_type_variant,
			parent);
	int64_t i;

	if (variant->tag_name->len == 0) {
		BT_LOGW("Invalid variant field type: no tag field name: "
			"addr=%p", type);
		ret = -1;
		goto end;
	}

	if (!variant->tag) {
		BT_LOGW("Invalid variant field type: no tag field type: "
			"addr=%p, tag-field-name=\"%s\"", type,
			variant->tag_name->str);
		ret = -1;
		goto end;
	}

	if (bt_field_type_enumeration_has_overlapping_ranges(
			variant->tag)) {
		BT_LOGW("Invalid variant field type: enumeration tag field type has overlapping ranges: "
			"variant-ft-addr=%p, tag-field-name=\"%s\", "
			"enum-ft-addr=%p", type, variant->tag_name->str,
			variant->tag);
		ret = -1;
		goto end;
	}

	/*
	 * It is valid to have a variant field type which does not have
	 * the fields corresponding to each label in the associated
	 * enumeration.
	 *
	 * It is also valid to have variant field type fields which
	 * cannot be selected because the variant field type tag has no
	 * mapping named as such. This scenario, while not ideal, cannot
	 * cause any error.
	 *
	 * If a non-existing field happens to be selected by an
	 * enumeration while reading a variant field, an error will be
	 * generated at that point (while reading the stream).
	 */
	field_count = bt_field_type_variant_get_field_count(type);
	if (field_count < 0) {
		BT_LOGW("Invalid variant field type: no fields: "
			"addr=%p, tag-field-name=\"%s\"",
			type, variant->tag_name->str);
		ret = -1;
		goto end;
	}

	for (i = 0; i < field_count; ++i) {
		const char *field_name;

		ret = bt_field_type_variant_get_field_by_index(type,
			&field_name, &child_type, i);
		assert(ret == 0);
		ret = bt_field_type_validate(child_type);
		if (ret) {
			BT_LOGW("Invalid variant field type: "
				"a contained field type is invalid: "
				"variant-ft-addr=%p, tag-field-name=\"%s\", "
				"field-ft-addr=%p, field-name=\"%s\", "
				"field-index=%" PRId64,
				type, variant->tag_name->str, child_type,
				field_name, i);
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
int bt_field_type_validate(struct bt_field_type *type)
{
	int ret = 0;
	enum bt_field_type_id id = bt_field_type_get_type_id(type);

	assert(type);

	if (type->valid) {
		/* Already marked as valid */
		goto end;
	}

	if (type_validate_funcs[id]) {
		ret = type_validate_funcs[id](type);
	}

	if (!ret && type->frozen) {
		/* Field type is valid */
		BT_LOGV("Marking field type as valid: addr=%p", type);
		type->valid = 1;
	}

end:
	return ret;
}

struct bt_field_type *bt_field_type_integer_create(unsigned int size)
{
	struct bt_field_type_integer *integer =
		g_new0(struct bt_field_type_integer, 1);

	BT_LOGD("Creating integer field type object: size=%u", size);

	if (!integer) {
		BT_LOGE_STR("Failed to allocate one integer field type.");
		return NULL;
	}

	if (size == 0 || size > 64) {
		BT_LOGW("Invalid parameter: size must be between 1 and 64: "
			"size=%u", size);
		return NULL;
	}

	integer->parent.id = BT_FIELD_TYPE_ID_INTEGER;
	integer->size = size;
	integer->base = BT_INTEGER_BASE_DECIMAL;
	integer->encoding = BT_STRING_ENCODING_NONE;
	bt_field_type_init(&integer->parent, TRUE);
	BT_LOGD("Created integer field type object: addr=%p, size=%u",
		&integer->parent, size);
	return &integer->parent;
}

int bt_field_type_integer_get_size(struct bt_field_type *type)
{
	int ret = 0;
	struct bt_field_type_integer *integer;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (type->id != BT_FIELD_TYPE_ID_INTEGER) {
		BT_LOGW("Invalid parameter: field type is not an integer field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		ret = -1;
		goto end;
	}

	integer = container_of(type, struct bt_field_type_integer, parent);
	ret = (int) integer->size;
end:
	return ret;
}

int bt_ctf_field_type_integer_get_signed(struct bt_field_type *type)
{
	int ret = 0;
	struct bt_field_type_integer *integer;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (type->id != BT_FIELD_TYPE_ID_INTEGER) {
		BT_LOGW("Invalid parameter: field type is not an integer field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		ret = -1;
		goto end;
	}

	integer = container_of(type, struct bt_field_type_integer, parent);
	ret = integer->is_signed;
end:
	return ret;
}

bt_bool bt_field_type_integer_is_signed(
		struct bt_field_type *int_field_type)
{
	return bt_ctf_field_type_integer_get_signed(int_field_type) ?
		BT_TRUE : BT_FALSE;
}

int bt_field_type_integer_set_is_signed(struct bt_field_type *type,
		bt_bool is_signed)
{
	int ret = 0;
	struct bt_field_type_integer *integer;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (type->frozen) {
		BT_LOGW("Invalid parameter: field type is frozen: addr=%p",
			type);
		ret = -1;
		goto end;
	}

	if (type->id != BT_FIELD_TYPE_ID_INTEGER) {
		BT_LOGW("Invalid parameter: field type is not an integer field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		ret = -1;
		goto end;
	}

	integer = container_of(type, struct bt_field_type_integer, parent);
	integer->is_signed = !!is_signed;
	BT_LOGV("Set integer field type's signedness: addr=%p, is-signed=%d",
		type, is_signed);
end:
	return ret;
}

int bt_field_type_integer_set_size(struct bt_field_type *type,
		unsigned int size)
{
	int ret = 0;
	struct bt_field_type_integer *integer;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (type->frozen) {
		BT_LOGW("Invalid parameter: field type is frozen: addr=%p",
			type);
		ret = -1;
		goto end;
	}

	if (type->id != BT_FIELD_TYPE_ID_INTEGER) {
		BT_LOGW("Invalid parameter: field type is not an integer field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		ret = -1;
		goto end;
	}

	if (size == 0 || size > 64) {
		BT_LOGW("Invalid parameter: size must be between 1 and 64: "
			"addr=%p, size=%u", type, size);
		ret = -1;
		goto end;
	}

	integer = container_of(type, struct bt_field_type_integer, parent);
	integer->size = size;
	BT_LOGV("Set integer field type's size: addr=%p, size=%u",
		type, size);
end:
	return ret;
}

enum bt_integer_base bt_field_type_integer_get_base(
		struct bt_field_type *type)
{
	enum bt_integer_base ret = BT_INTEGER_BASE_UNKNOWN;
	struct bt_field_type_integer *integer;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		goto end;
	}

	if (type->id != BT_FIELD_TYPE_ID_INTEGER) {
		BT_LOGW("Invalid parameter: field type is not an integer field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		goto end;
	}

	integer = container_of(type, struct bt_field_type_integer, parent);
	ret = integer->base;
end:
	return ret;
}

int bt_field_type_integer_set_base(struct bt_field_type *type,
		enum bt_integer_base base)
{
	int ret = 0;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (type->frozen) {
		BT_LOGW("Invalid parameter: field type is frozen: addr=%p",
			type);
		ret = -1;
		goto end;
	}

	if (type->id != BT_FIELD_TYPE_ID_INTEGER) {
		BT_LOGW("Invalid parameter: field type is not an integer field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		ret = -1;
		goto end;
	}

	switch (base) {
	case BT_INTEGER_BASE_UNSPECIFIED:
	case BT_INTEGER_BASE_BINARY:
	case BT_INTEGER_BASE_OCTAL:
	case BT_INTEGER_BASE_DECIMAL:
	case BT_INTEGER_BASE_HEXADECIMAL:
	{
		struct bt_field_type_integer *integer = container_of(type,
			struct bt_field_type_integer, parent);
		integer->base = base;
		break;
	}
	default:
		BT_LOGW("Invalid parameter: unknown integer field type base: "
			"addr=%p, base=%d", type, base);
		ret = -1;
	}

	BT_LOGV("Set integer field type's base: addr=%p, base=%s",
		type, bt_integer_base_string(base));

end:
	return ret;
}

enum bt_string_encoding bt_field_type_integer_get_encoding(
		struct bt_field_type *type)
{
	enum bt_string_encoding ret = BT_STRING_ENCODING_UNKNOWN;
	struct bt_field_type_integer *integer;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		goto end;
	}

	if (type->id != BT_FIELD_TYPE_ID_INTEGER) {
		BT_LOGW("Invalid parameter: field type is not an integer field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		goto end;
	}

	integer = container_of(type, struct bt_field_type_integer, parent);
	ret = integer->encoding;
end:
	return ret;
}

int bt_field_type_integer_set_encoding(struct bt_field_type *type,
		enum bt_string_encoding encoding)
{
	int ret = 0;
	struct bt_field_type_integer *integer;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (type->frozen) {
		BT_LOGW("Invalid parameter: field type is frozen: addr=%p",
			type);
		ret = -1;
		goto end;
	}

	if (type->id != BT_FIELD_TYPE_ID_INTEGER) {
		BT_LOGW("Invalid parameter: field type is not an integer field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		ret = -1;
		goto end;
	}

	if (encoding != BT_STRING_ENCODING_UTF8 &&
			encoding != BT_STRING_ENCODING_ASCII &&
			encoding != BT_STRING_ENCODING_NONE) {
		BT_LOGW("Invalid parameter: unknown string encoding: "
			"addr=%p, encoding=%d", type, encoding);
		ret = -1;
		goto end;
	}

	integer = container_of(type, struct bt_field_type_integer, parent);
	integer->encoding = encoding;
	BT_LOGV("Set integer field type's encoding: addr=%p, encoding=%s",
		type, bt_string_encoding_string(encoding));
end:
	return ret;
}

struct bt_clock_class *bt_field_type_integer_get_mapped_clock_class(
		struct bt_field_type *type)
{
	struct bt_field_type_integer *integer;
	struct bt_clock_class *clock_class = NULL;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		goto end;
	}

	integer = container_of(type, struct bt_field_type_integer, parent);
	clock_class = integer->mapped_clock;
	bt_get(clock_class);
end:
	return clock_class;
}

BT_HIDDEN
int bt_field_type_integer_set_mapped_clock_class_no_check(
		struct bt_field_type *type,
		struct bt_clock_class *clock_class)
{
	struct bt_field_type_integer *integer;
	int ret = 0;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (!clock_class) {
		BT_LOGW_STR("Invalid parameter: clock class is NULL.");
		ret = -1;
		goto end;
	}

	if (type->id != BT_FIELD_TYPE_ID_INTEGER) {
		BT_LOGW("Invalid parameter: field type is not an integer field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		goto end;
	}

	if (!bt_clock_class_is_valid(clock_class)) {
		BT_LOGW("Invalid parameter: clock class is invalid: ft-addr=%p"
			"clock-class-addr=%p, clock-class-name=\"%s\"",
			type, clock_class,
			bt_clock_class_get_name(clock_class));
		ret = -1;
		goto end;
	}

	integer = container_of(type, struct bt_field_type_integer, parent);
	bt_put(integer->mapped_clock);
	integer->mapped_clock = bt_get(clock_class);
	BT_LOGV("Set integer field type's mapped clock class: ft-addr=%p, "
		"clock-class-addr=%p, clock-class-name=\"%s\"",
		type, clock_class, bt_clock_class_get_name(clock_class));
end:
	return ret;
}

int bt_field_type_integer_set_mapped_clock_class(
		struct bt_field_type *type,
		struct bt_clock_class *clock_class)
{
	int ret = 0;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (type->frozen) {
		BT_LOGW("Invalid parameter: field type is frozen: addr=%p",
			type);
		ret = -1;
		goto end;
	}

	ret = bt_field_type_integer_set_mapped_clock_class_no_check(
		type, clock_class);

end:
	return ret;
}

static
void bt_field_type_enum_iter_destroy(struct bt_object *obj)
{
	struct bt_field_type_enumeration_mapping_iterator *iter =
		container_of(obj,
			struct bt_field_type_enumeration_mapping_iterator,
			base);

	BT_LOGD("Destroying enumeration field type mapping iterator: addr=%p",
		obj);
	BT_LOGD_STR("Putting parent enumeration field type.");
	bt_put(&iter->enumeration_type->parent);
	g_free(iter);
}

static
struct bt_field_type_enumeration_mapping_iterator *
bt_field_type_enumeration_find_mappings_type(
		struct bt_field_type *type,
		enum bt_field_type_enumeration_mapping_iterator_type iterator_type)
{
	struct bt_field_type_enumeration *enumeration_type;
	struct bt_field_type_enumeration_mapping_iterator *iter = NULL;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		goto end;
	}

	if (type->id != BT_FIELD_TYPE_ID_ENUM) {
		BT_LOGW("Invalid parameter: field type is not an enumeration field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		goto end;
	}

	enumeration_type = container_of(type,
		struct bt_field_type_enumeration, parent);
	iter = g_new0(struct bt_field_type_enumeration_mapping_iterator, 1);
	if (!iter) {
		BT_LOGE_STR("Failed to allocate one enumeration field type mapping.");
		goto end;
	}

	bt_object_init(&iter->base, bt_field_type_enum_iter_destroy);
	bt_get(type);
	iter->enumeration_type = enumeration_type;
	iter->index = -1;
	iter->type = iterator_type;
end:
	return iter;
}

struct bt_field_type_enumeration_mapping_iterator *
bt_field_type_enumeration_find_mappings_by_name(
		struct bt_field_type *type, const char *name)
{
	struct bt_field_type_enumeration_mapping_iterator *iter;

	iter = bt_field_type_enumeration_find_mappings_type(
			type, ITERATOR_BY_NAME);
	if (!iter) {
		BT_LOGE("Cannot create enumeration field type mapping iterator: "
			"ft-addr=%p, mapping-name=\"%s\"", type, name);
		goto error;
	}

	iter->u.name_quark = g_quark_try_string(name);
	if (!iter->u.name_quark) {
		/*
		 * No results are possible, set the iterator's position at the
		 * end.
		 */
		iter->index = iter->enumeration_type->entries->len;
	}
	return iter;
error:
	bt_put(iter);
	return NULL;
}

int bt_field_type_enumeration_mapping_iterator_next(
		struct bt_field_type_enumeration_mapping_iterator *iter)
{
	struct bt_field_type_enumeration *enumeration;
	struct bt_field_type *type;
	int i, ret = 0, len;

	if (!iter) {
		BT_LOGW_STR("Invalid parameter: enumeration field type mapping iterator is NULL.");
		ret = -1;
		goto end;
	}

	enumeration = iter->enumeration_type;
	type = &enumeration->parent;
	len = enumeration->entries->len;
	for (i = iter->index + 1; i < len; i++) {
		struct enumeration_mapping *mapping =
			get_enumeration_mapping(type, i);

		switch (iter->type) {
		case ITERATOR_BY_NAME:
			if (mapping->string == iter->u.name_quark) {
				iter->index = i;
				goto end;
			}
			break;
		case ITERATOR_BY_SIGNED_VALUE:
		{
			int64_t value = iter->u.signed_value;

			if (value >= mapping->range_start._signed &&
					value <= mapping->range_end._signed) {
				iter->index = i;
				goto end;
			}
			break;
		}
		case ITERATOR_BY_UNSIGNED_VALUE:
		{
			uint64_t value = iter->u.unsigned_value;

			if (value >= mapping->range_start._unsigned &&
					value <= mapping->range_end._unsigned) {
				iter->index = i;
				goto end;
			}
			break;
		}
		default:
			BT_LOGF("Invalid enumeration field type mapping iterator type: "
				"type=%d", iter->type);
			abort();
		}
	}

	ret = -1;
end:
	return ret;
}

struct bt_field_type_enumeration_mapping_iterator *
bt_field_type_enumeration_find_mappings_by_signed_value(
		struct bt_field_type *type, int64_t value)
{
	struct bt_field_type_enumeration_mapping_iterator *iter;

	iter = bt_field_type_enumeration_find_mappings_type(
			type, ITERATOR_BY_SIGNED_VALUE);
	if (!iter) {
		BT_LOGE("Cannot create enumeration field type mapping iterator: "
			"ft-addr=%p, value=%" PRId64, type, value);
		goto error;
	}

	if (bt_ctf_field_type_integer_get_signed(
			iter->enumeration_type->container) != 1) {
		BT_LOGW("Invalid parameter: enumeration field type is unsigned: "
			"enum-ft-addr=%p, int-ft-addr=%p",
			type, iter->enumeration_type->container);
		goto error;
	}

	iter->u.signed_value = value;
	return iter;
error:
	bt_put(iter);
	return NULL;
}

struct bt_field_type_enumeration_mapping_iterator *
bt_field_type_enumeration_find_mappings_by_unsigned_value(
		struct bt_field_type *type, uint64_t value)
{
	struct bt_field_type_enumeration_mapping_iterator *iter;

	iter = bt_field_type_enumeration_find_mappings_type(
			type, ITERATOR_BY_UNSIGNED_VALUE);
	if (!iter) {
		BT_LOGE("Cannot create enumeration field type mapping iterator: "
			"ft-addr=%p, value=%" PRIu64, type, value);
		goto error;
	}

	if (bt_ctf_field_type_integer_get_signed(
			iter->enumeration_type->container) != 0) {
		BT_LOGW("Invalid parameter: enumeration field type is signed: "
			"enum-ft-addr=%p, int-ft-addr=%p",
			type, iter->enumeration_type->container);
		goto error;
	}
	iter->u.unsigned_value = value;
	return iter;
error:
	bt_put(iter);
	return NULL;
}

int bt_field_type_enumeration_mapping_iterator_get_signed(
		struct bt_field_type_enumeration_mapping_iterator *iter,
		const char **mapping_name, int64_t *range_begin,
		int64_t *range_end)
{
	int ret = 0;

	if (!iter) {
		BT_LOGW_STR("Invalid parameter: enumeration field type mapping iterator is NULL.");
		ret = -1;
		goto end;
	}

	if (iter->index == -1) {
		BT_LOGW_STR("Invalid enumeration field type mapping iterator access: position=-1");
		ret = -1;
		goto end;
	}

	ret = bt_field_type_enumeration_get_mapping_signed(
			&iter->enumeration_type->parent, iter->index,
			mapping_name, range_begin, range_end);
end:
	return ret;
}

int bt_field_type_enumeration_mapping_iterator_get_unsigned(
		struct bt_field_type_enumeration_mapping_iterator *iter,
		const char **mapping_name, uint64_t *range_begin,
		uint64_t *range_end)
{
	int ret = 0;

	if (!iter) {
		BT_LOGW_STR("Invalid parameter: enumeration field type mapping iterator is NULL.");
		ret = -1;
		goto end;
	}

	if (iter->index == -1) {
		BT_LOGW_STR("Invalid enumeration field type mapping iterator access: position=-1");
		ret = -1;
		goto end;
	}

	ret = bt_field_type_enumeration_get_mapping_unsigned(
			&iter->enumeration_type->parent, iter->index,
			mapping_name, range_begin, range_end);
end:
	return ret;
}

int bt_field_type_enumeration_get_mapping_signed(
		struct bt_field_type *enum_field_type,
		uint64_t index, const char **mapping_name, int64_t *range_begin,
		int64_t *range_end)
{
	int ret = 0;
	struct enumeration_mapping *mapping;

	if (!enum_field_type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	mapping = get_enumeration_mapping(enum_field_type, index);
	if (!mapping) {
		/* get_enumeration_mapping() reports any error */
		ret = -1;
		goto end;
	}

	if (mapping_name) {
		*mapping_name = g_quark_to_string(mapping->string);
		assert(*mapping_name);
	}

	if (range_begin) {
		*range_begin = mapping->range_start._signed;
	}

	if (range_end) {
		*range_end = mapping->range_end._signed;
	}
end:
	return ret;
}

int bt_field_type_enumeration_get_mapping_unsigned(
		struct bt_field_type *enum_field_type,
		uint64_t index,
		const char **mapping_name, uint64_t *range_begin,
		uint64_t *range_end)
{
	int ret = 0;
	struct enumeration_mapping *mapping;

	if (!enum_field_type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	mapping = get_enumeration_mapping(enum_field_type, index);
	if (!mapping) {
		/* get_enumeration_mapping() reports any error */
		ret = -1;
		goto end;
	}

	if (mapping_name) {
		*mapping_name = g_quark_to_string(mapping->string);
		assert(*mapping_name);
	}

	if (range_begin) {
		*range_begin = mapping->range_start._unsigned;
	}

	if (range_end) {
		*range_end = mapping->range_end._unsigned;
	}
end:
	return ret;
}

struct bt_field_type *bt_field_type_enumeration_create(
		struct bt_field_type *integer_container_type)
{
	struct bt_field_type_enumeration *enumeration = NULL;

	BT_LOGD("Creating enumeration field type object: int-ft-addr=%p",
		integer_container_type);

	if (!integer_container_type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		goto error;
	}

	if (integer_container_type->id != BT_FIELD_TYPE_ID_INTEGER) {
		BT_LOGW("Invalid parameter: container field type is not an integer field type: "
			"container-ft-addr=%p, container-ft-id=%s",
			integer_container_type,
			bt_field_type_id_string(integer_container_type->id));
		goto error;
	}

	enumeration = g_new0(struct bt_field_type_enumeration, 1);
	if (!enumeration) {
		BT_LOGE_STR("Failed to allocate one enumeration field type.");
		goto error;
	}

	enumeration->parent.id = BT_FIELD_TYPE_ID_ENUM;
	bt_get(integer_container_type);
	enumeration->container = integer_container_type;
	enumeration->entries = g_ptr_array_new_with_free_func(
		(GDestroyNotify)destroy_enumeration_mapping);
	bt_field_type_init(&enumeration->parent, FALSE);
	BT_LOGD("Created enumeration field type object: addr=%p, "
		"int-ft-addr=%p, int-ft-size=%u",
		&enumeration->parent, integer_container_type,
		bt_field_type_integer_get_size(integer_container_type));
	return &enumeration->parent;
error:
	g_free(enumeration);
	return NULL;
}

struct bt_field_type *bt_field_type_enumeration_get_container_type(
		struct bt_field_type *type)
{
	struct bt_field_type *container_type = NULL;
	struct bt_field_type_enumeration *enumeration_type;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		goto end;
	}

	if (type->id != BT_FIELD_TYPE_ID_ENUM) {
		BT_LOGW("Invalid parameter: field type is not an enumeration field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		goto end;
	}

	enumeration_type = container_of(type,
		struct bt_field_type_enumeration, parent);
	container_type = enumeration_type->container;
	bt_get(container_type);
end:
	return container_type;
}

int bt_field_type_enumeration_add_mapping_signed(
		struct bt_field_type *type, const char *string,
		int64_t range_start, int64_t range_end)
{
	int ret = 0;
	GQuark mapping_name;
	struct enumeration_mapping *mapping;
	struct bt_field_type_enumeration *enumeration;
	char *escaped_string;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (!string) {
		BT_LOGW_STR("Invalid parameter: string is NULL.");
		ret = -1;
		goto end;
	}

	if (type->frozen) {
		BT_LOGW("Invalid parameter: field type is frozen: addr=%p",
			type);
		ret = -1;
		goto end;
	}

	if (type->id != BT_FIELD_TYPE_ID_ENUM) {
		BT_LOGW("Invalid parameter: field type is not an enumeration field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		ret = -1;
		goto end;
	}

	if (range_end < range_start) {
		BT_LOGW("Invalid parameter: range's end is lesser than range's start: "
			"addr=%p, range-start=%" PRId64 ", range-end=%" PRId64,
			type, range_start, range_end);
		ret = -1;
		goto end;
	}

	if (strlen(string) == 0) {
		BT_LOGW("Invalid parameter: mapping name is an empty string: "
			"enum-ft-addr=%p, mapping-name-addr=%p", type,
			string);
		ret = -1;
		goto end;
	}

	escaped_string = g_strescape(string, NULL);
	if (!escaped_string) {
		BT_LOGE("Cannot escape mapping name: enum-ft-addr=%p, "
			"mapping-name-addr=%p, mapping-name=\"%s\"",
			type, string, string);
		ret = -1;
		goto end;
	}

	mapping = g_new(struct enumeration_mapping, 1);
	if (!mapping) {
		BT_LOGE_STR("Failed to allocate one enumeration mapping.");
		ret = -1;
		goto error_free;
	}
	mapping_name = g_quark_from_string(escaped_string);
	*mapping = (struct enumeration_mapping) {
		.range_start._signed = range_start,
		.range_end._signed = range_end,
		.string =  mapping_name,
	};
	enumeration = container_of(type, struct bt_field_type_enumeration,
		parent);
	g_ptr_array_add(enumeration->entries, mapping);
	g_ptr_array_sort(enumeration->entries,
		(GCompareFunc)compare_enumeration_mappings_signed);
	BT_LOGV("Added mapping to signed enumeration field type: addr=%p, "
		"name=\"%s\", range-start=%" PRId64 ", "
		"range-end=%" PRId64,
		type, string, range_start, range_end);
error_free:
	free(escaped_string);
end:
	return ret;
}

int bt_field_type_enumeration_add_mapping_unsigned(
		struct bt_field_type *type, const char *string,
		uint64_t range_start, uint64_t range_end)
{
	int ret = 0;
	GQuark mapping_name;
	struct enumeration_mapping *mapping;
	struct bt_field_type_enumeration *enumeration;
	char *escaped_string;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (!string) {
		BT_LOGW_STR("Invalid parameter: string is NULL.");
		ret = -1;
		goto end;
	}

	if (type->frozen) {
		BT_LOGW("Invalid parameter: field type is frozen: addr=%p",
			type);
		ret = -1;
		goto end;
	}

	if (type->id != BT_FIELD_TYPE_ID_ENUM) {
		BT_LOGW("Invalid parameter: field type is not an enumeration field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		ret = -1;
		goto end;
	}

	if (range_end < range_start) {
		BT_LOGW("Invalid parameter: range's end is lesser than range's start: "
			"addr=%p, range-start=%" PRIu64 ", range-end=%" PRIu64,
			type, range_start, range_end);
		ret = -1;
		goto end;
	}

	if (strlen(string) == 0) {
		BT_LOGW("Invalid parameter: mapping name is an empty string: "
			"enum-ft-addr=%p, mapping-name-addr=%p", type,
			string);
		ret = -1;
		goto end;
	}

	escaped_string = g_strescape(string, NULL);
	if (!escaped_string) {
		BT_LOGE("Cannot escape mapping name: enum-ft-addr=%p, "
			"mapping-name-addr=%p, mapping-name=\"%s\"",
			type, string, string);
		ret = -1;
		goto end;
	}

	mapping = g_new(struct enumeration_mapping, 1);
	if (!mapping) {
		BT_LOGE_STR("Failed to allocate one enumeration mapping.");
		ret = -1;
		goto error_free;
	}
	mapping_name = g_quark_from_string(escaped_string);
	*mapping = (struct enumeration_mapping) {
		.range_start._unsigned = range_start,
		.range_end._unsigned = range_end,
		.string = mapping_name,
	};
	enumeration = container_of(type, struct bt_field_type_enumeration,
		parent);
	g_ptr_array_add(enumeration->entries, mapping);
	g_ptr_array_sort(enumeration->entries,
		(GCompareFunc)compare_enumeration_mappings_unsigned);
	BT_LOGV("Added mapping to unsigned enumeration field type: addr=%p, "
		"name=\"%s\", range-start=%" PRIu64 ", "
		"range-end=%" PRIu64,
		type, string, range_start, range_end);
error_free:
	free(escaped_string);
end:
	return ret;
}

int64_t bt_field_type_enumeration_get_mapping_count(
		struct bt_field_type *type)
{
	int64_t ret = 0;
	struct bt_field_type_enumeration *enumeration;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = (int64_t) -1;
		goto end;
	}

	if (type->id != BT_FIELD_TYPE_ID_ENUM) {
		BT_LOGW("Invalid parameter: field type is not an enumeration field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		ret = (int64_t) -1;
		goto end;
	}

	enumeration = container_of(type, struct bt_field_type_enumeration,
		parent);
	ret = (int64_t) enumeration->entries->len;
end:
	return ret;
}

struct bt_field_type *bt_field_type_floating_point_create(void)
{
	struct bt_field_type_floating_point *floating_point =
		g_new0(struct bt_field_type_floating_point, 1);

	BT_LOGD_STR("Creating floating point number field type object.");

	if (!floating_point) {
		BT_LOGE_STR("Failed to allocate one floating point number field type.");
		goto end;
	}

	floating_point->parent.id = BT_FIELD_TYPE_ID_FLOAT;
	floating_point->exp_dig = sizeof(float) * CHAR_BIT - FLT_MANT_DIG;
	floating_point->mant_dig = FLT_MANT_DIG;
	bt_field_type_init(&floating_point->parent, TRUE);
	BT_LOGD("Created floating point number field type object: addr=%p, "
		"exp-size=%u, mant-size=%u", &floating_point->parent,
		floating_point->exp_dig, floating_point->mant_dig);
end:
	return floating_point ? &floating_point->parent : NULL;
}

int bt_field_type_floating_point_get_exponent_digits(
		struct bt_field_type *type)
{
	int ret = 0;
	struct bt_field_type_floating_point *floating_point;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (type->id != BT_FIELD_TYPE_ID_FLOAT) {
		BT_LOGW("Invalid parameter: field type is not a floating point number field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		ret = -1;
		goto end;
	}

	floating_point = container_of(type,
		struct bt_field_type_floating_point, parent);
	ret = (int) floating_point->exp_dig;
end:
	return ret;
}

int bt_field_type_floating_point_set_exponent_digits(
		struct bt_field_type *type,
		unsigned int exponent_digits)
{
	int ret = 0;
	struct bt_field_type_floating_point *floating_point;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (type->frozen) {
		BT_LOGW("Invalid parameter: field type is frozen: addr=%p",
			type);
		ret = -1;
		goto end;
	}

	if (type->id != BT_FIELD_TYPE_ID_FLOAT) {
		BT_LOGW("Invalid parameter: field type is not a floating point number field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		ret = -1;
		goto end;
	}

	floating_point = container_of(type,
		struct bt_field_type_floating_point, parent);
	if ((exponent_digits != sizeof(float) * CHAR_BIT - FLT_MANT_DIG) &&
		(exponent_digits != sizeof(double) * CHAR_BIT - DBL_MANT_DIG) &&
		(exponent_digits !=
			sizeof(long double) * CHAR_BIT - LDBL_MANT_DIG)) {
		BT_LOGW("Invalid parameter: invalid exponent size: "
			"addr=%p, exp-size=%u", type, exponent_digits);
		ret = -1;
		goto end;
	}

	floating_point->exp_dig = exponent_digits;
	BT_LOGV("Set floating point number field type's exponent size: addr=%p, "
		"exp-size=%u", type, exponent_digits);
end:
	return ret;
}

int bt_field_type_floating_point_get_mantissa_digits(
		struct bt_field_type *type)
{
	int ret = 0;
	struct bt_field_type_floating_point *floating_point;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (type->id != BT_FIELD_TYPE_ID_FLOAT) {
		BT_LOGW("Invalid parameter: field type is not a floating point number field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		ret = -1;
		goto end;
	}

	floating_point = container_of(type,
		struct bt_field_type_floating_point, parent);
	ret = (int) floating_point->mant_dig;
end:
	return ret;
}

int bt_field_type_floating_point_set_mantissa_digits(
		struct bt_field_type *type,
		unsigned int mantissa_digits)
{
	int ret = 0;
	struct bt_field_type_floating_point *floating_point;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (type->frozen) {
		BT_LOGW("Invalid parameter: field type is frozen: addr=%p",
			type);
		ret = -1;
		goto end;
	}

	if (type->id != BT_FIELD_TYPE_ID_FLOAT) {
		BT_LOGW("Invalid parameter: field type is not a floating point number field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		ret = -1;
		goto end;
	}

	floating_point = container_of(type,
		struct bt_field_type_floating_point, parent);

	if ((mantissa_digits != FLT_MANT_DIG) &&
		(mantissa_digits != DBL_MANT_DIG) &&
		(mantissa_digits != LDBL_MANT_DIG)) {
		BT_LOGW("Invalid parameter: invalid mantissa size: "
			"addr=%p, mant-size=%u", type, mantissa_digits);
		ret = -1;
		goto end;
	}

	floating_point->mant_dig = mantissa_digits;
	BT_LOGV("Set floating point number field type's mantissa size: addr=%p, "
		"mant-size=%u", type, mantissa_digits);
end:
	return ret;
}

struct bt_field_type *bt_field_type_structure_create(void)
{
	struct bt_field_type_structure *structure =
		g_new0(struct bt_field_type_structure, 1);

	BT_LOGD_STR("Creating structure field type object.");

	if (!structure) {
		BT_LOGE_STR("Failed to allocate one structure field type.");
		goto error;
	}

	structure->parent.id = BT_FIELD_TYPE_ID_STRUCT;
	structure->fields = g_ptr_array_new_with_free_func(
		(GDestroyNotify)destroy_structure_field);
	structure->field_name_to_index = g_hash_table_new(NULL, NULL);
	bt_field_type_init(&structure->parent, TRUE);
	BT_LOGD("Created structure field type object: addr=%p",
		&structure->parent);
	return &structure->parent;
error:
	return NULL;
}

int bt_field_type_structure_add_field(struct bt_field_type *type,
		struct bt_field_type *field_type,
		const char *field_name)
{
	int ret = 0;
	struct bt_field_type_structure *structure;

	/*
	 * TODO: check that `field_type` does not contain `type`,
	 *       recursively.
	 */
	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (!field_name) {
		BT_LOGW_STR("Invalid parameter: field name is NULL.");
		ret = -1;
		goto end;
	}

	if (type->frozen) {
		BT_LOGW("Invalid parameter: field type is frozen: addr=%p",
			type);
		ret = -1;
		goto end;
	}

	if (type->id != BT_FIELD_TYPE_ID_STRUCT) {
		BT_LOGW("Invalid parameter: field type is not a structure field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		ret = -1;
		goto end;
	}

	if (type == field_type) {
		BT_LOGW("Invalid parameter: structure field type and field type to add are the same: "
			"addr=%p", type);
		ret = -1;
		goto end;
	}

	structure = container_of(type,
		struct bt_field_type_structure, parent);
	if (add_structure_field(structure->fields,
			structure->field_name_to_index, field_type, field_name)) {
		BT_LOGW("Cannot add field to structure field type: "
			"struct-ft-addr=%p, field-ft-addr=%p, field-name=\"%s\"",
			type, field_type, field_name);
		ret = -1;
		goto end;
	}

	BT_LOGV("Added structure field type field: struct-ft-addr=%p, "
		"field-ft-addr=%p, field-name=\"%s\"", type,
		field_type, field_name);
end:
	return ret;
}

int64_t bt_field_type_structure_get_field_count(
		struct bt_field_type *type)
{
	int64_t ret = 0;
	struct bt_field_type_structure *structure;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = (int64_t) -1;
		goto end;
	}

	if (type->id != BT_FIELD_TYPE_ID_STRUCT) {
		BT_LOGW("Invalid parameter: field type is not a structure field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		ret = (int64_t) -1;
		goto end;
	}

	structure = container_of(type, struct bt_field_type_structure,
		parent);
	ret = (int64_t) structure->fields->len;
end:
	return ret;
}

int bt_field_type_structure_get_field_by_index(
		struct bt_field_type *type,
		const char **field_name, struct bt_field_type **field_type,
		uint64_t index)
{
	struct bt_field_type_structure *structure;
	struct structure_field *field;
	int ret = 0;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (type->id != BT_FIELD_TYPE_ID_STRUCT) {
		BT_LOGW("Invalid parameter: field type is not a structure field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		ret = -1;
		goto end;
	}

	structure = container_of(type, struct bt_field_type_structure,
		parent);
	if (index >= structure->fields->len) {
		BT_LOGW("Invalid parameter: index is out of bounds: "
			"addr=%p, index=%" PRIu64 ", count=%u",
			type, index, structure->fields->len);
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
		assert(*field_name);
	}
end:
	return ret;
}

struct bt_field_type *bt_field_type_structure_get_field_type_by_name(
		struct bt_field_type *type,
		const char *name)
{
	size_t index;
	GQuark name_quark;
	struct structure_field *field;
	struct bt_field_type_structure *structure;
	struct bt_field_type *field_type = NULL;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		goto end;
	}

	if (!name) {
		BT_LOGW_STR("Invalid parameter: name is NULL.");
		goto end;
	}

	name_quark = g_quark_try_string(name);
	if (!name_quark) {
		BT_LOGV("No such structure field type field name: "
			"ft-addr=%p, field-name=\"%s\"",
			type, name);
		goto end;
	}

	structure = container_of(type, struct bt_field_type_structure,
		parent);
	if (!g_hash_table_lookup_extended(structure->field_name_to_index,
			GUINT_TO_POINTER(name_quark), NULL, (gpointer *)&index)) {
		BT_LOGV("No such structure field type field name: "
			"ft-addr=%p, field-name=\"%s\"",
			type, name);
		goto end;
	}

	field = structure->fields->pdata[index];
	field_type = field->type;
	bt_get(field_type);
end:
	return field_type;
}

struct bt_field_type *bt_field_type_variant_create(
	struct bt_field_type *enum_tag, const char *tag_name)
{
	struct bt_field_type_variant *variant = NULL;

	BT_LOGD("Creating variant field type object: "
		"tag-ft-addr=%p, tag-field-name=\"%s\"",
		enum_tag, tag_name);

	if (tag_name && bt_identifier_is_valid(tag_name)) {
		BT_LOGW("Invalid parameter: tag field name is not a valid CTF identifier: "
			"tag-ft-addr=%p, tag-field-name=\"%s\"",
			enum_tag, tag_name);
		goto error;
	}

	variant = g_new0(struct bt_field_type_variant, 1);
	if (!variant) {
		BT_LOGE_STR("Failed to allocate one variant field type.");
		goto error;
	}

	variant->parent.id = BT_FIELD_TYPE_ID_VARIANT;
	variant->tag_name = g_string_new(tag_name);
	variant->field_name_to_index = g_hash_table_new(NULL, NULL);
	variant->fields = g_ptr_array_new_with_free_func(
		(GDestroyNotify) destroy_structure_field);
	if (enum_tag) {
		bt_get(enum_tag);
		variant->tag = container_of(enum_tag,
			struct bt_field_type_enumeration, parent);
	}

	bt_field_type_init(&variant->parent, TRUE);
	/* A variant's alignment is undefined */
	variant->parent.alignment = 0;
	BT_LOGD("Created variant field type object: addr=%p, "
		"tag-ft-addr=%p, tag-field-name=\"%s\"",
		&variant->parent, enum_tag, tag_name);
	return &variant->parent;
error:
	return NULL;
}

struct bt_field_type *bt_field_type_variant_get_tag_type(
		struct bt_field_type *type)
{
	struct bt_field_type_variant *variant;
	struct bt_field_type *tag_type = NULL;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		goto end;
	}

	if (type->id != BT_FIELD_TYPE_ID_VARIANT) {
		BT_LOGW("Invalid parameter: field type is not a variant field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		goto end;
	}

	variant = container_of(type, struct bt_field_type_variant, parent);
	if (!variant->tag) {
		BT_LOGV("Variant field type has no tag field type: "
			"addr=%p", type);
		goto end;
	}

	tag_type = &variant->tag->parent;
	bt_get(tag_type);
end:
	return tag_type;
}

const char *bt_field_type_variant_get_tag_name(
		struct bt_field_type *type)
{
	struct bt_field_type_variant *variant;
	const char *tag_name = NULL;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		goto end;
	}

	if (type->id != BT_FIELD_TYPE_ID_VARIANT) {
		BT_LOGW("Invalid parameter: field type is not a variant field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		goto end;
	}

	variant = container_of(type, struct bt_field_type_variant, parent);
	if (variant->tag_name->len == 0) {
		BT_LOGV("Variant field type has no tag field name: "
			"addr=%p", type);
		goto end;
	}

	tag_name = variant->tag_name->str;
end:
	return tag_name;
}

int bt_field_type_variant_set_tag_name(
		struct bt_field_type *type, const char *name)
{
	int ret = 0;
	struct bt_field_type_variant *variant;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (type->frozen) {
		BT_LOGW("Invalid parameter: field type is frozen: addr=%p",
			type);
		ret = -1;
		goto end;
	}

	if (type->id != BT_FIELD_TYPE_ID_VARIANT) {
		BT_LOGW("Invalid parameter: field type is not a variant field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		ret = -1;
		goto end;
	}

	if (bt_identifier_is_valid(name)) {
		BT_LOGW("Invalid parameter: tag field name is not a valid CTF identifier: "
			"variant-ft-addr=%p, tag-field-name=\"%s\"",
			type, name);
		ret = -1;
		goto end;
	}

	variant = container_of(type, struct bt_field_type_variant, parent);
	g_string_assign(variant->tag_name, name);
	BT_LOGV("Set variant field type's tag field name: addr=%p, "
		"tag-field-name=\"%s\"", type, name);
end:
	return ret;
}

int bt_field_type_variant_add_field(struct bt_field_type *type,
		struct bt_field_type *field_type,
		const char *field_name)
{
	size_t i;
	int ret = 0;
	struct bt_field_type_variant *variant;
	GQuark field_name_quark = g_quark_from_string(field_name);

	/*
	 * TODO: check that `field_type` does not contain `type`,
	 *       recursively.
	 */
	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (type->frozen) {
		BT_LOGW("Invalid parameter: field type is frozen: addr=%p",
			type);
		ret = -1;
		goto end;
	}

	if (type->id != BT_FIELD_TYPE_ID_VARIANT) {
		BT_LOGW("Invalid parameter: field type is not a variant field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		ret = -1;
		goto end;
	}

	if (type == field_type) {
		BT_LOGW("Invalid parameter: variant field type and field type to add are the same: "
			"addr=%p", type);
		ret = -1;
		goto end;
	}

	variant = container_of(type, struct bt_field_type_variant, parent);

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
			BT_LOGW("Invalid parameter: field name does not name a tag field type's mapping: "
				"variant-ft-addr=%p, tag-ft-addr=%p, "
				"tag-field-name=\"%s\""
				"field-ft-addr=%p, field-name=\"%s\"",
				type, variant->tag, variant->tag_name->str,
				field_type, field_name);
			ret = -1;
			goto end;
		}
	}

	if (add_structure_field(variant->fields, variant->field_name_to_index,
			field_type, field_name)) {
		BT_LOGW("Cannot add field to variant field type: "
			"variant-ft-addr=%p, field-ft-addr=%p, field-name=\"%s\"",
			type, field_type, field_name);
		ret = -1;
		goto end;
	}

	BT_LOGV("Added variant field type field: variant-ft-addr=%p, "
		"field-ft-addr=%p, field-name=\"%s\"", type,
		field_type, field_name);

end:
	return ret;
}

struct bt_field_type *bt_field_type_variant_get_field_type_by_name(
		struct bt_field_type *type,
		const char *field_name)
{
	size_t index;
	GQuark name_quark;
	struct structure_field *field;
	struct bt_field_type_variant *variant;
	struct bt_field_type *field_type = NULL;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		goto end;
	}

	if (!field_name) {
		BT_LOGW_STR("Invalid parameter: field name is NULL.");
		goto end;
	}

	name_quark = g_quark_try_string(field_name);
	if (!name_quark) {
		BT_LOGV("No such variant field type field name: "
			"ft-addr=%p, field-name=\"%s\"",
			type, field_name);
		goto end;
	}

	variant = container_of(type, struct bt_field_type_variant, parent);
	if (!g_hash_table_lookup_extended(variant->field_name_to_index,
			GUINT_TO_POINTER(name_quark), NULL, (gpointer *)&index)) {
		BT_LOGV("No such variant field type field name: "
			"ft-addr=%p, field-name=\"%s\"",
			type, field_name);
		goto end;
	}

	field = g_ptr_array_index(variant->fields, index);
	field_type = field->type;
	bt_get(field_type);
end:
	return field_type;
}

struct bt_field_type *bt_field_type_variant_get_field_type_from_tag(
		struct bt_field_type *type,
		struct bt_field *tag)
{
	int ret;
	const char *enum_value;
	struct bt_field_type *field_type = NULL;
	struct bt_field_type_enumeration_mapping_iterator *iter = NULL;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: variant field type is NULL.");
		goto end;
	}

	if (!tag) {
		BT_LOGW_STR("Invalid parameter: tag field is NULL.");
		goto end;
	}

	if (type->id != BT_FIELD_TYPE_ID_VARIANT) {
		BT_LOGW("Invalid parameter: field type is not a variant field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		goto end;
	}

	iter = bt_field_enumeration_get_mappings(tag);
	ret = bt_field_type_enumeration_mapping_iterator_next(iter);
	if (!iter || ret) {
		BT_LOGE("Cannot get enumeration field type mapping iterator from enumeration field: "
			"enum-field-addr=%p", tag);
		goto end;
	}

	ret = bt_field_type_enumeration_mapping_iterator_get_signed(iter,
		&enum_value, NULL, NULL);
	if (ret) {
		BT_LOGW("Cannot get enumeration field type mapping iterator's current mapping: "
			"iter-addr=%p", iter);
		goto end;
	}

	field_type = bt_field_type_variant_get_field_type_by_name(
		type, enum_value);
end:
	bt_put(iter);
	return field_type;
}

int64_t bt_field_type_variant_get_field_count(struct bt_field_type *type)
{
	int64_t ret = 0;
	struct bt_field_type_variant *variant;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = (int64_t) -1;
		goto end;
	}

	if (type->id != BT_FIELD_TYPE_ID_VARIANT) {
		BT_LOGW("Invalid parameter: field type is not a variant field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		ret = (int64_t) -1;
		goto end;
	}

	variant = container_of(type, struct bt_field_type_variant,
		parent);
	ret = (int64_t) variant->fields->len;
end:
	return ret;

}

int bt_field_type_variant_get_field_by_index(struct bt_field_type *type,
		const char **field_name, struct bt_field_type **field_type,
		uint64_t index)
{
	struct bt_field_type_variant *variant;
	struct structure_field *field;
	int ret = 0;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (type->id != BT_FIELD_TYPE_ID_VARIANT) {
		BT_LOGW("Invalid parameter: field type is not a variant field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		ret = -1;
		goto end;
	}

	variant = container_of(type, struct bt_field_type_variant,
		parent);
	if (index >= variant->fields->len) {
		BT_LOGW("Invalid parameter: index is out of bounds: "
			"addr=%p, index=%" PRIu64 ", count=%u",
			type, index, variant->fields->len);
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
		assert(*field_name);
	}
end:
	return ret;
}

struct bt_field_type *bt_field_type_array_create(
		struct bt_field_type *element_type,
		unsigned int length)
{
	struct bt_field_type_array *array = NULL;

	BT_LOGD("Creating array field type object: element-ft-addr=%p, "
		"length=%u", element_type, length);

	if (!element_type) {
		BT_LOGW_STR("Invalid parameter: element field type is NULL.");
		goto error;
	}

	if (length == 0) {
		BT_LOGW_STR("Invalid parameter: length is zero.");
		goto error;
	}

	array = g_new0(struct bt_field_type_array, 1);
	if (!array) {
		BT_LOGE_STR("Failed to allocate one array field type.");
		goto error;
	}

	array->parent.id = BT_FIELD_TYPE_ID_ARRAY;
	bt_get(element_type);
	array->element_type = element_type;
	array->length = length;
	bt_field_type_init(&array->parent, FALSE);
	BT_LOGD("Created array field type object: addr=%p, "
		"element-ft-addr=%p, length=%u",
		&array->parent, element_type, length);
	return &array->parent;
error:
	return NULL;
}

struct bt_field_type *bt_field_type_array_get_element_type(
		struct bt_field_type *type)
{
	struct bt_field_type *ret = NULL;
	struct bt_field_type_array *array;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		goto end;
	}

	if (type->id != BT_FIELD_TYPE_ID_ARRAY) {
		BT_LOGW("Invalid parameter: field type is not an array field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		goto end;
	}

	array = container_of(type, struct bt_field_type_array, parent);
	ret = array->element_type;
	bt_get(ret);
end:
	return ret;
}

BT_HIDDEN
int bt_field_type_array_set_element_type(struct bt_field_type *type,
		struct bt_field_type *element_type)
{
	int ret = 0;
	struct bt_field_type_array *array;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: array field type is NULL.");
		ret = -1;
		goto end;
	}

	if (!element_type) {
		BT_LOGW_STR("Invalid parameter: element field type is NULL.");
		ret = -1;
		goto end;
	}

	if (type->id != BT_FIELD_TYPE_ID_ARRAY) {
		BT_LOGW("Invalid parameter: field type is not an array field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		ret = -1;
		goto end;
	}

	array = container_of(type, struct bt_field_type_array, parent);

	if (array->element_type) {
		BT_PUT(array->element_type);
	}

	array->element_type = element_type;
	bt_get(array->element_type);
	BT_LOGV("Set array field type's element field type: array-ft-addr=%p, "
		"element-ft-addr=%p", type, element_type);

end:
	return ret;
}

int64_t bt_field_type_array_get_length(struct bt_field_type *type)
{
	int64_t ret;
	struct bt_field_type_array *array;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = (int64_t) -1;
		goto end;
	}

	if (type->id != BT_FIELD_TYPE_ID_ARRAY) {
		BT_LOGW("Invalid parameter: field type is not an array field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		ret = (int64_t) -1;
		goto end;
	}

	array = container_of(type, struct bt_field_type_array, parent);
	ret = (int64_t) array->length;
end:
	return ret;
}

struct bt_field_type *bt_field_type_sequence_create(
		struct bt_field_type *element_type,
		const char *length_field_name)
{
	struct bt_field_type_sequence *sequence = NULL;

	BT_LOGD("Creating sequence field type object: element-ft-addr=%p, "
		"length-field-name=\"%s\"", element_type, length_field_name);

	if (!element_type) {
		BT_LOGW_STR("Invalid parameter: element field type is NULL.");
		goto error;
	}

	if (bt_identifier_is_valid(length_field_name)) {
		BT_LOGW("Invalid parameter: length field name is not a valid CTF identifier: "
			"length-field-name=\"%s\"", length_field_name);
		goto error;
	}

	sequence = g_new0(struct bt_field_type_sequence, 1);
	if (!sequence) {
		BT_LOGE_STR("Failed to allocate one sequence field type.");
		goto error;
	}

	sequence->parent.id = BT_FIELD_TYPE_ID_SEQUENCE;
	bt_get(element_type);
	sequence->element_type = element_type;
	sequence->length_field_name = g_string_new(length_field_name);
	bt_field_type_init(&sequence->parent, FALSE);
	BT_LOGD("Created sequence field type object: addr=%p, "
		"element-ft-addr=%p, length-field-name=\"%s\"",
		&sequence->parent, element_type, length_field_name);
	return &sequence->parent;
error:
	return NULL;
}

struct bt_field_type *bt_field_type_sequence_get_element_type(
		struct bt_field_type *type)
{
	struct bt_field_type *ret = NULL;
	struct bt_field_type_sequence *sequence;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		goto end;
	}

	if (type->id != BT_FIELD_TYPE_ID_SEQUENCE) {
		BT_LOGW("Invalid parameter: field type is not a sequence field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		goto end;
	}

	sequence = container_of(type, struct bt_field_type_sequence,
		parent);
	ret = sequence->element_type;
	bt_get(ret);
end:
	return ret;
}

BT_HIDDEN
int bt_field_type_sequence_set_element_type(struct bt_field_type *type,
		struct bt_field_type *element_type)
{
	int ret = 0;
	struct bt_field_type_sequence *sequence;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: sequence field type is NULL.");
		ret = -1;
		goto end;
	}

	if (!element_type) {
		BT_LOGW_STR("Invalid parameter: element field type is NULL.");
		ret = -1;
		goto end;
	}

	if (type->id != BT_FIELD_TYPE_ID_SEQUENCE) {
		BT_LOGW("Invalid parameter: field type is not a sequence field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		ret = -1;
		goto end;
	}

	sequence = container_of(type, struct bt_field_type_sequence, parent);
	if (sequence->element_type) {
		BT_PUT(sequence->element_type);
	}

	sequence->element_type = element_type;
	bt_get(sequence->element_type);
	BT_LOGV("Set sequence field type's element field type: sequence-ft-addr=%p, "
		"element-ft-addr=%p", type, element_type);

end:
	return ret;
}

const char *bt_field_type_sequence_get_length_field_name(
		struct bt_field_type *type)
{
	const char *ret = NULL;
	struct bt_field_type_sequence *sequence;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		goto end;
	}

	if (type->id != BT_FIELD_TYPE_ID_SEQUENCE) {
		BT_LOGW("Invalid parameter: field type is not a sequence field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		goto end;
	}

	sequence = container_of(type, struct bt_field_type_sequence,
		parent);
	ret = sequence->length_field_name->str;
end:
	return ret;
}

struct bt_field_type *bt_field_type_string_create(void)
{
	struct bt_field_type_string *string =
		g_new0(struct bt_field_type_string, 1);

	BT_LOGD_STR("Creating string field type object.");

	if (!string) {
		BT_LOGE_STR("Failed to allocate one string field type.");
		return NULL;
	}

	string->parent.id = BT_FIELD_TYPE_ID_STRING;
	bt_field_type_init(&string->parent, TRUE);
	string->encoding = BT_STRING_ENCODING_UTF8;
	string->parent.alignment = CHAR_BIT;
	BT_LOGD("Created string field type object: addr=%p", &string->parent);
	return &string->parent;
}

enum bt_string_encoding bt_field_type_string_get_encoding(
		struct bt_field_type *type)
{
	struct bt_field_type_string *string;
	enum bt_string_encoding ret = BT_STRING_ENCODING_UNKNOWN;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		goto end;
	}

	if (type->id != BT_FIELD_TYPE_ID_STRING) {
		BT_LOGW("Invalid parameter: field type is not a string field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		goto end;
	}

	string = container_of(type, struct bt_field_type_string,
		parent);
	ret = string->encoding;
end:
	return ret;
}

int bt_field_type_string_set_encoding(struct bt_field_type *type,
		enum bt_string_encoding encoding)
{
	int ret = 0;
	struct bt_field_type_string *string;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (type->id != BT_FIELD_TYPE_ID_STRING) {
		BT_LOGW("Invalid parameter: field type is not a string field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		ret = -1;
		goto end;
	}

	if (encoding != BT_STRING_ENCODING_UTF8 &&
			encoding != BT_STRING_ENCODING_ASCII) {
		BT_LOGW("Invalid parameter: unknown string encoding: "
			"addr=%p, encoding=%d", type, encoding);
		ret = -1;
		goto end;
	}

	string = container_of(type, struct bt_field_type_string, parent);
	string->encoding = encoding;
	BT_LOGV("Set string field type's encoding: addr=%p, encoding=%s",
		type, bt_string_encoding_string(encoding));
end:
	return ret;
}

int bt_field_type_get_alignment(struct bt_field_type *type)
{
	int ret;
	enum bt_field_type_id type_id;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (type->frozen) {
		ret = (int) type->alignment;
		goto end;
	}

	type_id = bt_field_type_get_type_id(type);
	switch (type_id) {
	case BT_FIELD_TYPE_ID_SEQUENCE:
	{
		struct bt_field_type *element =
			bt_field_type_sequence_get_element_type(type);

		assert(element);
		ret = bt_field_type_get_alignment(element);
		bt_put(element);
		break;
	}
	case BT_FIELD_TYPE_ID_ARRAY:
	{
		struct bt_field_type *element =
			bt_field_type_array_get_element_type(type);

		assert(element);
		ret = bt_field_type_get_alignment(element);
		bt_put(element);
		break;
	}
	case BT_FIELD_TYPE_ID_STRUCT:
	{
		int64_t i, element_count;

		element_count = bt_field_type_structure_get_field_count(
			type);
		assert(element_count >= 0);

		for (i = 0; i < element_count; i++) {
			struct bt_field_type *field;
			int field_alignment;

			ret = bt_field_type_structure_get_field_by_index(
				type, NULL, &field, i);
			assert(ret == 0);
			assert(field);
			field_alignment = bt_field_type_get_alignment(
				field);
			bt_put(field);
			if (field_alignment < 0) {
				ret = field_alignment;
				goto end;
			}

			type->alignment = MAX(field_alignment, type->alignment);
		}
		ret = (int) type->alignment;
		break;
	}
	case BT_FIELD_TYPE_ID_UNKNOWN:
		BT_LOGW("Invalid parameter: unknown field type ID: "
			"addr=%p, ft-id=%d", type, type_id);
		ret = -1;
		break;
	default:
		ret = (int) type->alignment;
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

int bt_field_type_set_alignment(struct bt_field_type *type,
		unsigned int alignment)
{
	int ret = 0;
	enum bt_field_type_id type_id;

	/* Alignment must be a power of two */
	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (type->frozen) {
		BT_LOGW("Invalid parameter: field type is frozen: addr=%p",
			type);
		ret = -1;
		goto end;
	}

	if (!is_power_of_two(alignment)) {
		BT_LOGW("Invalid parameter: alignment is not a power of two: "
			"addr=%p, align=%u", type, alignment);
		ret = -1;
		goto end;
	}

	type_id = bt_field_type_get_type_id(type);
	if (type_id == BT_FIELD_TYPE_ID_UNKNOWN) {
		BT_LOGW("Invalid parameter: unknown field type ID: "
			"addr=%p, ft-id=%d", type, type_id);
		ret = -1;
		goto end;
	}

	if (type->id == BT_FIELD_TYPE_ID_STRING &&
			alignment != CHAR_BIT) {
		BT_LOGW("Invalid parameter: alignment must be %u for a string field type: "
			"addr=%p, align=%u", CHAR_BIT, type, alignment);
		ret = -1;
		goto end;
	}

	if (type_id == BT_FIELD_TYPE_ID_VARIANT ||
			type_id == BT_FIELD_TYPE_ID_SEQUENCE ||
			type_id == BT_FIELD_TYPE_ID_ARRAY) {
		/* Setting an alignment on these types makes no sense */
		BT_LOGW("Invalid parameter: cannot set the alignment of this field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		ret = -1;
		goto end;
	}

	type->alignment = alignment;
	ret = 0;
	BT_LOGV("Set field type's alignment: addr=%p, align=%u",
		type, alignment);
end:
	return ret;
}

enum bt_byte_order bt_field_type_get_byte_order(
		struct bt_field_type *type)
{
	enum bt_byte_order ret = BT_BYTE_ORDER_UNKNOWN;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		goto end;
	}

	switch (type->id) {
	case BT_FIELD_TYPE_ID_INTEGER:
	{
		struct bt_field_type_integer *integer = container_of(
			type, struct bt_field_type_integer, parent);
		ret = integer->user_byte_order;
		break;
	}
	case BT_FIELD_TYPE_ID_ENUM:
	{
		struct bt_field_type_enumeration *enum_ft = container_of(
			type, struct bt_field_type_enumeration, parent);
		ret = bt_field_type_get_byte_order(enum_ft->container);
		break;
	}
	case BT_FIELD_TYPE_ID_FLOAT:
	{
		struct bt_field_type_floating_point *floating_point =
			container_of(type,
				struct bt_field_type_floating_point,
				parent);
		ret = floating_point->user_byte_order;
		break;
	}
	default:
		BT_LOGW("Invalid parameter: cannot get the byte order of this field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		goto end;
	}

	assert(ret == BT_BYTE_ORDER_NATIVE ||
		ret == BT_BYTE_ORDER_LITTLE_ENDIAN ||
		ret == BT_BYTE_ORDER_BIG_ENDIAN ||
		ret == BT_BYTE_ORDER_NETWORK);

end:
	return ret;
}

int bt_field_type_set_byte_order(struct bt_field_type *type,
		enum bt_byte_order byte_order)
{
	int ret = 0;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (type->frozen) {
		BT_LOGW("Invalid parameter: field type is frozen: addr=%p",
			type);
		ret = -1;
		goto end;
	}

	if (byte_order != BT_BYTE_ORDER_NATIVE &&
			byte_order != BT_BYTE_ORDER_LITTLE_ENDIAN &&
			byte_order != BT_BYTE_ORDER_BIG_ENDIAN &&
			byte_order != BT_BYTE_ORDER_NETWORK) {
		BT_LOGW("Invalid parameter: invalid byte order: "
			"addr=%p, bo=%s", type,
			bt_byte_order_string(byte_order));
		ret = -1;
		goto end;
	}

	if (set_byte_order_funcs[type->id]) {
		set_byte_order_funcs[type->id](type, byte_order);
	}

	BT_LOGV("Set field type's byte order: addr=%p, bo=%s",
		type, bt_byte_order_string(byte_order));

end:
	return ret;
}

enum bt_field_type_id bt_field_type_get_type_id(
		struct bt_field_type *type)
{
	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		return BT_FIELD_TYPE_ID_UNKNOWN;
	}

	return type->id;
}

int bt_field_type_is_integer(struct bt_field_type *type)
{
	return bt_field_type_get_type_id(type) == BT_FIELD_TYPE_ID_INTEGER;
}

int bt_field_type_is_floating_point(struct bt_field_type *type)
{
	return bt_field_type_get_type_id(type) == BT_FIELD_TYPE_ID_FLOAT;
}

int bt_field_type_is_enumeration(struct bt_field_type *type)
{
	return bt_field_type_get_type_id(type) == BT_FIELD_TYPE_ID_ENUM;
}

int bt_field_type_is_string(struct bt_field_type *type)
{
	return bt_field_type_get_type_id(type) == BT_FIELD_TYPE_ID_STRING;
}

int bt_field_type_is_structure(struct bt_field_type *type)
{
	return bt_field_type_get_type_id(type) == BT_FIELD_TYPE_ID_STRUCT;
}

int bt_field_type_is_array(struct bt_field_type *type)
{
	return bt_field_type_get_type_id(type) == BT_FIELD_TYPE_ID_ARRAY;
}

int bt_field_type_is_sequence(struct bt_field_type *type)
{
	return bt_field_type_get_type_id(type) == BT_FIELD_TYPE_ID_SEQUENCE;
}

int bt_field_type_is_variant(struct bt_field_type *type)
{
	return bt_field_type_get_type_id(type) == BT_FIELD_TYPE_ID_VARIANT;
}

/* Pre-2.0 CTF writer backward compatibility */
void bt_ctf_field_type_get(struct bt_field_type *type)
{
	bt_get(type);
}

/* Pre-2.0 CTF writer backward compatibility */
void bt_ctf_field_type_put(struct bt_field_type *type)
{
	bt_put(type);
}

BT_HIDDEN
void bt_field_type_freeze(struct bt_field_type *type)
{
	if (!type || type->frozen) {
		return;
	}

	type->freeze(type);
}

BT_HIDDEN
struct bt_field_type *bt_field_type_variant_get_field_type_signed(
		struct bt_field_type_variant *variant,
		int64_t tag_value)
{
	struct bt_field_type *type = NULL;
	GQuark field_name_quark;
	gpointer index;
	struct structure_field *field_entry;
	struct range_overlap_query query = {
		.range_start._signed = tag_value,
		.range_end._signed = tag_value,
		.mapping_name = 0,
		.overlaps = 0,
	};

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
struct bt_field_type *bt_field_type_variant_get_field_type_unsigned(
		struct bt_field_type_variant *variant,
		uint64_t tag_value)
{
	struct bt_field_type *type = NULL;
	GQuark field_name_quark;
	gpointer index;
	struct structure_field *field_entry;
	struct range_overlap_query query = {
		.range_start._unsigned = tag_value,
		.range_end._unsigned = tag_value,
		.mapping_name = 0,
		.overlaps = 0,
	};

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
int bt_field_type_serialize(struct bt_field_type *type,
		struct metadata_context *context)
{
	int ret;

	assert(type);
	assert(context);

	/* Make sure field type is valid before serializing it */
	ret = bt_field_type_validate(type);
	if (ret) {
		BT_LOGW("Cannot serialize field type's metadata: field type is invalid: "
			"addr=%p", type);
		goto end;
	}

	ret = type->serialize(type, context);
end:
	return ret;
}

struct bt_field_type *bt_field_type_copy(struct bt_field_type *type)
{
	struct bt_field_type *copy = NULL;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		goto end;
	}

	copy = type_copy_funcs[type->id](type);
	if (!copy) {
		BT_LOGE_STR("Cannot copy field type.");
		goto end;
	}

	copy->alignment = type->alignment;
end:
	return copy;
}

BT_HIDDEN
int bt_field_type_structure_get_field_name_index(
		struct bt_field_type *type, const char *name)
{
	int ret;
	size_t index;
	GQuark name_quark;
	struct bt_field_type_structure *structure;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (!name) {
		BT_LOGW_STR("Invalid parameter: field name is NULL.");
		ret = -1;
		goto end;
	}

	if (bt_field_type_get_type_id(type) != BT_FIELD_TYPE_ID_STRUCT) {
		BT_LOGW("Invalid parameter: field type is not a structure field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		ret = -1;
		goto end;
	}

	name_quark = g_quark_try_string(name);
	if (!name_quark) {
		BT_LOGV("No such structure field type field name: "
			"ft-addr=%p, field-name=\"%s\"",
			type, name);
		ret = -1;
		goto end;
	}

	structure = container_of(type, struct bt_field_type_structure,
		parent);
	if (!g_hash_table_lookup_extended(structure->field_name_to_index,
			GUINT_TO_POINTER(name_quark),
			NULL, (gpointer *)&index)) {
		BT_LOGV("No such structure field type field name: "
			"ft-addr=%p, field-name=\"%s\"",
			type, name);
		ret = -1;
		goto end;
	}
	ret = (int) index;
end:
	return ret;
}

BT_HIDDEN
int bt_field_type_variant_get_field_name_index(
		struct bt_field_type *type, const char *name)
{
	int ret;
	size_t index;
	GQuark name_quark;
	struct bt_field_type_variant *variant;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: variant field type is NULL.");
		ret = -1;
		goto end;
	}

	if (!name) {
		BT_LOGW_STR("Invalid parameter: field name is NULL.");
		ret = -1;
		goto end;
	}

	if (bt_field_type_get_type_id(type) != BT_FIELD_TYPE_ID_VARIANT) {
		BT_LOGW("Invalid parameter: field type is not a variant field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		ret = -1;
		goto end;
	}

	name_quark = g_quark_try_string(name);
	if (!name_quark) {
		BT_LOGV("No such variant field type field name: "
			"ft-addr=%p, field-name=\"%s\"",
			type, name);
		ret = -1;
		goto end;
	}

	variant = container_of(type, struct bt_field_type_variant,
		parent);
	if (!g_hash_table_lookup_extended(variant->field_name_to_index,
			GUINT_TO_POINTER(name_quark),
			NULL, (gpointer *)&index)) {
		BT_LOGV("No such variant field type field name: "
			"ft-addr=%p, field-name=\"%s\"",
			type, name);
		ret = -1;
		goto end;
	}
	ret = (int) index;
end:
	return ret;
}

BT_HIDDEN
int bt_field_type_sequence_set_length_field_path(
		struct bt_field_type *type,
		struct bt_field_path *path)
{
	int ret = 0;
	struct bt_field_type_sequence *sequence;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (bt_field_type_get_type_id(type) != BT_FIELD_TYPE_ID_SEQUENCE) {
		BT_LOGW("Invalid parameter: field type is not a sequence field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		ret = -1;
		goto end;
	}

	sequence = container_of(type, struct bt_field_type_sequence,
		parent);
	bt_get(path);
	BT_MOVE(sequence->length_field_path, path);
	BT_LOGV("Set sequence field type's length field path: ft-addr=%p, "
		"field-path-addr=%p", type, path);
end:
	return ret;
}

BT_HIDDEN
int bt_field_type_variant_set_tag_field_path(struct bt_field_type *type,
		struct bt_field_path *path)
{
	int ret = 0;
	struct bt_field_type_variant *variant;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (bt_field_type_get_type_id(type) != BT_FIELD_TYPE_ID_VARIANT) {
		BT_LOGW("Invalid parameter: field type is not a variant field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		ret = -1;
		goto end;
	}

	variant = container_of(type, struct bt_field_type_variant,
		parent);
	bt_get(path);
	BT_MOVE(variant->tag_field_path, path);
	BT_LOGV("Set variant field type's tag field path: ft-addr=%p, "
		"field-path-addr=%p", type, path);
end:
	return ret;
}

BT_HIDDEN
int bt_field_type_variant_set_tag_field_type(struct bt_field_type *type,
		struct bt_field_type *tag)
{
	int ret = 0;
	struct bt_field_type_variant *variant;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: variant field type is NULL.");
		ret = -1;
		goto end;
	}

	if (!tag) {
		BT_LOGW_STR("Invalid parameter: tag field type is NULL.");
		ret = -1;
		goto end;
	}

	if (bt_field_type_get_type_id(tag) != BT_FIELD_TYPE_ID_ENUM) {
		BT_LOGW("Invalid parameter: field type is not an enumeration field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		ret = -1;
		goto end;
	}

	variant = container_of(type, struct bt_field_type_variant,
		parent);
	bt_get(tag);
	if (variant->tag) {
		bt_put(&variant->tag->parent);
	}
	variant->tag = container_of(tag, struct bt_field_type_enumeration,
		parent);
	BT_LOGV("Set variant field type's tag field type: variant-ft-addr=%p, "
		"tag-ft-addr=%p", type, tag);
end:
	return ret;
}

static
void bt_field_type_integer_destroy(struct bt_field_type *type)
{
	struct bt_field_type_integer *integer =
		(struct bt_field_type_integer *) type;

	if (!type) {
		return;
	}

	BT_LOGD("Destroying integer field type object: addr=%p", type);
	BT_LOGD_STR("Putting mapped clock class.");
	bt_put(integer->mapped_clock);
	g_free(integer);
}

static
void bt_field_type_enumeration_destroy(struct bt_field_type *type)
{
	struct bt_field_type_enumeration *enumeration =
		(struct bt_field_type_enumeration *) type;

	if (!type) {
		return;
	}

	BT_LOGD("Destroying enumeration field type object: addr=%p", type);
	g_ptr_array_free(enumeration->entries, TRUE);
	BT_LOGD_STR("Putting container field type.");
	bt_put(enumeration->container);
	g_free(enumeration);
}

static
void bt_field_type_floating_point_destroy(struct bt_field_type *type)
{
	struct bt_field_type_floating_point *floating_point =
		(struct bt_field_type_floating_point *) type;

	if (!type) {
		return;
	}

	BT_LOGD("Destroying floating point number field type object: addr=%p", type);
	g_free(floating_point);
}

static
void bt_field_type_structure_destroy(struct bt_field_type *type)
{
	struct bt_field_type_structure *structure =
		(struct bt_field_type_structure *) type;

	if (!type) {
		return;
	}

	BT_LOGD("Destroying structure field type object: addr=%p", type);
	g_ptr_array_free(structure->fields, TRUE);
	g_hash_table_destroy(structure->field_name_to_index);
	g_free(structure);
}

static
void bt_field_type_variant_destroy(struct bt_field_type *type)
{
	struct bt_field_type_variant *variant =
		(struct bt_field_type_variant *) type;

	if (!type) {
		return;
	}

	BT_LOGD("Destroying variant field type object: addr=%p", type);
	g_ptr_array_free(variant->fields, TRUE);
	g_hash_table_destroy(variant->field_name_to_index);
	g_string_free(variant->tag_name, TRUE);
	BT_LOGD_STR("Putting tag field type.");
	bt_put(&variant->tag->parent);
	BT_PUT(variant->tag_field_path);
	g_free(variant);
}

static
void bt_field_type_array_destroy(struct bt_field_type *type)
{
	struct bt_field_type_array *array =
		(struct bt_field_type_array *) type;

	if (!type) {
		return;
	}

	BT_LOGD("Destroying array field type object: addr=%p", type);
	BT_LOGD_STR("Putting element field type.");
	bt_put(array->element_type);
	g_free(array);
}

static
void bt_field_type_sequence_destroy(struct bt_field_type *type)
{
	struct bt_field_type_sequence *sequence =
		(struct bt_field_type_sequence *) type;

	if (!type) {
		return;
	}

	BT_LOGD("Destroying sequence field type object: addr=%p", type);
	BT_LOGD_STR("Putting element field type.");
	bt_put(sequence->element_type);
	g_string_free(sequence->length_field_name, TRUE);
	BT_LOGD_STR("Putting length field path.");
	BT_PUT(sequence->length_field_path);
	g_free(sequence);
}

static
void bt_field_type_string_destroy(struct bt_field_type *type)
{
	struct bt_field_type_string *string =
		(struct bt_field_type_string *) type;

	if (!type) {
		return;
	}

	BT_LOGD("Destroying string field type object: addr=%p", type);
	g_free(string);
}

static
void generic_field_type_freeze(struct bt_field_type *type)
{
	type->frozen = 1;
}

static
void bt_field_type_integer_freeze(struct bt_field_type *type)
{
	struct bt_field_type_integer *integer_type = container_of(
		type, struct bt_field_type_integer, parent);

	BT_LOGD("Freezing integer field type object: addr=%p", type);

	if (integer_type->mapped_clock) {
		BT_LOGD_STR("Freezing integer field type's mapped clock class.");
		bt_clock_class_freeze(integer_type->mapped_clock);
	}

	generic_field_type_freeze(type);
}

static
void bt_field_type_enumeration_freeze(struct bt_field_type *type)
{
	struct bt_field_type_enumeration *enumeration_type = container_of(
		type, struct bt_field_type_enumeration, parent);

	BT_LOGD("Freezing enumeration field type object: addr=%p", type);
	set_enumeration_range_overlap(type);
	generic_field_type_freeze(type);
	BT_LOGD("Freezing enumeration field type object's container field type: int-ft-addr=%p",
		enumeration_type->container);
	bt_field_type_freeze(enumeration_type->container);
}

static
void freeze_structure_field(struct structure_field *field)
{
	BT_LOGD("Freezing structure/variant field type field: field-addr=%p, "
		"field-ft-addr=%p, field-name=\"%s\"", field,
		field->type, g_quark_to_string(field->name));
	bt_field_type_freeze(field->type);
}

static
void bt_field_type_structure_freeze(struct bt_field_type *type)
{
	struct bt_field_type_structure *structure_type = container_of(
		type, struct bt_field_type_structure, parent);

	/* Cache the alignment */
	BT_LOGD("Freezing structure field type object: addr=%p", type);
	type->alignment = bt_field_type_get_alignment(type);
	generic_field_type_freeze(type);
	g_ptr_array_foreach(structure_type->fields,
		(GFunc) freeze_structure_field, NULL);
}

static
void bt_field_type_variant_freeze(struct bt_field_type *type)
{
	struct bt_field_type_variant *variant_type = container_of(
		type, struct bt_field_type_variant, parent);

	BT_LOGD("Freezing variant field type object: addr=%p", type);
	generic_field_type_freeze(type);
	g_ptr_array_foreach(variant_type->fields,
		(GFunc) freeze_structure_field, NULL);
}

static
void bt_field_type_array_freeze(struct bt_field_type *type)
{
	struct bt_field_type_array *array_type = container_of(
		type, struct bt_field_type_array, parent);

	/* Cache the alignment */
	BT_LOGD("Freezing array field type object: addr=%p", type);
	type->alignment = bt_field_type_get_alignment(type);
	generic_field_type_freeze(type);
	BT_LOGD("Freezing array field type object's element field type: element-ft-addr=%p",
		array_type->element_type);
	bt_field_type_freeze(array_type->element_type);
}

static
void bt_field_type_sequence_freeze(struct bt_field_type *type)
{
	struct bt_field_type_sequence *sequence_type = container_of(
		type, struct bt_field_type_sequence, parent);

	/* Cache the alignment */
	BT_LOGD("Freezing sequence field type object: addr=%p", type);
	type->alignment = bt_field_type_get_alignment(type);
	generic_field_type_freeze(type);
	BT_LOGD("Freezing sequence field type object's element field type: element-ft-addr=%p",
		sequence_type->element_type);
	bt_field_type_freeze(sequence_type->element_type);
}

static
const char *get_encoding_string(enum bt_string_encoding encoding)
{
	const char *encoding_string;

	switch (encoding) {
	case BT_STRING_ENCODING_NONE:
		encoding_string = "none";
		break;
	case BT_STRING_ENCODING_ASCII:
		encoding_string = "ASCII";
		break;
	case BT_STRING_ENCODING_UTF8:
		encoding_string = "UTF8";
		break;
	default:
		encoding_string = "unknown";
		break;
	}

	return encoding_string;
}

static
const char *get_integer_base_string(enum bt_integer_base base)
{
	const char *base_string;

	switch (base) {
	case BT_INTEGER_BASE_DECIMAL:
	case BT_INTEGER_BASE_UNSPECIFIED:
		base_string = "decimal";
		break;
	case BT_INTEGER_BASE_HEXADECIMAL:
		base_string = "hexadecimal";
		break;
	case BT_INTEGER_BASE_OCTAL:
		base_string = "octal";
		break;
	case BT_INTEGER_BASE_BINARY:
		base_string = "binary";
		break;
	default:
		base_string = "unknown";
		break;
	}

	return base_string;
}

static
void append_field_name(struct metadata_context *context,
		const char *name)
{
	g_string_append_c(context->string, ' ');

	if (bt_identifier_is_valid(name) || *name == '_') {
		g_string_append_c(context->string, '_');
	}

	g_string_append(context->string, name);
}

static
int bt_field_type_integer_serialize(struct bt_field_type *type,
		struct metadata_context *context)
{
	struct bt_field_type_integer *integer = container_of(type,
		struct bt_field_type_integer, parent);
	int ret = 0;

	BT_LOGD("Serializing integer field type's metadata: "
		"ft-addr=%p, metadata-context-addr=%p", type, context);
	g_string_append_printf(context->string,
		"integer { size = %u; align = %u; signed = %s; encoding = %s; base = %s; byte_order = %s",
		integer->size, type->alignment,
		(integer->is_signed ? "true" : "false"),
		get_encoding_string(integer->encoding),
		get_integer_base_string(integer->base),
		get_byte_order_string(integer->user_byte_order));
	if (integer->mapped_clock) {
		const char *clock_name = bt_clock_class_get_name(
			integer->mapped_clock);

		assert(clock_name);
		g_string_append_printf(context->string,
			"; map = clock.%s.value", clock_name);
	}

	g_string_append(context->string, "; }");
	return ret;
}

static
int bt_field_type_enumeration_serialize(struct bt_field_type *type,
		struct metadata_context *context)
{
	size_t entry;
	int ret;
	struct bt_field_type_enumeration *enumeration = container_of(type,
		struct bt_field_type_enumeration, parent);
	struct bt_field_type *container_type;
	int container_signed;

	BT_LOGD("Serializing enumeration field type's metadata: "
		"ft-addr=%p, metadata-context-addr=%p", type, context);
	container_type = bt_field_type_enumeration_get_container_type(type);
	assert(container_type);
	container_signed = bt_ctf_field_type_integer_get_signed(container_type);
	assert(container_signed >= 0);
	g_string_append(context->string, "enum : ");
	BT_LOGD_STR("Serializing enumeration field type's container field type's metadata.");
	ret = bt_field_type_serialize(enumeration->container, context);
	if (ret) {
		BT_LOGW("Cannot serialize enumeration field type's container field type's metadata: "
			"container-ft-addr=%p", enumeration->container);
		goto end;
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
		append_field_name(context,
			context->field_name->str);
		g_string_assign(context->field_name, "");
	}
end:
	bt_put(container_type);
	return ret;
}

static
int bt_field_type_floating_point_serialize(struct bt_field_type *type,
		struct metadata_context *context)
{
	struct bt_field_type_floating_point *floating_point = container_of(
		type, struct bt_field_type_floating_point, parent);

	BT_LOGD("Serializing floating point number field type's metadata: "
		"ft-addr=%p, metadata-context-addr=%p", type, context);
	g_string_append_printf(context->string,
		"floating_point { exp_dig = %u; mant_dig = %u; byte_order = %s; align = %u; }",
		floating_point->exp_dig,
		floating_point->mant_dig,
		get_byte_order_string(floating_point->user_byte_order),
		type->alignment);
	return 0;
}

static
int bt_field_type_structure_serialize(struct bt_field_type *type,
		struct metadata_context *context)
{
	size_t i;
	unsigned int indent;
	int ret = 0;
	struct bt_field_type_structure *structure = container_of(type,
		struct bt_field_type_structure, parent);
	GString *structure_field_name = context->field_name;

	BT_LOGD("Serializing structure field type's metadata: "
		"ft-addr=%p, metadata-context-addr=%p", type, context);
	context->field_name = g_string_new("");

	context->current_indentation_level++;
	g_string_append(context->string, "struct {\n");

	for (i = 0; i < structure->fields->len; i++) {
		struct structure_field *field = structure->fields->pdata[i];

		BT_LOGD("Serializing structure field type's field metadata: "
			"index=%zu, "
			"field-ft-addr=%p, field-name=\"%s\"",
			i, field, g_quark_to_string(field->name));

		for (indent = 0; indent < context->current_indentation_level;
			indent++) {
			g_string_append_c(context->string, '\t');
		}

		g_string_assign(context->field_name,
			g_quark_to_string(field->name));
		ret = bt_field_type_serialize(field->type, context);
		if (ret) {
			BT_LOGW("Cannot serialize structure field type's field's metadata: "
				"index=%zu, "
				"field-ft-addr=%p, field-name=\"%s\"",
				i, field->type,
				g_quark_to_string(field->name));
			goto end;
		}

		if (context->field_name->len) {
			append_field_name(context,
				context->field_name->str);
		}
		g_string_append(context->string, ";\n");
	}

	context->current_indentation_level--;
	for (indent = 0; indent < context->current_indentation_level;
		indent++) {
		g_string_append_c(context->string, '\t');
	}

	g_string_append_printf(context->string, "} align(%u)",
		 type->alignment);
end:
	g_string_free(context->field_name, TRUE);
	context->field_name = structure_field_name;
	return ret;
}

static
int bt_field_type_variant_serialize(struct bt_field_type *type,
		struct metadata_context *context)
{
	size_t i;
	unsigned int indent;
	int ret = 0;
	struct bt_field_type_variant *variant = container_of(
		type, struct bt_field_type_variant, parent);
	GString *variant_field_name = context->field_name;

	BT_LOGD("Serializing variant field type's metadata: "
		"ft-addr=%p, metadata-context-addr=%p", type, context);
	context->field_name = g_string_new("");
	if (variant->tag_name->len > 0) {
		g_string_append(context->string, "variant <");
	        append_field_name(context, variant->tag_name->str);
		g_string_append(context->string, "> {\n");
	} else {
		g_string_append(context->string, "variant {\n");
	}

	context->current_indentation_level++;
	for (i = 0; i < variant->fields->len; i++) {
		struct structure_field *field = variant->fields->pdata[i];

		BT_LOGD("Serializing variant field type's field metadata: "
			"index=%zu, "
			"field-ft-addr=%p, field-name=\"%s\"",
			i, field, g_quark_to_string(field->name));

		g_string_assign(context->field_name,
			g_quark_to_string(field->name));
		for (indent = 0; indent < context->current_indentation_level;
			indent++) {
			g_string_append_c(context->string, '\t');
		}

		g_string_assign(context->field_name,
			g_quark_to_string(field->name));
		ret = bt_field_type_serialize(field->type, context);
		if (ret) {
			BT_LOGW("Cannot serialize variant field type's field's metadata: "
				"index=%zu, "
				"field-ft-addr=%p, field-name=\"%s\"",
				i, field->type,
				g_quark_to_string(field->name));
			goto end;
		}

		if (context->field_name->len) {
			append_field_name(context,
				context->field_name->str);
			g_string_append_c(context->string, ';');
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
int bt_field_type_array_serialize(struct bt_field_type *type,
		struct metadata_context *context)
{
	int ret = 0;
	struct bt_field_type_array *array = container_of(type,
		struct bt_field_type_array, parent);

	BT_LOGD("Serializing array field type's metadata: "
		"ft-addr=%p, metadata-context-addr=%p", type, context);
	BT_LOGD_STR("Serializing array field type's element field type's metadata.");
	ret = bt_field_type_serialize(array->element_type, context);
	if (ret) {
		BT_LOGW("Cannot serialize array field type's element field type's metadata: "
			"element-ft-addr=%p", array->element_type);
		goto end;
	}

	if (context->field_name->len) {
		append_field_name(context,
			context->field_name->str);

		g_string_append_printf(context->string, "[%u]", array->length);
		g_string_assign(context->field_name, "");
	} else {
		g_string_append_printf(context->string, "[%u]", array->length);
	}
end:
	return ret;
}

static
int bt_field_type_sequence_serialize(struct bt_field_type *type,
		struct metadata_context *context)
{
	int ret = 0;
	struct bt_field_type_sequence *sequence = container_of(
		type, struct bt_field_type_sequence, parent);

	BT_LOGD("Serializing sequence field type's metadata: "
		"ft-addr=%p, metadata-context-addr=%p", type, context);
	BT_LOGD_STR("Serializing sequence field type's element field type's metadata.");
	ret = bt_field_type_serialize(sequence->element_type, context);
	if (ret) {
		BT_LOGW("Cannot serialize sequence field type's element field type's metadata: "
			"element-ft-addr=%p", sequence->element_type);
		goto end;
	}

	if (context->field_name->len) {
		append_field_name(context, context->field_name->str);
		g_string_assign(context->field_name, "");
	}
	g_string_append(context->string, "[");
	append_field_name(context, sequence->length_field_name->str);
	g_string_append(context->string, "]");
end:
	return ret;
}

static
int bt_field_type_string_serialize(struct bt_field_type *type,
		struct metadata_context *context)
{
	struct bt_field_type_string *string = container_of(
		type, struct bt_field_type_string, parent);

	BT_LOGD("Serializing string field type's metadata: "
		"ft-addr=%p, metadata-context-addr=%p", type, context);
	g_string_append_printf(context->string,
		"string { encoding = %s; }",
		get_encoding_string(string->encoding));
	return 0;
}

static
void bt_field_type_integer_set_byte_order(struct bt_field_type *type,
		enum bt_byte_order byte_order)
{
	struct bt_field_type_integer *integer_type = container_of(type,
		struct bt_field_type_integer, parent);

	integer_type->user_byte_order = byte_order;
}

static
void bt_field_type_enumeration_set_byte_order(
		struct bt_field_type *type, enum bt_byte_order byte_order)
{
	struct bt_field_type_enumeration *enum_type = container_of(type,
		struct bt_field_type_enumeration, parent);

	/* Safe to assume that container is an integer */
	bt_field_type_integer_set_byte_order(enum_type->container,
		byte_order);
}

static
void bt_field_type_floating_point_set_byte_order(
		struct bt_field_type *type, enum bt_byte_order byte_order)
{
	struct bt_field_type_floating_point *floating_point_type =
		container_of(type, struct bt_field_type_floating_point,
		parent);

	floating_point_type->user_byte_order = byte_order;
}

static
void bt_field_type_structure_set_byte_order(struct bt_field_type *type,
		enum bt_byte_order byte_order)
{
	int i;
	struct bt_field_type_structure *structure_type =
		container_of(type, struct bt_field_type_structure,
		parent);

	for (i = 0; i < structure_type->fields->len; i++) {
		struct structure_field *field = g_ptr_array_index(
			structure_type->fields, i);
		struct bt_field_type *field_type = field->type;

		if (set_byte_order_funcs[field_type->id]) {
			set_byte_order_funcs[field_type->id](
				field_type, byte_order);
		}
	}
}

static
void bt_field_type_variant_set_byte_order(struct bt_field_type *type,
		enum bt_byte_order byte_order)
{
	int i;
	struct bt_field_type_variant *variant_type =
		container_of(type, struct bt_field_type_variant,
		parent);

	for (i = 0; i < variant_type->fields->len; i++) {
		struct structure_field *field = g_ptr_array_index(
			variant_type->fields, i);
		struct bt_field_type *field_type = field->type;

		if (set_byte_order_funcs[field_type->id]) {
			set_byte_order_funcs[field_type->id](
				field_type, byte_order);
		}
	}
}

static
void bt_field_type_array_set_byte_order(struct bt_field_type *type,
		enum bt_byte_order byte_order)
{
	struct bt_field_type_array *array_type =
		container_of(type, struct bt_field_type_array,
		parent);

	if (set_byte_order_funcs[array_type->element_type->id]) {
		set_byte_order_funcs[array_type->element_type->id](
			array_type->element_type, byte_order);
	}
}

static
void bt_field_type_sequence_set_byte_order(struct bt_field_type *type,
		enum bt_byte_order byte_order)
{
	struct bt_field_type_sequence *sequence_type =
		container_of(type, struct bt_field_type_sequence,
		parent);

	if (set_byte_order_funcs[
		sequence_type->element_type->id]) {
		set_byte_order_funcs[
			sequence_type->element_type->id](
			sequence_type->element_type, byte_order);
	}
}

static
struct bt_field_type *bt_field_type_integer_copy(
		struct bt_field_type *type)
{
	struct bt_field_type *copy;
	struct bt_field_type_integer *integer, *copy_integer;

	BT_LOGD("Copying integer field type's: addr=%p", type);
	integer = container_of(type, struct bt_field_type_integer, parent);
	copy = bt_field_type_integer_create(integer->size);
	if (!copy) {
		BT_LOGE_STR("Cannot create integer field type.");
		goto end;
	}

	copy_integer = container_of(copy, struct bt_field_type_integer,
		parent);
	copy_integer->mapped_clock = bt_get(integer->mapped_clock);
	copy_integer->user_byte_order = integer->user_byte_order;
	copy_integer->is_signed = integer->is_signed;
	copy_integer->size = integer->size;
	copy_integer->base = integer->base;
	copy_integer->encoding = integer->encoding;
	BT_LOGD("Copied integer field type: original-ft-addr=%p, copy-ft-addr=%p",
		type, copy);

end:
	return copy;
}

static
struct bt_field_type *bt_field_type_enumeration_copy(
		struct bt_field_type *type)
{
	size_t i;
	struct bt_field_type *copy = NULL, *copy_container;
	struct bt_field_type_enumeration *enumeration, *copy_enumeration;

	BT_LOGD("Copying enumeration field type's: addr=%p", type);
	enumeration = container_of(type, struct bt_field_type_enumeration,
		parent);

	/* Copy the source enumeration's container */
	BT_LOGD_STR("Copying enumeration field type's container field type.");
	copy_container = bt_field_type_copy(enumeration->container);
	if (!copy_container) {
		BT_LOGE_STR("Cannot copy enumeration field type's container field type.");
		goto end;
	}

	copy = bt_field_type_enumeration_create(copy_container);
	if (!copy) {
		BT_LOGE_STR("Cannot create enumeration field type.");
		goto end;
	}
	copy_enumeration = container_of(copy,
		struct bt_field_type_enumeration, parent);

	/* Copy all enumaration entries */
	for (i = 0; i < enumeration->entries->len; i++) {
		struct enumeration_mapping *mapping = g_ptr_array_index(
			enumeration->entries, i);
		struct enumeration_mapping *copy_mapping = g_new0(
			struct enumeration_mapping, 1);

		if (!copy_mapping) {
			BT_LOGE_STR("Failed to allocate one enumeration mapping.");
			goto error;
		}

		*copy_mapping = *mapping;
		g_ptr_array_add(copy_enumeration->entries, copy_mapping);
	}

	BT_LOGD("Copied enumeration field type: original-ft-addr=%p, copy-ft-addr=%p",
		type, copy);

end:
	bt_put(copy_container);
	return copy;
error:
	bt_put(copy_container);
        BT_PUT(copy);
	return copy;
}

static
struct bt_field_type *bt_field_type_floating_point_copy(
		struct bt_field_type *type)
{
	struct bt_field_type *copy;
	struct bt_field_type_floating_point *floating_point, *copy_float;

	BT_LOGD("Copying floating point number field type's: addr=%p", type);
	floating_point = container_of(type,
		struct bt_field_type_floating_point, parent);
	copy = bt_field_type_floating_point_create();
	if (!copy) {
		BT_LOGE_STR("Cannot create floating point number field type.");
		goto end;
	}

	copy_float = container_of(copy,
		struct bt_field_type_floating_point, parent);
	copy_float->user_byte_order = floating_point->user_byte_order;
	copy_float->exp_dig = floating_point->exp_dig;
	copy_float->mant_dig = floating_point->mant_dig;
	BT_LOGD("Copied floating point number field type: original-ft-addr=%p, copy-ft-addr=%p",
		type, copy);
end:
	return copy;
}

static
struct bt_field_type *bt_field_type_structure_copy(
		struct bt_field_type *type)
{
	int64_t i;
	GHashTableIter iter;
	gpointer key, value;
	struct bt_field_type *copy;
	struct bt_field_type_structure *structure, *copy_structure;

	BT_LOGD("Copying structure field type's: addr=%p", type);
	structure = container_of(type, struct bt_field_type_structure,
		parent);
	copy = bt_field_type_structure_create();
	if (!copy) {
		BT_LOGE_STR("Cannot create structure field type.");
		goto end;
	}

	copy_structure = container_of(copy,
		struct bt_field_type_structure, parent);

	/* Copy field_name_to_index */
	g_hash_table_iter_init(&iter, structure->field_name_to_index);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		g_hash_table_insert(copy_structure->field_name_to_index,
			key, value);
	}

	for (i = 0; i < structure->fields->len; i++) {
		struct structure_field *entry, *copy_entry;
		struct bt_field_type *copy_field;

		entry = g_ptr_array_index(structure->fields, i);
		BT_LOGD("Copying structure field type's field: "
			"index=%" PRId64 ", "
			"field-ft-addr=%p, field-name=\"%s\"",
			i, entry, g_quark_to_string(entry->name));
		copy_entry = g_new0(struct structure_field, 1);
		if (!copy_entry) {
			BT_LOGE_STR("Failed to allocate one structure field type field.");
			goto error;
		}

		copy_field = bt_field_type_copy(entry->type);
		if (!copy_field) {
			BT_LOGE("Cannot copy structure field type's field: "
				"index=%" PRId64 ", "
				"field-ft-addr=%p, field-name=\"%s\"",
				i, entry, g_quark_to_string(entry->name));
			g_free(copy_entry);
			goto error;
		}

		copy_entry->name = entry->name;
		copy_entry->type = copy_field;
		g_ptr_array_add(copy_structure->fields, copy_entry);
	}

	BT_LOGD("Copied structure field type: original-ft-addr=%p, copy-ft-addr=%p",
		type, copy);

end:
	return copy;
error:
        BT_PUT(copy);
	return copy;
}

static
struct bt_field_type *bt_field_type_variant_copy(
		struct bt_field_type *type)
{
	int64_t i;
	GHashTableIter iter;
	gpointer key, value;
	struct bt_field_type *copy = NULL, *copy_tag = NULL;
	struct bt_field_type_variant *variant, *copy_variant;

	BT_LOGD("Copying variant field type's: addr=%p", type);
	variant = container_of(type, struct bt_field_type_variant,
		parent);
	if (variant->tag) {
		BT_LOGD_STR("Copying variant field type's tag field type.");
		copy_tag = bt_field_type_copy(&variant->tag->parent);
		if (!copy_tag) {
			BT_LOGE_STR("Cannot copy variant field type's tag field type.");
			goto end;
		}
	}

	copy = bt_field_type_variant_create(copy_tag,
		variant->tag_name->len ? variant->tag_name->str : NULL);
	if (!copy) {
		BT_LOGE_STR("Cannot create variant field type.");
		goto end;
	}

	copy_variant = container_of(copy, struct bt_field_type_variant,
		parent);

	/* Copy field_name_to_index */
	g_hash_table_iter_init(&iter, variant->field_name_to_index);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		g_hash_table_insert(copy_variant->field_name_to_index,
			key, value);
	}

	for (i = 0; i < variant->fields->len; i++) {
		struct structure_field *entry, *copy_entry;
		struct bt_field_type *copy_field;

		entry = g_ptr_array_index(variant->fields, i);
		BT_LOGD("Copying variant field type's field: "
			"index=%" PRId64 ", "
			"field-ft-addr=%p, field-name=\"%s\"",
			i, entry, g_quark_to_string(entry->name));
		copy_entry = g_new0(struct structure_field, 1);
		if (!copy_entry) {
			BT_LOGE_STR("Failed to allocate one variant field type field.");
			goto error;
		}

		copy_field = bt_field_type_copy(entry->type);
		if (!copy_field) {
			BT_LOGE("Cannot copy variant field type's field: "
				"index=%" PRId64 ", "
				"field-ft-addr=%p, field-name=\"%s\"",
				i, entry, g_quark_to_string(entry->name));
			g_free(copy_entry);
			goto error;
		}

		copy_entry->name = entry->name;
		copy_entry->type = copy_field;
		g_ptr_array_add(copy_variant->fields, copy_entry);
	}

	if (variant->tag_field_path) {
		BT_LOGD_STR("Copying variant field type's tag field path.");
		copy_variant->tag_field_path = bt_field_path_copy(
			variant->tag_field_path);
		if (!copy_variant->tag_field_path) {
			BT_LOGE_STR("Cannot copy variant field type's tag field path.");
			goto error;
		}
	}

	BT_LOGD("Copied variant field type: original-ft-addr=%p, copy-ft-addr=%p",
		type, copy);

end:
	bt_put(copy_tag);
	return copy;
error:
	bt_put(copy_tag);
        BT_PUT(copy);
	return copy;
}

static
struct bt_field_type *bt_field_type_array_copy(
		struct bt_field_type *type)
{
	struct bt_field_type *copy = NULL, *copy_element;
	struct bt_field_type_array *array;

	BT_LOGD("Copying array field type's: addr=%p", type);
	array = container_of(type, struct bt_field_type_array,
		parent);
	BT_LOGD_STR("Copying array field type's element field type.");
	copy_element = bt_field_type_copy(array->element_type);
	if (!copy_element) {
		BT_LOGE_STR("Cannot copy array field type's element field type.");
		goto end;
	}

	copy = bt_field_type_array_create(copy_element, array->length);
	if (!copy) {
		BT_LOGE_STR("Cannot create array field type.");
		goto end;
	}

	BT_LOGD("Copied array field type: original-ft-addr=%p, copy-ft-addr=%p",
		type, copy);

end:
	bt_put(copy_element);
	return copy;
}

static
struct bt_field_type *bt_field_type_sequence_copy(
		struct bt_field_type *type)
{
	struct bt_field_type *copy = NULL, *copy_element;
	struct bt_field_type_sequence *sequence, *copy_sequence;

	BT_LOGD("Copying sequence field type's: addr=%p", type);
	sequence = container_of(type, struct bt_field_type_sequence,
		parent);
	BT_LOGD_STR("Copying sequence field type's element field type.");
	copy_element = bt_field_type_copy(sequence->element_type);
	if (!copy_element) {
		BT_LOGE_STR("Cannot copy sequence field type's element field type.");
		goto end;
	}

	copy = bt_field_type_sequence_create(copy_element,
		sequence->length_field_name->len ?
			sequence->length_field_name->str : NULL);
	if (!copy) {
		BT_LOGE_STR("Cannot create sequence field type.");
		goto end;
	}

	copy_sequence = container_of(copy, struct bt_field_type_sequence,
		parent);
	if (sequence->length_field_path) {
		BT_LOGD_STR("Copying sequence field type's length field path.");
		copy_sequence->length_field_path = bt_field_path_copy(
			sequence->length_field_path);
		if (!copy_sequence->length_field_path) {
			BT_LOGE_STR("Cannot copy sequence field type's length field path.");
			goto error;
		}
	}

	BT_LOGD("Copied sequence field type: original-ft-addr=%p, copy-ft-addr=%p",
		type, copy);

end:
	bt_put(copy_element);
	return copy;
error:
	BT_PUT(copy);
	goto end;
}

static
struct bt_field_type *bt_field_type_string_copy(
		struct bt_field_type *type)
{
	struct bt_field_type *copy;

	BT_LOGD("Copying string field type's: addr=%p", type);
	copy = bt_field_type_string_create();
	if (!copy) {
		BT_LOGE_STR("Cannot create string field type.");
		goto end;
	}

	BT_LOGD("Copied string field type: original-ft-addr=%p, copy-ft-addr=%p",
		type, copy);
end:
	return copy;
}

static
int bt_field_type_integer_compare(struct bt_field_type *type_a,
		struct bt_field_type *type_b)
{
	int ret = 1;
	struct bt_field_type_integer *int_type_a;
	struct bt_field_type_integer *int_type_b;

	int_type_a = container_of(type_a, struct bt_field_type_integer,
		parent);
	int_type_b = container_of(type_b, struct bt_field_type_integer,
		parent);

	/* Length */
	if (int_type_a->size != int_type_b->size) {
		BT_LOGV("Integer field types differ: different sizes: "
			"ft-a-size=%u, ft-b-size=%u",
			int_type_a->size, int_type_b->size);
		goto end;
	}

	/* Byte order */
	if (int_type_a->user_byte_order != int_type_b->user_byte_order) {
		BT_LOGV("Integer field types differ: different byte orders: "
			"ft-a-bo=%s, ft-b-bo=%s",
			bt_byte_order_string(int_type_a->user_byte_order),
			bt_byte_order_string(int_type_b->user_byte_order));
		goto end;
	}

	/* Signedness */
	if (int_type_a->is_signed != int_type_b->is_signed) {
		BT_LOGV("Integer field types differ: different signedness: "
			"ft-a-is-signed=%d, ft-b-is-signed=%d",
			int_type_a->is_signed,
			int_type_b->is_signed);
		goto end;
	}

	/* Base */
	if (int_type_a->base != int_type_b->base) {
		BT_LOGV("Integer field types differ: different bases: "
			"ft-a-base=%s, ft-b-base=%s",
			bt_integer_base_string(int_type_a->base),
			bt_integer_base_string(int_type_b->base));
		goto end;
	}

	/* Encoding */
	if (int_type_a->encoding != int_type_b->encoding) {
		BT_LOGV("Integer field types differ: different encodings: "
			"ft-a-encoding=%s, ft-b-encoding=%s",
			bt_string_encoding_string(int_type_a->encoding),
			bt_string_encoding_string(int_type_b->encoding));
		goto end;
	}

	/* Mapped clock */
	if (int_type_a->mapped_clock != int_type_b->mapped_clock) {
		BT_LOGV("Integer field types differ: different mapped clock classes: "
			"ft-a-mapped-clock-class-addr=%p, "
			"ft-b-mapped-clock-class-addr=%p, "
			"ft-a-mapped-clock-class-name=\"%s\", "
			"ft-b-mapped-clock-class-name=\"%s\"",
			int_type_a->mapped_clock, int_type_b->mapped_clock,
			int_type_a->mapped_clock ? bt_clock_class_get_name(int_type_a->mapped_clock) : "",
			int_type_b->mapped_clock ? bt_clock_class_get_name(int_type_b->mapped_clock) : "");
		goto end;
	}

	/* Equal */
	ret = 0;

end:
	return ret;
}

static
int bt_field_type_floating_point_compare(struct bt_field_type *type_a,
		struct bt_field_type *type_b)
{
	int ret = 1;
	struct bt_field_type_floating_point *float_a;
	struct bt_field_type_floating_point *float_b;

	float_a = container_of(type_a,
		struct bt_field_type_floating_point, parent);
	float_b = container_of(type_b,
		struct bt_field_type_floating_point, parent);

	/* Byte order */
	if (float_a->user_byte_order != float_b->user_byte_order) {
		BT_LOGV("Floating point number field types differ: different byte orders: "
			"ft-a-bo=%s, ft-b-bo=%s",
			bt_byte_order_string(float_a->user_byte_order),
			bt_byte_order_string(float_b->user_byte_order));
		goto end;
	}

	/* Exponent length */
	if (float_a->exp_dig != float_b->exp_dig) {
		BT_LOGV("Floating point number field types differ: different exponent sizes: "
			"ft-a-exp-size=%u, ft-b-exp-size=%u",
			float_a->exp_dig, float_b->exp_dig);
		goto end;
	}

	/* Mantissa length */
	if (float_a->mant_dig != float_b->mant_dig) {
		BT_LOGV("Floating point number field types differ: different mantissa sizes: "
			"ft-a-mant-size=%u, ft-b-mant-size=%u",
			float_a->mant_dig, float_b->mant_dig);
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
		BT_LOGV("Enumeration field type mappings differ: different names: "
			"mapping-a-name=\"%s\", mapping-b-name=\"%s\"",
			g_quark_to_string(mapping_a->string),
			g_quark_to_string(mapping_b->string));
		goto end;
	}

	/* Range start */
	if (mapping_a->range_start._unsigned !=
			mapping_b->range_start._unsigned) {
		BT_LOGV("Enumeration field type mappings differ: different starts of range: "
			"mapping-a-range-start-unsigned=%" PRIu64 ", "
			"mapping-b-range-start-unsigned=%" PRIu64,
			mapping_a->range_start._unsigned,
			mapping_b->range_start._unsigned);
		goto end;
	}

	/* Range end */
	if (mapping_a->range_end._unsigned !=
			mapping_b->range_end._unsigned) {
		BT_LOGV("Enumeration field type mappings differ: different ends of range: "
			"mapping-a-range-end-unsigned=%" PRIu64 ", "
			"mapping-b-range-end-unsigned=%" PRIu64,
			mapping_a->range_end._unsigned,
			mapping_b->range_end._unsigned);
		goto end;
	}

	/* Equal */
	ret = 0;

end:
	return ret;
}

static
int bt_field_type_enumeration_compare(struct bt_field_type *type_a,
		struct bt_field_type *type_b)
{
	int ret = 1;
	int i;
	struct bt_field_type_enumeration *enum_a;
	struct bt_field_type_enumeration *enum_b;

	enum_a = container_of(type_a,
		struct bt_field_type_enumeration, parent);
	enum_b = container_of(type_b,
		struct bt_field_type_enumeration, parent);

	/* Container field type */
	ret = bt_field_type_compare(enum_a->container, enum_b->container);
	if (ret) {
		BT_LOGV("Enumeration field types differ: different container field types: "
			"ft-a-container-ft-addr=%p, ft-b-container-ft-addr=%p",
			enum_a->container, enum_b->container);
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
			BT_LOGV("Enumeration field types differ: different mappings: "
				"ft-a-mapping-addr=%p, ft-b-mapping-addr=%p, "
				"ft-a-mapping-name=\"%s\", ft-b-mapping-name=\"%s\"",
				mapping_a, mapping_b,
				g_quark_to_string(mapping_a->string),
				g_quark_to_string(mapping_b->string));
			goto end;
		}
	}

	/* Equal */
	ret = 0;

end:
	return ret;
}

static
int bt_field_type_string_compare(struct bt_field_type *type_a,
		struct bt_field_type *type_b)
{
	int ret = 1;
	struct bt_field_type_string *string_a;
	struct bt_field_type_string *string_b;

	string_a = container_of(type_a,
		struct bt_field_type_string, parent);
	string_b = container_of(type_b,
		struct bt_field_type_string, parent);

	/* Encoding */
	if (string_a->encoding != string_b->encoding) {
		BT_LOGV("String field types differ: different encodings: "
			"ft-a-encoding=%s, ft-b-encoding=%s",
			bt_string_encoding_string(string_a->encoding),
			bt_string_encoding_string(string_b->encoding));
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
		BT_LOGV("Structure/variant field type fields differ: different names: "
			"field-a-name=%s, field-b-name=%s",
			g_quark_to_string(field_a->name),
			g_quark_to_string(field_b->name));
		goto end;
	}

	/* Type */
	ret = bt_field_type_compare(field_a->type, field_b->type);
	if (ret == 1) {
		BT_LOGV("Structure/variant field type fields differ: different field types: "
			"field-name=\"%s\", field-a-ft-addr=%p, field-b-ft-addr=%p",
			g_quark_to_string(field_a->name),
			field_a->type, field_b->type);
	}

end:
	return ret;
}

static
int bt_field_type_structure_compare(struct bt_field_type *type_a,
		struct bt_field_type *type_b)
{
	int ret = 1;
	int i;
	struct bt_field_type_structure *struct_a;
	struct bt_field_type_structure *struct_b;

	struct_a = container_of(type_a,
		struct bt_field_type_structure, parent);
	struct_b = container_of(type_b,
		struct bt_field_type_structure, parent);

	/* Alignment */
	if (bt_field_type_get_alignment(type_a) !=
			bt_field_type_get_alignment(type_b)) {
		BT_LOGV("Structure field types differ: different alignments: "
			"ft-a-align=%u, ft-b-align=%u",
			bt_field_type_get_alignment(type_a),
			bt_field_type_get_alignment(type_b));
		goto end;
	}

	/* Fields */
	if (struct_a->fields->len != struct_b->fields->len) {
		BT_LOGV("Structure field types differ: different field counts: "
			"ft-a-field-count=%u, ft-b-field-count=%u",
			struct_a->fields->len, struct_b->fields->len);
		goto end;
	}

	for (i = 0; i < struct_a->fields->len; ++i) {
		struct structure_field *field_a =
			g_ptr_array_index(struct_a->fields, i);
		struct structure_field *field_b =
			g_ptr_array_index(struct_b->fields, i);

		ret = compare_structure_fields(field_a, field_b);
		if (ret) {
			/* compare_structure_fields() logs what differs */
			BT_LOGV_STR("Structure field types differ: different fields.");
			goto end;
		}
	}

	/* Equal */
	ret = 0;

end:
	return ret;
}

static
int bt_field_type_variant_compare(struct bt_field_type *type_a,
		struct bt_field_type *type_b)
{
	int ret = 1;
	int i;
	struct bt_field_type_variant *variant_a;
	struct bt_field_type_variant *variant_b;

	variant_a = container_of(type_a,
		struct bt_field_type_variant, parent);
	variant_b = container_of(type_b,
		struct bt_field_type_variant, parent);

	/* Tag name */
	if (strcmp(variant_a->tag_name->str, variant_b->tag_name->str)) {
		BT_LOGV("Variant field types differ: different tag field names: "
			"ft-a-tag-field-name=\"%s\", ft-b-tag-field-name=\"%s\"",
			variant_a->tag_name->str, variant_b->tag_name->str);
		goto end;
	}

	/* Tag type */
	ret = bt_field_type_compare(
		(struct bt_field_type *) variant_a->tag,
		(struct bt_field_type *) variant_b->tag);
	if (ret) {
		BT_LOGV("Variant field types differ: different tag field types: "
			"ft-a-tag-ft-addr=%p, ft-b-tag-ft-addr=%p",
			variant_a->tag, variant_b->tag);
		goto end;
	}

	ret = 1;

	/* Fields */
	if (variant_a->fields->len != variant_b->fields->len) {
		BT_LOGV("Structure field types differ: different field counts: "
			"ft-a-field-count=%u, ft-b-field-count=%u",
			variant_a->fields->len, variant_b->fields->len);
		goto end;
	}

	for (i = 0; i < variant_a->fields->len; ++i) {
		struct structure_field *field_a =
			g_ptr_array_index(variant_a->fields, i);
		struct structure_field *field_b =
			g_ptr_array_index(variant_b->fields, i);

		ret = compare_structure_fields(field_a, field_b);
		if (ret) {
			/* compare_structure_fields() logs what differs */
			BT_LOGV_STR("Variant field types differ: different fields.");
			goto end;
		}
	}

	/* Equal */
	ret = 0;

end:
	return ret;
}

static
int bt_field_type_array_compare(struct bt_field_type *type_a,
		struct bt_field_type *type_b)
{
	int ret = 1;
	struct bt_field_type_array *array_a;
	struct bt_field_type_array *array_b;

	array_a = container_of(type_a,
		struct bt_field_type_array, parent);
	array_b = container_of(type_b,
		struct bt_field_type_array, parent);

	/* Length */
	if (array_a->length != array_b->length) {
		BT_LOGV("Structure field types differ: different lengths: "
			"ft-a-length=%u, ft-b-length=%u",
			array_a->length, array_b->length);
		goto end;
	}

	/* Element type */
	ret = bt_field_type_compare(array_a->element_type,
		array_b->element_type);
	if (ret == 1) {
		BT_LOGV("Array field types differ: different element field types: "
			"ft-a-element-ft-addr=%p, ft-b-element-ft-addr=%p",
			array_a->element_type, array_b->element_type);
	}

end:
	return ret;
}

static
int bt_field_type_sequence_compare(struct bt_field_type *type_a,
		struct bt_field_type *type_b)
{
	int ret = -1;
	struct bt_field_type_sequence *sequence_a;
	struct bt_field_type_sequence *sequence_b;

	sequence_a = container_of(type_a,
		struct bt_field_type_sequence, parent);
	sequence_b = container_of(type_b,
		struct bt_field_type_sequence, parent);

	/* Length name */
	if (strcmp(sequence_a->length_field_name->str,
			sequence_b->length_field_name->str)) {
		BT_LOGV("Sequence field types differ: different length field names: "
			"ft-a-length-field-name=\"%s\", "
			"ft-b-length-field-name=\"%s\"",
			sequence_a->length_field_name->str,
			sequence_b->length_field_name->str);
		goto end;
	}

	/* Element type */
	ret = bt_field_type_compare(sequence_a->element_type,
			sequence_b->element_type);
	if (ret == 1) {
		BT_LOGV("Sequence field types differ: different element field types: "
			"ft-a-element-ft-addr=%p, ft-b-element-ft-addr=%p",
			sequence_a->element_type, sequence_b->element_type);
	}

end:
	return ret;
}

int bt_field_type_compare(struct bt_field_type *type_a,
		struct bt_field_type *type_b)
{
	int ret = 1;

	if (type_a == type_b) {
		/* Same reference: equal (even if both are NULL) */
		ret = 0;
		goto end;
	}

	if (!type_a) {
		BT_LOGW_STR("Invalid parameter: field type A is NULL.");
		ret = -1;
		goto end;
	}

	if (!type_b) {
		BT_LOGW_STR("Invalid parameter: field type B is NULL.");
		ret = -1;
		goto end;
	}

	if (type_a->id != type_b->id) {
		/* Different type IDs */
		BT_LOGV("Field types differ: different IDs: "
			"ft-a-addr=%p, ft-b-addr=%p, "
			"ft-a-id=%s, ft-b-id=%s",
			type_a, type_b,
			bt_field_type_id_string(type_a->id),
			bt_field_type_id_string(type_b->id));
		goto end;
	}

	if (type_a->id == BT_FIELD_TYPE_ID_UNKNOWN) {
		/* Both have unknown type IDs */
		BT_LOGW_STR("Invalid parameter: field type IDs are unknown.");
		goto end;
	}

	ret = type_compare_funcs[type_a->id](type_a, type_b);
	if (ret == 1) {
		BT_LOGV("Field types differ: ft-a-addr=%p, ft-b-addr=%p",
			type_a, type_b);
	}

end:
	return ret;
}

BT_HIDDEN
int64_t bt_field_type_get_field_count(struct bt_field_type *field_type)
{
	int64_t field_count = -1;
	enum bt_field_type_id type_id = bt_field_type_get_type_id(field_type);

	switch (type_id) {
	case CTF_TYPE_STRUCT:
		field_count =
			bt_field_type_structure_get_field_count(field_type);
		break;
	case CTF_TYPE_VARIANT:
		field_count =
			bt_field_type_variant_get_field_count(field_type);
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
struct bt_field_type *bt_field_type_get_field_at_index(
		struct bt_field_type *field_type, int index)
{
	struct bt_field_type *field = NULL;
	enum bt_field_type_id type_id = bt_field_type_get_type_id(field_type);

	switch (type_id) {
	case CTF_TYPE_STRUCT:
	{
		int ret = bt_field_type_structure_get_field_by_index(
			field_type, NULL, &field, index);
		if (ret) {
			field = NULL;
			goto end;
		}
		break;
	}
	case CTF_TYPE_VARIANT:
	{
		int ret = bt_field_type_variant_get_field_by_index(
			field_type, NULL, &field, index);
		if (ret) {
			field = NULL;
			goto end;
		}
		break;
	}
	case CTF_TYPE_ARRAY:
		field = bt_field_type_array_get_element_type(field_type);
		break;
	case CTF_TYPE_SEQUENCE:
		field = bt_field_type_sequence_get_element_type(field_type);
		break;
	default:
		break;
	}
end:
	return field;
}

BT_HIDDEN
int bt_field_type_get_field_index(struct bt_field_type *field_type,
		const char *name)
{
	int field_index = -1;
	enum bt_field_type_id type_id = bt_field_type_get_type_id(field_type);

	switch (type_id) {
	case CTF_TYPE_STRUCT:
		field_index = bt_field_type_structure_get_field_name_index(
			field_type, name);
		break;
	case CTF_TYPE_VARIANT:
		field_index = bt_field_type_variant_get_field_name_index(
			field_type, name);
		break;
	default:
		break;
	}

	return field_index;
}

struct bt_field_path *bt_field_type_variant_get_tag_field_path(
		struct bt_field_type *type)
{
	struct bt_field_path *field_path = NULL;
	struct bt_field_type_variant *variant;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		goto end;
	}

	if (!bt_field_type_is_variant(type)) {
		BT_LOGW("Invalid parameter: field type is not a variant field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		goto end;
	}

	variant = container_of(type, struct bt_field_type_variant,
			parent);
	field_path = bt_get(variant->tag_field_path);
end:
	return field_path;
}

struct bt_field_path *bt_field_type_sequence_get_length_field_path(
		struct bt_field_type *type)
{
	struct bt_field_path *field_path = NULL;
	struct bt_field_type_sequence *sequence;

	if (!type) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		goto end;
	}

	if (!bt_field_type_is_sequence(type)) {
		BT_LOGW("Invalid parameter: field type is not a sequence field type: "
			"addr=%p, ft-id=%s", type,
			bt_field_type_id_string(type->id));
		goto end;
	}

	sequence = container_of(type, struct bt_field_type_sequence,
			parent);
	field_path = bt_get(sequence->length_field_path);
end:
	return field_path;
}
