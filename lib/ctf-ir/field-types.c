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

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/ctf-ir/field-types-internal.h>
#include <babeltrace/ctf-ir/field-path-internal.h>
#include <babeltrace/ctf-ir/fields-internal.h>
#include <babeltrace/ctf-ir/fields.h>
#include <babeltrace/ctf-ir/utils.h>
#include <babeltrace/ctf-ir/utils-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/ctf-ir/clock-class.h>
#include <babeltrace/ctf-ir/clock-class-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/endian-internal.h>
#include <babeltrace/assert-internal.h>
#include <float.h>
#include <inttypes.h>
#include <stdlib.h>

static
struct bt_field_type *bt_field_type_integer_copy(
		struct bt_field_type *ft);

static
struct bt_field_type *bt_field_type_enumeration_copy_recursive(
		struct bt_field_type *ft);

static
struct bt_field_type *bt_field_type_floating_point_copy(
		struct bt_field_type *ft);

static
struct bt_field_type *bt_field_type_structure_copy_recursive(
		struct bt_field_type *ft);

static
struct bt_field_type *bt_field_type_variant_copy_recursive(
		struct bt_field_type *ft);

static
struct bt_field_type *bt_field_type_array_copy_recursive(
		struct bt_field_type *ft);

static
struct bt_field_type *bt_field_type_sequence_copy_recursive(
		struct bt_field_type *type);

static
struct bt_field_type *bt_field_type_string_copy(struct bt_field_type *ft);

static
int bt_field_type_integer_validate(struct bt_field_type *ft);

static
int bt_field_type_enumeration_validate_recursive(
		struct bt_field_type *ft);
static
int bt_field_type_sequence_validate_recursive(
		struct bt_field_type *ft);
static
int bt_field_type_array_validate_recursive(
		struct bt_field_type *ft);
static
int bt_field_type_structure_validate_recursive(
		struct bt_field_type *ft);
static
int bt_field_type_variant_validate_recursive(
		struct bt_field_type *ft);

static
void bt_field_type_generic_freeze(struct bt_field_type *ft);

static
void bt_field_type_enumeration_freeze_recursive(
		struct bt_field_type *ft);
static
void bt_field_type_structure_freeze_recursive(
		struct bt_field_type *ft);
static
void bt_field_type_variant_freeze_recursive(
		struct bt_field_type *ft);
static
void bt_field_type_array_freeze_recursive(
		struct bt_field_type *ft);
static
void bt_field_type_sequence_freeze_recursive(
		struct bt_field_type *ft);

static
void bt_field_type_integer_set_byte_order(
		struct bt_field_type *ft, enum bt_byte_order byte_order);

static
void bt_field_type_enumeration_set_byte_order_recursive(
		struct bt_field_type *ft, enum bt_byte_order byte_order);

static
void bt_field_type_floating_point_set_byte_order(
		struct bt_field_type *ft, enum bt_byte_order byte_order);

static
void bt_field_type_structure_set_byte_order_recursive(
		struct bt_field_type *ft,
		enum bt_byte_order byte_order);
static
void bt_field_type_variant_set_byte_order_recursive(
		struct bt_field_type *ft,
		enum bt_byte_order byte_order);

static
void bt_field_type_array_set_byte_order_recursive(
		struct bt_field_type *ft,
		enum bt_byte_order byte_order);

static
void bt_field_type_sequence_set_byte_order_recursive(
		struct bt_field_type *ft,
		enum bt_byte_order byte_order);

static
int bt_field_type_integer_compare(struct bt_field_type *ft_a,
		struct bt_field_type *ft_b);

static
int bt_field_type_floating_point_compare(
		struct bt_field_type *ft_a,
		struct bt_field_type *ft_b);

static
int bt_field_type_enumeration_compare_recursive(
		struct bt_field_type *ft_a,
		struct bt_field_type *ft_b);

static
int bt_field_type_string_compare(struct bt_field_type *ft_a,
		struct bt_field_type *ft_b);

static
int bt_field_type_structure_compare_recursive(
		struct bt_field_type *ft_a,
		struct bt_field_type *ft_b);

static
int bt_field_type_variant_compare_recursive(
		struct bt_field_type *ft_a,
		struct bt_field_type *ft_b);

static
int bt_field_type_array_compare_recursive(
		struct bt_field_type *ft_a,
		struct bt_field_type *ft_b);

static
int bt_field_type_sequence_compare_recursive(
		struct bt_field_type *ft_a,
		struct bt_field_type *ft_b);

static struct bt_field_type_methods bt_field_type_integer_methods = {
	.freeze = bt_field_type_generic_freeze,
	.validate = bt_field_type_integer_validate,
	.set_byte_order = bt_field_type_integer_set_byte_order,
	.copy = (bt_field_type_method_copy)
		bt_field_type_integer_copy,
	.compare = bt_field_type_integer_compare,
};

static struct bt_field_type_methods bt_field_type_floating_point_methods = {
	.freeze = bt_field_type_generic_freeze,
	.validate = NULL,
	.set_byte_order = bt_field_type_floating_point_set_byte_order,
	.copy = (bt_field_type_method_copy)
		bt_field_type_floating_point_copy,
	.compare = bt_field_type_floating_point_compare,
};

static struct bt_field_type_methods bt_field_type_enumeration_methods = {
	.freeze = bt_field_type_enumeration_freeze_recursive,
	.validate = bt_field_type_enumeration_validate_recursive,
	.set_byte_order = bt_field_type_enumeration_set_byte_order_recursive,
	.copy = (bt_field_type_method_copy)
		bt_field_type_enumeration_copy_recursive,
	.compare = bt_field_type_enumeration_compare_recursive,
};

static struct bt_field_type_methods bt_field_type_string_methods = {
	.freeze = bt_field_type_generic_freeze,
	.validate = NULL,
	.set_byte_order = NULL,
	.copy = (bt_field_type_method_copy)
		bt_field_type_string_copy,
	.compare = bt_field_type_string_compare,
};

static struct bt_field_type_methods bt_field_type_array_methods = {
	.freeze = bt_field_type_array_freeze_recursive,
	.validate = bt_field_type_array_validate_recursive,
	.set_byte_order = bt_field_type_array_set_byte_order_recursive,
	.copy = (bt_field_type_method_copy)
		bt_field_type_array_copy_recursive,
	.compare = bt_field_type_array_compare_recursive,
};

static struct bt_field_type_methods bt_field_type_sequence_methods = {
	.freeze = bt_field_type_sequence_freeze_recursive,
	.validate = bt_field_type_sequence_validate_recursive,
	.set_byte_order = bt_field_type_sequence_set_byte_order_recursive,
	.copy = (bt_field_type_method_copy)
		bt_field_type_sequence_copy_recursive,
	.compare = bt_field_type_sequence_compare_recursive,
};

static struct bt_field_type_methods bt_field_type_structure_methods = {
	.freeze = bt_field_type_structure_freeze_recursive,
	.validate = bt_field_type_structure_validate_recursive,
	.set_byte_order = bt_field_type_structure_set_byte_order_recursive,
	.copy = (bt_field_type_method_copy)
		bt_field_type_structure_copy_recursive,
	.compare = bt_field_type_structure_compare_recursive,
};

static struct bt_field_type_methods bt_field_type_variant_methods = {
	.freeze = bt_field_type_variant_freeze_recursive,
	.validate = bt_field_type_variant_validate_recursive,
	.set_byte_order = bt_field_type_variant_set_byte_order_recursive,
	.copy = (bt_field_type_method_copy)
		bt_field_type_variant_copy_recursive,
	.compare = bt_field_type_variant_compare_recursive,
};

static
void destroy_enumeration_mapping(struct enumeration_mapping *mapping)
{
	g_free(mapping);
}

static
void bt_field_type_initialize(struct bt_field_type *ft,
		bool init_bo, bt_object_release_func release_func,
		struct bt_field_type_methods *methods)
{
	BT_ASSERT(ft && (ft->id > BT_FIELD_TYPE_ID_UNKNOWN) &&
		(ft->id < BT_FIELD_TYPE_ID_NR));

	bt_object_init_shared(&ft->base, release_func);
	ft->methods = methods;

	if (init_bo) {
		int ret;
		const enum bt_byte_order bo = BT_BYTE_ORDER_NATIVE;

		BT_LOGD("Setting initial field type's byte order: bo=%s",
			bt_common_byte_order_string(bo));
		ret = bt_field_type_set_byte_order(ft, bo);
		BT_ASSERT(ret == 0);
	}

	ft->alignment = 1;
}

static
void bt_field_type_integer_initialize(
		struct bt_field_type *ft,
		unsigned int size, bt_object_release_func release_func,
		struct bt_field_type_methods *methods)
{
	struct bt_field_type_integer *int_ft = (void *) ft;

	BT_ASSERT(size > 0);
	BT_LOGD("Initializing integer field type object: size=%u",
		size);
	ft->id = BT_FIELD_TYPE_ID_INTEGER;
	int_ft->size = size;
	int_ft->base = BT_INTEGER_BASE_DECIMAL;
	int_ft->encoding = BT_STRING_ENCODING_NONE;
	bt_field_type_initialize(ft, true, release_func, methods);
	BT_LOGD("Initialized integer field type object: addr=%p, size=%u",
		ft, size);
}

static
void bt_field_type_floating_point_initialize(
		struct bt_field_type *ft,
		bt_object_release_func release_func,
		struct bt_field_type_methods *methods)
{
	struct bt_field_type_floating_point *flt_ft = (void *) ft;

	BT_LOGD_STR("Initializing floating point number field type object.");
	ft->id = BT_FIELD_TYPE_ID_FLOAT;
	flt_ft->exp_dig = sizeof(float) * CHAR_BIT - FLT_MANT_DIG;
	flt_ft->mant_dig = FLT_MANT_DIG;
	bt_field_type_initialize(ft, true, release_func, methods);
	BT_LOGD("Initialized floating point number field type object: addr=%p, "
		"exp-size=%u, mant-size=%u", ft, flt_ft->exp_dig,
		flt_ft->mant_dig);
}

static
void bt_field_type_enumeration_initialize(
		struct bt_field_type *ft,
		struct bt_field_type *container_ft,
		bt_object_release_func release_func,
		struct bt_field_type_methods *methods)
{
	struct bt_field_type_enumeration *enum_ft = (void *) ft;

	BT_ASSERT(container_ft);
	BT_LOGD("Initializing enumeration field type object: int-ft-addr=%p",
		container_ft);
	ft->id = BT_FIELD_TYPE_ID_ENUM;
	enum_ft->container_ft = bt_get(container_ft);
	enum_ft->entries = g_ptr_array_new_with_free_func(
		(GDestroyNotify) destroy_enumeration_mapping);
	bt_field_type_initialize(ft, false, release_func, methods);
	BT_LOGD("Initialized enumeration field type object: addr=%p, "
		"int-ft-addr=%p, int-ft-size=%u", ft, container_ft,
		bt_field_type_integer_get_size(container_ft));
}

static
void bt_field_type_string_initialize(
		struct bt_field_type *ft,
		bt_object_release_func release_func,
		struct bt_field_type_methods *methods)
{
	struct bt_field_type_string *string_ft = (void *) ft;

	BT_LOGD_STR("Initializing string field type object.");
	ft->id = BT_FIELD_TYPE_ID_STRING;
	bt_field_type_initialize(ft, true, release_func, methods);
	string_ft->encoding = BT_STRING_ENCODING_UTF8;
	ft->alignment = CHAR_BIT;
	BT_LOGD("Initialized string field type object: addr=%p", ft);
}

static
void bt_field_type_structure_initialize(
		struct bt_field_type *ft,
		bt_object_release_func release_func,
		struct bt_field_type_methods *methods)
{
	struct bt_field_type_structure *struct_ft = (void *) ft;

	BT_LOGD_STR("Initializing structure field type object.");
	ft->id = BT_FIELD_TYPE_ID_STRUCT;
	struct_ft->fields = g_array_new(FALSE, TRUE,
		sizeof(struct bt_field_type_structure_field));
	struct_ft->field_name_to_index = g_hash_table_new(NULL, NULL);
	bt_field_type_initialize(ft, true, release_func, methods);
	BT_LOGD("Initialized structure field type object: addr=%p", ft);
}

static
void bt_field_type_array_initialize(
		struct bt_field_type *ft,
		struct bt_field_type *element_ft,
		unsigned int length, bt_object_release_func release_func,
		struct bt_field_type_methods *methods)
{
	struct bt_field_type_array *array_ft = (void *) ft;

	BT_ASSERT(element_ft);
	BT_LOGD("Initializing array field type object: element-ft-addr=%p, "
		"length=%u", element_ft, length);
	ft->id = BT_FIELD_TYPE_ID_ARRAY;
	array_ft->element_ft = bt_get(element_ft);
	array_ft->length = length;
	bt_field_type_initialize(ft, false, release_func, methods);
	BT_LOGD("Initialized array field type object: addr=%p, "
		"element-ft-addr=%p, length=%u", ft, element_ft, length);
}

static
void bt_field_type_sequence_initialize(
		struct bt_field_type *ft,
		struct bt_field_type *element_ft,
		const char *length_field_name,
		bt_object_release_func release_func,
		struct bt_field_type_methods *methods)
{
	struct bt_field_type_sequence *seq_ft = (void *) ft;

	BT_ASSERT(element_ft);
	BT_ASSERT(length_field_name);
	BT_ASSERT(bt_identifier_is_valid(length_field_name));
	BT_LOGD("Initializing sequence field type object: element-ft-addr=%p, "
		"length-field-name=\"%s\"", element_ft, length_field_name);
	ft->id = BT_FIELD_TYPE_ID_SEQUENCE;
	seq_ft->element_ft = bt_get(element_ft);
	seq_ft->length_field_name = g_string_new(length_field_name);
	bt_field_type_initialize(ft, false, release_func, methods);
	BT_LOGD("Initialized sequence field type object: addr=%p, "
		"element-ft-addr=%p, length-field-name=\"%s\"",
		ft, element_ft, length_field_name);
}

static
void bt_field_type_variant_initialize(
		struct bt_field_type *ft,
		struct bt_field_type *tag_ft,
		const char *tag_name,
		bt_object_release_func release_func,
		struct bt_field_type_methods *methods)
{
	struct bt_field_type_variant *var_ft = (void *) ft;

	BT_ASSERT(!tag_name || bt_identifier_is_valid(tag_name));
	BT_LOGD("Initializing variant field type object: "
		"tag-ft-addr=%p, tag-field-name=\"%s\"",
		tag_ft, tag_name);
	ft->id = BT_FIELD_TYPE_ID_VARIANT;
	var_ft->tag_name = g_string_new(tag_name);
	var_ft->choice_name_to_index = g_hash_table_new(NULL, NULL);
	var_ft->choices = g_array_new(FALSE, TRUE,
		sizeof(struct bt_field_type_variant_choice));

	if (tag_ft) {
		var_ft->tag_ft = bt_get(tag_ft);
	}

	bt_field_type_initialize(ft, true, release_func, methods);
	/* A variant's alignment is undefined */
	ft->alignment = 0;
	BT_LOGD("Initialized variant field type object: addr=%p, "
		"tag-ft-addr=%p, tag-field-name=\"%s\"",
		ft, tag_ft, tag_name);
}

static
void bt_field_type_integer_destroy(struct bt_object *obj)
{
	struct bt_field_type_integer *ft = (void *) obj;

	if (!ft) {
		return;
	}

	BT_LOGD("Destroying integer field type object: addr=%p", ft);
	BT_LOGD_STR("Putting mapped clock class.");
	bt_put(ft->mapped_clock_class);
	g_free(ft);
}

