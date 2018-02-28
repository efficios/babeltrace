/*
 * clock.c
 *
 * Babeltrace CTF IR - Clock
 *
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
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

#define BT_LOG_TAG "CTF-WRITER-CLOCK"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/ctf-writer/clock-internal.h>
#include <babeltrace/ctf-ir/clock-class.h>
#include <babeltrace/ctf-ir/clock-class-internal.h>
#include <babeltrace/ctf-ir/utils.h>
#include <babeltrace/compat/uuid-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/assert-pre-internal.h>
#include <inttypes.h>

static
void bt_ctf_clock_destroy(struct bt_object *obj);

struct bt_ctf_clock *bt_ctf_clock_create(const char *name)
{
	int ret;
	struct bt_ctf_clock *clock = NULL;
	unsigned char cc_uuid[BABELTRACE_UUID_LEN];

	BT_ASSERT_PRE_NON_NULL(name, "Name");
	clock = g_new0(struct bt_ctf_clock, 1);
	if (!clock) {
		goto error;
	}

	bt_object_init(clock, bt_ctf_clock_destroy);
	clock->value = 0;

	/* Pre-2.0.0 backward compatibility: default frequency is 1 GHz */
	clock->clock_class = bt_clock_class_create(name, 1000000000);
	if (!clock->clock_class) {
		goto error;
	}

	/* Automatically set clock class's UUID. */
	ret = bt_uuid_generate(cc_uuid);
	if (ret) {
		goto error;
	}

	ret = bt_clock_class_set_uuid(clock->clock_class, cc_uuid);
	BT_ASSERT(ret == 0);
	return clock;

error:
	BT_PUT(clock);
	return clock;
}

const char *bt_ctf_clock_get_name(struct bt_ctf_clock *clock)
{
	BT_ASSERT_PRE_NON_NULL(clock, "CTF writer clock");
	return bt_clock_class_get_name(clock->clock_class);
}

const char *bt_ctf_clock_get_description(struct bt_ctf_clock *clock)
{
	BT_ASSERT_PRE_NON_NULL(clock, "CTF writer clock");
	return bt_clock_class_get_description(clock->clock_class);
}

int bt_ctf_clock_set_description(struct bt_ctf_clock *clock, const char *desc)
{
	BT_ASSERT_PRE_NON_NULL(clock, "CTF writer clock");
	return bt_clock_class_set_description(clock->clock_class, desc);
}

uint64_t bt_ctf_clock_get_frequency(struct bt_ctf_clock *clock)
{
	BT_ASSERT_PRE_NON_NULL(clock, "CTF writer clock");
	return bt_clock_class_get_frequency(clock->clock_class);
}

int bt_ctf_clock_set_frequency(struct bt_ctf_clock *clock, uint64_t freq)
{
	BT_ASSERT_PRE_NON_NULL(clock, "CTF writer clock");
	return bt_clock_class_set_frequency(clock->clock_class, freq);
}

uint64_t bt_ctf_clock_get_precision(struct bt_ctf_clock *clock)
{
	BT_ASSERT_PRE_NON_NULL(clock, "CTF writer clock");
	return bt_clock_class_get_precision(clock->clock_class);
}

int bt_ctf_clock_set_precision(struct bt_ctf_clock *clock, uint64_t precision)
{
	BT_ASSERT_PRE_NON_NULL(clock, "CTF writer clock");
	return bt_clock_class_set_precision(clock->clock_class, precision);
}

int bt_ctf_clock_get_offset_s(struct bt_ctf_clock *clock, int64_t *offset_s)
{
	BT_ASSERT_PRE_NON_NULL(clock, "CTF writer clock");
	return bt_clock_class_get_offset_s(clock->clock_class, offset_s);
}

int bt_ctf_clock_set_offset_s(struct bt_ctf_clock *clock, int64_t offset_s)
{
	BT_ASSERT_PRE_NON_NULL(clock, "CTF writer clock");
	return bt_clock_class_set_offset_s(clock->clock_class, offset_s);
}

int bt_ctf_clock_get_offset(struct bt_ctf_clock *clock, int64_t *offset)
{
	BT_ASSERT_PRE_NON_NULL(clock, "CTF writer clock");
	return bt_clock_class_get_offset_cycles(clock->clock_class, offset);
}

int bt_ctf_clock_set_offset(struct bt_ctf_clock *clock, int64_t offset)
{
	BT_ASSERT_PRE_NON_NULL(clock, "CTF writer clock");
	return bt_clock_class_set_offset_cycles(clock->clock_class, offset);
}

int bt_ctf_clock_get_is_absolute(struct bt_ctf_clock *clock)
{
	BT_ASSERT_PRE_NON_NULL(clock, "CTF writer clock");
	return bt_clock_class_is_absolute(clock->clock_class);
}

int bt_ctf_clock_set_is_absolute(struct bt_ctf_clock *clock, int is_absolute)
{
	BT_ASSERT_PRE_NON_NULL(clock, "CTF writer clock");
	return bt_clock_class_set_is_absolute(clock->clock_class, is_absolute);
}

const unsigned char *bt_ctf_clock_get_uuid(struct bt_ctf_clock *clock)
{
	BT_ASSERT_PRE_NON_NULL(clock, "CTF writer clock");
	return bt_clock_class_get_uuid(clock->clock_class);
}

int bt_ctf_clock_set_uuid(struct bt_ctf_clock *clock, const unsigned char *uuid)
{
	BT_ASSERT_PRE_NON_NULL(clock, "CTF writer clock");
	return bt_clock_class_set_uuid(clock->clock_class, uuid);
}

int bt_ctf_clock_set_time(struct bt_ctf_clock *clock, int64_t time)
{
	int64_t value;

	BT_ASSERT_PRE_NON_NULL(clock, "CTF writer clock");

	/* Common case where cycles are actually nanoseconds */
	if (clock->clock_class->frequency == 1000000000) {
		value = time;
	} else {
		value = (uint64_t) (((double) time *
		        (double) clock->clock_class->frequency) / 1e9);
	}

	BT_ASSERT_PRE(clock->value <= value,
		"CTF writer clock value must be updated monotonically: "
		"prev-value=%" PRId64 ", new-value=%" PRId64,
		clock->value, value);
	clock->value = value;
	return 0;
}

void bt_ctf_clock_get(struct bt_ctf_clock *clock)
{
	bt_get(clock);
}

void bt_ctf_clock_put(struct bt_ctf_clock *clock)
{
	bt_put(clock);
}

BT_HIDDEN
int bt_ctf_clock_get_value(struct bt_ctf_clock *clock, uint64_t *value)
{
	BT_ASSERT_PRE_NON_NULL(clock, "CTF writer clock");
	BT_ASSERT_PRE_NON_NULL(value, "Value");
	*value = clock->value;
	return 0;
}

static
void bt_ctf_clock_destroy(struct bt_object *obj)
{
	struct bt_ctf_clock *clock;

	clock = container_of(obj, struct bt_ctf_clock, base);
	bt_put(clock->clock_class);
	g_free(clock);
}
