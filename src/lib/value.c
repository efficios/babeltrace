/*
 * Copyright (c) 2015-2018 Philippe Proulx <pproulx@efficios.com>
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

#define BT_LOG_TAG "LIB/VALUE"
#include "lib/logging.h"

#include <stdlib.h>
#include <string.h>
#include <string.h>
#include <inttypes.h>
#include <babeltrace2/babeltrace.h>

#include "compat/compiler.h"
#include "common/common.h"
#include "compat/glib.h"
#include "lib/assert-pre.h"
#include "lib/assert-post.h"
#include "lib/value.h"
#include "common/assert.h"
#include "func-status.h"

#define BT_VALUE_TO_BOOL(_base) ((struct bt_value_bool *) (_base))
#define BT_VALUE_TO_INTEGER(_base) ((struct bt_value_integer *) (_base))
#define BT_VALUE_TO_REAL(_base) ((struct bt_value_real *) (_base))
#define BT_VALUE_TO_STRING(_base) ((struct bt_value_string *) (_base))
#define BT_VALUE_TO_ARRAY(_base) ((struct bt_value_array *) (_base))
#define BT_VALUE_TO_MAP(_base) ((struct bt_value_map *) (_base))

#define _BT_ASSERT_PRE_VALUE_IS_TYPE_COND(_value, _type)		\
	(((struct bt_value *) (_value))->type == (_type))

#define _BT_ASSERT_PRE_VALUE_IS_TYPE_FMT				\
	"Value has the wrong type ID: expected-type=%s, %![value-]+v"

#define BT_ASSERT_PRE_VALUE_IS_TYPE(_value, _type)			\
	BT_ASSERT_PRE(							\
		_BT_ASSERT_PRE_VALUE_IS_TYPE_COND((_value), (_type)),	\
		_BT_ASSERT_PRE_VALUE_IS_TYPE_FMT,			\
		bt_common_value_type_string(_type), (_value))

#define BT_ASSERT_PRE_DEV_VALUE_IS_TYPE(_value, _type)			\
	BT_ASSERT_PRE_DEV(						\
		_BT_ASSERT_PRE_VALUE_IS_TYPE_COND((_value), (_type)),	\
		_BT_ASSERT_PRE_VALUE_IS_TYPE_FMT,			\
		bt_common_value_type_string(_type), (_value))

#define BT_ASSERT_PRE_DEV_VALUE_HOT(_value, _name)			\
	BT_ASSERT_PRE_DEV_HOT(((struct bt_value *) (_value)), (_name),	\
		": %!+v", (_value))

static
void bt_value_null_instance_release_func(struct bt_object *obj)
{
	BT_LOGW("Releasing the null value singleton: addr=%p", obj);
}

static
struct bt_value bt_value_null_instance = {
	.base = {
		.is_shared = true,
		.ref_count = 1,
		.release_func = bt_value_null_instance_release_func,
		.spec_release_func = NULL,
		.parent_is_owner_listener_func = NULL,
		.parent = NULL,
	},
	.type = BT_VALUE_TYPE_NULL,
	.frozen = BT_TRUE,
};

struct bt_value *const bt_value_null = &bt_value_null_instance;

static
void bt_value_destroy(struct bt_object *obj);

static
void bt_value_string_destroy(struct bt_value *object)
{
	g_string_free(BT_VALUE_TO_STRING(object)->gstr, TRUE);
	BT_VALUE_TO_STRING(object)->gstr = NULL;
}

static
void bt_value_array_destroy(struct bt_value *object)
{
	/*
	 * Pointer array's registered value destructor will take care
	 * of putting each contained object.
	 */
	g_ptr_array_free(BT_VALUE_TO_ARRAY(object)->garray, TRUE);
	BT_VALUE_TO_ARRAY(object)->garray = NULL;
}

static
void bt_value_map_destroy(struct bt_value *object)
{
	/*
	 * Hash table's registered value destructor will take care of
	 * putting each contained object. Keys are GQuarks and cannot
	 * be destroyed anyway.
	 */
	g_hash_table_destroy(BT_VALUE_TO_MAP(object)->ght);
	BT_VALUE_TO_MAP(object)->ght = NULL;
}

static
void (* const destroy_funcs[])(struct bt_value *) = {
	[BT_VALUE_TYPE_NULL] =			NULL,
	[BT_VALUE_TYPE_BOOL] =			NULL,
	[BT_VALUE_TYPE_UNSIGNED_INTEGER] =	NULL,
	[BT_VALUE_TYPE_SIGNED_INTEGER] =	NULL,
	[BT_VALUE_TYPE_REAL] =			NULL,
	[BT_VALUE_TYPE_STRING] =		bt_value_string_destroy,
	[BT_VALUE_TYPE_ARRAY] =			bt_value_array_destroy,
	[BT_VALUE_TYPE_MAP] =			bt_value_map_destroy,
};

static
struct bt_value *bt_value_null_copy(const struct bt_value *null_obj)
{
	BT_ASSERT(null_obj == bt_value_null);

	bt_object_get_ref_no_null_check(bt_value_null);
	return (void *) bt_value_null;
}

static
struct bt_value *bt_value_bool_copy(const struct bt_value *bool_obj)
{
	return bt_value_bool_create_init(
		BT_VALUE_TO_BOOL(bool_obj)->value);
}

static inline
struct bt_value *bt_value_integer_create_init(enum bt_value_type type,
		uint64_t uval);

static
struct bt_value *bt_value_integer_copy(
		const struct bt_value *integer_obj)
{
	return bt_value_integer_create_init(integer_obj->type,
		BT_VALUE_TO_INTEGER(integer_obj)->value.u);
}

static
struct bt_value *bt_value_real_copy(const struct bt_value *real_obj)
{
	return bt_value_real_create_init(
		BT_VALUE_TO_REAL(real_obj)->value);
}

static
struct bt_value *bt_value_string_copy(const struct bt_value *string_obj)
{
	return bt_value_string_create_init(
		BT_VALUE_TO_STRING(string_obj)->gstr->str);
}

static
struct bt_value *bt_value_array_copy(const struct bt_value *array_obj)
{
	int i;
	int ret;
	struct bt_value *copy_obj;
	struct bt_value_array *typed_array_obj;

	BT_LOGD("Copying array value: addr=%p", array_obj);
	typed_array_obj = BT_VALUE_TO_ARRAY(array_obj);
	copy_obj = bt_value_array_create();
	if (!copy_obj) {
		BT_LIB_LOGE_APPEND_CAUSE("Cannot create empty array value.");
		goto end;
	}

