/*
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

#define BT_LOG_TAG "FIELD-CLASSES"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/trace-ir/private-field-classes.h>
#include <babeltrace/trace-ir/field-classes-internal.h>
#include <babeltrace/trace-ir/field-path-internal.h>
#include <babeltrace/trace-ir/fields-internal.h>
#include <babeltrace/trace-ir/private-fields.h>
#include <babeltrace/trace-ir/fields.h>
#include <babeltrace/trace-ir/utils-internal.h>
#include <babeltrace/object.h>
#include <babeltrace/trace-ir/clock-class.h>
#include <babeltrace/trace-ir/clock-class-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/object.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/endian-internal.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/compat/glib-internal.h>
#include <float.h>
#include <inttypes.h>
#include <stdlib.h>

enum bt_field_class_type bt_field_class_get_type(struct bt_field_class *fc)
{
	BT_ASSERT_PRE_NON_NULL(fc, "Field class");
	return fc->type;
}

static
void init_field_class(struct bt_field_class *fc, enum bt_field_class_type type,
		bt_object_release_func release_func)
{
	BT_ASSERT(fc);
	BT_ASSERT(bt_field_class_has_known_type(fc));
	BT_ASSERT(release_func);
	bt_object_init_shared(&fc->base, release_func);
	fc->type = type;
}

static
void init_integer_field_class(struct bt_field_class_integer *fc,
		enum bt_field_class_type type,
		bt_object_release_func release_func)
{
	init_field_class((void *) fc, type, release_func);
	fc->range = 64;
	fc->base = BT_FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL;
}

static
void destroy_integer_field_class(struct bt_object *obj)
{
	BT_ASSERT(obj);
	BT_LIB_LOGD("Destroying integer field class object: %!+F", obj);
	g_free(obj);
}

static inline
struct bt_field_class *create_integer_field_class(enum bt_field_class_type type)
{
	struct bt_field_class_integer *int_fc = NULL;

	BT_LOGD("Creating default integer field class object: type=%s",
		bt_common_field_class_type_string(type));
	int_fc = g_new0(struct bt_field_class_integer, 1);
	if (!int_fc) {
		BT_LOGE_STR("Failed to allocate one integer field class.");
		goto error;
	}

	init_integer_field_class(int_fc, type, destroy_integer_field_class);
	BT_LIB_LOGD("Created integer field class object: %!+F", int_fc);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(int_fc);

end:
	return (void *) int_fc;
}

struct bt_private_field_class *
bt_private_field_class_unsigned_integer_create(void)
{
	return (void *) create_integer_field_class(
		BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER);
}

struct bt_private_field_class *bt_private_field_class_signed_integer_create(void)
{
	return (void *) create_integer_field_class(
		BT_FIELD_CLASS_TYPE_SIGNED_INTEGER);
}

uint64_t bt_field_class_integer_get_field_value_range(
		struct bt_field_class *fc)
{
	struct bt_field_class_integer *int_fc = (void *) fc;

	BT_ASSERT_PRE_NON_NULL(fc, "Field class");
	BT_ASSERT_PRE_FC_IS_INT(fc, "Field class");
	return int_fc->range;
}

BT_ASSERT_PRE_FUNC
static
bool size_is_valid_for_enumeration_field_class(struct bt_field_class *fc,
		uint64_t size)
{
	// TODO
	return true;
}

int bt_private_field_class_integer_set_field_value_range(
		struct bt_private_field_class *priv_fc, uint64_t size)
{
	struct bt_field_class *fc = (void *) priv_fc;
	struct bt_field_class_integer *int_fc = (void *) fc;

	BT_ASSERT_PRE_NON_NULL(fc, "Field class");
	BT_ASSERT_PRE_FC_IS_INT(fc, "Field class");
	BT_ASSERT_PRE_FC_HOT(fc, "Field class");
	BT_ASSERT_PRE(size <= 64,
		"Unsupported size for integer field class's field value range "
		"(maximum is 64): size=%" PRIu64, size);
	BT_ASSERT_PRE(
		int_fc->common.type == BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER ||
		int_fc->common.type == BT_FIELD_CLASS_TYPE_SIGNED_INTEGER ||
		size_is_valid_for_enumeration_field_class(fc, size),
		"Invalid field value range for enumeration field class: "
		"at least one of the current mapping ranges contains values "
		"which are outside this range: %!+F, size=%" PRIu64, fc, size);
	int_fc->range = size;
	BT_LIB_LOGV("Set integer field class's field value range: %!+F", fc);
	return 0;
}

enum bt_field_class_integer_preferred_display_base
bt_field_class_integer_get_preferred_display_base(struct bt_field_class *fc)
{
	struct bt_field_class_integer *int_fc = (void *) fc;

	BT_ASSERT_PRE_NON_NULL(fc, "Field class");
	BT_ASSERT_PRE_FC_IS_INT(fc, "Field class");
	return int_fc->base;
}

int bt_private_field_class_integer_set_preferred_display_base(
		struct bt_private_field_class *priv_fc,
		enum bt_field_class_integer_preferred_display_base base)
{
	struct bt_field_class *fc = (void *) priv_fc;
	struct bt_field_class_integer *int_fc = (void *) fc;

	BT_ASSERT_PRE_NON_NULL(fc, "Field class");
	BT_ASSERT_PRE_FC_IS_INT(fc, "Field class");
	BT_ASSERT_PRE_FC_HOT(fc, "Field class");
	int_fc->base = base;
	BT_LIB_LOGV("Set integer field class's preferred display base: %!+F", fc);
	return 0;
}

static
void finalize_enumeration_field_class_mapping(
		struct bt_field_class_enumeration_mapping *mapping)
{
	BT_ASSERT(mapping);

	if (mapping->label) {
		g_string_free(mapping->label, TRUE);
	}

	if (mapping->ranges) {
		g_array_free(mapping->ranges, TRUE);
	}
}

static
void destroy_enumeration_field_class(struct bt_object *obj)
{
	struct bt_field_class_enumeration *fc = (void *) obj;

	BT_ASSERT(fc);
	BT_LIB_LOGD("Destroying enumeration field class object: %!+F", fc);

	if (fc->mappings) {
		uint64_t i;

		for (i = 0; i < fc->mappings->len; i++) {
			finalize_enumeration_field_class_mapping(
				BT_FIELD_CLASS_ENUM_MAPPING_AT_INDEX(fc, i));
		}

		g_array_free(fc->mappings, TRUE);
	}

	if (fc->label_buf) {
		g_ptr_array_free(fc->label_buf, TRUE);
	}

	g_free(fc);
}

static
struct bt_field_class *create_enumeration_field_class(enum bt_field_class_type type)
{
	struct bt_field_class_enumeration *enum_fc = NULL;

	BT_LOGD("Creating default enumeration field class object: type=%s",
		bt_common_field_class_type_string(type));
	enum_fc = g_new0(struct bt_field_class_enumeration, 1);
	if (!enum_fc) {
		BT_LOGE_STR("Failed to allocate one enumeration field class.");
		goto error;
	}

	init_integer_field_class((void *) enum_fc, type,
		destroy_enumeration_field_class);
	enum_fc->mappings = g_array_new(FALSE, TRUE,
		sizeof(struct bt_field_class_enumeration_mapping));
	if (!enum_fc->mappings) {
		BT_LOGE_STR("Failed to allocate a GArray.");
		goto error;
	}

	enum_fc->label_buf = g_ptr_array_new();
	if (!enum_fc->label_buf) {
		BT_LOGE_STR("Failed to allocate a GArray.");
		goto error;
	}

	BT_LIB_LOGD("Created enumeration field class object: %!+F", enum_fc);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(enum_fc);

end:
	return (void *) enum_fc;
}

struct bt_private_field_class *
bt_private_field_class_unsigned_enumeration_create(void)
{
	return (void *) create_enumeration_field_class(
		BT_FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION);
}

struct bt_private_field_class *
bt_private_field_class_signed_enumeration_create(void)
{
	return (void *) create_enumeration_field_class(
		BT_FIELD_CLASS_TYPE_SIGNED_ENUMERATION);
}

uint64_t bt_field_class_enumeration_get_mapping_count(struct bt_field_class *fc)
{
	struct bt_field_class_enumeration *enum_fc = (void *) fc;

	BT_ASSERT_PRE_NON_NULL(fc, "Field class");
	BT_ASSERT_PRE_FC_IS_ENUM(fc, "Field class");
	return (uint64_t) enum_fc->mappings->len;
}

void bt_field_class_unsigned_enumeration_borrow_mapping_by_index(
		struct bt_field_class *fc, uint64_t index,
		const char **name,
		struct bt_field_class_unsigned_enumeration_mapping_ranges **ranges)
{
	struct bt_field_class_enumeration *enum_fc = (void *) fc;
	struct bt_field_class_enumeration_mapping *mapping;

	BT_ASSERT_PRE_NON_NULL(fc, "Field class");
	BT_ASSERT_PRE_NON_NULL(name, "Name (output)");
	BT_ASSERT_PRE_NON_NULL(ranges, "Ranges (output)");
	BT_ASSERT_PRE_VALID_INDEX(index, enum_fc->mappings->len);
	BT_ASSERT_PRE_FC_HAS_ID(fc, BT_FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION,
		"Field class");
	mapping = BT_FIELD_CLASS_ENUM_MAPPING_AT_INDEX(fc, index);
	*name = mapping->label->str;
	*ranges = (void *) mapping;
}

void bt_field_class_signed_enumeration_borrow_mapping_by_index(
		struct bt_field_class *fc, uint64_t index,
		const char **name,
		struct bt_field_class_signed_enumeration_mapping_ranges **ranges)
{
	struct bt_field_class_enumeration *enum_fc = (void *) fc;
	struct bt_field_class_enumeration_mapping *mapping;

	BT_ASSERT_PRE_NON_NULL(fc, "Field class");
	BT_ASSERT_PRE_NON_NULL(name, "Name (output)");
	BT_ASSERT_PRE_NON_NULL(ranges, "Ranges (output)");
	BT_ASSERT_PRE_VALID_INDEX(index, enum_fc->mappings->len);
	BT_ASSERT_PRE_FC_HAS_ID(fc, BT_FIELD_CLASS_TYPE_SIGNED_ENUMERATION,
		"Field class");
	mapping = BT_FIELD_CLASS_ENUM_MAPPING_AT_INDEX(fc, index);
	*name = mapping->label->str;
	*ranges = (void *) mapping;
}

static inline
uint64_t get_enumeration_field_class_mapping_range_count(
		struct bt_field_class_enumeration_mapping *mapping)
{
	BT_ASSERT_PRE_NON_NULL(mapping, "Ranges");
	return (uint64_t) mapping->ranges->len;
}

uint64_t bt_field_class_unsigned_enumeration_mapping_ranges_get_range_count(
		struct bt_field_class_unsigned_enumeration_mapping_ranges *ranges)
{
	return get_enumeration_field_class_mapping_range_count((void *) ranges);
}

uint64_t bt_field_class_signed_enumeration_mapping_ranges_get_range_count(
		struct bt_field_class_signed_enumeration_mapping_ranges *ranges)
{
	return get_enumeration_field_class_mapping_range_count((void *) ranges);
}

static inline
void get_enumeration_field_class_mapping_range_at_index(
		struct bt_field_class_enumeration_mapping *mapping,
		uint64_t index, uint64_t *lower, uint64_t *upper)
{
	struct bt_field_class_enumeration_mapping_range *range;

	BT_ASSERT_PRE_NON_NULL(mapping, "Ranges");
	BT_ASSERT_PRE_NON_NULL(lower, "Range's lower (output)");
	BT_ASSERT_PRE_NON_NULL(upper, "Range's upper (output)");
	BT_ASSERT_PRE_VALID_INDEX(index, mapping->ranges->len);
	range = BT_FIELD_CLASS_ENUM_MAPPING_RANGE_AT_INDEX(mapping, index);
	*lower = range->lower.u;
	*upper = range->upper.u;
}

void bt_field_class_unsigned_enumeration_mapping_ranges_get_range_by_index(
		struct bt_field_class_unsigned_enumeration_mapping_ranges *ranges,
		uint64_t index, uint64_t *lower, uint64_t *upper)
{
	get_enumeration_field_class_mapping_range_at_index((void *) ranges,
		index, lower, upper);
}

void bt_field_class_signed_enumeration_mapping_ranges_get_range_by_index(
		struct bt_field_class_unsigned_enumeration_mapping_ranges *ranges,
		uint64_t index, int64_t *lower, int64_t *upper)
{
	get_enumeration_field_class_mapping_range_at_index((void *) ranges,
		index, (uint64_t *) lower, (uint64_t *) upper);
}



int bt_field_class_unsigned_enumeration_get_mapping_labels_by_value(
		struct bt_field_class *fc, uint64_t value,
		bt_field_class_enumeration_mapping_label_array *label_array,
		uint64_t *count)
{
	struct bt_field_class_enumeration *enum_fc = (void *) fc;
	uint64_t i;

	BT_ASSERT_PRE_NON_NULL(fc, "Field class");
	BT_ASSERT_PRE_NON_NULL(label_array, "Label array (output)");
	BT_ASSERT_PRE_NON_NULL(count, "Count (output)");
	BT_ASSERT_PRE_FC_HAS_ID(fc, BT_FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION,
		"Field class");
	g_ptr_array_set_size(enum_fc->label_buf, 0);

	for (i = 0; i < enum_fc->mappings->len; i++) {
		uint64_t j;
		struct bt_field_class_enumeration_mapping *mapping =
			BT_FIELD_CLASS_ENUM_MAPPING_AT_INDEX(enum_fc, i);

		for (j = 0; j < mapping->ranges->len; j++) {
			struct bt_field_class_enumeration_mapping_range *range =
				BT_FIELD_CLASS_ENUM_MAPPING_RANGE_AT_INDEX(
					mapping, j);

			if (value >= range->lower.u &&
					value <= range->upper.u) {
				g_ptr_array_add(enum_fc->label_buf,
					mapping->label->str);
				break;
			}
		}
	}

	*label_array = (void *) enum_fc->label_buf->pdata;
	*count = (uint64_t) enum_fc->label_buf->len;
	return 0;
}

int bt_field_class_signed_enumeration_get_mapping_labels_by_value(
		struct bt_field_class *fc, int64_t value,
		bt_field_class_enumeration_mapping_label_array *label_array,
		uint64_t *count)
{
	struct bt_field_class_enumeration *enum_fc = (void *) fc;
	uint64_t i;

	BT_ASSERT_PRE_NON_NULL(fc, "Field class");
	BT_ASSERT_PRE_NON_NULL(label_array, "Label array (output)");
	BT_ASSERT_PRE_NON_NULL(count, "Count (output)");
	BT_ASSERT_PRE_FC_HAS_ID(fc, BT_FIELD_CLASS_TYPE_SIGNED_ENUMERATION,
		"Field class");
	g_ptr_array_set_size(enum_fc->label_buf, 0);

	for (i = 0; i < enum_fc->mappings->len; i++) {
		uint64_t j;
		struct bt_field_class_enumeration_mapping *mapping =
			BT_FIELD_CLASS_ENUM_MAPPING_AT_INDEX(enum_fc, i);

		for (j = 0; j < mapping->ranges->len; j++) {
			struct bt_field_class_enumeration_mapping_range *range =
				BT_FIELD_CLASS_ENUM_MAPPING_RANGE_AT_INDEX(
					mapping, j);

			if (value >= range->lower.i &&
					value <= range->upper.i) {
				g_ptr_array_add(enum_fc->label_buf,
					mapping->label->str);
				break;
			}
		}
	}

	*label_array = (void *) enum_fc->label_buf->pdata;
	*count = (uint64_t) enum_fc->label_buf->len;
	return 0;
}

static inline
int add_mapping_to_enumeration_field_class(struct bt_field_class *fc,
		const char *label, uint64_t lower, uint64_t upper)
{
	int ret = 0;
	uint64_t i;
	struct bt_field_class_enumeration *enum_fc = (void *) fc;
	struct bt_field_class_enumeration_mapping *mapping = NULL;
	struct bt_field_class_enumeration_mapping_range *range;

	BT_ASSERT(fc);
	BT_ASSERT_PRE_NON_NULL(label, "Label");

	/* Find existing mapping identified by this label */
	for (i = 0; i < enum_fc->mappings->len; i++) {
		struct bt_field_class_enumeration_mapping *mapping_candidate =
			BT_FIELD_CLASS_ENUM_MAPPING_AT_INDEX(enum_fc, i);

		if (strcmp(mapping_candidate->label->str, label) == 0) {
			mapping = mapping_candidate;
			break;
		}
	}

	if (!mapping) {
		/* Create new mapping for this label */
		g_array_set_size(enum_fc->mappings, enum_fc->mappings->len + 1);
		mapping = BT_FIELD_CLASS_ENUM_MAPPING_AT_INDEX(enum_fc,
			enum_fc->mappings->len - 1);
		mapping->ranges = g_array_new(FALSE, TRUE,
			sizeof(struct bt_field_class_enumeration_mapping_range));
		if (!mapping->ranges) {
			finalize_enumeration_field_class_mapping(mapping);
			g_array_set_size(enum_fc->mappings,
				enum_fc->mappings->len - 1);
			ret = -1;
			goto end;
		}

		mapping->label = g_string_new(label);
		if (!mapping->label) {
			finalize_enumeration_field_class_mapping(mapping);
			g_array_set_size(enum_fc->mappings,
				enum_fc->mappings->len - 1);
			ret = -1;
			goto end;
		}
	}

	/* Add range */
	BT_ASSERT(mapping);
	g_array_set_size(mapping->ranges, mapping->ranges->len + 1);
	range = BT_FIELD_CLASS_ENUM_MAPPING_RANGE_AT_INDEX(mapping,
		mapping->ranges->len - 1);
	range->lower.u = lower;
	range->upper.u = upper;
	BT_LIB_LOGV("Added mapping to enumeration field class: "
		"%![fc-]+F, label=\"%s\", lower-unsigned=%" PRIu64 ", "
		"upper-unsigned=%" PRIu64, fc, label, lower, upper);

