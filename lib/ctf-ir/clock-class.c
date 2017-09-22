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

static
void bt_clock_class_destroy(struct bt_object *obj);

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

	if (bt_identifier_is_valid(name)) {
		BT_LOGE("Clock class's name is not a valid CTF identifier: "
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
	bt_object_init(clock_class, bt_clock_class_destroy);

	if (name) {
		ret = bt_clock_class_set_name(clock_class, name);
		if (ret) {
			BT_LOGE("Cannot set clock class's name: "
				"addr=%p, name=\"%s\"",
				clock_class, name);
			goto error;
		}
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

static uint64_t ns_from_value(uint64_t frequency, uint64_t value)
{
	uint64_t ns;

	if (frequency == 1000000000) {
		ns = value;
	} else {
		double dblres = ((1e9 * (double) value) / (double) frequency);

		if (dblres >= (double) UINT64_MAX) {
			/* Overflows uint64_t */
			ns = -1ULL;
		} else {
			ns = (uint64_t) dblres;
		}
	}

	return ns;
}

BT_HIDDEN
void bt_clock_class_freeze(struct bt_clock_class *clock_class)
{
	if (!clock_class) {
		BT_LOGW_STR("Invalid parameter: clock class is NULL.");
		return;
	}

	if (!clock_class->frozen) {
		BT_LOGD("Freezing clock class: addr=%p, name=\"%s\"",
			clock_class, bt_clock_class_get_name(clock_class));
		clock_class->frozen = 1;
	}
}

BT_HIDDEN
void bt_clock_class_serialize(struct bt_clock_class *clock_class,
		struct metadata_context *context)
{
	unsigned char *uuid;

	BT_LOGD("Serializing clock class's metadata: clock-class-addr=%p, "
		"name=\"%s\", metadata-context-addr=%p", clock_class,
		bt_clock_class_get_name(clock_class), context);

	if (!clock_class || !context) {
		BT_LOGW("Invalid parameter: clock class or metadata context is NULL: "
			"clock-class-addr=%p, name=\"%s\", metadata-context-addr=%p",
			clock_class, bt_clock_class_get_name(clock_class),
			context);
		return;
	}

	uuid = clock_class->uuid;
	g_string_append(context->string, "clock {\n");
	g_string_append_printf(context->string, "\tname = %s;\n",
		clock_class->name->str);

	if (clock_class->uuid_set) {
		g_string_append_printf(context->string,
			"\tuuid = \"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\";\n",
			uuid[0], uuid[1], uuid[2], uuid[3],
			uuid[4], uuid[5], uuid[6], uuid[7],
			uuid[8], uuid[9], uuid[10], uuid[11],
			uuid[12], uuid[13], uuid[14], uuid[15]);
	}

	if (clock_class->description) {
		g_string_append_printf(context->string, "\tdescription = \"%s\";\n",
			clock_class->description->str);
	}

	g_string_append_printf(context->string, "\tfreq = %" PRIu64 ";\n",
		clock_class->frequency);
	g_string_append_printf(context->string, "\tprecision = %" PRIu64 ";\n",
		clock_class->precision);
	g_string_append_printf(context->string, "\toffset_s = %" PRIu64 ";\n",
		clock_class->offset_s);
	g_string_append_printf(context->string, "\toffset = %" PRIu64 ";\n",
		clock_class->offset);
	g_string_append_printf(context->string, "\tabsolute = %s;\n",
		clock_class->absolute ? "true" : "false");
	g_string_append(context->string, "};\n\n");
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

	g_free(clock_class);
}

static
void bt_clock_value_destroy(struct bt_object *obj)
{
	struct bt_clock_value *value;

	if (!obj) {
		return;
	}

	value = container_of(obj, struct bt_clock_value, base);
	BT_LOGD("Destroying clock value: addr=%p, clock-class-addr=%p, "
		"clock-class-name=\"%s\"", obj, value->clock_class,
		bt_clock_class_get_name(value->clock_class));
	bt_put(value->clock_class);
	g_free(value);
}

static
void set_ns_from_epoch(struct bt_clock_value *clock_value)
{
	struct bt_clock_class *clock_class = clock_value->clock_class;
	int64_t diff;
	int64_t s_ns;
	uint64_t u_ns;
	uint64_t cycles;

	/* Initialize nanosecond timestamp to clock's offset in seconds */
	if (clock_class->offset_s <= (INT64_MIN / 1000000000) ||
			clock_class->offset_s >= (INT64_MAX / 1000000000)) {
		/*
		 * Overflow: offset in seconds converted to nanoseconds
		 * is outside the int64_t range.
		 */
		clock_value->ns_from_epoch_overflows = true;
		goto end;
	}

	clock_value->ns_from_epoch = clock_class->offset_s * (int64_t) 1000000000;

	/* Add offset in cycles */
	if (clock_class->offset < 0) {
		cycles = (uint64_t) (-clock_class->offset);
	} else {
		cycles = (uint64_t) clock_class->offset;
	}

	u_ns = ns_from_value(clock_class->frequency, cycles);

	if (u_ns == -1ULL || u_ns >= INT64_MAX) {
		/*
		 * Overflow: offset in cycles converted to nanoseconds
		 * is outside the int64_t range.
		 */
		clock_value->ns_from_epoch_overflows = true;
		goto end;
	}

	s_ns = (int64_t) u_ns;
	assert(s_ns >= 0);

	if (clock_class->offset < 0) {
		if (clock_value->ns_from_epoch >= 0) {
			/*
			 * Offset in cycles is negative so it must also
			 * be negative once converted to nanoseconds.
			 */
			s_ns = -s_ns;
			goto offset_ok;
		}

		diff = clock_value->ns_from_epoch - INT64_MIN;

		if (s_ns >= diff) {
			/*
			 * Overflow: current timestamp in nanoseconds
			 * plus the offset in cycles converted to
			 * nanoseconds is outside the int64_t range.
			 */
			clock_value->ns_from_epoch_overflows = true;
			goto end;
		}

		/*
		 * Offset in cycles is negative so it must also be
		 * negative once converted to nanoseconds.
		 */
		s_ns = -s_ns;
	} else {
		if (clock_value->ns_from_epoch <= 0) {
			goto offset_ok;
		}

		diff = INT64_MAX - clock_value->ns_from_epoch;

		if (s_ns >= diff) {
			/*
			 * Overflow: current timestamp in nanoseconds
			 * plus the offset in cycles converted to
			 * nanoseconds is outside the int64_t range.
			 */
			clock_value->ns_from_epoch_overflows = true;
			goto end;
		}
	}

offset_ok:
	clock_value->ns_from_epoch += s_ns;

	/* Add clock value (cycles) */
	u_ns = ns_from_value(clock_class->frequency, clock_value->value);

	if (u_ns == -1ULL || u_ns >= INT64_MAX) {
		/*
		 * Overflow: value converted to nanoseconds is outside
		 * the int64_t range.
		 */
		clock_value->ns_from_epoch_overflows = true;
		goto end;
	}

	s_ns = (int64_t) u_ns;
	assert(s_ns >= 0);

	/* Clock value (cycles) is always positive */
	if (clock_value->ns_from_epoch <= 0) {
		goto value_ok;
	}

	diff = INT64_MAX - clock_value->ns_from_epoch;

	if (s_ns >= diff) {
		/*
		 * Overflow: current timestamp in nanoseconds plus the
		 * clock value converted to nanoseconds is outside the
		 * int64_t range.
		 */
		clock_value->ns_from_epoch_overflows = true;
		goto end;
	}

value_ok:
	clock_value->ns_from_epoch += s_ns;

end:
	if (clock_value->ns_from_epoch_overflows) {
		clock_value->ns_from_epoch = 0;
	}
}

struct bt_clock_value *bt_clock_value_create(
		struct bt_clock_class *clock_class, uint64_t value)
{
	struct bt_clock_value *ret = NULL;

	BT_LOGD("Creating clock value object: clock-class-addr=%p, "
		"clock-class-name=\"%s\", value=%" PRIu64, clock_class,
		bt_clock_class_get_name(clock_class), value);

	if (!clock_class) {
		BT_LOGW_STR("Invalid parameter: clock class is NULL.");
		goto end;
	}

	ret = g_new0(struct bt_clock_value, 1);
	if (!ret) {
		BT_LOGE_STR("Failed to allocate one clock value.");
		goto end;
	}

	bt_object_init(ret, bt_clock_value_destroy);
	ret->clock_class = bt_get(clock_class);
	ret->value = value;
	set_ns_from_epoch(ret);
	bt_clock_class_freeze(clock_class);
	BT_LOGD("Created clock value object: clock-value-addr=%p, "
		"clock-class-addr=%p, clock-class-name=\"%s\", "
		"ns-from-epoch=%" PRId64 ", ns-from-epoch-overflows=%d",
		ret, clock_class, bt_clock_class_get_name(clock_class),
		ret->ns_from_epoch, ret->ns_from_epoch_overflows);

end:
	return ret;
}

int bt_clock_value_get_value(
		struct bt_clock_value *clock_value, uint64_t *raw_value)
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

struct bt_clock_class *bt_clock_value_get_class(
		struct bt_clock_value *clock_value)
{
	struct bt_clock_class *clock_class = NULL;

	if (!clock_value) {
		BT_LOGW_STR("Invalid parameter: clock value is NULL.");
		goto end;
	}

	clock_class = bt_get(clock_value->clock_class);

end:
	return clock_class;
}
