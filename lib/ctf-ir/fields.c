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

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/ctf-ir/fields-internal.h>
#include <babeltrace/ctf-ir/field-types-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/compat/fcntl-internal.h>
#include <babeltrace/align-internal.h>
#include <babeltrace/assert-internal.h>
#include <inttypes.h>

static
struct bt_field_common *bt_field_integer_copy(struct bt_field_common *src);

static
struct bt_field_common *bt_field_enumeration_copy_recursive(
		struct bt_field_common *src);

static
struct bt_field_common *bt_field_floating_point_copy(
		struct bt_field_common *src);

static
struct bt_field_common *bt_field_structure_copy_recursive(
		struct bt_field_common *src);

static
struct bt_field_common *bt_field_variant_copy_recursive(
		struct bt_field_common *src);

static
struct bt_field_common *bt_field_array_copy_recursive(
		struct bt_field_common *src);

static
struct bt_field_common *bt_field_sequence_copy_recursive(
		struct bt_field_common *src);

static
struct bt_field_common *bt_field_string_copy(struct bt_field_common *src);

static struct bt_field_common_methods bt_field_integer_methods = {
	.freeze = bt_field_common_generic_freeze,
	.validate = bt_field_common_generic_validate,
	.copy = bt_field_integer_copy,
	.is_set = bt_field_common_generic_is_set,
	.reset = bt_field_common_generic_reset,
};

static struct bt_field_common_methods bt_field_floating_point_methods = {
	.freeze = bt_field_common_generic_freeze,
	.validate = bt_field_common_generic_validate,
	.copy = bt_field_floating_point_copy,
	.is_set = bt_field_common_generic_is_set,
	.reset = bt_field_common_generic_reset,
};

static struct bt_field_common_methods bt_field_enumeration_methods = {
	.freeze = bt_field_common_enumeration_freeze_recursive,
	.validate = bt_field_common_enumeration_validate_recursive,
	.copy = bt_field_enumeration_copy_recursive,
	.is_set = bt_field_common_enumeration_is_set_recursive,
	.reset = bt_field_common_enumeration_reset_recursive,
};

static struct bt_field_common_methods bt_field_string_methods = {
	.freeze = bt_field_common_generic_freeze,
	.validate = bt_field_common_generic_validate,
	.copy = bt_field_string_copy,
	.is_set = bt_field_common_generic_is_set,
	.reset = bt_field_common_string_reset,
};

static struct bt_field_common_methods bt_field_structure_methods = {
	.freeze = bt_field_common_structure_freeze_recursive,
	.validate = bt_field_common_structure_validate_recursive,
	.copy = bt_field_structure_copy_recursive,
	.is_set = bt_field_common_structure_is_set_recursive,
	.reset = bt_field_common_structure_reset_recursive,
};

static struct bt_field_common_methods bt_field_sequence_methods = {
	.freeze = bt_field_common_sequence_freeze_recursive,
	.validate = bt_field_common_sequence_validate_recursive,
	.copy = bt_field_sequence_copy_recursive,
	.is_set = bt_field_common_sequence_is_set_recursive,
	.reset = bt_field_common_sequence_reset_recursive,
};

static struct bt_field_common_methods bt_field_array_methods = {
	.freeze = bt_field_common_array_freeze_recursive,
	.validate = bt_field_common_array_validate_recursive,
	.copy = bt_field_array_copy_recursive,
	.is_set = bt_field_common_array_is_set_recursive,
	.reset = bt_field_common_array_reset_recursive,
};

static struct bt_field_common_methods bt_field_variant_methods = {
	.freeze = bt_field_common_variant_freeze_recursive,
	.validate = bt_field_common_variant_validate_recursive,
	.copy = bt_field_variant_copy_recursive,
	.is_set = bt_field_common_variant_is_set_recursive,
	.reset = bt_field_common_variant_reset_recursive,
};

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
struct bt_field *(* const field_create_funcs[])(struct bt_field_type *) = {
	[BT_FIELD_TYPE_ID_INTEGER] =	bt_field_integer_create,
	[BT_FIELD_TYPE_ID_ENUM] =	bt_field_enumeration_create,
	[BT_FIELD_TYPE_ID_FLOAT] =	bt_field_floating_point_create,
	[BT_FIELD_TYPE_ID_STRUCT] =	bt_field_structure_create,
	[BT_FIELD_TYPE_ID_VARIANT] =	bt_field_variant_create,
	[BT_FIELD_TYPE_ID_ARRAY] =	bt_field_array_create,
	[BT_FIELD_TYPE_ID_SEQUENCE] =	bt_field_sequence_create,
	[BT_FIELD_TYPE_ID_STRING] =	bt_field_string_create,
};

struct bt_field *bt_field_create(struct bt_field_type *type)
{
	struct bt_field *field = NULL;
	enum bt_field_type_id type_id;

	BT_ASSERT_PRE_NON_NULL(type, "Field type");
	BT_ASSERT(field_type_common_has_known_id((void *) type));
	BT_ASSERT_PRE(bt_field_type_common_validate((void *) type) == 0,
		"Field type is invalid: %!+F", type);
	type_id = bt_field_type_get_type_id(type);
	field = field_create_funcs[type_id](type);
	if (!field) {
		goto end;
	}

	bt_field_type_freeze(type);

end:
	return field;
}

struct bt_field_type *bt_field_borrow_type(struct bt_field *field)
{
	return (void *) bt_field_common_borrow_type((void *) field);
}