end:
	return ret;
}

int bt_private_field_class_unsigned_enumeration_map_range(
		struct bt_private_field_class *priv_fc, const char *label,
		uint64_t range_lower, uint64_t range_upper)
{
	struct bt_field_class *fc = (void *) priv_fc;
	struct bt_field_class_enumeration *enum_fc = (void *) fc;

	BT_ASSERT_PRE_NON_NULL(fc, "Field class");
	BT_ASSERT_PRE_FC_HAS_ID(fc, BT_FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION,
		"Field class");
	BT_ASSERT_PRE(range_lower <= range_upper,
		"Range's upper bound is less than lower bound: "
		"upper=%" PRIu64 ", lower=%" PRIu64,
		range_lower, range_upper);
	BT_ASSERT_PRE(bt_util_value_is_in_range_unsigned(enum_fc->common.range,
		range_lower),
		"Range's lower bound is outside the enumeration field class's value range: "
		"%![fc-]+F, lower=%" PRIu64, fc, range_lower);
	BT_ASSERT_PRE(bt_util_value_is_in_range_unsigned(enum_fc->common.range,
		range_upper),
		"Range's upper bound is outside the enumeration field class's value range: "
		"%![fc-]+F, upper=%" PRIu64, fc, range_upper);
	return add_mapping_to_enumeration_field_class(fc, label, range_lower,
		range_upper);
}