	for (i = 0; i < typed_array_obj->garray->len; ++i) {
		struct bt_value *element_obj_copy = NULL;
		const struct bt_value *element_obj =
			bt_value_array_borrow_element_by_index_const(
				array_obj, i);

		BT_LOGD("Copying array value's element: element-addr=%p, "
			"index=%d", element_obj, i);
		ret = bt_value_copy(element_obj, &element_obj_copy);
		if (ret) {
			BT_LIB_LOGE_APPEND_CAUSE(
				"Cannot copy array value's element: "
				"array-addr=%p, index=%d",
				array_obj, i);
			BT_OBJECT_PUT_REF_AND_RESET(copy_obj);
			goto end;
		}

		BT_ASSERT(element_obj_copy);
		ret = bt_value_array_append_element(copy_obj,
			(void *) element_obj_copy);
		BT_OBJECT_PUT_REF_AND_RESET(element_obj_copy);
		if (ret) {
			BT_LIB_LOGE_APPEND_CAUSE(
				"Cannot append to array value: addr=%p",
				array_obj);
			BT_OBJECT_PUT_REF_AND_RESET(copy_obj);
			goto end;
		}
	}

	BT_LOGD("Copied array value: original-addr=%p, copy-addr=%p",
		array_obj, copy_obj);

end:
	return copy_obj;
}

static
struct bt_value *bt_value_map_copy(const struct bt_value *map_obj)
{
	int ret;
	GHashTableIter iter;
	gpointer key, element_obj;
	struct bt_value *copy_obj;
	struct bt_value *element_obj_copy = NULL;
	struct bt_value_map *typed_map_obj;

	BT_LOGD("Copying map value: addr=%p", map_obj);
	typed_map_obj = BT_VALUE_TO_MAP(map_obj);
	copy_obj = bt_value_map_create();
	if (!copy_obj) {
		goto end;
	}

	g_hash_table_iter_init(&iter, typed_map_obj->ght);

	while (g_hash_table_iter_next(&iter, &key, &element_obj)) {
		const char *key_str = g_quark_to_string(GPOINTER_TO_UINT(key));

		BT_ASSERT(key_str);
		BT_LOGD("Copying map value's element: element-addr=%p, "
			"key=\"%s\"", element_obj, key_str);
		ret = bt_value_copy(element_obj, &element_obj_copy);
		if (ret) {
			BT_LIB_LOGE_APPEND_CAUSE(
				"Cannot copy map value's element: "
				"map-addr=%p, key=\"%s\"",
				map_obj, key_str);
			BT_OBJECT_PUT_REF_AND_RESET(copy_obj);
			goto end;
		}

		BT_ASSERT(element_obj_copy);
		ret = bt_value_map_insert_entry(copy_obj, key_str,
			(void *) element_obj_copy);
		BT_OBJECT_PUT_REF_AND_RESET(element_obj_copy);
		if (ret) {
			BT_LIB_LOGE_APPEND_CAUSE(
				"Cannot insert into map value: addr=%p, key=\"%s\"",
				map_obj, key_str);
			BT_OBJECT_PUT_REF_AND_RESET(copy_obj);
			goto end;
		}
	}

	BT_LOGD("Copied map value: addr=%p", map_obj);

end:
	return copy_obj;
}

static
struct bt_value *(* const copy_funcs[])(const struct bt_value *) = {
	[BT_VALUE_TYPE_NULL] =			bt_value_null_copy,
	[BT_VALUE_TYPE_BOOL] =			bt_value_bool_copy,
	[BT_VALUE_TYPE_UNSIGNED_INTEGER] =	bt_value_integer_copy,
	[BT_VALUE_TYPE_SIGNED_INTEGER] =	bt_value_integer_copy,
	[BT_VALUE_TYPE_REAL] =			bt_value_real_copy,
	[BT_VALUE_TYPE_STRING] =		bt_value_string_copy,
	[BT_VALUE_TYPE_ARRAY] =			bt_value_array_copy,
	[BT_VALUE_TYPE_MAP] =			bt_value_map_copy,
};

static
bt_bool bt_value_null_is_equal(const struct bt_value *object_a,
		const struct bt_value *object_b)
{
	/*
	 * Always BT_TRUE since bt_value_is_equal() already checks if both
	 * object_a and object_b have the same type, and in the case of
	 * null value objects, they're always the same if it is so.
	 */
	return BT_TRUE;
}

static
bt_bool bt_value_bool_is_equal(const struct bt_value *object_a,
		const struct bt_value *object_b)
{
	if (BT_VALUE_TO_BOOL(object_a)->value !=
			BT_VALUE_TO_BOOL(object_b)->value) {
		BT_LOGT("Boolean value objects are different: "
			"bool-a-val=%d, bool-b-val=%d",
			BT_VALUE_TO_BOOL(object_a)->value,
			BT_VALUE_TO_BOOL(object_b)->value);
		return BT_FALSE;
	}

	return BT_TRUE;
}

static
bt_bool bt_value_integer_is_equal(const struct bt_value *object_a,
		const struct bt_value *object_b)
{
	if (BT_VALUE_TO_INTEGER(object_a)->value.u !=
			BT_VALUE_TO_INTEGER(object_b)->value.u) {
		if (object_a->type == BT_VALUE_TYPE_UNSIGNED_INTEGER) {
			BT_LOGT("Unsigned integer value objects are different: "
				"int-a-val=%" PRIu64 ", int-b-val=%" PRIu64,
				BT_VALUE_TO_INTEGER(object_a)->value.u,
				BT_VALUE_TO_INTEGER(object_b)->value.u);
		} else {
			BT_LOGT("Signed integer value objects are different: "
				"int-a-val=%" PRId64 ", int-b-val=%" PRId64,
				BT_VALUE_TO_INTEGER(object_a)->value.i,
				BT_VALUE_TO_INTEGER(object_b)->value.i);
		}

		return BT_FALSE;
	}

	return BT_TRUE;
}

static
bt_bool bt_value_real_is_equal(const struct bt_value *object_a,
		const struct bt_value *object_b)
{
	if (BT_VALUE_TO_REAL(object_a)->value !=
			BT_VALUE_TO_REAL(object_b)->value) {
		BT_LOGT("Real number value objects are different: "
			"real-a-val=%f, real-b-val=%f",
			BT_VALUE_TO_REAL(object_a)->value,
			BT_VALUE_TO_REAL(object_b)->value);
		return BT_FALSE;
	}

	return BT_TRUE;
}

