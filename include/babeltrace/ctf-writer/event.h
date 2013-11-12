#ifndef BABELTRACE_CTF_WRITER_EVENT_H
#define BABELTRACE_CTF_WRITER_EVENT_H

/*
 * BabelTrace - CTF Writer: Event
 *
 * Copyright 2013 EfficiOS Inc.
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

#ifdef __cplusplus
extern "C" {
#endif

struct bt_ctf_event_class;
struct bt_ctf_event;
struct bt_ctf_field;
struct bt_ctf_field_type;

/*
 * bt_ctf_event_class_create: create an event class.
 *
 * Allocate a new event class of the given name. The creation of an event class
 * sets its reference count to 1.
 *
 * @param name Event class name (will be copied).
 *
 * Returns an allocated event class on success, NULL on error.
 */
extern struct bt_ctf_event_class *bt_ctf_event_class_create(const char *name);

/*
 * bt_ctf_event_class_add_field: add a field to an event class.
 *
 * Add a field of type "type" to the event class. The event class will share
 * type's ownership by increasing its reference count. The "name" will be
 * copied.
 *
 * @param event_class Event class.
 * @param type Field type to add to the event class.
 * @param name Name of the new field.
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_event_class_add_field(struct bt_ctf_event_class *event_class,
		struct bt_ctf_field_type *type,
		const char *name);

/*
 * bt_ctf_event_class__get and bt_ctf_event_class_put: increment and decrement
 * the event class' reference count.
 *
 * These functions ensure that the event class won't be destroyed while it
 * is in use. The same number of get and put (plus one extra put to
 * release the initial reference done at creation) have to be done to
 * destroy an event class.
 *
 * When the event class' reference count is decremented to 0 by a
 * bt_ctf_event_class_put, the event class is freed.
 *
 * @param event_class Event class.
 */
extern void bt_ctf_event_class_get(struct bt_ctf_event_class *event_class);
extern void bt_ctf_event_class_put(struct bt_ctf_event_class *event_class);

/*
 * bt_ctf_event_create: instanciate an event.
 *
 * Allocate a new event of the given event class. The creation of an event
 * sets its reference count to 1. Each instance shares the ownership of the
 * event class using its reference count.
 *
 * @param event_class Event class.
 *
 * Returns an allocated field type on success, NULL on error.
 */
extern struct bt_ctf_event *bt_ctf_event_create(
		struct bt_ctf_event_class *event_class);

/*
 * bt_ctf_event_set_payload: set an event's field.
 *
 * Set a manually allocated field as an event's payload. The event will share
 * the field's ownership by using its reference count.
 * bt_ctf_field_put() must be called on the returned value.
 *
 * @param event Event instance.
 * @param name Event field name.
 * @param value Instance of a field whose type corresponds to the event's field.
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_event_set_payload(struct bt_ctf_event *event,
		const char *name,
		struct bt_ctf_field *value);

/*
 * bt_ctf_event_get_payload: get an event's field.
 *
 * Returns the field matching "name". bt_ctf_field_put() must be called on the
 * returned value.
 *
 * @param event Event instance.
 * @param name Event field name.
 *
 * Returns a field instance on success, NULL on error.
 */
extern struct bt_ctf_field *bt_ctf_event_get_payload(struct bt_ctf_event *event,
		const char *name);

/*
 * bt_ctf_event_get and bt_ctf_event_put: increment and decrement
 * the event's reference count.
 *
 * These functions ensure that the event won't be destroyed while it
 * is in use. The same number of get and put (plus one extra put to
 * release the initial reference done at creation) have to be done to
 * destroy an event.
 *
 * When the event's reference count is decremented to 0 by a
 * bt_ctf_event_put, the event is freed.
 *
 * @param event Event instance.
 */
extern void bt_ctf_event_get(struct bt_ctf_event *event);
extern void bt_ctf_event_put(struct bt_ctf_event *event);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_WRITER_EVENT_H */
