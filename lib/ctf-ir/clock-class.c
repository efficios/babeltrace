/*
 * clock-class.c
 *
 * Babeltrace CTF IR - Clock class
 *
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
#include <babeltrace/ctf-ir/clock-class-internal.h>
#include <babeltrace/ctf-ir/clock-value-internal.h>
#include <babeltrace/ctf-ir/utils.h>
#include <babeltrace/ref.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/types.h>
#include <babeltrace/compat/string-internal.h>
#include <inttypes.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/assert-internal.h>

static
void bt_clock_class_destroy(struct bt_object *obj);

static
struct bt_clock_value *bt_clock_value_new(struct bt_clock_class *clock_class);

static
void bt_clock_value_destroy(struct bt_clock_value *clock_value);

BT_HIDDEN
bt_bool bt_clock_class_is_valid(struct bt_clock_class *clock_class)
{
	return clock_class && clock_class->name;
}

int bt_clock_class_set_name(struct bt_clock_class *clock_class,
		const char *name)
{
	int ret = 0;

	if (!clock_class) {
		BT_LOGW_STR("Invalid parameter: clock class is NULL.");
		ret = -1;
		goto end;
	}

	if (clock_class->frozen) {
		BT_LOGW("Invalid parameter: clock class is frozen: addr=%p, name=\"%s\"",
			clock_class, bt_clock_class_get_name(clock_class));
		ret = -1;
		goto end;
	}

	if (!bt_identifier_is_valid(name)) {
		BT_LOGW("Clock class's name is not a valid CTF identifier: "
			"addr=%p, name=\"%s\"",
			clock_class, name);
		ret = -1;
		goto end;
	}

	if (clock_class->name) {
		g_string_assign(clock_class->name, name);
	} else {
		clock_class->name = g_string_new(name);
		if (!clock_class->name) {
			BT_LOGE_STR("Failed to allocate a GString.");
			ret = -1;
			goto end;
		}
	}

	BT_LOGV("Set clock class's name: addr=%p, name=\"%s\"",
		clock_class, name);

end:
	return ret;
}

static
bool validate_freq(struct bt_clock_class *clock_class,
		const char *name, uint64_t freq)
{
	bool is_valid = true;

	if (freq == -1ULL || freq == 0) {
		BT_LOGW("Invalid parameter: frequency is invalid: "
			"addr=%p, name=\"%s\", freq=%" PRIu64,
			clock_class, name, freq);
		is_valid = false;
		goto end;
	}

end:
	return is_valid;
}

static
void bt_clock_class_free_clock_value(struct bt_clock_value *clock_value,
		struct bt_clock_class *clock_class)
{
	bt_clock_value_destroy(clock_value);
}

struct bt_clock_class *bt_clock_class_create(const char *name,
		uint64_t freq)
{
	int ret;
	struct bt_clock_class *clock_class = NULL;

	BT_LOGD("Creating default clock class object: name=\"%s\"",
		name);

	if (!validate_freq(NULL, name, freq)) {
		/* validate_freq() logs errors */
		goto error;
	}

	clock_class = g_new0(struct bt_clock_class, 1);
	if (!clock_class) {
		BT_LOGE_STR("Failed to allocate one clock class.");
		goto error;
	}

	clock_class->precision = 1;
	clock_class->frequency = freq;
	bt_object_init_shared(&clock_class->base, bt_clock_class_destroy);

	if (name) {
		ret = bt_clock_class_set_name(clock_class, name);
		if (ret) {
			/* bt_clock_class_set_name() logs errors */
			goto error;
		}
	}

	ret = bt_object_pool_initialize(&clock_class->cv_pool,
		(bt_object_pool_new_object_func) bt_clock_value_new,
		(bt_object_pool_destroy_object_func)
			bt_clock_class_free_clock_value,
		clock_class);
	if (ret) {
		BT_LOGE("Failed to initialize clock value pool: ret=%d",
			ret);
		goto error;
	}

	BT_LOGD("Created clock class object: addr=%p, name=\"%s\"",
		clock_class, name);
	return clock_class;
error:
	BT_PUT(clock_class);
	return clock_class;
}

