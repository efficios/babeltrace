/*
 * fields.c
 *
 * Babeltrace trace IR - Event Fields
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
#include <babeltrace/trace-ir/fields.h>
#include <babeltrace/trace-ir/fields-internal.h>
#include <babeltrace/trace-ir/field-types-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/compat/fcntl-internal.h>
#include <babeltrace/align-internal.h>
#include <babeltrace/assert-internal.h>
#include <inttypes.h>

static
void reset_single_field(struct bt_field *field);

static
void reset_array_field(struct bt_field *field);

static
void reset_structure_field(struct bt_field *field);

static
void reset_variant_field(struct bt_field *field);

static
void set_single_field_is_frozen(struct bt_field *field, bool is_frozen);

static
void set_array_field_is_frozen(struct bt_field *field, bool is_frozen);

static
void set_structure_field_is_frozen(struct bt_field *field, bool is_frozen);

static
void set_variant_field_is_frozen(struct bt_field *field, bool is_frozen);

static
bool single_field_is_set(struct bt_field *field);

static
bool array_field_is_set(struct bt_field *field);

static
bool structure_field_is_set(struct bt_field *field);

static
bool variant_field_is_set(struct bt_field *field);

static
struct bt_field_methods integer_field_methods = {
	.set_is_frozen = set_single_field_is_frozen,
	.is_set = single_field_is_set,
	.reset = reset_single_field,
};

static
struct bt_field_methods real_field_methods = {
	.set_is_frozen = set_single_field_is_frozen,
	.is_set = single_field_is_set,
	.reset = reset_single_field,
};

static
struct bt_field_methods string_field_methods = {
	.set_is_frozen = set_single_field_is_frozen,
	.is_set = single_field_is_set,
	.reset = reset_single_field,
};

static
struct bt_field_methods structure_field_methods = {
	.set_is_frozen = set_structure_field_is_frozen,
	.is_set = structure_field_is_set,
	.reset = reset_structure_field,
};

static
struct bt_field_methods array_field_methods = {
	.set_is_frozen = set_array_field_is_frozen,
	.is_set = array_field_is_set,
	.reset = reset_array_field,
};

static
struct bt_field_methods variant_field_methods = {
	.set_is_frozen = set_variant_field_is_frozen,
	.is_set = variant_field_is_set,
	.reset = reset_variant_field,
};

static
struct bt_field *create_integer_field(struct bt_field_type *);

static
struct bt_field *create_real_field(struct bt_field_type *);

static
struct bt_field *create_string_field(struct bt_field_type *);

static
struct bt_field *create_structure_field(struct bt_field_type *);

static
struct bt_field *create_static_array_field(struct bt_field_type *);

static
struct bt_field *create_dynamic_array_field(struct bt_field_type *);

static
struct bt_field *create_variant_field(struct bt_field_type *);

static
struct bt_field *(* const field_create_funcs[])(struct bt_field_type *) = {
	[BT_FIELD_TYPE_ID_UNSIGNED_INTEGER]	= create_integer_field,
	[BT_FIELD_TYPE_ID_SIGNED_INTEGER]	= create_integer_field,
	[BT_FIELD_TYPE_ID_UNSIGNED_ENUMERATION] = create_integer_field,
	[BT_FIELD_TYPE_ID_SIGNED_ENUMERATION]	= create_integer_field,
	[BT_FIELD_TYPE_ID_REAL]			= create_real_field,
	[BT_FIELD_TYPE_ID_STRING]		= create_string_field,
	[BT_FIELD_TYPE_ID_STRUCTURE]		= create_structure_field,
	[BT_FIELD_TYPE_ID_STATIC_ARRAY]		= create_static_array_field,
	[BT_FIELD_TYPE_ID_DYNAMIC_ARRAY]	= create_dynamic_array_field,
	[BT_FIELD_TYPE_ID_VARIANT]		= create_variant_field,
};

static
void destroy_integer_field(struct bt_field *field);

static
void destroy_real_field(struct bt_field *field);

static
void destroy_string_field(struct bt_field *field);

static
void destroy_structure_field(struct bt_field *field);

static
void destroy_array_field(struct bt_field *field);

static
void destroy_variant_field(struct bt_field *field);

static
void (* const field_destroy_funcs[])(struct bt_field *) = {
	[BT_FIELD_TYPE_ID_UNSIGNED_INTEGER]	= destroy_integer_field,
	[BT_FIELD_TYPE_ID_SIGNED_INTEGER]	= destroy_integer_field,
	[BT_FIELD_TYPE_ID_UNSIGNED_ENUMERATION] = destroy_integer_field,
	[BT_FIELD_TYPE_ID_SIGNED_ENUMERATION]	= destroy_integer_field,
	[BT_FIELD_TYPE_ID_REAL]			= destroy_real_field,
	[BT_FIELD_TYPE_ID_STRING]		= destroy_string_field,
	[BT_FIELD_TYPE_ID_STRUCTURE]		= destroy_structure_field,
	[BT_FIELD_TYPE_ID_STATIC_ARRAY]		= destroy_array_field,
	[BT_FIELD_TYPE_ID_DYNAMIC_ARRAY]	= destroy_array_field,
	[BT_FIELD_TYPE_ID_VARIANT]		= destroy_variant_field,
};

struct bt_field_type *bt_field_borrow_type(struct bt_field *field)
{
	BT_ASSERT_PRE_NON_NULL(field, "Field");
	return field->type;
}

enum bt_field_type_id bt_field_get_type_id(struct bt_field *field)
{
	BT_ASSERT_PRE_NON_NULL(field, "Field");
	return field->type->id;
}

BT_HIDDEN
struct bt_field *bt_field_create(struct bt_field_type *ft)
{
	struct bt_field *field = NULL;

	BT_ASSERT_PRE_NON_NULL(ft, "Field type");
	BT_ASSERT(bt_field_type_has_known_id(ft));
	field = field_create_funcs[ft->id](ft);
	if (!field) {
		BT_LIB_LOGE("Cannot create field object from field type: "
			"%![ft-]+F", ft);
		goto end;
	}

end:
	return field;
}

static inline
void init_field(struct bt_field *field, struct bt_field_type *ft,
		struct bt_field_methods *methods)
{
	BT_ASSERT(field);
	BT_ASSERT(ft);
	bt_object_init_unique(&field->base);
	field->methods = methods;
	field->type = bt_get(ft);
}

static
struct bt_field *create_integer_field(struct bt_field_type *ft)
{
	struct bt_field_integer *int_field;

	BT_LIB_LOGD("Creating integer field object: %![ft-]+F", ft);
	int_field = g_new0(struct bt_field_integer, 1);
	if (!int_field) {
		BT_LOGE_STR("Failed to allocate one integer field.");
		goto end;
	}

	init_field((void *) int_field, ft, &integer_field_methods);
	BT_LIB_LOGD("Created integer field object: %!+f", int_field);

end:
	return (void *) int_field;
}

static
struct bt_field *create_real_field(struct bt_field_type *ft)
{
	struct bt_field_real *real_field;

	BT_LIB_LOGD("Creating real field object: %![ft-]+F", ft);
	real_field = g_new0(struct bt_field_real, 1);
	if (!real_field) {
		BT_LOGE_STR("Failed to allocate one real field.");
		goto end;
	}

	init_field((void *) real_field, ft, &real_field_methods);
	BT_LIB_LOGD("Created real field object: %!+f", real_field);

end:
	return (void *) real_field;
}

static
struct bt_field *create_string_field(struct bt_field_type *ft)
{
	struct bt_field_string *string_field;

	BT_LIB_LOGD("Creating string field object: %![ft-]+F", ft);
	string_field = g_new0(struct bt_field_string, 1);
	if (!string_field) {
		BT_LOGE_STR("Failed to allocate one string field.");
		goto end;
	}

	init_field((void *) string_field, ft, &string_field_methods);
	string_field->buf = g_array_sized_new(FALSE, FALSE,
		sizeof(char), 1);
	if (!string_field->buf) {
		BT_LOGE_STR("Failed to allocate a GArray.");
		BT_PUT(string_field);
		goto end;
	}

	g_array_index(string_field->buf, char, 0) = '\0';
	BT_LIB_LOGD("Created string field object: %!+f", string_field);

end:
	return (void *) string_field;
}

static inline
int create_fields_from_named_field_types(
		struct bt_field_type_named_field_types_container *ft,
		GPtrArray **fields)
{
	int ret = 0;
	uint64_t i;

	*fields = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_field_destroy);
	if (!*fields) {
		BT_LOGE_STR("Failed to allocate a GPtrArray.");
		ret = -1;
		goto end;
	}

	g_ptr_array_set_size(*fields, ft->named_fts->len);

	for (i = 0; i < ft->named_fts->len; i++) {
		struct bt_field *field;
		struct bt_named_field_type *named_ft =
			BT_FIELD_TYPE_NAMED_FT_AT_INDEX(ft, i);

		field = bt_field_create(named_ft->ft);
		if (!field) {
			BT_LIB_LOGE("Failed to create structure member or variant option field: "
				"name=\"%s\", %![ft-]+F",
				named_ft->name->str, named_ft->ft);
			ret = -1;
			goto end;
		}

		g_ptr_array_index(*fields, i) = field;
	}

end:
	return ret;
}

static
struct bt_field *create_structure_field(struct bt_field_type *ft)
{
	struct bt_field_structure *struct_field;

	BT_LIB_LOGD("Creating structure field object: %![ft-]+F", ft);
	struct_field = g_new0(struct bt_field_structure, 1);
	if (!struct_field) {
		BT_LOGE_STR("Failed to allocate one structure field.");
		goto end;
	}

	init_field((void *) struct_field, ft, &structure_field_methods);

	if (create_fields_from_named_field_types((void *) ft,
			&struct_field->fields)) {
		BT_LIB_LOGE("Cannot create structure member fields: "
			"%![ft-]+F", ft);
		BT_PUT(struct_field);
		goto end;
	}

	BT_LIB_LOGD("Created structure field object: %!+f", struct_field);

end:
	return (void *) struct_field;
}

static
struct bt_field *create_variant_field(struct bt_field_type *ft)
{
	struct bt_field_variant *var_field;

	BT_LIB_LOGD("Creating variant field object: %![ft-]+F", ft);
	var_field = g_new0(struct bt_field_variant, 1);
	if (!var_field) {
		BT_LOGE_STR("Failed to allocate one variant field.");
		goto end;
	}

	init_field((void *) var_field, ft, &variant_field_methods);

	if (create_fields_from_named_field_types((void *) ft,
			&var_field->fields)) {
		BT_LIB_LOGE("Cannot create variant member fields: "
			"%![ft-]+F", ft);
		BT_PUT(var_field);
		goto end;
	}

	BT_LIB_LOGD("Created variant field object: %!+f", var_field);

end:
	return (void *) var_field;
}

static inline
int init_array_field_fields(struct bt_field_array *array_field)
{
	int ret = 0;
	uint64_t i;
	struct bt_field_type_array *array_ft;

	BT_ASSERT(array_field);
	array_ft = (void *) array_field->common.type;
	array_field->fields = g_ptr_array_sized_new(array_field->length);
	if (!array_field->fields) {
		BT_LOGE_STR("Failed to allocate a GPtrArray.");
		ret = -1;
		goto end;
	}

	g_ptr_array_set_free_func(array_field->fields,
		(GDestroyNotify) bt_field_destroy);
	g_ptr_array_set_size(array_field->fields, array_field->length);

	for (i = 0; i < array_field->length; i++) {
		array_field->fields->pdata[i] = bt_field_create(
			array_ft->element_ft);
		if (!array_field->fields->pdata[i]) {
			BT_LIB_LOGE("Cannot create array field's element field: "
				"index=%" PRIu64 ", %![ft-]+F", i, array_ft);
			ret = -1;
			goto end;
		}
	}

end:
	return ret;
}

static
struct bt_field *create_static_array_field(struct bt_field_type *ft)
{
	struct bt_field_type_static_array *array_ft = (void *) ft;
	struct bt_field_array *array_field;

	BT_LIB_LOGD("Creating static array field object: %![ft-]+F", ft);
	array_field = g_new0(struct bt_field_array, 1);
	if (!array_field) {
		BT_LOGE_STR("Failed to allocate one static array field.");
		goto end;
	}

	init_field((void *) array_field, ft, &array_field_methods);
	array_field->length = array_ft->length;

	if (init_array_field_fields(array_field)) {
		BT_LIB_LOGE("Cannot create static array fields: "
			"%![ft-]+F", ft);
		BT_PUT(array_field);
		goto end;
	}

	BT_LIB_LOGD("Created static array field object: %!+f", array_field);

end:
	return (void *) array_field;
}

static
struct bt_field *create_dynamic_array_field(struct bt_field_type *ft)
{
	struct bt_field_array *array_field;

	BT_LIB_LOGD("Creating dynamic array field object: %![ft-]+F", ft);
	array_field = g_new0(struct bt_field_array, 1);
	if (!array_field) {
		BT_LOGE_STR("Failed to allocate one dynamic array field.");
		goto end;
	}

	init_field((void *) array_field, ft, &array_field_methods);

	if (init_array_field_fields(array_field)) {
		BT_LIB_LOGE("Cannot create dynamic array fields: "
			"%![ft-]+F", ft);
		BT_PUT(array_field);
		goto end;
	}

	BT_LIB_LOGD("Created dynamic array field object: %!+f", array_field);

end:
	return (void *) array_field;
}

int64_t bt_field_signed_integer_get_value(struct bt_field *field)
{
	struct bt_field_integer *int_field = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "Field");
	BT_ASSERT_PRE_FIELD_IS_SET(field, "Field");
	BT_ASSERT_PRE_FIELD_IS_SIGNED_INT(field, "Field");
	return int_field->value.i;
}

void bt_field_signed_integer_set_value(struct bt_field *field, int64_t value)
{
	struct bt_field_integer *int_field = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "Field");
	BT_ASSERT_PRE_FIELD_IS_SIGNED_INT(field, "Field");
	BT_ASSERT_PRE_FIELD_HOT(field, "Field");
	BT_ASSERT_PRE(bt_util_value_is_in_range_signed(
		((struct bt_field_type_integer *) field->type)->range, value),
		"Value is out of bounds: value=%" PRId64 ", %![field-]+f, "
		"%![ft-]+F", value, field, field->type);
	int_field->value.i = value;
	bt_field_set_single(field, true);
}

uint64_t bt_field_unsigned_integer_get_value(struct bt_field *field)
{
	struct bt_field_integer *int_field = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "Field");
	BT_ASSERT_PRE_FIELD_IS_SET(field, "Field");
	BT_ASSERT_PRE_FIELD_IS_UNSIGNED_INT(field, "Field");
	return int_field->value.u;
}

void bt_field_unsigned_integer_set_value(struct bt_field *field,
		uint64_t value)
{
	struct bt_field_integer *int_field = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "Field");
	BT_ASSERT_PRE_FIELD_IS_UNSIGNED_INT(field, "Field");
	BT_ASSERT_PRE_FIELD_HOT(field, "Field");
	BT_ASSERT_PRE(bt_util_value_is_in_range_unsigned(
		((struct bt_field_type_integer *) field->type)->range, value),
		"Value is out of bounds: value=%" PRIu64 ", %![field-]+f, "
		"%![ft-]+F", value, field, field->type);
	int_field->value.u = value;
	bt_field_set_single(field, true);
}

double bt_field_real_get_value(struct bt_field *field)
{
	struct bt_field_real *real_field = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "Field");
	BT_ASSERT_PRE_FIELD_IS_SET(field, "Field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field, BT_FIELD_TYPE_ID_REAL, "Field");
	return real_field->value;
}

void bt_field_real_set_value(struct bt_field *field, double value)
{
	struct bt_field_real *real_field = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "Field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field, BT_FIELD_TYPE_ID_REAL, "Field");
	BT_ASSERT_PRE_FIELD_HOT(field, "Field");
	BT_ASSERT_PRE(
		!((struct bt_field_type_real *) field->type)->is_single_precision ||
		(double) (float) value == value,
		"Invalid value for a single-precision real number: value=%f, "
		"%![ft-]+F", value, field->type);
	real_field->value = value;
	bt_field_set_single(field, true);
}

int bt_field_unsigned_enumeration_get_mapping_labels(struct bt_field *field,
		bt_field_type_enumeration_mapping_label_array *label_array,
		uint64_t *count)
{
	struct bt_field_integer *int_field = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "Field");
	BT_ASSERT_PRE_NON_NULL(label_array, "Label array (output)");
	BT_ASSERT_PRE_NON_NULL(label_array, "Count (output)");
	BT_ASSERT_PRE_FIELD_IS_SET(field, "Field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_UNSIGNED_ENUMERATION, "Field");
	return bt_field_type_unsigned_enumeration_get_mapping_labels_by_value(
		field->type, int_field->value.u, label_array, count);
}

int bt_field_signed_enumeration_get_mapping_labels(struct bt_field *field,
		bt_field_type_enumeration_mapping_label_array *label_array,
		uint64_t *count)
{
	struct bt_field_integer *int_field = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "Field");
	BT_ASSERT_PRE_NON_NULL(label_array, "Label array (output)");
	BT_ASSERT_PRE_NON_NULL(label_array, "Count (output)");
	BT_ASSERT_PRE_FIELD_IS_SET(field, "Field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_SIGNED_ENUMERATION, "Field");
	return bt_field_type_signed_enumeration_get_mapping_labels_by_value(
		field->type, int_field->value.i, label_array, count);
}

const char *bt_field_string_get_value(struct bt_field *field)
{
	struct bt_field_string *string_field = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "Field");
	BT_ASSERT_PRE_FIELD_IS_SET(field, "Field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field, BT_FIELD_TYPE_ID_STRING,
		"Field");
	return (const char *) string_field->buf->data;
}

uint64_t bt_field_string_get_length(struct bt_field *field)
{
	struct bt_field_string *string_field = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "Field");
	BT_ASSERT_PRE_FIELD_IS_SET(field, "Field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field, BT_FIELD_TYPE_ID_STRING,
		"Field");
	return string_field->length;
}

int bt_field_string_set_value(struct bt_field *field, const char *value)
{
	BT_ASSERT_PRE_NON_NULL(field, "Field");
	BT_ASSERT_PRE_NON_NULL(value, "Value");
	BT_ASSERT_PRE_FIELD_HOT(field, "Field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field, BT_FIELD_TYPE_ID_STRING,
		"Field");
	bt_field_string_clear(field);
	return bt_field_string_append_with_length(field, value,
		(uint64_t) strlen(value));
}

int bt_field_string_append(struct bt_field *field, const char *value)
{
	return bt_field_string_append_with_length(field, value,
		(uint64_t) strlen(value));
}

int bt_field_string_append_with_length(struct bt_field *field,
		const char *value, uint64_t length)
{
	struct bt_field_string *string_field = (void *) field;
	char *data;
	uint64_t new_length;

	BT_ASSERT_PRE_NON_NULL(field, "Field");
	BT_ASSERT_PRE_NON_NULL(value, "Value");
	BT_ASSERT_PRE_FIELD_HOT(field, "Field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_STRING, "Field");

	/* Make sure no null bytes are appended */
	BT_ASSERT_PRE(memchr(value, '\0', length) == NULL,
		"String value to append contains a null character: "
		"partial-value=\"%.32s\", length=%" PRIu64, value, length);

	new_length = length + string_field->length;

	if (unlikely(new_length + 1 > string_field->buf->len)) {
		g_array_set_size(string_field->buf, new_length + 1);
	}

	data = string_field->buf->data;
	memcpy(data + string_field->length, value, length);
	((char *) string_field->buf->data)[new_length] = '\0';
	string_field->length = new_length;
	bt_field_set_single(field, true);
	return 0;
}