enum bt_field_type_id bt_field_get_type_id(struct bt_field *field)
{
	struct bt_field_common *field_common = (void *) field;

	BT_ASSERT_PRE_NON_NULL(field, "Field");
	return field_common->type->id;
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
	return bt_field_common_sequence_get_int_length((void *) field);
}

struct bt_field *bt_field_sequence_borrow_length(struct bt_field *field)
{
	return (void *) bt_field_common_sequence_borrow_length((void *) field);
}

int bt_field_sequence_set_length(struct bt_field *field,
		struct bt_field *length_field)
{
	return bt_field_common_sequence_set_length((void *) field,
		(void *) length_field);
}

struct bt_field *bt_field_structure_borrow_field_by_index(
		struct bt_field *field, uint64_t index)
{
	return (void *) bt_field_common_structure_borrow_field_by_index(
		(void *) field, index);
}

struct bt_field *bt_field_structure_borrow_field_by_name(
		struct bt_field *field, const char *name)
{
	return (void *) bt_field_common_structure_borrow_field_by_name(
		(void *) field, name);
}

int bt_field_structure_set_field_by_name(struct bt_field_common *field,
		const char *name, struct bt_field_common *value)
{
	return bt_field_common_structure_set_field_by_name((void *) field,
		name, (void *) value);
}

struct bt_field *bt_field_array_borrow_field(
		struct bt_field *field, uint64_t index)
{
	return (void *) bt_field_common_array_borrow_field((void *) field,
		index, (bt_field_common_create_func) bt_field_create);
}

struct bt_field *bt_field_sequence_borrow_field(
		struct bt_field *field, uint64_t index)
{
	return (void *) bt_field_common_sequence_borrow_field((void *) field,
		index, (bt_field_common_create_func) bt_field_create);
}

struct bt_field *bt_field_variant_borrow_field(struct bt_field *field,
		struct bt_field *tag_field)
{
	return (void *) bt_field_common_variant_borrow_field((void *) field,
		(void *) tag_field,
		(bt_field_common_create_func) bt_field_create);
}

struct bt_field *bt_field_variant_borrow_current_field(
		struct bt_field *variant_field)
{
	return (void *) bt_field_common_variant_borrow_current_field(
		(void *) variant_field);
}

struct bt_field_common *bt_field_variant_borrow_tag(
		struct bt_field_common *variant_field)
{
	return (void *) bt_field_common_variant_borrow_tag(
		(void *) variant_field);
}

struct bt_field *bt_field_enumeration_borrow_container(struct bt_field *field)
{
	return (void *) bt_field_common_enumeration_borrow_container(
		(void *) field, (bt_field_common_create_func) bt_field_create);
}

struct bt_field_type_enumeration_mapping_iterator *
bt_field_enumeration_get_mappings(struct bt_field *field)
{
	return bt_field_common_enumeration_get_mappings((void *) field,
		(bt_field_common_create_func) bt_field_create);
}

int bt_field_integer_signed_get_value(struct bt_field *field, int64_t *value)
{
	return bt_field_common_integer_signed_get_value((void *) field, value);
}

int bt_field_integer_signed_set_value(struct bt_field *field,
		int64_t value)
{
	return bt_field_common_integer_signed_set_value((void *) field, value);
}

int bt_field_integer_unsigned_get_value(struct bt_field *field,
		uint64_t *value)
{
	return bt_field_common_integer_unsigned_get_value((void *) field,
		value);
}

int bt_field_integer_unsigned_set_value(struct bt_field *field, uint64_t value)
{
	return bt_field_common_integer_unsigned_set_value((void *) field, value);
}

int bt_field_floating_point_get_value(struct bt_field *field,
		double *value)
{
	return bt_field_common_floating_point_get_value((void *) field, value);
}

int bt_field_floating_point_set_value(struct bt_field *field,
		double value)
{
	return bt_field_common_floating_point_set_value((void *) field, value);
}

const char *bt_field_string_get_value(struct bt_field *field)
{
	return bt_field_common_string_get_value((void *) field);
}

int bt_field_string_set_value(struct bt_field *field, const char *value)
{
	return bt_field_common_string_set_value((void *) field, value);
}

int bt_field_string_append(struct bt_field *field, const char *value)
{
	return bt_field_common_string_append((void *) field, value);
}

int bt_field_string_append_len(struct bt_field *field,
		const char *value, unsigned int length)
{
	return bt_field_common_string_append_len((void *) field, value, length);
}

BT_HIDDEN
struct bt_field_common *bt_field_common_copy(struct bt_field_common *field)
{
	struct bt_field_common *copy = NULL;

	BT_ASSERT_PRE_NON_NULL(field, "Field");
	BT_ASSERT(field_type_common_has_known_id(field->type));
	BT_ASSERT(field->methods->copy);
	copy = field->methods->copy(field);
	if (!copy) {
		BT_LOGW("Cannot create field: ft-addr=%p", field->type);
		goto end;
	}

	bt_field_common_set(copy, field->payload_set);

end:
	return copy;
}

struct bt_field *bt_field_copy(struct bt_field *field)
{
	return (void *) bt_field_common_copy((void *) field);
}

static void bt_field_common_finalize(struct bt_field_common *field)
{
	BT_ASSERT(field);
	BT_LOGD_STR("Putting field's type.");
	bt_put(field->type);
}

BT_HIDDEN
void bt_field_common_integer_destroy(struct bt_object *obj)
{
	struct bt_field_common_integer *integer = (void *) obj;

	BT_ASSERT(obj);
	bt_field_common_finalize(BT_TO_COMMON(integer));
	BT_LOGD("Destroying integer field object: addr=%p", obj);
	g_free(obj);
}

