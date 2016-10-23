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
 * bt_ctf_event_class_get_field_by_name: Get an event class's field by name
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

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_IR_EVENT_CLASS_H */
