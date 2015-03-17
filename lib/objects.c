/*
 * objects.c: basic object system
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
#include <babeltrace/ctf-writer/ref-internal.h>
#include <babeltrace/compiler.h>
#include <glib.h>
#include <babeltrace/objects.h>

#define BT_OBJECT_FROM_CONCRETE(_concrete) ((struct bt_object *) (_concrete))
#define BT_OBJECT_TO_BOOL(_base) ((struct bt_object_bool *) (_base))
#define BT_OBJECT_TO_INTEGER(_base) ((struct bt_object_integer *) (_base))
#define BT_OBJECT_TO_FLOAT(_base) ((struct bt_object_float *) (_base))
#define BT_OBJECT_TO_STRING(_base) ((struct bt_object_string *) (_base))
#define BT_OBJECT_TO_ARRAY(_base) ((struct bt_object_array *) (_base))
#define BT_OBJECT_TO_MAP(_base) ((struct bt_object_map *) (_base))

struct bt_object {
	enum bt_object_type type;
	struct bt_ctf_ref ref_count;
};

static
struct bt_object bt_object_null_instance = {
	.type = BT_OBJECT_TYPE_NULL,
};

struct bt_object *bt_object_null = &bt_object_null_instance;

struct bt_object_bool {
	struct bt_object base;
	bool value;
};

struct bt_object_integer {
	struct bt_object base;
	int64_t value;
};

struct bt_object_float {
	struct bt_object base;
	double value;
};

struct bt_object_string {
	struct bt_object base;
	GString *gstr;
};

struct bt_object_array {
	struct bt_object base;
	GPtrArray *garray;
};

struct bt_object_map {
	struct bt_object base;
	GHashTable *ght;
};

static
void bt_object_string_destroy(struct bt_object *object)
{
	g_string_free(BT_OBJECT_TO_STRING(object)->gstr, TRUE);
}

static
void bt_object_array_destroy(struct bt_object *object)
{
	/*
	 * Pointer array's registered value destructor will take care
	 * of putting each contained object.
	 */
	g_ptr_array_free(BT_OBJECT_TO_ARRAY(object)->garray, TRUE);
}

static
void bt_object_map_destroy(struct bt_object *object)
{
	/*
	 * Hash table's registered value destructor will take care of
	 * putting each contained object. Keys are GQuarks and cannot
	 * be destroyed anyway.
	 */
	g_hash_table_destroy(BT_OBJECT_TO_MAP(object)->ght);
}

static
void (* const destroy_funcs[])(struct bt_object *) = {
	[BT_OBJECT_TYPE_NULL] =		NULL,
	[BT_OBJECT_TYPE_BOOL] =		NULL,
	[BT_OBJECT_TYPE_INTEGER] =	NULL,
	[BT_OBJECT_TYPE_FLOAT] =	NULL,
	[BT_OBJECT_TYPE_STRING] =	bt_object_string_destroy,
	[BT_OBJECT_TYPE_ARRAY] =	bt_object_array_destroy,
	[BT_OBJECT_TYPE_MAP] =		bt_object_map_destroy,
};

static
struct bt_object *bt_object_null_copy(const struct bt_object *null_obj)
{
	return bt_object_null;
}

static
struct bt_object *bt_object_bool_copy(const struct bt_object *bool_obj)
{
	return bt_object_bool_create_init(BT_OBJECT_TO_BOOL(bool_obj)->value);
}

static
struct bt_object *bt_object_integer_copy(const struct bt_object *integer_obj)
{
	return bt_object_integer_create_init(
		BT_OBJECT_TO_INTEGER(integer_obj)->value);
}

static
struct bt_object *bt_object_float_copy(const struct bt_object *float_obj)
{
	return bt_object_float_create_init(
		BT_OBJECT_TO_FLOAT(float_obj)->value);
}

static
struct bt_object *bt_object_string_copy(const struct bt_object *string_obj)
{
	return bt_object_string_create_init(
		BT_OBJECT_TO_STRING(string_obj)->gstr->str);
}

