/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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
#include <babeltrace/trace-ir/clock-class-const.h>
#include <babeltrace/trace-ir/clock-class.h>
#include <babeltrace/trace-ir/clock-class-internal.h>
#include <babeltrace/trace-ir/clock-snapshot-internal.h>
#include <babeltrace/trace-ir/utils-internal.h>
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
		clock_class->name.str = NULL;
		clock_class->name.value = NULL;
	}

	if (clock_class->description.str) {
		g_string_free(clock_class->description.str, TRUE);
		clock_class->description.str = NULL;
		clock_class->description.value = NULL;
	}

	bt_object_pool_finalize(&clock_class->cs_pool);
	g_free(clock_class);
}

static
void free_clock_snapshot(struct bt_clock_snapshot *clock_snapshot,
		struct bt_clock_class *clock_class)
{
	bt_clock_snapshot_destroy(clock_snapshot);
}

static inline
void set_base_offset(struct bt_clock_class *clock_class)
{
	clock_class->base_offset.overflows = bt_util_get_base_offset_ns(
		clock_class->offset_seconds, clock_class->offset_cycles,
		clock_class->frequency, &clock_class->base_offset.value_ns);
}

struct bt_clock_class *bt_clock_class_create(bt_trace_class *trace_class)
{
	int ret;
	struct bt_clock_class *clock_class = NULL;

	BT_ASSERT_PRE_NON_NULL(trace_class, "Trace class");
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
	clock_class->origin_is_unix_epoch = BT_TRUE;
	set_base_offset(clock_class);
	ret = bt_object_pool_initialize(&clock_class->cs_pool,
		(bt_object_pool_new_object_func) bt_clock_snapshot_new,
		(bt_object_pool_destroy_object_func)
			free_clock_snapshot,
		clock_class);
	if (ret) {
		BT_LOGE("Failed to initialize clock snapshot pool: ret=%d",
			ret);
		goto error;
	}

	BT_LIB_LOGD("Created clock class object: %!+K", clock_class);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(clock_class);

end:
	return clock_class;
}

const char *bt_clock_class_get_name(const struct bt_clock_class *clock_class)
{
	BT_ASSERT_PRE_NON_NULL(clock_class, "Clock class");
	return clock_class->name.value;
}

enum bt_clock_class_status bt_clock_class_set_name(
		struct bt_clock_class *clock_class, const char *name)
{
	BT_ASSERT_PRE_NON_NULL(clock_class, "Clock class");
	BT_ASSERT_PRE_NON_NULL(name, "Name");
	BT_ASSERT_PRE_CLOCK_CLASS_HOT(clock_class);
	g_string_assign(clock_class->name.str, name);
	clock_class->name.value = clock_class->name.str->str;
	BT_LIB_LOGV("Set clock class's name: %!+K", clock_class);
	return BT_CLOCK_CLASS_STATUS_OK;
}

const char *bt_clock_class_get_description(
		const struct bt_clock_class *clock_class)
{
	BT_ASSERT_PRE_NON_NULL(clock_class, "Clock class");
	return clock_class->description.value;
}

enum bt_clock_class_status bt_clock_class_set_description(
		struct bt_clock_class *clock_class, const char *descr)
{
	BT_ASSERT_PRE_NON_NULL(clock_class, "Clock class");
	BT_ASSERT_PRE_NON_NULL(descr, "Description");
	BT_ASSERT_PRE_CLOCK_CLASS_HOT(clock_class);
	g_string_assign(clock_class->description.str, descr);
	clock_class->description.value = clock_class->description.str->str;
	BT_LIB_LOGV("Set clock class's description: %!+K",
		clock_class);
	return BT_CLOCK_CLASS_STATUS_OK;
}

uint64_t bt_clock_class_get_frequency(const struct bt_clock_class *clock_class)
{
	BT_ASSERT_PRE_NON_NULL(clock_class, "Clock class");
	return clock_class->frequency;
}

void bt_clock_class_set_frequency(struct bt_clock_class *clock_class,
		uint64_t frequency)
{
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
}

uint64_t bt_clock_class_get_precision(const struct bt_clock_class *clock_class)
{
	BT_ASSERT_PRE_NON_NULL(clock_class, "Clock class");
	return clock_class->precision;
}

void bt_clock_class_set_precision(struct bt_clock_class *clock_class,
		uint64_t precision)
{
	BT_ASSERT_PRE_NON_NULL(clock_class, "Clock class");
	BT_ASSERT_PRE_CLOCK_CLASS_HOT(clock_class);
	BT_ASSERT_PRE(precision != UINT64_C(-1),
		"Invalid precision: %![cc-]+K, new-precision=%" PRIu64,
		clock_class, precision);
	clock_class->precision = precision;
	BT_LIB_LOGV("Set clock class's precision: %!+K", clock_class);
}

