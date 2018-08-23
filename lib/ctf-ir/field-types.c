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
#include <babeltrace/compat/glib-internal.h>
#include <float.h>
#include <inttypes.h>
#include <stdlib.h>

enum bt_field_type_id bt_field_type_get_type_id(struct bt_field_type *ft)
{
	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	return ft->id;
}

static
void init_field_type(struct bt_field_type *ft, enum bt_field_type_id id,
		bt_object_release_func release_func)
{
	BT_ASSERT(ft);
	BT_ASSERT(bt_field_type_has_known_id(ft));
	BT_ASSERT(release_func);
	bt_object_init_shared(&ft->base, release_func);
	ft->id = id;
}

static
void init_integer_field_type(struct bt_field_type_integer *ft, enum bt_field_type_id id,
		bt_object_release_func release_func)
{
	init_field_type((void *) ft, id, release_func);
	ft->range = 64;
	ft->base = BT_FIELD_TYPE_INTEGER_PREFERRED_DISPLAY_BASE_DECIMAL;
}

static
void destroy_integer_field_type(struct bt_object *obj)
{
	BT_ASSERT(obj);
	BT_LIB_LOGD("Destroying integer field type object: %!+F", obj);
	g_free(obj);
}

static inline
struct bt_field_type *create_integer_field_type(enum bt_field_type_id id)
{
	struct bt_field_type_integer *int_ft = NULL;

	BT_LOGD("Creating default integer field type object: id=%s",
		bt_common_field_type_id_string(id));
	int_ft = g_new0(struct bt_field_type_integer, 1);
	if (!int_ft) {
		BT_LOGE_STR("Failed to allocate one integer field type.");
		goto error;
	}

	init_integer_field_type(int_ft, id, destroy_integer_field_type);
	BT_LIB_LOGD("Created integer field type object: %!+F", int_ft);
	goto end;

error:
	BT_PUT(int_ft);

end:
	return (void *) int_ft;
}

struct bt_field_type *bt_field_type_unsigned_integer_create(void)
{
	return create_integer_field_type(BT_FIELD_TYPE_ID_UNSIGNED_INTEGER);
}

struct bt_field_type *bt_field_type_signed_integer_create(void)
{
	return create_integer_field_type(BT_FIELD_TYPE_ID_SIGNED_INTEGER);
}

uint64_t bt_field_type_integer_get_field_value_range(
		struct bt_field_type *ft)
{
	struct bt_field_type_integer *int_ft = (void *) ft;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_IS_INT(ft, "Field type");
	return int_ft->range;
}

BT_ASSERT_PRE_FUNC
static
bool size_is_valid_for_enumeration_field_type(struct bt_field_type *ft,
		uint64_t size)
{
	// TODO
	return true;
}

int bt_field_type_integer_set_field_value_range(
		struct bt_field_type *ft, uint64_t size)
{
	struct bt_field_type_integer *int_ft = (void *) ft;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_IS_INT(ft, "Field type");
	BT_ASSERT_PRE_FT_HOT(ft, "Field type");
	BT_ASSERT_PRE(size <= 64,
		"Unsupported size for integer field type's field value range "
		"(maximum is 64): size=%" PRIu64, size);
	BT_ASSERT_PRE(int_ft->common.id == BT_FIELD_TYPE_ID_UNSIGNED_INTEGER ||
		int_ft->common.id == BT_FIELD_TYPE_ID_SIGNED_INTEGER ||
		size_is_valid_for_enumeration_field_type(ft, size),
		"Invalid field value range for enumeration field type: "
		"at least one of the current mapping ranges contains values "
		"which are outside this range: %!+F, size=%" PRIu64, ft, size);
	int_ft->range = size;
	BT_LIB_LOGV("Set integer field type's field value range: %!+F", ft);
	return 0;
}

enum bt_field_type_integer_preferred_display_base
bt_field_type_integer_get_preferred_display_base(struct bt_field_type *ft)
{
	struct bt_field_type_integer *int_ft = (void *) ft;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_IS_INT(ft, "Field type");
	return int_ft->base;
}

int bt_field_type_integer_set_preferred_display_base(struct bt_field_type *ft,
		enum bt_field_type_integer_preferred_display_base base)
{
	struct bt_field_type_integer *int_ft = (void *) ft;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_IS_INT(ft, "Field type");
	BT_ASSERT_PRE_FT_HOT(ft, "Field type");
	int_ft->base = base;
	BT_LIB_LOGV("Set integer field type's preferred display base: %!+F", ft);
	return 0;
}

