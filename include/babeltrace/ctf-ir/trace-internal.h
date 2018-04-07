#ifndef BABELTRACE_CTF_IR_TRACE_INTERNAL_H
#define BABELTRACE_CTF_IR_TRACE_INTERNAL_H

/*
 * BabelTrace - CTF IR: Trace internal
 *
 * Copyright 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/ctf-ir/trace.h>
#include <babeltrace/ctf-ir/stream-class-internal.h>
#include <babeltrace/ctf-ir/field-types.h>
#include <babeltrace/ctf-ir/fields.h>
#include <babeltrace/ctf-ir/validation-internal.h>
#include <babeltrace/ctf-ir/attributes-internal.h>
#include <babeltrace/ctf-ir/clock-class-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/values.h>
#include <babeltrace/types.h>
#include <glib.h>
#include <sys/types.h>
#include <babeltrace/compat/uuid-internal.h>

struct bt_trace_common {
	struct bt_object base;
	GString *name;
	int frozen;
	unsigned char uuid[BABELTRACE_UUID_LEN];
	bt_bool uuid_set;
	enum bt_byte_order native_byte_order;
	struct bt_value *environment;
	GPtrArray *clock_classes; /* Array of pointers to bt_clock_class */
	GPtrArray *stream_classes; /* Array of ptrs to bt_stream_class_common */
	GPtrArray *streams; /* Array of ptrs to bt_stream_common */
	struct bt_field_type_common *packet_header_field_type;
	int64_t next_stream_id;

	/*
	 * This flag indicates if the trace is valid. A valid
	 * trace is _always_ frozen.
	 */
	int valid;
};

struct bt_trace {
	struct bt_trace_common common;
	GPtrArray *listeners; /* Array of struct listener_wrapper */
	GArray *is_static_listeners;
	bt_bool is_static;
	bt_bool in_remove_listener;
};

BT_HIDDEN
int bt_trace_object_modification(struct bt_visitor_object *object,
		void *trace_ptr);

BT_HIDDEN
bt_bool bt_trace_common_has_clock_class(struct bt_trace_common *trace,
		struct bt_clock_class *clock_class);

/**
@brief	User function type to use with bt_trace_add_listener().

@param[in] obj	New CTF IR object which is part of the trace
		class hierarchy.
@param[in] data	User data.

@prenotnull{obj}
*/
typedef void (*bt_listener_cb)(struct bt_visitor_object *obj, void *data);

/**
@brief	Adds the trace class modification listener \p listener to
	the CTF IR trace class \p trace_class.

Once you add \p listener to \p trace_class, whenever \p trace_class
is modified, \p listener is called with the new element and with
\p data (user data).

@param[in] trace_class	Trace class to which to add \p listener.
@param[in] listener	Modification listener function.
@param[in] data		User data.
@returns		0 on success, or a negative value on error.

@prenotnull{trace_class}
@prenotnull{listener}
@postrefcountsame{trace_class}
*/
BT_HIDDEN
int bt_trace_add_listener(struct bt_trace *trace_class,
		bt_listener_cb listener, void *data);

BT_HIDDEN
int bt_trace_common_initialize(struct bt_trace_common *trace,
		bt_object_release_func release_func);

BT_HIDDEN
void bt_trace_common_finalize(struct bt_trace_common *trace);

static inline
const char *bt_trace_common_get_name(struct bt_trace_common *trace)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	return trace->name ? trace->name->str : NULL;
}

BT_HIDDEN
int bt_trace_common_set_name(struct bt_trace_common *trace, const char *name);

static inline
const unsigned char *bt_trace_common_get_uuid(struct bt_trace_common *trace)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	return trace->uuid_set ? trace->uuid : NULL;
}

BT_HIDDEN
int bt_trace_common_set_uuid(struct bt_trace_common *trace, const unsigned char *uuid);

BT_HIDDEN
int bt_trace_common_set_environment_field(struct bt_trace_common *trace,
		const char *name, struct bt_value *value);

BT_HIDDEN
int bt_trace_common_set_environment_field_string(struct bt_trace_common *trace,
		const char *name, const char *value);

BT_HIDDEN
int bt_trace_common_set_environment_field_integer(struct bt_trace_common *trace,
		const char *name, int64_t value);

static inline
int64_t bt_trace_common_get_environment_field_count(
		struct bt_trace_common *trace)
{
	int64_t ret;

	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	ret = bt_attributes_get_count(trace->environment);
	BT_ASSERT(ret >= 0);
	return ret;
}

static inline
const char *
bt_trace_common_get_environment_field_name_by_index(
		struct bt_trace_common *trace,
		uint64_t index)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	return bt_attributes_get_field_name(trace->environment, index);
}

static inline
struct bt_value *bt_trace_common_get_environment_field_value_by_index(
		struct bt_trace_common *trace, uint64_t index)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	return bt_attributes_get_field_value(trace->environment, index);
}

static inline
struct bt_value *bt_trace_common_get_environment_field_value_by_name(
		struct bt_trace_common *trace, const char *name)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	BT_ASSERT_PRE_NON_NULL(name, "Name");
	return bt_attributes_get_field_value_by_name(trace->environment,
		name);
}

BT_HIDDEN
int bt_trace_common_add_clock_class(struct bt_trace_common *trace,
		struct bt_clock_class *clock_class);

