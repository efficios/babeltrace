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

#define BT_LOG_TAG "LIB/TRACE-CLASS"
#include "lib/logging.h"

#include "lib/assert-pre.h"
#include "lib/assert-post.h"
#include <babeltrace2/trace-ir/trace-class.h>
#include <babeltrace2/trace-ir/event-class.h>
#include "ctf-writer/functor.h"
#include "ctf-writer/clock.h"
#include "compat/compiler.h"
#include <babeltrace2/value.h>
#include "lib/value.h"
#include <babeltrace2/types.h>
#include "compat/endian.h"
#include "common/assert.h"
#include "compat/glib.h"
#include <inttypes.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include "clock-class.h"
#include "event-class.h"
#include "event.h"
#include "field-class.h"
#include "field-wrapper.h"
#include "resolve-field-path.h"
#include "stream-class.h"
#include "stream.h"
#include "trace.h"
#include "utils.h"
#include "lib/value.h"
#include "lib/func-status.h"

struct bt_trace_class_destruction_listener_elem {
	bt_trace_class_destruction_listener_func func;
	void *data;
};

#define BT_ASSERT_PRE_DEV_TRACE_CLASS_HOT(_tc)				\
	BT_ASSERT_PRE_DEV_HOT((_tc), "Trace class", ": %!+T", (_tc))

static
void destroy_trace_class(struct bt_object *obj)
{
	struct bt_trace_class *tc = (void *) obj;

	BT_LIB_LOGD("Destroying trace class object: %!+T", tc);
	BT_OBJECT_PUT_REF_AND_RESET(tc->user_attributes);

	/*
	 * Call destruction listener functions so that everything else
	 * still exists in the trace class.
	 */
	if (tc->destruction_listeners) {
		uint64_t i;
		const struct bt_error *saved_error;

		BT_LIB_LOGD("Calling trace class destruction listener(s): %!+T", tc);

		/*
		* The trace class' reference count is 0 if we're here. Increment
		* it to avoid a double-destroy (possibly infinitely recursive).
		* This could happen for example if a destruction listener did
		* bt_object_get_ref() (or anything that causes
		* bt_object_get_ref() to be called) on the trace class (ref.
		* count goes from 0 to 1), and then bt_object_put_ref(): the
		* reference count would go from 1 to 0 again and this function
		* would be called again.
		*/
		tc->base.ref_count++;

		saved_error = bt_current_thread_take_error();

		/* Call all the trace class destruction listeners */
		for (i = 0; i < tc->destruction_listeners->len; i++) {
			struct bt_trace_class_destruction_listener_elem elem =
				g_array_index(tc->destruction_listeners,
						struct bt_trace_class_destruction_listener_elem, i);

			if (elem.func) {
				elem.func(tc, elem.data);
				BT_ASSERT_POST_NO_ERROR();
			}

			/*
			 * The destruction listener should not have kept a
			 * reference to the trace class.
			 */
			BT_ASSERT_POST(tc->base.ref_count == 1, "Destruction listener kept a reference to the trace class being destroyed: %![tc-]+T", tc);
		}
		g_array_free(tc->destruction_listeners, TRUE);
		tc->destruction_listeners = NULL;

		if (saved_error) {
			BT_CURRENT_THREAD_MOVE_ERROR_AND_RESET(saved_error);
		}
	}

	if (tc->stream_classes) {
		BT_LOGD_STR("Destroying stream classes.");
		g_ptr_array_free(tc->stream_classes, TRUE);
		tc->stream_classes = NULL;
	}

	g_free(tc);
}