static
void bt_field_type_floating_point_destroy(struct bt_object *obj)
{
	struct bt_field_type_floating_point *ft = (void *) obj;

	if (!ft) {
		return;
	}

	BT_LOGD("Destroying floating point number field type object: addr=%p", ft);
	g_free(ft);
}

static
void bt_field_type_enumeration_destroy_recursive(struct bt_object *obj)
{
	struct bt_field_type_enumeration *ft = (void *) obj;

	if (!ft) {
		return;
	}

	BT_LOGD("Destroying enumeration field type object: addr=%p", ft);
	g_ptr_array_free(ft->entries, TRUE);
	BT_LOGD_STR("Putting container field type.");
	bt_put(ft->container_ft);
	g_free(ft);
}

static
void bt_field_type_string_destroy(struct bt_object *obj)
{
	struct bt_field_type_string *ft = (void *) obj;

	if (!ft) {
		return;
	}

	BT_LOGD("Destroying string field type object: addr=%p", ft);
	g_free(ft);
}

static
void bt_field_type_structure_field_finalize(
		struct bt_field_type_structure_field *field)
{
	if (!field) {
		return;
	}

	BT_LOGD("Finalizing structure field type's field: "
		"addr=%p, field-ft-addr=%p, field-name=\"%s\"",
		field, field->type, g_quark_to_string(field->name));
	BT_LOGD_STR("Putting field type.");
	bt_put(field->type);
}

static
void bt_field_type_structure_destroy_recursive(struct bt_object *obj)
{
	struct bt_field_type_structure *ft = (void *) obj;
	uint64_t i;

	if (!ft) {
		return;
	}

	BT_LOGD("Destroying structure field type object: addr=%p", ft);

	if (ft->fields) {
		for (i = 0; i < ft->fields->len; i++) {
			bt_field_type_structure_field_finalize(
				BT_FIELD_TYPE_STRUCTURE_FIELD_AT_INDEX(
					ft, i));
		}

		g_array_free(ft->fields, TRUE);
	}

	if (ft->field_name_to_index) {
		g_hash_table_destroy(ft->field_name_to_index);
	}

	g_free(ft);
}

static
void bt_field_type_array_destroy_recursive(struct bt_object *obj)
{
	struct bt_field_type_array *ft = (void *) obj;

	if (!ft) {
		return;
	}

	BT_LOGD("Destroying array field type object: addr=%p", ft);
	BT_LOGD_STR("Putting element field type.");
	bt_put(ft->element_ft);
	g_free(ft);
}

static
void bt_field_type_sequence_destroy_recursive(struct bt_object *obj)
{
	struct bt_field_type_sequence *ft = (void *) obj;

	if (!ft) {
		return;
	}

	BT_LOGD("Destroying sequence field type object: addr=%p", ft);
	BT_LOGD_STR("Putting element field type.");
	bt_put(ft->element_ft);
	g_string_free(ft->length_field_name, TRUE);
	BT_LOGD_STR("Putting length field path.");
	bt_put(ft->length_field_path);
	g_free(ft);
}

static
void bt_field_type_variant_choice_finalize(
		struct bt_field_type_variant_choice *choice)
{
	if (!choice) {
		return;
	}

	BT_LOGD("Finalizing variant field type's choice: "
		"addr=%p, field-ft-addr=%p, field-name=\"%s\"",
		choice, choice->type, g_quark_to_string(choice->name));
	BT_LOGD_STR("Putting field type.");
	bt_put(choice->type);

	if (choice->ranges) {
		g_array_free(choice->ranges, TRUE);
	}
}

static
void bt_field_type_variant_destroy_recursive(struct bt_object *obj)
{
	struct bt_field_type_variant *ft = (void *) obj;
	uint64_t i;

	if (!ft) {
		return;
	}

	BT_LOGD("Destroying variant field type object: addr=%p", ft);

	if (ft->choices) {
		for (i = 0; i < ft->choices->len; i++) {
			bt_field_type_variant_choice_finalize(
				BT_FIELD_TYPE_VARIANT_CHOICE_AT_INDEX(
					ft, i));
		}

		g_array_free(ft->choices, TRUE);
	}

	if (ft->choice_name_to_index) {
		g_hash_table_destroy(ft->choice_name_to_index);
	}

	if (ft->tag_name) {
		g_string_free(ft->tag_name, TRUE);
	}

	BT_LOGD_STR("Putting tag field type.");
	bt_put(ft->tag_ft);
	BT_LOGD_STR("Putting tag field path.");
	bt_put(ft->tag_field_path);
	g_free(ft);
}

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
int add_structure_variant_member(GArray *members,
		GHashTable *field_name_to_index,
		struct bt_field_type *field_type, const char *field_name,
		bool is_variant)
{
	int ret = 0;
	GQuark name_quark = g_quark_from_string(field_name);
	struct bt_field_type **member_ft;
	GQuark *member_name;

	/* Make sure structure does not contain a field of the same name */
	if (g_hash_table_lookup_extended(field_name_to_index,
			GUINT_TO_POINTER(name_quark), NULL, NULL)) {
		BT_LOGW("Structure or variant field type already contains a field type with this name: "
			"field-name=\"%s\"", field_name);
		ret = -1;
		goto end;
	}

	g_array_set_size(members, members->len + 1);

	if (is_variant) {
		struct bt_field_type_variant_choice *choice =
			&g_array_index(members,
				struct bt_field_type_variant_choice,
				members->len - 1);

		member_ft = &choice->type;
		member_name = &choice->name;
		BT_ASSERT(!choice->ranges);
		choice->ranges = g_array_new(FALSE, TRUE,
			sizeof(struct bt_field_type_variant_choice_range));
		BT_ASSERT(choice->ranges);
	} else {
		struct bt_field_type_structure_field *field =
			&g_array_index(members,
				struct bt_field_type_structure_field,
				members->len - 1);

		member_ft = &field->type;
		member_name = &field->name;
	}

	*member_name = name_quark;
	*member_ft = bt_get(field_type);
	g_hash_table_insert(field_name_to_index,
		GUINT_TO_POINTER(name_quark),
		GUINT_TO_POINTER(members->len - 1));
	BT_LOGV("Added structure/variant field type member: member-ft-addr=%p, "
		"member-name=\"%s\"", field_type, field_name);

end:
	return ret;
}

static
int bt_field_type_integer_validate(struct bt_field_type *ft)
{
	int ret = 0;
	struct bt_field_type_integer *int_ft = (void *) ft;

	if (int_ft->mapped_clock_class && int_ft->is_signed) {
		BT_LOGW("Invalid integer field type: cannot be signed and have a mapped clock class: "
			"ft-addr=%p, clock-class-addr=%p, clock-class-name=\"%s\"",
			ft, int_ft->mapped_clock_class,
			bt_clock_class_get_name(int_ft->mapped_clock_class));
		ret = -1;
		goto end;
	}

end:
	return ret;
}

static
struct enumeration_mapping *bt_field_type_enumeration_get_mapping_by_index(
		struct bt_field_type *ft, uint64_t index)
{
	struct bt_field_type_enumeration *enum_ft = (void *) ft;
	struct enumeration_mapping *mapping = NULL;

	if (index >= enum_ft->entries->len) {
		BT_LOGW("Invalid parameter: index is out of bounds: "
			"addr=%p, index=%" PRIu64 ", count=%u",
			ft, index, enum_ft->entries->len);
		goto end;
	}

	mapping = g_ptr_array_index(enum_ft->entries, index);

end:
	return mapping;
}

/*
 * Note: This algorithm is O(n^2) vs number of enumeration mappings.
 * Only used when freezing an enumeration.
 */
static
void bt_field_type_enumeration_set_range_overlap(
		struct bt_field_type_enumeration *ft)
{
	int64_t i, j, len;
	int is_signed;

	BT_LOGV("Setting enumeration field type's overlap flag: addr=%p",
		ft);
	len = ft->entries->len;
	is_signed = bt_field_type_integer_is_signed((void *) ft->container_ft);

	for (i = 0; i < len; i++) {
		for (j = i + 1; j < len; j++) {
			struct enumeration_mapping *mapping[2];

			mapping[0] = bt_field_type_enumeration_get_mapping_by_index(
				(void *) ft, i);
			mapping[1] = bt_field_type_enumeration_get_mapping_by_index(
				(void *) ft, j);
			if (is_signed) {
				if (mapping[0]->range_start._signed
							<= mapping[1]->range_end._signed
						&& mapping[0]->range_end._signed
							>= mapping[1]->range_start._signed) {
					ft->has_overlapping_ranges = BT_TRUE;
					goto end;
				}
			} else {
				if (mapping[0]->range_start._unsigned
							<= mapping[1]->range_end._unsigned
						&& mapping[0]->range_end._unsigned
							>= mapping[1]->range_start._unsigned) {
					ft->has_overlapping_ranges = BT_TRUE;
					goto end;
				}
			}
		}
	}

end:
	if (ft->has_overlapping_ranges) {
		BT_LOGV_STR("Enumeration field type has overlapping ranges.");
	} else {
		BT_LOGV_STR("Enumeration field type has no overlapping ranges.");
	}
}

static
int bt_field_type_enumeration_validate_recursive(
		struct bt_field_type *ft)
{
	int ret = 0;
	struct bt_field_type_enumeration *enum_ft = (void *) ft;

	ret = bt_field_type_integer_validate((void *) enum_ft->container_ft);
	if (ret) {
		BT_LOGW("Invalid enumeration field type: container type is invalid: "
			"enum-ft-addr=%p, int-ft-addr=%p",
			ft, enum_ft->container_ft);
		goto end;
	}

	/* Ensure enum has entries */
	if (enum_ft->entries->len == 0) {
		BT_LOGW("Invalid enumeration field type: no entries: "
			"addr=%p", ft);
		ret = -1;
		goto end;
	}

end:
	return ret;
}

static
int bt_field_type_sequence_validate_recursive(
		struct bt_field_type *ft)
{
	int ret = 0;
	struct bt_field_type_sequence *seq_ft = (void *) ft;

	/* Length field name should be set at this point */
	if (seq_ft->length_field_name->len == 0) {
		BT_LOGW("Invalid sequence field type: no length field name: "
			"addr=%p", ft);
		ret = -1;
		goto end;
	}

	ret = bt_field_type_validate(seq_ft->element_ft);
	if (ret) {
		BT_LOGW("Invalid sequence field type: invalid element field type: "
			"seq-ft-addr=%p, element-ft-add=%p",
			ft, seq_ft->element_ft);
	}

end:
	return ret;
}

static
int bt_field_type_array_validate_recursive(
		struct bt_field_type *ft)
{
	int ret = 0;
	struct bt_field_type_array *array_ft = (void *) ft;

	ret = bt_field_type_validate(array_ft->element_ft);
	if (ret) {
		BT_LOGW("Invalid array field type: invalid element field type: "
			"array-ft-addr=%p, element-ft-add=%p",
			ft, array_ft->element_ft);
	}

	return ret;
}

static
int bt_field_type_structure_validate_recursive(
		struct bt_field_type *ft)
{
	int ret = 0;
	struct bt_field_type *child_ft = NULL;
	int64_t field_count =
		bt_field_type_structure_get_field_count(ft);
	int64_t i;

	BT_ASSERT(field_count >= 0);

	for (i = 0; i < field_count; ++i) {
		const char *field_name;

		ret = bt_field_type_structure_borrow_field_by_index(ft,
			&field_name, &child_ft, i);
		BT_ASSERT(ret == 0);
		ret = bt_field_type_validate(child_ft);
		if (ret) {
			BT_LOGW("Invalid structure field type: "
				"a contained field type is invalid: "
				"struct-ft-addr=%p, field-ft-addr=%p, "
				"field-name=\"%s\", field-index=%" PRId64,
				ft, child_ft, field_name, i);
			goto end;
		}
	}

end:
	return ret;
}

static
bt_bool bt_field_type_enumeration_has_overlapping_ranges(
		struct bt_field_type_enumeration *enum_ft)
{
	if (!enum_ft->common.frozen) {
		bt_field_type_enumeration_set_range_overlap(enum_ft);
	}

	return enum_ft->has_overlapping_ranges;
}

static
int bt_field_type_variant_validate_recursive(
		struct bt_field_type *ft)
{
	int ret = 0;
	int64_t field_count;
	struct bt_field_type *child_ft = NULL;
	struct bt_field_type_variant *var_ft = (void *) ft;
	int64_t i;

	if (var_ft->tag_name->len == 0) {
		BT_LOGW("Invalid variant field type: no tag field name: "
			"addr=%p", ft);
		ret = -1;
		goto end;
	}

	if (!var_ft->tag_ft) {
		BT_LOGW("Invalid variant field type: no tag field type: "
			"addr=%p, tag-field-name=\"%s\"", var_ft,
			var_ft->tag_name->str);
		ret = -1;
		goto end;
	}

	if (bt_field_type_enumeration_has_overlapping_ranges(var_ft->tag_ft)) {
		BT_LOGW("Invalid variant field type: enumeration tag field type has overlapping ranges: "
			"variant-ft-addr=%p, tag-field-name=\"%s\", "
			"enum-ft-addr=%p", ft, var_ft->tag_name->str,
			var_ft->tag_ft);
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
	field_count = bt_field_type_variant_get_field_count(ft);
	if (field_count < 0) {
		BT_LOGW("Invalid variant field type: no fields: "
			"addr=%p, tag-field-name=\"%s\"",
			ft, var_ft->tag_name->str);
		ret = -1;
		goto end;
	}

	for (i = 0; i < field_count; ++i) {
		const char *field_name;

		ret = bt_field_type_variant_borrow_field_by_index(ft,
			&field_name, &child_ft, i);
		BT_ASSERT(ret == 0);
		ret = bt_field_type_validate(child_ft);
		if (ret) {
			BT_LOGW("Invalid variant field type: "
				"a contained field type is invalid: "
				"variant-ft-addr=%p, tag-field-name=\"%s\", "
				"field-ft-addr=%p, field-name=\"%s\", "
				"field-index=%" PRId64,
				ft, var_ft->tag_name->str, child_ft,
				field_name, i);
			goto end;
		}
	}

end:
	return ret;
}

/*
 * This function validates a given field type without considering
 * where this field type is located. It only validates the properties
 * of the given field type and the properties of its children if
 * applicable.
 */
BT_HIDDEN
int bt_field_type_validate(struct bt_field_type *ft)
{
	int ret = 0;

	BT_ASSERT(ft);

	if (ft->valid) {
		/* Already marked as valid */
		goto end;
	}

	if (ft->methods->validate) {
		ret = ft->methods->validate(ft);
	}

	if (ret == 0 && ft->frozen) {
		/* Field type is valid */
		BT_LOGV("Marking field type as valid: addr=%p", ft);
		ft->valid = 1;
	}

end:
	return ret;
}

struct bt_field_type *bt_field_type_integer_create(unsigned int size)
{
	struct bt_field_type_integer *integer = NULL;

	BT_LOGD("Creating integer field type object: size=%u", size);

	if (size == 0 || size > 64) {
		BT_LOGW("Invalid parameter: size must be between 1 and 64: "
			"size=%u", size);
		goto error;
	}

	integer = g_new0(struct bt_field_type_integer, 1);
	if (!integer) {
		BT_LOGE_STR("Failed to allocate one integer field type.");
		goto error;
	}

	bt_field_type_integer_initialize((void *) integer,
		size, bt_field_type_integer_destroy,
		&bt_field_type_integer_methods);
	BT_LOGD("Created integer field type object: addr=%p, size=%u",
		integer, size);
	goto end;

error:
	BT_PUT(integer);

end:
	return (void *) integer;
}


int bt_field_type_integer_get_size(struct bt_field_type *ft)
{
	struct bt_field_type_integer *int_ft = (void *) ft;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_INTEGER,
		"Field type");
	return (int) int_ft->size;
}