static
bt_bool bt_value_string_is_equal(const struct bt_value *object_a,
		const struct bt_value *object_b)
{
	if (strcmp(BT_VALUE_TO_STRING(object_a)->gstr->str,
			BT_VALUE_TO_STRING(object_b)->gstr->str) != 0) {
		BT_LOGT("String value objects are different: "
			"string-a-val=\"%s\", string-b-val=\"%s\"",
			BT_VALUE_TO_STRING(object_a)->gstr->str,
			BT_VALUE_TO_STRING(object_b)->gstr->str);
		return BT_FALSE;
	}

	return BT_TRUE;
}

static
bt_bool bt_value_array_is_equal(const struct bt_value *object_a,
		const struct bt_value *object_b)
{
	int i;
	bt_bool ret = BT_TRUE;
	const struct bt_value_array *array_obj_a =
		BT_VALUE_TO_ARRAY(object_a);

	if (bt_value_array_get_length(object_a) !=
			bt_value_array_get_length(object_b)) {
		BT_LOGT("Array values are different: size mismatch "
			"value-a-addr=%p, value-b-addr=%p, "
			"value-a-size=%" PRId64 ", value-b-size=%" PRId64,
			object_a, object_b,
			bt_value_array_get_length(object_a),
			bt_value_array_get_length(object_b));
		ret = BT_FALSE;
		goto end;
	}

	for (i = 0; i < array_obj_a->garray->len; ++i) {
		const struct bt_value *element_obj_a;
		const struct bt_value *element_obj_b;

		element_obj_a = bt_value_array_borrow_element_by_index_const(
			object_a, i);
		element_obj_b = bt_value_array_borrow_element_by_index_const(
			object_b, i);

		if (!bt_value_is_equal(element_obj_a, element_obj_b)) {
			BT_LOGT("Array values's elements are different: "
				"value-a-addr=%p, value-b-addr=%p, index=%d",
				element_obj_a, element_obj_b, i);
			ret = BT_FALSE;
			goto end;
		}
	}

end:
	return ret;
}

static
bt_bool bt_value_map_is_equal(const struct bt_value *object_a,
		const struct bt_value *object_b)
{
	bt_bool ret = BT_TRUE;
	GHashTableIter iter;
	gpointer key, element_obj_a;
	const struct bt_value_map *map_obj_a = BT_VALUE_TO_MAP(object_a);

	if (bt_value_map_get_size(object_a) !=
			bt_value_map_get_size(object_b)) {
		BT_LOGT("Map values are different: size mismatch "
			"value-a-addr=%p, value-b-addr=%p, "
			"value-a-size=%" PRId64 ", value-b-size=%" PRId64,
			object_a, object_b,
			bt_value_map_get_size(object_a),
			bt_value_map_get_size(object_b));
		ret = BT_FALSE;
		goto end;
	}

	g_hash_table_iter_init(&iter, map_obj_a->ght);

	while (g_hash_table_iter_next(&iter, &key, &element_obj_a)) {
		const struct bt_value *element_obj_b;
		const char *key_str = g_quark_to_string(GPOINTER_TO_UINT(key));

		element_obj_b = bt_value_map_borrow_entry_value_const(object_b,
			key_str);

		if (!bt_value_is_equal(element_obj_a, element_obj_b)) {
			BT_LOGT("Map values's elements are different: "
				"value-a-addr=%p, value-b-addr=%p, key=\"%s\"",
				element_obj_a, element_obj_b, key_str);
			ret = BT_FALSE;
			goto end;
		}
	}

end:
	return ret;
}

static
bt_bool (* const is_equal_funcs[])(const struct bt_value *,
		const struct bt_value *) = {
	[BT_VALUE_TYPE_NULL] =			bt_value_null_is_equal,
	[BT_VALUE_TYPE_BOOL] =			bt_value_bool_is_equal,
	[BT_VALUE_TYPE_UNSIGNED_INTEGER] =	bt_value_integer_is_equal,
	[BT_VALUE_TYPE_SIGNED_INTEGER] =	bt_value_integer_is_equal,
	[BT_VALUE_TYPE_REAL] =			bt_value_real_is_equal,
	[BT_VALUE_TYPE_STRING] =		bt_value_string_is_equal,
	[BT_VALUE_TYPE_ARRAY] =			bt_value_array_is_equal,
	[BT_VALUE_TYPE_MAP] =			bt_value_map_is_equal,
};

static
void bt_value_null_freeze(struct bt_value *object)
{
}

static
void bt_value_generic_freeze(struct bt_value *object)
{
	object->frozen = BT_TRUE;
}

static
void bt_value_array_freeze(struct bt_value *object)
{
	int i;
	struct bt_value_array *typed_array_obj =
		BT_VALUE_TO_ARRAY(object);

	for (i = 0; i < typed_array_obj->garray->len; ++i) {
		bt_value_freeze(g_ptr_array_index(typed_array_obj->garray, i));
	}

	bt_value_generic_freeze(object);
}

static
void bt_value_map_freeze(struct bt_value *object)
{
	GHashTableIter iter;
	gpointer key, element_obj;
	const struct bt_value_map *map_obj = BT_VALUE_TO_MAP(object);

	g_hash_table_iter_init(&iter, map_obj->ght);

	while (g_hash_table_iter_next(&iter, &key, &element_obj)) {
		bt_value_freeze(element_obj);
	}

	bt_value_generic_freeze(object);
}

static
void (* const freeze_funcs[])(struct bt_value *) = {
	[BT_VALUE_TYPE_NULL] =			bt_value_null_freeze,
	[BT_VALUE_TYPE_BOOL] =			bt_value_generic_freeze,
	[BT_VALUE_TYPE_UNSIGNED_INTEGER] =	bt_value_generic_freeze,
	[BT_VALUE_TYPE_SIGNED_INTEGER] =	bt_value_generic_freeze,
	[BT_VALUE_TYPE_REAL] =			bt_value_generic_freeze,
	[BT_VALUE_TYPE_STRING] =		bt_value_generic_freeze,
	[BT_VALUE_TYPE_ARRAY] =			bt_value_array_freeze,
	[BT_VALUE_TYPE_MAP] =			bt_value_map_freeze,
};

static
void bt_value_destroy(struct bt_object *obj)
{
	struct bt_value *value;

	value = container_of(obj, struct bt_value, base);
	BT_LOGD("Destroying value: addr=%p", value);

	if (bt_value_is_null(value)) {
		BT_LOGD_STR("Not destroying the null value singleton.");
		return;
	}

	if (destroy_funcs[value->type]) {
		destroy_funcs[value->type](value);
	}

	g_free(value);
}

