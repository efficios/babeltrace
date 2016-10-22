/*
 * Values.c: value objects
 *
 * Babeltrace Library
 *
 * Copyright (c) 2015 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2015 Philippe Proulx <pproulx@efficios.com>
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

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <string.h>
#include <babeltrace/compiler.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/values.h>
#include <babeltrace/compat/glib.h>

#define BT_VALUE_FROM_CONCRETE(_concrete) ((struct bt_value *) (_concrete))
#define BT_VALUE_TO_BOOL(_base) ((struct bt_value_bool *) (_base))
#define BT_VALUE_TO_INTEGER(_base) ((struct bt_value_integer *) (_base))
#define BT_VALUE_TO_FLOAT(_base) ((struct bt_value_float *) (_base))
#define BT_VALUE_TO_STRING(_base) ((struct bt_value_string *) (_base))
#define BT_VALUE_TO_ARRAY(_base) ((struct bt_value_array *) (_base))
#define BT_VALUE_TO_MAP(_base) ((struct bt_value_map *) (_base))

struct bt_value {
	struct bt_object base;
	enum bt_value_type type;
	bool is_frozen;
};

static
struct bt_value bt_value_null_instance = {
	.type = BT_VALUE_TYPE_NULL,
	.is_frozen = true,
};

struct bt_value *bt_value_null = &bt_value_null_instance;

struct bt_value_bool {
	struct bt_value base;
	bool value;
};

struct bt_value_integer {
	struct bt_value base;
	int64_t value;
};

struct bt_value_float {
	struct bt_value base;
	double value;
};

struct bt_value_string {
	struct bt_value base;
	GString *gstr;
};

struct bt_value_array {
	struct bt_value base;
	GPtrArray *garray;
};

struct bt_value_map {
	struct bt_value base;
	GHashTable *ght;
};

static
void bt_value_destroy(struct bt_object *obj);

static
void bt_value_string_destroy(struct bt_value *object)
{
	g_string_free(BT_VALUE_TO_STRING(object)->gstr, TRUE);
}

static
void bt_value_array_destroy(struct bt_value *object)
{
	/*
	 * Pointer array's registered value destructor will take care
	 * of putting each contained object.
	 */
	g_ptr_array_free(BT_VALUE_TO_ARRAY(object)->garray, TRUE);
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
}

static
void (* const destroy_funcs[])(struct bt_value *) = {
	[BT_VALUE_TYPE_NULL] =		NULL,
	[BT_VALUE_TYPE_BOOL] =		NULL,
	[BT_VALUE_TYPE_INTEGER] =	NULL,
	[BT_VALUE_TYPE_FLOAT] =		NULL,
	[BT_VALUE_TYPE_STRING] =	bt_value_string_destroy,
	[BT_VALUE_TYPE_ARRAY] =		bt_value_array_destroy,
	[BT_VALUE_TYPE_MAP] =		bt_value_map_destroy,
};

static
struct bt_value *bt_value_null_copy(const struct bt_value *null_obj)
{
	return bt_value_null;
}

static
struct bt_value *bt_value_bool_copy(const struct bt_value *bool_obj)
{
	return bt_value_bool_create_init(BT_VALUE_TO_BOOL(bool_obj)->value);
}

static
struct bt_value *bt_value_integer_copy(const struct bt_value *integer_obj)
{
	return bt_value_integer_create_init(
		BT_VALUE_TO_INTEGER(integer_obj)->value);
}

static
struct bt_value *bt_value_float_copy(const struct bt_value *float_obj)
{
	return bt_value_float_create_init(
		BT_VALUE_TO_FLOAT(float_obj)->value);
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

	typed_array_obj = BT_VALUE_TO_ARRAY(array_obj);
	copy_obj = bt_value_array_create();

	if (!copy_obj) {
		goto end;
	}