static
struct bt_object *bt_object_array_copy(const struct bt_object *array_obj)
{
	int x;
	int ret;
	struct bt_object *copy_obj;
	struct bt_object_array *typed_array_obj;

	typed_array_obj = BT_OBJECT_TO_ARRAY(array_obj);
	copy_obj = bt_object_array_create();

	if (!copy_obj) {
		goto end;
	}

	for (x = 0; x < typed_array_obj->garray->len; ++x) {
		struct bt_object *element_obj_copy;
		struct bt_object *element_obj =
			bt_object_array_get(array_obj, x);

		if (!element_obj) {
			BT_OBJECT_PUT(copy_obj);
			goto end;
		}

		element_obj_copy = bt_object_copy(element_obj);
		BT_OBJECT_PUT(element_obj);

		if (!element_obj_copy) {
			BT_OBJECT_PUT(copy_obj);
			goto end;
		}

		ret = bt_object_array_append(copy_obj, element_obj_copy);
		BT_OBJECT_PUT(element_obj_copy);

		if (ret) {
			BT_OBJECT_PUT(copy_obj);
			goto end;
		}
	}

end:
	return copy_obj;
}

static
struct bt_object *bt_object_map_copy(const struct bt_object *map_obj)
{
	int ret;
	GHashTableIter iter;
	gpointer key, element_obj;
	struct bt_object *copy_obj;
	struct bt_object *element_obj_copy;
	struct bt_object_map *typed_map_obj;

	typed_map_obj = BT_OBJECT_TO_MAP(map_obj);
	copy_obj = bt_object_map_create();

	if (!copy_obj) {
		goto end;
	}

	g_hash_table_iter_init(&iter, typed_map_obj->ght);

	while (g_hash_table_iter_next(&iter, &key, &element_obj)) {
		const char *key_str = g_quark_to_string((unsigned long) key);

		element_obj_copy = bt_object_copy(element_obj);

		if (!element_obj_copy) {
			BT_OBJECT_PUT(copy_obj);
			goto end;
		}

		ret = bt_object_map_insert(copy_obj, key_str, element_obj_copy);
		BT_OBJECT_PUT(element_obj_copy);

		if (ret) {
			BT_OBJECT_PUT(copy_obj);
			goto end;
		}
	}

end:
	return copy_obj;
}

static
struct bt_object *(* const copy_funcs[])(const struct bt_object *) = {
	[BT_OBJECT_TYPE_NULL] =		bt_object_null_copy,
	[BT_OBJECT_TYPE_BOOL] =		bt_object_bool_copy,
	[BT_OBJECT_TYPE_INTEGER] =	bt_object_integer_copy,
	[BT_OBJECT_TYPE_FLOAT] =	bt_object_float_copy,
	[BT_OBJECT_TYPE_STRING] =	bt_object_string_copy,
	[BT_OBJECT_TYPE_ARRAY] =	bt_object_array_copy,
	[BT_OBJECT_TYPE_MAP] =		bt_object_map_copy,
};

static
bool bt_object_null_compare(const struct bt_object *object_a,
		const struct bt_object *object_b)
{
	/*
	 * Always true since bt_object_compare() already checks if both
	 * object_a and object_b have the same type, and in the case of
	 * null objects, they're always the same if it is so.
	 */
	return true;
}

static
bool bt_object_bool_compare(const struct bt_object *object_a,
		const struct bt_object *object_b)
{
	return BT_OBJECT_TO_BOOL(object_a)->value ==
		BT_OBJECT_TO_BOOL(object_b)->value;
}

static
bool bt_object_integer_compare(const struct bt_object *object_a,
		const struct bt_object *object_b)
{
	return BT_OBJECT_TO_INTEGER(object_a)->value ==
		BT_OBJECT_TO_INTEGER(object_b)->value;
}

static
bool bt_object_float_compare(const struct bt_object *object_a,
		const struct bt_object *object_b)
{
	return BT_OBJECT_TO_FLOAT(object_a)->value ==
		BT_OBJECT_TO_FLOAT(object_b)->value;
}