static
void finalize_enumeration_field_type_mapping(
		struct bt_field_type_enumeration_mapping *mapping)
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
void destroy_enumeration_field_type(struct bt_object *obj)
{
	struct bt_field_type_enumeration *ft = (void *) obj;

	BT_ASSERT(ft);
	BT_LIB_LOGD("Destroying enumeration field type object: %!+F", ft);

	if (ft->mappings) {
		uint64_t i;

		for (i = 0; i < ft->mappings->len; i++) {
			finalize_enumeration_field_type_mapping(
				BT_FIELD_TYPE_ENUM_MAPPING_AT_INDEX(ft, i));
		}

		g_array_free(ft->mappings, TRUE);
	}

	if (ft->label_buf) {
		g_ptr_array_free(ft->label_buf, TRUE);
	}

	g_free(ft);
}

static
struct bt_field_type *create_enumeration_field_type(enum bt_field_type_id id)
{
	struct bt_field_type_enumeration *enum_ft = NULL;

	BT_LOGD("Creating default enumeration field type object: id=%s",
		bt_common_field_type_id_string(id));
	enum_ft = g_new0(struct bt_field_type_enumeration, 1);
	if (!enum_ft) {
		BT_LOGE_STR("Failed to allocate one enumeration field type.");
		goto error;
	}

	init_integer_field_type((void *) enum_ft, id,
		destroy_enumeration_field_type);
	enum_ft->mappings = g_array_new(FALSE, TRUE,
		sizeof(struct bt_field_type_enumeration_mapping));
	if (!enum_ft->mappings) {
		BT_LOGE_STR("Failed to allocate a GArray.");
		goto error;
	}

	enum_ft->label_buf = g_ptr_array_new();
	if (!enum_ft->label_buf) {
		BT_LOGE_STR("Failed to allocate a GArray.");
		goto error;
	}

	BT_LIB_LOGD("Created enumeration field type object: %!+F", enum_ft);
	goto end;

error:
	BT_PUT(enum_ft);

end:
	return (void *) enum_ft;
}

struct bt_field_type *bt_field_type_unsigned_enumeration_create(void)
{
	return create_enumeration_field_type(
		BT_FIELD_TYPE_ID_UNSIGNED_ENUMERATION);
}

struct bt_field_type *bt_field_type_signed_enumeration_create(void)
{
	return create_enumeration_field_type(
		BT_FIELD_TYPE_ID_SIGNED_ENUMERATION);
}

uint64_t bt_field_type_enumeration_get_mapping_count(struct bt_field_type *ft)
{
	struct bt_field_type_enumeration *enum_ft = (void *) ft;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_IS_ENUM(ft, "Field type");
	return (uint64_t) enum_ft->mappings->len;
}

void bt_field_type_unsigned_enumeration_borrow_mapping_by_index(
		struct bt_field_type *ft, uint64_t index,
		const char **name,
		struct bt_field_type_unsigned_enumeration_mapping_ranges **ranges)
{
	struct bt_field_type_enumeration *enum_ft = (void *) ft;
	struct bt_field_type_enumeration_mapping *mapping;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_NON_NULL(name, "Name (output)");
	BT_ASSERT_PRE_NON_NULL(ranges, "Ranges (output)");
	BT_ASSERT_PRE_VALID_INDEX(index, enum_ft->mappings->len);
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_UNSIGNED_ENUMERATION,
		"Field type");
	mapping = BT_FIELD_TYPE_ENUM_MAPPING_AT_INDEX(ft, index);
	*name = mapping->label->str;
	*ranges = (void *) mapping;
}

void bt_field_type_signed_enumeration_borrow_mapping_by_index(
		struct bt_field_type *ft, uint64_t index,
		const char **name,
		struct bt_field_type_signed_enumeration_mapping_ranges **ranges)
{
	struct bt_field_type_enumeration *enum_ft = (void *) ft;
	struct bt_field_type_enumeration_mapping *mapping;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_NON_NULL(name, "Name (output)");
	BT_ASSERT_PRE_NON_NULL(ranges, "Ranges (output)");
	BT_ASSERT_PRE_VALID_INDEX(index, enum_ft->mappings->len);
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_SIGNED_ENUMERATION,
		"Field type");
	mapping = BT_FIELD_TYPE_ENUM_MAPPING_AT_INDEX(ft, index);
	*name = mapping->label->str;
	*ranges = (void *) mapping;
}

static inline
uint64_t get_enumeration_field_type_mapping_range_count(
		struct bt_field_type_enumeration_mapping *mapping)
{
	BT_ASSERT_PRE_NON_NULL(mapping, "Ranges");
	return (uint64_t) mapping->ranges->len;
}

uint64_t bt_field_type_unsigned_enumeration_mapping_ranges_get_range_count(
		struct bt_field_type_unsigned_enumeration_mapping_ranges *ranges)
{
	return get_enumeration_field_type_mapping_range_count((void *) ranges);
}

