#ifndef BABELTRACE_CTF_WRITER_TRACE_INTERNAL_H
#define BABELTRACE_CTF_WRITER_TRACE_INTERNAL_H

/*
 * Copyright 2014 EfficiOS Inc.
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
 *
 * The Common Trace Format (CTF) Specification is available at
 * http://www.efficios.com/ctf
 */

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/compat/uuid-internal.h>
#include <babeltrace/ctf-writer/attributes-internal.h>
#include <babeltrace/ctf-writer/clock-class-internal.h>
#include <babeltrace/ctf-writer/field-types.h>
#include <babeltrace/ctf-writer/fields.h>
#include <babeltrace/ctf-writer/stream-class-internal.h>
#include <babeltrace/ctf-writer/trace.h>
#include <babeltrace/ctf-writer/validation-internal.h>
#include <babeltrace/ctf-writer/object-internal.h>
#include <babeltrace/types.h>
#include <babeltrace/values-internal.h>
#include <glib.h>
#include <sys/types.h>

struct bt_ctf_trace_common {
	struct bt_ctf_object base;
	GString *name;
	int frozen;
	unsigned char uuid[BABELTRACE_UUID_LEN];
	bt_bool uuid_set;
	enum bt_ctf_byte_order native_byte_order;
	struct bt_ctf_private_value *environment;
	GPtrArray *clock_classes; /* Array of pointers to bt_ctf_clock_class */
	GPtrArray *stream_classes; /* Array of ptrs to bt_ctf_stream_class_common */
	GPtrArray *streams; /* Array of ptrs to bt_ctf_stream_common */
	struct bt_ctf_field_type_common *packet_header_field_type;
	int64_t next_stream_id;

	/*
	 * This flag indicates if the trace is valid. A valid
	 * trace is _always_ frozen.
	 */
	int valid;
};

BT_HIDDEN
bt_bool bt_ctf_trace_common_has_clock_class(struct bt_ctf_trace_common *trace,
		struct bt_ctf_clock_class *clock_class);

BT_HIDDEN
int bt_ctf_trace_common_initialize(struct bt_ctf_trace_common *trace,
		bt_ctf_object_release_func release_func);

BT_HIDDEN
void bt_ctf_trace_common_finalize(struct bt_ctf_trace_common *trace);

static inline
const char *bt_ctf_trace_common_get_name(struct bt_ctf_trace_common *trace)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	return trace->name ? trace->name->str : NULL;
}

BT_HIDDEN
int bt_ctf_trace_common_set_name(struct bt_ctf_trace_common *trace, const char *name);

static inline
const unsigned char *bt_ctf_trace_common_get_uuid(struct bt_ctf_trace_common *trace)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	return trace->uuid_set ? trace->uuid : NULL;
}

BT_HIDDEN
int bt_ctf_trace_common_set_uuid(struct bt_ctf_trace_common *trace, const unsigned char *uuid);

BT_HIDDEN
int bt_ctf_trace_common_set_environment_field(struct bt_ctf_trace_common *trace,
		const char *name, struct bt_ctf_private_value *value);

BT_HIDDEN
int bt_ctf_trace_common_set_environment_field_string(struct bt_ctf_trace_common *trace,
		const char *name, const char *value);

BT_HIDDEN
int bt_ctf_trace_common_set_environment_field_integer(struct bt_ctf_trace_common *trace,
		const char *name, int64_t value);

static inline
int64_t bt_ctf_trace_common_get_environment_field_count(
		struct bt_ctf_trace_common *trace)
{
	int64_t ret;

	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	ret = bt_ctf_attributes_get_count(trace->environment);
	BT_ASSERT(ret >= 0);
	return ret;
}

static inline
const char *
bt_ctf_trace_common_get_environment_field_name_by_index(
		struct bt_ctf_trace_common *trace, uint64_t index)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	return bt_ctf_attributes_get_field_name(trace->environment, index);
}

static inline
struct bt_ctf_private_value *
bt_ctf_trace_common_borrow_environment_field_value_by_index(
		struct bt_ctf_trace_common *trace, uint64_t index)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	return bt_ctf_attributes_borrow_field_value(trace->environment, index);
}