static
bool bt_object_string_compare(const struct bt_object *object_a,
		const struct bt_object *object_b)
{
	return !strcmp(BT_OBJECT_TO_STRING(object_a)->gstr->str,
		BT_OBJECT_TO_STRING(object_b)->gstr->str);
}

static
bool bt_object_array_compare(const struct bt_object *object_a,
		const struct bt_object *object_b)
{
	int x;
	bool ret = true;
	const struct bt_object_array *array_obj_a =
		BT_OBJECT_TO_ARRAY(object_a);

	if (bt_object_array_size(object_a) != bt_object_array_size(object_b)) {
		ret = false;
		goto end;
	}

	for (x = 0; x < array_obj_a->garray->len; ++x) {
		struct bt_object *element_obj_a;
		struct bt_object *element_obj_b;

		element_obj_a = bt_object_array_get(object_a, x);
		element_obj_b = bt_object_array_get(object_b, x);

		if (!bt_object_compare(element_obj_a, element_obj_b)) {
			BT_OBJECT_PUT(element_obj_a);
			BT_OBJECT_PUT(element_obj_b);
			ret = false;
			goto end;
		}

		BT_OBJECT_PUT(element_obj_a);
		BT_OBJECT_PUT(element_obj_b);
	}

end:
	return ret;
}

static
bool bt_object_map_compare(const struct bt_object *object_a,
		const struct bt_object *object_b)
{
	bool ret = true;
	GHashTableIter iter;
	gpointer key, element_obj_a;
	const struct bt_object_map *map_obj_a = BT_OBJECT_TO_MAP(object_a);

	if (bt_object_map_size(object_a) != bt_object_map_size(object_b)) {
		ret = false;
		goto end;
	}

	g_hash_table_iter_init(&iter, map_obj_a->ght);

	while (g_hash_table_iter_next(&iter, &key, &element_obj_a)) {
		struct bt_object *element_obj_b;
		const char *key_str = g_quark_to_string((unsigned long) key);

		element_obj_b = bt_object_map_get(object_b, key_str);

		if (!bt_object_compare(element_obj_a, element_obj_b)) {
			BT_OBJECT_PUT(element_obj_b);
			ret = false;
			goto end;
		}

		BT_OBJECT_PUT(element_obj_b);
	}

end:
	return ret;
}

static
bool (* const compare_funcs[])(const struct bt_object *,
		const struct bt_object *) = {
	[BT_OBJECT_TYPE_NULL] =		bt_object_null_compare,
	[BT_OBJECT_TYPE_BOOL] =		bt_object_bool_compare,
	[BT_OBJECT_TYPE_INTEGER] =	bt_object_integer_compare,
	[BT_OBJECT_TYPE_FLOAT] =	bt_object_float_compare,
	[BT_OBJECT_TYPE_STRING] =	bt_object_string_compare,
	[BT_OBJECT_TYPE_ARRAY] =	bt_object_array_compare,
	[BT_OBJECT_TYPE_MAP] =		bt_object_map_compare,
};

static
void bt_object_destroy(struct bt_ctf_ref *ref_count)
{
	struct bt_object *object;

	object = container_of(ref_count, struct bt_object, ref_count);
	assert(object->type != BT_OBJECT_TYPE_UNKNOWN);

	if (bt_object_is_null(object)) {
		return;
	}

	if (destroy_funcs[object->type]) {
		destroy_funcs[object->type](object);
	}

	g_free(object);
}

void bt_object_get(struct bt_object *object)
{
	if (!object) {
		goto skip;
	}

	bt_ctf_ref_get(&object->ref_count);

skip:
	return;
}

void bt_object_put(struct bt_object *object)
{
	if (!object) {
		goto skip;
	}

	bt_ctf_ref_put(&object->ref_count, bt_object_destroy);

skip:
	return;
}

enum bt_object_type bt_object_get_type(const struct bt_object *object)
{
	if (!object) {
		return BT_OBJECT_TYPE_UNKNOWN;
	}

	return object->type;
}

static
struct bt_object bt_object_create_base(enum bt_object_type type)
{
	struct bt_object base;