bt_bool bt_field_type_integer_is_signed(struct bt_field_type *ft)
{
	struct bt_field_type_integer *int_ft = (void *) ft;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_INTEGER,
		"Field type");
	return int_ft->is_signed;
}

int bt_field_type_integer_set_is_signed(struct bt_field_type *ft,
		bt_bool is_signed)
{
	int ret = 0;
	struct bt_field_type_integer *int_ft = (void *) ft;

	if (!ft) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (ft->frozen) {
		BT_LOGW("Invalid parameter: field type is frozen: addr=%p",
			ft);
		ret = -1;
		goto end;
	}

	if (ft->id != BT_FIELD_TYPE_ID_INTEGER) {
		BT_LOGW("Invalid parameter: field type is not an integer field type: "
			"addr=%p, ft-id=%s", ft,
			bt_common_field_type_id_string(ft->id));
		ret = -1;
		goto end;
	}

	int_ft->is_signed = !!is_signed;
	BT_LOGV("Set integer field type's signedness: addr=%p, is-signed=%d",
		ft, is_signed);

end:
	return ret;
}

int bt_field_type_integer_set_size(struct bt_field_type *ft,
		unsigned int size)
{
	int ret = 0;
	struct bt_field_type_integer *int_ft = (void *) ft;

	if (!ft) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (ft->frozen) {
		BT_LOGW("Invalid parameter: field type is frozen: addr=%p",
			ft);
		ret = -1;
		goto end;
	}

	if (ft->id != BT_FIELD_TYPE_ID_INTEGER) {
		BT_LOGW("Invalid parameter: field type is not an integer field type: "
			"addr=%p, ft-id=%s", ft,
			bt_common_field_type_id_string(ft->id));
		ret = -1;
		goto end;
	}

	if (size == 0 || size > 64) {
		BT_LOGW("Invalid parameter: size must be between 1 and 64: "
			"addr=%p, size=%u", ft, size);
		ret = -1;
		goto end;
	}

	int_ft->size = size;
	BT_LOGV("Set integer field type's size: addr=%p, size=%u",
		ft, size);

end:
	return ret;
}

enum bt_integer_base bt_field_type_integer_get_base(
		struct bt_field_type *ft)
{
	struct bt_field_type_integer *int_ft = (void *) ft;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_INTEGER,
		"Field type");
	return int_ft->base;
}

int bt_field_type_integer_set_base(struct bt_field_type *ft,
		enum bt_integer_base base)
{
	int ret = 0;
	struct bt_field_type_integer *int_ft = (void *) ft;

	if (!ft) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (ft->frozen) {
		BT_LOGW("Invalid parameter: field type is frozen: addr=%p",
			ft);
		ret = -1;
		goto end;
	}

	if (ft->id != BT_FIELD_TYPE_ID_INTEGER) {
		BT_LOGW("Invalid parameter: field type is not an integer field type: "
			"addr=%p, ft-id=%s", ft,
			bt_common_field_type_id_string(ft->id));
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
		int_ft->base = base;
		break;
	}
	default:
		BT_LOGW("Invalid parameter: unknown integer field type base: "
			"addr=%p, base=%d", ft, base);
		ret = -1;
	}

	BT_LOGV("Set integer field type's base: addr=%p, base=%s",
		ft, bt_common_integer_base_string(base));

end:
	return ret;
}

enum bt_string_encoding bt_field_type_integer_get_encoding(
		struct bt_field_type *ft)
{
	struct bt_field_type_integer *int_ft = (void *) ft;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_INTEGER,
		"Field type");
	return int_ft->encoding;
}

int bt_field_type_integer_set_encoding(struct bt_field_type *ft,
		enum bt_string_encoding encoding)
{
	int ret = 0;
	struct bt_field_type_integer *int_ft = (void *) ft;

	if (!ft) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (ft->frozen) {
		BT_LOGW("Invalid parameter: field type is frozen: addr=%p",
			ft);
		ret = -1;
		goto end;
	}

	if (ft->id != BT_FIELD_TYPE_ID_INTEGER) {
		BT_LOGW("Invalid parameter: field type is not an integer field type: "
			"addr=%p, ft-id=%s", ft,
			bt_common_field_type_id_string(ft->id));
		ret = -1;
		goto end;
	}

	if (encoding != BT_STRING_ENCODING_UTF8 &&
			encoding != BT_STRING_ENCODING_ASCII &&
			encoding != BT_STRING_ENCODING_NONE) {
		BT_LOGW("Invalid parameter: unknown string encoding: "
			"addr=%p, encoding=%d", ft, encoding);
		ret = -1;
		goto end;
	}

	int_ft->encoding = encoding;
	BT_LOGV("Set integer field type's encoding: addr=%p, encoding=%s",
		ft, bt_common_string_encoding_string(encoding));

end:
	return ret;
}

struct bt_clock_class *bt_field_type_integer_borrow_mapped_clock_class(
		struct bt_field_type *ft)
{
	struct bt_field_type_integer *int_ft = (void *) ft;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_INTEGER,
		"Field type");
	return int_ft->mapped_clock_class;
}

static
int bt_field_type_integer_set_mapped_clock_class_no_check_frozen(
		struct bt_field_type *ft,
		struct bt_clock_class *clock_class)
{
	struct bt_field_type_integer *int_ft = (void *) ft;
	int ret = 0;

	if (!clock_class) {
		BT_LOGW_STR("Invalid parameter: clock class is NULL.");
		ret = -1;
		goto end;
	}

	if (ft->id != BT_FIELD_TYPE_ID_INTEGER) {
		BT_LOGW("Invalid parameter: field type is not an integer field type: "
			"addr=%p, ft-id=%s", ft,
			bt_common_field_type_id_string(ft->id));
		goto end;
	}

	if (!bt_clock_class_is_valid(clock_class)) {
		BT_LOGW("Invalid parameter: clock class is invalid: ft-addr=%p"
			"clock-class-addr=%p, clock-class-name=\"%s\"",
			ft, clock_class,
			bt_clock_class_get_name(clock_class));
		ret = -1;
		goto end;
	}

	bt_put(int_ft->mapped_clock_class);
	int_ft->mapped_clock_class = bt_get(clock_class);
	BT_LOGV("Set integer field type's mapped clock class: ft-addr=%p, "
		"clock-class-addr=%p, clock-class-name=\"%s\"",
		ft, clock_class, bt_clock_class_get_name(clock_class));

end:
	return ret;
}

int bt_field_type_integer_set_mapped_clock_class(struct bt_field_type *ft,
		struct bt_clock_class *clock_class)
{
	int ret = 0;

	if (!ft) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (ft->frozen) {
		BT_LOGW("Invalid parameter: field type is frozen: addr=%p",
			ft);
		ret = -1;
		goto end;
	}

	ret = bt_field_type_integer_set_mapped_clock_class_no_check_frozen(
		ft, clock_class);

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
	bt_put(iter->enumeration_ft);
	g_free(iter);
}

static
struct bt_field_type_enumeration_mapping_iterator *
bt_field_type_enumeration_find_mappings_type(
		struct bt_field_type *ft,
		enum bt_field_type_enumeration_mapping_iterator_type iterator_type)
{
	struct bt_field_type_enumeration_mapping_iterator *iter = NULL;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_ENUM,
		"Field type");
	iter = g_new0(struct bt_field_type_enumeration_mapping_iterator, 1);
	if (!iter) {
		BT_LOGE_STR("Failed to allocate one enumeration field type mapping.");
		goto end;
	}

	bt_object_init_shared(&iter->base, bt_field_type_enum_iter_destroy);
	iter->enumeration_ft = bt_get(ft);
	iter->index = -1;
	iter->type = iterator_type;

end:
	return iter;
}

struct bt_field_type_enumeration_mapping_iterator *
bt_field_type_enumeration_find_mappings_by_name(
		struct bt_field_type *ft, const char *name)
{
	struct bt_field_type_enumeration_mapping_iterator *iter;

	iter = bt_field_type_enumeration_find_mappings_type(
			ft, ITERATOR_BY_NAME);
	if (!iter) {
		BT_LOGW("Cannot create enumeration field type mapping iterator: "
			"ft-addr=%p, mapping-name=\"%s\"", ft, name);
		goto error;
	}

	iter->u.name_quark = g_quark_try_string(name);
	if (!iter->u.name_quark) {
		/*
		 * No results are possible, set the iterator's position at the
		 * end.
		 */
		iter->index = iter->enumeration_ft->entries->len;
	}

	return iter;

error:
	bt_put(iter);
	return NULL;
}

int bt_field_type_enumeration_mapping_iterator_next(
		struct bt_field_type_enumeration_mapping_iterator *iter)
{
	struct bt_field_type_enumeration *enum_ft = iter->enumeration_ft;
	int i, ret = 0, len;

	BT_ASSERT_PRE_NON_NULL(iter, "Enumeration field type mapping iterator");
	len = enum_ft->entries->len;
	for (i = iter->index + 1; i < len; i++) {
		struct enumeration_mapping *mapping =
			bt_field_type_enumeration_get_mapping_by_index(
				(void *) enum_ft, i);

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
bt_field_type_enumeration_signed_find_mappings_by_value(
		struct bt_field_type *ft, int64_t value)
{
	struct bt_field_type_enumeration_mapping_iterator *iter;

	iter = bt_field_type_enumeration_find_mappings_type(
			ft, ITERATOR_BY_SIGNED_VALUE);
	if (!iter) {
		BT_LOGW("Cannot create enumeration field type mapping iterator: "
			"ft-addr=%p, value=%" PRId64, ft, value);
		goto error;
	}

	if (bt_field_type_integer_is_signed(
			(void *) iter->enumeration_ft->container_ft) != 1) {
		BT_LOGW("Invalid parameter: enumeration field type is unsigned: "
			"enum-ft-addr=%p, int-ft-addr=%p",
			ft, iter->enumeration_ft->container_ft);
		goto error;
	}

	iter->u.signed_value = value;
	return iter;

error:
	bt_put(iter);
	return NULL;
}

struct bt_field_type_enumeration_mapping_iterator *
bt_field_type_enumeration_unsigned_find_mappings_by_value(
		struct bt_field_type *ft, uint64_t value)
{
	struct bt_field_type_enumeration_mapping_iterator *iter;

	iter = bt_field_type_enumeration_find_mappings_type(
			ft, ITERATOR_BY_UNSIGNED_VALUE);
	if (!iter) {
		BT_LOGW("Cannot create enumeration field type mapping iterator: "
			"ft-addr=%p, value=%" PRIu64, ft, value);
		goto error;
	}

	if (bt_field_type_integer_is_signed(
			(void *) iter->enumeration_ft->container_ft) != 0) {
		BT_LOGW("Invalid parameter: enumeration field type is signed: "
			"enum-ft-addr=%p, int-ft-addr=%p",
			ft, iter->enumeration_ft->container_ft);
		goto error;
	}

	iter->u.unsigned_value = value;
	return iter;

error:
	bt_put(iter);
	return NULL;
}

int bt_field_type_enumeration_mapping_iterator_signed_get(
		struct bt_field_type_enumeration_mapping_iterator *iter,
		const char **mapping_name, int64_t *range_begin,
		int64_t *range_end)
{
	BT_ASSERT_PRE_NON_NULL(iter, "Enumeration field type mapping iterator");
	BT_ASSERT_PRE(iter->index != -1,
		"Invalid enumeration field type mapping iterator access: "
		"addr=%p, position=-1", iter);
	return bt_field_type_enumeration_signed_get_mapping_by_index(
			(void *) iter->enumeration_ft, iter->index,
			mapping_name, range_begin, range_end);
}

int bt_field_type_enumeration_mapping_iterator_unsigned_get(
		struct bt_field_type_enumeration_mapping_iterator *iter,
		const char **mapping_name, uint64_t *range_begin,
		uint64_t *range_end)
{
	BT_ASSERT_PRE_NON_NULL(iter, "Enumeration field type mapping iterator");
	BT_ASSERT_PRE(iter->index != -1,
		"Invalid enumeration field type mapping iterator access: "
		"addr=%p, position=-1", iter);
	return bt_field_type_enumeration_unsigned_get_mapping_by_index(
			(void *) iter->enumeration_ft, iter->index,
			mapping_name, range_begin, range_end);
}

int bt_field_type_enumeration_signed_get_mapping_by_index(
		struct bt_field_type *ft, uint64_t index,
		const char **mapping_name, int64_t *range_begin,
		int64_t *range_end)
{
	int ret = 0;
	struct enumeration_mapping *mapping;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft,
		BT_FIELD_TYPE_ID_ENUM, "Field type");
	mapping = bt_field_type_enumeration_get_mapping_by_index(ft,
		index);
	if (!mapping) {
		/* bt_field_type_enumeration_get_mapping_by_index() logs errors */
		ret = -1;
		goto end;
	}

	if (mapping_name) {
		*mapping_name = g_quark_to_string(mapping->string);
		BT_ASSERT(*mapping_name);
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

int bt_field_type_enumeration_unsigned_get_mapping_by_index(
		struct bt_field_type *ft, uint64_t index,
		const char **mapping_name, uint64_t *range_begin,
		uint64_t *range_end)
{
	int ret = 0;
	struct enumeration_mapping *mapping;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_ENUM, "Field type");
	mapping = bt_field_type_enumeration_get_mapping_by_index(
		ft, index);
	if (!mapping) {
		/* bt_field_type_enumeration_get_mapping_by_index() reports any error */
		ret = -1;
		goto end;
	}

	if (mapping_name) {
		*mapping_name = g_quark_to_string(mapping->string);
		BT_ASSERT(*mapping_name);
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
		struct bt_field_type *container_ft)
{
	struct bt_field_type_enumeration *enumeration = NULL;
	struct bt_field_type *int_ft = (void *) container_ft;

	BT_LOGD("Creating enumeration field type object: int-ft-addr=%p",
		container_ft);

	if (!container_ft) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		goto error;
	}

	if (int_ft->id != BT_FIELD_TYPE_ID_INTEGER) {
		BT_LOGW("Invalid parameter: container field type is not an integer field type: "
			"container-ft-addr=%p, container-ft-id=%s",
			container_ft, bt_common_field_type_id_string(int_ft->id));
		goto error;
	}

	enumeration = g_new0(struct bt_field_type_enumeration, 1);
	if (!enumeration) {
		BT_LOGE_STR("Failed to allocate one enumeration field type.");
		goto error;
	}

	bt_field_type_enumeration_initialize((void *) enumeration,
		int_ft, bt_field_type_enumeration_destroy_recursive,
		&bt_field_type_enumeration_methods);
	BT_LOGD("Created enumeration field type object: addr=%p, "
		"int-ft-addr=%p, int-ft-size=%u",
		enumeration, container_ft,
		bt_field_type_integer_get_size(container_ft));
	goto end;

error:
	BT_PUT(enumeration);

end:
	return (void *) enumeration;
}

struct bt_field_type *bt_field_type_enumeration_borrow_container_field_type(
		struct bt_field_type *ft)
{
	struct bt_field_type_enumeration *enum_ft = (void *) ft;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_ENUM, "Field type");
	return (void *) enum_ft->container_ft;
}

int bt_field_type_enumeration_signed_add_mapping(
		struct bt_field_type *ft, const char *string,
		int64_t range_start, int64_t range_end)
{
	int ret = 0;
	GQuark mapping_name;
	struct enumeration_mapping *mapping;
	struct bt_field_type_enumeration *enum_ft = (void *) ft;
	char *escaped_string;

	if (!ft) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (!string) {
		BT_LOGW_STR("Invalid parameter: string is NULL.");
		ret = -1;
		goto end;
	}

	if (ft->frozen) {
		BT_LOGW("Invalid parameter: field type is frozen: addr=%p",
			ft);
		ret = -1;
		goto end;
	}

	if (ft->id != BT_FIELD_TYPE_ID_ENUM) {
		BT_LOGW("Invalid parameter: field type is not an enumeration field type: "
			"addr=%p, ft-id=%s", ft,
			bt_common_field_type_id_string(ft->id));
		ret = -1;
		goto end;
	}

	if (range_end < range_start) {
		BT_LOGW("Invalid parameter: range's end is lesser than range's start: "
			"addr=%p, range-start=%" PRId64 ", range-end=%" PRId64,
			ft, range_start, range_end);
		ret = -1;
		goto end;
	}

	if (strlen(string) == 0) {
		BT_LOGW("Invalid parameter: mapping name is an empty string: "
			"enum-ft-addr=%p, mapping-name-addr=%p", ft,
			string);
		ret = -1;
		goto end;
	}

	escaped_string = g_strescape(string, NULL);
	if (!escaped_string) {
		BT_LOGE("Cannot escape mapping name: enum-ft-addr=%p, "
			"mapping-name-addr=%p, mapping-name=\"%s\"",
			ft, string, string);
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
	g_ptr_array_add(enum_ft->entries, mapping);
	g_ptr_array_sort(enum_ft->entries,
		(GCompareFunc) compare_enumeration_mappings_signed);
	BT_LOGV("Added mapping to signed enumeration field type: addr=%p, "
		"name=\"%s\", range-start=%" PRId64 ", "
		"range-end=%" PRId64,
		ft, string, range_start, range_end);

error_free:
	free(escaped_string);

end:
	return ret;
}

int bt_field_type_enumeration_unsigned_add_mapping(
		struct bt_field_type *ft, const char *string,
		uint64_t range_start, uint64_t range_end)
{
	int ret = 0;
	GQuark mapping_name;
	struct enumeration_mapping *mapping;
	struct bt_field_type_enumeration *enum_ft = (void *) ft;
	char *escaped_string;

	if (!ft) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (!string) {
		BT_LOGW_STR("Invalid parameter: string is NULL.");
		ret = -1;
		goto end;
	}

	if (ft->frozen) {
		BT_LOGW("Invalid parameter: field type is frozen: addr=%p",
			ft);
		ret = -1;
		goto end;
	}

	if (ft->id != BT_FIELD_TYPE_ID_ENUM) {
		BT_LOGW("Invalid parameter: field type is not an enumeration field type: "
			"addr=%p, ft-id=%s", ft,
			bt_common_field_type_id_string(ft->id));
		ret = -1;
		goto end;
	}

	if (range_end < range_start) {
		BT_LOGW("Invalid parameter: range's end is lesser than range's start: "
			"addr=%p, range-start=%" PRIu64 ", range-end=%" PRIu64,
			ft, range_start, range_end);
		ret = -1;
		goto end;
	}

	if (strlen(string) == 0) {
		BT_LOGW("Invalid parameter: mapping name is an empty string: "
			"enum-ft-addr=%p, mapping-name-addr=%p", ft,
			string);
		ret = -1;
		goto end;
	}

	escaped_string = g_strescape(string, NULL);
	if (!escaped_string) {
		BT_LOGE("Cannot escape mapping name: enum-ft-addr=%p, "
			"mapping-name-addr=%p, mapping-name=\"%s\"",
			ft, string, string);
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
	g_ptr_array_add(enum_ft->entries, mapping);
	g_ptr_array_sort(enum_ft->entries,
		(GCompareFunc) compare_enumeration_mappings_unsigned);
	BT_LOGV("Added mapping to unsigned enumeration field type: addr=%p, "
		"name=\"%s\", range-start=%" PRIu64 ", "
		"range-end=%" PRIu64,
		ft, string, range_start, range_end);

error_free:
	free(escaped_string);

end:
	return ret;
}

int64_t bt_field_type_enumeration_get_mapping_count(
		struct bt_field_type *ft)
{
	struct bt_field_type_enumeration *enum_ft = (void *) ft;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_ENUM, "Field type");
	return (int64_t) enum_ft->entries->len;
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

	bt_field_type_floating_point_initialize(
		(void *) floating_point,
		bt_field_type_floating_point_destroy,
		&bt_field_type_floating_point_methods);
	BT_LOGD("Created floating point number field type object: addr=%p, "
		"exp-size=%u, mant-size=%u", floating_point,
		floating_point->exp_dig, floating_point->mant_dig);

end:
	return (void *) floating_point;
}

int bt_field_type_floating_point_get_exponent_digits(
		struct bt_field_type *ft)
{
	struct bt_field_type_floating_point *flt_ft = (void *) ft;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_FLOAT,
		"Field type");
	return (int) flt_ft->exp_dig;
}

int bt_field_type_floating_point_set_exponent_digits(
		struct bt_field_type *ft, unsigned int exponent_digits)
{
	int ret = 0;
	struct bt_field_type_floating_point *flt_ft = (void *) ft;

