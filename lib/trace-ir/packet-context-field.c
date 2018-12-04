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

#define BT_LOG_TAG "PACKET-CONTEXT-FIELD"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/trace-ir/packet-context-field.h>
#include <babeltrace/trace-ir/stream-class-internal.h>
#include <babeltrace/trace-ir/fields-internal.h>
#include <babeltrace/trace-ir/field-wrapper-internal.h>
#include <glib.h>

struct bt_field *bt_packet_context_field_borrow_field(
		struct bt_packet_context_field *context_field)
{
	struct bt_field_wrapper *field_wrapper = (void *) context_field;

	BT_ASSERT_PRE_NON_NULL(field_wrapper, "Packet context field");
	return (void *) field_wrapper->field;
}

void bt_packet_context_field_release(
		struct bt_packet_context_field *context_field)
{
	struct bt_field_wrapper *field_wrapper = (void *) context_field;

	BT_ASSERT_PRE_NON_NULL(field_wrapper, "Packet context field");

	/*
	 * Do not recycle because the pool could be destroyed at this
	 * point. This function is only called when there's an error
	 * anyway because the goal of a packet context field wrapper is
	 * to eventually move it to a packet with
	 * bt_packet_move_context() after creating it.
	 */
	bt_field_wrapper_destroy(field_wrapper);
}

struct bt_packet_context_field *bt_packet_context_field_create(
		struct bt_stream_class *priv_stream_class)
{
	struct bt_stream_class *stream_class = (void *) priv_stream_class;
	struct bt_field_wrapper *field_wrapper;

	BT_ASSERT_PRE_NON_NULL(stream_class, "Stream class");
	BT_ASSERT_PRE(stream_class->frozen,
		"Stream class is not part of a trace: %!+S", stream_class);
	BT_ASSERT_PRE(stream_class->packet_context_fc,
		"Stream class has no packet context field classe: %!+S",
		stream_class);
	field_wrapper = bt_field_wrapper_create(
		&stream_class->packet_context_field_pool,
		(void *) stream_class->packet_context_fc);
	if (!field_wrapper) {
		BT_LIB_LOGE("Cannot allocate one packet context field from stream class: "
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
