/*
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Author: Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#define BT_LOG_TAG "CLOCK-CLASS"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/compat/uuid-internal.h>
#include <babeltrace/trace-ir/clock-class-internal.h>
#include <babeltrace/trace-ir/clock-value-internal.h>
#include <babeltrace/trace-ir/utils-internal.h>
#include <babeltrace/object.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/types.h>
#include <babeltrace/compat/string-internal.h>
#include <inttypes.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/assert-internal.h>

#define BT_ASSERT_PRE_CLOCK_CLASS_HOT(_cc) \
	BT_ASSERT_PRE_HOT((_cc), "Clock class", ": %!+K", (_cc))

static
void destroy_clock_class(struct bt_object *obj)
{
	struct bt_clock_class *clock_class = (void *) obj;

	BT_LIB_LOGD("Destroying clock class: %!+K", clock_class);

	if (clock_class->name.str) {
		g_string_free(clock_class->name.str, TRUE);
	}

	if (clock_class->description.str) {
		g_string_free(clock_class->description.str, TRUE);
	}

	bt_object_pool_finalize(&clock_class->cv_pool);
	g_free(clock_class);
}

static
void free_clock_value(struct bt_clock_value *clock_value,
		struct bt_clock_class *clock_class)
{
	bt_clock_value_destroy(clock_value);
}

static inline
void set_base_offset(struct bt_clock_class *clock_class)
{
	uint64_t offset_cycles_ns;

	/* Initialize nanosecond timestamp to clock's offset in seconds */
	if (clock_class->offset_seconds <= (INT64_MIN / INT64_C(1000000000) - 1) ||
			clock_class->offset_seconds >= (INT64_MAX / INT64_C(1000000000)) - 1) {
		/*
		 * Overflow: offset in seconds converted to nanoseconds
		 * is outside the int64_t range. We also subtract 1 here
		 * to leave "space" for the offset in cycles converted
		 * to nanoseconds (which is always less than 1 second by
		 * contract).
		 */
		clock_class->base_offset.overflows = true;
		goto end;
	}

	/* Offset (seconds) to nanoseconds */
	clock_class->base_offset.value_ns = clock_class->offset_seconds *
		INT64_C(1000000000);

	/* Add offset in cycles */
	BT_ASSERT(clock_class->offset_cycles < clock_class->frequency);
	offset_cycles_ns = bt_util_ns_from_value(clock_class->frequency,
		clock_class->offset_cycles);
	BT_ASSERT(offset_cycles_ns < 1000000000);
	clock_class->base_offset.value_ns += (int64_t) offset_cycles_ns;
	clock_class->base_offset.overflows = false;

end:
	return;
}

struct bt_private_clock_class *bt_private_clock_class_create(void)
{
	int ret;
	struct bt_clock_class *clock_class = NULL;

	BT_LOGD_STR("Creating default clock class object");

	clock_class = g_new0(struct bt_clock_class, 1);
	if (!clock_class) {
		BT_LOGE_STR("Failed to allocate one clock class.");
		goto error;
	}

	bt_object_init_shared(&clock_class->base, destroy_clock_class);
	clock_class->name.str = g_string_new(NULL);
	if (!clock_class->name.str) {
		BT_LOGE_STR("Failed to allocate a GString.");
		goto error;
	}

	clock_class->description.str = g_string_new(NULL);
	if (!clock_class->description.str) {
		BT_LOGE_STR("Failed to allocate a GString.");
		goto error;
	}

	clock_class->frequency = UINT64_C(1000000000);
	clock_class->is_absolute = BT_TRUE;
	set_base_offset(clock_class);
	ret = bt_object_pool_initialize(&clock_class->cv_pool,
		(bt_object_pool_new_object_func) bt_clock_value_new,
		(bt_object_pool_destroy_object_func)
			free_clock_value,
		clock_class);
	if (ret) {
		BT_LOGE("Failed to initialize clock value pool: ret=%d",
			ret);
		goto error;
	}

	BT_LIB_LOGD("Created clock class object: %!+K", clock_class);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(clock_class);

end:
	return (void *) clock_class;
}