	for (i = 0; i < typed_array_obj->garray->len; ++i) {
		struct bt_value *element_obj_copy;
		struct bt_value *element_obj = bt_value_array_get(array_obj, i);

		if (!element_obj) {
			BT_PUT(copy_obj);
			goto end;
		}

		element_obj_copy = bt_value_copy(element_obj);
		BT_PUT(element_obj);

		if (!element_obj_copy) {
			BT_PUT(copy_obj);
			goto end;
		}

		ret = bt_value_array_append(copy_obj, element_obj_copy);
		BT_PUT(element_obj_copy);

		if (ret) {
			BT_PUT(copy_obj);
			goto end;
		}
	}

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
	struct bt_value *element_obj_copy;
	struct bt_value_map *typed_map_obj;

	typed_map_obj = BT_VALUE_TO_MAP(map_obj);
	copy_obj = bt_value_map_create();

	if (!copy_obj) {
		goto end;
	}

	g_hash_table_iter_init(&iter, typed_map_obj->ght);

	while (g_hash_table_iter_next(&iter, &key, &element_obj)) {
		const char *key_str = g_quark_to_string((unsigned long) key);

		element_obj_copy = bt_value_copy(element_obj);

		if (!element_obj_copy) {
			BT_PUT(copy_obj);
			goto end;
		}

		ret = bt_value_map_insert(copy_obj, key_str, element_obj_copy);
		BT_PUT(element_obj_copy);

		if (ret) {
			BT_PUT(copy_obj);
			goto end;
		}
	}

end:
	return copy_obj;
}

static
struct bt_value *(* const copy_funcs[])(const struct bt_value *) = {
	[BT_VALUE_TYPE_NULL] =		bt_value_null_copy,
	[BT_VALUE_TYPE_BOOL] =		bt_value_bool_copy,
	[BT_VALUE_TYPE_INTEGER] =	bt_value_integer_copy,
	[BT_VALUE_TYPE_FLOAT] =		bt_value_float_copy,
	[BT_VALUE_TYPE_STRING] =	bt_value_string_copy,
	[BT_VALUE_TYPE_ARRAY] =		bt_value_array_copy,
	[BT_VALUE_TYPE_MAP] =		bt_value_map_copy,
};

static
bool bt_value_null_compare(const struct bt_value *object_a,
		const struct bt_value *object_b)
{
	/*
	 * Always true since bt_value_compare() already checks if both
	 * object_a and object_b have the same type, and in the case of
	 * null value objects, they're always the same if it is so.
	 */
	return true;
}

static
bool bt_value_bool_compare(const struct bt_value *object_a,
		const struct bt_value *object_b)
{
	return BT_VALUE_TO_BOOL(object_a)->value ==
		BT_VALUE_TO_BOOL(object_b)->value;
}

static
bool bt_value_integer_compare(const struct bt_value *object_a,
		const struct bt_value *object_b)
{
	return BT_VALUE_TO_INTEGER(object_a)->value ==
		BT_VALUE_TO_INTEGER(object_b)->value;
}

static
bool bt_value_float_compare(const struct bt_value *object_a,
		const struct bt_value *object_b)
{
	return BT_VALUE_TO_FLOAT(object_a)->value ==
		BT_VALUE_TO_FLOAT(object_b)->value;
}

static
bool bt_value_string_compare(const struct bt_value *object_a,
		const struct bt_value *object_b)
{
	return !strcmp(BT_VALUE_TO_STRING(object_a)->gstr->str,
		BT_VALUE_TO_STRING(object_b)->gstr->str);
}

static
bool bt_value_array_compare(const struct bt_value *object_a,
		const struct bt_value *object_b)
{
	int i;
	bool ret = true;
	const struct bt_value_array *array_obj_a =
		BT_VALUE_TO_ARRAY(object_a);

	if (bt_value_array_size(object_a) != bt_value_array_size(object_b)) {
		ret = false;
		goto end;
	}

	for (i = 0; i < array_obj_a->garray->len; ++i) {
		struct bt_value *element_obj_a;
		struct bt_value *element_obj_b;

		element_obj_a = bt_value_array_get(object_a, i);
		element_obj_b = bt_value_array_get(object_b, i);

		if (!bt_value_compare(element_obj_a, element_obj_b)) {
			BT_PUT(element_obj_a);
			BT_PUT(element_obj_b);
			ret = false;
			goto end;
		}

		BT_PUT(element_obj_a);
		BT_PUT(element_obj_b);
	}

end:
	return ret;
}

