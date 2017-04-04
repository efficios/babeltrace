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
#include <inttypes.h>

static
void bt_ctf_clock_class_destroy(struct bt_object *obj);

BT_HIDDEN
bool bt_ctf_clock_class_is_valid(struct bt_ctf_clock_class *clock_class)
{
	return clock_class && clock_class->name;
}

int bt_ctf_clock_class_set_name(struct bt_ctf_clock_class *clock_class,
		const char *name)
{
	int ret = 0;

	if (!clock_class || clock_class->frozen) {
		ret = -1;
		goto end;
	}

	if (bt_ctf_validate_identifier(name)) {
		ret = -1;
		goto end;
	}

	if (clock_class->name) {
		g_string_assign(clock_class->name, name);
	} else {
		clock_class->name = g_string_new(name);
		if (!clock_class->name) {
			ret = -1;
			goto end;
		}
	}

end:
	return ret;
}

struct bt_ctf_clock_class *bt_ctf_clock_class_create(const char *name)
{
	int ret;
	struct bt_ctf_clock_class *clock_class =
		g_new0(struct bt_ctf_clock_class, 1);

	if (!clock_class) {
		goto error;
	}

	clock_class->precision = 1;
	clock_class->frequency = 1000000000;
	bt_object_init(clock_class, bt_ctf_clock_class_destroy);

	if (name) {
		ret = bt_ctf_clock_class_set_name(clock_class, name);
		if (ret) {
			goto error;
		}
	}

	ret = bt_uuid_generate(clock_class->uuid);
	if (ret) {
		goto error;
	}

	clock_class->uuid_set = 1;
	return clock_class;
error:
	BT_PUT(clock_class);
	return clock_class;
}

const char *bt_ctf_clock_class_get_name(struct bt_ctf_clock_class *clock_class)
{
	const char *ret = NULL;

	if (!clock_class) {
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

	if (!clock_class || !desc || clock_class->frozen) {
		ret = -1;
		goto end;
	}

	clock_class->description = g_string_new(desc);
	ret = clock_class->description ? 0 : -1;
end:
	return ret;
}

uint64_t bt_ctf_clock_class_get_frequency(
		struct bt_ctf_clock_class *clock_class)
{
	uint64_t ret = -1ULL;

	if (!clock_class) {
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

	if (!clock_class || clock_class->frozen) {
		ret = -1;
		goto end;
	}

	clock_class->frequency = freq;
end:
	return ret;
}

uint64_t bt_ctf_clock_class_get_precision(struct bt_ctf_clock_class *clock_class)
{
	uint64_t ret = -1ULL;

	if (!clock_class) {
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

	if (!clock_class || clock_class->frozen) {
		ret = -1;
		goto end;
	}

	clock_class->precision = precision;
end:
	return ret;
}

int bt_ctf_clock_class_get_offset_s(struct bt_ctf_clock_class *clock_class,
		int64_t *offset_s)
{
	int ret = 0;

	if (!clock_class || !offset_s) {
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

	if (!clock_class || clock_class->frozen) {
		ret = -1;
		goto end;
	}

	clock_class->offset_s = offset_s;
end:
	return ret;
}

int bt_ctf_clock_class_get_offset_cycles(struct bt_ctf_clock_class *clock_class,
		int64_t *offset)
{
	int ret = 0;

	if (!clock_class || !offset) {
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

	if (!clock_class || clock_class->frozen) {
		ret = -1;
		goto end;
	}

	clock_class->offset = offset;
end:
	return ret;
}

int bt_ctf_clock_class_get_is_absolute(struct bt_ctf_clock_class *clock_class)
{
	int ret = -1;

	if (!clock_class) {
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

	if (!clock_class || clock_class->frozen) {
		ret = -1;
		goto end;
	}

	clock_class->absolute = !!is_absolute;
end:
	return ret;
}

const unsigned char *bt_ctf_clock_class_get_uuid(
		struct bt_ctf_clock_class *clock_class)
{
	const unsigned char *ret;

	if (!clock_class || !clock_class->uuid_set) {
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

	if (!clock_class || !uuid || clock_class->frozen) {
		ret = -1;
		goto end;
	}

	memcpy(clock_class->uuid, uuid, sizeof(uuid_t));
	clock_class->uuid_set = 1;
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
		return;
	}

	clock_class->frozen = 1;
}

BT_HIDDEN
void bt_ctf_clock_class_serialize(struct bt_ctf_clock_class *clock_class,
		struct metadata_context *context)
{
	unsigned char *uuid;

	if (!clock_class || !context) {
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

	if (!clock_class) {
		goto end;
	}

	ret = g_new0(struct bt_ctf_clock_value, 1);
	if (!ret) {
		goto end;
	}

	bt_object_init(ret, bt_ctf_clock_value_destroy);
	ret->clock_class = bt_get(clock_class);
	ret->value = value;
end:
	return ret;
}

int bt_ctf_clock_value_get_value(
		struct bt_ctf_clock_value *clock_value, uint64_t *raw_value)
{
	int ret = 0;

	if (!clock_value || !raw_value) {
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
		ret = -1;
		goto end;
	}

	/* Initialize nanosecond timestamp to clock's offset in seconds. */
	ns = value->clock_class->offset_s * 1000000000;

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
		goto end;
	}

	clock_class = bt_get(clock_value->clock_class);

end:
	return clock_class;
}