int bt_private_field_class_signed_enumeration_map_range(
		struct bt_private_field_class *priv_fc, const char *label,
		int64_t range_lower, int64_t range_upper)
{
	struct bt_field_class *fc = (void *) priv_fc;
	struct bt_field_class_enumeration *enum_fc = (void *) fc;

	BT_ASSERT_PRE_NON_NULL(fc, "Field class");
	BT_ASSERT_PRE_FC_HAS_ID(fc, BT_FIELD_CLASS_TYPE_SIGNED_ENUMERATION,
		"Field class");
	BT_ASSERT_PRE(range_lower <= range_upper,
		"Range's upper bound is less than lower bound: "
		"upper=%" PRId64 ", lower=%" PRId64,
		range_lower, range_upper);
	BT_ASSERT_PRE(bt_util_value_is_in_range_signed(enum_fc->common.range,
		range_lower),
		"Range's lower bound is outside the enumeration field class's value range: "
		"%![fc-]+F, lower=%" PRId64, fc, range_lower);
	BT_ASSERT_PRE(bt_util_value_is_in_range_signed(enum_fc->common.range,
		range_upper),
		"Range's upper bound is outside the enumeration field class's value range: "
		"%![fc-]+F, upper=%" PRId64, fc, range_upper);
	return add_mapping_to_enumeration_field_class(fc, label, range_lower,
		range_upper);
}

