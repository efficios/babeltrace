/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 */

#define BT_LOG_TAG "LIB/CLOCK-SNAPSHOT"
#include "lib/logging.h"

#include "lib/assert-cond.h"
#include "clock-class.h"
#include "clock-snapshot.h"
#include <babeltrace2/trace-ir/clock-snapshot.h>
#include "compat/compiler.h"
#include <babeltrace2/types.h>
#include "lib/object.h"
#include "common/assert.h"
#include "lib/func-status.h"

void bt_clock_snapshot_destroy(struct bt_clock_snapshot *clock_snapshot)
{
	BT_ASSERT(clock_snapshot);
	BT_LIB_LOGD("Destroying clock snapshot: %!+k", clock_snapshot);
	BT_OBJECT_PUT_REF_AND_RESET(clock_snapshot->clock_class);
	g_free(clock_snapshot);
}

struct bt_clock_snapshot *bt_clock_snapshot_new(
		struct bt_clock_class *clock_class)
{
	struct bt_clock_snapshot *ret = NULL;

	BT_ASSERT(clock_class);
	BT_LIB_LOGD("Creating clock snapshot object: %![cc-]+K=",
		clock_class);
	ret = g_new0(struct bt_clock_snapshot, 1);
	if (!ret) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to allocate one clock snapshot.");
		goto end;
	}

	bt_object_init_unique(&ret->base);
	ret->clock_class = clock_class;
	bt_object_get_ref_no_null_check(clock_class);
	bt_clock_class_freeze(clock_class);
	BT_LIB_LOGD("Created clock snapshot object: %!+k", ret);

end:
	return ret;
}

struct bt_clock_snapshot *bt_clock_snapshot_create(
		struct bt_clock_class *clock_class)
{
	struct bt_clock_snapshot *clock_snapshot = NULL;

	BT_ASSERT_DBG(clock_class);
	clock_snapshot = bt_object_pool_create_object(&clock_class->cs_pool);
	if (!clock_snapshot) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Cannot allocate one clock snapshot from clock class's clock snapshot pool: "
			"%![cc-]+K", clock_class);
		goto end;
	}

	if (G_LIKELY(!clock_snapshot->clock_class)) {
		clock_snapshot->clock_class = clock_class;
		bt_object_get_ref_no_null_check(clock_class);
	}

end:
	return clock_snapshot;
}

void bt_clock_snapshot_recycle(struct bt_clock_snapshot *clock_snapshot)
{
	struct bt_clock_class *clock_class;

	BT_ASSERT_DBG(clock_snapshot);
	BT_LIB_LOGD("Recycling clock snapshot: %!+k", clock_snapshot);

	/*
	 * Those are the important ordered steps:
	 *
	 * 1. Reset the clock snapshot object, but do NOT put its clock
	 *    class's reference. This clock class contains the pool to
	 *    which we're about to recycle this clock snapshot object,
	 *    so we must guarantee its existence thanks to this existing
	 *    reference.
	 *
	 * 2. Move the clock class reference to our `clock_class`
	 *    variable so that we can set the clock snapshot's clock
	 *    class member to NULL before recycling it. We CANNOT do
	 *    this after we put the clock class reference because this
	 *    bt_object_put_ref() could destroy the clock class, also
	 *    destroying its clock snapshot pool, thus also destroying
	 *    our clock snapshot object (this would result in an invalid
	 *    write access).
	 *
	 * 3. Recycle the clock snapshot object.
	 *
	 * 4. Put our clock class reference.
	 */
	bt_clock_snapshot_reset(clock_snapshot);
	clock_class = clock_snapshot->clock_class;
	BT_ASSERT_DBG(clock_class);
	clock_snapshot->clock_class = NULL;
	bt_object_pool_recycle_object(&clock_class->cs_pool, clock_snapshot);
	bt_object_put_ref(clock_class);
}

BT_EXPORT
uint64_t bt_clock_snapshot_get_value(
		const struct bt_clock_snapshot *clock_snapshot)
{
	BT_ASSERT_PRE_DEV_CS_NON_NULL(clock_snapshot);
	BT_ASSERT_DBG(clock_snapshot->is_set);
	return clock_snapshot->value_cycles;
}

BT_EXPORT
enum bt_clock_snapshot_get_ns_from_origin_status
bt_clock_snapshot_get_ns_from_origin(
		const struct bt_clock_snapshot *clock_snapshot,
		int64_t *ret_value_ns)
{
	int ret = BT_FUNC_STATUS_OK;

	BT_ASSERT_PRE_DEV_NO_ERROR();
	BT_ASSERT_PRE_DEV_CS_NON_NULL(clock_snapshot);
	BT_ASSERT_PRE_DEV_NON_NULL("value-ns-output", ret_value_ns,
		"Value (ns) (output)");
	BT_ASSERT_DBG(clock_snapshot->is_set);

	if (clock_snapshot->ns_from_origin_overflows) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Clock snapshot, once converted to nanoseconds from origin, "
			"overflows the signed 64-bit integer range: "
			"%![cs-]+k", clock_snapshot);
		ret = BT_FUNC_STATUS_OVERFLOW_ERROR;
		goto end;
	}

	*ret_value_ns = clock_snapshot->ns_from_origin;

end:
	return ret;
}

BT_EXPORT
const struct bt_clock_class *bt_clock_snapshot_borrow_clock_class_const(
		const struct bt_clock_snapshot *clock_snapshot)
{
	BT_ASSERT_PRE_DEV_CS_NON_NULL(clock_snapshot);
	return clock_snapshot->clock_class;
}
