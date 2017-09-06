/*
 * clock-fields.c
 *
 * Babeltrace - Update clock fields to write uint64 values
 *
 * Copyright 2017 Julien Desfossez <jdesfossez@efficios.com>
 *
 * Author: Julien Desfossez <jdesfossez@efficios.com>
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

#define BT_LOG_TAG "PLUGIN-CTFCOPYTRACE-LIB-CLOCK-FIELDS"
#include "logging.h"

#include <babeltrace/babeltrace.h>
#include <assert.h>
#include <stdio.h>

#include "clock-fields.h"

static
int find_update_struct_clock_fields(FILE *err, struct bt_ctf_field_type *type,
		struct bt_ctf_clock_class *writer_clock_class);
static
int find_update_array_clock_fields(FILE *err, struct bt_ctf_field_type *type,
		struct bt_ctf_clock_class *writer_clock_class);
static
int find_update_enum_clock_fields(FILE *err, struct bt_ctf_field_type *type,
		struct bt_ctf_clock_class *writer_clock_class);
static
int find_update_sequence_clock_fields(FILE *err, struct bt_ctf_field_type *type,
		struct bt_ctf_clock_class *writer_clock_class);
static
int find_update_variant_clock_fields(FILE *err, struct bt_ctf_field_type *type,
		struct bt_ctf_clock_class *writer_clock_class);

static
int copy_find_clock_int_field(FILE *err,
		struct bt_ctf_event *event, struct bt_ctf_event *writer_event,
		struct bt_ctf_field *field, struct bt_ctf_field_type *type,
		struct bt_ctf_field *copy_field);
static
int copy_find_clock_struct_field(FILE *err,
		struct bt_ctf_event *event, struct bt_ctf_event *writer_event,
		struct bt_ctf_field *field, struct bt_ctf_field_type *type,
		struct bt_ctf_field *copy_field);
static
int copy_find_clock_array_field(FILE *err,
		struct bt_ctf_event *event, struct bt_ctf_event *writer_event,
		struct bt_ctf_field *field, struct bt_ctf_field_type *type,
		struct bt_ctf_field *copy_field);
static
int copy_find_clock_sequence_field(FILE *err,
		struct bt_ctf_event *event, struct bt_ctf_event *writer_event,
		struct bt_ctf_field *field, struct bt_ctf_field_type *type,
		struct bt_ctf_field *copy_field);
static
int copy_find_clock_variant_field(FILE *err, struct bt_ctf_event *event,
		struct bt_ctf_event *writer_event, struct bt_ctf_field *field,
		struct bt_ctf_field_type *type, struct bt_ctf_field *copy_field);
static
int copy_find_clock_enum_field(FILE *err, struct bt_ctf_event *event,
		struct bt_ctf_event *writer_event, struct bt_ctf_field *field,
		struct bt_ctf_field_type *type, struct bt_ctf_field *copy_field);

static
int update_header_clock_int_field_type(FILE *err, struct bt_ctf_field_type *type,
		struct bt_ctf_clock_class *writer_clock_class)
{
	struct bt_ctf_clock_class *clock = NULL;
	int ret;

	clock = bt_ctf_field_type_integer_get_mapped_clock_class(type);
	if (!clock) {
		return 0;
	}
	BT_PUT(clock);

	ret = bt_ctf_field_type_integer_set_size(type, 64);
	if (ret) {
		BT_LOGE_STR("Failed to set integer size to 64.");
		goto end;
	}

	ret = bt_ctf_field_type_integer_set_mapped_clock_class(type,
			writer_clock_class);
	if (ret) {
		BT_LOGE_STR("Failed to map integer to clock_class.");
		goto end;
	}

end:
	return ret;
}

static
int find_update_clock_fields(FILE *err, struct bt_ctf_field_type *type,
		struct bt_ctf_clock_class *writer_clock_class)
{
	int ret;

	switch (bt_ctf_field_type_get_type_id(type)) {
	case BT_CTF_FIELD_TYPE_ID_INTEGER:
		return update_header_clock_int_field_type(err, type,
				writer_clock_class);
	case BT_CTF_FIELD_TYPE_ID_STRUCT:
		return find_update_struct_clock_fields(err, type,
				writer_clock_class);
	case BT_CTF_FIELD_TYPE_ID_ARRAY:
		return find_update_array_clock_fields(err, type,
				writer_clock_class);
	case BT_CTF_FIELD_TYPE_ID_SEQUENCE:
		return find_update_sequence_clock_fields(err, type,
				writer_clock_class);
	case BT_CTF_FIELD_TYPE_ID_VARIANT:
		return find_update_variant_clock_fields(err, type,
				writer_clock_class);
	case BT_CTF_FIELD_TYPE_ID_ENUM:
		return find_update_enum_clock_fields(err, type,
				writer_clock_class);
		break;
	default:
		break;
	}

	ret = 0;

	return ret;
}

static
int find_update_variant_clock_fields(FILE *err, struct bt_ctf_field_type *type,
		struct bt_ctf_clock_class *writer_clock_class)
{
	int count, i, ret;
	struct bt_ctf_field_type *entry_type = NULL;

	count = bt_ctf_field_type_variant_get_field_count(type);
	for (i = 0; i < count; i++) {
		const char *entry_name;

		ret = bt_ctf_field_type_variant_get_field(type,
				&entry_name, &entry_type, i);
		if (ret) {
			BT_LOGE_STR("Failed to get variant field.");
			goto error;
		}

		ret = find_update_clock_fields(err, entry_type,
				writer_clock_class);
		if (ret) {
			BT_LOGE_STR("Failed to find clock fields.");
			goto error;
		}
		BT_PUT(entry_type);
	}

	ret = 0;
	goto end;

error:
	ret = -1;
end:
	bt_put(entry_type);
	return ret;
}

static
int find_update_struct_clock_fields(FILE *err, struct bt_ctf_field_type *type,
		struct bt_ctf_clock_class *writer_clock_class)
{
	int count, i, ret;
	struct bt_ctf_field_type *entry_type = NULL;

	count = bt_ctf_field_type_structure_get_field_count(type);
	for (i = 0; i < count; i++) {
		const char *entry_name;

		ret = bt_ctf_field_type_structure_get_field(type,
				&entry_name, &entry_type, i);
		if (ret) {
			BT_LOGE_STR("Failed to get struct field.");
			goto error;
		}

		ret = find_update_clock_fields(err, entry_type,
				writer_clock_class);
		if (ret) {
			BT_LOGE_STR("Failed to find clock fields.");
			goto error;
		}
		BT_PUT(entry_type);
	}

	ret = 0;
	goto end;

error:
	bt_put(entry_type);
end:
	return ret;
}

static
int find_update_sequence_clock_fields(FILE *err, struct bt_ctf_field_type *type,
		struct bt_ctf_clock_class *writer_clock_class)
{
	int ret;
	struct bt_ctf_field_type *entry_type = NULL;

	entry_type = bt_ctf_field_type_sequence_get_element_type(type);
	assert(entry_type);

	ret = find_update_clock_fields(err, entry_type, writer_clock_class);
	BT_PUT(entry_type);
	if (ret) {
		BT_LOGE_STR("Failed to find clock fields.");
		goto error;
	}

	ret = 0;
	goto end;

error:
	ret = -1;
end:
	return ret;
}

static
int find_update_array_clock_fields(FILE *err, struct bt_ctf_field_type *type,
		struct bt_ctf_clock_class *writer_clock_class)
{
	int ret = 0;
	struct bt_ctf_field_type *entry_type = NULL;

	entry_type = bt_ctf_field_type_array_get_element_type(type);
	assert(entry_type);

	ret = find_update_clock_fields(err, entry_type, writer_clock_class);
	BT_PUT(entry_type);
	if (ret) {
		BT_LOGE_STR("Failed to find clock fields.");
		ret = -1;
		goto end;
	}

end:
	return ret;
}

static
int find_update_enum_clock_fields(FILE *err, struct bt_ctf_field_type *type,
		struct bt_ctf_clock_class *writer_clock_class)
{
	int ret;
	struct bt_ctf_field_type *entry_type = NULL;

	entry_type = bt_ctf_field_type_enumeration_get_container_type(type);
	assert(entry_type);

	ret = find_update_clock_fields(err, entry_type, writer_clock_class);
	BT_PUT(entry_type);
	if (ret) {
		BT_LOGE_STR("Failed to find clock fields.");
		goto error;
	}

	ret = 0;
	goto end;

error:
	ret = -1;
end:
	return ret;
}

BT_HIDDEN
struct bt_ctf_field_type *override_header_type(FILE *err,
		struct bt_ctf_field_type *type,
		struct bt_ctf_trace *writer_trace)
{
	struct bt_ctf_field_type *new_type = NULL;
	struct bt_ctf_clock_class *writer_clock_class = NULL;
	int ret;

	/* FIXME multi-clock? */
	writer_clock_class = bt_ctf_trace_get_clock_class_by_index(writer_trace, 0);
	assert(writer_clock_class);

	new_type = bt_ctf_field_type_copy(type);
	if (!new_type) {
		BT_LOGE_STR("Failed to copy field type.");
		goto error;
	}

	if (bt_ctf_field_type_get_type_id(new_type) != BT_CTF_FIELD_TYPE_ID_STRUCT) {
		BT_LOGE("Expected header field type to be struct: type=%d",
				bt_ctf_field_type_get_type_id(new_type));
		goto error;
	}

	ret = find_update_struct_clock_fields(err, new_type, writer_clock_class);
	if (ret) {
		BT_LOGE_STR("Failed to find clock fields in struct.");
		goto error;
	}
	BT_PUT(writer_clock_class);

	goto end;

