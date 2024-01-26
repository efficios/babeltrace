/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2019 Philippe Proulx <pproulx@efficios.com>
 */

#define BT_LOG_TAG "LIB/INTERRUPTER"
#include "lib/logging.h"

#include <babeltrace2/babeltrace.h>

#include "interrupter.h"
#include "lib/assert-cond.h"

static
void destroy_interrupter(struct bt_object *obj)
{
	g_free(obj);
}

BT_EXPORT
struct bt_interrupter *bt_interrupter_create(void)
{
	struct bt_interrupter *intr = g_new0(struct bt_interrupter, 1);

	BT_ASSERT_PRE_NO_ERROR();

	if (!intr) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate one interrupter.");
		goto error;
	}

	bt_object_init_shared(&intr->base, destroy_interrupter);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(intr);

end:
	return intr;
}

BT_EXPORT
void bt_interrupter_set(struct bt_interrupter *intr)
{
	BT_ASSERT_PRE_INTR_NON_NULL(intr);
	intr->is_set = true;
}

BT_EXPORT
void bt_interrupter_reset(struct bt_interrupter *intr)
{
	BT_ASSERT_PRE_INTR_NON_NULL(intr);
	intr->is_set = false;
}

BT_EXPORT
bt_bool bt_interrupter_is_set(const struct bt_interrupter *intr)
{
	BT_ASSERT_PRE_INTR_NON_NULL(intr);
	return (bt_bool) intr->is_set;
}

BT_EXPORT
void bt_interrupter_get_ref(const struct bt_interrupter *intr)
{
	bt_object_get_ref(intr);
}

BT_EXPORT
void bt_interrupter_put_ref(const struct bt_interrupter *intr)
{
	bt_object_put_ref(intr);
}