BT_HIDDEN
void bt_field_common_enumeration_destroy_recursive(struct bt_object *obj)
{
	struct bt_field_common_enumeration *enumeration = (void *) obj;

	BT_ASSERT(enumeration);
	bt_field_common_finalize(BT_TO_COMMON(enumeration));
	BT_LOGD("Destroying enumeration field object: addr=%p", obj);
	BT_LOGD_STR("Putting payload field.");
	bt_put(enumeration->payload);
	g_free(enumeration);
}

BT_HIDDEN
void bt_field_common_floating_point_destroy(struct bt_object *obj)
{
	struct bt_field_common_floating_point *floating_point = (void *) obj;

	BT_ASSERT(obj);
	bt_field_common_finalize(BT_TO_COMMON(floating_point));
	BT_LOGD("Destroying floating point number field object: addr=%p", obj);
	g_free(obj);
}

BT_HIDDEN
void bt_field_common_structure_destroy_recursive(struct bt_object *obj)
{
	struct bt_field_common_structure *structure = (void *) obj;

	BT_ASSERT(obj);
	bt_field_common_finalize(BT_TO_COMMON(structure));
	BT_LOGD("Destroying structure field object: addr=%p", obj);
	g_ptr_array_free(structure->fields, TRUE);
	g_free(structure);
}

BT_HIDDEN
void bt_field_common_variant_destroy_recursive(struct bt_object *obj)
{
	struct bt_field_common_variant *variant = (void *) obj;

	BT_ASSERT(obj);
	bt_field_common_finalize(BT_TO_COMMON(variant));
	BT_LOGD("Destroying variant field object: addr=%p", obj);
	BT_LOGD_STR("Putting tag field.");
	bt_put(variant->tag);
	BT_LOGD_STR("Putting payload field.");
	bt_put(variant->payload);
	g_free(variant);
}

BT_HIDDEN
void bt_field_common_array_destroy_recursive(struct bt_object *obj)
{
	struct bt_field_common_array *array = (void *) obj;

	BT_ASSERT(obj);
	bt_field_common_finalize(BT_TO_COMMON(array));
	BT_LOGD("Destroying array field object: addr=%p", obj);
	g_ptr_array_free(array->elements, TRUE);
	g_free(array);
}

BT_HIDDEN
void bt_field_common_sequence_destroy_recursive(struct bt_object *obj)
{
	struct bt_field_common_sequence *sequence = (void *) obj;

	BT_ASSERT(obj);
	bt_field_common_finalize(BT_TO_COMMON(sequence));
	BT_LOGD("Destroying sequence field object: addr=%p", obj);

	if (sequence->elements) {
		g_ptr_array_free(sequence->elements, TRUE);
	}

	BT_LOGD_STR("Putting length field.");
	bt_put(sequence->length);
	g_free(sequence);
}

BT_HIDDEN
void bt_field_common_string_destroy(struct bt_object *obj)
{
	struct bt_field_common_string *string = (void *) obj;

	BT_ASSERT(obj);
	bt_field_common_finalize(BT_TO_COMMON(string));
	BT_LOGD("Destroying string field object: addr=%p", obj);

	if (string->payload) {
		g_string_free(string->payload, TRUE);
	}

	g_free(string);
}

static
struct bt_field *bt_field_integer_create(struct bt_field_type *type)
{
	struct bt_field_common_integer *integer =
		g_new0(struct bt_field_common_integer, 1);

	BT_LOGD("Creating integer field object: ft-addr=%p", type);

	if (integer) {
		bt_field_common_initialize(BT_TO_COMMON(integer), (void *) type,
			bt_field_common_integer_destroy,
			&bt_field_integer_methods);
		BT_LOGD("Created integer field object: addr=%p, ft-addr=%p",
			integer, type);
	} else {
		BT_LOGE_STR("Failed to allocate one integer field.");
	}

	return (void *) integer;
}

static
struct bt_field *bt_field_enumeration_create(struct bt_field_type *type)
{
	struct bt_field_common_enumeration *enumeration = g_new0(
		struct bt_field_common_enumeration, 1);

	BT_LOGD("Creating enumeration field object: ft-addr=%p", type);

	if (enumeration) {
		bt_field_common_initialize(BT_TO_COMMON(enumeration),
			(void *) type,
			bt_field_common_enumeration_destroy_recursive,
			&bt_field_enumeration_methods);
		BT_LOGD("Created enumeration field object: addr=%p, ft-addr=%p",
			enumeration, type);
	} else {
		BT_LOGE_STR("Failed to allocate one enumeration field.");
	}

	return (void *) enumeration;
}

static
struct bt_field *bt_field_floating_point_create(struct bt_field_type *type)
{
	struct bt_field_common_floating_point *floating_point;

	BT_LOGD("Creating floating point number field object: ft-addr=%p", type);
	floating_point = g_new0(struct bt_field_common_floating_point, 1);

	if (floating_point) {
		bt_field_common_initialize(BT_TO_COMMON(floating_point),
			(void *) type,
			bt_field_common_floating_point_destroy,
			&bt_field_floating_point_methods);
		BT_LOGD("Created floating point number field object: addr=%p, ft-addr=%p",
			floating_point, type);
	} else {
		BT_LOGE_STR("Failed to allocate one floating point number field.");
	}

	return (void *) floating_point;
}