error:
	bt_put(writer_clock_class);
	BT_PUT(new_type);
end:
	return new_type;
}

static
int copy_float_field(FILE *err, struct bt_ctf_field *field,
		struct bt_ctf_field_type *type,
		struct bt_ctf_field *copy_field)
{
	double value;
	int ret;

	ret = bt_ctf_field_floating_point_get_value(field, &value);
	if (ret) {
		BT_LOGE_STR("Failed to get value.");
		goto error;
	}

	ret = bt_ctf_field_floating_point_set_value(copy_field, value);
	if (ret) {
		ret = -1;
		BT_LOGE_STR("Failed to set floating point value.");
		goto end;
	}

	ret = 0;
	goto end;

error:
	ret = -1;
end:
	return ret;
}

static
int copy_string_field(FILE *err, struct bt_ctf_field *field,
		struct bt_ctf_field_type *type,
		struct bt_ctf_field *copy_field)
{
	const char *value;
	int ret;

	value = bt_ctf_field_string_get_value(field);
	if (!value) {
		BT_LOGE_STR("Failed to get value.");
		goto error;
	}

	ret = bt_ctf_field_string_set_value(copy_field, value);
	if (ret) {
		ret = -1;
		BT_LOGE_STR("Failed to set string value.");
		goto end;
	}