const char *bt_clock_class_get_name(struct bt_clock_class *clock_class)
{
	const char *ret = NULL;

	if (!clock_class) {
		BT_LOGW_STR("Invalid parameter: clock class is NULL.");
		goto end;
	}

	if (clock_class->name) {
		ret = clock_class->name->str;
	}

end:
	return ret;
}

const char *bt_clock_class_get_description(
		struct bt_clock_class *clock_class)
{
	const char *ret = NULL;

	if (!clock_class) {
		BT_LOGW_STR("Invalid parameter: clock class is NULL.");
		goto end;
	}

	if (clock_class->description) {
		ret = clock_class->description->str;
	}
end:
	return ret;
}

int bt_clock_class_set_description(struct bt_clock_class *clock_class,
		const char *desc)
{
	int ret = 0;

	if (!clock_class || !desc) {
		BT_LOGW("Invalid parameter: clock class or description is NULL: "
			"clock-class-addr=%p, name=\"%s\", desc-addr=%p",
			clock_class, bt_clock_class_get_name(clock_class),
			desc);
		ret = -1;
		goto end;
	}

	if (clock_class->frozen) {
		BT_LOGW("Invalid parameter: clock class is frozen: addr=%p, name=\"%s\"",
			clock_class, bt_clock_class_get_name(clock_class));
		ret = -1;
		goto end;
	}

	clock_class->description = g_string_new(desc);
	ret = clock_class->description ? 0 : -1;
	BT_LOGV("Set clock class's description: addr=%p, "
		"name=\"%s\", desc=\"%s\"",
		clock_class, bt_clock_class_get_name(clock_class), desc);
end:
	return ret;
}

uint64_t bt_clock_class_get_frequency(
		struct bt_clock_class *clock_class)
{
	uint64_t ret = -1ULL;

	if (!clock_class) {
		BT_LOGW_STR("Invalid parameter: clock class is NULL.");
		goto end;
	}

	ret = clock_class->frequency;
end:
	return ret;
}

int bt_clock_class_set_frequency(struct bt_clock_class *clock_class,
		uint64_t freq)
{
	int ret = 0;

	if (!clock_class) {
		BT_LOGW("Invalid parameter: clock class is NULL or frequency is invalid: "
			"addr=%p, name=\"%s\"",
			clock_class, bt_clock_class_get_name(clock_class));
		ret = -1;
		goto end;
	}

	if (!validate_freq(clock_class, bt_clock_class_get_name(clock_class),
			freq)) {
		/* validate_freq() logs errors */
		goto end;
	}

	if (clock_class->frozen) {
		BT_LOGW("Invalid parameter: clock class is frozen: addr=%p, name=\"%s\"",
			clock_class, bt_clock_class_get_name(clock_class));
		ret = -1;
		goto end;
	}

	clock_class->frequency = freq;
	BT_LOGV("Set clock class's frequency: addr=%p, name=\"%s\", freq=%" PRIu64,
		clock_class, bt_clock_class_get_name(clock_class), freq);
end:
	return ret;
}

uint64_t bt_clock_class_get_precision(struct bt_clock_class *clock_class)
{
	uint64_t ret = -1ULL;

	if (!clock_class) {
		BT_LOGW_STR("Invalid parameter: clock class is NULL.");
		goto end;
	}

	ret = clock_class->precision;
end:
	return ret;
}

int bt_clock_class_set_precision(struct bt_clock_class *clock_class,
		uint64_t precision)
{
	int ret = 0;

	if (!clock_class || precision == -1ULL) {
		BT_LOGW("Invalid parameter: clock class is NULL or precision is invalid: "
			"addr=%p, name=\"%s\", precision=%" PRIu64,
			clock_class, bt_clock_class_get_name(clock_class),
			precision);
		ret = -1;
		goto end;
	}

	if (clock_class->frozen) {
		BT_LOGW("Invalid parameter: clock class is frozen: addr=%p, name=\"%s\"",
			clock_class, bt_clock_class_get_name(clock_class));
		ret = -1;
		goto end;
	}

	clock_class->precision = precision;
	BT_LOGV("Set clock class's precision: addr=%p, name=\"%s\", precision=%" PRIu64,
		clock_class, bt_clock_class_get_name(clock_class),
		precision);
end:
	return ret;
}