	base.type = type;
	bt_ctf_ref_init(&base.ref_count);

	return base;
}

struct bt_object *bt_object_bool_create_init(bool val)
{
	struct bt_object_bool *bool_obj;

	bool_obj = g_new0(struct bt_object_bool, 1);

	if (!bool_obj) {
		goto end;
	}

	bool_obj->base = bt_object_create_base(BT_OBJECT_TYPE_BOOL);
	bool_obj->value = val;

end:
	return BT_OBJECT_FROM_CONCRETE(bool_obj);
}

struct bt_object *bt_object_bool_create(void)
{
	return bt_object_bool_create_init(false);
}

struct bt_object *bt_object_integer_create_init(int64_t val)
{
	struct bt_object_integer *integer_obj;

	integer_obj = g_new0(struct bt_object_integer, 1);

	if (!integer_obj) {
		goto end;
	}

	integer_obj->base = bt_object_create_base(BT_OBJECT_TYPE_INTEGER);
	integer_obj->value = val;

end:
	return BT_OBJECT_FROM_CONCRETE(integer_obj);
}

struct bt_object *bt_object_integer_create(void)
{
	return bt_object_integer_create_init(0);
}

struct bt_object *bt_object_float_create_init(double val)
{
	struct bt_object_float *float_obj;

	float_obj = g_new0(struct bt_object_float, 1);

	if (!float_obj) {
		goto end;
	}

	float_obj->base = bt_object_create_base(BT_OBJECT_TYPE_FLOAT);
	float_obj->value = val;

end:
	return BT_OBJECT_FROM_CONCRETE(float_obj);
}

struct bt_object *bt_object_float_create(void)
{
	return bt_object_float_create_init(0.);
}

struct bt_object *bt_object_string_create_init(const char *val)
{
	struct bt_object_string *string_obj = NULL;

	if (!val) {
		goto end;
	}

	string_obj = g_new0(struct bt_object_string, 1);

	if (!string_obj) {
		goto end;
	}

	string_obj->base = bt_object_create_base(BT_OBJECT_TYPE_STRING);
	string_obj->gstr = g_string_new(val);

	if (!string_obj->gstr) {
		g_free(string_obj);
		string_obj = NULL;
		goto end;
	}

end:
	return BT_OBJECT_FROM_CONCRETE(string_obj);
}

struct bt_object *bt_object_string_create(void)
{
	return bt_object_string_create_init("");
}

struct bt_object *bt_object_array_create(void)
{
	struct bt_object_array *array_obj;

	array_obj = g_new0(struct bt_object_array, 1);

	if (!array_obj) {
		goto end;
	}

	array_obj->base = bt_object_create_base(BT_OBJECT_TYPE_ARRAY);
	array_obj->garray = g_ptr_array_new_full(0,
		(GDestroyNotify) bt_object_put);

	if (!array_obj->garray) {
		g_free(array_obj);
		array_obj = NULL;
		goto end;
	}

end:
	return BT_OBJECT_FROM_CONCRETE(array_obj);
}

struct bt_object *bt_object_map_create(void)
{
	struct bt_object_map *map_obj;

	map_obj = g_new0(struct bt_object_map, 1);

	if (!map_obj) {
		goto end;
	}

	map_obj->base = bt_object_create_base(BT_OBJECT_TYPE_MAP);
	map_obj->ght = g_hash_table_new_full(g_direct_hash, g_direct_equal,
		NULL, (GDestroyNotify) bt_object_put);

	if (!map_obj->ght) {
		g_free(map_obj);
		map_obj = NULL;
		goto end;
	}

end:
	return BT_OBJECT_FROM_CONCRETE(map_obj);
}

int bt_object_bool_get(const struct bt_object *bool_obj, bool *val)
{
	int ret = 0;
	struct bt_object_bool *typed_bool_obj = BT_OBJECT_TO_BOOL(bool_obj);

	if (!bool_obj || !bt_object_is_bool(bool_obj)) {
		ret = -1;
		goto end;
	}

	*val = typed_bool_obj->value;

end:
	return ret;
}