BT_HIDDEN
void _bt_value_freeze(const struct bt_value *c_object)
{
	const struct bt_value *object = (void *) c_object;

	BT_ASSERT(object);

	if (object->frozen) {
		goto end;
	}

	BT_LOGD("Freezing value: addr=%p", object);
	freeze_funcs[object->type]((void *) object);

end:
	return;
}

enum bt_value_type bt_value_get_type(const struct bt_value *object)
{
	BT_ASSERT_PRE_DEV_NON_NULL(object, "Value object");
	return object->type;
}

static
struct bt_value bt_value_create_base(enum bt_value_type type)
{
	struct bt_value value;

	value.type = type;
	value.frozen = BT_FALSE;
	bt_object_init_shared(&value.base, bt_value_destroy);
	return value;
}

struct bt_value *bt_value_bool_create_init(bt_bool val)
{
	struct bt_value_bool *bool_obj;

	BT_ASSERT_PRE_NO_ERROR();

	BT_LOGD("Creating boolean value object: val=%d", val);
	bool_obj = g_new0(struct bt_value_bool, 1);
	if (!bool_obj) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate one boolean value object.");
		goto end;
	}

	bool_obj->base = bt_value_create_base(BT_VALUE_TYPE_BOOL);
	bool_obj->value = val;
	BT_LOGD("Created boolean value object: addr=%p", bool_obj);

end:
	return (void *) bool_obj;
}

struct bt_value *bt_value_bool_create(void)
{
	BT_ASSERT_PRE_NO_ERROR();

	return bt_value_bool_create_init(BT_FALSE);
}

static inline
struct bt_value *bt_value_integer_create_init(enum bt_value_type type,
		uint64_t uval)
{
	struct bt_value_integer *integer_obj;

	BT_ASSERT(type == BT_VALUE_TYPE_UNSIGNED_INTEGER ||
		type == BT_VALUE_TYPE_SIGNED_INTEGER);

	if (type == BT_VALUE_TYPE_UNSIGNED_INTEGER) {
		BT_LOGD("Creating unsigned integer value object: val=%" PRIu64,
			uval);
	} else {
		BT_LOGD("Creating signed integer value object: val=%" PRId64,
			(int64_t) uval);
	}

	integer_obj = g_new0(struct bt_value_integer, 1);
	if (!integer_obj) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate one integer value object.");
		goto end;
	}

	integer_obj->base = bt_value_create_base(type);
	integer_obj->value.u = uval;
	BT_LOGD("Created %ssigned integer value object: addr=%p",
		type == BT_VALUE_TYPE_UNSIGNED_INTEGER ? "un" : "",
		integer_obj);

end:
	return (void *) integer_obj;
}

struct bt_value *bt_value_integer_unsigned_create_init(uint64_t val)
{
	BT_ASSERT_PRE_NO_ERROR();

	return bt_value_integer_create_init(BT_VALUE_TYPE_UNSIGNED_INTEGER,
		val);
}

struct bt_value *bt_value_integer_unsigned_create(void)
{
	BT_ASSERT_PRE_NO_ERROR();

	return bt_value_integer_unsigned_create_init(0);
}

struct bt_value *bt_value_integer_signed_create_init(int64_t val)
{
	BT_ASSERT_PRE_NO_ERROR();

	return bt_value_integer_create_init(BT_VALUE_TYPE_SIGNED_INTEGER,
		(uint64_t) val);
}

struct bt_value *bt_value_integer_signed_create(void)
{
	BT_ASSERT_PRE_NO_ERROR();

	return bt_value_integer_signed_create_init(0);
}

struct bt_value *bt_value_real_create_init(double val)
{
	struct bt_value_real *real_obj;

	BT_ASSERT_PRE_NO_ERROR();

	BT_LOGD("Creating real number value object: val=%f", val);
	real_obj = g_new0(struct bt_value_real, 1);
	if (!real_obj) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate one real number value object.");
		goto end;
	}

	real_obj->base = bt_value_create_base(BT_VALUE_TYPE_REAL);
	real_obj->value = val;
	BT_LOGD("Created real number value object: addr=%p",
		real_obj);

end:
	return (void *) real_obj;
}

struct bt_value *bt_value_real_create(void)
{
	BT_ASSERT_PRE_NO_ERROR();

	return bt_value_real_create_init(0.);
}

struct bt_value *bt_value_string_create_init(const char *val)
{
	struct bt_value_string *string_obj = NULL;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_NON_NULL(val, "Value");

	BT_LOGD("Creating string value object: val-len=%zu", strlen(val));
	string_obj = g_new0(struct bt_value_string, 1);
	if (!string_obj) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate one string object.");
		goto end;
	}

	string_obj->base = bt_value_create_base(BT_VALUE_TYPE_STRING);
	string_obj->gstr = g_string_new(val);
	if (!string_obj->gstr) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate a GString.");
		g_free(string_obj);
		string_obj = NULL;
		goto end;
	}

	BT_LOGD("Created string value object: addr=%p",
		string_obj);

end:
	return (void *) string_obj;
}

struct bt_value *bt_value_string_create(void)
{
	BT_ASSERT_PRE_NO_ERROR();

	return bt_value_string_create_init("");
}

struct bt_value *bt_value_array_create(void)
{
	struct bt_value_array *array_obj;

	BT_ASSERT_PRE_NO_ERROR();

	BT_LOGD_STR("Creating empty array value object.");
	array_obj = g_new0(struct bt_value_array, 1);
	if (!array_obj) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate one array object.");
		goto end;
	}

	array_obj->base = bt_value_create_base(BT_VALUE_TYPE_ARRAY);
	array_obj->garray = bt_g_ptr_array_new_full(0,
		(GDestroyNotify) bt_object_put_ref);
	if (!array_obj->garray) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate a GPtrArray.");
		g_free(array_obj);
		array_obj = NULL;
		goto end;
	}

	BT_LOGD("Created array value object: addr=%p",
		array_obj);

end:
	return (void *) array_obj;
}

struct bt_value *bt_value_map_create(void)
{
	struct bt_value_map *map_obj;

	BT_ASSERT_PRE_NO_ERROR();