int bt_clock_class_get_offset_s(struct bt_clock_class *clock_class,
		int64_t *offset_s)
{
	int ret = 0;

	if (!clock_class || !offset_s) {
		BT_LOGW("Invalid parameter: clock class or offset pointer is NULL: "
			"clock-class-addr=%p, name=\"%s\", offset-addr=%p",
			clock_class, bt_clock_class_get_name(clock_class),
			offset_s);
		ret = -1;
		goto end;
	}

	*offset_s = clock_class->offset_s;
end:
	return ret;
}

int bt_clock_class_set_offset_s(struct bt_clock_class *clock_class,
		int64_t offset_s)
{
	int ret = 0;

	if (!clock_class) {
		BT_LOGW_STR("Invalid parameter: clock class is NULL.");
		ret = -1;
		goto end;
	}

	if (clock_class->frozen) {
		BT_LOGW("Invalid parameter: clock class is frozen: addr=%p, name=\"%s\"",
			clock_class, bt_clock_class_get_name(clock_class));
		ret = -1;
		goto end;
	}

	clock_class->offset_s = offset_s;
	BT_LOGV("Set clock class's offset (seconds): "
		"addr=%p, name=\"%s\", offset-s=%" PRId64,
		clock_class, bt_clock_class_get_name(clock_class),
		offset_s);
end:
	return ret;
}

int bt_clock_class_get_offset_cycles(struct bt_clock_class *clock_class,
		int64_t *offset)
{
	int ret = 0;

	if (!clock_class || !offset) {
		BT_LOGW("Invalid parameter: clock class or offset pointer is NULL: "
			"clock-class-addr=%p, name=\"%s\", offset-addr=%p",
			clock_class, bt_clock_class_get_name(clock_class),
			offset);
		ret = -1;
		goto end;
	}

	*offset = clock_class->offset;
end:
	return ret;
}

int bt_clock_class_set_offset_cycles(struct bt_clock_class *clock_class,
		int64_t offset)
{
	int ret = 0;

	if (!clock_class) {
		BT_LOGW_STR("Invalid parameter: clock class is NULL.");
		ret = -1;
		goto end;
	}

	if (clock_class->frozen) {
		BT_LOGW("Invalid parameter: clock class is frozen: addr=%p, name=\"%s\"",
			clock_class, bt_clock_class_get_name(clock_class));
		ret = -1;
		goto end;
	}

	clock_class->offset = offset;
	BT_LOGV("Set clock class's offset (cycles): addr=%p, name=\"%s\", offset-cycles=%" PRId64,
		clock_class, bt_clock_class_get_name(clock_class), offset);
end:
	return ret;
}

bt_bool bt_clock_class_is_absolute(struct bt_clock_class *clock_class)
{
	int ret = -1;

	if (!clock_class) {
		BT_LOGW_STR("Invalid parameter: clock class is NULL.");
		goto end;
	}

	ret = clock_class->absolute;
end:
	return ret;
}

int bt_clock_class_set_is_absolute(struct bt_clock_class *clock_class,
		bt_bool is_absolute)
{
	int ret = 0;

	if (!clock_class) {
		BT_LOGW_STR("Invalid parameter: clock class is NULL.");
		ret = -1;
		goto end;
	}

	if (clock_class->frozen) {
		BT_LOGW("Invalid parameter: clock class is frozen: addr=%p, name=\"%s\"",
			clock_class, bt_clock_class_get_name(clock_class));
		ret = -1;
		goto end;
	}

	clock_class->absolute = !!is_absolute;
	BT_LOGV("Set clock class's absolute flag: addr=%p, name=\"%s\", is-absolute=%d",
		clock_class, bt_clock_class_get_name(clock_class),
		is_absolute);
end:
	return ret;
}

const unsigned char *bt_clock_class_get_uuid(
		struct bt_clock_class *clock_class)
{
	const unsigned char *ret;

	if (!clock_class) {
		BT_LOGW_STR("Invalid parameter: clock class is NULL.");
		ret = NULL;
		goto end;
	}

	if (!clock_class->uuid_set) {
		BT_LOGV("Clock class's UUID is not set: addr=%p, name=\"%s\"",
			clock_class, bt_clock_class_get_name(clock_class));
		ret = NULL;
		goto end;
	}

	ret = clock_class->uuid;
end:
	return ret;
}