static
bool bt_value_map_compare(const struct bt_value *object_a,
		const struct bt_value *object_b)
{
	bool ret = true;
	GHashTableIter iter;
	gpointer key, element_obj_a;
	const struct bt_value_map *map_obj_a = BT_VALUE_TO_MAP(object_a);

	if (bt_value_map_size(object_a) != bt_value_map_size(object_b)) {
		ret = false;
		goto end;
	}

	g_hash_table_iter_init(&iter, map_obj_a->ght);

	while (g_hash_table_iter_next(&iter, &key, &element_obj_a)) {
		struct bt_value *element_obj_b;
		const char *key_str = g_quark_to_string((unsigned long) key);

		element_obj_b = bt_value_map_get(object_b, key_str);

		if (!bt_value_compare(element_obj_a, element_obj_b)) {
			BT_PUT(element_obj_b);
			ret = false;
			goto end;
		}

		BT_PUT(element_obj_b);
	}

end:
	return ret;
}

static
bool (* const compare_funcs[])(const struct bt_value *,
		const struct bt_value *) = {
	[BT_VALUE_TYPE_NULL] =		bt_value_null_compare,
	[BT_VALUE_TYPE_BOOL] =		bt_value_bool_compare,
	[BT_VALUE_TYPE_INTEGER] =	bt_value_integer_compare,
	[BT_VALUE_TYPE_FLOAT] =	bt_value_float_compare,
	[BT_VALUE_TYPE_STRING] =	bt_value_string_compare,
	[BT_VALUE_TYPE_ARRAY] =	bt_value_array_compare,
	[BT_VALUE_TYPE_MAP] =		bt_value_map_compare,
};

BT_HIDDEN
void bt_value_null_freeze(struct bt_value *object)
{
}

BT_HIDDEN
void bt_value_generic_freeze(struct bt_value *object)
{
	object->is_frozen = true;
}

BT_HIDDEN
void bt_value_array_freeze(struct bt_value *object)
{
	int i;
	struct bt_value_array *typed_array_obj =
		BT_VALUE_TO_ARRAY(object);

	for (i = 0; i < typed_array_obj->garray->len; ++i) {
		struct bt_value *element_obj =
			g_ptr_array_index(typed_array_obj->garray, i);

		bt_value_freeze(element_obj);
	}

	bt_value_generic_freeze(object);
}

BT_HIDDEN
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
	[BT_VALUE_TYPE_NULL] =		bt_value_null_freeze,
	[BT_VALUE_TYPE_BOOL] =		bt_value_generic_freeze,
	[BT_VALUE_TYPE_INTEGER] =	bt_value_generic_freeze,
	[BT_VALUE_TYPE_FLOAT] =		bt_value_generic_freeze,
	[BT_VALUE_TYPE_STRING] =	bt_value_generic_freeze,
	[BT_VALUE_TYPE_ARRAY] =		bt_value_array_freeze,
	[BT_VALUE_TYPE_MAP] =		bt_value_map_freeze,
};

static
void bt_value_destroy(struct bt_object *obj)
{
	struct bt_value *value;

	value = container_of(obj, struct bt_value, base);
	assert(value->type != BT_VALUE_TYPE_UNKNOWN);

	if (bt_value_is_null(value)) {
		return;
	}

	if (destroy_funcs[value->type]) {
		destroy_funcs[value->type](value);
	}

	g_free(value);
}

BT_HIDDEN
enum bt_value_status bt_value_freeze(struct bt_value *object)
{
	enum bt_value_status ret = BT_VALUE_STATUS_OK;

	if (!object) {
		ret = BT_VALUE_STATUS_INVAL;
		goto end;
	}

	freeze_funcs[object->type](object);

end:
	return ret;
}

BT_HIDDEN
bool bt_value_is_frozen(const struct bt_value *object)
{
	return object && object->is_frozen;
}

BT_HIDDEN
enum bt_value_type bt_value_get_type(const struct bt_value *object)
{
	if (!object) {
		return BT_VALUE_TYPE_UNKNOWN;
	}