	BT_LOGD_STR("Creating empty map value object.");
	map_obj = g_new0(struct bt_value_map, 1);
	if (!map_obj) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate one map object.");
		goto end;
	}

	map_obj->base = bt_value_create_base(BT_VALUE_TYPE_MAP);
	map_obj->ght = g_hash_table_new_full(g_direct_hash, g_direct_equal,
		NULL, (GDestroyNotify) bt_object_put_ref);
	if (!map_obj->ght) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate a GHashTable.");
		g_free(map_obj);
		map_obj = NULL;
		goto end;
	}

	BT_LOGD("Created map value object: addr=%p",
		map_obj);

end:
	return (void *) map_obj;
}

bt_bool bt_value_bool_get(const struct bt_value *bool_obj)
{
	BT_ASSERT_PRE_DEV_NON_NULL(bool_obj, "Value object");
	BT_ASSERT_PRE_DEV_VALUE_IS_TYPE(bool_obj, BT_VALUE_TYPE_BOOL);
	return BT_VALUE_TO_BOOL(bool_obj)->value;
}

void bt_value_bool_set(struct bt_value *bool_obj, bt_bool val)
{
	BT_ASSERT_PRE_NON_NULL(bool_obj, "Value object");
	BT_ASSERT_PRE_VALUE_IS_TYPE(bool_obj, BT_VALUE_TYPE_BOOL);
	BT_ASSERT_PRE_DEV_VALUE_HOT(bool_obj, "Value object");
	BT_VALUE_TO_BOOL(bool_obj)->value = val;
	BT_LOGT("Set boolean value's raw value: value-addr=%p, value=%d",
		bool_obj, val);
}

uint64_t bt_value_integer_unsigned_get(const struct bt_value *integer_obj)
{
	BT_ASSERT_PRE_DEV_NON_NULL(integer_obj, "Value object");
	BT_ASSERT_PRE_DEV_VALUE_IS_TYPE(integer_obj,
		BT_VALUE_TYPE_UNSIGNED_INTEGER);
	return BT_VALUE_TO_INTEGER(integer_obj)->value.u;
}

int64_t bt_value_integer_signed_get(const struct bt_value *integer_obj)
{
	BT_ASSERT_PRE_DEV_NON_NULL(integer_obj, "Value object");
	BT_ASSERT_PRE_DEV_VALUE_IS_TYPE(integer_obj,
		BT_VALUE_TYPE_SIGNED_INTEGER);
	return BT_VALUE_TO_INTEGER(integer_obj)->value.i;
}

static inline
void bt_value_integer_set(struct bt_value *integer_obj,
		enum bt_value_type expected_type, uint64_t uval)
{
	BT_ASSERT_PRE_NON_NULL(integer_obj, "Value object");
	BT_ASSERT_PRE_VALUE_IS_TYPE(integer_obj, expected_type);
	BT_ASSERT_PRE_DEV_VALUE_HOT(integer_obj, "Value object");
	BT_VALUE_TO_INTEGER(integer_obj)->value.u = uval;
}

void bt_value_integer_unsigned_set(struct bt_value *integer_obj,
		uint64_t val)
{
	bt_value_integer_set(integer_obj, BT_VALUE_TYPE_UNSIGNED_INTEGER, val);
	BT_LOGT("Set unsigned integer value's raw value: "
		"value-addr=%p, value=%" PRIu64, integer_obj, val);
}

void bt_value_integer_signed_set(struct bt_value *integer_obj,
		int64_t val)
{
	bt_value_integer_set(integer_obj, BT_VALUE_TYPE_SIGNED_INTEGER,
		(uint64_t) val);
	BT_LOGT("Set signed integer value's raw value: "
		"value-addr=%p, value=%" PRId64, integer_obj, val);
}

double bt_value_real_get(const struct bt_value *real_obj)
{
	BT_ASSERT_PRE_DEV_NON_NULL(real_obj, "Value object");
	BT_ASSERT_PRE_DEV_VALUE_IS_TYPE(real_obj, BT_VALUE_TYPE_REAL);
	return BT_VALUE_TO_REAL(real_obj)->value;
}

void bt_value_real_set(struct bt_value *real_obj, double val)
{
	BT_ASSERT_PRE_NON_NULL(real_obj, "Value object");
	BT_ASSERT_PRE_VALUE_IS_TYPE(real_obj, BT_VALUE_TYPE_REAL);
	BT_ASSERT_PRE_DEV_VALUE_HOT(real_obj, "Value object");
	BT_VALUE_TO_REAL(real_obj)->value = val;
	BT_LOGT("Set real number value's raw value: value-addr=%p, value=%f",
		real_obj, val);
}

const char *bt_value_string_get(const struct bt_value *string_obj)
{
	BT_ASSERT_PRE_DEV_NON_NULL(string_obj, "Value object");
	BT_ASSERT_PRE_DEV_VALUE_IS_TYPE(string_obj, BT_VALUE_TYPE_STRING);
	return BT_VALUE_TO_STRING(string_obj)->gstr->str;
}

enum bt_value_string_set_status bt_value_string_set(
		struct bt_value *string_obj, const char *val)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_NON_NULL(string_obj, "Value object");
	BT_ASSERT_PRE_VALUE_IS_TYPE(string_obj, BT_VALUE_TYPE_STRING);
	BT_ASSERT_PRE_DEV_VALUE_HOT(string_obj, "Value object");
	g_string_assign(BT_VALUE_TO_STRING(string_obj)->gstr, val);
	BT_LOGT("Set string value's raw value: value-addr=%p, raw-value-addr=%p",
		string_obj, val);
	return BT_FUNC_STATUS_OK;
}

uint64_t bt_value_array_get_length(const struct bt_value *array_obj)
{
	BT_ASSERT_PRE_DEV_NON_NULL(array_obj, "Value object");
	BT_ASSERT_PRE_DEV_VALUE_IS_TYPE(array_obj, BT_VALUE_TYPE_ARRAY);
	return (uint64_t) BT_VALUE_TO_ARRAY(array_obj)->garray->len;
}

struct bt_value *bt_value_array_borrow_element_by_index(
		struct bt_value *array_obj, uint64_t index)
{
	struct bt_value_array *typed_array_obj =
		BT_VALUE_TO_ARRAY(array_obj);

	BT_ASSERT_PRE_DEV_NON_NULL(array_obj, "Value object");
	BT_ASSERT_PRE_DEV_VALUE_IS_TYPE(array_obj, BT_VALUE_TYPE_ARRAY);
	BT_ASSERT_PRE_DEV_VALID_INDEX(index, typed_array_obj->garray->len);
	return g_ptr_array_index(typed_array_obj->garray, index);
}