int bt_clock_class_set_uuid(struct bt_clock_class *clock_class,
		const unsigned char *uuid)
{
	int ret = 0;

	if (!clock_class || !uuid) {
		BT_LOGW("Invalid parameter: clock class or UUID is NULL: "
			"clock-class-addr=%p, name=\"%s\", uuid-addr=%p",
			clock_class, bt_clock_class_get_name(clock_class),
			uuid);
		ret = -1;
		goto end;
	}

	if (clock_class->frozen) {
		BT_LOGW("Invalid parameter: clock class is frozen: addr=%p, name=\"%s\"",
			clock_class, bt_clock_class_get_name(clock_class));
		ret = -1;
		goto end;
	}

	memcpy(clock_class->uuid, uuid, BABELTRACE_UUID_LEN);
	clock_class->uuid_set = 1;
	BT_LOGV("Set clock class's UUID: addr=%p, name=\"%s\", "
		"uuid=\"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\"",
		clock_class, bt_clock_class_get_name(clock_class),
		(unsigned int) uuid[0],
		(unsigned int) uuid[1],
		(unsigned int) uuid[2],
		(unsigned int) uuid[3],
		(unsigned int) uuid[4],
		(unsigned int) uuid[5],
		(unsigned int) uuid[6],
		(unsigned int) uuid[7],
		(unsigned int) uuid[8],
		(unsigned int) uuid[9],
		(unsigned int) uuid[10],
		(unsigned int) uuid[11],
		(unsigned int) uuid[12],
		(unsigned int) uuid[13],
		(unsigned int) uuid[14],
		(unsigned int) uuid[15]);
end:
	return ret;
}

BT_HIDDEN
void bt_clock_class_freeze(struct bt_clock_class *clock_class)
{
	if (!clock_class || clock_class->frozen) {
		return;
	}

	BT_LOGD("Freezing clock class: addr=%p, name=\"%s\"",
		clock_class, bt_clock_class_get_name(clock_class));
	clock_class->frozen = 1;
}

static
void bt_clock_class_destroy(struct bt_object *obj)
{
	struct bt_clock_class *clock_class;

	clock_class = container_of(obj, struct bt_clock_class, base);
	BT_LOGD("Destroying clock class: addr=%p, name=\"%s\"",
		obj, bt_clock_class_get_name(clock_class));

	if (clock_class->name) {
		g_string_free(clock_class->name, TRUE);
	}

	if (clock_class->description) {
		g_string_free(clock_class->description, TRUE);
	}

	bt_object_pool_finalize(&clock_class->cv_pool);
	g_free(clock_class);
}

static
void bt_clock_value_destroy(struct bt_clock_value *clock_value)
{
	BT_LOGD("Destroying clock value: addr=%p, clock-class-addr=%p, "
		"clock-class-name=\"%s\"", clock_value,
		clock_value->clock_class,
		bt_clock_class_get_name(clock_value->clock_class));
	bt_put(clock_value->clock_class);
	g_free(clock_value);
}

static
struct bt_clock_value *bt_clock_value_new(struct bt_clock_class *clock_class)
{
	struct bt_clock_value *ret = NULL;

	BT_LOGD("Creating clock value object: clock-class-addr=%p, "
		"clock-class-name=\"%s\"", clock_class,
		bt_clock_class_get_name(clock_class));

	if (!clock_class) {
		BT_LOGW_STR("Invalid parameter: clock class is NULL.");
		goto end;
	}

	ret = g_new0(struct bt_clock_value, 1);
	if (!ret) {
		BT_LOGE_STR("Failed to allocate one clock value.");
		goto end;
	}

	bt_object_init_unique(&ret->base);
	ret->clock_class = bt_get(clock_class);
	bt_clock_class_freeze(clock_class);
	BT_LOGD("Created clock value object: clock-value-addr=%p, "
		"clock-class-addr=%p, clock-class-name=\"%s\", "
		"ns-from-epoch=%" PRId64 ", ns-from-epoch-overflows=%d",
		ret, clock_class, bt_clock_class_get_name(clock_class),
		ret->ns_from_epoch, ret->ns_from_epoch_overflows);

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