int bt_field_string_clear(struct bt_field *field)
{
	struct bt_field_string *string_field = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "Field");
	BT_ASSERT_PRE_FIELD_HOT(field, "Field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_STRING, "Field");
	string_field->length = 0;
	bt_field_set_single(field, true);
	return 0;
}

uint64_t bt_field_array_get_length(struct bt_field *field)
{
	struct bt_field_array *array_field = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "Field");
	BT_ASSERT_PRE_FIELD_IS_ARRAY(field, "Field");
	return array_field->length;
}

int bt_field_dynamic_array_set_length(struct bt_field *field,
		uint64_t length)
{
	int ret = 0;
	struct bt_field_array *array_field = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "Field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_DYNAMIC_ARRAY, "Field");
	BT_ASSERT_PRE_FIELD_HOT(field, "Field");

	if (unlikely(length > array_field->fields->len)) {
		/* Make more room */
		struct bt_field_type_array *array_ft;
		uint64_t cur_len = array_field->fields->len;
		uint64_t i;

		g_ptr_array_set_size(array_field->fields, length);
		array_ft = (void *) field->type;

		for (i = cur_len; i < array_field->fields->len; i++) {
			struct bt_field *elem_field = bt_field_create(
				array_ft->element_ft);

			if (!elem_field) {
				BT_LIB_LOGE("Cannot create element field for "
					"dynamic array field: "
					"index=%" PRIu64 ", "
					"%![array-field-]+f", i, field);
				ret = -1;
				goto end;
			}

			BT_ASSERT(!array_field->fields->pdata[i]);
			array_field->fields->pdata[i] = elem_field;
		}
	}

	array_field->length = length;

end:
	return ret;
}