struct bt_trace_class *bt_trace_class_create(bt_self_component *self_comp)
{
	struct bt_trace_class *tc = NULL;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_NON_NULL(self_comp, "Self component");
	BT_LOGD_STR("Creating default trace class object.");
	tc = g_new0(struct bt_trace_class, 1);
	if (!tc) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate one trace class.");
		goto error;
	}

	bt_object_init_shared_with_parent(&tc->base, destroy_trace_class);
	tc->user_attributes = bt_value_map_create();
	if (!tc->user_attributes) {
		BT_LIB_LOGE_APPEND_CAUSE(
			"Failed to create a map value object.");
		goto error;
	}

	tc->stream_classes = g_ptr_array_new_with_free_func(
		(GDestroyNotify) bt_object_try_spec_release);
	if (!tc->stream_classes) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate one GPtrArray.");
		goto error;
	}

	tc->destruction_listeners = g_array_new(FALSE, TRUE,
		sizeof(struct bt_trace_class_destruction_listener_elem));
	if (!tc->destruction_listeners) {
		BT_LIB_LOGE_APPEND_CAUSE("Failed to allocate one GArray.");
		goto error;
	}

	tc->assigns_automatic_stream_class_id = true;
	BT_LIB_LOGD("Created trace class object: %!+T", tc);
	goto end;

error:
	BT_OBJECT_PUT_REF_AND_RESET(tc);

end:
	return tc;
}

enum bt_trace_class_add_listener_status bt_trace_class_add_destruction_listener(
		const struct bt_trace_class *_tc,
		bt_trace_class_destruction_listener_func listener,
		void *data, bt_listener_id *listener_id)
{
	struct bt_trace_class *tc = (void *) _tc;
	uint64_t i;
	struct bt_trace_class_destruction_listener_elem new_elem = {
		.func = listener,
		.data = data,
	};

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_NON_NULL(tc, "Trace class");
	BT_ASSERT_PRE_NON_NULL(listener, "Listener");

	/* Find the next available spot */
	for (i = 0; i < tc->destruction_listeners->len; i++) {
		struct bt_trace_class_destruction_listener_elem elem =
			g_array_index(tc->destruction_listeners,
				struct bt_trace_class_destruction_listener_elem, i);

		if (!elem.func) {
			break;
		}
	}

	if (i == tc->destruction_listeners->len) {
		g_array_append_val(tc->destruction_listeners, new_elem);
	} else {
		g_array_insert_val(tc->destruction_listeners, i, new_elem);
	}

	if (listener_id) {
		*listener_id = i;
	}

	BT_LIB_LOGD("Added trace class destruction listener: %![tc-]+T, "
			"listener-id=%" PRIu64, tc, i);
	return BT_FUNC_STATUS_OK;
}

static
bool has_listener_id(const struct bt_trace_class *tc, uint64_t listener_id)
{
	BT_ASSERT(listener_id < tc->destruction_listeners->len);
	return (&g_array_index(tc->destruction_listeners,
			struct bt_trace_class_destruction_listener_elem,
			listener_id))->func;
}

enum bt_trace_class_remove_listener_status bt_trace_class_remove_destruction_listener(
		const struct bt_trace_class *_tc, bt_listener_id listener_id)
{
	struct bt_trace_class *tc = (void *) _tc;
	struct bt_trace_class_destruction_listener_elem *elem;

	BT_ASSERT_PRE_NO_ERROR();
	BT_ASSERT_PRE_NON_NULL(tc, "Trace class");
	BT_ASSERT_PRE(has_listener_id(tc, listener_id),
		"Trace class has no such trace class destruction listener ID: "
		"%![tc-]+T, %" PRIu64, tc, listener_id);
	elem = &g_array_index(tc->destruction_listeners,
			struct bt_trace_class_destruction_listener_elem,
			listener_id);
	BT_ASSERT(elem->func);

	elem->func = NULL;
	elem->data = NULL;
	BT_LIB_LOGD("Removed trace class destruction listener: "
		"%![tc-]+T, listener-id=%" PRIu64,
		tc, listener_id);
	return BT_FUNC_STATUS_OK;
}

uint64_t bt_trace_class_get_stream_class_count(const struct bt_trace_class *tc)
{
	BT_ASSERT_PRE_DEV_NON_NULL(tc, "Trace class");
	return (uint64_t) tc->stream_classes->len;
}