	if (!clock_value->clock_class) {
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
	bt_clock_value_set_is_frozen(clock_value, false);
	clock_class = clock_value->clock_class;
	BT_ASSERT(clock_class);
	clock_value->clock_class = NULL;
	bt_object_pool_recycle_object(&clock_class->cv_pool, clock_value);
	bt_put(clock_class);
}

int bt_clock_value_get_value(struct bt_clock_value *clock_value,
		uint64_t *raw_value)
{
	int ret = 0;

	if (!clock_value || !raw_value) {
		BT_LOGW("Invalid parameter: clock value or raw value is NULL: "
			"clock-value-addr=%p, raw-value-addr=%p",
			clock_value, raw_value);
		ret = -1;
		goto end;
	}

	*raw_value = clock_value->value;
end:
	return ret;
}

int bt_clock_value_get_value_ns_from_epoch(struct bt_clock_value *value,
		int64_t *ret_value_ns)
{
	int ret = 0;

	if (!value || !ret_value_ns) {
		BT_LOGW("Invalid parameter: clock value or return value pointer is NULL: "
			"clock-value-addr=%p, ret-value-addr=%p",
			value, ret_value_ns);
		ret = -1;
		goto end;
	}

	if (value->ns_from_epoch_overflows) {
		BT_LOGW("Clock value converted to nanoseconds from Epoch overflows the signed 64-bit integer range: "
			"clock-value-addr=%p, "
			"clock-class-offset-s=%" PRId64 ", "
			"clock-class-offset-cycles=%" PRId64 ", "
			"value=%" PRIu64,
			value, value->clock_class->offset_s,
			value->clock_class->offset,
			value->value);
		ret = -1;
		goto end;
	}

	*ret_value_ns = value->ns_from_epoch;

end:
	return ret;
}

struct bt_clock_class *bt_clock_value_borrow_class(
		struct bt_clock_value *clock_value)
{
	struct bt_clock_class *clock_class = NULL;

	if (!clock_value) {
		BT_LOGW_STR("Invalid parameter: clock value is NULL.");
		goto end;
	}

	clock_class = clock_value->clock_class;

end:
	return clock_class;
}

BT_HIDDEN
int bt_clock_class_compare(struct bt_clock_class *clock_class_a,
		struct bt_clock_class *clock_class_b)
{
	int ret = 1;
	BT_ASSERT(clock_class_a);
	BT_ASSERT(clock_class_b);

	/* Name */
	if (strcmp(clock_class_a->name->str, clock_class_b->name->str) != 0) {
		BT_LOGV("Clock classes differ: different names: "
			"cc-a-name=\"%s\", cc-b-name=\"%s\"",
			clock_class_a->name->str,
			clock_class_b->name->str);
		goto end;
	}

	/* Description */
	if (clock_class_a->description) {
		if (!clock_class_b->description) {
			BT_LOGV_STR("Clock classes differ: clock class A has a "
				"description, but clock class B does not.");
			goto end;
		}

		if (strcmp(clock_class_a->name->str, clock_class_b->name->str)
				!= 0) {
			BT_LOGV("Clock classes differ: different descriptions: "
				"cc-a-descr=\"%s\", cc-b-descr=\"%s\"",
				clock_class_a->description->str,
				clock_class_b->description->str);
			goto end;
		}
	} else {
		if (clock_class_b->description) {
			BT_LOGV_STR("Clock classes differ: clock class A has "
				"no description, but clock class B has one.");
			goto end;
		}
	}

	/* Frequency */
	if (clock_class_a->frequency != clock_class_b->frequency) {
		BT_LOGV("Clock classes differ: different frequencies: "
			"cc-a-freq=%" PRIu64 ", cc-b-freq=%" PRIu64,
			clock_class_a->frequency,
			clock_class_b->frequency);
		goto end;
	}

	/* Precision */
	if (clock_class_a->precision != clock_class_b->precision) {
		BT_LOGV("Clock classes differ: different precisions: "
			"cc-a-freq=%" PRIu64 ", cc-b-freq=%" PRIu64,
			clock_class_a->precision,
			clock_class_b->precision);
		goto end;
	}

	/* Offset (seconds) */
	if (clock_class_a->offset_s != clock_class_b->offset_s) {
		BT_LOGV("Clock classes differ: different offsets (seconds): "
			"cc-a-offset-s=%" PRId64 ", cc-b-offset-s=%" PRId64,
			clock_class_a->offset_s,
			clock_class_b->offset_s);
		goto end;
	}

