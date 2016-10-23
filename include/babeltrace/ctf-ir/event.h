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

#ifdef __cplusplus
extern "C" {
#endif

struct bt_ctf_event_class;
struct bt_ctf_event;
struct bt_ctf_field;
struct bt_ctf_field_type;
struct bt_ctf_stream_class;

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
 * bt_ctf_event_get_payload: get an event's field.
 *
 * Returns the field matching "name". bt_ctf_field_put() must be called on the
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
 * bt_ctf_field_put() must be called on the returned value.
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

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_IR_EVENT_H */