uint64_t bt_field_type_signed_enumeration_mapping_ranges_get_range_count(
		struct bt_field_type_signed_enumeration_mapping_ranges *ranges)
{
	return get_enumeration_field_type_mapping_range_count((void *) ranges);
}

static inline
void get_enumeration_field_type_mapping_range_at_index(
		struct bt_field_type_enumeration_mapping *mapping,
		uint64_t index, uint64_t *lower, uint64_t *upper)
{
	struct bt_field_type_enumeration_mapping_range *range;

	BT_ASSERT_PRE_NON_NULL(mapping, "Ranges");
	BT_ASSERT_PRE_NON_NULL(lower, "Range's lower (output)");
	BT_ASSERT_PRE_NON_NULL(upper, "Range's upper (output)");
	BT_ASSERT_PRE_VALID_INDEX(index, mapping->ranges->len);
	range = BT_FIELD_TYPE_ENUM_MAPPING_RANGE_AT_INDEX(mapping, index);
	*lower = range->lower.u;
	*upper = range->upper.u;
}

void bt_field_type_unsigned_enumeration_mapping_ranges_get_range_by_index(
		struct bt_field_type_unsigned_enumeration_mapping_ranges *ranges,
		uint64_t index, uint64_t *lower, uint64_t *upper)
{
	get_enumeration_field_type_mapping_range_at_index((void *) ranges,
		index, lower, upper);
}

void bt_field_type_signed_enumeration_mapping_ranges_get_range_by_index(
		struct bt_field_type_unsigned_enumeration_mapping_ranges *ranges,
		uint64_t index, int64_t *lower, int64_t *upper)
{
	get_enumeration_field_type_mapping_range_at_index((void *) ranges,
		index, (uint64_t *) lower, (uint64_t *) upper);
}



int bt_field_type_unsigned_enumeration_get_mapping_labels_by_value(
		struct bt_field_type *ft, uint64_t value,
		bt_field_type_enumeration_mapping_label_array *label_array,
		uint64_t *count)
{
	struct bt_field_type_enumeration *enum_ft = (void *) ft;
	uint64_t i;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_NON_NULL(label_array, "Label array (output)");
	BT_ASSERT_PRE_NON_NULL(count, "Count (output)");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_UNSIGNED_ENUMERATION,
		"Field type");
	g_ptr_array_set_size(enum_ft->label_buf, 0);

	for (i = 0; i < enum_ft->mappings->len; i++) {
		uint64_t j;
		struct bt_field_type_enumeration_mapping *mapping =
			BT_FIELD_TYPE_ENUM_MAPPING_AT_INDEX(enum_ft, i);

		for (j = 0; j < mapping->ranges->len; j++) {
			struct bt_field_type_enumeration_mapping_range *range =
				BT_FIELD_TYPE_ENUM_MAPPING_RANGE_AT_INDEX(
					mapping, j);

			if (value >= range->lower.u &&
					value <= range->upper.u) {
				g_ptr_array_add(enum_ft->label_buf,
					mapping->label->str);
				break;
			}
		}
	}

	*label_array = (void *) enum_ft->label_buf->pdata;
	*count = (uint64_t) enum_ft->label_buf->len;
	return 0;
}

int bt_field_type_signed_enumeration_get_mapping_labels_by_value(
		struct bt_field_type *ft, int64_t value,
		bt_field_type_enumeration_mapping_label_array *label_array,
		uint64_t *count)
{
	struct bt_field_type_enumeration *enum_ft = (void *) ft;
	uint64_t i;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_NON_NULL(label_array, "Label array (output)");
	BT_ASSERT_PRE_NON_NULL(count, "Count (output)");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_SIGNED_ENUMERATION,
		"Field type");
	g_ptr_array_set_size(enum_ft->label_buf, 0);

	for (i = 0; i < enum_ft->mappings->len; i++) {
		uint64_t j;
		struct bt_field_type_enumeration_mapping *mapping =
			BT_FIELD_TYPE_ENUM_MAPPING_AT_INDEX(enum_ft, i);

		for (j = 0; j < mapping->ranges->len; j++) {
			struct bt_field_type_enumeration_mapping_range *range =
				BT_FIELD_TYPE_ENUM_MAPPING_RANGE_AT_INDEX(
					mapping, j);

			if (value >= range->lower.i &&
					value <= range->upper.i) {
				g_ptr_array_add(enum_ft->label_buf,
					mapping->label->str);
				break;
			}
		}
	}

	*label_array = (void *) enum_ft->label_buf->pdata;
	*count = (uint64_t) enum_ft->label_buf->len;
	return 0;
}

