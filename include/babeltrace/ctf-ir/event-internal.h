#ifndef BABELTRACE_CTF_IR_EVENT_INTERNAL_H
#define BABELTRACE_CTF_IR_EVENT_INTERNAL_H

/*
 * Babeltrace - CTF IR: Event internal
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
 */

#include <babeltrace/ctf-writer/event-types.h>
#include <babeltrace/ctf-writer/event-fields.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/values.h>
#include <babeltrace/ctf-ir/stream-class.h>
#include <babeltrace/ctf-ir/stream.h>
#include <babeltrace/ctf-ir/packet.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/assert-internal.h>
#include <glib.h>

struct bt_stream_pos;

struct bt_event {
	struct bt_object base;
	struct bt_event_class *event_class;
	struct bt_packet *packet;
	struct bt_field *event_header;
	struct bt_field *stream_event_context;
	struct bt_field *context_payload;
	struct bt_field *fields_payload;
	/* Maps clock classes to bt_clock_value. */
	GHashTable *clock_values;
	int frozen;
};

BT_HIDDEN
int bt_event_serialize(struct bt_event *event,
		struct bt_stream_pos *pos,
		enum bt_byte_order native_byte_order);

BT_HIDDEN
int _bt_event_validate(struct bt_event *event);

BT_HIDDEN
void _bt_event_freeze(struct bt_event *event);

#ifdef BT_DEV_MODE
# define bt_event_validate		_bt_event_validate
# define bt_event_freeze		_bt_event_freeze
#else
# define bt_event_validate(_event)	0
# define bt_event_freeze(_event)
#endif

static inline struct bt_packet *bt_event_borrow_packet(struct bt_event *event)
{
	BT_ASSERT(event);
	return event->packet;
}

BT_HIDDEN
struct bt_stream *bt_event_borrow_stream(struct bt_event *event);

static inline
struct bt_event_class *bt_event_borrow_event_class(
		struct bt_event *event)
{
	BT_ASSERT(event);
	return event->event_class;
}

#endif /* BABELTRACE_CTF_IR_EVENT_INTERNAL_H */