	return object->type;
}

static
struct bt_value bt_value_create_base(enum bt_value_type type)
{
	struct bt_value base;

	base.type = type;
	base.is_frozen = false;
	bt_object_init(&base, bt_value_destroy);

	return base;
}

BT_HIDDEN
struct bt_value *bt_value_bool_create_init(bool val)
{
	struct bt_value_bool *bool_obj;

	bool_obj = g_new0(struct bt_value_bool, 1);

	if (!bool_obj) {
		goto end;
	}

	bool_obj->base = bt_value_create_base(BT_VALUE_TYPE_BOOL);
	bool_obj->value = val;

end:
	return BT_VALUE_FROM_CONCRETE(bool_obj);
}

BT_HIDDEN
struct bt_value *bt_value_bool_create(void)
{
	return bt_value_bool_create_init(false);
}

BT_HIDDEN
struct bt_value *bt_value_integer_create_init(int64_t val)
{
	struct bt_value_integer *integer_obj;

	integer_obj = g_new0(struct bt_value_integer, 1);

	if (!integer_obj) {
		goto end;
	}

	integer_obj->base = bt_value_create_base(BT_VALUE_TYPE_INTEGER);
	integer_obj->value = val;

end:
	return BT_VALUE_FROM_CONCRETE(integer_obj);
}

BT_HIDDEN
struct bt_value *bt_value_integer_create(void)
{
	return bt_value_integer_create_init(0);
}

BT_HIDDEN
struct bt_value *bt_value_float_create_init(double val)
{
	struct bt_value_float *float_obj;

	float_obj = g_new0(struct bt_value_float, 1);

	if (!float_obj) {
		goto end;
	}

	float_obj->base = bt_value_create_base(BT_VALUE_TYPE_FLOAT);
	float_obj->value = val;

end:
	return BT_VALUE_FROM_CONCRETE(float_obj);
}

BT_HIDDEN
struct bt_value *bt_value_float_create(void)
{
	return bt_value_float_create_init(0.);
}

BT_HIDDEN
struct bt_value *bt_value_string_create_init(const char *val)
{
	struct bt_value_string *string_obj = NULL;

	if (!val) {
		goto end;
	}

	string_obj = g_new0(struct bt_value_string, 1);

	if (!string_obj) {
		goto end;
	}

	string_obj->base = bt_value_create_base(BT_VALUE_TYPE_STRING);
	string_obj->gstr = g_string_new(val);

	if (!string_obj->gstr) {
		g_free(string_obj);
		string_obj = NULL;
		goto end;
	}

end:
	return BT_VALUE_FROM_CONCRETE(string_obj);
}

BT_HIDDEN
struct bt_value *bt_value_string_create(void)
{
	return bt_value_string_create_init("");
}

BT_HIDDEN
struct bt_value *bt_value_array_create(void)
{
	struct bt_value_array *array_obj;

	array_obj = g_new0(struct bt_value_array, 1);

	if (!array_obj) {
		goto end;
	}

	array_obj->base = bt_value_create_base(BT_VALUE_TYPE_ARRAY);
	array_obj->garray = babeltrace_g_ptr_array_new_full(0,
		(GDestroyNotify) bt_put);

	if (!array_obj->garray) {
		g_free(array_obj);
		array_obj = NULL;
		goto end;
	}

end:
	return BT_VALUE_FROM_CONCRETE(array_obj);
}

BT_HIDDEN
struct bt_value *bt_value_map_create(void)
{
	struct bt_value_map *map_obj;

	map_obj = g_new0(struct bt_value_map, 1);

	if (!map_obj) {
		goto end;
	}

	map_obj->base = bt_value_create_base(BT_VALUE_TYPE_MAP);
	map_obj->ght = g_hash_table_new_full(g_direct_hash, g_direct_equal,
		NULL, (GDestroyNotify) bt_put);

	if (!map_obj->ght) {
		g_free(map_obj);
		map_obj = NULL;
		goto end;
	}

end:
	return BT_VALUE_FROM_CONCRETE(map_obj);
}