struct bt_stream_class *bt_trace_class_borrow_stream_class_by_index(
		struct bt_trace_class *tc, uint64_t index)
{
	BT_ASSERT_PRE_DEV_NON_NULL(tc, "Trace class");
	BT_ASSERT_PRE_DEV_VALID_INDEX(index, tc->stream_classes->len);
	return g_ptr_array_index(tc->stream_classes, index);
}

const struct bt_stream_class *
bt_trace_class_borrow_stream_class_by_index_const(
		const struct bt_trace_class *tc, uint64_t index)
{
	return bt_trace_class_borrow_stream_class_by_index(
		(void *) tc, index);
}

struct bt_stream_class *bt_trace_class_borrow_stream_class_by_id(
		struct bt_trace_class *tc, uint64_t id)
{
	struct bt_stream_class *stream_class = NULL;
	uint64_t i;

	BT_ASSERT_PRE_DEV_NON_NULL(tc, "Trace class");

	for (i = 0; i < tc->stream_classes->len; i++) {
		struct bt_stream_class *stream_class_candidate =
			g_ptr_array_index(tc->stream_classes, i);

		if (stream_class_candidate->id == id) {
			stream_class = stream_class_candidate;
			goto end;
		}
	}

end:
	return stream_class;
}

const struct bt_stream_class *
bt_trace_class_borrow_stream_class_by_id_const(
		const struct bt_trace_class *tc, uint64_t id)
{
	return bt_trace_class_borrow_stream_class_by_id((void *) tc, id);
}

BT_HIDDEN
void _bt_trace_class_freeze(const struct bt_trace_class *tc)
{
	BT_ASSERT(tc);
	BT_LIB_LOGD("Freezing trace class: %!+T", tc);
	((struct bt_trace_class *) tc)->frozen = true;
}

bt_bool bt_trace_class_assigns_automatic_stream_class_id(const struct bt_trace_class *tc)
{
	BT_ASSERT_PRE_DEV_NON_NULL(tc, "Trace class");
	return (bt_bool) tc->assigns_automatic_stream_class_id;
}

void bt_trace_class_set_assigns_automatic_stream_class_id(struct bt_trace_class *tc,
		bt_bool value)
{
	BT_ASSERT_PRE_NON_NULL(tc, "Trace class");
	BT_ASSERT_PRE_DEV_TRACE_CLASS_HOT(tc);
	tc->assigns_automatic_stream_class_id = (bool) value;
	BT_LIB_LOGD("Set trace class's automatic stream class ID "
		"assignment property: %!+T", tc);
}

const struct bt_value *bt_trace_class_borrow_user_attributes_const(
		const struct bt_trace_class *trace_class)
{
	BT_ASSERT_PRE_DEV_NON_NULL(trace_class, "Trace class");
	return trace_class->user_attributes;
}

struct bt_value *bt_trace_class_borrow_user_attributes(
		struct bt_trace_class *trace_class)
{
	return (void *) bt_trace_class_borrow_user_attributes_const(
		(void *) trace_class);
}

void bt_trace_class_set_user_attributes(struct bt_trace_class *trace_class,
		const struct bt_value *user_attributes)
{
	BT_ASSERT_PRE_NON_NULL(trace_class, "Trace class");
	BT_ASSERT_PRE_NON_NULL(user_attributes, "User attributes");
	BT_ASSERT_PRE(user_attributes->type == BT_VALUE_TYPE_MAP,
		"User attributes object is not a map value object.");
	BT_ASSERT_PRE_DEV_TRACE_CLASS_HOT(trace_class);
	bt_object_put_ref_no_null_check(trace_class->user_attributes);
	trace_class->user_attributes = (void *) user_attributes;
	bt_object_get_ref_no_null_check(trace_class->user_attributes);
}

void bt_trace_class_get_ref(const struct bt_trace_class *trace_class)
{
	bt_object_get_ref(trace_class);
}

void bt_trace_class_put_ref(const struct bt_trace_class *trace_class)
{
	bt_object_put_ref(trace_class);
}
