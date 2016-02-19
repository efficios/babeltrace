#ifndef BABELTRACE_CTF_IR_EVENT_CLASS_H
#define BABELTRACE_CTF_IR_EVENT_CLASS_H

/*
 * BabelTrace - CTF IR: Event class
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
 * bt_ctf_event_class_set_attribute: sets an attribute to the event
 *	class.
 *
 * Sets an attribute to the event class. The name parameter is copied,
 * whereas the value parameter's reference count is incremented
 * (if the function succeeds).
 *
 * If an attribute exists in the event class for the specified name, it
 * is replaced by the new value.
 *
 * Valid attributes and object types are:
 *
 *   - "id":            integer object with a value >= 0
 *   - "name":          string object
 *   - "loglevel":      integer object with a value >= 0
 *   - "model.emf.uri": string object
 *
 * @param event_class Event class.
 * @param name Name of the attribute (will be copied).
 * @param value Value of the attribute.
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_event_class_set_attribute(
		struct bt_ctf_event_class *event_class, const char *name,
		struct bt_value *value);

/*
 * bt_ctf_event_class_get_attribute_count: get the number of attributes
 *	in this event class.
 *
 * Get the event class' attribute count.
 *
 * @param event_class Event class.
 *
 * Returns the attribute count, a negative value on error.
 */
extern int bt_ctf_event_class_get_attribute_count(
		struct bt_ctf_event_class *event_class);

/*
 * bt_ctf_event_class_get_attribute_name: get attribute name.
 *
 * Get an attribute's name. The string's ownership is not
 * transferred to the caller. The string data is valid as long as
 * this event class' attributes are not modified.
 *
 * @param event_class Event class.
 * @param index Index of the attribute.
 *
 * Returns the attribute's name, NULL on error.
 */
extern const char *
bt_ctf_event_class_get_attribute_name(
		struct bt_ctf_event_class *event_class, int index);

/*
 * bt_ctf_event_class_get_attribute_value: get attribute value (an object).
 *
 * Get an attribute's value (an object). The returned object's
 * reference count is incremented. When done with the object, the caller
 * must call bt_value_put() on it.
 *
 * @param event_class Event class.
 * @param index Index of the attribute.
 *
 * Returns the attribute's object value, NULL on error.
 */
extern struct bt_value *
bt_ctf_event_class_get_attribute_value(struct bt_ctf_event_class *event_class,
		int index);

/*
 * bt_ctf_event_class_get_attribute_value_by_name: get attribute
 *	value (an object) by name.
 *
 * Get an attribute's value (an object) by its name. The returned object's
 * reference count is incremented. When done with the object, the caller
 * must call bt_value_put() on it.
 *
 * @param event_class Event class.
 * @param name Attribute's name
 *
 * Returns the attribute's object value, NULL on error.
 */
extern struct bt_value *
bt_ctf_event_class_get_attribute_value_by_name(
		struct bt_ctf_event_class *event_class, const char *name);

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
 * bt_ctf_event_class_get_payload_type: get an event class' payload.
 *
 * Get an event class' payload type.
 *
 * @param event_class Event class.
 *
 * Returns the event class' payload, NULL on error.
 */
extern struct bt_ctf_field_type *bt_ctf_event_class_get_payload_type(
		struct bt_ctf_event_class *event_class);

/*
 * bt_ctf_event_class_set_payload_type: set an event class' payload.
 *
 * Set an event class' payload type.
 *
 * @param event_class Event class.
 * @param payload The payload's type (must be a structure).
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_event_class_set_payload_type(
		struct bt_ctf_event_class *event_class,
		struct bt_ctf_field_type *payload);

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
 *
 * Note: Returns an error if the payload is not a structure.
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
 *
 * Note: Returns an error if the payload is not a structure.
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
 *
 * Note: Returns an error if the payload is not a structure.
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
 *
 * Note: Returns an error if the payload is not a structure.
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

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_IR_EVENT_CLASS_H */