static inline
struct bt_ctf_private_value *
bt_ctf_trace_common_borrow_environment_field_value_by_name(
		struct bt_ctf_trace_common *trace, const char *name)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	BT_ASSERT_PRE_NON_NULL(name, "Name");
	return bt_ctf_attributes_borrow_field_value_by_name(trace->environment,
		name);
}

BT_HIDDEN
int bt_ctf_trace_common_add_clock_class(struct bt_ctf_trace_common *trace,
		struct bt_ctf_clock_class *clock_class);

static inline
int64_t bt_ctf_trace_common_get_clock_class_count(struct bt_ctf_trace_common *trace)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	return trace->clock_classes->len;
}

static inline
struct bt_ctf_clock_class *bt_ctf_trace_common_borrow_clock_class_by_index(
		struct bt_ctf_trace_common *trace, uint64_t index)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	BT_ASSERT_PRE(index < trace->clock_classes->len,
		"Index is out of bounds: index=%" PRIu64 ", "
		"count=%u",
		index, trace->clock_classes->len);
	return g_ptr_array_index(trace->clock_classes, index);
}

static inline
int64_t bt_ctf_trace_common_get_stream_count(struct bt_ctf_trace_common *trace)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	return (int64_t) trace->streams->len;
}

static inline
struct bt_ctf_stream_common *bt_ctf_trace_common_borrow_stream_by_index(
		struct bt_ctf_trace_common *trace,
		uint64_t index)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	BT_ASSERT_PRE(index < trace->streams->len,
		"Index is out of bounds: index=%" PRIu64 ", "
		"count=%u",
		index, trace->streams->len);
	return g_ptr_array_index(trace->streams, index);
}

static inline
int64_t bt_ctf_trace_common_get_stream_class_count(struct bt_ctf_trace_common *trace)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	return (int64_t) trace->stream_classes->len;
}

static inline
struct bt_ctf_stream_class_common *bt_ctf_trace_common_borrow_stream_class_by_index(
		struct bt_ctf_trace_common *trace, uint64_t index)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	BT_ASSERT_PRE(index < trace->stream_classes->len,
		"Index is out of bounds: index=%" PRIu64 ", "
		"count=%u",
		index, trace->stream_classes->len);
	return g_ptr_array_index(trace->stream_classes, index);
}

static inline
struct bt_ctf_stream_class_common *bt_ctf_trace_common_borrow_stream_class_by_id(
		struct bt_ctf_trace_common *trace, uint64_t id_param)
{
	int i;
	struct bt_ctf_stream_class_common *stream_class = NULL;
	int64_t id = (int64_t) id_param;

	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	BT_ASSERT_PRE(id >= 0,
		"Invalid stream class ID: %" PRIu64, id_param);

	for (i = 0; i < trace->stream_classes->len; i++) {
		struct bt_ctf_stream_class_common *stream_class_candidate;

		stream_class_candidate =
			g_ptr_array_index(trace->stream_classes, i);

		if (bt_ctf_stream_class_common_get_id(stream_class_candidate) ==
				(int64_t) id) {
			stream_class = stream_class_candidate;
			goto end;
		}
	}

end:
	return stream_class;
}

static inline
struct bt_ctf_clock_class *bt_ctf_trace_common_borrow_clock_class_by_name(
		struct bt_ctf_trace_common *trace, const char *name)
{
	size_t i;
	struct bt_ctf_clock_class *clock_class = NULL;

	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	BT_ASSERT_PRE_NON_NULL(name, "Name");

	for (i = 0; i < trace->clock_classes->len; i++) {
		struct bt_ctf_clock_class *cur_clk =
			g_ptr_array_index(trace->clock_classes, i);
		const char *cur_clk_name = bt_ctf_clock_class_get_name(cur_clk);

		if (!cur_clk_name) {
			goto end;
		}

		if (!strcmp(cur_clk_name, name)) {
			clock_class = cur_clk;
			goto end;
		}
	}

end:
	return clock_class;
}

static inline
enum bt_ctf_byte_order bt_ctf_trace_common_get_native_byte_order(
		struct bt_ctf_trace_common *trace)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	return trace->native_byte_order;
}

