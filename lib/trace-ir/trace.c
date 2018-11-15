/*
 * trace.c
 *
 * Babeltrace trace IR - Trace
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

#define BT_LOG_TAG "TRACE"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/trace-ir/trace-internal.h>
#include <babeltrace/trace-ir/clock-class-internal.h>
#include <babeltrace/trace-ir/stream-internal.h>
#include <babeltrace/trace-ir/stream-class-internal.h>
#include <babeltrace/trace-ir/event-internal.h>
#include <babeltrace/trace-ir/event-class.h>
#include <babeltrace/trace-ir/event-class-internal.h>
#include <babeltrace/ctf-writer/functor-internal.h>
#include <babeltrace/ctf-writer/clock-internal.h>
#include <babeltrace/trace-ir/field-wrapper-internal.h>
#include <babeltrace/trace-ir/field-classes-internal.h>
#include <babeltrace/trace-ir/attributes-internal.h>
#include <babeltrace/trace-ir/utils-internal.h>
#include <babeltrace/trace-ir/resolve-field-path-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/values.h>
#include <babeltrace/values-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/types.h>
#include <babeltrace/endian-internal.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/compat/glib-internal.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

struct bt_trace_is_static_listener_elem {
	bt_trace_is_static_listener func;
	bt_trace_listener_removed removed;
	void *data;
};

#define BT_ASSERT_PRE_TRACE_HOT(_trace) \
	BT_ASSERT_PRE_HOT((_trace), "Trace", ": %!+t", (_trace))

static
void destroy_trace(struct bt_object *obj)
{
	struct bt_trace *trace = (void *) obj;

	BT_LIB_LOGD("Destroying trace object: %!+t", trace);

	/*
	 * Call remove listeners first so that everything else still
	 * exists in the trace.
	 */
	if (trace->is_static_listeners) {
		size_t i;

		for (i = 0; i < trace->is_static_listeners->len; i++) {
			struct bt_trace_is_static_listener_elem elem =
				g_array_index(trace->is_static_listeners,
					struct bt_trace_is_static_listener_elem, i);

			if (elem.removed) {
				elem.removed(trace, elem.data);
			}
		}

		g_array_free(trace->is_static_listeners, TRUE);
	}

	bt_object_pool_finalize(&trace->packet_header_field_pool);

	if (trace->environment) {
		BT_LOGD_STR("Destroying environment attributes.");
		bt_attributes_destroy(trace->environment);
	}

	if (trace->name.str) {
		g_string_free(trace->name.str, TRUE);
	}

	if (trace->streams) {
		BT_LOGD_STR("Destroying streams.");
		g_ptr_array_free(trace->streams, TRUE);
	}

	if (trace->stream_classes) {
		BT_LOGD_STR("Destroying stream classes.");
		g_ptr_array_free(trace->stream_classes, TRUE);
	}

	if (trace->stream_classes_stream_count) {
		g_hash_table_destroy(trace->stream_classes_stream_count);
	}

	BT_LOGD_STR("Putting packet header field classe.");
	bt_put(trace->packet_header_fc);
	g_free(trace);
}

static
void free_packet_header_field(struct bt_field_wrapper *field_wrapper,
		struct bt_trace *trace)
{
	bt_field_wrapper_destroy(field_wrapper);
}

struct bt_trace *bt_trace_create(void)
{
	struct bt_trace *trace = NULL;
	int ret;

	BT_LOGD_STR("Creating default trace object.");
	trace = g_new0(struct bt_trace, 1);
	if (!trace) {
		BT_LOGE_STR("Failed to allocate one trace.");
		goto error;
	}

	bt_object_init_shared_with_parent(&trace->base, destroy_trace);
	trace->streams = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_object_try_spec_release);
	if (!trace->streams) {
		BT_LOGE_STR("Failed to allocate one GPtrArray.");
		goto error;
	}

	trace->stream_classes = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_object_try_spec_release);
	if (!trace->stream_classes) {
		BT_LOGE_STR("Failed to allocate one GPtrArray.");
		goto error;
	}

	trace->stream_classes_stream_count = g_hash_table_new(g_direct_hash,
		g_direct_equal);
	if (!trace->stream_classes_stream_count) {
		BT_LOGE_STR("Failed to allocate one GHashTable.");
		goto error;
	}

	trace->name.str = g_string_new(NULL);
	if (!trace->name.str) {
		BT_LOGE_STR("Failed to allocate one GString.");
		goto error;
	}

	trace->environment = bt_attributes_create();
	if (!trace->environment) {
		BT_LOGE_STR("Cannot create empty attributes object.");
		goto error;
	}

	trace->is_static_listeners = g_array_new(FALSE, TRUE,
		sizeof(struct bt_trace_is_static_listener_elem));
	if (!trace->is_static_listeners) {
		BT_LOGE_STR("Failed to allocate one GArray.");
		goto error;
	}

	trace->assigns_automatic_stream_class_id = true;
	ret = bt_object_pool_initialize(&trace->packet_header_field_pool,
		(bt_object_pool_new_object_func) bt_field_wrapper_new,
		(bt_object_pool_destroy_object_func) free_packet_header_field,
		trace);
	if (ret) {
		BT_LOGE("Failed to initialize packet header field pool: ret=%d",
			ret);
		goto error;
	}

	BT_LIB_LOGD("Created trace object: %!+t", trace);
	goto end;