BT_HIDDEN
int bt_field_common_structure_initialize(struct bt_field_common *field,
		struct bt_field_type_common *type,
		bt_object_release_func release_func,
		struct bt_field_common_methods *methods,
		bt_field_common_create_func field_create_func)
{
	int ret = 0;
	struct bt_field_type_common_structure *structure_type =
		BT_FROM_COMMON(type);
	struct bt_field_common_structure *structure = BT_FROM_COMMON(field);
	size_t i;

	BT_LOGD("Initializing common structure field object: ft-addr=%p", type);
	bt_field_common_initialize(field, type, release_func, methods);
	structure->fields = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_put);
	g_ptr_array_set_size(structure->fields, structure_type->fields->len);

	/* Create all fields contained by the structure field. */
	for (i = 0; i < structure_type->fields->len; i++) {
		struct bt_field_common *field;
		struct structure_field_common *struct_field =
			g_ptr_array_index(structure_type->fields, i);

		field = field_create_func(struct_field->type);
		if (!field) {
			BT_LOGE("Failed to create structure field's member: name=\"%s\", index=%zu",
				g_quark_to_string(struct_field->name), i);
			ret = -1;
			goto end;
		}

		g_ptr_array_index(structure->fields, i) = field;
	}

	BT_LOGD("Initialized common structure field object: addr=%p, ft-addr=%p",
		field, type);

end:
	return ret;
}

static
struct bt_field *bt_field_structure_create(struct bt_field_type *type)
{
	struct bt_field_common_structure *structure = g_new0(
		struct bt_field_common_structure, 1);
	int iret;

	BT_LOGD("Creating structure field object: ft-addr=%p", type);

	if (!structure) {
		BT_LOGE_STR("Failed to allocate one structure field.");
		goto end;
	}

	iret = bt_field_common_structure_initialize(BT_TO_COMMON(structure),
		(void *) type, bt_field_common_structure_destroy_recursive,
		&bt_field_structure_methods,
		(bt_field_common_create_func) bt_field_create);
	if (iret) {
		BT_PUT(structure);
		goto end;
	}

	BT_LOGD("Created structure field object: addr=%p, ft-addr=%p",
		structure, type);

end:
	return (void *) structure;
}

static
struct bt_field *bt_field_variant_create(struct bt_field_type *type)
{
	struct bt_field_common_variant *variant = g_new0(
		struct bt_field_common_variant, 1);

	BT_LOGD("Creating variant field object: ft-addr=%p", type);

	if (variant) {
		bt_field_common_initialize(BT_TO_COMMON(variant),
			(void *) type,
			bt_field_common_variant_destroy_recursive,
			&bt_field_variant_methods);
		BT_LOGD("Created variant field object: addr=%p, ft-addr=%p",
			variant, type);
	} else {
		BT_LOGE_STR("Failed to allocate one variant field.");
	}

	return (void *) variant;
}

BT_HIDDEN
int bt_field_common_array_initialize(struct bt_field_common *field,
		struct bt_field_type_common *type,
		bt_object_release_func release_func,
		struct bt_field_common_methods *methods)
{
	struct bt_field_type_common_array *array_type = BT_FROM_COMMON(type);
	struct bt_field_common_array *array = BT_FROM_COMMON(field);
	unsigned int array_length;
	int ret = 0;

	BT_LOGD("Initializing common array field object: ft-addr=%p", type);
	BT_ASSERT(type);
	bt_field_common_initialize(field, type, release_func, methods);
	array_length = array_type->length;
	array->elements = g_ptr_array_sized_new(array_length);
	if (!array->elements) {
		ret = -1;
		goto end;
	}

	g_ptr_array_set_free_func(array->elements, (GDestroyNotify) bt_put);
	g_ptr_array_set_size(array->elements, array_length);
	BT_LOGD("Initialized common array field object: addr=%p, ft-addr=%p",
		field, type);

end:
	return ret;
}

static
struct bt_field *bt_field_array_create(struct bt_field_type *type)
{
	struct bt_field_common_array *array =
		g_new0(struct bt_field_common_array, 1);
	int ret;

	BT_LOGD("Creating array field object: ft-addr=%p", type);
	BT_ASSERT(type);

	if (!array) {
		BT_LOGE_STR("Failed to allocate one array field.");
		goto end;
	}

	ret = bt_field_common_array_initialize(BT_TO_COMMON(array),
		(void *) type,
		bt_field_common_array_destroy_recursive,
		&bt_field_array_methods);
	if (ret) {
		BT_PUT(array);
		goto end;
	}

	BT_LOGD("Created array field object: addr=%p, ft-addr=%p",
		array, type);

end:
	return (void *) array;
}

static
struct bt_field *bt_field_sequence_create(struct bt_field_type *type)
{
	struct bt_field_common_sequence *sequence = g_new0(
		struct bt_field_common_sequence, 1);

	BT_LOGD("Creating sequence field object: ft-addr=%p", type);

	if (sequence) {
		bt_field_common_initialize(BT_TO_COMMON(sequence),
			(void *) type,
			bt_field_common_sequence_destroy_recursive,
			&bt_field_sequence_methods);
		BT_LOGD("Created sequence field object: addr=%p, ft-addr=%p",
			sequence, type);
	} else {
		BT_LOGE_STR("Failed to allocate one sequence field.");
	}

	return (void *) sequence;
}

static
struct bt_field *bt_field_string_create(struct bt_field_type *type)
{
	struct bt_field_common_string *string = g_new0(
		struct bt_field_common_string, 1);

	BT_LOGD("Creating string field object: ft-addr=%p", type);

