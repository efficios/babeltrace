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

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/ctf-ir/field-types.h>
#include <babeltrace/ctf-ir/fields.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/values.h>
#include <babeltrace/ctf-ir/trace-internal.h>
#include <babeltrace/ctf-ir/stream-class.h>
#include <babeltrace/ctf-ir/stream.h>
#include <babeltrace/ctf-ir/event-class.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/object-pool-internal.h>
#include <glib.h>

struct bt_event_class {
	struct bt_object base;
	struct bt_field_type *context_field_type;
	struct bt_field_type *payload_field_type;
	int frozen;

	/*
	 * This flag indicates if the event class is valid. A valid
	 * event class is _always_ frozen. However, an event class
	 * may be frozen, but not valid yet. This is okay, as long as
	 * no events are created out of this event class.
	 */
	int valid;

	/* Attributes */
	GString *name;
	int64_t id;
	int log_level;
	GString *emf_uri;

	/* Pool of `struct bt_event *` */
	struct bt_object_pool event_pool;
};

BT_HIDDEN
void bt_event_class_freeze(struct bt_event_class *event_class);

typedef struct bt_field_type *(*bt_field_type_structure_create_func)();

BT_HIDDEN
int bt_event_class_validate_single_clock_class(
		struct bt_event_class *event_class,
		struct bt_clock_class **expected_clock_class);

#endif /* BABELTRACE_CTF_IR_EVENT_CLASS_INTERNAL_H */