BT_HIDDEN
enum bt_value_status bt_value_bool_get(const struct bt_value *bool_obj,
		bool *val)
{
	enum bt_value_status ret = BT_VALUE_STATUS_OK;
	struct bt_value_bool *typed_bool_obj = BT_VALUE_TO_BOOL(bool_obj);

	if (!bool_obj || !bt_value_is_bool(bool_obj) || !val) {
		ret = BT_VALUE_STATUS_INVAL;
		goto end;
	}

	*val = typed_bool_obj->value;

end:
	return ret;
}

BT_HIDDEN
enum bt_value_status bt_value_bool_set(struct bt_value *bool_obj, bool val)
{
	enum bt_value_status ret = BT_VALUE_STATUS_OK;
	struct bt_value_bool *typed_bool_obj = BT_VALUE_TO_BOOL(bool_obj);

	if (!bool_obj || !bt_value_is_bool(bool_obj)) {
		ret = BT_VALUE_STATUS_INVAL;
		goto end;
	}

	if (bool_obj->is_frozen) {
		ret = BT_VALUE_STATUS_FROZEN;
		goto end;
	}

	typed_bool_obj->value = val;

end:
	return ret;
}

BT_HIDDEN
enum bt_value_status bt_value_integer_get(const struct bt_value *integer_obj,
		int64_t *val)
{
	enum bt_value_status ret = BT_VALUE_STATUS_OK;
	struct bt_value_integer *typed_integer_obj =
		BT_VALUE_TO_INTEGER(integer_obj);

	if (!integer_obj || !bt_value_is_integer(integer_obj) || !val) {
		ret = BT_VALUE_STATUS_INVAL;
		goto end;
	}

	*val = typed_integer_obj->value;

end:
	return ret;
}

BT_HIDDEN
enum bt_value_status bt_value_integer_set(struct bt_value *integer_obj,
		int64_t val)
{
	enum bt_value_status ret = BT_VALUE_STATUS_OK;
	struct bt_value_integer *typed_integer_obj =
		BT_VALUE_TO_INTEGER(integer_obj);

	if (!integer_obj || !bt_value_is_integer(integer_obj)) {
		ret = BT_VALUE_STATUS_INVAL;
		goto end;
	}

	if (integer_obj->is_frozen) {
		ret = BT_VALUE_STATUS_FROZEN;
		goto end;
	}

	typed_integer_obj->value = val;

end:
	return ret;
}

BT_HIDDEN
enum bt_value_status bt_value_float_get(const struct bt_value *float_obj,
		double *val)
{
	enum bt_value_status ret = BT_VALUE_STATUS_OK;
	struct bt_value_float *typed_float_obj =
		BT_VALUE_TO_FLOAT(float_obj);

	if (!float_obj || !bt_value_is_float(float_obj) || !val) {
		ret = BT_VALUE_STATUS_INVAL;
		goto end;
	}

	*val = typed_float_obj->value;

end:
	return ret;
}

BT_HIDDEN
enum bt_value_status bt_value_float_set(struct bt_value *float_obj,
		double val)
{
	enum bt_value_status ret = BT_VALUE_STATUS_OK;
	struct bt_value_float *typed_float_obj =
		BT_VALUE_TO_FLOAT(float_obj);

	if (!float_obj || !bt_value_is_float(float_obj)) {
		ret = BT_VALUE_STATUS_INVAL;
		goto end;
	}

	if (float_obj->is_frozen) {
		ret = BT_VALUE_STATUS_FROZEN;
		goto end;
	}

	typed_float_obj->value = val;

end:
	return ret;
}

BT_HIDDEN
enum bt_value_status bt_value_string_get(const struct bt_value *string_obj,
		const char **val)
{
	enum bt_value_status ret = BT_VALUE_STATUS_OK;
	struct bt_value_string *typed_string_obj =
		BT_VALUE_TO_STRING(string_obj);

	if (!string_obj || !bt_value_is_string(string_obj) || !val) {
		ret = BT_VALUE_STATUS_INVAL;
		goto end;
	}

	*val = typed_string_obj->gstr->str;

end:
	return ret;
}