	if (string) {
		bt_field_common_initialize(BT_TO_COMMON(string),
			(void *) type,
			bt_field_common_string_destroy,
			&bt_field_string_methods);
		BT_LOGD("Created string field object: addr=%p, ft-addr=%p",
			string, type);
	} else {
		BT_LOGE_STR("Failed to allocate one string field.");
	}

	return (void *) string;
}

BT_HIDDEN
int bt_field_common_generic_validate(struct bt_field_common *field)
{
	return (field && field->payload_set) ? 0 : -1;
}

BT_HIDDEN
int bt_field_common_enumeration_validate_recursive(
		struct bt_field_common *field)
{
	int ret;
	struct bt_field_common_enumeration *enumeration = BT_FROM_COMMON(field);

	BT_ASSERT(field);

	if (!enumeration->payload) {
		BT_ASSERT_PRE_MSG("Invalid enumeration field: payload is not set: "
			"%!+_f", field);
		ret = -1;
		goto end;
	}

	ret = bt_field_common_validate_recursive(enumeration->payload);

end:
	return ret;
}

BT_HIDDEN
int bt_field_common_structure_validate_recursive(struct bt_field_common *field)
{
	int64_t i;
	int ret = 0;
	struct bt_field_common_structure *structure = BT_FROM_COMMON(field);

	BT_ASSERT(field);

	for (i = 0; i < structure->fields->len; i++) {
		ret = bt_field_common_validate_recursive(
			(void *) structure->fields->pdata[i]);

		if (ret) {
			int this_ret;
			const char *name;

			this_ret = bt_field_type_common_structure_borrow_field_by_index(
				field->type, &name, NULL, i);
			BT_ASSERT(this_ret == 0);
			BT_ASSERT_PRE_MSG("Invalid structure field's field: "
				"%![struct-field-]+_f, field-name=\"%s\", "
				"index=%" PRId64 ", %![field-]+_f",
				field, name, i, structure->fields->pdata[i]);
			goto end;
		}
	}

end:
	return ret;
}

BT_HIDDEN
int bt_field_common_variant_validate_recursive(struct bt_field_common *field)
{
	int ret = 0;
	struct bt_field_common_variant *variant = BT_FROM_COMMON(field);

	BT_ASSERT(field);
	ret = bt_field_common_validate_recursive(variant->payload);
	if (ret) {
		BT_ASSERT_PRE_MSG("Invalid variant field's payload field: "
			"%![variant-field-]+_f, %![payload-field-]+_f",
			field, variant->payload);
	}

	return ret;
}

BT_HIDDEN
int bt_field_common_array_validate_recursive(struct bt_field_common *field)
{
	int64_t i;
	int ret = 0;
	struct bt_field_common_array *array = BT_FROM_COMMON(field);

	BT_ASSERT(field);

	for (i = 0; i < array->elements->len; i++) {
		ret = bt_field_common_validate_recursive((void *) array->elements->pdata[i]);
		if (ret) {
			BT_ASSERT_PRE_MSG("Invalid array field's element field: "
				"%![array-field-]+_f, " PRId64 ", "
				"%![elem-field-]+_f",
				field, i, array->elements->pdata[i]);
			goto end;
		}
	}

end:
	return ret;
}

BT_HIDDEN
int bt_field_common_sequence_validate_recursive(struct bt_field_common *field)
{
	size_t i;
	int ret = 0;
	struct bt_field_common_sequence *sequence = BT_FROM_COMMON(field);

	BT_ASSERT(field);

	for (i = 0; i < sequence->elements->len; i++) {
		ret = bt_field_common_validate_recursive(
			(void *) sequence->elements->pdata[i]);
		if (ret) {
			BT_ASSERT_PRE_MSG("Invalid sequence field's element field: "
				"%![seq-field-]+_f, " PRId64 ", "
				"%![elem-field-]+_f",
				field, i, sequence->elements->pdata[i]);
			goto end;
		}
	}
end:
	return ret;
}

BT_HIDDEN
void bt_field_common_generic_reset(struct bt_field_common *field)
{
	BT_ASSERT(field);
	field->payload_set = false;
}

BT_HIDDEN
void bt_field_common_enumeration_reset_recursive(struct bt_field_common *field)
{
	struct bt_field_common_enumeration *enumeration = BT_FROM_COMMON(field);

	BT_ASSERT(field);

	if (!enumeration->payload) {
		return;
	}

	bt_field_common_reset_recursive(enumeration->payload);
}

BT_HIDDEN
void bt_field_common_structure_reset_recursive(struct bt_field_common *field)
{
	int64_t i;
	struct bt_field_common_structure *structure = BT_FROM_COMMON(field);

	BT_ASSERT(field);

	for (i = 0; i < structure->fields->len; i++) {
		struct bt_field_common *member = structure->fields->pdata[i];

		if (!member) {
			/*
			 * Structure members are lazily initialized;
			 * skip if this member has not been allocated
			 * yet.
			 */
			continue;
		}

		bt_field_common_reset_recursive(member);
	}
}

BT_HIDDEN
void bt_field_common_variant_reset_recursive(struct bt_field_common *field)
{
	struct bt_field_common_variant *variant = BT_FROM_COMMON(field);

	BT_ASSERT(field);
	BT_PUT(variant->tag);
	BT_PUT(variant->payload);
}