static inline
int add_mapping_to_enumeration_field_type(struct bt_field_type *ft,
		const char *label, uint64_t lower, uint64_t upper)
{
	int ret = 0;
	uint64_t i;
	struct bt_field_type_enumeration *enum_ft = (void *) ft;
	struct bt_field_type_enumeration_mapping *mapping = NULL;
	struct bt_field_type_enumeration_mapping_range *range;

	BT_ASSERT(ft);
	BT_ASSERT_PRE_NON_NULL(label, "Label");

	/* Find existing mapping identified by this label */
	for (i = 0; i < enum_ft->mappings->len; i++) {
		struct bt_field_type_enumeration_mapping *mapping_candidate =
			BT_FIELD_TYPE_ENUM_MAPPING_AT_INDEX(enum_ft, i);

		if (strcmp(mapping_candidate->label->str, label) == 0) {
			mapping = mapping_candidate;
			break;
		}
	}

	if (!mapping) {
		/* Create new mapping for this label */
		g_array_set_size(enum_ft->mappings, enum_ft->mappings->len + 1);
		mapping = BT_FIELD_TYPE_ENUM_MAPPING_AT_INDEX(enum_ft,
			enum_ft->mappings->len - 1);
		mapping->ranges = g_array_new(FALSE, TRUE,
			sizeof(struct bt_field_type_enumeration_mapping_range));
		if (!mapping->ranges) {
			finalize_enumeration_field_type_mapping(mapping);
			g_array_set_size(enum_ft->mappings,
				enum_ft->mappings->len - 1);
			ret = -1;
			goto end;
		}

		mapping->label = g_string_new(label);
		if (!mapping->label) {
			finalize_enumeration_field_type_mapping(mapping);
			g_array_set_size(enum_ft->mappings,
				enum_ft->mappings->len - 1);
			ret = -1;
			goto end;
		}
	}

	/* Add range */
	BT_ASSERT(mapping);
	g_array_set_size(mapping->ranges, mapping->ranges->len + 1);
	range = BT_FIELD_TYPE_ENUM_MAPPING_RANGE_AT_INDEX(mapping,
		mapping->ranges->len - 1);
	range->lower.u = lower;
	range->upper.u = upper;
	BT_LIB_LOGV("Added mapping to enumeration field type: "
		"%![ft-]+F, label=\"%s\", lower-unsigned=%" PRIu64 ", "
		"upper-unsigned=%" PRIu64, ft, label, lower, upper);

end:
	return ret;
}

int bt_field_type_unsigned_enumeration_map_range(
		struct bt_field_type *ft, const char *label,
		uint64_t range_lower, uint64_t range_upper)
{
	struct bt_field_type_enumeration *enum_ft = (void *) ft;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_UNSIGNED_ENUMERATION,
		"Field type");
	BT_ASSERT_PRE(range_lower <= range_upper,
		"Range's upper bound is less than lower bound: "
		"upper=%" PRIu64 ", lower=%" PRIu64,
		range_lower, range_upper);
	BT_ASSERT_PRE(bt_util_value_is_in_range_unsigned(enum_ft->common.range,
		range_lower),
		"Range's lower bound is outside the enumeration field type's value range: "
		"%![ft-]+F, lower=%" PRIu64, ft, range_lower);
	BT_ASSERT_PRE(bt_util_value_is_in_range_unsigned(enum_ft->common.range,
		range_upper),
		"Range's upper bound is outside the enumeration field type's value range: "
		"%![ft-]+F, upper=%" PRIu64, ft, range_upper);
	return add_mapping_to_enumeration_field_type(ft, label, range_lower,
		range_upper);
}

int bt_field_type_signed_enumeration_map_range(
		struct bt_field_type *ft, const char *label,
		int64_t range_lower, int64_t range_upper)
{
	struct bt_field_type_enumeration *enum_ft = (void *) ft;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_SIGNED_ENUMERATION,
		"Field type");
	BT_ASSERT_PRE(range_lower <= range_upper,
		"Range's upper bound is less than lower bound: "
		"upper=%" PRId64 ", lower=%" PRId64,
		range_lower, range_upper);
	BT_ASSERT_PRE(bt_util_value_is_in_range_signed(enum_ft->common.range,
		range_lower),
		"Range's lower bound is outside the enumeration field type's value range: "
		"%![ft-]+F, lower=%" PRId64, ft, range_lower);
	BT_ASSERT_PRE(bt_util_value_is_in_range_signed(enum_ft->common.range,
		range_upper),
		"Range's upper bound is outside the enumeration field type's value range: "
		"%![ft-]+F, upper=%" PRId64, ft, range_upper);
	return add_mapping_to_enumeration_field_type(ft, label, range_lower,
		range_upper);
}