const char *bt_clock_class_get_name(
		struct bt_clock_class *clock_class)
{
	BT_ASSERT_PRE_NON_NULL(clock_class, "Clock class");
	return clock_class->name.value;
}

int bt_private_clock_class_set_name(
		struct bt_private_clock_class *priv_clock_class,
		const char *name)
{
	struct bt_clock_class *clock_class = (void *) priv_clock_class;

	BT_ASSERT_PRE_NON_NULL(clock_class, "Clock class");
	BT_ASSERT_PRE_NON_NULL(name, "Name");
	BT_ASSERT_PRE_CLOCK_CLASS_HOT(clock_class);
	g_string_assign(clock_class->name.str, name);
	clock_class->name.value = clock_class->name.str->str;
	BT_LIB_LOGV("Set clock class's name: %!+K", clock_class);
	return 0;
}

const char *bt_clock_class_get_description(struct bt_clock_class *clock_class)
{
	BT_ASSERT_PRE_NON_NULL(clock_class, "Clock class");
	return clock_class->description.value;
}

int bt_private_clock_class_set_description(
		struct bt_private_clock_class *priv_clock_class,
		const char *descr)
{
	struct bt_clock_class *clock_class = (void *) priv_clock_class;

	BT_ASSERT_PRE_NON_NULL(clock_class, "Clock class");
	BT_ASSERT_PRE_NON_NULL(descr, "Description");
	BT_ASSERT_PRE_CLOCK_CLASS_HOT(clock_class);
	g_string_assign(clock_class->description.str, descr);
	clock_class->description.value = clock_class->description.str->str;
	BT_LIB_LOGV("Set clock class's description: %!+K",
		clock_class);
	return 0;
}

uint64_t bt_clock_class_get_frequency(struct bt_clock_class *clock_class)
{
	BT_ASSERT_PRE_NON_NULL(clock_class, "Clock class");
	return clock_class->frequency;
}

int bt_private_clock_class_set_frequency(
		struct bt_private_clock_class *priv_clock_class,
		uint64_t frequency)
{
	struct bt_clock_class *clock_class = (void *) priv_clock_class;

	BT_ASSERT_PRE_NON_NULL(clock_class, "Clock class");
	BT_ASSERT_PRE_CLOCK_CLASS_HOT(clock_class);
	BT_ASSERT_PRE(frequency != UINT64_C(-1) && frequency != 0,
		"Invalid frequency: %![cc-]+K, new-freq=%" PRIu64,
		clock_class, frequency);
	BT_ASSERT_PRE(clock_class->offset_cycles < frequency,
		"Offset (cycles) is greater than clock class's frequency: "
		"%![cc-]+K, new-freq=%" PRIu64, clock_class, frequency);
	clock_class->frequency = frequency;
	set_base_offset(clock_class);
	BT_LIB_LOGV("Set clock class's frequency: %!+K", clock_class);
	return 0;
}

uint64_t bt_clock_class_get_precision(struct bt_clock_class *clock_class)
{
	BT_ASSERT_PRE_NON_NULL(clock_class, "Clock class");
	return clock_class->precision;
}

int bt_private_clock_class_set_precision(
		struct bt_private_clock_class *priv_clock_class,
		uint64_t precision)
{
	struct bt_clock_class *clock_class = (void *) priv_clock_class;

	BT_ASSERT_PRE_NON_NULL(clock_class, "Clock class");
	BT_ASSERT_PRE_CLOCK_CLASS_HOT(clock_class);
	BT_ASSERT_PRE(precision != UINT64_C(-1),
		"Invalid precision: %![cc-]+K, new-precision=%" PRIu64,
		clock_class, precision);
	clock_class->precision = precision;
	BT_LIB_LOGV("Set clock class's precision: %!+K", clock_class);
	return 0;
}