BT_HIDDEN
void bt_field_common_array_reset_recursive(struct bt_field_common *field)
{
	size_t i;
	struct bt_field_common_array *array = BT_FROM_COMMON(field);

	BT_ASSERT(field);

	for (i = 0; i < array->elements->len; i++) {
		struct bt_field_common *member = array->elements->pdata[i];

		if (!member) {
			/*
			 * Array elements are lazily initialized; skip
			 * if this member has not been allocated yet.
			 */
			continue;
		}

		bt_field_common_reset_recursive(member);
	}
}

BT_HIDDEN
void bt_field_common_sequence_reset_recursive(struct bt_field_common *field)
{
	struct bt_field_common_sequence *sequence = BT_FROM_COMMON(field);

	BT_ASSERT(field);

	if (sequence->elements) {
		g_ptr_array_free(sequence->elements, TRUE);
		sequence->elements = NULL;
	}

	BT_PUT(sequence->length);
}

BT_HIDDEN
void bt_field_common_string_reset(struct bt_field_common *field)
{
	struct bt_field_common_string *string = BT_FROM_COMMON(field);

	BT_ASSERT(field);
	bt_field_common_generic_reset(field);

	if (string->payload) {
		g_string_truncate(string->payload, 0);
	}
}

static
struct bt_field_common *bt_field_integer_copy(struct bt_field_common *src)
{
	struct bt_field_common_integer *integer_src = (void *) src;
	struct bt_field_common_integer *integer_dst;

	BT_LOGD("Copying integer field: src-field-addr=%p", src);
	integer_dst = (void *) bt_field_create((void *) src->type);
	if (!integer_dst) {
		goto end;
	}

	integer_dst->payload = integer_src->payload;
	BT_LOGD_STR("Copied integer field.");

end:
	return BT_TO_COMMON(integer_dst);
}

static
struct bt_field_common *bt_field_enumeration_copy_recursive(
		struct bt_field_common *src)
{
	struct bt_field_common_enumeration *enum_src = BT_FROM_COMMON(src);
	struct bt_field_common_enumeration *enum_dst;

	BT_LOGD("Copying enumeration field: src-field-addr=%p", src);
	enum_dst = (void *) bt_field_create((void *) src->type);
	if (!enum_dst) {
		goto error;
	}

	if (enum_src->payload) {
		BT_LOGD_STR("Copying enumeration field's payload field.");
		enum_dst->payload = (void *)
			bt_field_copy((void *) enum_src->payload);
		if (!enum_dst->payload) {
			BT_LOGE_STR("Cannot copy enumeration field's payload field.");
			goto error;
		}
	}

	BT_LOGD_STR("Copied enumeration field.");
	goto end;

error:
	BT_PUT(enum_dst);

end:
	return BT_TO_COMMON(enum_dst);
}

static
struct bt_field_common *bt_field_floating_point_copy(
		struct bt_field_common *src)
{
	struct bt_field_common_floating_point *float_src = BT_FROM_COMMON(src);
	struct bt_field_common_floating_point *float_dst;

	BT_LOGD("Copying floating point number field: src-field-addr=%p", src);
	float_dst = (void *) bt_field_create((void *) src->type);
	if (!float_dst) {
		goto end;
	}

	float_dst->payload = float_src->payload;
	BT_LOGD_STR("Copied floating point number field.");

end:
	return BT_TO_COMMON(float_dst);
}

static
struct bt_field_common *bt_field_structure_copy_recursive(
		struct bt_field_common *src)
{
	int64_t i;
	struct bt_field_common_structure *struct_src = BT_FROM_COMMON(src);
	struct bt_field_common_structure *struct_dst;

	BT_LOGD("Copying structure field: src-field-addr=%p", src);
	struct_dst = (void *) bt_field_create((void *) src->type);
	if (!struct_dst) {
		goto error;
	}

	g_ptr_array_set_size(struct_dst->fields, struct_src->fields->len);

	for (i = 0; i < struct_src->fields->len; i++) {
		struct bt_field_common *field =
			g_ptr_array_index(struct_src->fields, i);
		struct bt_field_common *field_copy = NULL;

		if (field) {
			BT_LOGD("Copying structure field's field: src-field-addr=%p"
				"index=%" PRId64, field, i);
			field_copy = (void *) bt_field_copy((void *) field);
			if (!field_copy) {
				BT_LOGE("Cannot copy structure field's field: "
					"src-field-addr=%p, index=%" PRId64,
					field, i);
				goto error;
			}
		}

		BT_MOVE(g_ptr_array_index(struct_dst->fields, i), field_copy);
	}

	BT_LOGD_STR("Copied structure field.");
	goto end;

error:
	BT_PUT(struct_dst);

end:
	return BT_TO_COMMON(struct_dst);
}

static
struct bt_field_common *bt_field_variant_copy_recursive(
		struct bt_field_common *src)
{
	struct bt_field_common_variant *variant_src = BT_FROM_COMMON(src);
	struct bt_field_common_variant *variant_dst;

	BT_LOGD("Copying variant field: src-field-addr=%p", src);
	variant_dst = (void *) bt_field_create((void *) src->type);
	if (!variant_dst) {
		goto end;
	}

	if (variant_src->tag) {
		BT_LOGD_STR("Copying variant field's tag field.");
		variant_dst->tag = (void *) bt_field_copy(
			(void *) variant_src->tag);
		if (!variant_dst->tag) {
			BT_LOGE_STR("Cannot copy variant field's tag field.");
			goto error;
		}
	}
	if (variant_src->payload) {
		BT_LOGD_STR("Copying variant field's payload field.");
		variant_dst->payload = (void *) bt_field_copy(
			(void *) variant_src->payload);
		if (!variant_dst->payload) {
			BT_LOGE_STR("Cannot copy variant field's payload field.");
			goto error;
		}
	}

