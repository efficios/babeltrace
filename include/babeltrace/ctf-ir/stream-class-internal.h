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

#include <babeltrace/ctf-writer/clock.h>
#include <babeltrace/ctf-writer/event-fields.h>
#include <babeltrace/ctf-writer/event-types.h>
#include <babeltrace/ctf-ir/trace.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/ctf-ir/trace-internal.h>
#include <assert.h>
#include <glib.h>

struct bt_ctf_stream_class {
	struct bt_object base;
	GString *name;
	struct bt_ctf_clock *clock;
	GPtrArray *event_classes; /* Array of pointers to bt_ctf_event_class */
	/* event class id (int64_t) to event class */
	GHashTable *event_classes_ht;
	int id_set;
	uint32_t id;
	uint32_t next_event_id;
	uint32_t next_stream_id;
	struct bt_ctf_field_type *packet_context_type;
	struct bt_ctf_field_type *event_header_type;
	struct bt_ctf_field_type *event_context_type;
	int frozen;
	int byte_order;

	/*
	 * This flag indicates if the stream class is valid. A valid
	 * stream class is _always_ frozen.
	 */
	int valid;
};

BT_HIDDEN
void bt_ctf_stream_class_freeze(struct bt_ctf_stream_class *stream_class);

BT_HIDDEN
int bt_ctf_stream_class_serialize(struct bt_ctf_stream_class *stream_class,
		struct metadata_context *context);

BT_HIDDEN
void bt_ctf_stream_class_set_byte_order(
		struct bt_ctf_stream_class *stream_class, int byte_order);

/* Set stream_class id without checking if the stream class is frozen */
BT_HIDDEN
int _bt_ctf_stream_class_set_id(struct bt_ctf_stream_class *stream_class,
		uint32_t id);

BT_HIDDEN
int bt_ctf_stream_class_set_id_no_check(
		struct bt_ctf_stream_class *stream_class, uint32_t id);

static inline
struct bt_ctf_trace *bt_ctf_stream_class_borrow_trace(
		struct bt_ctf_stream_class *stream_class)
{
	assert(stream_class);
	return (void *) bt_object_borrow_parent(stream_class);
}

#endif /* BABELTRACE_CTF_IR_STREAM_CLASS_INTERNAL_H */