error:
	BT_PUT(trace);

end:
	return trace;
}

const char *bt_trace_get_name(struct bt_trace *trace)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	return trace->name.value;
}

int bt_trace_set_name(struct bt_trace *trace, const char *name)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	BT_ASSERT_PRE_NON_NULL(name, "Name");
	BT_ASSERT_PRE_TRACE_HOT(trace);
	g_string_assign(trace->name.str, name);
	trace->name.value = trace->name.str->str;
	BT_LIB_LOGV("Set trace's name: %!+t", trace);
	return 0;
}

bt_uuid bt_trace_get_uuid(struct bt_trace *trace)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	return trace->uuid.value;
}

int bt_trace_set_uuid(struct bt_trace *trace, bt_uuid uuid)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	BT_ASSERT_PRE_NON_NULL(uuid, "UUID");
	BT_ASSERT_PRE_TRACE_HOT(trace);
	memcpy(trace->uuid.uuid, uuid, BABELTRACE_UUID_LEN);
	trace->uuid.value = trace->uuid.uuid;
	BT_LIB_LOGV("Set trace's UUID: %!+t", trace);
	return 0;
}

BT_ASSERT_FUNC
static
bool trace_has_environment_entry(struct bt_trace *trace, const char *name)
{
	struct bt_value *attribute;

	BT_ASSERT(trace);

	attribute = bt_attributes_borrow_field_value_by_name(
		trace->environment, name);
	return attribute != NULL;
}

static
int set_environment_entry(struct bt_trace *trace, const char *name,
		struct bt_value *value)
{
	int ret;

	BT_ASSERT(trace);
	BT_ASSERT(name);
	BT_ASSERT(value);
	BT_ASSERT_PRE(!trace->frozen ||
		!trace_has_environment_entry(trace, name),
		"Trace is frozen: cannot replace environment entry: "
		"%![trace-]+t, entry-name=\"%s\"", trace, name);
	ret = bt_attributes_set_field_value(trace->environment, name,
		value);
	bt_value_freeze(value);
	if (ret) {
		BT_LIB_LOGE("Cannot set trace's environment entry: "
			"%![trace-]+t, entry-name=\"%s\"", trace, name);
	} else {
		BT_LIB_LOGV("Set trace's environment entry: "
			"%![trace-]+t, entry-name=\"%s\"", trace, name);
	}

	return ret;
}

int bt_trace_set_environment_entry_string(struct bt_trace *trace,
		const char *name, const char *value)
{
	int ret;
	struct bt_value *value_obj;

	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	BT_ASSERT_PRE_NON_NULL(name, "Name");
	BT_ASSERT_PRE_NON_NULL(value, "Value");
	value_obj = bt_value_string_create_init(value);
	if (!value_obj) {
		BT_LOGE_STR("Cannot create a string value object.");
		ret = -1;
		goto end;
	}

	/* set_environment_entry() logs errors */
	ret = set_environment_entry(trace, name, value_obj);

end:
	bt_put(value_obj);
	return ret;
}

int bt_trace_set_environment_entry_integer(
		struct bt_trace *trace, const char *name, int64_t value)
{
	int ret;
	struct bt_value *value_obj;

	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	BT_ASSERT_PRE_NON_NULL(name, "Name");
	value_obj = bt_value_integer_create_init(value);
	if (!value_obj) {
		BT_LOGE_STR("Cannot create an integer value object.");
		ret = -1;
		goto end;
	}

	/* set_environment_entry() logs errors */
	ret = set_environment_entry(trace, name, value_obj);

end:
	bt_put(value_obj);
	return ret;
}

uint64_t bt_trace_get_environment_entry_count(struct bt_trace *trace)
{
	int64_t ret;

	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	ret = bt_attributes_get_count(trace->environment);
	BT_ASSERT(ret >= 0);
	return (uint64_t) ret;
}

void bt_trace_borrow_environment_entry_by_index(
		struct bt_trace *trace, uint64_t index,
		const char **name, struct bt_value **value)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	BT_ASSERT_PRE_NON_NULL(name, "Name");
	BT_ASSERT_PRE_NON_NULL(value, "Value");
	BT_ASSERT_PRE_VALID_INDEX(index,
		bt_attributes_get_count(trace->environment));
	*value = bt_attributes_borrow_field_value(trace->environment, index);
	BT_ASSERT(*value);
	*name = bt_attributes_get_field_name(trace->environment, index);
	BT_ASSERT(*name);
}

