/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2016 Philippe Proulx <pproulx@efficios.com>
 *
 * Babeltrace CTF writer - Field path
 */

#define BT_LOG_TAG "CTF-WRITER/FIELD-PATH"
#include "logging.h"

#include <glib.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>

#include <babeltrace2-ctf-writer/field-types.h>

#include "common/assert.h"

#include "field-path.h"
#include "field-types.h"

static
void field_path_destroy(struct bt_ctf_object *obj)
{
	struct bt_ctf_field_path *field_path = (struct bt_ctf_field_path *) obj;

	BT_LOGD("Destroying field path: addr=%p", obj);

	if (!field_path) {
		return;
	}

	if (field_path->indexes) {
		g_array_free(field_path->indexes, TRUE);
	}
	g_free(field_path);
}

struct bt_ctf_field_path *bt_ctf_field_path_create(void)
{
	struct bt_ctf_field_path *field_path = NULL;

	BT_LOGD_STR("Creating empty field path object.");

	field_path = g_new0(struct bt_ctf_field_path, 1);
	if (!field_path) {
		BT_LOGE_STR("Failed to allocate one field path.");
		goto error;
	}

	bt_ctf_object_init_shared(&field_path->base, field_path_destroy);
	field_path->root = BT_CTF_SCOPE_UNKNOWN;
	field_path->indexes = g_array_new(TRUE, FALSE, sizeof(int));
	if (!field_path->indexes) {
		BT_LOGE_STR("Failed to allocate a GArray.");
		goto error;
	}

	BT_LOGD("Created empty field path object: addr=%p", field_path);
	return field_path;

error:
	BT_CTF_OBJECT_PUT_REF_AND_RESET(field_path);
	return NULL;
}

void bt_ctf_field_path_clear(struct bt_ctf_field_path *field_path)
{
	if (field_path->indexes->len > 0) {
		g_array_remove_range(field_path->indexes, 0,
			field_path->indexes->len);
	}
}

struct bt_ctf_field_path *bt_ctf_field_path_copy(
		struct bt_ctf_field_path *path)
{
	struct bt_ctf_field_path *new_path;

	BT_ASSERT_DBG(path);
	BT_LOGD("Copying field path: addr=%p, index-count=%u",
		path, path->indexes->len);
	new_path = bt_ctf_field_path_create();
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

enum bt_ctf_scope bt_ctf_field_path_get_root_scope(
		const struct bt_ctf_field_path *field_path)
{
	enum bt_ctf_scope scope = BT_CTF_SCOPE_UNKNOWN;

	if (!field_path) {
		BT_LOGW_STR("Invalid parameter: field path is NULL.");
		goto end;
	}

	scope = field_path->root;

end:
	return scope;
}

int64_t bt_ctf_field_path_get_index_count(
		const struct bt_ctf_field_path *field_path)
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

int bt_ctf_field_path_get_index(const struct bt_ctf_field_path *field_path,
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

	ret = bt_g_array_index(field_path->indexes, int, index);

end:
	return ret;
}