	ret = 0;
	goto end;

error:
	ret = -1;
end:
	return ret;
}

BT_HIDDEN
int copy_override_field(FILE *err, struct bt_ctf_event *event,
		struct bt_ctf_event *writer_event, struct bt_ctf_field *field,
		struct bt_ctf_field *copy_field)
{
	struct bt_ctf_field_type *type = NULL;
	int ret = 0;

	type = bt_ctf_field_get_type(field);
	assert(type);

	switch (bt_ctf_field_type_get_type_id(type)) {
	case BT_CTF_FIELD_TYPE_ID_INTEGER:
		ret = copy_find_clock_int_field(err, event, writer_event,
				field, type, copy_field);
		break;
	case BT_CTF_FIELD_TYPE_ID_STRUCT:
		ret = copy_find_clock_struct_field(err, event, writer_event,
				field, type, copy_field);
		break;
	case BT_CTF_FIELD_TYPE_ID_FLOAT:
		ret = copy_float_field(err, field, type, copy_field);
		break;
	case BT_CTF_FIELD_TYPE_ID_ENUM:
		ret = copy_find_clock_enum_field(err, event, writer_event,
				field, type, copy_field);
		break;
	case BT_CTF_FIELD_TYPE_ID_STRING:
		ret = copy_string_field(err, field, type, copy_field);
		break;
	case BT_CTF_FIELD_TYPE_ID_ARRAY:
		ret = copy_find_clock_array_field(err, event, writer_event,
				field, type, copy_field);
		break;
	case BT_CTF_FIELD_TYPE_ID_SEQUENCE:
		ret = copy_find_clock_sequence_field(err, event, writer_event,
				field, type, copy_field);
		break;
	case BT_CTF_FIELD_TYPE_ID_VARIANT:
		ret = copy_find_clock_variant_field(err, event, writer_event,
				field, type, copy_field);
		break;
	/* No default, we want to catch missing field types. */
	case BT_CTF_FIELD_TYPE_ID_UNKNOWN:
	case BT_CTF_NR_TYPE_IDS:
		break;
	}