const struct bt_value *bt_value_array_borrow_element_by_index_const(
		const struct bt_value *array_obj,
		uint64_t index)
{
	return bt_value_array_borrow_element_by_index(
		(void *) array_obj, index);
}

enum bt_value_array_append_element_status bt_value_array_append_element(
		struct bt_value *array_obj,
		struct bt_value *element_obj)
{
	struct bt_value_array *typed_array_obj =
		BT_VALUE_TO_ARRAY(array_obj);

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_NON_NULL(array_obj, "Array value object");
	BT_ASSERT_PRE_NON_NULL(element_obj, "Element value object");
	BT_ASSERT_PRE_VALUE_IS_TYPE(array_obj, BT_VALUE_TYPE_ARRAY);
	BT_ASSERT_PRE_DEV_VALUE_HOT(array_obj, "Array value object");
	g_ptr_array_add(typed_array_obj->garray, element_obj);
	bt_object_get_ref(element_obj);
	BT_LOGT("Appended element to array value: array-value-addr=%p, "
		"element-value-addr=%p, new-size=%u",
		array_obj, element_obj, typed_array_obj->garray->len);
	return BT_FUNC_STATUS_OK;
}

enum bt_value_array_append_element_status
bt_value_array_append_bool_element(struct bt_value *array_obj, bt_bool val)
{
	enum bt_value_array_append_element_status ret;
	struct bt_value *bool_obj = NULL;

	BT_ASSERT_PRE_NO_ERROR();

	bool_obj = bt_value_bool_create_init(val);
	ret = bt_value_array_append_element(array_obj,
		(void *) bool_obj);
	bt_object_put_ref(bool_obj);
	return ret;
}

enum bt_value_array_append_element_status
bt_value_array_append_unsigned_integer_element(struct bt_value *array_obj,
		uint64_t val)
{
	enum bt_value_array_append_element_status ret;
	struct bt_value *integer_obj = NULL;

	BT_ASSERT_PRE_NO_ERROR();

	integer_obj = bt_value_integer_unsigned_create_init(val);
	ret = bt_value_array_append_element(array_obj,
		(void *) integer_obj);
	bt_object_put_ref(integer_obj);
	return ret;
}

enum bt_value_array_append_element_status
bt_value_array_append_signed_integer_element(struct bt_value *array_obj,
		int64_t val)
{
	enum bt_value_array_append_element_status ret;
	struct bt_value *integer_obj = NULL;

	BT_ASSERT_PRE_NO_ERROR();

	integer_obj = bt_value_integer_signed_create_init(val);
	ret = bt_value_array_append_element(array_obj,
		(void *) integer_obj);
	bt_object_put_ref(integer_obj);
	return ret;
}

enum bt_value_array_append_element_status
bt_value_array_append_real_element(struct bt_value *array_obj, double val)
{
	enum bt_value_array_append_element_status ret;
	struct bt_value *real_obj = NULL;

	BT_ASSERT_PRE_NO_ERROR();

	real_obj = bt_value_real_create_init(val);
	ret = bt_value_array_append_element(array_obj,
		(void *) real_obj);
	bt_object_put_ref(real_obj);
	return ret;
}

enum bt_value_array_append_element_status
bt_value_array_append_string_element(struct bt_value *array_obj,
		const char *val)
{
	enum bt_value_array_append_element_status ret;
	struct bt_value *string_obj = NULL;

	BT_ASSERT_PRE_NO_ERROR();

	string_obj = bt_value_string_create_init(val);
	ret = bt_value_array_append_element(array_obj,
		(void *) string_obj);
	bt_object_put_ref(string_obj);
	return ret;
}

enum bt_value_array_append_element_status
bt_value_array_append_empty_array_element(struct bt_value *array_obj,
		struct bt_value **element_obj)
{
	enum bt_value_array_append_element_status ret;
	struct bt_value *empty_array_obj = NULL;

	BT_ASSERT_PRE_NO_ERROR();

	empty_array_obj = bt_value_array_create();
	ret = bt_value_array_append_element(array_obj,
		(void *) empty_array_obj);

	if (element_obj) {
		*element_obj = empty_array_obj;
	}

	bt_object_put_ref(empty_array_obj);
	return ret;
}

enum bt_value_array_append_element_status
bt_value_array_append_empty_map_element(struct bt_value *array_obj,
		struct bt_value **element_obj)
{
	enum bt_value_array_append_element_status ret;
	struct bt_value *map_obj = NULL;

	BT_ASSERT_PRE_NO_ERROR();

	map_obj = bt_value_map_create();
	ret = bt_value_array_append_element(array_obj,
		(void *) map_obj);

	if (element_obj) {
		*element_obj = map_obj;
	}

	bt_object_put_ref(map_obj);
	return ret;
}

enum bt_value_array_set_element_by_index_status
bt_value_array_set_element_by_index(struct bt_value *array_obj, uint64_t index,
		struct bt_value *element_obj)
{
	struct bt_value_array *typed_array_obj =
		BT_VALUE_TO_ARRAY(array_obj);

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_NON_NULL(array_obj, "Array value object");
	BT_ASSERT_PRE_NON_NULL(element_obj, "Element value object");
	BT_ASSERT_PRE_VALUE_IS_TYPE(array_obj, BT_VALUE_TYPE_ARRAY);
	BT_ASSERT_PRE_DEV_VALUE_HOT(array_obj, "Array value object");
	BT_ASSERT_PRE_VALID_INDEX(index, typed_array_obj->garray->len);
	bt_object_put_ref(g_ptr_array_index(typed_array_obj->garray, index));
	g_ptr_array_index(typed_array_obj->garray, index) = element_obj;
	bt_object_get_ref(element_obj);
	BT_LOGT("Set array value's element: array-value-addr=%p, "
		"index=%" PRIu64 ", element-value-addr=%p",
		array_obj, index, element_obj);
	return BT_FUNC_STATUS_OK;
}

uint64_t bt_value_map_get_size(const struct bt_value *map_obj)
{
	BT_ASSERT_PRE_DEV_NON_NULL(map_obj, "Value object");
	BT_ASSERT_PRE_DEV_VALUE_IS_TYPE(map_obj, BT_VALUE_TYPE_MAP);
	return (uint64_t) g_hash_table_size(BT_VALUE_TO_MAP(map_obj)->ght);
}

