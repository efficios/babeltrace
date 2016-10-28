#ifndef BABELTRACE_CTF_IR_EVENT_H
#define BABELTRACE_CTF_IR_EVENT_H

/*
 * BabelTrace - CTF IR: Event
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

#include <stdint.h>
#include <stddef.h>
#include <babeltrace/values.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_ctf_event_class;
struct bt_ctf_event;
struct bt_ctf_field;
struct bt_ctf_field_type;
struct bt_ctf_stream_class;
struct bt_ctf_packet;

/*
 * bt_ctf_event_create: instanciate an event.
 *
 * Allocate a new event of the given event class. The creation of an event
 * sets its reference count to 1. Each instance shares the ownership of the
 * event class using its reference count.
 *
 * An event class must be associated with a stream class before events
 * may be instanciated.
 *
 * @param event_class Event class.
 *
 * Returns an allocated field type on success, NULL on error.
 */
extern struct bt_ctf_event *bt_ctf_event_create(
		struct bt_ctf_event_class *event_class);

/*
 * bt_ctf_event_get_class: get an event's class.
 *
 * @param event Event.
 *
 * Returns the event's class, NULL on error.
 */
extern struct bt_ctf_event_class *bt_ctf_event_get_class(
		struct bt_ctf_event *event);

/*
 * bt_ctf_event_get_stream: get an event's associated stream.
 *
 * @param event Event.
 *
 * Returns the event's associated stream, NULL on error.
 */
extern struct bt_ctf_stream *bt_ctf_event_get_stream(
		struct bt_ctf_event *event);

/*
 * bt_ctf_event_get_clock: get an event's associated clock.
 *
 * @param event Event.
 *
 * Returns the event's clock, NULL on error.
 */
extern struct bt_ctf_clock *bt_ctf_event_get_clock(
		struct bt_ctf_event *event);

/*
 * bt_ctf_event_get_payload_field: get an event's payload.
 *
 * @param event Event instance.
 *
 * Returns a field instance on success, NULL on error.
 */
extern struct bt_ctf_field *bt_ctf_event_get_payload_field(
		struct bt_ctf_event *event);

/*
 * bt_ctf_event_set_payload_field: set an event's payload.
 *
 * @param event Event instance.
 * @param payload Field instance (must be a structure).
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_event_set_payload_field(struct bt_ctf_event *event,
		struct bt_ctf_field *payload);

/*
 * bt_ctf_event_get_payload: get an event's field.
 *
 * Returns the field matching "name". bt_put() must be called on the
 * returned value.
 *
 * @param event Event instance.
 * @param name Event field name, see notes.
 *
 * Returns a field instance on success, NULL on error.
 *
 * Note: Passing a name will cause the function to perform a look-up by
 *	name assuming the event's payload is a structure. This will return
 *	the raw payload instance if name is NULL.
 */
extern struct bt_ctf_field *bt_ctf_event_get_payload(struct bt_ctf_event *event,
		const char *name);

/*
 * bt_ctf_event_set_payload: set an event's field.
 *
 * Set a manually allocated field as an event's payload. The event will share
 * the field's ownership by using its reference count.
 * bt_put() must be called on the returned value.
 *
 * @param event Event instance.
 * @param name Event field name, see notes.
 * @param value Instance of a field whose type corresponds to the event's field.
 *
 * Returns 0 on success, a negative value on error.
 *
 * Note: The function will return an error if a name is provided and the payload
 *	type is not a structure. If name is NULL, the payload field will be set
 *	directly and must match the event class' payload's type.
 */
extern int bt_ctf_event_set_payload(struct bt_ctf_event *event,
		const char *name,
		struct bt_ctf_field *value);

/*
 * bt_ctf_event_get_payload_by_index: Get event's field by index.
 *
 * Returns the field associated with the provided index. bt_put()
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
extern struct bt_ctf_field *bt_ctf_event_get_payload_by_index(
		struct bt_ctf_event *event, int index);

/*
 * bt_ctf_event_get_header: get an event's header.
 *
 * @param event Event instance.
 *
 * Returns a field instance on success, NULL on error.
 */
extern struct bt_ctf_field *bt_ctf_event_get_header(
		struct bt_ctf_event *event);

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
extern int bt_ctf_event_set_header(
		struct bt_ctf_event *event,
		struct bt_ctf_field *header);

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
extern struct bt_ctf_field *bt_ctf_event_get_event_context(
		struct bt_ctf_event *event);

/*
 * bt_ctf_event_set_event_context: Set an event's context
 *
 * @param event Event.
 * @param context Event context field (must match the event class'
 * 	context type).
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_event_set_event_context(struct bt_ctf_event *event,
		struct bt_ctf_field *context);

/*
 * bt_ctf_event_get_stream_event_context: Get an event's stream event context
 *
 * @param event_class Event class.
 *
 * Returns a field on success (a structure), NULL on error.
 */
extern struct bt_ctf_field *bt_ctf_event_get_stream_event_context(
		struct bt_ctf_event *event);

/*
 * bt_ctf_event_set_stream_event_context: Set an event's stream event context
 *
 * @param event Event.
 * @param context Event stream context field (must match the stream class'
 * 	stream event context type).
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_event_set_stream_event_context(struct bt_ctf_event *event,
		struct bt_ctf_field *context);

extern struct bt_ctf_packet *bt_ctf_event_get_packet(
		struct bt_ctf_event *event);

extern int bt_ctf_event_set_packet(struct bt_ctf_event *event,
		struct bt_ctf_packet *packet);

extern uint64_t bt_ctf_event_get_clock_value(struct bt_ctf_event *event,
		struct bt_ctf_clock *clock);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_IR_EVENT_H */