static
void destroy_real_field_type(struct bt_object *obj)
{
	BT_ASSERT(obj);
	BT_LIB_LOGD("Destroying real field type object: %!+F", obj);
	g_free(obj);
}

struct bt_field_type *bt_field_type_real_create(void)
{
	struct bt_field_type_real *real_ft = NULL;

	BT_LOGD_STR("Creating default real field type object.");
	real_ft = g_new0(struct bt_field_type_real, 1);
	if (!real_ft) {
		BT_LOGE_STR("Failed to allocate one real field type.");
		goto error;
	}

	init_field_type((void *) real_ft, BT_FIELD_TYPE_ID_REAL,
		destroy_real_field_type);
	BT_LIB_LOGD("Created real field type object: %!+F", real_ft);
	goto end;

error:
	BT_PUT(real_ft);

end:
	return (void *) real_ft;
}

bt_bool bt_field_type_real_is_single_precision(struct bt_field_type *ft)
{
	struct bt_field_type_real *real_ft = (void *) ft;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_REAL, "Field type");
	return real_ft->is_single_precision;
}

int bt_field_type_real_set_is_single_precision(struct bt_field_type *ft,
		bt_bool is_single_precision)
{
	struct bt_field_type_real *real_ft = (void *) ft;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_REAL, "Field type");
	BT_ASSERT_PRE_FT_HOT(ft, "Field type");
	real_ft->is_single_precision = (bool) is_single_precision;
	BT_LIB_LOGV("Set real field type's \"is single precision\" property: "
		"%!+F", ft);
	return 0;
}

static
int init_named_field_types_container(
		struct bt_field_type_named_field_types_container *ft,
		enum bt_field_type_id id, bt_object_release_func release_func)
{
	int ret = 0;

	init_field_type((void *) ft, id, release_func);
	ft->named_fts = g_array_new(FALSE, TRUE,
		sizeof(struct bt_named_field_type));
	if (!ft->named_fts) {
		BT_LOGE_STR("Failed to allocate a GArray.");
		ret = -1;
		goto end;
	}

	ft->name_to_index = g_hash_table_new(g_str_hash, g_str_equal);
	if (!ft->name_to_index) {
		BT_LOGE_STR("Failed to allocate a GHashTable.");
		ret = -1;
		goto end;
	}

end:
	return ret;
}

static
void finalize_named_field_type(struct bt_named_field_type *named_ft)
{
	BT_ASSERT(named_ft);
	BT_LIB_LOGD("Finalizing named field type: "
		"addr=%p, name=\"%s\", %![ft-]+F",
		named_ft, named_ft->name ? named_ft->name->str : NULL,
		named_ft->ft);

	if (named_ft->name) {
		g_string_free(named_ft->name, TRUE);
	}

	BT_LOGD_STR("Putting named field type's field type.");
	bt_put(named_ft->ft);
}

static
void finalize_named_field_types_container(
		struct bt_field_type_named_field_types_container *ft)
{
	uint64_t i;

	BT_ASSERT(ft);

	if (ft->named_fts) {
		for (i = 0; i < ft->named_fts->len; i++) {
			finalize_named_field_type(
				&g_array_index(ft->named_fts,
					struct bt_named_field_type, i));
		}

		g_array_free(ft->named_fts, TRUE);
	}

	if (ft->name_to_index) {
		g_hash_table_destroy(ft->name_to_index);
	}
}

static
void destroy_structure_field_type(struct bt_object *obj)
{
	BT_ASSERT(obj);
	BT_LIB_LOGD("Destroying string field type object: %!+F", obj);
	finalize_named_field_types_container((void *) obj);
	g_free(obj);
}

struct bt_field_type *bt_field_type_structure_create(void)
{
	int ret;
	struct bt_field_type_structure *struct_ft = NULL;

	BT_LOGD_STR("Creating default structure field type object.");
	struct_ft = g_new0(struct bt_field_type_structure, 1);
	if (!struct_ft) {
		BT_LOGE_STR("Failed to allocate one structure field type.");
		goto error;
	}

	ret = init_named_field_types_container((void *) struct_ft,
		BT_FIELD_TYPE_ID_STRUCTURE, destroy_structure_field_type);
	if (ret) {
		goto error;
	}

	BT_LIB_LOGD("Created structure field type object: %!+F", struct_ft);
	goto end;

error:
	BT_PUT(struct_ft);

end:
	return (void *) struct_ft;
}

