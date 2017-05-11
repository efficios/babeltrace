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

#include <babeltrace/ctf-ir/clock-class-internal.h>
#include <babeltrace/ctf-ir/utils.h>
#include <babeltrace/ref.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/types.h>
#include <inttypes.h>

#define BT_LOG_TAG "CLOCK-CLASS"
#include <babeltrace/lib-logging-internal.h>

static
void bt_ctf_clock_class_destroy(struct bt_object *obj);

BT_HIDDEN
bt_bool bt_ctf_clock_class_is_valid(struct bt_ctf_clock_class *clock_class)
{
	return clock_class && clock_class->name;
}

int bt_ctf_clock_class_set_name(struct bt_ctf_clock_class *clock_class,
		const char *name)
{
	int ret = 0;

	if (!clock_class) {
		BT_LOGW_STR("Invalid parameter: clock class is NULL.");
		ret = -1;
		goto end;
	}

	if (clock_class->frozen) {
		BT_LOGW("Invalid parameter: clock class is frozen: addr=%p",
			clock_class);
		ret = -1;
		goto end;
	}

	if (bt_ctf_validate_identifier(name)) {
		BT_LOGE("Clock class's name is not a valid CTF identifier: "
			"clock-class-addr=%p, name=\"%s\"",
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

	BT_LOGV("Set clock class's name: clock-class-addr=%p, name=\"%s\"",
		clock_class, name);

end:
	return ret;
}

struct bt_ctf_clock_class *bt_ctf_clock_class_create(const char *name)
{
	int ret;
	struct bt_ctf_clock_class *clock_class;

	BT_LOGD("Creating default clock class object: name=\"%s\"",
		name);
	clock_class = g_new0(struct bt_ctf_clock_class, 1);
	if (!clock_class) {
		BT_LOGE_STR("Failed to allocate one clock class.");
		goto error;
	}

	clock_class->precision = 1;
	clock_class->frequency = 1000000000;
	bt_object_init(clock_class, bt_ctf_clock_class_destroy);

	if (name) {
		ret = bt_ctf_clock_class_set_name(clock_class, name);
		if (ret) {
			BT_LOGE("Cannot set clock class's name: "
				"clock-class-addr=%p, name=\"%s\"",
				clock_class, name);
			goto error;
		}
	}

	ret = bt_uuid_generate(clock_class->uuid);
	if (ret) {
		BT_LOGE_STR("Failed to generate a UUID.");
		goto error;
	}

	clock_class->uuid_set = 1;
	BT_LOGD("Created clock class object: addr=%p", clock_class);
	return clock_class;
error:
	BT_PUT(clock_class);
	return clock_class;
}

const char *bt_ctf_clock_class_get_name(struct bt_ctf_clock_class *clock_class)
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

const char *bt_ctf_clock_class_get_description(
		struct bt_ctf_clock_class *clock_class)
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

int bt_ctf_clock_class_set_description(struct bt_ctf_clock_class *clock_class,
		const char *desc)
{
	int ret = 0;

	if (!clock_class || !desc) {
		BT_LOGW("Invalid parameter: clock class or description is NULL: "
			"clock-class-addr=%p, desc-addr=%p",
			clock_class, desc);
		ret = -1;
		goto end;
	}

	if (clock_class->frozen) {
		BT_LOGW("Invalid parameter: clock class is frozen: addr=%p",
			clock_class);
		ret = -1;
		goto end;
	}

	clock_class->description = g_string_new(desc);
	ret = clock_class->description ? 0 : -1;
	BT_LOGV("Set clock class's description: clock-class-addr=%p, desc=\"%s\"",
		clock_class, desc);
end:
	return ret;
}

uint64_t bt_ctf_clock_class_get_frequency(
		struct bt_ctf_clock_class *clock_class)
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

int bt_ctf_clock_class_set_frequency(struct bt_ctf_clock_class *clock_class,
		uint64_t freq)
{
	int ret = 0;

	if (!clock_class || freq == -1ULL) {
		BT_LOGW("Invalid parameter: clock class is NULL or frequency is invalid: "
			"clock-class-addr=%p, freq=%" PRIu64,
			clock_class, freq);
		ret = -1;
		goto end;
	}

	if (clock_class->frozen) {
		BT_LOGW("Invalid parameter: clock class is frozen: addr=%p",
			clock_class);
		ret = -1;
		goto end;
	}

	clock_class->frequency = freq;
	BT_LOGV("Set clock class's frequency: clock-class-addr=%p, freq=%" PRIu64,
		clock_class, freq);
end:
	return ret;
}

uint64_t bt_ctf_clock_class_get_precision(struct bt_ctf_clock_class *clock_class)
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

int bt_ctf_clock_class_set_precision(struct bt_ctf_clock_class *clock_class,
		uint64_t precision)
{
	int ret = 0;

	if (!clock_class || precision == -1ULL) {
		BT_LOGW("Invalid parameter: clock class is NULL or precision is invalid: "
			"clock-class-addr=%p, precision=%" PRIu64,
			clock_class, precision);
		ret = -1;
		goto end;
	}

	if (clock_class->frozen) {
		BT_LOGW("Invalid parameter: clock class is frozen: addr=%p",
			clock_class);
		ret = -1;
		goto end;
	}

	clock_class->precision = precision;
	BT_LOGV("Set clock class's precision: clock-class-addr=%p, precision=%" PRIu64,
		clock_class, precision);
end:
	return ret;
}

int bt_ctf_clock_class_get_offset_s(struct bt_ctf_clock_class *clock_class,
		int64_t *offset_s)
{
	int ret = 0;

	if (!clock_class || !offset_s) {
		BT_LOGW("Invalid parameter: clock class or offset pointer is NULL: "
			"clock-class-addr=%p, offset-addr=%p",
			clock_class, offset_s);
		ret = -1;
		goto end;
	}

	*offset_s = clock_class->offset_s;
end:
	return ret;
}

int bt_ctf_clock_class_set_offset_s(struct bt_ctf_clock_class *clock_class,
		int64_t offset_s)
{
	int ret = 0;

	if (!clock_class) {
		BT_LOGW_STR("Invalid parameter: clock class is NULL.");
		ret = -1;
		goto end;
	}

	if (clock_class->frozen) {
		BT_LOGW("Invalid parameter: clock class is frozen: addr=%p",
			clock_class);
		ret = -1;
		goto end;
	}

	clock_class->offset_s = offset_s;
	BT_LOGV("Set clock class's offset (seconds): clock-class-addr=%p, offset-s=%" PRId64,
		clock_class, offset_s);
end:
	return ret;
}

int bt_ctf_clock_class_get_offset_cycles(struct bt_ctf_clock_class *clock_class,
		int64_t *offset)
{
	int ret = 0;

	if (!clock_class || !offset) {
		BT_LOGW("Invalid parameter: clock class or offset pointer is NULL: "
			"clock-class-addr=%p, offset-addr=%p",
			clock_class, offset);
		ret = -1;
		goto end;
	}

	*offset = clock_class->offset;
end:
	return ret;
}

int bt_ctf_clock_class_set_offset_cycles(struct bt_ctf_clock_class *clock_class,
		int64_t offset)
{
	int ret = 0;

	if (!clock_class) {
		BT_LOGW_STR("Invalid parameter: clock class is NULL.");
		ret = -1;
		goto end;
	}

	if (clock_class->frozen) {
		BT_LOGW("Invalid parameter: clock class is frozen: addr=%p",
			clock_class);
		ret = -1;
		goto end;
	}

	clock_class->offset = offset;
	BT_LOGV("Set clock class's offset (cycles): clock-class-addr=%p, offset-cycles=%" PRId64,
		clock_class, offset);
end:
	return ret;
}

int bt_ctf_clock_class_is_absolute(struct bt_ctf_clock_class *clock_class)
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

int bt_ctf_clock_class_set_is_absolute(struct bt_ctf_clock_class *clock_class,
		int is_absolute)
{
	int ret = 0;

	if (!clock_class) {
		BT_LOGW_STR("Invalid parameter: clock class is NULL.");
		ret = -1;
		goto end;
	}

	if (clock_class->frozen) {
		BT_LOGW("Invalid parameter: clock class is frozen: addr=%p",
			clock_class);
		ret = -1;
		goto end;
	}

	clock_class->absolute = !!is_absolute;
	BT_LOGV("Set clock class's absolute flag: clock-class-addr=%p, is-absolute=%d",
		clock_class, !!is_absolute);
end:
	return ret;
}

const unsigned char *bt_ctf_clock_class_get_uuid(
		struct bt_ctf_clock_class *clock_class)
{
	const unsigned char *ret;

	if (!clock_class) {
		BT_LOGW_STR("Invalid parameter: clock class is NULL.");
		ret = NULL;
		goto end;
	}

	if (!clock_class->uuid_set) {
		BT_LOGV("Clock class's UUID is not set: clock-class-addr=%p",
			clock_class);
		ret = NULL;
		goto end;
	}

	ret = clock_class->uuid;
end:
	return ret;
}

int bt_ctf_clock_class_set_uuid(struct bt_ctf_clock_class *clock_class,
		const unsigned char *uuid)
{
	int ret = 0;

	if (!clock_class || !uuid) {
		BT_LOGW("Invalid parameter: clock class or UUID is NULL: "
			"clock-class-addr=%p, uuid-addr=%p",
			clock_class, uuid);
		ret = -1;
		goto end;
	}

	if (clock_class->frozen) {
		BT_LOGW("Invalid parameter: clock class is frozen: addr=%p",
			clock_class);
		ret = -1;
		goto end;
	}

	memcpy(clock_class->uuid, uuid, sizeof(uuid_t));
	clock_class->uuid_set = 1;
	BT_LOGV("Set clock class's UUID: clock-class-addr=%p, "
		"uuid=\"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\"",
		clock_class,
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
		ns = (uint64_t) ((1e9 * (double) value) / (double) frequency);
	}

	return ns;
}

BT_HIDDEN
void bt_ctf_clock_class_freeze(struct bt_ctf_clock_class *clock_class)
{
	if (!clock_class) {
		BT_LOGW_STR("Invalid parameter: clock class is NULL.");
		return;
	}

	if (!clock_class->frozen) {
		BT_LOGD("Freezing clock class: addr=%p", clock_class);
		clock_class->frozen = 1;
	}
}

BT_HIDDEN
void bt_ctf_clock_class_serialize(struct bt_ctf_clock_class *clock_class,
		struct metadata_context *context)
{
	unsigned char *uuid;

	BT_LOGD("Serializing clock class's metadata: clock-class-addr=%p, "
		"metadata-context-addr=%p", clock_class, context);

	if (!clock_class || !context) {
		BT_LOGW("Invalid parameter: clock class or metadata context is NULL: "
			"clock-class-addr=%p, metadata-context-addr=%p",
			clock_class, context);
		return;
	}

	uuid = clock_class->uuid;
	g_string_append(context->string, "clock {\n");
	g_string_append_printf(context->string, "\tname = %s;\n",
		clock_class->name->str);
	g_string_append_printf(context->string,
		"\tuuid = \"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x\";\n",
		uuid[0], uuid[1], uuid[2], uuid[3],
		uuid[4], uuid[5], uuid[6], uuid[7],
		uuid[8], uuid[9], uuid[10], uuid[11],
		uuid[12], uuid[13], uuid[14], uuid[15]);
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
		clock_class->absolute ? "TRUE" : "FALSE");
	g_string_append(context->string, "};\n\n");
}

static
void bt_ctf_clock_class_destroy(struct bt_object *obj)
{
	struct bt_ctf_clock_class *clock_class;

	BT_LOGD("Destroying clock class: addr=%p", obj);
	clock_class = container_of(obj, struct bt_ctf_clock_class, base);
	if (clock_class->name) {
		g_string_free(clock_class->name, TRUE);
	}
	if (clock_class->description) {
		g_string_free(clock_class->description, TRUE);
	}

	g_free(clock_class);
}

static
void bt_ctf_clock_value_destroy(struct bt_object *obj)
{
	struct bt_ctf_clock_value *value;

	BT_LOGD("Destroying clock value: addr=%p", obj);

	if (!obj) {
		return;
	}

	value = container_of(obj, struct bt_ctf_clock_value, base);
	bt_put(value->clock_class);
	g_free(value);
}

struct bt_ctf_clock_value *bt_ctf_clock_value_create(
		struct bt_ctf_clock_class *clock_class, uint64_t value)
{
	struct bt_ctf_clock_value *ret = NULL;

	BT_LOGD("Creating clock value object: clock-class-addr=%p, "
		"value=%" PRIu64, clock_class, value);

	if (!clock_class) {
		BT_LOGW_STR("Invalid parameter: clock class is NULL.");
		goto end;
	}

	ret = g_new0(struct bt_ctf_clock_value, 1);
	if (!ret) {
		BT_LOGE_STR("Failed to allocate one clock value.");
		goto end;
	}

	bt_object_init(ret, bt_ctf_clock_value_destroy);
	ret->clock_class = bt_get(clock_class);
	ret->value = value;
	BT_LOGD("Created clock value object: addr=%p", ret);
end:
	return ret;
}

int bt_ctf_clock_value_get_value(
		struct bt_ctf_clock_value *clock_value, uint64_t *raw_value)
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

int bt_ctf_clock_value_get_value_ns_from_epoch(struct bt_ctf_clock_value *value,
		int64_t *ret_value_ns)
{
	int ret = 0;
	int64_t ns;

	if (!value || !ret_value_ns) {
		BT_LOGW("Invalid parameter: clock value or return value pointer is NULL: "
			"clock-value-addr=%p, ret-value-addr=%p",
			value, ret_value_ns);
		ret = -1;
		goto end;
	}

	/* Initialize nanosecond timestamp to clock's offset in seconds. */
	ns = value->clock_class->offset_s * (int64_t) 1000000000;

	/* Add offset in cycles, converted to nanoseconds. */
	ns += ns_from_value(value->clock_class->frequency,
			value->clock_class->offset);

	/* Add given value, converter to nanoseconds. */
	ns += ns_from_value(value->clock_class->frequency, value->value);

	*ret_value_ns = ns;
end:
	return ret;
}

struct bt_ctf_clock_class *bt_ctf_clock_value_get_class(
		struct bt_ctf_clock_value *clock_value)
{
	struct bt_ctf_clock_class *clock_class = NULL;

	if (!clock_value) {
		BT_LOGW_STR("Invalid parameter: clock value is NULL.");
		goto end;
	}

	clock_class = bt_get(clock_value->clock_class);

end:
	return clock_class;
}