int bt_object_bool_set(struct bt_object *bool_obj, bool val)
{
	int ret = 0;
	struct bt_object_bool *typed_bool_obj = BT_OBJECT_TO_BOOL(bool_obj);

	if (!bool_obj || !bt_object_is_bool(bool_obj)) {
		ret = -1;
		goto end;
	}

	typed_bool_obj->value = val;

end:
	return ret;
}

int bt_object_integer_get(const struct bt_object *integer_obj, int64_t *val)
{
	int ret = 0;
	struct bt_object_integer *typed_integer_obj =
		BT_OBJECT_TO_INTEGER(integer_obj);

	if (!integer_obj || !bt_object_is_integer(integer_obj)) {
		ret = -1;
		goto end;
	}

	*val = typed_integer_obj->value;

end:
	return ret;
}

int bt_object_integer_set(struct bt_object *integer_obj, int64_t val)
{
	int ret = 0;
	struct bt_object_integer *typed_integer_obj =
		BT_OBJECT_TO_INTEGER(integer_obj);

	if (!integer_obj || !bt_object_is_integer(integer_obj)) {
		ret = -1;
		goto end;
	}

	typed_integer_obj->value = val;

end:
	return ret;
}

int bt_object_float_get(const struct bt_object *float_obj, double *val)
{
	int ret = 0;
	struct bt_object_float *typed_float_obj =
		BT_OBJECT_TO_FLOAT(float_obj);

	if (!float_obj || !bt_object_is_float(float_obj)) {
		ret = -1;
		goto end;
	}

	*val = typed_float_obj->value;

end:
	return ret;
}

int bt_object_float_set(struct bt_object *float_obj, double val)
{
	int ret = 0;
	struct bt_object_float *typed_float_obj =
		BT_OBJECT_TO_FLOAT(float_obj);

	if (!float_obj || !bt_object_is_float(float_obj)) {
		ret = -1;
		goto end;
	}

	typed_float_obj->value = val;

end:
	return ret;
}

const char *bt_object_string_get(const struct bt_object *string_obj)
{
	const char *ret;
	struct bt_object_string *typed_string_obj =
		BT_OBJECT_TO_STRING(string_obj);

	if (!string_obj || !bt_object_is_string(string_obj)) {
		ret = NULL;
		goto end;
	}

	ret = typed_string_obj->gstr->str;

end:
	return ret;
}

int bt_object_string_set(struct bt_object *string_obj, const char *val)
{
	int ret = 0;
	struct bt_object_string *typed_string_obj =
		BT_OBJECT_TO_STRING(string_obj);

	if (!string_obj || !bt_object_is_string(string_obj) || !val) {
		ret = -1;
		goto end;
	}

	g_string_assign(typed_string_obj->gstr, val);

end:
	return ret;
}

int bt_object_array_size(const struct bt_object *array_obj)
{
	int ret = 0;
	struct bt_object_array *typed_array_obj =
		BT_OBJECT_TO_ARRAY(array_obj);

	if (!array_obj || !bt_object_is_array(array_obj)) {
		ret = -1;
		goto end;
	}

	ret = (int) typed_array_obj->garray->len;

end:
	return ret;
}

bool bt_object_array_is_empty(const struct bt_object *array_obj)
{
	return bt_object_array_size(array_obj) == 0;
}

struct bt_object *bt_object_array_get(const struct bt_object *array_obj,
	size_t index)
{
	struct bt_object *ret;
	struct bt_object_array *typed_array_obj =
		BT_OBJECT_TO_ARRAY(array_obj);

	if (!array_obj || !bt_object_is_array(array_obj) ||
			index >= typed_array_obj->garray->len) {
		ret = NULL;
		goto end;
	}

	ret = g_ptr_array_index(typed_array_obj->garray, index);
	bt_object_get(ret);

end:
	return ret;
}

int bt_object_array_append(struct bt_object *array_obj,
	struct bt_object *element_obj)
{
	int ret = 0;
	struct bt_object_array *typed_array_obj =
		BT_OBJECT_TO_ARRAY(array_obj);