struct bt_field *bt_field_array_borrow_element_field_by_index(
		struct bt_field *field, uint64_t index)
{
	struct bt_field_array *array_field = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "Field");
	BT_ASSERT_PRE_FIELD_IS_ARRAY(field, "Field");
	BT_ASSERT_PRE_VALID_INDEX(index, array_field->length);
	return array_field->fields->pdata[index];
}

struct bt_field *bt_field_structure_borrow_member_field_by_index(
		struct bt_field *field, uint64_t index)
{
	struct bt_field_structure *struct_field = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "Field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_STRUCTURE, "Field");
	BT_ASSERT_PRE_VALID_INDEX(index, struct_field->fields->len);
	return struct_field->fields->pdata[index];
}

struct bt_field *bt_field_structure_borrow_member_field_by_name(
		struct bt_field *field, const char *name)
{
	struct bt_field *ret_field = NULL;
	struct bt_field_type_structure *struct_ft;
	struct bt_field_structure *struct_field = (void *) field;
	gpointer orig_key;
	gpointer index;

	BT_ASSERT_PRE_NON_NULL(field, "Field");
	BT_ASSERT_PRE_NON_NULL(name, "Field name");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_STRUCTURE, "Field");
	struct_ft = (void *) field->type;

	if (!g_hash_table_lookup_extended(struct_ft->common.name_to_index, name,
			&orig_key, &index)) {
		goto end;
	}

	ret_field = struct_field->fields->pdata[GPOINTER_TO_UINT(index)];
	BT_ASSERT(ret_field);