void bt_clock_class_get_offset(struct bt_clock_class *clock_class,
		int64_t *seconds, uint64_t *cycles)
{
	BT_ASSERT_PRE_NON_NULL(clock_class, "Clock class");
	BT_ASSERT_PRE_NON_NULL(seconds, "Seconds (output)");
	BT_ASSERT_PRE_NON_NULL(cycles, "Cycles (output)");
	*seconds = clock_class->offset_seconds;
	*cycles = clock_class->offset_cycles;
}

int bt_private_clock_class_set_offset(
		struct bt_private_clock_class *priv_clock_class,
		int64_t seconds, uint64_t cycles)
{
	struct bt_clock_class *clock_class = (void *) priv_clock_class;

	BT_ASSERT_PRE_NON_NULL(clock_class, "Clock class");
	BT_ASSERT_PRE_CLOCK_CLASS_HOT(clock_class);
	BT_ASSERT_PRE(cycles < clock_class->frequency,
		"Offset (cycles) is greater than clock class's frequency: "
		"%![cc-]+K, new-offset-cycles=%" PRIu64, clock_class, cycles);
	clock_class->offset_seconds = seconds;
	clock_class->offset_cycles = cycles;
	set_base_offset(clock_class);
	BT_LIB_LOGV("Set clock class's offset: %!+K", clock_class);
	return 0;
}

bt_bool bt_clock_class_is_absolute(struct bt_clock_class *clock_class)
{
	BT_ASSERT_PRE_NON_NULL(clock_class, "Clock class");
	return (bool) clock_class->is_absolute;
}

int bt_private_clock_class_set_is_absolute(
		struct bt_private_clock_class *priv_clock_class,
		bt_bool is_absolute)
{
	struct bt_clock_class *clock_class = (void *) priv_clock_class;

	BT_ASSERT_PRE_NON_NULL(clock_class, "Clock class");
	BT_ASSERT_PRE_CLOCK_CLASS_HOT(clock_class);
	clock_class->is_absolute = (bool) is_absolute;
	BT_LIB_LOGV("Set clock class's absolute property: %!+K",
		clock_class);
	return 0;
}

bt_uuid bt_clock_class_get_uuid(struct bt_clock_class *clock_class)
{
	BT_ASSERT_PRE_NON_NULL(clock_class, "Clock class");
	return clock_class->uuid.value;
}

int bt_private_clock_class_set_uuid(
		struct bt_private_clock_class *priv_clock_class,
		bt_uuid uuid)
{
	struct bt_clock_class *clock_class = (void *) priv_clock_class;

	BT_ASSERT_PRE_NON_NULL(clock_class, "Clock class");
	BT_ASSERT_PRE_NON_NULL(uuid, "UUID");
	BT_ASSERT_PRE_CLOCK_CLASS_HOT(clock_class);
	memcpy(clock_class->uuid.uuid, uuid, BABELTRACE_UUID_LEN);
	clock_class->uuid.value = clock_class->uuid.uuid;
	BT_LIB_LOGV("Set clock class's UUID: %!+K", clock_class);
	return 0;
}

BT_HIDDEN
void _bt_clock_class_freeze(struct bt_clock_class *clock_class)
{
	BT_ASSERT(clock_class);

	if (clock_class->frozen) {
		return;
	}

	BT_LIB_LOGD("Freezing clock class: %!+K", clock_class);
	clock_class->frozen = 1;
}

int bt_clock_class_cycles_to_ns_from_origin(struct bt_clock_class *clock_class,
		uint64_t cycles, int64_t *ns)
{
	int ret;

	BT_ASSERT_PRE_NON_NULL(clock_class, "Clock class");
	BT_ASSERT_PRE_NON_NULL(ns, "Nanoseconds (output)");
	ret = bt_util_ns_from_origin(clock_class, cycles, ns);
	if (ret) {
		BT_LIB_LOGW("Cannot convert cycles to nanoseconds "
			"from origin for given clock class: "
			"value overflows the signed 64-bit integer range: "
			"%![cc-]+K, cycles=%" PRIu64,
			clock_class, cycles);
	}

	return ret;
}

struct bt_clock_class *bt_clock_class_borrow_from_private(
		struct bt_private_clock_class *priv_clock_class)
{
	return (void *) priv_clock_class;
}
