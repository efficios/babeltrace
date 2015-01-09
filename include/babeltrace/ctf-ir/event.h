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
 * bt_ctf_event_class_create: create an event class.
 *
 * Allocate a new event class of the given name. The creation of an event class
 * sets its reference count to 1. A unique event id is automatically assigned
 * to the event class.
 *
 * @param name Event class name (will be copied).
 *
 * Returns an allocated event class on success, NULL on error.
 */
extern struct bt_ctf_event_class *bt_ctf_event_class_create(const char *name);

/*
 * bt_ctf_event_class_get_name: Get an event class' name.
 *
 * @param event_class Event class.
 *
 * Returns the event class' name, NULL on error.
 */
extern const char *bt_ctf_event_class_get_name(
		struct bt_ctf_event_class *event_class);

/*
 * bt_ctf_event_class_get_id: Get an event class' id.
 *
 * @param event_class Event class.
 *
 * Returns the event class' id, a negative value on error.
 */
extern int64_t bt_ctf_event_class_get_id(
		struct bt_ctf_event_class *event_class);

/*
 * bt_ctf_event_class_set_id: Set an event class' id.
 *
 * Set an event class' id. Must be unique stream-wise.
 * Note that event classes are already assigned a unique id when added to a
 * stream class if none was set explicitly.
 *
 * @param event_class Event class.
 * @param id Event class id.
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_event_class_set_id(
		struct bt_ctf_event_class *event_class, uint32_t id);

/*
 * bt_ctf_event_class_get_stream_class: Get an event class' stream class.
 *
 * @param event_class Event class.
 *
 * Returns the event class' stream class, NULL on error or if the event class
 * is not associated with a stream class.
 */
extern struct bt_ctf_stream_class *bt_ctf_event_class_get_stream_class(
		struct bt_ctf_event_class *event_class);

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
 * bt_ctf_event_class_get_field_count: Get an event class' field count.
 *
 * @param event_class Event class.
 *
 * Returns the event class' field count, a negative value on error.
 */
extern int bt_ctf_event_class_get_field_count(
		struct bt_ctf_event_class *event_class);

/*
 * bt_ctf_event_class_get_field: Get event class' field type and name by index.
 *
 * @param event_class Event class.
 * @param field_name Pointer to a const char* where the field's name will
 *	be returned.
 * @param field_type Pointer to a bt_ctf_field_type* where the field's type will
 *	be returned.
 * @param index Index of field.
 *
 * Returns 0 on success, a negative error on value.
 */
extern int bt_ctf_event_class_get_field(struct bt_ctf_event_class *event_class,
		const char **field_name, struct bt_ctf_field_type **field_type,
		int index);

/*
 * bt_ctf_event_class_get_field_type_by_name: Get an event class's field by name
 *
 * @param event_class Event class.
 * @param name Name of the field.
 *
 * Returns a field type on success, NULL on error.
 */
extern struct bt_ctf_field_type *bt_ctf_event_class_get_field_by_name(
		struct bt_ctf_event_class *event_class, const char *name);

/*
 * bt_ctf_event_class_get_context_type: Get an event class's context type
 *
 * @param event_class Event class.
 *
 * Returns a field type (a structure) on success, NULL on error.
 */
extern struct bt_ctf_field_type *bt_ctf_event_class_get_context_type(
		struct bt_ctf_event_class *event_class);

/*
 * bt_ctf_event_class_set_context_type: Set an event class's context type
 *
 * @param event_class Event class.
 * @param context Event context field type (must be a structure).
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_event_class_set_context_type(
		struct bt_ctf_event_class *event_class,
		struct bt_ctf_field_type *context);

/*
 * bt_ctf_event_class_get and bt_ctf_event_class_put: increment and decrement
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
 * bt_ctf_event_get_class: get an event's class.
 *
 * @param event Event.
 *
 * Returns the event's class, NULL on error.
 */
extern struct bt_ctf_event_class *bt_ctf_event_get_class(
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

#endif /* BABELTRACE_CTF_IR_EVENT_H */