end:
	return ret_field;
}

struct bt_field *bt_field_variant_borrow_selected_option_field(
		struct bt_field *field)
{
	struct bt_field_variant *var_field = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "Field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_VARIANT, "Field");
	BT_ASSERT_PRE(var_field->selected_field,
		"Variant field has no selected field: %!+f", field);
	return var_field->selected_field;
}

int bt_field_variant_select_option_field(struct bt_field *field,
		uint64_t index)
{
	struct bt_field_variant *var_field = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "Field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_VARIANT, "Field");
	BT_ASSERT_PRE_FIELD_HOT(field, "Field");
	BT_ASSERT_PRE_VALID_INDEX(index, var_field->fields->len);
	var_field->selected_field = var_field->fields->pdata[index];
	var_field->selected_index = index;
	return 0;
}

uint64_t bt_field_variant_get_selected_option_field_index(
		struct bt_field *field)
{
	struct bt_field_variant *var_field = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "Field");
	BT_ASSERT_PRE_FIELD_HAS_TYPE_ID(field,
		BT_FIELD_TYPE_ID_VARIANT, "Field");
	BT_ASSERT_PRE(var_field->selected_field,
		"Variant field has no selected field: %!+f", field);
	return var_field->selected_index;
}

static inline
void bt_field_finalize(struct bt_field *field)
{
	BT_ASSERT(field);
	BT_LOGD_STR("Putting field's type.");
	bt_put(field->type);
}