struct bt_value *bt_value_map_borrow_entry_value(struct bt_value *map_obj,
		const char *key)
{
	BT_ASSERT_PRE_DEV_NON_NULL(map_obj, "Value object");
	BT_ASSERT_PRE_DEV_NON_NULL(key, "Key");
	BT_ASSERT_PRE_DEV_VALUE_IS_TYPE(map_obj, BT_VALUE_TYPE_MAP);
	return g_hash_table_lookup(BT_VALUE_TO_MAP(map_obj)->ght,
		GUINT_TO_POINTER(g_quark_from_string(key)));
}

const struct bt_value *bt_value_map_borrow_entry_value_const(
		const struct bt_value *map_obj, const char *key)
{
	return bt_value_map_borrow_entry_value((void *) map_obj, key);
}

bt_bool bt_value_map_has_entry(const struct bt_value *map_obj, const char *key)
{
	BT_ASSERT_PRE_DEV_NON_NULL(map_obj, "Value object");
	BT_ASSERT_PRE_DEV_NON_NULL(key, "Key");
	BT_ASSERT_PRE_DEV_VALUE_IS_TYPE(map_obj, BT_VALUE_TYPE_MAP);
	return bt_g_hash_table_contains(BT_VALUE_TO_MAP(map_obj)->ght,
		GUINT_TO_POINTER(g_quark_from_string(key)));
}

enum bt_value_map_insert_entry_status bt_value_map_insert_entry(
		struct bt_value *map_obj, const char *key,
		struct bt_value *element_obj)
{
	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_NON_NULL(map_obj, "Map value object");
	BT_ASSERT_PRE_NON_NULL(key, "Key");
	BT_ASSERT_PRE_NON_NULL(element_obj, "Element value object");
	BT_ASSERT_PRE_VALUE_IS_TYPE(map_obj, BT_VALUE_TYPE_MAP);
	BT_ASSERT_PRE_DEV_VALUE_HOT(map_obj, "Map value object");
	g_hash_table_insert(BT_VALUE_TO_MAP(map_obj)->ght,
		GUINT_TO_POINTER(g_quark_from_string(key)), element_obj);
	bt_object_get_ref(element_obj);
	BT_LOGT("Inserted value into map value: map-value-addr=%p, "
		"key=\"%s\", element-value-addr=%p",
		map_obj, key, element_obj);
	return BT_FUNC_STATUS_OK;
}

enum bt_value_map_insert_entry_status bt_value_map_insert_bool_entry(
		struct bt_value *map_obj, const char *key, bt_bool val)
{
	enum bt_value_map_insert_entry_status ret;
	struct bt_value *bool_obj = NULL;

	BT_ASSERT_PRE_NO_ERROR();

	bool_obj = bt_value_bool_create_init(val);
	ret = bt_value_map_insert_entry(map_obj, key,
		(void *) bool_obj);
	bt_object_put_ref(bool_obj);
	return ret;
}

enum bt_value_map_insert_entry_status
bt_value_map_insert_unsigned_integer_entry(struct bt_value *map_obj,
		const char *key, uint64_t val)
{
	enum bt_value_map_insert_entry_status ret;
	struct bt_value *integer_obj = NULL;

	BT_ASSERT_PRE_NO_ERROR();

	integer_obj = bt_value_integer_unsigned_create_init(val);
	ret = bt_value_map_insert_entry(map_obj, key,
		(void *) integer_obj);
	bt_object_put_ref(integer_obj);
	return ret;
}

enum bt_value_map_insert_entry_status
bt_value_map_insert_signed_integer_entry(struct bt_value *map_obj,
		const char *key, int64_t val)
{
	enum bt_value_map_insert_entry_status ret;
	struct bt_value *integer_obj = NULL;

	BT_ASSERT_PRE_NO_ERROR();

	integer_obj = bt_value_integer_signed_create_init(val);
	ret = bt_value_map_insert_entry(map_obj, key,
		(void *) integer_obj);
	bt_object_put_ref(integer_obj);
	return ret;
}

enum bt_value_map_insert_entry_status bt_value_map_insert_real_entry(
		struct bt_value *map_obj, const char *key, double val)
{
	enum bt_value_map_insert_entry_status ret;
	struct bt_value *real_obj = NULL;

	BT_ASSERT_PRE_NO_ERROR();

	real_obj = bt_value_real_create_init(val);
	ret = bt_value_map_insert_entry(map_obj, key,
		(void *) real_obj);
	bt_object_put_ref(real_obj);
	return ret;
}

enum bt_value_map_insert_entry_status bt_value_map_insert_string_entry(
		struct bt_value *map_obj, const char *key,
		const char *val)
{
	enum bt_value_map_insert_entry_status ret;
	struct bt_value *string_obj = NULL;

	BT_ASSERT_PRE_NO_ERROR();

	string_obj = bt_value_string_create_init(val);
	ret = bt_value_map_insert_entry(map_obj, key,
		(void *) string_obj);
	bt_object_put_ref(string_obj);
	return ret;
}

enum bt_value_map_insert_entry_status
bt_value_map_insert_empty_array_entry(
		struct bt_value *map_obj, const char *key,
		bt_value **entry_obj)
{
	enum bt_value_map_insert_entry_status ret;
	struct bt_value *array_obj = NULL;

	BT_ASSERT_PRE_NO_ERROR();

	array_obj = bt_value_array_create();
	ret = bt_value_map_insert_entry(map_obj, key,
		(void *) array_obj);

	if (entry_obj) {
		*entry_obj = array_obj;
	}

	bt_object_put_ref(array_obj);
	return ret;
}

enum bt_value_map_insert_entry_status
bt_value_map_insert_empty_map_entry(struct bt_value *map_obj, const char *key,
		bt_value **entry_obj)
{
	enum bt_value_map_insert_entry_status ret;
	struct bt_value *empty_map_obj = NULL;

	BT_ASSERT_PRE_NO_ERROR();

	empty_map_obj = bt_value_map_create();
	ret = bt_value_map_insert_entry(map_obj, key,
		(void *) empty_map_obj);

	if (entry_obj) {
		*entry_obj = empty_map_obj;
	}

	bt_object_put_ref(empty_map_obj);
	return ret;
}