	BT_PUT(type);

	return ret;
}

static
int copy_find_clock_enum_field(FILE *err, struct bt_ctf_event *event,
		struct bt_ctf_event *writer_event, struct bt_ctf_field *field,
		struct bt_ctf_field_type *type, struct bt_ctf_field *copy_field)
{
	int ret;
	struct bt_ctf_field *container = NULL, *copy_container = NULL;

	container = bt_ctf_field_enumeration_get_container(field);
	assert(container);

	copy_container = bt_ctf_field_enumeration_get_container(copy_field);
	assert(copy_container);

	ret = copy_override_field(err, event, writer_event, container,
			copy_container);
	if (ret) {
		ret = -1;
		BT_LOGE_STR("Failed to override enum field.");
		goto error;
	}

	ret = 0;
	goto end;

error:
	ret = -1;
end:
	bt_put(copy_container);
	bt_put(container);
	return ret;
}

static
int copy_find_clock_variant_field(FILE *err, struct bt_ctf_event *event,
		struct bt_ctf_event *writer_event, struct bt_ctf_field *field,
		struct bt_ctf_field_type *type, struct bt_ctf_field *copy_field)
{
	int ret;
	struct bt_ctf_field *tag = NULL;
	struct bt_ctf_field *variant_field = NULL, *copy_variant_field = NULL;

	tag = bt_ctf_field_variant_get_tag(field);
	assert(tag);

	variant_field = bt_ctf_field_variant_get_field(field, tag);
	if (!variant_field) {
		BT_LOGE_STR("Failed to get variant field.");
		goto error;
	}

	copy_variant_field = bt_ctf_field_variant_get_field(copy_field, tag);
	assert(copy_variant_field);

	ret = copy_override_field(err, event, writer_event, variant_field,
			copy_variant_field);
	if (ret) {
		BT_LOGE_STR("Failed to override variant field.");
		goto error;
	}

	ret = 0;
	goto end;

error:
	ret = -1;
end:
	bt_put(copy_variant_field);
	bt_put(variant_field);
	bt_put(tag);
	return ret;
}

static
int copy_find_clock_sequence_field(FILE *err,
		struct bt_ctf_event *event, struct bt_ctf_event *writer_event,
		struct bt_ctf_field *field, struct bt_ctf_field_type *type,
		struct bt_ctf_field *copy_field)
{
	int ret;
	uint64_t i, count;
	struct bt_ctf_field *length_field = NULL;
	struct bt_ctf_field *entry_field = NULL, *entry_copy = NULL;

	length_field = bt_ctf_field_sequence_get_length(field);
	assert(length_field);

	ret = bt_ctf_field_unsigned_integer_get_value(length_field, &count);
	if (ret) {
		BT_LOGE("Failed to get value.");
		goto error;
	}