static
void destroy_integer_field(struct bt_field *field)
{
	BT_ASSERT(field);
	BT_LIB_LOGD("Destroying integer field object: %!+f", field);
	bt_field_finalize(field);
	g_free(field);
}

static
void destroy_real_field(struct bt_field *field)
{
	BT_ASSERT(field);
	BT_LIB_LOGD("Destroying real field object: %!+f", field);
	bt_field_finalize(field);
	g_free(field);
}

static
void destroy_structure_field(struct bt_field *field)
{
	struct bt_field_structure *struct_field = (void *) field;

	BT_ASSERT(field);
	BT_LIB_LOGD("Destroying structure field object: %!+f", field);
	bt_field_finalize(field);

	if (struct_field->fields) {
		g_ptr_array_free(struct_field->fields, TRUE);
	}

	g_free(field);
}

static
void destroy_variant_field(struct bt_field *field)
{
	struct bt_field_variant *var_field = (void *) field;

	BT_ASSERT(field);
	BT_LIB_LOGD("Destroying variant field object: %!+f", field);
	bt_field_finalize(field);

	if (var_field->fields) {
		g_ptr_array_free(var_field->fields, TRUE);
	}

	g_free(field);
}

static
void destroy_array_field(struct bt_field *field)
{
	struct bt_field_array *array_field = (void *) field;

	BT_ASSERT(field);
	BT_LIB_LOGD("Destroying array field object: %!+f", field);
	bt_field_finalize(field);

	if (array_field->fields) {
		g_ptr_array_free(array_field->fields, TRUE);
	}

	g_free(field);
}