BT_HIDDEN
enum bt_value_status bt_value_string_set(struct bt_value *string_obj,
		const char *val)
{
	enum bt_value_status ret = BT_VALUE_STATUS_OK;
	struct bt_value_string *typed_string_obj =
		BT_VALUE_TO_STRING(string_obj);

	if (!string_obj || !bt_value_is_string(string_obj) || !val) {
		ret = BT_VALUE_STATUS_INVAL;
		goto end;
	}

	if (string_obj->is_frozen) {
		ret = BT_VALUE_STATUS_FROZEN;
		goto end;
	}

	g_string_assign(typed_string_obj->gstr, val);

end:
	return ret;
}

BT_HIDDEN
int bt_value_array_size(const struct bt_value *array_obj)
{
	int ret;
	struct bt_value_array *typed_array_obj =
		BT_VALUE_TO_ARRAY(array_obj);

	if (!array_obj || !bt_value_is_array(array_obj)) {
		ret = BT_VALUE_STATUS_INVAL;
		goto end;
	}

	ret = (int) typed_array_obj->garray->len;

end:
	return ret;
}

BT_HIDDEN
bool bt_value_array_is_empty(const struct bt_value *array_obj)
{
	return bt_value_array_size(array_obj) == 0;
}

BT_HIDDEN
struct bt_value *bt_value_array_get(const struct bt_value *array_obj,
		size_t index)
{
	struct bt_value *ret;
	struct bt_value_array *typed_array_obj =
		BT_VALUE_TO_ARRAY(array_obj);

	if (!array_obj || !bt_value_is_array(array_obj) ||
			index >= typed_array_obj->garray->len) {
		ret = NULL;
		goto end;
	}

	ret = g_ptr_array_index(typed_array_obj->garray, index);
	bt_get(ret);

end:
	return ret;
}

BT_HIDDEN
enum bt_value_status bt_value_array_append(struct bt_value *array_obj,
		struct bt_value *element_obj)
{
	enum bt_value_status ret = BT_VALUE_STATUS_OK;
	struct bt_value_array *typed_array_obj =
		BT_VALUE_TO_ARRAY(array_obj);

	if (!array_obj || !bt_value_is_array(array_obj) || !element_obj) {
		ret = BT_VALUE_STATUS_INVAL;
		goto end;
	}

	if (array_obj->is_frozen) {
		ret = BT_VALUE_STATUS_FROZEN;
		goto end;
	}

	g_ptr_array_add(typed_array_obj->garray, element_obj);
	bt_get(element_obj);

end:
	return ret;
}

BT_HIDDEN
enum bt_value_status bt_value_array_append_bool(struct bt_value *array_obj,
		bool val)
{
	enum bt_value_status ret;
	struct bt_value *bool_obj = NULL;

	bool_obj = bt_value_bool_create_init(val);
	ret = bt_value_array_append(array_obj, bool_obj);
	bt_put(bool_obj);

	return ret;
}

BT_HIDDEN
enum bt_value_status bt_value_array_append_integer(
		struct bt_value *array_obj, int64_t val)
{
	enum bt_value_status ret;
	struct bt_value *integer_obj = NULL;

	integer_obj = bt_value_integer_create_init(val);
	ret = bt_value_array_append(array_obj, integer_obj);
	bt_put(integer_obj);

	return ret;
}

BT_HIDDEN
enum bt_value_status bt_value_array_append_float(struct bt_value *array_obj,
		double val)
{
	enum bt_value_status ret;
	struct bt_value *float_obj = NULL;

	float_obj = bt_value_float_create_init(val);
	ret = bt_value_array_append(array_obj, float_obj);
	bt_put(float_obj);

	return ret;
}

BT_HIDDEN
enum bt_value_status bt_value_array_append_string(struct bt_value *array_obj,
		const char *val)
{
	enum bt_value_status ret;
	struct bt_value *string_obj = NULL;

	string_obj = bt_value_string_create_init(val);
	ret = bt_value_array_append(array_obj, string_obj);
	bt_put(string_obj);

	return ret;
}