	ret = bt_ctf_field_sequence_set_length(copy_field, length_field);
	if (ret) {
		BT_LOGE_STR("Failed to set sequence length.");
		goto error;
	}
	BT_PUT(length_field);

	for (i = 0; i < count; i++) {
		entry_field = bt_ctf_field_sequence_get_field(field, i);
		if (!entry_field) {
			BT_LOGE_STR("Failed to get sequence field.");
			goto error;
		}

		entry_copy = bt_ctf_field_sequence_get_field(copy_field, i);
		assert(entry_copy);

		ret = copy_override_field(err, event, writer_event, entry_field,
				entry_copy);
		if (ret) {
			BT_LOGE_STR("Faield to override field in sequence.");
			goto error;
		}
		BT_PUT(entry_field);
		BT_PUT(entry_copy);
	}

	ret = 0;
	goto end;

error:
	bt_put(length_field);
	bt_put(entry_field);
	bt_put(entry_copy);
	ret = -1;
end:
	return ret;
}

static
int copy_find_clock_array_field(FILE *err,
		struct bt_ctf_event *event, struct bt_ctf_event *writer_event,
		struct bt_ctf_field *field, struct bt_ctf_field_type *type,
		struct bt_ctf_field *copy_field)
{
	int ret, count, i;
	struct bt_ctf_field *entry_field = NULL, *entry_copy = NULL;

	count = bt_ctf_field_type_array_get_length(type);
	for (i = 0; i < count; i++) {
		entry_field = bt_ctf_field_array_get_field(field, i);
		if (!entry_field) {
			ret = -1;
			BT_LOGE_STR("Failed to get array field.");
			goto error;
		}

		entry_copy = bt_ctf_field_array_get_field(copy_field, i);
		assert(entry_copy);

		ret = copy_override_field(err, event, writer_event, entry_field,
				entry_copy);
		if (ret) {
			ret = -1;
			BT_LOGE_STR("Failed to override field in array.");
			goto error;
		}
		BT_PUT(entry_field);
		BT_PUT(entry_copy);
	}

	ret = 0;
	goto end;

error:
	bt_put(entry_field);
	bt_put(entry_copy);

end:
	return ret;
}

static
int copy_find_clock_struct_field(FILE *err,
		struct bt_ctf_event *event, struct bt_ctf_event *writer_event,
		struct bt_ctf_field *field, struct bt_ctf_field_type *type,
		struct bt_ctf_field *copy_field)
{
	int count, i, ret;
	struct bt_ctf_field_type *entry_type = NULL;
	struct bt_ctf_field *entry_field = NULL, *entry_copy = NULL;

	count = bt_ctf_field_type_structure_get_field_count(type);
	for (i = 0; i < count; i++) {
		const char *entry_name;

		entry_field = bt_ctf_field_structure_get_field_by_index(field, i);
		if (!entry_field) {
			BT_LOGE_STR("Failed to get struct field.");
			goto error;
		}

		ret = bt_ctf_field_type_structure_get_field(type, &entry_name,
				&entry_type, i);
		if (ret) {
			BT_LOGE_STR("Failed to get struct field.");
			goto error;
		}

		entry_copy = bt_ctf_field_structure_get_field_by_index(copy_field, i);
		assert(entry_copy);

		ret = copy_override_field(err, event, writer_event, entry_field,
				entry_copy);
		if (ret) {
			BT_LOGE_STR("Failed to override field in struct.");
			goto error;
		}
		BT_PUT(entry_copy);
		BT_PUT(entry_field);
		BT_PUT(entry_type);
	}

	ret = 0;
	goto end;

error:
	bt_put(entry_type);
	bt_put(entry_field);
	bt_put(entry_copy);
	ret = -1;
end:
	return ret;
}

static
int set_int_value(FILE *err, struct bt_ctf_field *field,
		struct bt_ctf_field *copy_field,
		struct bt_ctf_field_type *type)
{
	uint64_t uvalue;
	int64_t value;
	int ret;

