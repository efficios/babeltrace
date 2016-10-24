#ifndef BABELTRACE_CTF_IR_EVENT_CLASS_INTERNAL_H
#define BABELTRACE_CTF_IR_EVENT_CLASS_INTERNAL_H

/*
 * Babeltrace - CTF IR: Event class internal
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

#include <babeltrace/ctf-ir/field-types.h>
#include <babeltrace/ctf-ir/fields.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/values.h>
#include <babeltrace/ctf/types.h>
#include <babeltrace/ctf-ir/stream-class.h>
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
BT_HIDDEN
int bt_ctf_event_class_set_attribute(
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
BT_HIDDEN
int bt_ctf_event_class_get_attribute_count(
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
BT_HIDDEN
const char *
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
BT_HIDDEN
struct bt_value *
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
BT_HIDDEN
struct bt_value *
bt_ctf_event_class_get_attribute_value_by_name(
		struct bt_ctf_event_class *event_class, const char *name);

/*
 * bt_ctf_event_class_get_context_type: Get an event class's context type
 *
 * @param event_class Event class.
 *
 * Returns a field type (a structure) on success, NULL on error.
 */
BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_event_class_get_context_type(
		struct bt_ctf_event_class *event_class);


/*
 * bt_ctf_event_class_get_field_count: Get an event class' field count.
 *
 * @param event_class Event class.
 *
 * Returns the event class' field count, a negative value on error.
 *
 * Note: Returns an error if the payload is not a structure.
 */
BT_HIDDEN
int bt_ctf_event_class_get_field_count(
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
BT_HIDDEN
int bt_ctf_event_class_get_field(struct bt_ctf_event_class *event_class,
		const char **field_name, struct bt_ctf_field_type **field_type,
		int index);

/*
 * bt_ctf_event_class_get_id: Get an event class' id.
 *
 * @param event_class Event class.
 *
 * Returns the event class' id, a negative value on error.
 */
BT_HIDDEN
int64_t bt_ctf_event_class_get_id(struct bt_ctf_event_class *event_class);

/*
 * bt_ctf_event_class_get_name: Get an event class' name.
 *
 * @param event_class Event class.
 *
 * Returns the event class' name, NULL on error.
 */
BT_HIDDEN
const char *bt_ctf_event_class_get_name(
		struct bt_ctf_event_class *event_class);

/*
 * bt_ctf_event_class_get_stream_class: Get an event class' stream class.
 *
 * @param event_class Event class.
 *
 * Returns the event class' stream class, NULL on error or if the event class
 * is not associated with a stream class.
 */
BT_HIDDEN
struct bt_ctf_stream_class *bt_ctf_event_class_get_stream_class(
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
BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_event_class_get_payload_type(
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
BT_HIDDEN
int bt_ctf_event_class_set_id(
		struct bt_ctf_event_class *event_class, uint32_t id);

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
BT_HIDDEN
int bt_ctf_event_class_set_payload_type(
		struct bt_ctf_event_class *event_class,
		struct bt_ctf_field_type *payload);

/*
 * bt_ctf_event_class_set_context_type: Set an event class's context type
 *
 * @param event_class Event class.
 * @param context Event context field type (must be a structure).
 *
 * Returns 0 on success, a negative value on error.
 */
BT_HIDDEN
int bt_ctf_event_class_set_context_type(
		struct bt_ctf_event_class *event_class,
		struct bt_ctf_field_type *context);

#endif /* BABELTRACE_CTF_IR_EVENT_CLASS_INTERNAL_H */
