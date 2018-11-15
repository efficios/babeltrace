/*
 * Copyright 2018 Philippe Proulx <pproulx@efficios.com>
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

#define BT_LOG_TAG "EVENT-HEADER-FIELD"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/trace-ir/event-header-field.h>
#include <babeltrace/trace-ir/stream-class-internal.h>
#include <babeltrace/trace-ir/fields-internal.h>
#include <babeltrace/trace-ir/field-wrapper-internal.h>
#include <glib.h>

struct bt_field *bt_event_header_field_borrow_field(
		struct bt_event_header_field *header_field)
{
	struct bt_field_wrapper *field_wrapper = (void *) header_field;

	BT_ASSERT_PRE_NON_NULL(field_wrapper, "Event header field");
	return (void *) field_wrapper->field;
}

void bt_event_header_field_release(struct bt_event_header_field *header_field)
{
	struct bt_field_wrapper *field_wrapper = (void *) header_field;

	BT_ASSERT_PRE_NON_NULL(field_wrapper, "Event header field");

	/*
	 * Do not recycle because the pool could be destroyed at this
	 * point. This function is only called when there's an error
	 * anyway because the goal of an event header field wrapper is
	 * to eventually move it to an event with bt_event_move_header()
	 * after creating it.
	 */
	bt_field_wrapper_destroy(field_wrapper);
}

struct bt_event_header_field *bt_event_header_field_create(
		struct bt_stream_class *stream_class)
{
	struct bt_field_wrapper *field_wrapper;

	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");
	BT_ASSERT_PRE(bt_stream_class_borrow_trace_inline(stream_class),
		"Stream class is not part of a trace: %!+S", stream_class);
	BT_ASSERT_PRE(stream_class->event_header_fc,
		"Stream class has no event header field classe: %!+S",
		stream_class);
	field_wrapper = bt_field_wrapper_create(
		&stream_class->event_header_field_pool,
		(void *) stream_class->event_header_fc);
	if (!field_wrapper) {
		BT_LIB_LOGE("Cannot allocate one event header field from stream class: "
			"%![sc-]+S", stream_class);
		goto error;
	}

	BT_ASSERT(field_wrapper->field);
	bt_stream_class_freeze(stream_class);
	goto end;

error:
	if (field_wrapper) {
		bt_field_wrapper_destroy(field_wrapper);
		field_wrapper = NULL;
	}

end:
	return (void *) field_wrapper;
}