static inline
int64_t bt_trace_common_get_clock_class_count(struct bt_trace_common *trace)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	return trace->clock_classes->len;
}

static inline
struct bt_clock_class *bt_trace_common_get_clock_class_by_index(
		struct bt_trace_common *trace, uint64_t index)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	BT_ASSERT_PRE(index < trace->clock_classes->len,
		"Index is out of bounds: index=%" PRIu64 ", "
		"count=%u",
		index, trace->clock_classes->len);
	return bt_get(g_ptr_array_index(trace->clock_classes, index));
}

static inline
int64_t bt_trace_common_get_stream_count(struct bt_trace_common *trace)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	return (int64_t) trace->streams->len;
}

static inline
struct bt_stream_common *bt_trace_common_get_stream_by_index(
		struct bt_trace_common *trace,
		uint64_t index)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	BT_ASSERT_PRE(index < trace->streams->len,
		"Index is out of bounds: index=%" PRIu64 ", "
		"count=%u",
		index, trace->streams->len);
	return bt_get(g_ptr_array_index(trace->streams, index));
}

static inline
int64_t bt_trace_common_get_stream_class_count(struct bt_trace_common *trace)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	return (int64_t) trace->stream_classes->len;
}

static inline
struct bt_stream_class_common *bt_trace_common_get_stream_class_by_index(
		struct bt_trace_common *trace, uint64_t index)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	BT_ASSERT_PRE(index < trace->stream_classes->len,
		"Index is out of bounds: index=%" PRIu64 ", "
		"count=%u",
		index, trace->stream_classes->len);
	return bt_get(g_ptr_array_index(trace->stream_classes, index));
}

static inline
struct bt_stream_class_common *bt_trace_common_get_stream_class_by_id(
		struct bt_trace_common *trace, uint64_t id_param)
{
	int i;
	struct bt_stream_class_common *stream_class = NULL;
	int64_t id = (int64_t) id_param;

	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	BT_ASSERT_PRE(id >= 0,
		"Invalid stream class ID: %" PRIu64, id_param);

	for (i = 0; i < trace->stream_classes->len; i++) {
		struct bt_stream_class_common *stream_class_candidate;

		stream_class_candidate =
			g_ptr_array_index(trace->stream_classes, i);

		if (bt_stream_class_common_get_id(stream_class_candidate) ==
				(int64_t) id) {
			stream_class = stream_class_candidate;
			bt_get(stream_class);
			goto end;
		}
	}

end:
	return stream_class;
}

static inline
struct bt_clock_class *bt_trace_common_get_clock_class_by_name(
		struct bt_trace_common *trace, const char *name)
{
	size_t i;
	struct bt_clock_class *clock_class = NULL;

	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	BT_ASSERT_PRE_NON_NULL(name, "Name");

	for (i = 0; i < trace->clock_classes->len; i++) {
		struct bt_clock_class *cur_clk =
			g_ptr_array_index(trace->clock_classes, i);
		const char *cur_clk_name = bt_clock_class_get_name(cur_clk);

		if (!cur_clk_name) {
			goto end;
		}

		if (!strcmp(cur_clk_name, name)) {
			clock_class = cur_clk;
			bt_get(clock_class);
			goto end;
		}
	}

end:
	return clock_class;
}

static inline
enum bt_byte_order bt_trace_common_get_native_byte_order(
		struct bt_trace_common *trace)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	return trace->native_byte_order;
}

BT_HIDDEN
int bt_trace_common_set_native_byte_order(struct bt_trace_common *trace,
		enum bt_byte_order byte_order, bool allow_unspecified);

static inline
struct bt_field_type_common *bt_trace_common_get_packet_header_field_type(
		struct bt_trace_common *trace)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	return bt_get(trace->packet_header_field_type);
}

BT_HIDDEN
int bt_trace_common_set_packet_header_field_type(struct bt_trace_common *trace,
		struct bt_field_type_common *packet_header_field_type);

static inline
void bt_trace_common_freeze(struct bt_trace_common *trace)
{
	int i;

	if (trace->frozen) {
		return;
	}

	BT_LOGD("Freezing trace: addr=%p, name=\"%s\"",
		trace, bt_trace_common_get_name(trace));
	BT_LOGD_STR("Freezing packet header field type.");
	bt_field_type_common_freeze(trace->packet_header_field_type);
	BT_LOGD_STR("Freezing environment attributes.");
	bt_attributes_freeze(trace->environment);

	if (trace->clock_classes->len > 0) {
		BT_LOGD_STR("Freezing clock classes.");
	}

	for (i = 0; i < trace->clock_classes->len; i++) {
		struct bt_clock_class *clock_class =
			g_ptr_array_index(trace->clock_classes, i);

		bt_clock_class_freeze(clock_class);
	}

	trace->frozen = 1;
}

BT_HIDDEN
int bt_trace_common_add_stream_class(struct bt_trace_common *trace,
		struct bt_stream_class_common *stream_class,
		bt_validation_flag_copy_field_type_func copy_field_type_func,
		struct bt_clock_class *init_expected_clock_class,
		int (*map_clock_classes_func)(struct bt_stream_class_common *stream_class,
			struct bt_field_type_common *packet_context_field_type,
			struct bt_field_type_common *event_header_field_type),
		bool check_ts_begin_end_mapped);

#endif /* BABELTRACE_CTF_IR_TRACE_INTERNAL_H */