	if (bt_ctf_field_type_integer_get_signed(type)) {
		ret = bt_ctf_field_signed_integer_get_value(field, &value);
		if (ret) {
			BT_LOGE("Failed to get value.");
			goto error;
		}

		ret = bt_ctf_field_signed_integer_set_value(copy_field, value);
		if (ret) {
			ret = -1;
			BT_LOGE_STR("Failed to set signed integer value.");
			goto end;
		}
	} else {
		ret = bt_ctf_field_unsigned_integer_get_value(field,
				&uvalue);
		if (ret) {
			BT_LOGE("Failed to get value.");
			goto error;
		}

		ret = bt_ctf_field_unsigned_integer_set_value(copy_field, uvalue);
		if (ret) {
			ret = -1;
			BT_LOGE_STR("Failed to set unsigned integer value.");
			goto end;
		}
	}
	ret = 0;
	goto end;

error:
	ret = -1;
end:
	return ret;
}

struct bt_ctf_clock_class *stream_class_get_clock_class(FILE *err,
		struct bt_ctf_stream_class *stream_class)
{
	struct bt_ctf_trace *trace = NULL;
	struct bt_ctf_clock_class *clock_class = NULL;

	trace = bt_ctf_stream_class_get_trace(stream_class);
	assert(trace);

	/* FIXME multi-clock? */
	clock_class = bt_ctf_trace_get_clock_class_by_index(trace, 0);

	bt_put(trace);

	return clock_class;
}

struct bt_ctf_clock_class *event_get_clock_class(FILE *err, struct bt_ctf_event *event)
{
	struct bt_ctf_event_class *event_class = NULL;
	struct bt_ctf_stream_class *stream_class = NULL;
	struct bt_ctf_clock_class *clock_class = NULL;

	event_class = bt_ctf_event_get_class(event);
	assert(event_class);

	stream_class = bt_ctf_event_class_get_stream_class(event_class);
	BT_PUT(event_class);
	assert(stream_class);

	clock_class = stream_class_get_clock_class(err, stream_class);
	bt_put(stream_class);

	return clock_class;
}

static
int copy_find_clock_int_field(FILE *err,
		struct bt_ctf_event *event, struct bt_ctf_event *writer_event,
		struct bt_ctf_field *field, struct bt_ctf_field_type *type,
		struct bt_ctf_field *copy_field)
{
	struct bt_ctf_clock_class *clock_class = NULL, *writer_clock_class = NULL;
	struct bt_ctf_clock_value *clock_value = NULL, *writer_clock_value = NULL;
	uint64_t value;
	int ret;

	clock_class = bt_ctf_field_type_integer_get_mapped_clock_class(type);
	if (!clock_class) {
		return set_int_value(err, field, copy_field, type);
	}

	clock_value = bt_ctf_event_get_clock_value(event, clock_class);
	BT_PUT(clock_class);
	assert(clock_value);

	ret = bt_ctf_clock_value_get_value(clock_value, &value);
	BT_PUT(clock_value);
	if (ret) {
		BT_LOGE("Failed to get clock value.");
		goto error;
	}

	ret = bt_ctf_field_unsigned_integer_set_value(copy_field, value);
	if (ret) {
		BT_LOGE_STR("Failed to set unsigned integer value.");
		goto error;
	}

	writer_clock_class = event_get_clock_class(err, writer_event);
	assert(writer_clock_class);

	writer_clock_value = bt_ctf_clock_value_create(writer_clock_class, value);
	BT_PUT(writer_clock_class);
	if (!writer_clock_value) {
		BT_LOGE_STR("Failed to create clock value.");
		goto error;
	}

	ret = bt_ctf_event_set_clock_value(writer_event, writer_clock_value);
	BT_PUT(writer_clock_value);
	if (ret) {
		BT_LOGE_STR("Failed to set clock value.");
		goto error;
	}

	ret = 0;
	goto end;

error:
	ret = -1;
end:
	return ret;
}