static
int append_named_field_type_to_container_field_type(
		struct bt_field_type_named_field_types_container *container_ft,
		const char *name, struct bt_field_type *ft)
{
	int ret = 0;
	struct bt_named_field_type *named_ft;
	GString *name_str;

	BT_ASSERT(container_ft);
	BT_ASSERT_PRE_FT_HOT(container_ft, "Field type");
	BT_ASSERT_PRE_NON_NULL(name, "Name");
	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE(!bt_g_hash_table_contains(container_ft->name_to_index,
		name),
		"Duplicate member/option name in structure/variant field type: "
		"%![container-ft-]+F, name=\"%s\"", container_ft, name);
	name_str = g_string_new(name);
	if (!name_str) {
		BT_LOGE_STR("Failed to allocate a GString.");
		ret = -1;
		goto end;
	}

	g_array_set_size(container_ft->named_fts,
		container_ft->named_fts->len + 1);
	named_ft = &g_array_index(container_ft->named_fts,
		struct bt_named_field_type, container_ft->named_fts->len - 1);
	named_ft->name = name_str;
	named_ft->ft = bt_get(ft);
	g_hash_table_insert(container_ft->name_to_index, named_ft->name->str,
		GUINT_TO_POINTER(container_ft->named_fts->len - 1));
	bt_field_type_freeze(ft);

end:
	return ret;
}

int bt_field_type_structure_append_member(struct bt_field_type *ft,
		const char *name, struct bt_field_type *member_ft)
{
	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_STRUCTURE, "Field type");
	return append_named_field_type_to_container_field_type((void *) ft,
		name, member_ft);
}

uint64_t bt_field_type_structure_get_member_count(struct bt_field_type *ft)
{
	struct bt_field_type_structure *struct_ft = (void *) ft;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_STRUCTURE, "Field type");
	return (uint64_t) struct_ft->common.named_fts->len;
}

static
void borrow_named_field_type_from_container_field_type_at_index(
		struct bt_field_type_named_field_types_container *ft,
		uint64_t index, const char **name,
		struct bt_field_type **out_ft)
{
	struct bt_named_field_type *named_ft;

	BT_ASSERT(ft);
	BT_ASSERT_PRE_NON_NULL(name, "Name");
	BT_ASSERT_PRE_NON_NULL(out_ft, "Field type (output)");
	BT_ASSERT_PRE_VALID_INDEX(index, ft->named_fts->len);
	named_ft = BT_FIELD_TYPE_NAMED_FT_AT_INDEX(ft, index);
	*name = named_ft->name->str;
	*out_ft = named_ft->ft;
}

void bt_field_type_structure_borrow_member_by_index(
		struct bt_field_type *ft, uint64_t index,
		const char **name, struct bt_field_type **out_ft)
{
	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_STRUCTURE, "Field type");
	borrow_named_field_type_from_container_field_type_at_index((void *) ft,
		index, name, out_ft);
}

static
struct bt_field_type *borrow_field_type_from_container_field_type_by_name(
		struct bt_field_type_named_field_types_container *ft,
		const char *name)
{
	struct bt_field_type *ret_ft = NULL;
	struct bt_named_field_type *named_ft;
	gpointer orig_key;
	gpointer value;

	BT_ASSERT(ft);
	BT_ASSERT_PRE_NON_NULL(name, "Name");
	if (!g_hash_table_lookup_extended(ft->name_to_index, name, &orig_key,
			&value)) {
		goto end;
	}

	named_ft = BT_FIELD_TYPE_NAMED_FT_AT_INDEX(ft,
		GPOINTER_TO_UINT(value));
	ret_ft = named_ft->ft;

end:
	return ret_ft;
}

struct bt_field_type *bt_field_type_structure_borrow_member_field_type_by_name(
		struct bt_field_type *ft, const char *name)
{
	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_STRUCTURE, "Field type");
	return borrow_field_type_from_container_field_type_by_name((void *) ft,
		name);
}

static
void destroy_variant_field_type(struct bt_object *obj)
{
	struct bt_field_type_variant *ft = (void *) obj;

	BT_ASSERT(ft);
	BT_LIB_LOGD("Destroying variant field type object: %!+F", ft);
	finalize_named_field_types_container((void *) ft);
	BT_LOGD_STR("Putting selector field path.");
	bt_put(ft->selector_field_path);
	g_free(ft);
}

struct bt_field_type *bt_field_type_variant_create(void)
{
	int ret;
	struct bt_field_type_variant *var_ft = NULL;

	BT_LOGD_STR("Creating default variant field type object.");
	var_ft = g_new0(struct bt_field_type_variant, 1);
	if (!var_ft) {
		BT_LOGE_STR("Failed to allocate one variant field type.");
		goto error;
	}

	ret = init_named_field_types_container((void *) var_ft,
		BT_FIELD_TYPE_ID_VARIANT, destroy_variant_field_type);
	if (ret) {
		goto error;
	}