static
void destroy_real_field_class(struct bt_object *obj)
{
	BT_ASSERT(obj);
	BT_LIB_LOGD("Destroying real field class object: %!+F", obj);
	g_free(obj);
}

struct bt_private_field_class *bt_private_field_class_real_create(void)
{
	struct bt_field_class_real *real_fc = NULL;

	BT_LOGD_STR("Creating default real field class object.");
	real_fc = g_new0(struct bt_field_class_real, 1);
	if (!real_fc) {
		BT_LOGE_STR("Failed to allocate one real field class.");
		goto error;
	}

	init_field_class((void *) real_fc, BT_FIELD_CLASS_TYPE_REAL,
		destroy_real_field_class);
	BT_LIB_LOGD("Created real field class object: %!+F", real_fc);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(real_fc);

end:
	return (void *) real_fc;
}

bt_bool bt_field_class_real_is_single_precision(struct bt_field_class *fc)
{
	struct bt_field_class_real *real_fc = (void *) fc;

	BT_ASSERT_PRE_NON_NULL(fc, "Field class");
	BT_ASSERT_PRE_FC_HAS_ID(fc, BT_FIELD_CLASS_TYPE_REAL, "Field class");
	return real_fc->is_single_precision;
}

int bt_private_field_class_real_set_is_single_precision(
		struct bt_private_field_class *priv_fc,
		bt_bool is_single_precision)
{
	struct bt_field_class *fc = (void *) priv_fc;
	struct bt_field_class_real *real_fc = (void *) fc;

	BT_ASSERT_PRE_NON_NULL(fc, "Field class");
	BT_ASSERT_PRE_FC_HAS_ID(fc, BT_FIELD_CLASS_TYPE_REAL, "Field class");
	BT_ASSERT_PRE_FC_HOT(fc, "Field class");
	real_fc->is_single_precision = (bool) is_single_precision;
	BT_LIB_LOGV("Set real field class's \"is single precision\" property: "
		"%!+F", fc);
	return 0;
}

