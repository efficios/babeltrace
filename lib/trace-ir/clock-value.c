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

#define BT_LOG_TAG "CLOCK-VALUE"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/compat/uuid-internal.h>
#include <babeltrace/trace-ir/clock-class-internal.h>
#include <babeltrace/trace-ir/clock-value-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/types.h>
#include <babeltrace/compat/string-internal.h>
#include <inttypes.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/assert-internal.h>

BT_HIDDEN
void bt_clock_value_destroy(struct bt_clock_value *clock_value)
{
	BT_LIB_LOGD("Destroying clock value: %!+k", clock_value);
	bt_put(clock_value->clock_class);
	g_free(clock_value);
}

BT_HIDDEN
struct bt_clock_value *bt_clock_value_new(struct bt_clock_class *clock_class)
{
	struct bt_clock_value *ret = NULL;

	BT_ASSERT(clock_class);
	BT_LIB_LOGD("Creating clock value object: %![cc-]+K=",
		clock_class);
	ret = g_new0(struct bt_clock_value, 1);
	if (!ret) {
		BT_LOGE_STR("Failed to allocate one clock value.");
		goto end;
	}

	bt_object_init_unique(&ret->base);
	ret->clock_class = bt_get(clock_class);
	bt_clock_class_freeze(clock_class);
	BT_LIB_LOGD("Created clock value object: %!+k", ret);

end:
	return ret;
}

BT_HIDDEN
struct bt_clock_value *bt_clock_value_create(struct bt_clock_class *clock_class)
{
	struct bt_clock_value *clock_value = NULL;

	BT_ASSERT(clock_class);
	clock_value = bt_object_pool_create_object(&clock_class->cv_pool);
	if (!clock_value) {
		BT_LIB_LOGE("Cannot allocate one clock value from clock class's clock value pool: "
			"%![cc-]+K", clock_class);
		goto error;
	}

	if (likely(!clock_value->clock_class)) {
		clock_value->clock_class = bt_get(clock_class);
	}

	goto end;

error:
	if (clock_value) {
		bt_clock_value_recycle(clock_value);
		clock_value = NULL;
	}

end:
	return clock_value;
}

BT_HIDDEN
void bt_clock_value_recycle(struct bt_clock_value *clock_value)
{
	struct bt_clock_class *clock_class;

	BT_ASSERT(clock_value);
	BT_LIB_LOGD("Recycling clock value: %!+k", clock_value);

	/*
	 * Those are the important ordered steps:
	 *
	 * 1. Reset the clock value object, but do NOT put its clock
	 *    class's reference. This clock class contains the pool to
	 *    which we're about to recycle this clock value object, so
	 *    we must guarantee its existence thanks to this existing
	 *    reference.
	 *
	 * 2. Move the clock class reference to our `clock_class`
	 *    variable so that we can set the clock value's clock class
	 *    member to NULL before recycling it. We CANNOT do this
	 *    after we put the clock class reference because this
	 *    bt_put() could destroy the clock class, also destroying
	 *    its clock value pool, thus also destroying our clock value
	 *    object (this would result in an invalid write access).
	 *
	 * 3. Recycle the clock value object.
	 *
	 * 4. Put our clock class reference.
	 */
	bt_clock_value_reset(clock_value);
	clock_class = clock_value->clock_class;
	BT_ASSERT(clock_class);
	clock_value->clock_class = NULL;
	bt_object_pool_recycle_object(&clock_class->cv_pool, clock_value);
	bt_put(clock_class);
}

uint64_t bt_clock_value_get_value(struct bt_clock_value *clock_value)
{
	BT_ASSERT_PRE_NON_NULL(clock_value, "Clock value");
	BT_ASSERT_PRE(clock_value->is_set,
		"Clock value is not set: %!+k", clock_value);
	return clock_value->value_cycles;
}

int bt_clock_value_get_ns_from_origin(struct bt_clock_value *clock_value,
		int64_t *ret_value_ns)
{
	int ret = 0;
	BT_ASSERT_PRE_NON_NULL(clock_value, "Clock value");
	BT_ASSERT_PRE_NON_NULL(ret_value_ns, "Value (ns) (output)");
	BT_ASSERT_PRE(clock_value->is_set,
		"Clock value is not set: %!+k", clock_value);

	if (clock_value->ns_from_origin_overflows) {
		BT_LIB_LOGD("Clock value, once converted to nanoseconds from origin, "
			"overflows the signed 64-bit integer range: "
			"%![cv-]+k", clock_value);
		ret = -1;
		goto end;
	}

	*ret_value_ns = clock_value->ns_from_origin;

end:
	return ret;
}

struct bt_clock_class *bt_clock_value_borrow_clock_class(
		struct bt_clock_value *clock_value)
{
	BT_ASSERT_PRE_NON_NULL(clock_value, "Clock value");
	return clock_value->clock_class;
}