	if (!array_obj || !bt_object_is_array(array_obj) || !element_obj) {
		ret = -1;
		goto end;
	}

	g_ptr_array_add(typed_array_obj->garray, element_obj);
	bt_object_get(element_obj);

end:
	return ret;
}

int bt_object_array_append_bool(struct bt_object *array_obj, bool val)
{
	int ret;
	struct bt_object *bool_obj = NULL;

	bool_obj = bt_object_bool_create_init(val);
	ret = bt_object_array_append(array_obj, bool_obj);
	bt_object_put(bool_obj);

	return ret;
}

int bt_object_array_append_integer(struct bt_object *array_obj, int64_t val)
{
	int ret;
	struct bt_object *integer_obj = NULL;

	integer_obj = bt_object_integer_create_init(val);
	ret = bt_object_array_append(array_obj, integer_obj);
	bt_object_put(integer_obj);

	return ret;
}

int bt_object_array_append_float(struct bt_object *array_obj, double val)
{
	int ret;
	struct bt_object *float_obj = NULL;

	float_obj = bt_object_float_create_init(val);
	ret = bt_object_array_append(array_obj, float_obj);
	bt_object_put(float_obj);

	return ret;
}

int bt_object_array_append_string(struct bt_object *array_obj, const char *val)
{
	int ret;
	struct bt_object *string_obj = NULL;

	string_obj = bt_object_string_create_init(val);
	ret = bt_object_array_append(array_obj, string_obj);
	bt_object_put(string_obj);

	return ret;
}

int bt_object_array_append_array(struct bt_object *array_obj)
{
	int ret;
	struct bt_object *empty_array_obj = NULL;

	empty_array_obj = bt_object_array_create();
	ret = bt_object_array_append(array_obj, empty_array_obj);
	bt_object_put(empty_array_obj);

	return ret;
}

int bt_object_array_append_map(struct bt_object *array_obj)
{
	int ret;
	struct bt_object *map_obj = NULL;

	map_obj = bt_object_map_create();
	ret = bt_object_array_append(array_obj, map_obj);
	bt_object_put(map_obj);

	return ret;
}

int bt_object_array_set(struct bt_object *array_obj, size_t index,
	struct bt_object *element_obj)
{
	int ret = 0;
	struct bt_object_array *typed_array_obj =
		BT_OBJECT_TO_ARRAY(array_obj);

	if (!array_obj || !bt_object_is_array(array_obj) || !element_obj ||
			index >= typed_array_obj->garray->len) {
		ret = -1;
		goto end;
	}

	bt_object_put(g_ptr_array_index(typed_array_obj->garray, index));
	g_ptr_array_index(typed_array_obj->garray, index) = element_obj;
	bt_object_get(element_obj);

end:
	return ret;
}

int bt_object_map_size(const struct bt_object *map_obj)
{
	int ret;
	struct bt_object_map *typed_map_obj = BT_OBJECT_TO_MAP(map_obj);

	if (!map_obj || !bt_object_is_map(map_obj)) {
		ret = -1;
		goto end;
	}

	ret = (int) g_hash_table_size(typed_map_obj->ght);

end:
	return ret;
}

bool bt_object_map_is_empty(const struct bt_object *map_obj)
{
	return bt_object_map_size(map_obj) == 0;
}

struct bt_object *bt_object_map_get(const struct bt_object *map_obj,
	const char *key)
{
	GQuark quark;
	struct bt_object *ret;
	struct bt_object_map *typed_map_obj = BT_OBJECT_TO_MAP(map_obj);

	if (!map_obj || !bt_object_is_map(map_obj) || !key) {
		ret = NULL;
		goto end;
	}

	quark = g_quark_from_string(key);
	ret = g_hash_table_lookup(typed_map_obj->ght, GUINT_TO_POINTER(quark));

	if (ret) {
		bt_object_get(ret);
	}

end:
	return ret;
}