	/* Offset (cycles) */
	if (clock_class_a->offset != clock_class_b->offset) {
		BT_LOGV("Clock classes differ: different offsets (cycles): "
			"cc-a-offset-s=%" PRId64 ", cc-b-offset-s=%" PRId64,
			clock_class_a->offset,
			clock_class_b->offset);
		goto end;
	}

	/* UUIDs */
	if (clock_class_a->uuid_set) {
		if (!clock_class_b->uuid_set) {
			BT_LOGV_STR("Clock classes differ: clock class A has a "
				"UUID, but clock class B does not.");
			goto end;
		}

		if (memcmp(clock_class_a->uuid, clock_class_b->uuid,
				BABELTRACE_UUID_LEN) != 0) {
			BT_LOGV("Clock classes differ: different UUIDs: "
				"cc-a-uuid=\"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\", "
				"cc-b-uuid=\"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\"",
				(unsigned int) clock_class_a->uuid[0],
				(unsigned int) clock_class_a->uuid[1],
				(unsigned int) clock_class_a->uuid[2],
				(unsigned int) clock_class_a->uuid[3],
				(unsigned int) clock_class_a->uuid[4],
				(unsigned int) clock_class_a->uuid[5],
				(unsigned int) clock_class_a->uuid[6],
				(unsigned int) clock_class_a->uuid[7],
				(unsigned int) clock_class_a->uuid[8],
				(unsigned int) clock_class_a->uuid[9],
				(unsigned int) clock_class_a->uuid[10],
				(unsigned int) clock_class_a->uuid[11],
				(unsigned int) clock_class_a->uuid[12],
				(unsigned int) clock_class_a->uuid[13],
				(unsigned int) clock_class_a->uuid[14],
				(unsigned int) clock_class_a->uuid[15],
				(unsigned int) clock_class_b->uuid[0],
				(unsigned int) clock_class_b->uuid[1],
				(unsigned int) clock_class_b->uuid[2],
				(unsigned int) clock_class_b->uuid[3],
				(unsigned int) clock_class_b->uuid[4],
				(unsigned int) clock_class_b->uuid[5],
				(unsigned int) clock_class_b->uuid[6],
				(unsigned int) clock_class_b->uuid[7],
				(unsigned int) clock_class_b->uuid[8],
				(unsigned int) clock_class_b->uuid[9],
				(unsigned int) clock_class_b->uuid[10],
				(unsigned int) clock_class_b->uuid[11],
				(unsigned int) clock_class_b->uuid[12],
				(unsigned int) clock_class_b->uuid[13],
				(unsigned int) clock_class_b->uuid[14],
				(unsigned int) clock_class_b->uuid[15]);
			goto end;
		}
	} else {
		if (clock_class_b->uuid_set) {
			BT_LOGV_STR("Clock classes differ: clock class A has "
				"no UUID, but clock class B has one.");
			goto end;
		}
	}

	/* Absolute */
	if (!!clock_class_a->absolute != !!clock_class_b->absolute) {
		BT_LOGV("Clock classes differ: one is absolute, the other "
			"is not: cc-a-is-absolute=%d, cc-b-is-absolute=%d",
			!!clock_class_a->absolute,
			!!clock_class_b->absolute);
		goto end;
	}

	/* Equal */
	ret = 0;

end:
	return ret;
}

int bt_clock_class_cycles_to_ns(struct bt_clock_class *clock_class,
		uint64_t cycles, int64_t *ns)
{
	int ret;
	bool overflows;

	BT_ASSERT_PRE_NON_NULL(clock_class, "Clock class");
	BT_ASSERT_PRE_NON_NULL(ns, "Nanoseconds");

	ret = ns_from_epoch(clock_class, cycles, ns, &overflows);
	if (ret) {
		if (overflows) {
			BT_LIB_LOGW("Cannot convert cycles to nanoseconds "
				"from Epoch for given clock class: "
				"value overflow: %![cc-]+K, cycles=%" PRIu64,
				clock_class, cycles);
		} else {
			BT_LIB_LOGW("Cannot convert cycles to nanoseconds "
				"from Epoch for given clock class: "
				"%![cc-]+K, cycles=%" PRIu64,
				clock_class, cycles);
		}

		goto end;
	}

end:
	return ret;
}