static
int init_named_field_classes_container(
		struct bt_field_class_named_field_class_container *fc,
		enum bt_field_class_type type, bt_object_release_func release_func)
{
	int ret = 0;

	init_field_class((void *) fc, type, release_func);
	fc->named_fcs = g_array_new(FALSE, TRUE,
		sizeof(struct bt_named_field_class));
	if (!fc->named_fcs) {
		BT_LOGE_STR("Failed to allocate a GArray.");
		ret = -1;
		goto end;
	}

	fc->name_to_index = g_hash_table_new(g_str_hash, g_str_equal);
	if (!fc->name_to_index) {
		BT_LOGE_STR("Failed to allocate a GHashTable.");
		ret = -1;
		goto end;
	}

end:
	return ret;
}

static
void finalize_named_field_class(struct bt_named_field_class *named_fc)
{
	BT_ASSERT(named_fc);
	BT_LIB_LOGD("Finalizing named field class: "
		"addr=%p, name=\"%s\", %![fc-]+F",
		named_fc, named_fc->name ? named_fc->name->str : NULL,
		named_fc->fc);

	if (named_fc->name) {
		g_string_free(named_fc->name, TRUE);
	}

	BT_LOGD_STR("Putting named field class's field class.");
	bt_object_put_ref(named_fc->fc);
}

static
void finalize_named_field_classes_container(
		struct bt_field_class_named_field_class_container *fc)
{
	uint64_t i;

	BT_ASSERT(fc);

	if (fc->named_fcs) {
		for (i = 0; i < fc->named_fcs->len; i++) {
			finalize_named_field_class(
				&g_array_index(fc->named_fcs,
					struct bt_named_field_class, i));
		}

		g_array_free(fc->named_fcs, TRUE);
	}

	if (fc->name_to_index) {
		g_hash_table_destroy(fc->name_to_index);
	}
}

static
void destroy_structure_field_class(struct bt_object *obj)
{
	BT_ASSERT(obj);
	BT_LIB_LOGD("Destroying string field class object: %!+F", obj);
	finalize_named_field_classes_container((void *) obj);
	g_free(obj);
}

struct bt_private_field_class *bt_private_field_class_structure_create(void)
{
	int ret;
	struct bt_field_class_structure *struct_fc = NULL;

	BT_LOGD_STR("Creating default structure field class object.");
	struct_fc = g_new0(struct bt_field_class_structure, 1);
	if (!struct_fc) {
		BT_LOGE_STR("Failed to allocate one structure field class.");
		goto error;
	}

	ret = init_named_field_classes_container((void *) struct_fc,
		BT_FIELD_CLASS_TYPE_STRUCTURE, destroy_structure_field_class);
	if (ret) {
		goto error;
	}

	BT_LIB_LOGD("Created structure field class object: %!+F", struct_fc);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(struct_fc);

end:
	return (void *) struct_fc;
}

static
int append_named_field_class_to_container_field_class(
		struct bt_field_class_named_field_class_container *container_fc,
		const char *name, struct bt_field_class *fc)
{
	int ret = 0;
	struct bt_named_field_class *named_fc;
	GString *name_str;

	BT_ASSERT(container_fc);
	BT_ASSERT_PRE_FC_HOT(container_fc, "Field class");
	BT_ASSERT_PRE_NON_NULL(name, "Name");
	BT_ASSERT_PRE_NON_NULL(fc, "Field class");
	BT_ASSERT_PRE(!bt_g_hash_table_contains(container_fc->name_to_index,
		name),
		"Duplicate member/option name in structure/variant field class: "
		"%![container-fc-]+F, name=\"%s\"", container_fc, name);
	name_str = g_string_new(name);
	if (!name_str) {
		BT_LOGE_STR("Failed to allocate a GString.");
		ret = -1;
		goto end;
	}

	g_array_set_size(container_fc->named_fcs,
		container_fc->named_fcs->len + 1);
	named_fc = &g_array_index(container_fc->named_fcs,
		struct bt_named_field_class, container_fc->named_fcs->len - 1);
	named_fc->name = name_str;
	named_fc->fc = bt_object_get_ref(fc);
	g_hash_table_insert(container_fc->name_to_index, named_fc->name->str,
		GUINT_TO_POINTER(container_fc->named_fcs->len - 1));
	bt_field_class_freeze(fc);

end:
	return ret;
}