	if (!ft) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (ft->frozen) {
		BT_LOGW("Invalid parameter: field type is frozen: addr=%p",
			ft);
		ret = -1;
		goto end;
	}

	if (ft->id != BT_FIELD_TYPE_ID_FLOAT) {
		BT_LOGW("Invalid parameter: field type is not a floating point number field type: "
			"addr=%p, ft-id=%s", ft,
			bt_common_field_type_id_string(ft->id));
		ret = -1;
		goto end;
	}

	if ((exponent_digits != sizeof(float) * CHAR_BIT - FLT_MANT_DIG) &&
		(exponent_digits != sizeof(double) * CHAR_BIT - DBL_MANT_DIG) &&
		(exponent_digits !=
			sizeof(long double) * CHAR_BIT - LDBL_MANT_DIG)) {
		BT_LOGW("Invalid parameter: invalid exponent size: "
			"addr=%p, exp-size=%u", ft, exponent_digits);
		ret = -1;
		goto end;
	}

	flt_ft->exp_dig = exponent_digits;
	BT_LOGV("Set floating point number field type's exponent size: addr=%p, "
		"exp-size=%u", ft, exponent_digits);

end:
	return ret;
}

int bt_field_type_floating_point_get_mantissa_digits(
		struct bt_field_type *ft)
{
	struct bt_field_type_floating_point *flt_ft = (void *) ft;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_FLOAT,
		"Field type");
	return (int) flt_ft->mant_dig;
}

int bt_field_type_floating_point_set_mantissa_digits(
		struct bt_field_type *ft, unsigned int mantissa_digits)
{
	int ret = 0;
	struct bt_field_type_floating_point *flt_ft = (void *) ft;

	if (!ft) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (ft->frozen) {
		BT_LOGW("Invalid parameter: field type is frozen: addr=%p",
			ft);
		ret = -1;
		goto end;
	}

	if (ft->id != BT_FIELD_TYPE_ID_FLOAT) {
		BT_LOGW("Invalid parameter: field type is not a floating point number field type: "
			"addr=%p, ft-id=%s", ft,
			bt_common_field_type_id_string(ft->id));
		ret = -1;
		goto end;
	}

	if ((mantissa_digits != FLT_MANT_DIG) &&
		(mantissa_digits != DBL_MANT_DIG) &&
		(mantissa_digits != LDBL_MANT_DIG)) {
		BT_LOGW("Invalid parameter: invalid mantissa size: "
			"addr=%p, mant-size=%u", ft, mantissa_digits);
		ret = -1;
		goto end;
	}

	flt_ft->mant_dig = mantissa_digits;
	BT_LOGV("Set floating point number field type's mantissa size: addr=%p, "
		"mant-size=%u", ft, mantissa_digits);

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

	bt_field_type_structure_initialize((void *) structure,
		bt_field_type_structure_destroy_recursive,
		&bt_field_type_structure_methods);
	BT_LOGD("Created structure field type object: addr=%p",
		structure);
	goto end;

error:
	BT_PUT(structure);

end:
	return (void *) structure;
}

int bt_field_type_structure_add_field(struct bt_field_type *ft,
		struct bt_field_type *field_type,
		const char *field_name)
{
	int ret = 0;
	struct bt_field_type_structure *struct_ft = (void *) ft;

	/*
	 * TODO: check that `field_type` does not contain `type`,
	 *       recursively.
	 */
	if (!ft) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (!field_name) {
		BT_LOGW_STR("Invalid parameter: field name is NULL.");
		ret = -1;
		goto end;
	}

	if (ft->frozen) {
		BT_LOGW("Invalid parameter: field type is frozen: addr=%p",
			ft);
		ret = -1;
		goto end;
	}

	if (ft->id != BT_FIELD_TYPE_ID_STRUCT) {
		BT_LOGW("Invalid parameter: field type is not a structure field type: "
			"addr=%p, ft-id=%s", ft,
			bt_common_field_type_id_string(ft->id));
		ret = -1;
		goto end;
	}

	if (ft == field_type) {
		BT_LOGW("Invalid parameter: structure field type and field type to add are the same: "
			"addr=%p", ft);
		ret = -1;
		goto end;
	}

	if (add_structure_variant_member(struct_ft->fields,
			struct_ft->field_name_to_index, field_type, field_name,
			false)) {
		BT_LOGW("Cannot add field to structure field type: "
			"struct-ft-addr=%p, field-ft-addr=%p, field-name=\"%s\"",
			ft, field_type, field_name);
		ret = -1;
		goto end;
	}

	BT_LOGV("Added structure field type field: struct-ft-addr=%p, "
		"field-ft-addr=%p, field-name=\"%s\"", ft,
		field_type, field_name);

end:
	return ret;
}

int64_t bt_field_type_structure_get_field_count(struct bt_field_type *ft)
{
	struct bt_field_type_structure *struct_ft = (void *) ft;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_STRUCT,
		"Field type");
	return (int64_t) struct_ft->fields->len;
}

int bt_field_type_structure_borrow_field_by_index(
		struct bt_field_type *ft,
		const char **field_name,
		struct bt_field_type **field_type, uint64_t index)
{
	struct bt_field_type_structure *struct_ft = (void *) ft;
	struct bt_field_type_structure_field *field;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_STRUCT,
		"Field type");
	BT_ASSERT_PRE(index < struct_ft->fields->len,
		"Index is out of bounds: index=%" PRIu64 ", "
		"count=%u, %![ft-]+F",
		index, struct_ft->fields->len, ft);
	field = BT_FIELD_TYPE_STRUCTURE_FIELD_AT_INDEX(struct_ft, index);

	if (field_type) {
		*field_type = field->type;
	}

	if (field_name) {
		*field_name = g_quark_to_string(field->name);
		BT_ASSERT(*field_name);
	}

	return 0;
}

struct bt_field_type *bt_field_type_structure_borrow_field_type_by_name(
		struct bt_field_type *ft, const char *name)
{
	size_t index;
	GQuark name_quark;
	struct bt_field_type_structure_field *field;
	struct bt_field_type_structure *struct_ft = (void *) ft;
	struct bt_field_type *field_type = NULL;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_NON_NULL(name, "Name");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_STRUCT,
		"Field type");
	name_quark = g_quark_try_string(name);
	if (!name_quark) {
		BT_LOGV("No such structure field type field name: "
			"ft-addr=%p, field-name=\"%s\"",
			ft, name);
		goto end;
	}

	if (!g_hash_table_lookup_extended(struct_ft->field_name_to_index,
			GUINT_TO_POINTER(name_quark), NULL, (gpointer *) &index)) {
		BT_LOGV("No such structure field type field name: "
			"ft-addr=%p, field-name=\"%s\"",
			ft, name);
		goto end;
	}

	field = BT_FIELD_TYPE_STRUCTURE_FIELD_AT_INDEX(ft, index);
	field_type = field->type;

end:
	return field_type;
}

struct bt_field_type *bt_field_type_variant_create(
	struct bt_field_type *tag_ft, const char *tag_name)
{
	struct bt_field_type_variant *var_ft = NULL;

	BT_LOGD("Creating variant field type object: "
		"tag-ft-addr=%p, tag-field-name=\"%s\"",
		tag_ft, tag_name);

	if (tag_name && !bt_identifier_is_valid(tag_name)) {
		BT_LOGW("Invalid parameter: tag field name is not a valid CTF identifier: "
			"tag-ft-addr=%p, tag-field-name=\"%s\"",
			tag_ft, tag_name);
		goto error;
	}

	var_ft = g_new0(struct bt_field_type_variant, 1);
	if (!var_ft) {
		BT_LOGE_STR("Failed to allocate one variant field type.");
		goto error;
	}

	bt_field_type_variant_initialize((void *) var_ft,
		(void *) tag_ft, tag_name,
		bt_field_type_variant_destroy_recursive,
		&bt_field_type_variant_methods);
	BT_LOGD("Created variant field type object: addr=%p, "
		"tag-ft-addr=%p, tag-field-name=\"%s\"",
		var_ft, tag_ft, tag_name);
	goto end;

error:
	BT_PUT(var_ft);

end:
	return (void *) var_ft;
}

struct bt_field_type *bt_field_type_variant_borrow_tag_field_type(
		struct bt_field_type *ft)
{
	struct bt_field_type_variant *var_ft = (void *) ft;
	struct bt_field_type *tag_ft = NULL;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_VARIANT,
		"Field type");

	if (!var_ft->tag_ft) {
		BT_LOGV("Variant field type has no tag field type: "
			"addr=%p", ft);
		goto end;
	}

	tag_ft = (void *) var_ft->tag_ft;

end:
	return tag_ft;
}

const char *bt_field_type_variant_get_tag_name(struct bt_field_type *ft)
{
	struct bt_field_type_variant *var_ft = (void *) ft;
	const char *tag_name = NULL;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_VARIANT,
		"Field type");

	if (var_ft->tag_name->len == 0) {
		BT_LOGV("Variant field type has no tag field name: "
			"addr=%p", ft);
		goto end;
	}

	tag_name = var_ft->tag_name->str;

end:
	return tag_name;
}

int bt_field_type_variant_set_tag_name(
		struct bt_field_type *ft, const char *name)
{
	int ret = 0;
	struct bt_field_type_variant *var_ft = (void *) ft;

	if (!ft) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (ft->frozen) {
		BT_LOGW("Invalid parameter: field type is frozen: addr=%p",
			ft);
		ret = -1;
		goto end;
	}

	if (ft->id != BT_FIELD_TYPE_ID_VARIANT) {
		BT_LOGW("Invalid parameter: field type is not a variant field type: "
			"addr=%p, ft-id=%s", ft, bt_common_field_type_id_string(ft->id));
		ret = -1;
		goto end;
	}

	if (!bt_identifier_is_valid(name)) {
		BT_LOGW("Invalid parameter: tag field name is not a valid CTF identifier: "
			"variant-ft-addr=%p, tag-field-name=\"%s\"",
			ft, name);
		ret = -1;
		goto end;
	}

	g_string_assign(var_ft->tag_name, name);
	BT_LOGV("Set variant field type's tag field name: addr=%p, "
		"tag-field-name=\"%s\"", ft, name);

end:
	return ret;
}