BT_HIDDEN
enum bt_value_status bt_value_array_append_empty_array(
		struct bt_value *array_obj)
{
	enum bt_value_status ret;
	struct bt_value *empty_array_obj = NULL;

	empty_array_obj = bt_value_array_create();
	ret = bt_value_array_append(array_obj, empty_array_obj);
	bt_put(empty_array_obj);

	return ret;
}

BT_HIDDEN
enum bt_value_status bt_value_array_append_empty_map(struct bt_value *array_obj)
{
	enum bt_value_status ret;
	struct bt_value *map_obj = NULL;

	map_obj = bt_value_map_create();
	ret = bt_value_array_append(array_obj, map_obj);
	bt_put(map_obj);

	return ret;
}

BT_HIDDEN
enum bt_value_status bt_value_array_set(struct bt_value *array_obj,
		size_t index, struct bt_value *element_obj)
{
	enum bt_value_status ret = BT_VALUE_STATUS_OK;
	struct bt_value_array *typed_array_obj =
		BT_VALUE_TO_ARRAY(array_obj);

	if (!array_obj || !bt_value_is_array(array_obj) || !element_obj ||
			index >= typed_array_obj->garray->len) {
		ret = BT_VALUE_STATUS_INVAL;
		goto end;
	}

	if (array_obj->is_frozen) {
		ret = BT_VALUE_STATUS_FROZEN;
		goto end;
	}

	bt_put(g_ptr_array_index(typed_array_obj->garray, index));
	g_ptr_array_index(typed_array_obj->garray, index) = element_obj;
	bt_get(element_obj);

end:
	return ret;
}

BT_HIDDEN
int bt_value_map_size(const struct bt_value *map_obj)
{
	int ret;
	struct bt_value_map *typed_map_obj = BT_VALUE_TO_MAP(map_obj);

	if (!map_obj || !bt_value_is_map(map_obj)) {
		ret = BT_VALUE_STATUS_INVAL;
		goto end;
	}

	ret = (int) g_hash_table_size(typed_map_obj->ght);

end:
	return ret;
}

BT_HIDDEN
bool bt_value_map_is_empty(const struct bt_value *map_obj)
{
	return bt_value_map_size(map_obj) == 0;
}

BT_HIDDEN
struct bt_value *bt_value_map_get(const struct bt_value *map_obj,
		const char *key)
{
	GQuark quark;
	struct bt_value *ret;
	struct bt_value_map *typed_map_obj = BT_VALUE_TO_MAP(map_obj);

	if (!map_obj || !bt_value_is_map(map_obj) || !key) {
		ret = NULL;
		goto end;
	}

	quark = g_quark_from_string(key);
	ret = g_hash_table_lookup(typed_map_obj->ght, GUINT_TO_POINTER(quark));

	if (ret) {
		bt_get(ret);
	}

end:
	return ret;
}

BT_HIDDEN
bool bt_value_map_has_key(const struct bt_value *map_obj, const char *key)
{
	bool ret;
	GQuark quark;
	struct bt_value_map *typed_map_obj = BT_VALUE_TO_MAP(map_obj);

	if (!map_obj || !bt_value_is_map(map_obj) || !key) {
		ret = false;
		goto end;
	}

	quark = g_quark_from_string(key);
	ret = babeltrace_g_hash_table_contains(typed_map_obj->ght,
		GUINT_TO_POINTER(quark));

end:
	return ret;
}

BT_HIDDEN
enum bt_value_status bt_value_map_insert(struct bt_value *map_obj,
		const char *key, struct bt_value *element_obj)
{
	GQuark quark;
	enum bt_value_status ret = BT_VALUE_STATUS_OK;
	struct bt_value_map *typed_map_obj = BT_VALUE_TO_MAP(map_obj);

	if (!map_obj || !bt_value_is_map(map_obj) || !key || !element_obj) {
		ret = BT_VALUE_STATUS_INVAL;
		goto end;
	}

	if (map_obj->is_frozen) {
		ret = BT_VALUE_STATUS_FROZEN;
		goto end;
	}

	quark = g_quark_from_string(key);
	g_hash_table_insert(typed_map_obj->ght,
		GUINT_TO_POINTER(quark), element_obj);
	bt_get(element_obj);

end:
	return ret;
}

