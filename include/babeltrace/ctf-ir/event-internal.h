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
#include <babeltrace/ctf/types.h>
#include <babeltrace/ctf-ir/stream-class.h>
#include <babeltrace/ctf-ir/packet.h>
#include <babeltrace/object-internal.h>
#include <glib.h>

struct bt_ctf_event {
	struct bt_object base;
	struct bt_ctf_event_class *event_class;
	struct bt_ctf_packet *packet;
	struct bt_ctf_field *event_header;
	struct bt_ctf_field *stream_event_context;
	struct bt_ctf_field *context_payload;
	struct bt_ctf_field *fields_payload;
	GHashTable *clock_values; /* Maps clock addresses to (uint64_t *) */
	int frozen;
};

BT_HIDDEN
int bt_ctf_event_register_stream_clock_values(struct bt_ctf_event *event);

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

BT_HIDDEN
void bt_ctf_event_freeze(struct bt_ctf_event *event);

/*
 * bt_ctf_event_get_class: get an event's class.
 *
 * @param event Event.
 *
 * Returns the event's class, NULL on error.
 */
BT_HIDDEN
struct bt_ctf_event_class *bt_ctf_event_get_class(struct bt_ctf_event *event);

/*
 * bt_ctf_event_get_stream: get an event's associated stream.
 *
 * @param event Event.
 *
 * Returns the event's associated stream, NULL on error.
 */
BT_HIDDEN
struct bt_ctf_stream *bt_ctf_event_get_stream(struct bt_ctf_event *event);

/*
 * bt_ctf_event_get_clock: get an event's associated clock.
 *
 * @param event Event.
 *
 * Returns the event's clock, NULL on error.
 */
BT_HIDDEN
struct bt_ctf_clock *bt_ctf_event_get_clock(struct bt_ctf_event *event);

/*
 * bt_ctf_event_get_payload_field: get an event's payload.
 *
 * @param event Event instance.
 *
 * Returns a field instance on success, NULL on error.
 */
BT_HIDDEN
struct bt_ctf_field *bt_ctf_event_get_payload_field(struct bt_ctf_event *event);

/*
 * bt_ctf_event_set_payload_field: set an event's payload.
 *
 * @param event Event instance.
 * @param payload Field instance (must be a structure).
 *
 * Returns 0 on success, a negative value on error.
 */
BT_HIDDEN
int bt_ctf_event_set_payload_field(struct bt_ctf_event *event,
		struct bt_ctf_field *payload);

/*
 * bt_ctf_event_get_payload_by_index: Get event's field by index.
 *
 * Returns the field associated with the provided index. bt_ctf_field_put()
 * must be called on the returned value. The indexes to be provided are
 * the same as can be retrieved from the event class.
 *
 * @param event Event.
 * @param index Index of field.
 *
 * Returns the event's field, NULL on error.
 *
 * Note: Will return an error if the payload's type is not a structure.
 */
BT_HIDDEN
struct bt_ctf_field *bt_ctf_event_get_payload_by_index(
		struct bt_ctf_event *event, int index);

/*
 * bt_ctf_event_get_header: get an event's header.
 *
 * @param event Event instance.
 *
 * Returns a field instance on success, NULL on error.
 */
BT_HIDDEN
struct bt_ctf_field *bt_ctf_event_get_header(struct bt_ctf_event *event);

/*
 * bt_ctf_event_get_event_context: Get an event's context
 *
 * @param event_class Event class.
 *
 * Returns a field on success (a structure), NULL on error.
 *
 * Note: This function is named this way instead of the expected
 * "bt_ctf_event_get_context" in order to work around a name clash with
 * an unrelated function bearing this name in context.h.
 */
BT_HIDDEN
struct bt_ctf_field *bt_ctf_event_get_event_context(struct bt_ctf_event *event);

/*
 * bt_ctf_event_get_stream_event_context: Get an event's stream event context
 *
 * @param event_class Event class.
 *
 * Returns a field on success (a structure), NULL on error.
 */
BT_HIDDEN
struct bt_ctf_field *bt_ctf_event_get_stream_event_context(
		struct bt_ctf_event *event);

BT_HIDDEN
uint64_t bt_ctf_event_get_clock_value(struct bt_ctf_event *event,
		struct bt_ctf_clock *clock);

/*
 * bt_ctf_event_set_header: set an event's header.
 *
 * The event header's type must match the stream class' event
 * header type.
 *
 * @param event Event instance.
 * @param header Event header field instance.
 *
 * Returns a field instance on success, NULL on error.
 */
BT_HIDDEN
int bt_ctf_event_set_header(
		struct bt_ctf_event *event,
		struct bt_ctf_field *header);

/*
 * bt_ctf_event_set_event_context: Set an event's context
 *
 * @param event Event.
 * @param context Event context field (must match the event class'
 * 	context type).
 *
 * Returns 0 on success, a negative value on error.
 */
BT_HIDDEN
int bt_ctf_event_set_event_context(struct bt_ctf_event *event,
		struct bt_ctf_field *context);

/*
 * bt_ctf_event_set_stream_event_context: Set an event's stream event context
 *
 * @param event Event.
 * @param context Event stream context field (must match the stream class'
 * 	stream event context type).
 *
 * Returns 0 on success, a negative value on error.
 */
BT_HIDDEN
int bt_ctf_event_set_stream_event_context(struct bt_ctf_event *event,
		struct bt_ctf_field *context);

BT_HIDDEN
int bt_ctf_event_set_packet(struct bt_ctf_event *event,
		struct bt_ctf_packet *packet);

#endif /* BABELTRACE_CTF_IR_EVENT_INTERNAL_H */
