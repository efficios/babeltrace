#ifndef BABELTRACE_CTF_IR_TRACE_INTERNAL_H
#define BABELTRACE_CTF_IR_TRACE_INTERNAL_H

/*
 * BabelTrace - CTF IR: Trace internal
 *
 * Copyright 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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
#include <babeltrace/ctf-ir/trace.h>
#include <babeltrace/ctf-ir/stream-class-internal.h>
#include <babeltrace/ctf-ir/field-types.h>
#include <babeltrace/ctf-ir/fields.h>
#include <babeltrace/ctf-ir/validation-internal.h>
#include <babeltrace/ctf-ir/attributes-internal.h>
#include <babeltrace/ctf-ir/clock-class-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/object-pool-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/values.h>
#include <babeltrace/types.h>
#include <glib.h>
#include <sys/types.h>
#include <babeltrace/compat/uuid-internal.h>

struct bt_trace {
	struct bt_object base;
	GString *name;
	int frozen;
	unsigned char uuid[BABELTRACE_UUID_LEN];
	bt_bool uuid_set;
	enum bt_byte_order native_byte_order;
	struct bt_value *environment;
	GPtrArray *clock_classes; /* Array of pointers to bt_clock_class */
	GPtrArray *stream_classes; /* Array of ptrs to bt_stream_class */
	GPtrArray *streams; /* Array of ptrs to bt_stream */
	struct bt_field_type *packet_header_field_type;
	int64_t next_stream_id;

	/*
	 * This flag indicates if the trace is valid. A valid
	 * trace is _always_ frozen.
	 */
	int valid;

	GPtrArray *listeners; /* Array of struct listener_wrapper */
	GArray *is_static_listeners;
	bt_bool is_static;
	bt_bool in_remove_listener;

	/* Pool of `struct bt_field_wrapper *` */
	struct bt_object_pool packet_header_field_pool;
};

BT_HIDDEN
int bt_trace_object_modification(struct bt_visitor_object *object,
		void *trace_ptr);

BT_HIDDEN
bt_bool bt_trace_has_clock_class(struct bt_trace *trace,
		struct bt_clock_class *clock_class);

/**
@brief	User function type to use with bt_trace_add_listener().

@param[in] obj	New CTF IR object which is part of the trace
		class hierarchy.
@param[in] data	User data.

@prenotnull{obj}
*/
typedef void (*bt_listener_cb)(struct bt_visitor_object *obj, void *data);

/**
@brief	Adds the trace class modification listener \p listener to
	the CTF IR trace class \p trace_class.

Once you add \p listener to \p trace_class, whenever \p trace_class
is modified, \p listener is called with the new element and with
\p data (user data).

@param[in] trace_class	Trace class to which to add \p listener.
@param[in] listener	Modification listener function.
@param[in] data		User data.
@returns		0 on success, or a negative value on error.

@prenotnull{trace_class}
@prenotnull{listener}
@postrefcountsame{trace_class}
*/
BT_HIDDEN
int bt_trace_add_listener(struct bt_trace *trace_class,
		bt_listener_cb listener, void *data);

static inline
void bt_trace_freeze(struct bt_trace *trace)
{
	int i;

	if (trace->frozen) {
		return;
	}

	BT_LOGD("Freezing trace: addr=%p, name=\"%s\"",
		trace, bt_trace_get_name(trace));
	BT_LOGD_STR("Freezing packet header field type.");
	bt_field_type_freeze(trace->packet_header_field_type);
	BT_LOGD_STR("Freezing environment attributes.");
	bt_attributes_freeze(trace->environment);

	if (trace->clock_classes->len > 0) {
		BT_LOGD_STR("Freezing clock classes.");
	}

	for (i = 0; i < trace->clock_classes->len; i++) {
		struct bt_clock_class *clock_class =
			g_ptr_array_index(trace->clock_classes, i);

		bt_clock_class_freeze(clock_class);
	}

	trace->frozen = 1;
}

#endif /* BABELTRACE_CTF_IR_TRACE_INTERNAL_H */
