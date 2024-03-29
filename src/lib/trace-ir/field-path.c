/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2016-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#define BT_LOG_TAG "LIB/FIELD-PATH"
#include "lib/logging.h"

#include "lib/assert-cond.h"
#include <babeltrace2/trace-ir/field-class.h>
#include <babeltrace2/trace-ir/field-path.h>
#include <stdint.h>
#include "common/assert.h"
#include <glib.h>

#include "field-path.h"

static
void destroy_field_path(struct bt_object *obj)
{
	struct bt_field_path *field_path = (struct bt_field_path *) obj;

	BT_ASSERT(field_path);
	BT_LIB_LOGD("Destroying field path: %!+P", field_path);
	g_array_free(field_path->items, TRUE);
	field_path->items = NULL;
	g_free(field_path);
}

struct bt_field_path *bt_field_path_create(void)
{
	struct bt_field_path *field_path = NULL;

	BT_LOGD_STR("Creating empty field path object.");

	field_path = g_new0(struct bt_field_path, 1);
	if (!field_path) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate one field path.");
		goto error;
	}

	bt_object_init_shared(&field_path->base, destroy_field_path);
	field_path->items = g_array_new(FALSE, FALSE,
		sizeof(struct bt_field_path_item));
	if (!field_path->items) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate a GArray.");
		goto error;
	}

	BT_LIB_LOGD("Created empty field path object: %!+P", field_path);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(field_path);

end:
	return field_path;
}

BT_EXPORT
enum bt_field_path_scope bt_field_path_get_root_scope(
		const struct bt_field_path *field_path)
{
	BT_ASSERT_PRE_DEV_FP_NON_NULL(field_path);
	return field_path->root;
}

BT_EXPORT
uint64_t bt_field_path_get_item_count(const struct bt_field_path *field_path)
{
	BT_ASSERT_PRE_DEV_FP_NON_NULL(field_path);
	return (uint64_t) field_path->items->len;
}

BT_EXPORT
const struct bt_field_path_item *bt_field_path_borrow_item_by_index_const(
		const struct bt_field_path *field_path, uint64_t index)
{
	BT_ASSERT_PRE_DEV_FP_NON_NULL(field_path);
	BT_ASSERT_PRE_DEV_VALID_INDEX(index, field_path->items->len);
	return bt_field_path_borrow_item_by_index_inline(field_path, index);
}

BT_EXPORT
enum bt_field_path_item_type bt_field_path_item_get_type(
		const struct bt_field_path_item *field_path_item)
{
	BT_ASSERT_PRE_DEV_NON_NULL("field-path-item", field_path_item,
		"Field path item");
	return field_path_item->type;
}

BT_EXPORT
uint64_t bt_field_path_item_index_get_index(
		const struct bt_field_path_item *field_path_item)
{
	BT_ASSERT_PRE_DEV_NON_NULL("field-path-item", field_path_item,
		"Field path item");
	BT_ASSERT_PRE_DEV("is-index-field-path-item",
		field_path_item->type == BT_FIELD_PATH_ITEM_TYPE_INDEX,
		"Field path item is not an index field path item: "
		"addr=%p, type=%s", field_path_item,
		bt_field_path_item_type_string(field_path_item->type));
	return 	field_path_item->index;
}

BT_EXPORT
void bt_field_path_get_ref(const struct bt_field_path *field_path)
{
	bt_object_get_ref(field_path);
}

BT_EXPORT
void bt_field_path_put_ref(const struct bt_field_path *field_path)
{
	bt_object_put_ref(field_path);
}