	BT_LIB_LOGD("Created variant field type object: %!+F", var_ft);
	goto end;

error:
	BT_PUT(var_ft);

end:
	return (void *) var_ft;
}

int bt_field_type_variant_set_selector_field_type(
		struct bt_field_type *ft, struct bt_field_type *selector_ft)
{
	struct bt_field_type_variant *var_ft = (void *) ft;

	BT_ASSERT_PRE_NON_NULL(ft, "Variant field type");
	BT_ASSERT_PRE_NON_NULL(selector_ft, "Selector field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_VARIANT, "Field type");
	BT_ASSERT_PRE_FT_IS_ENUM(selector_ft, "Selector field type");
	BT_ASSERT_PRE_FT_HOT(ft, "Variant field type");
	var_ft->selector_ft = selector_ft;
	bt_field_type_freeze(selector_ft);
	return 0;
}

int bt_field_type_variant_append_option(struct bt_field_type *ft,
		const char *name, struct bt_field_type *option_ft)
{
	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_VARIANT, "Field type");
	return append_named_field_type_to_container_field_type((void *) ft,
		name, option_ft);
}

struct bt_field_type *bt_field_type_variant_borrow_option_field_type_by_name(
		struct bt_field_type *ft, const char *name)
{
	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_VARIANT, "Field type");
	return borrow_field_type_from_container_field_type_by_name((void *) ft,
		name);
}

uint64_t bt_field_type_variant_get_option_count(struct bt_field_type *ft)
{
	struct bt_field_type_variant *var_ft = (void *) ft;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_VARIANT, "Field type");
	return (uint64_t) var_ft->common.named_fts->len;
}

void bt_field_type_variant_borrow_option_by_index(
		struct bt_field_type *ft, uint64_t index,
		const char **name, struct bt_field_type **out_ft)
{
	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_VARIANT, "Field type");
	borrow_named_field_type_from_container_field_type_at_index((void *) ft,
		index, name, out_ft);
}

struct bt_field_path *bt_field_type_variant_borrow_selector_field_path(
		struct bt_field_type *ft)
{
	struct bt_field_type_variant *var_ft = (void *) ft;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_VARIANT,
		"Field type");
	return var_ft->selector_field_path;
}

static
void init_array_field_type(struct bt_field_type_array *ft,
		enum bt_field_type_id id, bt_object_release_func release_func,
		struct bt_field_type *element_ft)
{
	BT_ASSERT(element_ft);
	init_field_type((void *) ft, id, release_func);
	ft->element_ft = bt_get(element_ft);
	bt_field_type_freeze(element_ft);
}

static
void finalize_array_field_type(struct bt_field_type_array *array_ft)
{
	BT_ASSERT(array_ft);
	BT_LOGD_STR("Putting element field type.");
	bt_put(array_ft->element_ft);
}

static
void destroy_static_array_field_type(struct bt_object *obj)
{
	BT_ASSERT(obj);
	BT_LIB_LOGD("Destroying static array field type object: %!+F", obj);
	finalize_array_field_type((void *) obj);
	g_free(obj);
}

struct bt_field_type *bt_field_type_static_array_create(
		struct bt_field_type *element_ft, uint64_t length)
{
	struct bt_field_type_static_array *array_ft = NULL;

	BT_ASSERT_PRE_NON_NULL(element_ft, "Element field type");
	BT_LOGD_STR("Creating default static array field type object.");
	array_ft = g_new0(struct bt_field_type_static_array, 1);
	if (!array_ft) {
		BT_LOGE_STR("Failed to allocate one static array field type.");
		goto error;
	}

	init_array_field_type((void *) array_ft, BT_FIELD_TYPE_ID_STATIC_ARRAY,
		destroy_static_array_field_type, element_ft);
	array_ft->length = length;
	BT_LIB_LOGD("Created static array field type object: %!+F", array_ft);
	goto end;

error:
	BT_PUT(array_ft);

end:
	return (void *) array_ft;
}

struct bt_field_type *bt_field_type_array_borrow_element_field_type(
		struct bt_field_type *ft)
{
	struct bt_field_type_array *array_ft = (void *) ft;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_IS_ARRAY(ft, "Field type");
	return array_ft->element_ft;
}

uint64_t bt_field_type_static_array_get_length(struct bt_field_type *ft)
{
	struct bt_field_type_static_array *array_ft = (void *) ft;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_STATIC_ARRAY,
		"Field type");
	return (uint64_t) array_ft->length;
}

