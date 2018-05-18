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

#define BT_LOG_TAG "PACKET-HEADER-FIELD"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/ctf-ir/packet-header-field.h>
#include <babeltrace/ctf-ir/field-wrapper-internal.h>
#include <babeltrace/ctf-ir/fields-internal.h>
#include <glib.h>

struct bt_field *bt_packet_header_field_borrow_field(
		struct bt_packet_header_field *header_field)
{
	struct bt_field_wrapper *field_wrapper = (void *) header_field;

	BT_ASSERT_PRE_NON_NULL(field_wrapper, "Event header field");
	BT_ASSERT_PRE_NON_NULL(field_wrapper->field,
		"Event header field's field object");
	return (void *) field_wrapper->field;
}

void bt_packet_header_field_release(struct bt_packet_header_field *header_field)
{
	struct bt_field_wrapper *field_wrapper = (void *) header_field;

	BT_ASSERT_PRE_NON_NULL(field_wrapper, "Event header field");

	/*
	 * Do not recycle because the pool could be destroyed at this
	 * point. This function is only called when there's an error
	 * anyway because the goal of a packet header field wrapper is
	 * to eventually move it to a packet with
	 * bt_packet_move_header() after creating it.
	 */
	bt_field_wrapper_destroy(field_wrapper);
}
