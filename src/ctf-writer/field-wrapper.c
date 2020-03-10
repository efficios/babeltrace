/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2018 Philippe Proulx <pproulx@efficios.com>
 */

#define BT_LOG_TAG "CTF-WRITER/FIELD-WRAPPER"
#include "logging.h"

#include <glib.h>

#include "fields.h"
#include "field-wrapper.h"
#include "object.h"

BT_HIDDEN
struct bt_ctf_field_wrapper *bt_ctf_field_wrapper_new(void *data)
{
	struct bt_ctf_field_wrapper *field_wrapper =
		g_new0(struct bt_ctf_field_wrapper, 1);

	BT_LOGD_STR("Creating empty field wrapper object.");

	if (!field_wrapper) {
		BT_LOGE("Failed to allocate one field wrapper.");
		goto end;
	}

	bt_ctf_object_init_unique(&field_wrapper->base);
	BT_LOGD("Created empty field wrapper object: addr=%p",
		field_wrapper);

end:
	return field_wrapper;
}

BT_HIDDEN
void bt_ctf_field_wrapper_destroy(struct bt_ctf_field_wrapper *field_wrapper)
{
	BT_LOGD("Destroying field wrapper: addr=%p", field_wrapper);
	BT_ASSERT_DBG(!field_wrapper->field);
	BT_LOGD_STR("Putting stream class.");
	g_free(field_wrapper);
}

BT_HIDDEN
struct bt_ctf_field_wrapper *bt_ctf_field_wrapper_create(
		struct bt_ctf_object_pool *pool, struct bt_ctf_field_type *ft)
{
	struct bt_ctf_field_wrapper *field_wrapper = NULL;

	BT_ASSERT_DBG(pool);
	BT_ASSERT_DBG(ft);
	field_wrapper = bt_ctf_object_pool_create_object(pool);
	if (!field_wrapper) {
		BT_LOGE("Cannot allocate one field wrapper");
		goto end;
	}

	BT_ASSERT_DBG(field_wrapper->field);

end:
	return field_wrapper;
}