static
void destroy_dynamic_array_field_type(struct bt_object *obj)
{
	struct bt_field_type_dynamic_array *ft = (void *) obj;

	BT_ASSERT(ft);
	BT_LIB_LOGD("Destroying dynamic array field type object: %!+F", ft);
	finalize_array_field_type((void *) ft);
	BT_LOGD_STR("Putting length field path.");
	bt_put(ft->length_field_path);
	g_free(ft);
}

struct bt_field_type *bt_field_type_dynamic_array_create(
		struct bt_field_type *element_ft)
{
	struct bt_field_type_dynamic_array *array_ft = NULL;

	BT_ASSERT_PRE_NON_NULL(element_ft, "Element field type");
	BT_LOGD_STR("Creating default dynamic array field type object.");
	array_ft = g_new0(struct bt_field_type_dynamic_array, 1);
	if (!array_ft) {
		BT_LOGE_STR("Failed to allocate one dynamic array field type.");
		goto error;
	}

	init_array_field_type((void *) array_ft, BT_FIELD_TYPE_ID_DYNAMIC_ARRAY,
		destroy_dynamic_array_field_type, element_ft);
	BT_LIB_LOGD("Created dynamic array field type object: %!+F", array_ft);
	goto end;

error:
	BT_PUT(array_ft);

end:
	return (void *) array_ft;
}

int bt_field_type_dynamic_array_set_length_field_type(struct bt_field_type *ft,
		struct bt_field_type *length_ft)
{
	struct bt_field_type_dynamic_array *array_ft = (void *) ft;

	BT_ASSERT_PRE_NON_NULL(ft, "Dynamic array field type");
	BT_ASSERT_PRE_NON_NULL(length_ft, "Length field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_DYNAMIC_ARRAY,
		"Field type");
	BT_ASSERT_PRE_FT_IS_UNSIGNED_INT(length_ft, "Length field type");
	BT_ASSERT_PRE_FT_HOT(ft, "Dynamic array field type");
	array_ft->length_ft = length_ft;
	bt_field_type_freeze(length_ft);
	return 0;
}

struct bt_field_path *bt_field_type_dynamic_array_borrow_length_field_path(
		struct bt_field_type *ft)
{
	struct bt_field_type_dynamic_array *seq_ft = (void *) ft;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT_PRE_FT_HAS_ID(ft, BT_FIELD_TYPE_ID_DYNAMIC_ARRAY,
		"Field type");
	return seq_ft->length_field_path;
}

static
void destroy_string_field_type(struct bt_object *obj)
{
	BT_ASSERT(obj);
	BT_LIB_LOGD("Destroying string field type object: %!+F", obj);
	g_free(obj);
}

struct bt_field_type *bt_field_type_string_create(void)
{
	struct bt_field_type_string *string_ft = NULL;

	BT_LOGD_STR("Creating default string field type object.");
	string_ft = g_new0(struct bt_field_type_string, 1);
	if (!string_ft) {
		BT_LOGE_STR("Failed to allocate one string field type.");
		goto error;
	}

	init_field_type((void *) string_ft, BT_FIELD_TYPE_ID_STRING,
		destroy_string_field_type);
	BT_LIB_LOGD("Created string field type object: %!+F", string_ft);
	goto end;

error:
	BT_PUT(string_ft);

end:
	return (void *) string_ft;
}

BT_HIDDEN
void _bt_field_type_freeze(struct bt_field_type *ft)
{
	/*
	 * Element/member/option field types are frozen when added to
	 * their owner.
	 */
	BT_ASSERT(ft);
	ft->frozen = true;
}

BT_HIDDEN
void _bt_field_type_make_part_of_trace(struct bt_field_type *ft)
{
	BT_ASSERT(ft);
	BT_ASSERT_PRE(!ft->part_of_trace,
		"Field type is already part of a trace: %!+F", ft);
	ft->part_of_trace = true;

	switch (ft->id) {
	case BT_FIELD_TYPE_ID_STRUCTURE:
	case BT_FIELD_TYPE_ID_VARIANT:
	{
		struct bt_field_type_named_field_types_container *container_ft =
			(void *) ft;
		uint64_t i;

		for (i = 0; i < container_ft->named_fts->len; i++) {
			struct bt_named_field_type *named_ft =
				BT_FIELD_TYPE_NAMED_FT_AT_INDEX(
					container_ft, i);

			bt_field_type_make_part_of_trace(named_ft->ft);
		}

		break;
	}
	case BT_FIELD_TYPE_ID_STATIC_ARRAY:
	case BT_FIELD_TYPE_ID_DYNAMIC_ARRAY:
	{
		struct bt_field_type_array *array_ft = (void *) ft;

		bt_field_type_make_part_of_trace(array_ft->element_ft);
		break;
	}
	default:
		break;
	}
}