int bt_field_type_variant_add_field(struct bt_field_type *ft,
		struct bt_field_type *field_type,
		const char *field_name)
{
	size_t i;
	int ret = 0;
	struct bt_field_type_variant *var_ft = (void *) ft;
	GQuark field_name_quark = g_quark_from_string(field_name);

	/*
	 * TODO: check that `field_type` does not contain `type`,
	 *       recursively.
	 */
	if (!ft) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (ft->frozen) {
		BT_LOGW("Invalid parameter: field type is frozen: addr=%p",
			ft);
		ret = -1;
		goto end;
	}

	if (ft->id != BT_FIELD_TYPE_ID_VARIANT) {
		BT_LOGW("Invalid parameter: field type is not a variant field type: "
			"addr=%p, ft-id=%s", ft,
			bt_common_field_type_id_string(ft->id));
		ret = -1;
		goto end;
	}

	if (ft == field_type) {
		BT_LOGW("Invalid parameter: variant field type and field type to add are the same: "
			"addr=%p", ft);
		ret = -1;
		goto end;
	}

	/* The user has explicitly provided a tag; validate against it. */
	if (var_ft->tag_ft) {
		int name_found = 0;

		/* Make sure this name is present in the enum tag */
		for (i = 0; i < var_ft->tag_ft->entries->len; i++) {
			struct enumeration_mapping *mapping =
				g_ptr_array_index(var_ft->tag_ft->entries, i);

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
				ft, var_ft->tag_ft, var_ft->tag_name->str,
				field_type, field_name);
			ret = -1;
			goto end;
		}
	}

	if (add_structure_variant_member(var_ft->choices,
			var_ft->choice_name_to_index, field_type,
			field_name, true)) {
		BT_LOGW("Cannot add field to variant field type: "
			"variant-ft-addr=%p, field-ft-addr=%p, field-name=\"%s\"",
			ft, field_type, field_name);
		ret = -1;
		goto end;
	}

	BT_LOGV("Added variant field type field: variant-ft-addr=%p, "
		"field-ft-addr=%p, field-name=\"%s\"", ft,
		field_type, field_name);

end:
	return ret;
}

struct bt_field_type *bt_field_type_variant_borrow_field_type_by_name(
		struct bt_field_type *ft,
		const char *field_name)
{
	size_t index;
	GQuark name_quark;
	struct bt_field_type_variant_choice *choice;
	struct bt_field_type_variant *var_ft = (void *) ft;
	struct bt_field_type *field_type = NULL;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_NON_NULL(field_name, "Name");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_VARIANT,
		"Field type");
	name_quark = g_quark_try_string(field_name);
	if (!name_quark) {
		BT_LOGV("No such variant field type field name: "
			"ft-addr=%p, field-name=\"%s\"",
			ft, field_name);
		goto end;
	}

	if (!g_hash_table_lookup_extended(var_ft->choice_name_to_index,
			GUINT_TO_POINTER(name_quark), NULL, (gpointer *) &index)) {
		BT_LOGV("No such variant field type field name: "
			"ft-addr=%p, field-name=\"%s\"",
			ft, field_name);
		goto end;
	}

	choice = BT_FIELD_TYPE_VARIANT_CHOICE_AT_INDEX(ft, index);
	field_type = choice->type;

end:
	return field_type;
}

int64_t bt_field_type_variant_get_field_count(struct bt_field_type *ft)
{
	struct bt_field_type_variant *var_ft = (void *) ft;

	BT_ASSERT_PRE_NON_NULL(ft, "Variant field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_VARIANT,
		"Field type");
	return (int64_t) var_ft->choices->len;
}

int bt_field_type_variant_borrow_field_by_index(struct bt_field_type *ft,
		const char **field_name, struct bt_field_type **field_type,
		uint64_t index)
{
	struct bt_field_type_variant *var_ft = (void *) ft;
	struct bt_field_type_variant_choice *choice;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_VARIANT,
		"Field type");
	BT_ASSERT_PRE(index < var_ft->choices->len,
		"Index is out of bounds: index=%" PRIu64 ", "
		"count=%u, %![ft-]+F",
		index, var_ft->choices->len, ft);
	choice = BT_FIELD_TYPE_VARIANT_CHOICE_AT_INDEX(ft, index);

	if (field_type) {
		*field_type = choice->type;
	}

	if (field_name) {
		*field_name = g_quark_to_string(choice->name);
		BT_ASSERT(*field_name);
	}

	return 0;
}

BT_HIDDEN
int64_t bt_field_type_variant_find_choice_index(
		struct bt_field_type *ft, uint64_t uval,
		bool is_signed)
{
	int64_t ret;
	uint64_t i;
	struct bt_field_type_variant *var_ft = (void *) ft;

	BT_ASSERT(ft);
	BT_ASSERT(ft->id == BT_FIELD_TYPE_ID_VARIANT);

	if (bt_field_type_variant_update_choices(ft)) {
		ret = INT64_C(-1);
		goto end;
	}

	for (i = 0; i < var_ft->choices->len; i++) {
		uint64_t range_i;
		struct bt_field_type_variant_choice *choice =
			BT_FIELD_TYPE_VARIANT_CHOICE_AT_INDEX(
				var_ft, i);

		for (range_i = 0; range_i < choice->ranges->len; range_i++) {
			struct bt_field_type_variant_choice_range *range =
				&g_array_index(
					choice->ranges,
					struct bt_field_type_variant_choice_range,
					range_i);

			if (is_signed) {
				int64_t tag_ival = (int64_t) uval;

				if (tag_ival >= range->lower.i &&
						tag_ival <= range->upper.i) {
					goto found;
				}
			} else {
				if (uval >= range->lower.u &&
						uval <= range->upper.u) {
					goto found;
				}
			}
		}
	}

	/* Range not found */
	ret = INT64_C(-1);
	goto end;

found:
	ret = (int64_t) i;

end:
	return ret;
}

struct bt_field_type *bt_field_type_array_create(
		struct bt_field_type *element_ft, unsigned int length)
{
	struct bt_field_type_array *array = NULL;

	BT_LOGD("Creating array field type object: element-ft-addr=%p, "
		"length=%u", element_ft, length);

	if (!element_ft) {
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

	bt_field_type_array_initialize((void *) array,
		(void *) element_ft, length,
		bt_field_type_array_destroy_recursive,
		&bt_field_type_array_methods);
	BT_LOGD("Created array field type object: addr=%p, "
		"element-ft-addr=%p, length=%u",
		array, element_ft, length);
	goto end;

error:
	BT_PUT(array);

end:
	return (void *) array;
}

struct bt_field_type *bt_field_type_array_borrow_element_field_type(
		struct bt_field_type *ft)
{
	struct bt_field_type_array *array_ft = (void *) ft;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_ARRAY,
		"Field type");
	BT_ASSERT(array_ft && array_ft->element_ft);
	return array_ft->element_ft;
}

int64_t bt_field_type_array_get_length(struct bt_field_type *ft)
{
	struct bt_field_type_array *array_ft = (void *) ft;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_ARRAY,
		"Field type");
	return (int64_t) array_ft->length;
}

struct bt_field_type *bt_field_type_sequence_create(
		struct bt_field_type *element_ft,
		const char *length_field_name)
{
	struct bt_field_type_sequence *sequence = NULL;

	BT_LOGD("Creating sequence field type object: element-ft-addr=%p, "
		"length-field-name=\"%s\"", element_ft, length_field_name);

	if (!element_ft) {
		BT_LOGW_STR("Invalid parameter: element field type is NULL.");
		goto error;
	}

	if (!bt_identifier_is_valid(length_field_name)) {
		BT_LOGW("Invalid parameter: length field name is not a valid CTF identifier: "
			"length-field-name=\"%s\"", length_field_name);
		goto error;
	}

	sequence = g_new0(struct bt_field_type_sequence, 1);
	if (!sequence) {
		BT_LOGE_STR("Failed to allocate one sequence field type.");
		goto error;
	}

	bt_field_type_sequence_initialize((void *) sequence,
		(void *) element_ft, length_field_name,
		bt_field_type_sequence_destroy_recursive,
		&bt_field_type_sequence_methods);
	BT_LOGD("Created sequence field type object: addr=%p, "
		"element-ft-addr=%p, length-field-name=\"%s\"",
		sequence, element_ft, length_field_name);
	goto end;

error:
	BT_PUT(sequence);

end:
	return (void *) sequence;
}

struct bt_field_type *bt_field_type_sequence_borrow_element_field_type(
		struct bt_field_type *ft)
{
	struct bt_field_type_sequence *seq_ft = (void *) ft;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_SEQUENCE,
		"Field type");
	return seq_ft->element_ft;
}

const char *bt_field_type_sequence_get_length_field_name(
		struct bt_field_type *ft)
{
	struct bt_field_type_sequence *seq_ft = (void *) ft;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_SEQUENCE,
		"Field type");
	return seq_ft->length_field_name ?
		seq_ft->length_field_name->str : NULL;
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

	bt_field_type_string_initialize((void *) string,
		bt_field_type_string_destroy,
		&bt_field_type_string_methods);
	BT_LOGD("Created string field type object: addr=%p", string);
	return (void *) string;
}

enum bt_string_encoding bt_field_type_string_get_encoding(
		struct bt_field_type *ft)
{
	struct bt_field_type_string *string_ft = (void *) ft;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_STRING,
		"Field type");
	return string_ft->encoding;
}

int bt_field_type_string_set_encoding(struct bt_field_type *ft,
		enum bt_string_encoding encoding)
{
	int ret = 0;
	struct bt_field_type_string *string_ft = (void *) ft;

	if (!ft) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (ft->id != BT_FIELD_TYPE_ID_STRING) {
		BT_LOGW("Invalid parameter: field type is not a string field type: "
			"addr=%p, ft-id=%s", ft,
			bt_common_field_type_id_string(ft->id));
		ret = -1;
		goto end;
	}

	if (encoding != BT_STRING_ENCODING_UTF8 &&
			encoding != BT_STRING_ENCODING_ASCII) {
		BT_LOGW("Invalid parameter: unknown string encoding: "
			"addr=%p, encoding=%d", ft, encoding);
		ret = -1;
		goto end;
	}

	string_ft->encoding = encoding;
	BT_LOGV("Set string field type's encoding: addr=%p, encoding=%s",
		ft, bt_common_string_encoding_string(encoding));

end:
	return ret;
}

int bt_field_type_get_alignment(struct bt_field_type *ft)
{
	int ret;
	enum bt_field_type_id type_id;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");

	if (ft->frozen) {
		ret = (int) ft->alignment;
		goto end;
	}

	type_id = bt_field_type_get_type_id(ft);
	switch (type_id) {
	case BT_FIELD_TYPE_ID_SEQUENCE:
	{
		struct bt_field_type *element_ft =
			bt_field_type_sequence_borrow_element_field_type(ft);

		BT_ASSERT(element_ft);
		ret = bt_field_type_get_alignment(element_ft);
		break;
	}
	case BT_FIELD_TYPE_ID_ARRAY:
	{
		struct bt_field_type *element_ft =
			bt_field_type_array_borrow_element_field_type(ft);

		BT_ASSERT(element_ft);
		ret = bt_field_type_get_alignment(element_ft);
		break;
	}
	case BT_FIELD_TYPE_ID_STRUCT:
	{
		int64_t i, element_count;

		element_count = bt_field_type_structure_get_field_count(
			ft);
		BT_ASSERT(element_count >= 0);

		for (i = 0; i < element_count; i++) {
			struct bt_field_type *field = NULL;
			int field_alignment;

			ret = bt_field_type_structure_borrow_field_by_index(
				ft, NULL, &field, i);
			BT_ASSERT(ret == 0);
			BT_ASSERT(field);
			field_alignment = bt_field_type_get_alignment(
				field);
			if (field_alignment < 0) {
				ret = field_alignment;
				goto end;
			}

			ft->alignment = MAX(field_alignment, ft->alignment);
		}
		ret = (int) ft->alignment;
		break;
	}
	case BT_FIELD_TYPE_ID_UNKNOWN:
		BT_LOGW("Invalid parameter: unknown field type ID: "
			"addr=%p, ft-id=%d", ft, type_id);
		ret = -1;
		break;
	default:
		ret = (int) ft->alignment;
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

int bt_field_type_set_alignment(struct bt_field_type *ft,
		unsigned int alignment)
{
	int ret = 0;
	enum bt_field_type_id type_id;

	/* Alignment must be a power of two */
	if (!ft) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (ft->frozen) {
		BT_LOGW("Invalid parameter: field type is frozen: addr=%p",
			ft);
		ret = -1;
		goto end;
	}

	if (!is_power_of_two(alignment)) {
		BT_LOGW("Invalid parameter: alignment is not a power of two: "
			"addr=%p, align=%u", ft, alignment);
		ret = -1;
		goto end;
	}

	type_id = bt_field_type_get_type_id(ft);
	if (type_id == BT_FIELD_TYPE_ID_UNKNOWN) {
		BT_LOGW("Invalid parameter: unknown field type ID: "
			"addr=%p, ft-id=%d", ft, type_id);
		ret = -1;
		goto end;
	}

	if (ft->id == BT_FIELD_TYPE_ID_STRING && alignment != CHAR_BIT) {
		BT_LOGW("Invalid parameter: alignment must be %u for a string field type: "
			"addr=%p, align=%u", CHAR_BIT, ft, alignment);
		ret = -1;
		goto end;
	}

	if (type_id == BT_FIELD_TYPE_ID_VARIANT ||
			type_id == BT_FIELD_TYPE_ID_SEQUENCE ||
			type_id == BT_FIELD_TYPE_ID_ARRAY) {
		/* Setting an alignment on these types makes no sense */
		BT_LOGW("Invalid parameter: cannot set the alignment of this field type: "
			"addr=%p, ft-id=%s", ft,
			bt_common_field_type_id_string(ft->id));
		ret = -1;
		goto end;
	}

	ft->alignment = alignment;
	ret = 0;
	BT_LOGV("Set field type's alignment: addr=%p, align=%u",
		ft, alignment);

end:
	return ret;
}

enum bt_byte_order bt_field_type_get_byte_order(struct bt_field_type *ft)
{
	enum bt_byte_order ret = BT_BYTE_ORDER_UNKNOWN;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");

	switch (ft->id) {
	case BT_FIELD_TYPE_ID_INTEGER:
	{
		struct bt_field_type_integer *integer = (void *) ft;

		ret = integer->user_byte_order;
		break;
	}
	case BT_FIELD_TYPE_ID_ENUM:
	{
		struct bt_field_type_enumeration *enum_ft = (void *) ft;

		ret = bt_field_type_get_byte_order(
			(void *) enum_ft->container_ft);
		break;
	}
	case BT_FIELD_TYPE_ID_FLOAT:
	{
		struct bt_field_type_floating_point *floating_point = (void *) ft;
		ret = floating_point->user_byte_order;
		break;
	}
	default:
		BT_LOGW("Invalid parameter: cannot get the byte order of this field type: "
			"addr=%p, ft-id=%s", ft,
			bt_common_field_type_id_string(ft->id));
		goto end;
	}

	BT_ASSERT(ret == BT_BYTE_ORDER_NATIVE ||
		ret == BT_BYTE_ORDER_LITTLE_ENDIAN ||
		ret == BT_BYTE_ORDER_BIG_ENDIAN ||
		ret == BT_BYTE_ORDER_NETWORK);

end:
	return ret;
}

int bt_field_type_set_byte_order(struct bt_field_type *ft,
		enum bt_byte_order byte_order)
{
	int ret = 0;

	if (!ft) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (ft->frozen) {
		BT_LOGW("Invalid parameter: field type is frozen: addr=%p",
			ft);
		ret = -1;
		goto end;
	}

	if (byte_order != BT_BYTE_ORDER_NATIVE &&
			byte_order != BT_BYTE_ORDER_LITTLE_ENDIAN &&
			byte_order != BT_BYTE_ORDER_BIG_ENDIAN &&
			byte_order != BT_BYTE_ORDER_NETWORK) {
		BT_LOGW("Invalid parameter: invalid byte order: "
			"addr=%p, bo=%s", ft,
			bt_common_byte_order_string(byte_order));
		ret = -1;
		goto end;
	}

	if (ft->methods->set_byte_order) {
		ft->methods->set_byte_order(ft, byte_order);
	}

	BT_LOGV("Set field type's byte order: addr=%p, bo=%s",
		ft, bt_common_byte_order_string(byte_order));

end:
	return ret;
}

enum bt_field_type_id bt_field_type_get_type_id(struct bt_field_type *ft)
{
	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	return ft->id;
}

BT_HIDDEN
void bt_field_type_freeze(struct bt_field_type *ft)
{
	if (!ft || ft->frozen) {
		return;
	}

	BT_ASSERT(ft->methods->freeze);
	ft->methods->freeze(ft);
}

struct bt_field_type *bt_field_type_copy(struct bt_field_type *ft)
{
	struct bt_field_type *ft_copy = NULL;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT(ft->methods->copy);
	ft_copy = ft->methods->copy(ft);
	if (!ft_copy) {
		BT_LOGE_STR("Cannot copy field type.");
		goto end;
	}

	ft_copy->alignment = ft->alignment;

end:
	return ft_copy;
}

static inline
int bt_field_type_structure_get_field_name_index(
		struct bt_field_type *ft, const char *name)
{
	int ret;
	size_t index;
	GQuark name_quark;
	struct bt_field_type_structure *struct_ft = (void *) ft;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_NON_NULL(name, "Name");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_STRUCT,
		"Field type");

	name_quark = g_quark_try_string(name);
	if (!name_quark) {
		BT_LOGV("No such structure field type field name: "
			"ft-addr=%p, field-name=\"%s\"",
			ft, name);
		ret = -1;
		goto end;
	}

	if (!g_hash_table_lookup_extended(struct_ft->field_name_to_index,
			GUINT_TO_POINTER(name_quark),
			NULL, (gpointer *) &index)) {
		BT_LOGV("No such structure field type field name: "
			"ft-addr=%p, field-name=\"%s\"",
			ft, name);
		ret = -1;
		goto end;
	}

	ret = (int) index;

end:
	return ret;
}

static inline
int bt_field_type_variant_get_field_name_index(
		struct bt_field_type *ft, const char *name)
{
	int ret;
	size_t index;
	GQuark name_quark;
	struct bt_field_type_variant *var_ft = (void *) ft;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_NON_NULL(name, "Name");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_VARIANT,
		"Field type");
	name_quark = g_quark_try_string(name);
	if (!name_quark) {
		BT_LOGV("No such variant field type field name: "
			"ft-addr=%p, field-name=\"%s\"",
			ft, name);
		ret = -1;
		goto end;
	}

	if (!g_hash_table_lookup_extended(var_ft->choice_name_to_index,
			GUINT_TO_POINTER(name_quark),
			NULL, (gpointer *) &index)) {
		BT_LOGV("No such variant field type field name: "
			"ft-addr=%p, field-name=\"%s\"",
			ft, name);
		ret = -1;
		goto end;
	}

	ret = (int) index;

end:
	return ret;
}

