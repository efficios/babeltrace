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

#include <babeltrace/ctf-ir/trace.h>
#include <babeltrace/ctf-ir/field-types.h>
#include <babeltrace/ctf-ir/fields.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/values.h>
#include <glib.h>
#include <sys/types.h>
#include <uuid/uuid.h>

enum field_type_alias {
	FIELD_TYPE_ALIAS_UINT5_T = 0,
	FIELD_TYPE_ALIAS_UINT8_T,
	FIELD_TYPE_ALIAS_UINT16_T,
	FIELD_TYPE_ALIAS_UINT27_T,
	FIELD_TYPE_ALIAS_UINT32_T,
	FIELD_TYPE_ALIAS_UINT64_T,
	NR_FIELD_TYPE_ALIAS,
};

struct bt_ctf_trace {
	struct bt_object base;
	GString *name;
	int frozen;
	uuid_t uuid;
	bool uuid_set;
	enum bt_ctf_byte_order native_byte_order;
	struct bt_value *environment;
	GPtrArray *clocks; /* Array of pointers to bt_ctf_clock_class */
	GPtrArray *stream_classes; /* Array of ptrs to bt_ctf_stream_class */
	GPtrArray *streams; /* Array of ptrs to bt_ctf_stream */
	struct bt_ctf_field_type *packet_header_type;
	int64_t next_stream_id;
	int is_created_by_writer;

	/*
	 * This flag indicates if the trace is valid. A valid
	 * trace is _always_ frozen.
	 */
	int valid;
	GPtrArray *listeners; /* Array of struct listener_wrapper */
	bool is_static;
};

struct metadata_context {
	GString *string;
	GString *field_name;
	unsigned int current_indentation_level;
};

BT_HIDDEN
const char *get_byte_order_string(int byte_order);

BT_HIDDEN
struct bt_ctf_field_type *get_field_type(enum field_type_alias alias);

BT_HIDDEN
int bt_ctf_trace_object_modification(struct bt_ctf_object *object,
		void *trace_ptr);

BT_HIDDEN
bool bt_ctf_trace_has_clock_class(struct bt_ctf_trace *trace,
		struct bt_ctf_clock_class *clock_class);

/**
@brief	User function type to use with bt_ctf_trace_add_listener().

@param[in] obj	New CTF IR object which is part of the trace
		class hierarchy.
@param[in] data	User data.

@prenotnull{obj}
*/
typedef void (*bt_ctf_listener_cb)(struct bt_ctf_object *obj, void *data);

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
int bt_ctf_trace_add_listener(struct bt_ctf_trace *trace_class,
		bt_ctf_listener_cb listener, void *data);

/*
 * bt_ctf_trace_get_metadata_string: get metadata string.
 *
 * Get the trace's TSDL metadata. The caller assumes the ownership of the
 * returned string.
 *
 * @param trace Trace instance.
 *
 * Returns the metadata string on success, NULL on error.
 */
BT_HIDDEN
char *bt_ctf_trace_get_metadata_string(struct bt_ctf_trace *trace);

#endif /* BABELTRACE_CTF_IR_TRACE_INTERNAL_H */