struct bt_value *bt_trace_borrow_environment_entry_value_by_name(
		struct bt_trace *trace, const char *name)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	BT_ASSERT_PRE_NON_NULL(name, "Name");
	return bt_attributes_borrow_field_value_by_name(trace->environment,
		name);
}

uint64_t bt_trace_get_stream_count(struct bt_trace *trace)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	return (uint64_t) trace->streams->len;
}

struct bt_stream *bt_trace_borrow_stream_by_index(
		struct bt_trace *trace, uint64_t index)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	BT_ASSERT_PRE_VALID_INDEX(index, trace->streams->len);
	return g_ptr_array_index(trace->streams, index);
}

struct bt_stream *bt_trace_borrow_stream_by_id(
		struct bt_trace *trace, uint64_t id)
{
	struct bt_stream *stream = NULL;
	uint64_t i;

	BT_ASSERT_PRE_NON_NULL(trace, "Trace");

	for (i = 0; i < trace->streams->len; i++) {
		struct bt_stream *stream_candidate =
			g_ptr_array_index(trace->streams, i);

		if (stream_candidate->id == id) {
			stream = stream_candidate;
			goto end;
		}
	}

end:
	return stream;
}

uint64_t bt_trace_get_stream_class_count(struct bt_trace *trace)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	return (uint64_t) trace->stream_classes->len;
}

struct bt_stream_class *bt_trace_borrow_stream_class_by_index(
		struct bt_trace *trace, uint64_t index)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	BT_ASSERT_PRE_VALID_INDEX(index, trace->stream_classes->len);
	return g_ptr_array_index(trace->stream_classes, index);
}

struct bt_stream_class *bt_trace_borrow_stream_class_by_id(
		struct bt_trace *trace, uint64_t id)
{
	struct bt_stream_class *stream_class = NULL;
	uint64_t i;

	BT_ASSERT_PRE_NON_NULL(trace, "Trace");

	for (i = 0; i < trace->stream_classes->len; i++) {
		struct bt_stream_class *stream_class_candidate =
			g_ptr_array_index(trace->stream_classes, i);

		if (stream_class_candidate->id == id) {
			stream_class = stream_class_candidate;
			goto end;
		}
	}

end:
	return stream_class;
}

struct bt_field_class *bt_trace_borrow_packet_header_field_class(
		struct bt_trace *trace)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	return trace->packet_header_fc;
}

int bt_trace_set_packet_header_field_class(struct bt_trace *trace,
		struct bt_field_class *field_class)
{
	int ret;
	struct bt_resolve_field_path_context resolve_ctx = {
		.packet_header = field_class,
		.packet_context = NULL,
		.event_header = NULL,
		.event_common_context = NULL,
		.event_specific_context = NULL,
		.event_payload = NULL,
	};

	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	BT_ASSERT_PRE_NON_NULL(field_class, "Field class");
	BT_ASSERT_PRE_TRACE_HOT(trace);
	BT_ASSERT_PRE(bt_field_class_get_id(field_class) ==
		BT_FIELD_CLASS_ID_STRUCTURE,
		"Packet header field classe is not a structure field classe: %!+F",
		field_class);
	ret = bt_resolve_field_paths(field_class, &resolve_ctx);
	if (ret) {
		goto end;
	}

	bt_field_class_make_part_of_trace(field_class);
	bt_put(trace->packet_header_fc);
	trace->packet_header_fc = bt_get(field_class);
	bt_field_class_freeze(field_class);
	BT_LIB_LOGV("Set trace's packet header field classe: %!+t", trace);

end:
	return ret;
}

bt_bool bt_trace_is_static(struct bt_trace *trace)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	return (bt_bool) trace->is_static;
}

int bt_trace_make_static(struct bt_trace *trace)
{
	uint64_t i;

	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	trace->is_static = true;
	bt_trace_freeze(trace);
	BT_LIB_LOGV("Trace is now static: %!+t", trace);

	/* Call all the "trace is static" listeners */
	for (i = 0; i < trace->is_static_listeners->len; i++) {
		struct bt_trace_is_static_listener_elem elem =
			g_array_index(trace->is_static_listeners,
				struct bt_trace_is_static_listener_elem, i);

		if (elem.func) {
			elem.func(trace, elem.data);
		}
	}

	return 0;
}