BT_HIDDEN
int bt_field_type_sequence_set_length_field_path(
		struct bt_field_type *ft, struct bt_field_path *path)
{
	int ret = 0;
	struct bt_field_type_sequence *seq_ft = (void *) ft;

	if (!ft) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (ft->id != BT_FIELD_TYPE_ID_SEQUENCE) {
		BT_LOGW("Invalid parameter: field type is not a sequence field type: "
			"addr=%p, ft-id=%s", ft,
			bt_common_field_type_id_string(ft->id));
		ret = -1;
		goto end;
	}

	bt_get(path);
	BT_MOVE(seq_ft->length_field_path, path);
	BT_LOGV("Set sequence field type's length field path: ft-addr=%p, "
		"field-path-addr=%p", ft, path);

end:
	return ret;
}

BT_HIDDEN
int bt_field_type_variant_set_tag_field_path(
		struct bt_field_type *ft,
		struct bt_field_path *path)
{
	int ret = 0;
	struct bt_field_type_variant *var_ft = (void *) ft;

	if (!ft) {
		BT_LOGW_STR("Invalid parameter: field type is NULL.");
		ret = -1;
		goto end;
	}

	if (ft->id != BT_FIELD_TYPE_ID_VARIANT) {
		BT_LOGW("Invalid parameter: field type is not a variant field type: "
			"addr=%p, ft-id=%s", ft,
			bt_common_field_type_id_string(ft->id));
		ret = -1;
		goto end;
	}

	bt_get(path);
	BT_MOVE(var_ft->tag_field_path, path);
	BT_LOGV("Set variant field type's tag field path: ft-addr=%p, "
		"field-path-addr=%p", ft, path);

end:
	return ret;
}

BT_HIDDEN
int bt_field_type_variant_set_tag_field_type(
		struct bt_field_type *ft,
		struct bt_field_type *tag_ft)
{
	int ret = 0;
	struct bt_field_type_variant *var_ft = (void *) ft;

	if (!ft) {
		BT_LOGW_STR("Invalid parameter: variant field type is NULL.");
		ret = -1;
		goto end;
	}

	if (!tag_ft) {
		BT_LOGW_STR("Invalid parameter: tag field type is NULL.");
		ret = -1;
		goto end;
	}

	if (tag_ft->id != BT_FIELD_TYPE_ID_ENUM) {
		BT_LOGW("Invalid parameter: tag field type is not an enumeration field type: "
			"addr=%p, ft-id=%s", tag_ft,
			bt_common_field_type_id_string(tag_ft->id));
		ret = -1;
		goto end;
	}

	bt_put(var_ft->tag_ft);
	var_ft->tag_ft = bt_get(tag_ft);
	BT_LOGV("Set variant field type's tag field type: variant-ft-addr=%p, "
		"tag-ft-addr=%p", ft, tag_ft);

end:
	return ret;
}

static
void bt_field_type_generic_freeze(struct bt_field_type *ft)
{
	ft->frozen = 1;
}

static
void bt_field_type_enumeration_freeze_recursive(
		struct bt_field_type *ft)
{
	struct bt_field_type_enumeration *enum_ft = (void *) ft;

	BT_LOGD("Freezing enumeration field type object: addr=%p", ft);
	bt_field_type_enumeration_set_range_overlap(enum_ft);
	bt_field_type_generic_freeze(ft);
	BT_LOGD("Freezing enumeration field type object's container field type: int-ft-addr=%p",
		enum_ft->container_ft);
	bt_field_type_freeze((void *) enum_ft->container_ft);
}

static
void bt_field_type_structure_freeze_recursive(
		struct bt_field_type *ft)
{
	struct bt_field_type_structure *struct_ft = (void *) ft;
	uint64_t i;

	/* Cache the alignment */
	BT_LOGD("Freezing structure field type object: addr=%p", ft);
	ft->alignment = bt_field_type_get_alignment(ft);
	bt_field_type_generic_freeze(ft);

	for (i = 0; i < struct_ft->fields->len; i++) {
		struct bt_field_type_structure_field *field =
			BT_FIELD_TYPE_STRUCTURE_FIELD_AT_INDEX(ft, i);

		BT_LOGD("Freezing structure field type field: "
			"ft-addr=%p, name=\"%s\"",
			field->type, g_quark_to_string(field->name));
		bt_field_type_freeze(field->type);
	}
}

BT_HIDDEN
int bt_field_type_variant_update_choices(struct bt_field_type *ft)
{
	struct bt_field_type_variant *var_ft = (void *) ft;
	uint64_t i;
	int ret = 0;
	bool is_signed;

	if (ft->frozen && var_ft->choices_up_to_date) {
		goto end;
	}

	BT_ASSERT(var_ft->tag_ft);
	is_signed = !!var_ft->tag_ft->container_ft->is_signed;

	for (i = 0; i < var_ft->choices->len; i++) {
		struct bt_field_type_variant_choice *choice =
			BT_FIELD_TYPE_VARIANT_CHOICE_AT_INDEX(ft, i);
		const char *choice_name = g_quark_to_string(choice->name);
		struct bt_field_type_enumeration_mapping_iterator *iter =
			bt_field_type_enumeration_find_mappings_by_name(
				(void *) var_ft->tag_ft, choice_name);

		if (!iter) {
			ret = -1;
			goto end;
		}

		BT_ASSERT(choice->ranges);
		g_array_set_size(choice->ranges, 0);

		while (bt_field_type_enumeration_mapping_iterator_next(iter) == 0) {
			struct bt_field_type_variant_choice_range range;

			if (is_signed) {
				ret = bt_field_type_enumeration_mapping_iterator_signed_get(
					iter, NULL,
					&range.lower.i, &range.upper.i);
			} else {
				ret = bt_field_type_enumeration_mapping_iterator_unsigned_get(
					iter, NULL,
					&range.lower.u, &range.upper.u);
			}

			BT_ASSERT(ret == 0);
			g_array_append_val(choice->ranges, range);
		}

		bt_put(iter);
	}

	var_ft->choices_up_to_date = true;

end:
	return ret;
}

static
void bt_field_type_variant_freeze_recursive(
		struct bt_field_type *ft)
{
	struct bt_field_type_variant *var_ft = (void *) ft;
	uint64_t i;

	BT_LOGD("Freezing variant field type object: addr=%p", ft);
	bt_field_type_generic_freeze(ft);

	for (i = 0; i < var_ft->choices->len; i++) {
		struct bt_field_type_variant_choice *choice =
			BT_FIELD_TYPE_VARIANT_CHOICE_AT_INDEX(ft, i);

		BT_LOGD("Freezing variant field type member: "
			"ft-addr=%p, name=\"%s\"",
			choice->type, g_quark_to_string(choice->name));
		bt_field_type_freeze(choice->type);
	}
}

static
void bt_field_type_array_freeze_recursive(
		struct bt_field_type *ft)
{
	struct bt_field_type_array *array_ft = (void *) ft;

	/* Cache the alignment */
	BT_LOGD("Freezing array field type object: addr=%p", ft);
	ft->alignment = bt_field_type_get_alignment(ft);
	bt_field_type_generic_freeze(ft);
	BT_LOGD("Freezing array field type object's element field type: element-ft-addr=%p",
		array_ft->element_ft);
	bt_field_type_freeze(array_ft->element_ft);
}

static
void bt_field_type_sequence_freeze_recursive(
		struct bt_field_type *ft)
{
	struct bt_field_type_sequence *seq_ft = (void *) ft;

	/* Cache the alignment */
	BT_LOGD("Freezing sequence field type object: addr=%p", ft);
	ft->alignment = bt_field_type_get_alignment(ft);
	bt_field_type_generic_freeze(ft);
	BT_LOGD("Freezing sequence field type object's element field type: element-ft-addr=%p",
		seq_ft->element_ft);
	bt_field_type_freeze(seq_ft->element_ft);
}

static
void bt_field_type_integer_set_byte_order(
		struct bt_field_type *ft, enum bt_byte_order byte_order)
{
	struct bt_field_type_integer *int_ft = (void *) ft;

	int_ft->user_byte_order = byte_order;
}

static
void bt_field_type_enumeration_set_byte_order_recursive(
		struct bt_field_type *ft, enum bt_byte_order byte_order)
{
	struct bt_field_type_enumeration *enum_ft = (void *) ft;

	bt_field_type_set_byte_order((void *) enum_ft->container_ft,
		byte_order);
}

static
void bt_field_type_floating_point_set_byte_order(
		struct bt_field_type *ft, enum bt_byte_order byte_order)
{
	struct bt_field_type_floating_point *flt_ft = (void *) ft;

	flt_ft->user_byte_order = byte_order;
}

static
void bt_field_type_structure_set_byte_order_recursive(
		struct bt_field_type *ft,
		enum bt_byte_order byte_order)
{
	int i;
	struct bt_field_type_structure *struct_ft = (void *) ft;

	for (i = 0; i < struct_ft->fields->len; i++) {
		struct bt_field_type_structure_field *field =
			BT_FIELD_TYPE_STRUCTURE_FIELD_AT_INDEX(
				struct_ft, i);
		struct bt_field_type *field_type = field->type;

		bt_field_type_set_byte_order(field_type, byte_order);
	}
}

static
void bt_field_type_variant_set_byte_order_recursive(
		struct bt_field_type *ft,
		enum bt_byte_order byte_order)
{
	int i;
	struct bt_field_type_variant *var_ft = (void *) ft;

	for (i = 0; i < var_ft->choices->len; i++) {
		struct bt_field_type_variant_choice *choice =
			BT_FIELD_TYPE_VARIANT_CHOICE_AT_INDEX(
				var_ft, i);
		struct bt_field_type *field_type = choice->type;

		bt_field_type_set_byte_order(field_type, byte_order);
	}
}

static
void bt_field_type_array_set_byte_order_recursive(
		struct bt_field_type *ft,
		enum bt_byte_order byte_order)
{
	struct bt_field_type_array *array_ft = (void *) ft;

	bt_field_type_set_byte_order(array_ft->element_ft, byte_order);
}

static
void bt_field_type_sequence_set_byte_order_recursive(
		struct bt_field_type *ft,
		enum bt_byte_order byte_order)
{
	struct bt_field_type_sequence *seq_ft = (void *) ft;

	bt_field_type_set_byte_order(seq_ft->element_ft, byte_order);
}


static
int bt_field_type_integer_compare(struct bt_field_type *ft_a,
		struct bt_field_type *ft_b)
{
	int ret = 1;
	struct bt_field_type_integer *int_ft_a = (void *) ft_a;
	struct bt_field_type_integer *int_ft_b = (void *) ft_b;

	/* Length */
	if (int_ft_a->size != int_ft_b->size) {
		BT_LOGV("Integer field types differ: different sizes: "
			"ft-a-size=%u, ft-b-size=%u",
			int_ft_a->size, int_ft_b->size);
		goto end;
	}

	/* Byte order */
	if (int_ft_a->user_byte_order != int_ft_b->user_byte_order) {
		BT_LOGV("Integer field types differ: different byte orders: "
			"ft-a-bo=%s, ft-b-bo=%s",
			bt_common_byte_order_string(int_ft_a->user_byte_order),
			bt_common_byte_order_string(int_ft_b->user_byte_order));
		goto end;
	}

	/* Signedness */
	if (int_ft_a->is_signed != int_ft_b->is_signed) {
		BT_LOGV("Integer field types differ: different signedness: "
			"ft-a-is-signed=%d, ft-b-is-signed=%d",
			int_ft_a->is_signed,
			int_ft_b->is_signed);
		goto end;
	}

	/* Base */
	if (int_ft_a->base != int_ft_b->base) {
		BT_LOGV("Integer field types differ: different bases: "
			"ft-a-base=%s, ft-b-base=%s",
			bt_common_integer_base_string(int_ft_a->base),
			bt_common_integer_base_string(int_ft_b->base));
		goto end;
	}

	/* Encoding */
	if (int_ft_a->encoding != int_ft_b->encoding) {
		BT_LOGV("Integer field types differ: different encodings: "
			"ft-a-encoding=%s, ft-b-encoding=%s",
			bt_common_string_encoding_string(int_ft_a->encoding),
			bt_common_string_encoding_string(int_ft_b->encoding));
		goto end;
	}

	/* Mapped clock class */
	if (int_ft_a->mapped_clock_class) {
		if (!int_ft_b->mapped_clock_class) {
			BT_LOGV_STR("Integer field types differ: field type A "
				"has a mapped clock class, but field type B "
				"does not.");
			goto end;
		}

		if (bt_clock_class_compare(int_ft_a->mapped_clock_class,
				int_ft_b->mapped_clock_class) != 0) {
			BT_LOGV_STR("Integer field types differ: different "
				"mapped clock classes.");
		}
	} else {
		if (int_ft_b->mapped_clock_class) {
			BT_LOGV_STR("Integer field types differ: field type A "
				"has no description, but field type B has one.");
			goto end;
		}
	}

