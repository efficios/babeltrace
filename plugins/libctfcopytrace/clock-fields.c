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

#include <babeltrace/ctf-ir/event.h>
#include <babeltrace/ctf-ir/packet.h>
#include <babeltrace/ctf-ir/event-class.h>
#include <babeltrace/ctf-ir/stream.h>
#include <babeltrace/ctf-ir/stream-class.h>
#include <babeltrace/ctf-ir/clock-class.h>
#include <babeltrace/ctf-ir/fields.h>
#include <babeltrace/ctf-writer/stream.h>
#include <babeltrace/ctf-ir/field-types.h>
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
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end;
	}

	ret = bt_ctf_field_type_integer_set_mapped_clock_class(type,
			writer_clock_class);
	if (ret) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
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
			fprintf(err, "[error] %s in %s:%d\n",
					__func__, __FILE__, __LINE__);
			goto error;
		}
		ret = find_update_clock_fields(err, entry_type,
				writer_clock_class);
		if (ret) {
			fprintf(err, "[error] %s in %s:%d\n",
					__func__, __FILE__, __LINE__);
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
			fprintf(err, "[error] %s in %s:%d\n",
					__func__, __FILE__, __LINE__);
			goto error;
		}
		ret = find_update_clock_fields(err, entry_type,
				writer_clock_class);
		if (ret) {
			fprintf(err, "[error] %s in %s:%d\n",
					__func__, __FILE__, __LINE__);
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
	if (!entry_type) {
		fprintf(err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto error;
	}
	ret = find_update_clock_fields(err, entry_type, writer_clock_class);
	BT_PUT(entry_type);
	if (ret) {
		fprintf(err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
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
	int ret;
	struct bt_ctf_field_type *entry_type = NULL;

	entry_type = bt_ctf_field_type_array_get_element_type(type);
	if (!entry_type) {
		fprintf(err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto error;
	}
	ret = find_update_clock_fields(err, entry_type, writer_clock_class);
	BT_PUT(entry_type);
	if (ret) {
		fprintf(err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		ret = -1;
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
int find_update_enum_clock_fields(FILE *err, struct bt_ctf_field_type *type,
		struct bt_ctf_clock_class *writer_clock_class)
{
	int ret;
	struct bt_ctf_field_type *entry_type = NULL;

	entry_type = bt_ctf_field_type_enumeration_get_container_type(type);
	if (!entry_type) {
		fprintf(err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto error;
	}
	ret = find_update_clock_fields(err, entry_type, writer_clock_class);
	BT_PUT(entry_type);
	if (ret) {
		fprintf(err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
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
	if (!writer_clock_class) {
		fprintf(err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto error;
	}

	new_type = bt_ctf_field_type_copy(type);
	if (!new_type) {
		fprintf(err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
		goto error;
	}

	if (bt_ctf_field_type_get_type_id(new_type) != BT_CTF_FIELD_TYPE_ID_STRUCT) {
		fprintf(err, "[error] Unexpected header field type\n");
		goto error;
	}

	ret = find_update_struct_clock_fields(err, new_type, writer_clock_class);
	if (ret) {
		fprintf(err, "[error] %s in %s:%d\n",
				__func__, __FILE__, __LINE__);
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
		ret = -1;
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto end;
	}
	ret = bt_ctf_field_floating_point_set_value(copy_field, value);
	if (ret) {
		ret = -1;
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto end;
	}

	ret = 0;

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
		ret = -1;
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto end;
	}
	ret = bt_ctf_field_string_set_value(copy_field, value);
	if (ret) {
		ret = -1;
		fprintf(err, "[error] %s in %s:%d\n", __func__,
				__FILE__, __LINE__);
		goto end;
	}

	ret = 0;

end:
	return ret;
}

BT_HIDDEN
int copy_override_field(FILE *err, struct bt_ctf_event *event,
		struct bt_ctf_event *writer_event, struct bt_ctf_field *field,
		struct bt_ctf_field *copy_field)
{
	struct bt_ctf_field_type *type = NULL;
	int ret;

	type = bt_ctf_field_get_type(field);
	if (!type) {
		ret = -1;
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end;
	}

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

	ret = 0;
	BT_PUT(type);

end:
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
	if (!container) {
		ret = -1;
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end;
	}

	copy_container = bt_ctf_field_enumeration_get_container(copy_field);
	if (!copy_container) {
		ret = -1;
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	ret = copy_override_field(err, event, writer_event, container,
			copy_container);
	if (ret) {
		ret = -1;
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
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
	if (!tag) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	variant_field = bt_ctf_field_variant_get_field(field, tag);
	if (!variant_field) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	copy_variant_field = bt_ctf_field_variant_get_field(copy_field, tag);
	if (!copy_variant_field) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	ret = copy_override_field(err, event, writer_event, variant_field,
			copy_variant_field);
	if (ret) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
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
	if (!length_field) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	ret = bt_ctf_field_unsigned_integer_get_value(length_field, &count);
	if (ret) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	ret = bt_ctf_field_sequence_set_length(copy_field, length_field);
	if (ret) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}
	BT_PUT(length_field);

	for (i = 0; i < count; i++) {
		entry_field = bt_ctf_field_sequence_get_field(field, i);
		if (!entry_field) {
			fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
					__LINE__);
			goto error;
		}

		entry_copy = bt_ctf_field_sequence_get_field(copy_field, i);
		if (!entry_copy) {
			fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
					__LINE__);
			goto error;
		}

		ret = copy_override_field(err, event, writer_event, entry_field,
				entry_copy);
		if (ret) {
			fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
					__LINE__);
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
			fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
					__LINE__);
			goto error;
		}

		entry_copy = bt_ctf_field_array_get_field(copy_field, i);
		if (!entry_copy) {
			fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
					__LINE__);
			goto error;
		}

		ret = copy_override_field(err, event, writer_event, entry_field,
				entry_copy);
		if (ret) {
			ret = -1;
			fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
					__LINE__);
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
			fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
					__LINE__);
			goto error;
		}

		ret = bt_ctf_field_type_structure_get_field(type, &entry_name,
				&entry_type, i);
		if (ret) {
			fprintf(err, "[error] %s in %s:%d\n",
					__func__, __FILE__, __LINE__);
			goto error;
		}

		entry_copy = bt_ctf_field_structure_get_field_by_index(copy_field, i);
		if (!entry_copy) {
			fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
					__LINE__);
			goto error;
		}

		ret = copy_override_field(err, event, writer_event, entry_field,
				entry_copy);
		if (ret) {
			fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
					__LINE__);
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
			ret = -1;
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto end;
		}
		ret = bt_ctf_field_signed_integer_set_value(copy_field, value);
		if (ret) {
			ret = -1;
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto end;
		}
	} else {
		ret = bt_ctf_field_unsigned_integer_get_value(field,
				&uvalue);
		if (ret) {
			ret = -1;
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto end;
		}
		ret = bt_ctf_field_unsigned_integer_set_value(copy_field, uvalue);
		if (ret) {
			ret = -1;
			fprintf(err, "[error] %s in %s:%d\n", __func__,
					__FILE__, __LINE__);
			goto end;
		}
	}
	ret = 0;

end:
	return ret;
}

struct bt_ctf_clock_class *stream_class_get_clock_class(FILE *err,
		struct bt_ctf_stream_class *stream_class)
{
	struct bt_ctf_trace *trace = NULL;
	struct bt_ctf_clock_class *clock_class = NULL;

	trace = bt_ctf_stream_class_get_trace(stream_class);
	if (!trace) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end;
	}

	/* FIXME multi-clock? */
	clock_class = bt_ctf_trace_get_clock_class_by_index(trace, 0);

	bt_put(trace);
end:
	return clock_class;
}

struct bt_ctf_clock_class *event_get_clock_class(FILE *err, struct bt_ctf_event *event)
{
	struct bt_ctf_event_class *event_class = NULL;
	struct bt_ctf_stream_class *stream_class = NULL;
	struct bt_ctf_clock_class *clock_class = NULL;

	event_class = bt_ctf_event_get_class(event);
	if (!event_class) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end;
	}

	stream_class = bt_ctf_event_class_get_stream_class(event_class);
	BT_PUT(event_class);
	if (!stream_class) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto end;
	}

	clock_class = stream_class_get_clock_class(err, stream_class);
	bt_put(stream_class);

end:
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
	if (!clock_value) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	ret = bt_ctf_clock_value_get_value(clock_value, &value);
	BT_PUT(clock_value);
	if (ret) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	ret = bt_ctf_field_unsigned_integer_set_value(copy_field, value);
	if (ret) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	writer_clock_class = event_get_clock_class(err, writer_event);
	if (!writer_clock_class) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	writer_clock_value = bt_ctf_clock_value_create(writer_clock_class, value);
	BT_PUT(writer_clock_class);
	if (!writer_clock_value) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	ret = bt_ctf_event_set_clock_value(writer_event, writer_clock_value);
	BT_PUT(writer_clock_value);
	if (ret) {
		fprintf(err, "[error] %s in %s:%d\n", __func__, __FILE__,
				__LINE__);
		goto error;
	}

	ret = 0;
	goto end;

error:
	ret = -1;
end:
	return ret;
}