void bt_clock_class_get_offset(const struct bt_clock_class *clock_class,
		int64_t *seconds, uint64_t *cycles)
{
	BT_ASSERT_PRE_NON_NULL(clock_class, "Clock class");
	BT_ASSERT_PRE_NON_NULL(seconds, "Seconds (output)");
	BT_ASSERT_PRE_NON_NULL(cycles, "Cycles (output)");
	*seconds = clock_class->offset_seconds;
	*cycles = clock_class->offset_cycles;
}

void bt_clock_class_set_offset(struct bt_clock_class *clock_class,
		int64_t seconds, uint64_t cycles)
{
	BT_ASSERT_PRE_NON_NULL(clock_class, "Clock class");
	BT_ASSERT_PRE_CLOCK_CLASS_HOT(clock_class);
	BT_ASSERT_PRE(cycles < clock_class->frequency,
		"Offset (cycles) is greater than clock class's frequency: "
		"%![cc-]+K, new-offset-cycles=%" PRIu64, clock_class, cycles);
	clock_class->offset_seconds = seconds;
	clock_class->offset_cycles = cycles;
	set_base_offset(clock_class);
	BT_LIB_LOGV("Set clock class's offset: %!+K", clock_class);
}

bt_bool bt_clock_class_origin_is_unix_epoch(const struct bt_clock_class *clock_class)
{
	BT_ASSERT_PRE_NON_NULL(clock_class, "Clock class");
	return (bool) clock_class->origin_is_unix_epoch;
}

void bt_clock_class_set_origin_is_unix_epoch(struct bt_clock_class *clock_class,
		bt_bool origin_is_unix_epoch)
{
	BT_ASSERT_PRE_NON_NULL(clock_class, "Clock class");
	BT_ASSERT_PRE_CLOCK_CLASS_HOT(clock_class);
	clock_class->origin_is_unix_epoch = (bool) origin_is_unix_epoch;
	BT_LIB_LOGV("Set clock class's origin is Unix epoch property: %!+K",
		clock_class);
}

bt_uuid bt_clock_class_get_uuid(const struct bt_clock_class *clock_class)
{
	BT_ASSERT_PRE_NON_NULL(clock_class, "Clock class");
	return clock_class->uuid.value;
}

void bt_clock_class_set_uuid(struct bt_clock_class *clock_class,
		bt_uuid uuid)
{
	BT_ASSERT_PRE_NON_NULL(clock_class, "Clock class");
	BT_ASSERT_PRE_NON_NULL(uuid, "UUID");
	BT_ASSERT_PRE_CLOCK_CLASS_HOT(clock_class);
	memcpy(clock_class->uuid.uuid, uuid, BABELTRACE_UUID_LEN);
	clock_class->uuid.value = clock_class->uuid.uuid;
	BT_LIB_LOGV("Set clock class's UUID: %!+K", clock_class);
}

BT_HIDDEN
void _bt_clock_class_freeze(const struct bt_clock_class *clock_class)
{
	BT_ASSERT(clock_class);

	if (clock_class->frozen) {
		return;
	}

	BT_LIB_LOGD("Freezing clock class: %!+K", clock_class);
	((struct bt_clock_class *) clock_class)->frozen = 1;
}

enum bt_clock_class_status bt_clock_class_cycles_to_ns_from_origin(
		const struct bt_clock_class *clock_class,
		uint64_t cycles, int64_t *ns)
{
	int ret;

	BT_ASSERT_PRE_NON_NULL(clock_class, "Clock class");
	BT_ASSERT_PRE_NON_NULL(ns, "Nanoseconds (output)");
	ret = bt_util_ns_from_origin_clock_class(clock_class, cycles, ns);
	if (ret) {
		ret = BT_CLOCK_CLASS_STATUS_OVERFLOW;
		BT_LIB_LOGW("Cannot convert cycles to nanoseconds "
			"from origin for given clock class: "
			"value overflows the signed 64-bit integer range: "
			"%![cc-]+K, cycles=%" PRIu64,
			clock_class, cycles);
	}

	return ret;
}

void bt_clock_class_get_ref(const struct bt_clock_class *clock_class)
{
	bt_object_get_ref(clock_class);
}

void bt_clock_class_put_ref(const struct bt_clock_class *clock_class)
{
	bt_object_put_ref(clock_class);
}