enum bt_value_map_foreach_entry_status bt_value_map_foreach_entry(
		struct bt_value *map_obj, bt_value_map_foreach_entry_func func,
		void *data)
{
	int status = BT_FUNC_STATUS_OK;
	gpointer key, element_obj;
	GHashTableIter iter;
	struct bt_value_map *typed_map_obj = BT_VALUE_TO_MAP(map_obj);

	BT_ASSERT_PRE_NO_ERROR();

	BT_ASSERT_PRE_DEV_NON_NULL(map_obj, "Value object");
	BT_ASSERT_PRE_DEV_NON_NULL(func, "Callback");
	BT_ASSERT_PRE_DEV_VALUE_IS_TYPE(map_obj, BT_VALUE_TYPE_MAP);
	g_hash_table_iter_init(&iter, typed_map_obj->ght);

	while (g_hash_table_iter_next(&iter, &key, &element_obj)) {
		const char *key_str = g_quark_to_string(GPOINTER_TO_UINT(key));

		status = func(key_str, element_obj, data);
		BT_ASSERT_POST_NO_ERROR_IF_NO_ERROR_STATUS(status);
		if (status != BT_FUNC_STATUS_OK) {
			if (status < 0) {
				BT_LIB_LOGE_APPEND_CAUSE(
					"User function failed while iterating "
					"map value entries: "
					"status=%s, key=\"%s\", "
					"value-addr=%p, data=%p",
					bt_common_func_status_string(status),
					key_str, element_obj, data);

				if (status == BT_FUNC_STATUS_ERROR) {
					/*
					 * User function error becomes a
					 * user error from this
					 * function's caller's
					 * perspective.
					 */
					status = BT_FUNC_STATUS_USER_ERROR;
				}
			} else {
				BT_ASSERT(status == BT_FUNC_STATUS_INTERRUPTED);
				BT_LOGT("User interrupted the loop: status=%s, "
					"key=\"%s\", value-addr=%p, data=%p",
					bt_common_func_status_string(status),
					key_str, element_obj, data);
			}

			break;
		}
	}

	return status;
}

enum bt_value_map_foreach_entry_const_status bt_value_map_foreach_entry_const(
		const struct bt_value *map_obj,
		bt_value_map_foreach_entry_const_func func, void *data)
{
	BT_ASSERT_PRE_NO_ERROR();

	return (int) bt_value_map_foreach_entry((void *) map_obj,
		(bt_value_map_foreach_entry_func) func, data);
}

struct extend_map_element_data {
	struct bt_value *base_obj;
};

static
bt_value_map_foreach_entry_const_func_status extend_map_element(
		const char *key, const struct bt_value *extension_obj_elem,
		void *data)
{
	int status;
	struct extend_map_element_data *extend_data = data;
	struct bt_value *extension_obj_elem_copy = NULL;

	/* Copy object which is to replace the current one */
	status = bt_value_copy(extension_obj_elem, &extension_obj_elem_copy);
	if (status) {
		BT_LIB_LOGE_APPEND_CAUSE("Cannot copy map element: %!+v",
			extension_obj_elem);
		goto error;
	}

	BT_ASSERT(extension_obj_elem_copy);

	/* Replace in base map value. */
	status = bt_value_map_insert_entry(extend_data->base_obj, key,
		extension_obj_elem_copy);
	if (status) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Cannot replace value in base map value: key=\"%s\", "
			"%![base-map-value-]+v, %![element-value-]+v",
			key, extend_data->base_obj, extension_obj_elem_copy);
		goto error;
	}

	goto end;

error:
	BT_ASSERT(status < 0);

end:
	BT_OBJECT_PUT_REF_AND_RESET(extension_obj_elem_copy);
	BT_ASSERT(status == BT_FUNC_STATUS_OK ||
		status == BT_FUNC_STATUS_MEMORY_ERROR);
	return status;
}

enum bt_value_map_extend_status bt_value_map_extend(
		struct bt_value *base_map_obj,
		const struct bt_value *extension_obj)
{
	int status = BT_FUNC_STATUS_OK;
	struct extend_map_element_data extend_data = {
		.base_obj = NULL,
	};

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_NON_NULL(base_map_obj, "Base value object");
	BT_ASSERT_PRE_DEV_VALUE_HOT(base_map_obj, "Base value object");
	BT_ASSERT_PRE_NON_NULL(extension_obj, "Extension value object");
	BT_ASSERT_PRE_VALUE_IS_TYPE(base_map_obj, BT_VALUE_TYPE_MAP);
	BT_ASSERT_PRE_VALUE_IS_TYPE(extension_obj, BT_VALUE_TYPE_MAP);
	BT_LOGD("Extending map value: base-value-addr=%p, extension-value-addr=%p",
		base_map_obj, extension_obj);

	/*
	 * For each key in the extension map object, replace this key
	 * in the base map object.
	 */
	extend_data.base_obj = base_map_obj;
	status = bt_value_map_foreach_entry_const(extension_obj,
		extend_map_element, &extend_data);
	if (status != BT_FUNC_STATUS_OK) {
		BT_ASSERT(status == BT_FUNC_STATUS_MEMORY_ERROR);
		BT_LIB_LOGE_APPEND_CAUSE(
			"Cannot iterate on the extension object's elements: "
			"%![extension-value-]+v", extension_obj);
	}

	return status;
}

enum bt_value_copy_status bt_value_copy(const struct bt_value *object,
		struct bt_value **copy_obj)
{
	enum bt_value_copy_status status = BT_FUNC_STATUS_OK;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_NON_NULL(object, "Value object");
	BT_ASSERT_PRE_NON_NULL(copy_obj, "Value object copy (output)");
	BT_LOGD("Copying value object: addr=%p", object);
	*copy_obj = copy_funcs[object->type](object);
	if (*copy_obj) {
		BT_LOGD("Copied value object: copy-value-addr=%p",
			copy_obj);
	} else {
		status = BT_FUNC_STATUS_MEMORY_ERROR;
		*copy_obj = NULL;
		BT_LIB_LOGE_APPEND_CAUSE("Failed to copy value object.");
	}

	return status;
}

bt_bool bt_value_is_equal(const struct bt_value *object_a,
	const struct bt_value *object_b)
{
	bt_bool ret = BT_FALSE;

	BT_ASSERT_PRE_DEV_NON_NULL(object_a, "Value object A");
	BT_ASSERT_PRE_DEV_NON_NULL(object_b, "Value object B");

	if (object_a->type != object_b->type) {
		BT_LOGT("Values are different: type mismatch: "
			"value-a-addr=%p, value-b-addr=%p, "
			"value-a-type=%s, value-b-type=%s",
			object_a, object_b,
			bt_common_value_type_string(object_a->type),
			bt_common_value_type_string(object_b->type));
		goto end;
	}

	ret = is_equal_funcs[object_a->type](object_a, object_b);

end:
	return ret;
}

void bt_value_get_ref(const struct bt_value *value)
{
	bt_object_get_ref(value);
}

void bt_value_put_ref(const struct bt_value *value)
{
	bt_object_put_ref(value);
}
