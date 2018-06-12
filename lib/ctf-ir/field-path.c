/*
 * field-path.c
 *
 * Babeltrace CTF IR - Field path
 *
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2016 Philippe Proulx <pproulx@efficios.com>
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

#define BT_LOG_TAG "FIELD-PATH"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/ctf-ir/field-types.h>
#include <babeltrace/ctf-ir/field-types-internal.h>
#include <babeltrace/ctf-ir/field-path-internal.h>
#include <babeltrace/ctf-ir/field-path.h>
#include <limits.h>
#include <stdint.h>
#include <inttypes.h>
#include <babeltrace/assert-internal.h>
#include <glib.h>

static
void field_path_destroy(struct bt_object *obj)
{
	struct bt_field_path *field_path = (struct bt_field_path *) obj;

	BT_LOGD("Destroying field path: addr=%p", obj);

	if (!field_path) {
		return;
	}

	if (field_path->indexes) {
		g_array_free(field_path->indexes, TRUE);
	}
	g_free(field_path);
}

BT_HIDDEN
struct bt_field_path *bt_field_path_create(void)
{
	struct bt_field_path *field_path = NULL;

	BT_LOGD_STR("Creating empty field path object.");

	field_path = g_new0(struct bt_field_path, 1);
	if (!field_path) {
		BT_LOGE_STR("Failed to allocate one field path.");
		goto error;
	}

	bt_object_init_shared(&field_path->base, field_path_destroy);
	field_path->root = BT_SCOPE_UNKNOWN;
	field_path->indexes = g_array_new(TRUE, FALSE, sizeof(int));
	if (!field_path->indexes) {
		BT_LOGE_STR("Failed to allocate a GArray.");
		goto error;
	}

	BT_LOGD("Created empty field path object: addr=%p", field_path);
	return field_path;

error:
	BT_PUT(field_path);
	return NULL;
}

BT_HIDDEN
void bt_field_path_clear(struct bt_field_path *field_path)
{
	if (field_path->indexes->len > 0) {
		g_array_remove_range(field_path->indexes, 0,
			field_path->indexes->len);
	}
}

BT_HIDDEN
struct bt_field_path *bt_field_path_copy(
		struct bt_field_path *path)
{
	struct bt_field_path *new_path;

	BT_ASSERT(path);
	BT_LOGD("Copying field path: addr=%p, index-count=%u",
		path, path->indexes->len);
	new_path = bt_field_path_create();
	if (!new_path) {
		BT_LOGE_STR("Cannot create empty field path.");
		goto end;
	}

	new_path->root = path->root;
	g_array_insert_vals(new_path->indexes, 0,
		path->indexes->data, path->indexes->len);
	BT_LOGD("Copied field path: original-addr=%p, copy-addr=%p",
		path, new_path);
end:
	return new_path;
}

enum bt_scope bt_field_path_get_root_scope(
		const struct bt_field_path *field_path)
{
	enum bt_scope scope = BT_SCOPE_UNKNOWN;

	if (!field_path) {
		BT_LOGW_STR("Invalid parameter: field path is NULL.");
		goto end;
	}

	scope = field_path->root;

end:
	return scope;
}

int64_t bt_field_path_get_index_count(
		const struct bt_field_path *field_path)
{
	int64_t count = (int64_t) -1;

	if (!field_path) {
		BT_LOGW_STR("Invalid parameter: field path is NULL.");
		goto end;
	}

	count = (int64_t) field_path->indexes->len;

end:
	return count;
}

int bt_field_path_get_index(const struct bt_field_path *field_path,
		uint64_t index)
{
	int ret = INT_MIN;

	if (!field_path) {
		BT_LOGW_STR("Invalid parameter: field path is NULL.");
		goto end;
	}

	if (index >= field_path->indexes->len) {
		BT_LOGW("Invalid parameter: index is out of bounds: "
			"addr=%p, index=%" PRIu64 ", count=%u",
			field_path, index, field_path->indexes->len);
		goto end;
	}

	ret = g_array_index(field_path->indexes, int, index);

end:
	return ret;
}