	BT_LOGD_STR("Copied variant field.");
	goto end;

error:
	BT_PUT(variant_dst);

end:
	return BT_TO_COMMON(variant_dst);
}

static
struct bt_field_common *bt_field_array_copy_recursive(
		struct bt_field_common *src)
{
	int64_t i;
	struct bt_field_common_array *array_src = BT_FROM_COMMON(src);
	struct bt_field_common_array *array_dst;

	BT_LOGD("Copying array field: src-field-addr=%p", src);
	array_dst = (void *) bt_field_create((void *) src->type);
	if (!array_dst) {
		goto error;
	}

	g_ptr_array_set_size(array_dst->elements, array_src->elements->len);
	for (i = 0; i < array_src->elements->len; i++) {
		struct bt_field_common *field =
			g_ptr_array_index(array_src->elements, i);
		struct bt_field_common *field_copy = NULL;

		if (field) {
			BT_LOGD("Copying array field's element field: field-addr=%p, "
				"index=%" PRId64, field, i);
			field_copy = (void *) bt_field_copy((void *) field);
			if (!field_copy) {
				BT_LOGE("Cannot copy array field's element field: "
					"src-field-addr=%p, index=%" PRId64,
					field, i);
				goto error;
			}
		}

		g_ptr_array_index(array_dst->elements, i) = field_copy;
	}

	BT_LOGD_STR("Copied array field.");
	goto end;

error:
	BT_PUT(array_dst);

end:
	return BT_TO_COMMON(array_dst);
}

static
struct bt_field_common *bt_field_sequence_copy_recursive(
		struct bt_field_common *src)
{
	int ret = 0;
	int64_t i;
	struct bt_field_common_sequence *sequence_src = BT_FROM_COMMON(src);
	struct bt_field_common_sequence *sequence_dst;
	struct bt_field_common *src_length;
	struct bt_field_common *dst_length;

	BT_LOGD("Copying sequence field: src-field-addr=%p", src);
	sequence_dst = (void *) bt_field_create((void *) src->type);
	if (!sequence_dst) {
		goto error;
	}

	src_length = bt_field_common_sequence_borrow_length(src);
	if (!src_length) {
		/* no length set yet: keep destination sequence empty */
		goto end;
	}

	/* copy source length */
	BT_LOGD_STR("Copying sequence field's length field.");
	dst_length = (void *) bt_field_copy((void *) src_length);
	if (!dst_length) {
		BT_LOGE_STR("Cannot copy sequence field's length field.");
		goto error;
	}

	/* this will initialize the destination sequence's internal array */
	ret = bt_field_common_sequence_set_length(
		BT_TO_COMMON(sequence_dst), dst_length);
	bt_put(dst_length);
	if (ret) {
		BT_LOGE("Cannot set sequence field copy's length field: "
			"dst-length-field-addr=%p", dst_length);
		goto error;
	}

	BT_ASSERT(sequence_dst->elements->len == sequence_src->elements->len);

	for (i = 0; i < sequence_src->elements->len; i++) {
		struct bt_field_common *field =
			g_ptr_array_index(sequence_src->elements, i);
		struct bt_field_common *field_copy = NULL;

		if (field) {
			BT_LOGD("Copying sequence field's element field: field-addr=%p, "
				"index=%" PRId64, field, i);
			field_copy = (void *) bt_field_copy((void *) field);
			if (!field_copy) {
				BT_LOGE("Cannot copy sequence field's element field: "
					"src-field-addr=%p, index=%" PRId64,
					field, i);
				goto error;
			}
		}

		g_ptr_array_index(sequence_dst->elements, i) = field_copy;
	}

	BT_LOGD_STR("Copied sequence field.");
	goto end;

error:
	BT_PUT(sequence_dst);

end:
	return BT_TO_COMMON(sequence_dst);
}

static
struct bt_field_common *bt_field_string_copy(struct bt_field_common *src)
{
	struct bt_field_common_string *string_src = BT_FROM_COMMON(src);
	struct bt_field_common_string *string_dst;

	BT_LOGD("Copying string field: src-field-addr=%p", src);
	string_dst = (void *) bt_field_create((void *) src->type);
	if (!string_dst) {
		goto error;
	}

	if (string_src->payload) {
		string_dst->payload = g_string_new(string_src->payload->str);
		if (!string_dst->payload) {
			BT_LOGE_STR("Failed to allocate a GString.");
			goto error;
		}
	}

	BT_LOGD_STR("Copied string field.");
	goto end;

error:
	BT_PUT(string_dst);

end:
	return BT_TO_COMMON(string_dst);
}

BT_HIDDEN
void bt_field_common_generic_freeze(struct bt_field_common *field)
{
	field->frozen = true;
}

BT_HIDDEN
void bt_field_common_enumeration_freeze_recursive(struct bt_field_common *field)
{
	struct bt_field_common_enumeration *enum_field = BT_FROM_COMMON(field);

	BT_LOGD("Freezing enumeration field object: addr=%p", field);
	BT_LOGD("Freezing enumeration field object's contained payload field: payload-field-addr=%p", enum_field->payload);
	bt_field_common_freeze_recursive(enum_field->payload);
	bt_field_common_generic_freeze(field);
}