static
void destroy_string_field(struct bt_field *field)
{
	struct bt_field_string *string_field = (void *) field;

	BT_ASSERT(field);
	BT_LIB_LOGD("Destroying string field object: %!+f", field);
	bt_field_finalize(field);

	if (string_field->buf) {
		g_array_free(string_field->buf, TRUE);
	}

	g_free(field);
}

BT_HIDDEN
void bt_field_destroy(struct bt_field *field)
{
	BT_ASSERT(field);
	BT_ASSERT(bt_field_type_has_known_id(field->type));
	field_destroy_funcs[field->type->id](field);
}

static
void reset_single_field(struct bt_field *field)
{
	BT_ASSERT(field);
	field->is_set = false;
}

static
void reset_structure_field(struct bt_field *field)
{
	uint64_t i;
	struct bt_field_structure *struct_field = (void *) field;

	BT_ASSERT(field);

	for (i = 0; i < struct_field->fields->len; i++) {
		bt_field_reset(struct_field->fields->pdata[i]);
	}
}

static
void reset_variant_field(struct bt_field *field)
{
	uint64_t i;
	struct bt_field_variant *var_field = (void *) field;

	BT_ASSERT(field);

	for (i = 0; i < var_field->fields->len; i++) {
		bt_field_reset(var_field->fields->pdata[i]);
	}
}

