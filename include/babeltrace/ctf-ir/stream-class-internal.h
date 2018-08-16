#ifndef BABELTRACE_CTF_IR_STREAM_CLASS_INTERNAL_H
#define BABELTRACE_CTF_IR_STREAM_CLASS_INTERNAL_H

/*
 * BabelTrace - CTF IR: Stream class internal
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

#include <babeltrace/assert-internal.h>
#include <babeltrace/common-internal.h>
#include <babeltrace/ctf-ir/validation-internal.h>
#include <babeltrace/ctf-ir/field-types-internal.h>
#include <babeltrace/ctf-ir/utils-internal.h>
#include <babeltrace/ctf-ir/visitor.h>
#include <babeltrace/ctf-ir/stream-class.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/object-pool-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <glib.h>
#include <inttypes.h>

struct bt_stream_class {
	struct bt_object base;
	GString *name;

	/* Array of pointers to event class addresses */
	GPtrArray *event_classes;

	/* event class id (int64_t) to event class address */
	GHashTable *event_classes_ht;
	int id_set;
	int64_t id;
	int64_t next_event_id;
	struct bt_field_type *packet_context_field_type;
	struct bt_field_type *event_header_field_type;
	struct bt_field_type *event_context_field_type;
	int frozen;
	int byte_order;

	/*
	 * This flag indicates if the stream class is valid. A valid
	 * stream class is _always_ frozen.
	 */
	int valid;

	/*
	 * Unique clock class mapped to any field type within this
	 * stream class, including all the stream class's event class
	 * field types. This is only set if the stream class is frozen.
	 *
	 * If the stream class is frozen and this is still NULL, it is
	 * still possible that it becomes non-NULL because
	 * bt_stream_class_add_event_class() can add an event class
	 * containing a field type mapped to some clock class. In this
	 * case, this is the mapped clock class, and at this point, both
	 * the new event class and the stream class are frozen, so the
	 * next added event classes are expected to contain field types
	 * which only map to this specific clock class.
	 *
	 * If this is a CTF writer stream class, then this is the
	 * backing clock class of the `clock` member above.
	 */
	struct bt_clock_class *clock_class;

	/* Pool of `struct bt_field_wrapper *` */
	struct bt_object_pool event_header_field_pool;

	/* Pool of `struct bt_field_wrapper *` */
	struct bt_object_pool packet_context_field_pool;
};

struct bt_event_class;

BT_HIDDEN
void bt_stream_class_freeze(struct bt_stream_class *stream_class);

BT_HIDDEN
int bt_stream_class_validate_single_clock_class(
		struct bt_stream_class *stream_class,
		struct bt_clock_class **expected_clock_class);

BT_HIDDEN
int bt_stream_class_visit(struct bt_stream_class *stream_class,
		bt_visitor visitor, void *data);

static inline
void _bt_stream_class_set_id(
		struct bt_stream_class *stream_class, int64_t id)
{
	BT_ASSERT(stream_class);
	stream_class->id = id;
	stream_class->id_set = 1;
	BT_LOGV("Set stream class's ID (internal): "
		"addr=%p, name=\"%s\", id=%" PRId64,
		stream_class, bt_stream_class_get_name(stream_class),
		bt_stream_class_get_id(stream_class));
}

static inline
int bt_stream_class_set_id_no_check(
		struct bt_stream_class *stream_class, int64_t id)
{
	_bt_stream_class_set_id(stream_class, id);
	return 0;
}

#endif /* BABELTRACE_CTF_IR_STREAM_CLASS_INTERNAL_H */