	/* Equal */
	ret = 0;

end:
	return ret;
}

static
int bt_field_type_floating_point_compare(
		struct bt_field_type *ft_a,
		struct bt_field_type *ft_b)
{
	int ret = 1;
	struct bt_field_type_floating_point *flt_ft_a = (void *) ft_a;
	struct bt_field_type_floating_point *flt_ft_b = (void *) ft_b;

	/* Byte order */
	if (flt_ft_a->user_byte_order != flt_ft_b->user_byte_order) {
		BT_LOGV("Floating point number field types differ: different byte orders: "
			"ft-a-bo=%s, ft-b-bo=%s",
			bt_common_byte_order_string(flt_ft_a->user_byte_order),
			bt_common_byte_order_string(flt_ft_b->user_byte_order));
		goto end;
	}

	/* Exponent length */
	if (flt_ft_a->exp_dig != flt_ft_b->exp_dig) {
		BT_LOGV("Floating point number field types differ: different exponent sizes: "
			"ft-a-exp-size=%u, ft-b-exp-size=%u",
			flt_ft_a->exp_dig, flt_ft_b->exp_dig);
		goto end;
	}

	/* Mantissa length */
	if (flt_ft_a->mant_dig != flt_ft_b->mant_dig) {
		BT_LOGV("Floating point number field types differ: different mantissa sizes: "
			"ft-a-mant-size=%u, ft-b-mant-size=%u",
			flt_ft_a->mant_dig, flt_ft_b->mant_dig);
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
int bt_field_type_enumeration_compare_recursive(
		struct bt_field_type *ft_a,
		struct bt_field_type *ft_b)
{
	int ret = 1;
	int i;
	struct bt_field_type_enumeration *enum_ft_a = (void *) ft_a;
	struct bt_field_type_enumeration *enum_ft_b = (void *) ft_b;

	/* Container field type */
	ret = bt_field_type_compare((void *) enum_ft_a->container_ft,
		(void *) enum_ft_b->container_ft);
	if (ret) {
		BT_LOGV("Enumeration field types differ: different container field types: "
			"ft-a-container-ft-addr=%p, ft-b-container-ft-addr=%p",
			enum_ft_a->container_ft, enum_ft_b->container_ft);
		goto end;
	}

	ret = 1;

	/* Entries */
	if (enum_ft_a->entries->len != enum_ft_b->entries->len) {
		goto end;
	}

	for (i = 0; i < enum_ft_a->entries->len; ++i) {
		struct enumeration_mapping *mapping_a =
			g_ptr_array_index(enum_ft_a->entries, i);
		struct enumeration_mapping *mapping_b =
			g_ptr_array_index(enum_ft_b->entries, i);

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
int bt_field_type_string_compare(struct bt_field_type *ft_a,
		struct bt_field_type *ft_b)
{
	int ret = 1;
	struct bt_field_type_string *string_ft_a = (void *) ft_a;
	struct bt_field_type_string *string_ft_b = (void *) ft_b;

	/* Encoding */
	if (string_ft_a->encoding != string_ft_b->encoding) {
		BT_LOGV("String field types differ: different encodings: "
			"ft-a-encoding=%s, ft-b-encoding=%s",
			bt_common_string_encoding_string(string_ft_a->encoding),
			bt_common_string_encoding_string(string_ft_b->encoding));
		goto end;
	}

	/* Equal */
	ret = 0;

end:
	return ret;
}

static
int compare_structure_variant_members(
		struct bt_field_type *member_a_ft,
		struct bt_field_type *member_b_ft,
		GQuark member_a_name, GQuark member_b_name)
{
	int ret = 1;

	/* Label */
	if (member_a_name != member_b_name) {
		BT_LOGV("Structure/variant field type fields differ: different names: "
			"field-a-name=%s, field-b-name=%s",
			g_quark_to_string(member_a_name),
			g_quark_to_string(member_b_name));
		goto end;
	}

	/* Type */
	ret = bt_field_type_compare(member_a_ft, member_b_ft);
	if (ret == 1) {
		BT_LOGV("Structure/variant field type fields differ: different field types: "
			"field-name=\"%s\", field-a-ft-addr=%p, field-b-ft-addr=%p",
			g_quark_to_string(member_a_name),
			member_a_ft, member_b_ft);
	}

end:
	return ret;
}

static
int bt_field_type_structure_compare_recursive(
		struct bt_field_type *ft_a,
		struct bt_field_type *ft_b)
{
	int ret = 1;
	int i;
	struct bt_field_type_structure *struct_ft_a = (void *) ft_a;
	struct bt_field_type_structure *struct_ft_b = (void *) ft_b;

	/* Alignment */
	if (bt_field_type_get_alignment(ft_a) !=
			bt_field_type_get_alignment(ft_b)) {
		BT_LOGV("Structure field types differ: different alignments: "
			"ft-a-align=%u, ft-b-align=%u",
			bt_field_type_get_alignment(ft_a),
			bt_field_type_get_alignment(ft_b));
		goto end;
	}

	/* Fields */
	if (struct_ft_a->fields->len != struct_ft_b->fields->len) {
		BT_LOGV("Structure field types differ: different field counts: "
			"ft-a-field-count=%u, ft-b-field-count=%u",
			struct_ft_a->fields->len, struct_ft_b->fields->len);
		goto end;
	}

	for (i = 0; i < struct_ft_a->fields->len; ++i) {
		struct bt_field_type_structure_field *field_a =
			BT_FIELD_TYPE_STRUCTURE_FIELD_AT_INDEX(
				struct_ft_a, i);
		struct bt_field_type_structure_field *field_b =
			BT_FIELD_TYPE_STRUCTURE_FIELD_AT_INDEX(
				struct_ft_b, i);

		ret = compare_structure_variant_members(field_a->type,
			field_b->type, field_a->name, field_b->name);
		if (ret) {
			/* compare_structure_variant_members() logs what differs */
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
int bt_field_type_variant_compare_recursive(
		struct bt_field_type *ft_a,
		struct bt_field_type *ft_b)
{
	int ret = 1;
	int i;
	struct bt_field_type_variant *var_ft_a = (void *) ft_a;
	struct bt_field_type_variant *var_ft_b = (void *) ft_b;

	/* Tag name */
	if (strcmp(var_ft_a->tag_name->str, var_ft_b->tag_name->str)) {
		BT_LOGV("Variant field types differ: different tag field names: "
			"ft-a-tag-field-name=\"%s\", ft-b-tag-field-name=\"%s\"",
			var_ft_a->tag_name->str, var_ft_b->tag_name->str);
		goto end;
	}

	/* Tag type */
	ret = bt_field_type_compare((void *) var_ft_a->tag_ft,
		(void *) var_ft_b->tag_ft);
	if (ret) {
		BT_LOGV("Variant field types differ: different tag field types: "
			"ft-a-tag-ft-addr=%p, ft-b-tag-ft-addr=%p",
			var_ft_a->tag_ft, var_ft_b->tag_ft);
		goto end;
	}

	ret = 1;

	/* Fields */
	if (var_ft_a->choices->len != var_ft_b->choices->len) {
		BT_LOGV("Variant field types differ: different field counts: "
			"ft-a-field-count=%u, ft-b-field-count=%u",
			var_ft_a->choices->len, var_ft_b->choices->len);
		goto end;
	}

	for (i = 0; i < var_ft_a->choices->len; ++i) {
		struct bt_field_type_variant_choice *choice_a =
			BT_FIELD_TYPE_VARIANT_CHOICE_AT_INDEX(
				var_ft_a, i);
		struct bt_field_type_variant_choice *choice_b =
			BT_FIELD_TYPE_VARIANT_CHOICE_AT_INDEX(
				var_ft_b, i);

		ret = compare_structure_variant_members(choice_a->type,
			choice_b->type, choice_a->name, choice_b->name);
		if (ret) {
			/* compare_structure_variant_members() logs what differs */
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
int bt_field_type_array_compare_recursive(
		struct bt_field_type *ft_a,
		struct bt_field_type *ft_b)
{
	int ret = 1;
	struct bt_field_type_array *array_ft_a = (void *) ft_a;
	struct bt_field_type_array *array_ft_b = (void *) ft_b;

	/* Length */
	if (array_ft_a->length != array_ft_b->length) {
		BT_LOGV("Structure field types differ: different lengths: "
			"ft-a-length=%u, ft-b-length=%u",
			array_ft_a->length, array_ft_b->length);
		goto end;
	}

	/* Element type */
	ret = bt_field_type_compare(array_ft_a->element_ft,
		array_ft_b->element_ft);
	if (ret == 1) {
		BT_LOGV("Array field types differ: different element field types: "
			"ft-a-element-ft-addr=%p, ft-b-element-ft-addr=%p",
			array_ft_a->element_ft, array_ft_b->element_ft);
	}

end:
	return ret;
}

static
int bt_field_type_sequence_compare_recursive(
		struct bt_field_type *ft_a,
		struct bt_field_type *ft_b)
{
	int ret = -1;
	struct bt_field_type_sequence *seq_ft_a = (void *) ft_a;
	struct bt_field_type_sequence *seq_ft_b = (void *) ft_b;

	/* Length name */
	if (strcmp(seq_ft_a->length_field_name->str,
			seq_ft_b->length_field_name->str)) {
		BT_LOGV("Sequence field types differ: different length field names: "
			"ft-a-length-field-name=\"%s\", "
			"ft-b-length-field-name=\"%s\"",
			seq_ft_a->length_field_name->str,
			seq_ft_b->length_field_name->str);
		goto end;
	}

	/* Element type */
	ret = bt_field_type_compare(seq_ft_a->element_ft,
			seq_ft_b->element_ft);
	if (ret == 1) {
		BT_LOGV("Sequence field types differ: different element field types: "
			"ft-a-element-ft-addr=%p, ft-b-element-ft-addr=%p",
			seq_ft_a->element_ft, seq_ft_b->element_ft);
	}

end:
	return ret;
}

BT_HIDDEN
int bt_field_type_compare(struct bt_field_type *ft_a,
		struct bt_field_type *ft_b)
{
	int ret = 1;

	BT_ASSERT_PRE_NON_NULL(ft_a, "Field type A");
	BT_ASSERT_PRE_NON_NULL(ft_b, "Field type B");

	if (ft_a == ft_b) {
		/* Same reference: equal (even if both are NULL) */
		ret = 0;
		goto end;
	}

	if (!ft_a) {
		BT_LOGW_STR("Invalid parameter: field type A is NULL.");
		ret = -1;
		goto end;
	}

	if (!ft_b) {
		BT_LOGW_STR("Invalid parameter: field type B is NULL.");
		ret = -1;
		goto end;
	}

	if (ft_a->id != ft_b->id) {
		/* Different type IDs */
		BT_LOGV("Field types differ: different IDs: "
			"ft-a-addr=%p, ft-b-addr=%p, "
			"ft-a-id=%s, ft-b-id=%s",
			ft_a, ft_b,
			bt_common_field_type_id_string(ft_a->id),
			bt_common_field_type_id_string(ft_b->id));
		goto end;
	}

	if (ft_a->id == BT_FIELD_TYPE_ID_UNKNOWN) {
		/* Both have unknown type IDs */
		BT_LOGW_STR("Invalid parameter: field type IDs are unknown.");
		goto end;
	}

	BT_ASSERT(ft_a->methods->compare);
	ret = ft_a->methods->compare(ft_a, ft_b);
	if (ret == 1) {
		BT_LOGV("Field types differ: ft-a-addr=%p, ft-b-addr=%p",
			ft_a, ft_b);
	}

end:
	return ret;
}

BT_HIDDEN
int64_t bt_field_type_get_field_count(struct bt_field_type *ft)
{
	int64_t field_count = -1;

	switch (ft->id) {
	case BT_FIELD_TYPE_ID_STRUCT:
		field_count =
			bt_field_type_structure_get_field_count(ft);
		break;
	case BT_FIELD_TYPE_ID_VARIANT:
		field_count =
			bt_field_type_variant_get_field_count(ft);
		break;
	case BT_FIELD_TYPE_ID_ARRAY:
	case BT_FIELD_TYPE_ID_SEQUENCE:
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
struct bt_field_type *bt_field_type_borrow_field_at_index(
		struct bt_field_type *ft, int index)
{
	struct bt_field_type *field_type = NULL;

	switch (ft->id) {
	case BT_FIELD_TYPE_ID_STRUCT:
	{
		int ret = bt_field_type_structure_borrow_field_by_index(
			ft, NULL, &field_type, index);
		if (ret) {
			field_type = NULL;
			goto end;
		}
		break;
	}
	case BT_FIELD_TYPE_ID_VARIANT:
	{
		int ret = bt_field_type_variant_borrow_field_by_index(
			ft, NULL, &field_type, index);
		if (ret) {
			field_type = NULL;
			goto end;
		}
		break;
	}
	case BT_FIELD_TYPE_ID_ARRAY:
		field_type =
			bt_field_type_array_borrow_element_field_type(ft);
		break;
	case BT_FIELD_TYPE_ID_SEQUENCE:
		field_type =
			bt_field_type_sequence_borrow_element_field_type(ft);
		break;
	default:
		break;
	}

end:
	return field_type;
}

BT_HIDDEN
int bt_field_type_get_field_index(struct bt_field_type *ft,
		const char *name)
{
	int field_index = -1;

	switch (ft->id) {
	case BT_FIELD_TYPE_ID_STRUCT:
		field_index = bt_field_type_structure_get_field_name_index(
			ft, name);
		break;
	case BT_FIELD_TYPE_ID_VARIANT:
		field_index = bt_field_type_variant_get_field_name_index(
			ft, name);
		break;
	default:
		break;
	}

	return field_index;
}

struct bt_field_path *bt_field_type_variant_borrow_tag_field_path(
		struct bt_field_type *ft)
{
	struct bt_field_type_variant *var_ft = (void *) ft;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_VARIANT,
		"Field type");
	return var_ft->tag_field_path;
}

struct bt_field_path *bt_field_type_sequence_borrow_length_field_path(
		struct bt_field_type *ft)
{
	struct bt_field_type_sequence *seq_ft = (void *) ft;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_SEQUENCE,
		"Field type");
	return seq_ft->length_field_path;
}

BT_HIDDEN
int bt_field_type_validate_single_clock_class(
		struct bt_field_type *ft,
		struct bt_clock_class **expected_clock_class)
{
	int ret = 0;

	if (!ft) {
		goto end;
	}

	BT_ASSERT(expected_clock_class);

	switch (ft->id) {
	case BT_FIELD_TYPE_ID_INTEGER:
	{
		struct bt_clock_class *mapped_clock_class =
			bt_field_type_integer_borrow_mapped_clock_class(ft);

		if (!mapped_clock_class) {
			goto end;
		}

		if (!*expected_clock_class) {
			/* Move reference to output parameter */
			*expected_clock_class = bt_get(mapped_clock_class);
			mapped_clock_class = NULL;
			BT_LOGV("Setting expected clock class: "
				"expected-clock-class-addr=%p",
				*expected_clock_class);
		} else {
			if (mapped_clock_class != *expected_clock_class) {
				BT_LOGW("Integer field type is not mapped to "
					"the expected clock class: "
					"mapped-clock-class-addr=%p, "
					"mapped-clock-class-name=\"%s\", "
					"expected-clock-class-addr=%p, "
					"expected-clock-class-name=\"%s\"",
					mapped_clock_class,
					bt_clock_class_get_name(mapped_clock_class),
					*expected_clock_class,
					bt_clock_class_get_name(*expected_clock_class));
				bt_put(mapped_clock_class);
				ret = -1;
				goto end;
			}
		}

		break;
	}
	case BT_FIELD_TYPE_ID_ENUM:
	case BT_FIELD_TYPE_ID_ARRAY:
	case BT_FIELD_TYPE_ID_SEQUENCE:
	{
		struct bt_field_type *sub_ft = NULL;

		switch (ft->id) {
		case BT_FIELD_TYPE_ID_ENUM:
			sub_ft = bt_field_type_enumeration_borrow_container_field_type(
				ft);
			break;
		case BT_FIELD_TYPE_ID_ARRAY:
			sub_ft = bt_field_type_array_borrow_element_field_type(
				ft);
			break;
		case BT_FIELD_TYPE_ID_SEQUENCE:
			sub_ft = bt_field_type_sequence_borrow_element_field_type(
				ft);
			break;
		default:
			BT_LOGF("Unexpected field type ID: id=%d", ft->id);
			abort();
		}

		BT_ASSERT(sub_ft);
		ret = bt_field_type_validate_single_clock_class(sub_ft,
			expected_clock_class);
		break;
	}
	case BT_FIELD_TYPE_ID_STRUCT:
	{
		uint64_t i;
		int64_t count = bt_field_type_structure_get_field_count(
			ft);

		for (i = 0; i < count; i++) {
			const char *name;
			struct bt_field_type *member_type;

			ret = bt_field_type_structure_borrow_field_by_index(
				ft, &name, &member_type, i);
			BT_ASSERT(ret == 0);
			ret = bt_field_type_validate_single_clock_class(
				member_type, expected_clock_class);
			if (ret) {
				BT_LOGW("Structure field type's field's type "
					"is not recursively mapped to the "
					"expected clock class: "
					"field-ft-addr=%p, field-name=\"%s\"",
					member_type, name);
				goto end;
			}
		}
		break;
	}
	case BT_FIELD_TYPE_ID_VARIANT:
	{
		uint64_t i;
		int64_t count = bt_field_type_variant_get_field_count(
			ft);

		for (i = 0; i < count; i++) {
			const char *name;
			struct bt_field_type *member_type;

			ret = bt_field_type_variant_borrow_field_by_index(
				ft, &name, &member_type, i);
			BT_ASSERT(ret == 0);
			ret = bt_field_type_validate_single_clock_class(
				member_type, expected_clock_class);
			if (ret) {
				BT_LOGW("Variant field type's field's type "
					"is not recursively mapped to the "
					"expected clock class: "
					"field-ft-addr=%p, field-name=\"%s\"",
					member_type, name);
				goto end;
			}
		}
		break;
	}
	default:
		break;
	}

end:
	return ret;
}

static
struct bt_field_type *bt_field_type_integer_copy(
		struct bt_field_type *ft)
{
	struct bt_field_type_integer *int_ft = (void *) ft;
	struct bt_field_type_integer *copy_ft;

	BT_LOGD("Copying integer field type's: addr=%p", ft);
	copy_ft = (void *) bt_field_type_integer_create(int_ft->size);
	if (!copy_ft) {
		BT_LOGE_STR("Cannot create integer field type.");
		goto end;
	}

	copy_ft->mapped_clock_class = bt_get(int_ft->mapped_clock_class);
	copy_ft->user_byte_order = int_ft->user_byte_order;
	copy_ft->is_signed = int_ft->is_signed;
	copy_ft->size = int_ft->size;
	copy_ft->base = int_ft->base;
	copy_ft->encoding = int_ft->encoding;
	BT_LOGD("Copied integer field type: original-ft-addr=%p, copy-ft-addr=%p",
		ft, copy_ft);

end:
	return (void *) copy_ft;
}

static
struct bt_field_type *bt_field_type_enumeration_copy_recursive(
		struct bt_field_type *ft)
{
	size_t i;
	struct bt_field_type_enumeration *enum_ft = (void *) ft;
	struct bt_field_type_enumeration *copy_ft = NULL;
	struct bt_field_type_enumeration *container_copy_ft;

	BT_LOGD("Copying enumeration field type's: addr=%p", ft);

	/* Copy the source enumeration's container */
	BT_LOGD_STR("Copying enumeration field type's container field type.");
	container_copy_ft =
		(void *) bt_field_type_copy((void *) enum_ft->container_ft);
	if (!container_copy_ft) {
		BT_LOGE_STR("Cannot copy enumeration field type's container field type.");
		goto end;
	}

	copy_ft = (void *) bt_field_type_enumeration_create(
		(void *) container_copy_ft);
	if (!copy_ft) {
		BT_LOGE_STR("Cannot create enumeration field type.");
		goto end;
	}

	/* Copy all enumaration entries */
	for (i = 0; i < enum_ft->entries->len; i++) {
		struct enumeration_mapping *mapping = g_ptr_array_index(
			enum_ft->entries, i);
		struct enumeration_mapping *copy_mapping = g_new0(
			struct enumeration_mapping, 1);

		if (!copy_mapping) {
			BT_LOGE_STR("Failed to allocate one enumeration mapping.");
			goto error;
		}

		*copy_mapping = *mapping;
		g_ptr_array_add(copy_ft->entries, copy_mapping);
	}

	BT_LOGD("Copied enumeration field type: original-ft-addr=%p, copy-ft-addr=%p",
		ft, copy_ft);

end:
	bt_put(container_copy_ft);
	return (void *) copy_ft;

error:
	bt_put(container_copy_ft);
        BT_PUT(copy_ft);
	return (void *) copy_ft;
}

static
struct bt_field_type *bt_field_type_floating_point_copy(
		struct bt_field_type *ft)
{
	struct bt_field_type_floating_point *flt_ft = (void *) ft;
	struct bt_field_type_floating_point *copy_ft;

	BT_LOGD("Copying floating point number field type's: addr=%p", ft);
	copy_ft = (void *) bt_field_type_floating_point_create();
	if (!copy_ft) {
		BT_LOGE_STR("Cannot create floating point number field type.");
		goto end;
	}

	copy_ft->user_byte_order = flt_ft->user_byte_order;
	copy_ft->exp_dig = flt_ft->exp_dig;
	copy_ft->mant_dig = flt_ft->mant_dig;
	BT_LOGD("Copied floating point number field type: original-ft-addr=%p, copy-ft-addr=%p",
		ft, copy_ft);

end:
	return (void *) copy_ft;
}

static
struct bt_field_type *bt_field_type_structure_copy_recursive(
		struct bt_field_type *ft)
{
	int64_t i;
	GHashTableIter iter;
	gpointer key, value;
	struct bt_field_type_structure *struct_ft = (void *) ft;
	struct bt_field_type_structure *copy_ft;

	BT_LOGD("Copying structure field type's: addr=%p", ft);
	copy_ft = (void *) bt_field_type_structure_create();
	if (!copy_ft) {
		BT_LOGE_STR("Cannot create structure field type.");
		goto end;
	}

	/* Copy field_name_to_index */
	g_hash_table_iter_init(&iter, struct_ft->field_name_to_index);
	while (g_hash_table_iter_next(&iter, &key, &value)) {
		g_hash_table_insert(copy_ft->field_name_to_index,
			key, value);
	}

	g_array_set_size(copy_ft->fields, struct_ft->fields->len);

	for (i = 0; i < struct_ft->fields->len; i++) {
		struct bt_field_type_structure_field *entry, *copy_entry;
		struct bt_field_type *field_ft_copy;

		entry = BT_FIELD_TYPE_STRUCTURE_FIELD_AT_INDEX(
			struct_ft, i);
		copy_entry = BT_FIELD_TYPE_STRUCTURE_FIELD_AT_INDEX(
			copy_ft, i);
		BT_LOGD("Copying structure field type's field: "
			"index=%" PRId64 ", "
			"field-ft-addr=%p, field-name=\"%s\"",
			i, entry, g_quark_to_string(entry->name));

		field_ft_copy = (void *) bt_field_type_copy(
			(void *) entry->type);
		if (!field_ft_copy) {
			BT_LOGE("Cannot copy structure field type's field: "
				"index=%" PRId64 ", "
				"field-ft-addr=%p, field-name=\"%s\"",
				i, entry, g_quark_to_string(entry->name));
			goto error;
		}

		copy_entry->name = entry->name;
		copy_entry->type = field_ft_copy;
	}

	BT_LOGD("Copied structure field type: original-ft-addr=%p, copy-ft-addr=%p",
		ft, copy_ft);

end:
	return (void *) copy_ft;

error:
        BT_PUT(copy_ft);
	return NULL;
}

static
struct bt_field_type *bt_field_type_variant_copy_recursive(
		struct bt_field_type *ft)
{
	int64_t i;
	GHashTableIter iter;
	gpointer key, value;
	struct bt_field_type *tag_ft_copy = NULL;
	struct bt_field_type_variant *var_ft = (void *) ft;
	struct bt_field_type_variant *copy_ft = NULL;

	BT_LOGD("Copying variant field type's: addr=%p", ft);
	if (var_ft->tag_ft) {
		BT_LOGD_STR("Copying variant field type's tag field type.");
		tag_ft_copy = bt_field_type_copy((void *) var_ft->tag_ft);
		if (!tag_ft_copy) {
			BT_LOGE_STR("Cannot copy variant field type's tag field type.");
			goto end;
		}
	}

	copy_ft = (void *) bt_field_type_variant_create(
		(void *) tag_ft_copy,
		var_ft->tag_name->len ? var_ft->tag_name->str : NULL);
	if (!copy_ft) {
		BT_LOGE_STR("Cannot create variant field type.");
		goto end;
	}

	/* Copy field_name_to_index */
	g_hash_table_iter_init(&iter, var_ft->choice_name_to_index);
	while (g_hash_table_iter_next(&iter, &key, &value)) {
		g_hash_table_insert(copy_ft->choice_name_to_index,
			key, value);
	}

	g_array_set_size(copy_ft->choices, var_ft->choices->len);

	for (i = 0; i < var_ft->choices->len; i++) {
		struct bt_field_type_variant_choice *entry, *copy_entry;
		struct bt_field_type *field_ft_copy;
		uint64_t range_i;

		entry = BT_FIELD_TYPE_VARIANT_CHOICE_AT_INDEX(var_ft, i);
		copy_entry = BT_FIELD_TYPE_VARIANT_CHOICE_AT_INDEX(
			copy_ft, i);
		BT_LOGD("Copying variant field type's field: "
			"index=%" PRId64 ", "
			"field-ft-addr=%p, field-name=\"%s\"",
			i, entry, g_quark_to_string(entry->name));

		field_ft_copy = (void *) bt_field_type_copy(
			(void *) entry->type);
		if (!field_ft_copy) {
			BT_LOGE("Cannot copy variant field type's field: "
				"index=%" PRId64 ", "
				"field-ft-addr=%p, field-name=\"%s\"",
				i, entry, g_quark_to_string(entry->name));
			g_free(copy_entry);
			goto error;
		}

		copy_entry->name = entry->name;
		copy_entry->type = field_ft_copy;

		/* Copy ranges */
		copy_entry->ranges = g_array_new(FALSE, TRUE,
			sizeof(struct bt_field_type_variant_choice_range));
		BT_ASSERT(copy_entry->ranges);
		g_array_set_size(copy_entry->ranges, entry->ranges->len);

		for (range_i = 0; range_i < entry->ranges->len; range_i++) {
			copy_entry->ranges[range_i] = entry->ranges[range_i];
		}
	}

	if (var_ft->tag_field_path) {
		BT_LOGD_STR("Copying variant field type's tag field path.");
		copy_ft->tag_field_path = bt_field_path_copy(
			var_ft->tag_field_path);
		if (!copy_ft->tag_field_path) {
			BT_LOGE_STR("Cannot copy variant field type's tag field path.");
			goto error;
		}
	}

	copy_ft->choices_up_to_date = var_ft->choices_up_to_date;
	BT_LOGD("Copied variant field type: original-ft-addr=%p, copy-ft-addr=%p",
		ft, copy_ft);

end:
	bt_put(tag_ft_copy);
	return (void *) copy_ft;

error:
	bt_put(tag_ft_copy);
        BT_PUT(copy_ft);
	return NULL;
}

static
struct bt_field_type *bt_field_type_array_copy_recursive(
		struct bt_field_type *ft)
{
	struct bt_field_type *container_ft_copy = NULL;
	struct bt_field_type_array *array_ft = (void *) ft;
	struct bt_field_type_array *copy_ft = NULL;

	BT_LOGD("Copying array field type's: addr=%p", ft);
	BT_LOGD_STR("Copying array field type's element field type.");
	container_ft_copy = bt_field_type_copy(array_ft->element_ft);
	if (!container_ft_copy) {
		BT_LOGE_STR("Cannot copy array field type's element field type.");
		goto end;
	}

	copy_ft = (void *) bt_field_type_array_create(
		(void *) container_ft_copy, array_ft->length);
	if (!copy_ft) {
		BT_LOGE_STR("Cannot create array field type.");
		goto end;
	}

	BT_LOGD("Copied array field type: original-ft-addr=%p, copy-ft-addr=%p",
		ft, copy_ft);

end:
	bt_put(container_ft_copy);
	return (void *) copy_ft;
}

static
struct bt_field_type *bt_field_type_sequence_copy_recursive(
		struct bt_field_type *ft)
{
	struct bt_field_type *container_ft_copy = NULL;
	struct bt_field_type_sequence *seq_ft = (void *) ft;
	struct bt_field_type_sequence *copy_ft = NULL;

	BT_LOGD("Copying sequence field type's: addr=%p", ft);
	BT_LOGD_STR("Copying sequence field type's element field type.");
	container_ft_copy = bt_field_type_copy(seq_ft->element_ft);
	if (!container_ft_copy) {
		BT_LOGE_STR("Cannot copy sequence field type's element field type.");
		goto end;
	}

	copy_ft = (void *) bt_field_type_sequence_create(
		(void *) container_ft_copy,
		seq_ft->length_field_name->len ?
			seq_ft->length_field_name->str : NULL);
	if (!copy_ft) {
		BT_LOGE_STR("Cannot create sequence field type.");
		goto end;
	}

	if (seq_ft->length_field_path) {
		BT_LOGD_STR("Copying sequence field type's length field path.");
		copy_ft->length_field_path = bt_field_path_copy(
			seq_ft->length_field_path);
		if (!copy_ft->length_field_path) {
			BT_LOGE_STR("Cannot copy sequence field type's length field path.");
			goto error;
		}
	}

	BT_LOGD("Copied sequence field type: original-ft-addr=%p, copy-ft-addr=%p",
		ft, copy_ft);

end:
	bt_put(container_ft_copy);
	return (void *) copy_ft;
error:
	bt_put(container_ft_copy);
	BT_PUT(copy_ft);
	return NULL;
}

static
struct bt_field_type *bt_field_type_string_copy(struct bt_field_type *ft)
{
	struct bt_field_type_string *string_ft = (void *) ft;
	struct bt_field_type_string *copy_ft = NULL;

	BT_LOGD("Copying string field type's: addr=%p", ft);
	copy_ft = (void *) bt_field_type_string_create();
	if (!copy_ft) {
		BT_LOGE_STR("Cannot create string field type.");
		goto end;
	}

	copy_ft->encoding = string_ft->encoding;
	BT_LOGD("Copied string field type: original-ft-addr=%p, copy-ft-addr=%p",
		ft, copy_ft);

end:
	return (void *) copy_ft;
}