static
void reset_array_field(struct bt_field *field)
{
	uint64_t i;
	struct bt_field_array *array_field = (void *) field;

	BT_ASSERT(field);

	for (i = 0; i < array_field->fields->len; i++) {
		bt_field_reset(array_field->fields->pdata[i]);
	}
}

static
void set_single_field_is_frozen(struct bt_field *field, bool is_frozen)
{
	field->frozen = is_frozen;
}

static
void set_structure_field_is_frozen(struct bt_field *field, bool is_frozen)
{
	uint64_t i;
	struct bt_field_structure *struct_field = (void *) field;

	BT_LIB_LOGD("Setting structure field's frozen state: "
		"%![field-]+f, is-frozen=%d", field, is_frozen);

	for (i = 0; i < struct_field->fields->len; i++) {
		struct bt_field *member_field = struct_field->fields->pdata[i];

		BT_LIB_LOGD("Setting structure field's member field's "
			"frozen state: %![field-]+f, index=%" PRIu64,
			member_field, i);
		bt_field_set_is_frozen(member_field, is_frozen);
	}

	set_single_field_is_frozen(field, is_frozen);
}

static
void set_variant_field_is_frozen(struct bt_field *field, bool is_frozen)
{
	uint64_t i;
	struct bt_field_variant *var_field = (void *) field;

	BT_LIB_LOGD("Setting variant field's frozen state: "
		"%![field-]+f, is-frozen=%d", field, is_frozen);

	for (i = 0; i < var_field->fields->len; i++) {
		struct bt_field *option_field = var_field->fields->pdata[i];

		BT_LIB_LOGD("Setting variant field's option field's "
			"frozen state: %![field-]+f, index=%" PRIu64,
			option_field, i);
		bt_field_set_is_frozen(option_field, is_frozen);
	}

	set_single_field_is_frozen(field, is_frozen);
}

