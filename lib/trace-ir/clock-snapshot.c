/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
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

#define BT_LOG_TAG "CLOCK-SNAPSHOT"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/compat/uuid-internal.h>
#include <babeltrace/trace-ir/clock-class-internal.h>
#include <babeltrace/trace-ir/clock-snapshot-internal.h>
#include <babeltrace/trace-ir/clock-snapshot-const.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/types.h>
#include <babeltrace/compat/string-internal.h>
#include <inttypes.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/assert-internal.h>

BT_HIDDEN
void bt_clock_snapshot_destroy(struct bt_clock_snapshot *clock_snapshot)
{
	BT_LIB_LOGD("Destroying clock snapshot: %!+k", clock_snapshot);
	BT_OBJECT_PUT_REF_AND_RESET(clock_snapshot->clock_class);
	g_free(clock_snapshot);
}

BT_HIDDEN
struct bt_clock_snapshot *bt_clock_snapshot_new(struct bt_clock_class *clock_class)
{
	struct bt_clock_snapshot *ret = NULL;

	BT_ASSERT(clock_class);
	BT_LIB_LOGD("Creating clock snapshot object: %![cc-]+K=",
		clock_class);
	ret = g_new0(struct bt_clock_snapshot, 1);
	if (!ret) {
		BT_LOGE_STR("Failed to allocate one clock snapshot.");
		goto end;
	}

	bt_object_init_unique(&ret->base);
	ret->clock_class = clock_class;
	bt_object_get_no_null_check(clock_class);
	bt_clock_class_freeze(clock_class);
	BT_LIB_LOGD("Created clock snapshot object: %!+k", ret);

end:
	return ret;
}

BT_HIDDEN
struct bt_clock_snapshot *bt_clock_snapshot_create(struct bt_clock_class *clock_class)
{
	struct bt_clock_snapshot *clock_snapshot = NULL;

	BT_ASSERT(clock_class);
	clock_snapshot = bt_object_pool_create_object(&clock_class->cs_pool);
	if (!clock_snapshot) {
		BT_LIB_LOGE("Cannot allocate one clock snapshot from clock class's clock snapshot pool: "
			"%![cc-]+K", clock_class);
		goto error;
	}

	if (likely(!clock_snapshot->clock_class)) {
		clock_snapshot->clock_class = clock_class;
		bt_object_get_no_null_check(clock_class);
	}

	goto end;

error:
	if (clock_snapshot) {
		bt_clock_snapshot_recycle(clock_snapshot);
		clock_snapshot = NULL;
	}

end:
	return clock_snapshot;
}

BT_HIDDEN
void bt_clock_snapshot_recycle(struct bt_clock_snapshot *clock_snapshot)
{
	struct bt_clock_class *clock_class;

	BT_ASSERT(clock_snapshot);
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
	BT_ASSERT(clock_class);
	clock_snapshot->clock_class = NULL;
	bt_object_pool_recycle_object(&clock_class->cs_pool, clock_snapshot);
	bt_object_put_ref(clock_class);
}

uint64_t bt_clock_snapshot_get_value(const struct bt_clock_snapshot *clock_snapshot)
{
	BT_ASSERT_PRE_NON_NULL(clock_snapshot, "Clock snapshot");
	BT_ASSERT_PRE(clock_snapshot->is_set,
		"Clock snapshot is not set: %!+k", clock_snapshot);
	return clock_snapshot->value_cycles;
}

enum bt_clock_snapshot_status bt_clock_snapshot_get_ns_from_origin(
		const struct bt_clock_snapshot *clock_snapshot,
		int64_t *ret_value_ns)
{
	int ret = BT_CLOCK_SNAPSHOT_STATUS_OK;

	BT_ASSERT_PRE_NON_NULL(clock_snapshot, "Clock snapshot");
	BT_ASSERT_PRE_NON_NULL(ret_value_ns, "Value (ns) (output)");
	BT_ASSERT_PRE(clock_snapshot->is_set,
		"Clock snapshot is not set: %!+k", clock_snapshot);

	if (clock_snapshot->ns_from_origin_overflows) {
		BT_LIB_LOGD("Clock snapshot, once converted to nanoseconds from origin, "
			"overflows the signed 64-bit integer range: "
			"%![cs-]+k", clock_snapshot);
		ret = BT_CLOCK_SNAPSHOT_STATUS_OVERFLOW;
		goto end;
	}

	*ret_value_ns = clock_snapshot->ns_from_origin;

end:
	return ret;
}

const struct bt_clock_class *bt_clock_snapshot_borrow_clock_class_const(
		const struct bt_clock_snapshot *clock_snapshot)
{
	BT_ASSERT_PRE_NON_NULL(clock_snapshot, "Clock snapshot");
	return clock_snapshot->clock_class;
}
