/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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
#include <babeltrace/trace-ir/trace.h>
#include <babeltrace/trace-ir/trace-class-internal.h>
#include <babeltrace/trace-ir/trace-const.h>
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
#include <babeltrace/trace-ir/field-class-internal.h>
#include <babeltrace/trace-ir/attributes-internal.h>
#include <babeltrace/trace-ir/utils-internal.h>
#include <babeltrace/trace-ir/resolve-field-path-internal.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/value.h>
#include <babeltrace/value-const.h>
#include <babeltrace/value-internal.h>
#include <babeltrace/types.h>
#include <babeltrace/endian-internal.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/compat/glib-internal.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

struct bt_trace_is_static_listener_elem {
	bt_trace_is_static_listener_func func;
	bt_trace_listener_removed_func removed;
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
				elem.removed((void *) trace, elem.data);
			}
		}

		g_array_free(trace->is_static_listeners, TRUE);
		trace->is_static_listeners = NULL;
	}

	if (trace->name.str) {
		g_string_free(trace->name.str, TRUE);
		trace->name.str = NULL;
		trace->name.value = NULL;
	}

	if (trace->streams) {
		BT_LOGD_STR("Destroying streams.");
		g_ptr_array_free(trace->streams, TRUE);
		trace->streams = NULL;
	}

	if (trace->stream_classes_stream_count) {
		g_hash_table_destroy(trace->stream_classes_stream_count);
		trace->stream_classes_stream_count = NULL;
	}

	BT_LOGD_STR("Putting trace's class.");
	bt_object_put_ref(trace->class);
	trace->class = NULL;
	g_free(trace);
}

struct bt_trace *bt_trace_create(struct bt_trace_class *tc)
{
	struct bt_trace *trace = NULL;

	BT_LIB_LOGD("Creating trace object: %![tc-]+T", tc);
	trace = g_new0(struct bt_trace, 1);
	if (!trace) {
		BT_LOGE_STR("Failed to allocate one trace.");
		goto error;
	}

	bt_object_init_shared(&trace->base, destroy_trace);
	trace->streams = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_object_try_spec_release);
	if (!trace->streams) {
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

	trace->is_static_listeners = g_array_new(FALSE, TRUE,
		sizeof(struct bt_trace_is_static_listener_elem));
	if (!trace->is_static_listeners) {
		BT_LOGE_STR("Failed to allocate one GArray.");
		goto error;
	}

	trace->class = tc;
	bt_object_get_no_null_check(trace->class);
	BT_LIB_LOGD("Created trace object: %!+t", trace);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(trace);

end:
	return trace;
}

const char *bt_trace_get_name(const struct bt_trace *trace)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	return trace->name.value;
}

enum bt_trace_status bt_trace_set_name(struct bt_trace *trace, const char *name)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	BT_ASSERT_PRE_NON_NULL(name, "Name");
	BT_ASSERT_PRE_TRACE_HOT(trace);
	g_string_assign(trace->name.str, name);
	trace->name.value = trace->name.str->str;
	BT_LIB_LOGV("Set trace's name: %!+t", trace);
	return BT_TRACE_STATUS_OK;
}

uint64_t bt_trace_get_stream_count(const struct bt_trace *trace)
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

const struct bt_stream *bt_trace_borrow_stream_by_index_const(
		const struct bt_trace *trace, uint64_t index)
{
	return bt_trace_borrow_stream_by_index((void *) trace, index);
}

struct bt_stream *bt_trace_borrow_stream_by_id(struct bt_trace *trace,
		uint64_t id)
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

const struct bt_stream *bt_trace_borrow_stream_by_id_const(
		const struct bt_trace *trace, uint64_t id)
{
	return bt_trace_borrow_stream_by_id((void *) trace, id);
}

bt_bool bt_trace_is_static(const struct bt_trace *trace)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	return (bt_bool) trace->is_static;
}

enum bt_trace_status bt_trace_make_static(struct bt_trace *trace)
{	uint64_t i;

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
			elem.func((void *) trace, elem.data);
		}
	}

	return BT_TRACE_STATUS_OK;
}

enum bt_trace_status bt_trace_add_is_static_listener(
		const struct bt_trace *c_trace,
		bt_trace_is_static_listener_func listener,
		bt_trace_listener_removed_func listener_removed, void *data,
		uint64_t *listener_id)
{
	struct bt_trace *trace = (void *) c_trace;
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
	return BT_TRACE_STATUS_OK;
}

BT_ASSERT_PRE_FUNC
static
bool has_listener_id(const struct bt_trace *trace, uint64_t listener_id)
{
	BT_ASSERT(listener_id < trace->is_static_listeners->len);
	return (&g_array_index(trace->is_static_listeners,
			struct bt_trace_is_static_listener_elem,
			listener_id))->func != NULL;
}

enum bt_trace_status bt_trace_remove_is_static_listener(
		const struct bt_trace *c_trace, uint64_t listener_id)
{
	struct bt_trace *trace = (void *) c_trace;
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
		elem->removed((void *) trace, elem->data);
		trace->in_remove_listener = false;
	}

	elem->func = NULL;
	elem->removed = NULL;
	elem->data = NULL;
	BT_LIB_LOGV("Removed \"trace is static\" listener: "
		"%![trace-]+t, listener-id=%" PRIu64,
		trace, listener_id);
	return BT_TRACE_STATUS_OK;
}

BT_HIDDEN
void _bt_trace_freeze(const struct bt_trace *trace)
{
	BT_ASSERT(trace);
	BT_LIB_LOGD("Freezing trace's class: %!+T", trace->class);
	bt_trace_class_freeze(trace->class);
	BT_LIB_LOGD("Freezing trace: %!+t", trace);
	((struct bt_trace *) trace)->frozen = true;
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
uint64_t bt_trace_get_automatic_stream_id(const struct bt_trace *trace,
		const struct bt_stream_class *stream_class)
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

struct bt_trace_class *bt_trace_borrow_class(struct bt_trace *trace)
{
	BT_ASSERT_PRE_NON_NULL(trace, "Trace");
	return trace->class;
}

const struct bt_trace_class *bt_trace_borrow_class_const(
		const struct bt_trace *trace)
{
	return bt_trace_borrow_class((void *) trace);
}

void bt_trace_get_ref(const struct bt_trace *trace)
{
	bt_object_get_ref(trace);
}

void bt_trace_put_ref(const struct bt_trace *trace)
{
	bt_object_put_ref(trace);
}