int bt_private_field_class_structure_append_member(
		struct bt_private_field_class *priv_fc,
		const char *name, struct bt_private_field_class *member_fc)
{
	struct bt_field_class *fc = (void *) priv_fc;

	BT_ASSERT_PRE_NON_NULL(fc, "Field class");
	BT_ASSERT_PRE_FC_HAS_ID(fc, BT_FIELD_CLASS_TYPE_STRUCTURE, "Field class");
	return append_named_field_class_to_container_field_class((void *) fc,
		name, (void *) member_fc);
}

uint64_t bt_field_class_structure_get_member_count(struct bt_field_class *fc)
{
	struct bt_field_class_structure *struct_fc = (void *) fc;

	BT_ASSERT_PRE_NON_NULL(fc, "Field class");
	BT_ASSERT_PRE_FC_HAS_ID(fc, BT_FIELD_CLASS_TYPE_STRUCTURE, "Field class");
	return (uint64_t) struct_fc->common.named_fcs->len;
}

static
void borrow_named_field_class_from_container_field_class_at_index(
		struct bt_field_class_named_field_class_container *fc,
		uint64_t index, const char **name,
		struct bt_field_class **out_fc)
{
	struct bt_named_field_class *named_fc;

	BT_ASSERT(fc);
	BT_ASSERT_PRE_NON_NULL(name, "Name");
	BT_ASSERT_PRE_NON_NULL(out_fc, "Field class (output)");
	BT_ASSERT_PRE_VALID_INDEX(index, fc->named_fcs->len);
	named_fc = BT_FIELD_CLASS_NAMED_FC_AT_INDEX(fc, index);
	*name = named_fc->name->str;
	*out_fc = named_fc->fc;
}

void bt_field_class_structure_borrow_member_by_index(
		struct bt_field_class *fc, uint64_t index,
		const char **name, struct bt_field_class **out_fc)
{
	BT_ASSERT_PRE_NON_NULL(fc, "Field class");
	BT_ASSERT_PRE_FC_HAS_ID(fc, BT_FIELD_CLASS_TYPE_STRUCTURE, "Field class");
	borrow_named_field_class_from_container_field_class_at_index((void *) fc,
		index, name, out_fc);
}

void bt_private_field_class_structure_borrow_member_by_index(
		struct bt_private_field_class *fc, uint64_t index,
		const char **name, struct bt_private_field_class **out_fc)
{
	bt_field_class_structure_borrow_member_by_index((void *) fc,
		index, name, (void *) out_fc);
}

static
struct bt_field_class *borrow_field_class_from_container_field_class_by_name(
		struct bt_field_class_named_field_class_container *fc,
		const char *name)
{
	struct bt_field_class *ret_fc = NULL;
	struct bt_named_field_class *named_fc;
	gpointer orig_key;
	gpointer value;

	BT_ASSERT(fc);
	BT_ASSERT_PRE_NON_NULL(name, "Name");
	if (!g_hash_table_lookup_extended(fc->name_to_index, name, &orig_key,
			&value)) {
		goto end;
	}

	named_fc = BT_FIELD_CLASS_NAMED_FC_AT_INDEX(fc,
		GPOINTER_TO_UINT(value));
	ret_fc = named_fc->fc;

end:
	return ret_fc;
}

struct bt_field_class *bt_field_class_structure_borrow_member_field_class_by_name(
		struct bt_field_class *fc, const char *name)
{
	BT_ASSERT_PRE_NON_NULL(fc, "Field class");
	BT_ASSERT_PRE_FC_HAS_ID(fc, BT_FIELD_CLASS_TYPE_STRUCTURE, "Field class");
	return borrow_field_class_from_container_field_class_by_name((void *) fc,
		name);
}

struct bt_private_field_class *
bt_private_field_class_structure_borrow_member_field_class_by_name(
		struct bt_private_field_class *fc, const char *name)
{
	return (void *) bt_field_class_structure_borrow_member_field_class_by_name(
		(void *) fc, name);
}

static
void destroy_variant_field_class(struct bt_object *obj)
{
	struct bt_field_class_variant *fc = (void *) obj;

	BT_ASSERT(fc);
	BT_LIB_LOGD("Destroying variant field class object: %!+F", fc);
	finalize_named_field_classes_container((void *) fc);
	BT_LOGD_STR("Putting selector field path.");
	bt_object_put_ref(fc->selector_field_path);
	g_free(fc);
}

struct bt_private_field_class *bt_private_field_class_variant_create(void)
{
	int ret;
	struct bt_field_class_variant *var_fc = NULL;

	BT_LOGD_STR("Creating default variant field class object.");
	var_fc = g_new0(struct bt_field_class_variant, 1);
	if (!var_fc) {
		BT_LOGE_STR("Failed to allocate one variant field class.");
		goto error;
	}

	ret = init_named_field_classes_container((void *) var_fc,
		BT_FIELD_CLASS_TYPE_VARIANT, destroy_variant_field_class);
	if (ret) {
		goto error;
	}

	BT_LIB_LOGD("Created variant field class object: %!+F", var_fc);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(var_fc);

end:
	return (void *) var_fc;
}

int bt_private_field_class_variant_set_selector_field_class(
		struct bt_private_field_class *priv_fc,
		struct bt_private_field_class *selector_fc)
{
	struct bt_field_class *fc = (void *) priv_fc;
	struct bt_field_class_variant *var_fc = (void *) fc;