BT_HIDDEN
int bt_ctf_trace_common_set_native_byte_order(struct bt_ctf_trace_common *trace,
		enum bt_ctf_byte_order byte_order, bool allow_unspecified);

static inline
struct bt_ctf_field_type_common *bt_ctf_trace_common_borrow_packet_header_field_type(
		struct bt_ctf_trace_common *trace)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	return trace->packet_header_field_type;
}

BT_HIDDEN
int bt_ctf_trace_common_set_packet_header_field_type(struct bt_ctf_trace_common *trace,
		struct bt_ctf_field_type_common *packet_header_field_type);

static inline
void bt_ctf_trace_common_freeze(struct bt_ctf_trace_common *trace)
{
	int i;

	if (trace->frozen) {
		return;
	}

	BT_LOGD("Freezing trace: addr=%p, name=\"%s\"",
		trace, bt_ctf_trace_common_get_name(trace));
	BT_LOGD_STR("Freezing packet header field type.");
	bt_ctf_field_type_common_freeze(trace->packet_header_field_type);
	BT_LOGD_STR("Freezing environment attributes.");
	bt_ctf_attributes_freeze(trace->environment);

	if (trace->clock_classes->len > 0) {
		BT_LOGD_STR("Freezing clock classes.");
	}

	for (i = 0; i < trace->clock_classes->len; i++) {
		struct bt_ctf_clock_class *clock_class =
			g_ptr_array_index(trace->clock_classes, i);

		bt_ctf_clock_class_freeze(clock_class);
	}

	trace->frozen = 1;
}

BT_HIDDEN
int bt_ctf_trace_common_add_stream_class(struct bt_ctf_trace_common *trace,
		struct bt_ctf_stream_class_common *stream_class,
		bt_ctf_validation_flag_copy_field_type_func copy_field_type_func,
		struct bt_ctf_clock_class *init_expected_clock_class,
		int (*map_clock_classes_func)(struct bt_ctf_stream_class_common *stream_class,
			struct bt_ctf_field_type_common *packet_context_field_type,
			struct bt_ctf_field_type_common *event_header_field_type),
		bool check_ts_begin_end_mapped);

struct bt_ctf_trace {
	struct bt_ctf_trace_common common;
};

/*
 * bt_ctf_trace_get_metadata_string: get metadata string.
 *
 * Get the trace's TSDL metadata. The caller assumes the ownership of the
 * returned string.
 *
 * @param trace Trace instance.
 *
 * Returns the metadata string on success, NULL on error.
 */
BT_HIDDEN
char *bt_ctf_trace_get_metadata_string(struct bt_ctf_trace *trace);

BT_HIDDEN
struct bt_ctf_trace *bt_ctf_trace_create(void);

BT_HIDDEN
int64_t bt_ctf_trace_get_clock_class_count(
		struct bt_ctf_trace *trace);

BT_HIDDEN
struct bt_ctf_clock_class *bt_ctf_trace_get_clock_class_by_index(
		struct bt_ctf_trace *trace, uint64_t index);

BT_HIDDEN
struct bt_ctf_clock_class *bt_ctf_trace_get_clock_class_by_name(
		struct bt_ctf_trace *trace, const char *name);

BT_HIDDEN
int bt_ctf_trace_add_clock_class(struct bt_ctf_trace *trace,
		struct bt_ctf_clock_class *clock_class);

BT_HIDDEN
int64_t bt_ctf_trace_get_environment_field_count(
		struct bt_ctf_trace *trace);

BT_HIDDEN
const char *bt_ctf_trace_get_environment_field_name_by_index(
		struct bt_ctf_trace *trace, uint64_t index);

BT_HIDDEN
struct bt_ctf_value *
bt_ctf_trace_get_environment_field_value_by_index(struct bt_ctf_trace *trace,
		uint64_t index);

BT_HIDDEN
struct bt_ctf_value *
bt_ctf_trace_get_environment_field_value_by_name(
		struct bt_ctf_trace *trace, const char *name);

#endif /* BABELTRACE_CTF_WRITER_TRACE_INTERNAL_H */