bool bt_object_map_has_key(const struct bt_object *map_obj, const char *key)
{
	bool ret;
	GQuark quark;
	struct bt_object_map *typed_map_obj = BT_OBJECT_TO_MAP(map_obj);

	if (!map_obj || !bt_object_is_map(map_obj) || !key) {
		ret = false;
		goto end;
	}

	quark = g_quark_from_string(key);
	ret = g_hash_table_contains(typed_map_obj->ght,
		GUINT_TO_POINTER(quark));

end:
	return ret;
}

int bt_object_map_insert(struct bt_object *map_obj, const char *key,
	struct bt_object *element_obj)
{
	int ret = 0;
	GQuark quark;
	struct bt_object_map *typed_map_obj = BT_OBJECT_TO_MAP(map_obj);

	if (!map_obj || !bt_object_is_map(map_obj) || !key || !element_obj) {
		ret = -1;
		goto end;
	}

	quark = g_quark_from_string(key);
	g_hash_table_insert(typed_map_obj->ght,
		GUINT_TO_POINTER(quark), element_obj);
	bt_object_get(element_obj);

end:
	return ret;
}

int bt_object_map_insert_bool(struct bt_object *map_obj,
	const char *key, bool val)
{
	int ret;
	struct bt_object *bool_obj = NULL;

	bool_obj = bt_object_bool_create_init(val);
	ret = bt_object_map_insert(map_obj, key, bool_obj);
	bt_object_put(bool_obj);

	return ret;
}

int bt_object_map_insert_integer(struct bt_object *map_obj,
	const char *key, int64_t val)
{
	int ret;
	struct bt_object *integer_obj = NULL;

	integer_obj = bt_object_integer_create_init(val);
	ret = bt_object_map_insert(map_obj, key, integer_obj);
	bt_object_put(integer_obj);

	return ret;
}

int bt_object_map_insert_float(struct bt_object *map_obj,
	const char *key, double val)
{
	int ret;
	struct bt_object *float_obj = NULL;

	float_obj = bt_object_float_create_init(val);
	ret = bt_object_map_insert(map_obj, key, float_obj);
	bt_object_put(float_obj);

	return ret;
}

int bt_object_map_insert_string(struct bt_object *map_obj,
	const char *key, const char *val)
{
	int ret;
	struct bt_object *string_obj = NULL;

	string_obj = bt_object_string_create_init(val);
	ret = bt_object_map_insert(map_obj, key, string_obj);
	bt_object_put(string_obj);

	return ret;
}

int bt_object_map_insert_array(struct bt_object *map_obj,
	const char *key)
{
	int ret;
	struct bt_object *array_obj = NULL;

	array_obj = bt_object_array_create();
	ret = bt_object_map_insert(map_obj, key, array_obj);
	bt_object_put(array_obj);

	return ret;
}

int bt_object_map_insert_map(struct bt_object *map_obj,
	const char *key)
{
	int ret;
	struct bt_object *empty_map_obj = NULL;

	empty_map_obj = bt_object_map_create();
	ret = bt_object_map_insert(map_obj, key, empty_map_obj);
	bt_object_put(empty_map_obj);

	return ret;
}

int bt_object_map_foreach(const struct bt_object *map_obj,
	bt_object_map_foreach_cb cb, void *data)
{
	int ret = 0;
	gpointer key, element_obj;
	GHashTableIter iter;
	struct bt_object_map *typed_map_obj = BT_OBJECT_TO_MAP(map_obj);

	if (!map_obj || !bt_object_is_map(map_obj) || !cb) {
		ret = -1;
		goto end;
	}

	g_hash_table_iter_init(&iter, typed_map_obj->ght);

	while (g_hash_table_iter_next(&iter, &key, &element_obj)) {
		const char *key_str = g_quark_to_string((unsigned long) key);

		if (!cb(key_str, element_obj, data)) {
			break;
		}
	}

end:
	return ret;
}

struct bt_object *bt_object_copy(const struct bt_object *object)
{
	struct bt_object *copy_obj = NULL;

	if (!object) {
		goto end;
	}

	copy_obj = copy_funcs[object->type](object);

end:
	return copy_obj;
}

bool bt_object_compare(const struct bt_object *object_a,
	const struct bt_object *object_b)
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