static
void set_array_field_is_frozen(struct bt_field *field, bool is_frozen)
{
	uint64_t i;
	struct bt_field_array *array_field = (void *) field;

	BT_LIB_LOGD("Setting array field's frozen state: "
		"%![field-]+f, is-frozen=%d", field, is_frozen);

	for (i = 0; i < array_field->fields->len; i++) {
		struct bt_field *elem_field = array_field->fields->pdata[i];

		BT_LIB_LOGD("Setting array field's element field's "
			"frozen state: %![field-]+f, index=%" PRIu64,
			elem_field, i);
		bt_field_set_is_frozen(elem_field, is_frozen);
	}

	set_single_field_is_frozen(field, is_frozen);
}

BT_HIDDEN
void _bt_field_set_is_frozen(struct bt_field *field,
		bool is_frozen)
{
	BT_ASSERT(field);
	BT_LIB_LOGD("Setting field object's frozen state: %!+f, is-frozen=%d",
		field, is_frozen);
	BT_ASSERT(field->methods->set_is_frozen);
	field->methods->set_is_frozen(field, is_frozen);
}

static
bool single_field_is_set(struct bt_field *field)
{
	BT_ASSERT(field);
	return field->is_set;
}

static
bool structure_field_is_set(struct bt_field *field)
{
	bool is_set = true;
	uint64_t i;
	struct bt_field_structure *struct_field = (void *) field;

	BT_ASSERT(field);

	for (i = 0; i < struct_field->fields->len; i++) {
		is_set = bt_field_is_set(struct_field->fields->pdata[i]);
		if (!is_set) {
			goto end;
		}
	}

end:
	return is_set;
}

static
bool variant_field_is_set(struct bt_field *field)
{
	struct bt_field_variant *var_field = (void *) field;
	bool is_set = false;

	BT_ASSERT(field);

	if (var_field->selected_field) {
		is_set = bt_field_is_set(var_field->selected_field);
	}

	return is_set;
}

static
bool array_field_is_set(struct bt_field *field)
{
	bool is_set = true;
	uint64_t i;
	struct bt_field_array *array_field = (void *) field;

	BT_ASSERT(field);

	for (i = 0; i < array_field->length; i++) {
		is_set = bt_field_is_set(array_field->fields->pdata[i]);
		if (!is_set) {
			goto end;
		}
	}

end:
	return is_set;
}