BT_HIDDEN
enum bt_value_status bt_value_map_insert_bool(struct bt_value *map_obj,
		const char *key, bool val)
{
	enum bt_value_status ret;
	struct bt_value *bool_obj = NULL;

	bool_obj = bt_value_bool_create_init(val);
	ret = bt_value_map_insert(map_obj, key, bool_obj);
	bt_put(bool_obj);

	return ret;
}

BT_HIDDEN
enum bt_value_status bt_value_map_insert_integer(struct bt_value *map_obj,
		const char *key, int64_t val)
{
	enum bt_value_status ret;
	struct bt_value *integer_obj = NULL;

	integer_obj = bt_value_integer_create_init(val);
	ret = bt_value_map_insert(map_obj, key, integer_obj);
	bt_put(integer_obj);

	return ret;
}

BT_HIDDEN
enum bt_value_status bt_value_map_insert_float(struct bt_value *map_obj,
		const char *key, double val)
{
	enum bt_value_status ret;
	struct bt_value *float_obj = NULL;

	float_obj = bt_value_float_create_init(val);
	ret = bt_value_map_insert(map_obj, key, float_obj);
	bt_put(float_obj);

	return ret;
}

BT_HIDDEN
enum bt_value_status bt_value_map_insert_string(struct bt_value *map_obj,
		const char *key, const char *val)
{
	enum bt_value_status ret;
	struct bt_value *string_obj = NULL;

	string_obj = bt_value_string_create_init(val);
	ret = bt_value_map_insert(map_obj, key, string_obj);
	bt_put(string_obj);

	return ret;
}

BT_HIDDEN
enum bt_value_status bt_value_map_insert_empty_array(struct bt_value *map_obj,
		const char *key)
{
	enum bt_value_status ret;
	struct bt_value *array_obj = NULL;

	array_obj = bt_value_array_create();
	ret = bt_value_map_insert(map_obj, key, array_obj);
	bt_put(array_obj);

	return ret;
}

BT_HIDDEN
enum bt_value_status bt_value_map_insert_empty_map(struct bt_value *map_obj,
		const char *key)
{
	enum bt_value_status ret;
	struct bt_value *empty_map_obj = NULL;

	empty_map_obj = bt_value_map_create();
	ret = bt_value_map_insert(map_obj, key, empty_map_obj);
	bt_put(empty_map_obj);

	return ret;
}

BT_HIDDEN
enum bt_value_status bt_value_map_foreach(const struct bt_value *map_obj,
		bt_value_map_foreach_cb cb, void *data)
{
	enum bt_value_status ret = BT_VALUE_STATUS_OK;
	gpointer key, element_obj;
	GHashTableIter iter;
	struct bt_value_map *typed_map_obj = BT_VALUE_TO_MAP(map_obj);

	if (!map_obj || !bt_value_is_map(map_obj) || !cb) {
		ret = BT_VALUE_STATUS_INVAL;
		goto end;
	}

	g_hash_table_iter_init(&iter, typed_map_obj->ght);

	while (g_hash_table_iter_next(&iter, &key, &element_obj)) {
		const char *key_str = g_quark_to_string((unsigned long) key);

		if (!cb(key_str, element_obj, data)) {
			ret = BT_VALUE_STATUS_CANCELLED;
			break;
		}
	}

end:
	return ret;
}

BT_HIDDEN
struct bt_value *bt_value_copy(const struct bt_value *object)
{
	struct bt_value *copy_obj = NULL;

	if (!object) {
		goto end;
	}

	copy_obj = copy_funcs[object->type](object);

end:
	return copy_obj;
}

BT_HIDDEN
bool bt_value_compare(const struct bt_value *object_a,
	const struct bt_value *object_b)
{
	bool ret = false;

	if (!object_a || !object_b) {
		goto end;
	}

	if (object_a->type != object_b->type) {
		goto end;
	}

	ret = compare_funcs[object_a->type](object_a, object_b);

end:
	return ret;
}