int bt_trace_add_is_static_listener(struct bt_trace *trace,
		bt_trace_is_static_listener listener,
		bt_trace_listener_removed listener_removed, void *data,
		uint64_t *listener_id)
{
	uint64_t i;
	struct bt_trace_is_static_listener_elem new_elem = {
		.func = listener,
		.removed = listener_removed,
		.data = data,
	};

	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	BT_ASSERT_PRE_NON_NULL(listener, "Listener");
	BT_ASSERT_PRE(!trace->is_static,
		"Trace is already static: %!+t", trace);
	BT_ASSERT_PRE(trace->in_remove_listener,
		"Cannot call this function while executing a "
		"remove listener: %!+t", trace);

	/* Find the next available spot */
	for (i = 0; i < trace->is_static_listeners->len; i++) {
		struct bt_trace_is_static_listener_elem elem =
			g_array_index(trace->is_static_listeners,
				struct bt_trace_is_static_listener_elem, i);

		if (!elem.func) {
			break;
		}
	}

	if (i == trace->is_static_listeners->len) {
		g_array_append_val(trace->is_static_listeners, new_elem);
	} else {
		g_array_insert_val(trace->is_static_listeners, i, new_elem);
	}

	if (listener_id) {
		*listener_id = i;
	}

	BT_LIB_LOGV("Added \"trace is static\" listener: "
		"%![trace-]+t, listener-id=%" PRIu64, trace, i);
	return 0;
}

BT_ASSERT_PRE_FUNC
static
bool has_listener_id(struct bt_trace *trace, uint64_t listener_id)
{
	BT_ASSERT(listener_id < trace->is_static_listeners->len);
	return (&g_array_index(trace->is_static_listeners,
			struct bt_trace_is_static_listener_elem,
			listener_id))->func != NULL;
}

int bt_trace_remove_is_static_listener(
		struct bt_trace *trace, uint64_t listener_id)
{
	struct bt_trace_is_static_listener_elem *elem;

	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	BT_ASSERT_PRE(!trace->is_static,
		"Trace is already static: %!+t", trace);
	BT_ASSERT_PRE(trace->in_remove_listener,
		"Cannot call this function while executing a "
		"remove listener: %!+t", trace);
	BT_ASSERT_PRE(has_listener_id(trace, listener_id),
		"Trace has no such \"trace is static\" listener ID: "
		"%![trace-]+t, %" PRIu64, trace, listener_id);
	elem = &g_array_index(trace->is_static_listeners,
			struct bt_trace_is_static_listener_elem,
			listener_id);
	BT_ASSERT(elem->func);

	if (elem->removed) {
		/* Call remove listener */
		BT_LIB_LOGV("Calling remove listener: "
			"%![trace-]+t, listener-id=%" PRIu64,
			trace, listener_id);
		trace->in_remove_listener = true;
		elem->removed(trace, elem->data);
		trace->in_remove_listener = false;
	}

	elem->func = NULL;
	elem->removed = NULL;
	elem->data = NULL;
	BT_LIB_LOGV("Removed \"trace is static\" listener: "
		"%![trace-]+t, listener-id=%" PRIu64,
		trace, listener_id);
	return 0;
}

BT_HIDDEN
void _bt_trace_freeze(struct bt_trace *trace)
{
	/* The packet header field classe is already frozen */
	BT_ASSERT(trace);
	BT_LIB_LOGD("Freezing trace: %!+t", trace);
	trace->frozen = true;
}

bt_bool bt_trace_assigns_automatic_stream_class_id(struct bt_trace *trace)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	return (bt_bool) trace->assigns_automatic_stream_class_id;
}

int bt_trace_set_assigns_automatic_stream_class_id(
		struct bt_trace *trace, bt_bool value)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	BT_ASSERT_PRE_TRACE_HOT(trace);
	trace->assigns_automatic_stream_class_id = (bool) value;
	BT_LIB_LOGV("Set trace's automatic stream class ID "
		"assignment property: %!+t", trace);
	return 0;
}

BT_HIDDEN
void bt_trace_add_stream(struct bt_trace *trace, struct bt_stream *stream)
{
	guint count = 0;

	bt_object_set_parent(&stream->base, &trace->base);
	g_ptr_array_add(trace->streams, stream);
	bt_trace_freeze(trace);

	if (bt_g_hash_table_contains(trace->stream_classes_stream_count,
			stream->class)) {
		count = GPOINTER_TO_UINT(g_hash_table_lookup(
			trace->stream_classes_stream_count, stream->class));
	}

	g_hash_table_insert(trace->stream_classes_stream_count,
		stream->class, GUINT_TO_POINTER(count + 1));
}

BT_HIDDEN
uint64_t bt_trace_get_automatic_stream_id(struct bt_trace *trace,
		struct bt_stream_class *stream_class)
{
	gpointer orig_key;
	gpointer value;
	uint64_t id = 0;

	BT_ASSERT(stream_class);
	BT_ASSERT(trace);
	if (g_hash_table_lookup_extended(trace->stream_classes_stream_count,
			stream_class, &orig_key, &value)) {
		id = (uint64_t) GPOINTER_TO_UINT(value);
	}

	return id;
}
