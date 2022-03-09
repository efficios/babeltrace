/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2018 Philippe Proulx <pproulx@efficios.com>
 */

#define BT_LOG_TAG "LIB/FIELD-WRAPPER"
#include "lib/logging.h"

#include "lib/object-pool.h"
#include "lib/object.h"
#include <glib.h>

#include "field-wrapper.h"
#include "field.h"

struct bt_field_wrapper *bt_field_wrapper_new(void *data __attribute__((unused)))
{
	struct bt_field_wrapper *field_wrapper =
		g_new0(struct bt_field_wrapper, 1);

	BT_LOGD_STR("Creating empty field wrapper object.");

	if (!field_wrapper) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate one field wrapper.");
		goto end;
	}

	bt_object_init_unique(&field_wrapper->base);
	BT_LOGD("Created empty field wrapper object: addr=%p",
		field_wrapper);

end:
	return field_wrapper;
}

void bt_field_wrapper_destroy(struct bt_field_wrapper *field_wrapper)
{
	BT_LOGD("Destroying field wrapper: addr=%p", field_wrapper);

	if (field_wrapper->field) {
		BT_LOGD_STR("Destroying field.");
		bt_field_destroy((void *) field_wrapper->field);
		field_wrapper->field = NULL;
	}

	BT_LOGD_STR("Putting stream class.");
	g_free(field_wrapper);
}

struct bt_field_wrapper *bt_field_wrapper_create(
		struct bt_object_pool *pool, struct bt_field_class *fc)
{
	struct bt_field_wrapper *field_wrapper = NULL;

	BT_ASSERT_DBG(pool);
	BT_ASSERT_DBG(fc);
	field_wrapper = bt_object_pool_create_object(pool);
	if (!field_wrapper) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Cannot allocate one field wrapper from field wrapper pool: "
			"%![pool-]+o", pool);
		goto error;
	}

	if (!field_wrapper->field) {
		field_wrapper->field = (void *) bt_field_create(fc);
		if (!field_wrapper->field) {
			BT_LIB_LOGE_APPEND_CAUSE(
				"Cannot create field wrapper from field class: "
				"%![fc-]+F", fc);
			goto error;
		}

		BT_LIB_LOGD("Created initial field wrapper object: "
			"wrapper-addr=%p, %![field-]+f", field_wrapper,
			field_wrapper->field);
	}

	goto end;

error:
	if (field_wrapper) {
		bt_field_wrapper_destroy(field_wrapper);
		field_wrapper = NULL;
	}

end:
	return field_wrapper;
}
