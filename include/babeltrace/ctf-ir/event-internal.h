#ifndef BABELTRACE_CTF_IR_EVENT_INTERNAL_H
#define BABELTRACE_CTF_IR_EVENT_INTERNAL_H

/*
 * BabelTrace - CTF IR: Event internal
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
#include <babeltrace/ctf/types.h>
#include <babeltrace/ctf-ir/stream-class.h>
#include <babeltrace/ctf-ir/stream.h>
#include <babeltrace/object-internal.h>
#include <glib.h>

#define BT_CTF_EVENT_CLASS_ATTR_ID_INDEX	0
#define BT_CTF_EVENT_CLASS_ATTR_NAME_INDEX	1

struct bt_ctf_event_class {
	struct bt_object base;
	struct bt_value *attributes;
	/* Structure type containing the event's context */
	struct bt_ctf_field_type *context;
	/* Structure type containing the event's fields */
	struct bt_ctf_field_type *fields;
	int frozen;

	/*
	 * This flag indicates if the event class is valid. A valid
	 * event class is _always_ frozen. However, an event class
	 * may be frozen, but not valid yet. This is okay, as long as
	 * no events are created out of this event class.
	 */
	int valid;
};

struct bt_ctf_event {
	struct bt_object base;
	struct bt_ctf_event_class *event_class;
	struct bt_ctf_field *event_header;
	struct bt_ctf_field *context_payload;
	struct bt_ctf_field *fields_payload;
};

BT_HIDDEN
void bt_ctf_event_class_freeze(struct bt_ctf_event_class *event_class);

BT_HIDDEN
int bt_ctf_event_class_serialize(struct bt_ctf_event_class *event_class,
		struct metadata_context *context);

BT_HIDDEN
void bt_ctf_event_class_set_native_byte_order(
		struct bt_ctf_event_class *event_class,
		int byte_order);

BT_HIDDEN
int bt_ctf_event_class_set_stream_id(struct bt_ctf_event_class *event_class,
		uint32_t stream_id);

BT_HIDDEN
int bt_ctf_event_validate(struct bt_ctf_event *event);

BT_HIDDEN
int bt_ctf_event_serialize(struct bt_ctf_event *event,
		struct ctf_stream_pos *pos);

/*
 * Attempt to populate the "id" and "timestamp" fields of the event header if
 * they are present, unset and their types are integers.
 *
 * Not finding these fields or encountering unexpected types is not an error
 * since the user may have defined a different event header layout. In this
 * case, it is expected that the fields be manually populated before appending
 * an event to a stream.
 */
BT_HIDDEN
int bt_ctf_event_populate_event_header(struct bt_ctf_event *event);

#endif /* BABELTRACE_CTF_IR_EVENT_INTERNAL_H */
