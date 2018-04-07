#ifndef BABELTRACE_CTF_WRITER_EVENT_INTERNAL_H
#define BABELTRACE_CTF_WRITER_EVENT_INTERNAL_H

/*
 * BabelTrace - CTF Writer: Event
 *
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/ctf-ir/event-class-internal.h>
#include <babeltrace/ctf-ir/event-internal.h>
#include <babeltrace/ctf-writer/field-types.h>

struct bt_ctf_stream_class;
struct bt_ctf_stream_pos;
struct metadata_context;

struct bt_ctf_event {
	struct bt_event_common common;
};

struct bt_ctf_event_class {
	struct bt_event_class_common common;
};

BT_HIDDEN
int bt_ctf_event_class_serialize(struct bt_ctf_event_class *event_class,
		struct metadata_context *context);

BT_HIDDEN
int bt_ctf_event_serialize(struct bt_ctf_event *event,
		struct bt_ctf_stream_pos *pos,
		enum bt_ctf_byte_order native_byte_order);

static inline
struct bt_ctf_stream_class *bt_ctf_event_class_borrow_stream_class(
		struct bt_ctf_event_class *event_class)
{
	return BT_FROM_COMMON(bt_event_class_common_borrow_stream_class(
		BT_TO_COMMON(event_class)));
}

#endif /* BABELTRACE_CTF_WRITER_EVENT_INTERNAL_H */