	BT_ASSERT_PRE_NON_NULL(fc, "Variant field class");
	BT_ASSERT_PRE_NON_NULL(selector_fc, "Selector field class");
	BT_ASSERT_PRE_FC_HAS_ID(fc, BT_FIELD_CLASS_TYPE_VARIANT, "Field class");
	BT_ASSERT_PRE_FC_IS_ENUM(selector_fc, "Selector field class");
	BT_ASSERT_PRE_FC_HOT(fc, "Variant field class");
	var_fc->selector_fc = (void *) selector_fc;
	bt_field_class_freeze((void *) selector_fc);
	return 0;
}

int bt_private_field_class_variant_append_private_option(
		struct bt_private_field_class *priv_fc,
		const char *name, struct bt_private_field_class *option_fc)
{
	struct bt_field_class *fc = (void *) priv_fc;

	BT_ASSERT_PRE_NON_NULL(fc, "Field class");
	BT_ASSERT_PRE_FC_HAS_ID(fc, BT_FIELD_CLASS_TYPE_VARIANT, "Field class");
	return append_named_field_class_to_container_field_class((void *) fc,
		name, (void *) option_fc);
}

struct bt_field_class *bt_field_class_variant_borrow_option_field_class_by_name(
		struct bt_field_class *fc, const char *name)
{
	BT_ASSERT_PRE_NON_NULL(fc, "Field class");
	BT_ASSERT_PRE_FC_HAS_ID(fc, BT_FIELD_CLASS_TYPE_VARIANT, "Field class");
	return borrow_field_class_from_container_field_class_by_name((void *) fc,
		name);
}

struct bt_private_field_class *
bt_private_field_class_variant_borrow_option_field_class_by_name(
		struct bt_private_field_class *fc, const char *name)
{
	return (void *) bt_field_class_variant_borrow_option_field_class_by_name(
		(void *) fc, name);
}

uint64_t bt_field_class_variant_get_option_count(struct bt_field_class *fc)
{
	struct bt_field_class_variant *var_fc = (void *) fc;

	BT_ASSERT_PRE_NON_NULL(fc, "Field class");
	BT_ASSERT_PRE_FC_HAS_ID(fc, BT_FIELD_CLASS_TYPE_VARIANT, "Field class");
	return (uint64_t) var_fc->common.named_fcs->len;
}

void bt_field_class_variant_borrow_option_by_index(
		struct bt_field_class *fc, uint64_t index,
		const char **name, struct bt_field_class **out_fc)
{
	BT_ASSERT_PRE_NON_NULL(fc, "Field class");
	BT_ASSERT_PRE_FC_HAS_ID(fc, BT_FIELD_CLASS_TYPE_VARIANT, "Field class");
	borrow_named_field_class_from_container_field_class_at_index((void *) fc,
		index, name, out_fc);
}

void bt_private_field_class_variant_borrow_option_by_index(
		struct bt_private_field_class *fc, uint64_t index,
		const char **name, struct bt_private_field_class **out_fc)
{
	bt_field_class_variant_borrow_option_by_index((void *) fc,
		index, name, (void *) out_fc);
}

struct bt_field_path *bt_field_class_variant_borrow_selector_field_path(
		struct bt_field_class *fc)
{
	struct bt_field_class_variant *var_fc = (void *) fc;

	BT_ASSERT_PRE_NON_NULL(fc, "Field class");
	BT_ASSERT_PRE_FC_HAS_ID(fc, BT_FIELD_CLASS_TYPE_VARIANT,
		"Field class");
	return var_fc->selector_field_path;
}

static
void init_array_field_class(struct bt_field_class_array *fc,
		enum bt_field_class_type type, bt_object_release_func release_func,
		struct bt_field_class *element_fc)
{
	BT_ASSERT(element_fc);
	init_field_class((void *) fc, type, release_func);
	fc->element_fc = bt_object_get_ref(element_fc);
	bt_field_class_freeze(element_fc);
}

static
void finalize_array_field_class(struct bt_field_class_array *array_fc)
{
	BT_ASSERT(array_fc);
	BT_LOGD_STR("Putting element field class.");
	bt_object_put_ref(array_fc->element_fc);
}

static
void destroy_static_array_field_class(struct bt_object *obj)
{
	BT_ASSERT(obj);
	BT_LIB_LOGD("Destroying static array field class object: %!+F", obj);
	finalize_array_field_class((void *) obj);
	g_free(obj);
}

struct bt_private_field_class *
bt_private_field_class_static_array_create(
		struct bt_private_field_class *priv_element_fc,
		uint64_t length)
{
	struct bt_field_class *element_fc = (void *) priv_element_fc;
	struct bt_field_class_static_array *array_fc = NULL;

	BT_ASSERT_PRE_NON_NULL(element_fc, "Element field class");
	BT_LOGD_STR("Creating default static array field class object.");
	array_fc = g_new0(struct bt_field_class_static_array, 1);
	if (!array_fc) {
		BT_LOGE_STR("Failed to allocate one static array field class.");
		goto error;
	}

	init_array_field_class((void *) array_fc, BT_FIELD_CLASS_TYPE_STATIC_ARRAY,
		destroy_static_array_field_class, element_fc);
	array_fc->length = length;
	BT_LIB_LOGD("Created static array field class object: %!+F", array_fc);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(array_fc);

end:
	return (void *) array_fc;
}

struct bt_field_class *bt_field_class_array_borrow_element_field_class(
		struct bt_field_class *fc)
{
	struct bt_field_class_array *array_fc = (void *) fc;

	BT_ASSERT_PRE_NON_NULL(fc, "Field class");
	BT_ASSERT_PRE_FC_IS_ARRAY(fc, "Field class");
	return array_fc->element_fc;
}