BT_HIDDEN
void bt_field_common_structure_freeze_recursive(struct bt_field_common *field)
{
	int64_t i;
	struct bt_field_common_structure *structure_field =
		BT_FROM_COMMON(field);

	BT_LOGD("Freezing structure field object: addr=%p", field);

	for (i = 0; i < structure_field->fields->len; i++) {
		struct bt_field_common *field =
			g_ptr_array_index(structure_field->fields, i);

		BT_LOGD("Freezing structure field's field: field-addr=%p, index=%" PRId64,
			field, i);
		bt_field_common_freeze_recursive(field);
	}

	bt_field_common_generic_freeze(field);
}

BT_HIDDEN
void bt_field_common_variant_freeze_recursive(struct bt_field_common *field)
{
	struct bt_field_common_variant *variant_field = BT_FROM_COMMON(field);

	BT_LOGD("Freezing variant field object: addr=%p", field);
	BT_LOGD("Freezing variant field object's tag field: tag-field-addr=%p", variant_field->tag);
	bt_field_common_freeze_recursive(variant_field->tag);
	BT_LOGD("Freezing variant field object's payload field: payload-field-addr=%p", variant_field->payload);
	bt_field_common_freeze_recursive(variant_field->payload);
	bt_field_common_generic_freeze(field);
}

BT_HIDDEN
void bt_field_common_array_freeze_recursive(struct bt_field_common *field)
{
	int64_t i;
	struct bt_field_common_array *array_field = BT_FROM_COMMON(field);

	BT_LOGD("Freezing array field object: addr=%p", field);

	for (i = 0; i < array_field->elements->len; i++) {
		struct bt_field_common *elem_field =
			g_ptr_array_index(array_field->elements, i);

		BT_LOGD("Freezing array field object's element field: "
			"element-field-addr=%p, index=%" PRId64,
			elem_field, i);
		bt_field_common_freeze_recursive(elem_field);
	}

	bt_field_common_generic_freeze(field);
}

BT_HIDDEN
void bt_field_common_sequence_freeze_recursive(struct bt_field_common *field)
{
	int64_t i;
	struct bt_field_common_sequence *sequence_field =
		BT_FROM_COMMON(field);

	BT_LOGD("Freezing sequence field object: addr=%p", field);
	BT_LOGD("Freezing sequence field object's length field: length-field-addr=%p",
		sequence_field->length);
	bt_field_common_freeze_recursive(sequence_field->length);

	for (i = 0; i < sequence_field->elements->len; i++) {
		struct bt_field_common *elem_field =
			g_ptr_array_index(sequence_field->elements, i);

		BT_LOGD("Freezing sequence field object's element field: "
			"element-field-addr=%p, index=%" PRId64,
			elem_field, i);
		bt_field_common_freeze_recursive(elem_field);
	}

	bt_field_common_generic_freeze(field);
}

BT_HIDDEN
void _bt_field_common_freeze_recursive(struct bt_field_common *field)
{
	if (!field) {
		goto end;
	}

	if (field->frozen) {
		goto end;
	}

	BT_LOGD("Freezing field object: addr=%p", field);
	BT_ASSERT(field_type_common_has_known_id(field->type));
	BT_ASSERT(field->methods->freeze);
	field->methods->freeze(field);

end:
	return;
}

BT_HIDDEN
bt_bool bt_field_common_generic_is_set(struct bt_field_common *field)
{
	return field && field->payload_set;
}

BT_HIDDEN
bt_bool bt_field_common_enumeration_is_set_recursive(
		struct bt_field_common *field)
{
	bt_bool is_set = BT_FALSE;
	struct bt_field_common_enumeration *enumeration = BT_FROM_COMMON(field);

	BT_ASSERT(field);

	if (!enumeration->payload) {
		goto end;
	}

	is_set = bt_field_common_is_set_recursive(enumeration->payload);

end:
	return is_set;
}

BT_HIDDEN
bt_bool bt_field_common_structure_is_set_recursive(
		struct bt_field_common *field)
{
	bt_bool is_set = BT_FALSE;
	size_t i;
	struct bt_field_common_structure *structure = BT_FROM_COMMON(field);

	BT_ASSERT(field);

	for (i = 0; i < structure->fields->len; i++) {
		is_set = bt_field_common_is_set_recursive(
			structure->fields->pdata[i]);
		if (!is_set) {
			goto end;
		}
	}

end:
	return is_set;
}

BT_HIDDEN
bt_bool bt_field_common_variant_is_set_recursive(struct bt_field_common *field)
{
	struct bt_field_common_variant *variant = BT_FROM_COMMON(field);

	BT_ASSERT(field);
	return bt_field_common_is_set_recursive(variant->payload);
}

BT_HIDDEN
bt_bool bt_field_common_array_is_set_recursive(struct bt_field_common *field)
{
	size_t i;
	bt_bool is_set = BT_FALSE;
	struct bt_field_common_array *array = BT_FROM_COMMON(field);

	BT_ASSERT(field);

	for (i = 0; i < array->elements->len; i++) {
		is_set = bt_field_common_is_set_recursive(array->elements->pdata[i]);
		if (!is_set) {
			goto end;
		}
	}

end:
	return is_set;
}

BT_HIDDEN
bt_bool bt_field_common_sequence_is_set_recursive(struct bt_field_common *field)
{
	size_t i;
	bt_bool is_set = BT_FALSE;
	struct bt_field_common_sequence *sequence = BT_FROM_COMMON(field);

	BT_ASSERT(field);

	if (!sequence->elements) {
		goto end;
	}

	for (i = 0; i < sequence->elements->len; i++) {
		is_set = bt_field_common_is_set_recursive(
			sequence->elements->pdata[i]);
		if (!is_set) {
			goto end;
		}
	}

end:
	return is_set;
}