struct bt_private_field_class *
bt_private_field_class_array_borrow_element_field_class(
		struct bt_private_field_class *fc)
{
	return (void *) bt_field_class_array_borrow_element_field_class(
		(void *) fc);
}

uint64_t bt_field_class_static_array_get_length(struct bt_field_class *fc)
{
	struct bt_field_class_static_array *array_fc = (void *) fc;

	BT_ASSERT_PRE_NON_NULL(fc, "Field class");
	BT_ASSERT_PRE_FC_HAS_ID(fc, BT_FIELD_CLASS_TYPE_STATIC_ARRAY,
		"Field class");
	return (uint64_t) array_fc->length;
}

static
void destroy_dynamic_array_field_class(struct bt_object *obj)
{
	struct bt_field_class_dynamic_array *fc = (void *) obj;

	BT_ASSERT(fc);
	BT_LIB_LOGD("Destroying dynamic array field class object: %!+F", fc);
	finalize_array_field_class((void *) fc);
	BT_LOGD_STR("Putting length field path.");
	bt_object_put_ref(fc->length_field_path);
	g_free(fc);
}

struct bt_private_field_class *bt_private_field_class_dynamic_array_create(
		struct bt_private_field_class *priv_element_fc)
{
	struct bt_field_class *element_fc = (void *) priv_element_fc;
	struct bt_field_class_dynamic_array *array_fc = NULL;

	BT_ASSERT_PRE_NON_NULL(element_fc, "Element field class");
	BT_LOGD_STR("Creating default dynamic array field class object.");
	array_fc = g_new0(struct bt_field_class_dynamic_array, 1);
	if (!array_fc) {
		BT_LOGE_STR("Failed to allocate one dynamic array field class.");
		goto error;
	}

	init_array_field_class((void *) array_fc, BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY,
		destroy_dynamic_array_field_class, element_fc);
	BT_LIB_LOGD("Created dynamic array field class object: %!+F", array_fc);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(array_fc);

end:
	return (void *) array_fc;
}

int bt_private_field_class_dynamic_array_set_length_field_class(
		struct bt_private_field_class *priv_fc,
		struct bt_private_field_class *priv_length_fc)
{
	struct bt_field_class *fc = (void *) priv_fc;
	struct bt_field_class *length_fc = (void *) priv_length_fc;
	struct bt_field_class_dynamic_array *array_fc = (void *) fc;

	BT_ASSERT_PRE_NON_NULL(fc, "Dynamic array field class");
	BT_ASSERT_PRE_NON_NULL(length_fc, "Length field class");
	BT_ASSERT_PRE_FC_HAS_ID(fc, BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY,
		"Field class");
	BT_ASSERT_PRE_FC_IS_UNSIGNED_INT(length_fc, "Length field class");
	BT_ASSERT_PRE_FC_HOT(fc, "Dynamic array field class");
	array_fc->length_fc = length_fc;
	bt_field_class_freeze(length_fc);
	return 0;
}

struct bt_field_path *bt_field_class_dynamic_array_borrow_length_field_path(
		struct bt_field_class *fc)
{
	struct bt_field_class_dynamic_array *seq_fc = (void *) fc;

	BT_ASSERT_PRE_NON_NULL(fc, "Field class");
	BT_ASSERT_PRE_FC_HAS_ID(fc, BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY,
		"Field class");
	return seq_fc->length_field_path;
}

static
void destroy_string_field_class(struct bt_object *obj)
{
	BT_ASSERT(obj);
	BT_LIB_LOGD("Destroying string field class object: %!+F", obj);
	g_free(obj);
}

struct bt_private_field_class *bt_private_field_class_string_create(void)
{
	struct bt_field_class_string *string_fc = NULL;

	BT_LOGD_STR("Creating default string field class object.");
	string_fc = g_new0(struct bt_field_class_string, 1);
	if (!string_fc) {
		BT_LOGE_STR("Failed to allocate one string field class.");
		goto error;
	}

	init_field_class((void *) string_fc, BT_FIELD_CLASS_TYPE_STRING,
		destroy_string_field_class);
	BT_LIB_LOGD("Created string field class object: %!+F", string_fc);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(string_fc);

end:
	return (void *) string_fc;
}

BT_HIDDEN
void _bt_field_class_freeze(struct bt_field_class *fc)
{
	/*
	 * Element/member/option field classes are frozen when added to
	 * their owner.
	 */
	BT_ASSERT(fc);
	fc->frozen = true;
}

BT_HIDDEN
void _bt_field_class_make_part_of_trace(struct bt_field_class *fc)
{
	BT_ASSERT(fc);
	BT_ASSERT_PRE(!fc->part_of_trace,
		"Field class is already part of a trace: %!+F", fc);
	fc->part_of_trace = true;

	switch (fc->type) {
	case BT_FIELD_CLASS_TYPE_STRUCTURE:
	case BT_FIELD_CLASS_TYPE_VARIANT:
	{
		struct bt_field_class_named_field_class_container *container_fc =
			(void *) fc;
		uint64_t i;

		for (i = 0; i < container_fc->named_fcs->len; i++) {
			struct bt_named_field_class *named_fc =
				BT_FIELD_CLASS_NAMED_FC_AT_INDEX(
					container_fc, i);

			bt_field_class_make_part_of_trace(named_fc->fc);
		}

		break;
	}
	case BT_FIELD_CLASS_TYPE_STATIC_ARRAY:
	case BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY:
	{
		struct bt_field_class_array *array_fc = (void *) fc;

		bt_field_class_make_part_of_trace(array_fc->element_fc);
		break;
	}
	default:
		break;
	}
}
