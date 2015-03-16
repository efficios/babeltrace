#ifndef BABELTRACE_CTF_IR_TRACE_H
#define BABELTRACE_CTF_IR_TRACE_H

/*
 * BabelTrace - CTF IR: Trace
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
 *
 * The Common Trace Format (CTF) Specification is available at
 * http://www.efficios.com/ctf
 */

#include <babeltrace/ctf-ir/event-types.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_ctf_trace;
struct bt_ctf_stream;
struct bt_ctf_stream_class;
struct bt_ctf_clock;

enum bt_environment_field_type {
	BT_ENVIRONMENT_FIELD_TYPE_UNKNOWN = -1,
	BT_ENVIRONMENT_FIELD_TYPE_STRING = 0,
	BT_ENVIRONMENT_FIELD_TYPE_INTEGER = 1,
};

/*
 * bt_ctf_trace_create: create a trace instance.
 *
 * Allocate a new trace.
 *
 * A trace's default packet header is a structure initialized with the following
 * fields:
 *	- uint32_t magic
 *	- uint8_t  uuid[16]
 *	- uint32_t stream_id
 *
 * Returns a new trace on success, NULL on error.
 */
extern struct bt_ctf_trace *bt_ctf_trace_create(void);

/*
 * bt_ctf_trace_create_stream: create a stream instance.
 *
 * Allocate a new stream instance and register it to the trace. The creation of
 * a stream sets its reference count to 1.
 *
 * @param trace Trace instance.
 * @param stream_class Stream class to instantiate.
 *
 * Returns a new stream on success, NULL on error.
 */
extern struct bt_ctf_stream *bt_ctf_trace_create_stream(
		struct bt_ctf_trace *trace,
		struct bt_ctf_stream_class *stream_class);

/*
 * bt_ctf_trace_add_environment_field: add a string environment field to the
 *	trace.
 *
 * Add a string environment field to the trace. The name and value parameters
 * are copied.
 *
 * @param trace Trace instance.
 * @param name Name of the environment field (will be copied).
 * @param value Value of the environment field (will be copied).
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_trace_add_environment_field(struct bt_ctf_trace *trace,
		const char *name,
		const char *value);

/*
 * bt_ctf_trace_add_environment_field_integer: add an integer environment
 *	field to the trace.
 *
 * Add an integer environment field to the trace. The name parameter is
 * copied.
 *
 * @param trace Trace instance.
 * @param name Name of the environment field (will be copied).
 * @param value Value of the environment field.
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_trace_add_environment_field_integer(
		struct bt_ctf_trace *trace, const char *name,
		int64_t value);

/*
 * bt_ctf_trace_get_environment_field_count: get environment field count.
 *
 * Get the trace's environment field count.
 *
 * @param trace Trace instance.
 *
 * Returns the environment field count, a negative value on error.
 */
extern int bt_ctf_trace_get_environment_field_count(
		struct bt_ctf_trace *trace);

/*
 * bt_ctf_trace_get_environment_field_type: get environment field type.
 *
 * Get an environment field's type.
 *
 * @param trace Trace instance.
 * @param index Index of the environment field.
 *
 * Returns the environment field count, a negative value on error.
 */
extern enum bt_environment_field_type
bt_ctf_trace_get_environment_field_type(struct bt_ctf_trace *trace,
		int index);

/*
 * bt_ctf_trace_get_environment_field_name: get environment field name.
 *
 * Get an environment field's name. The string's ownership is not
 * transferred to the caller.
 *
 * @param trace Trace instance.
 * @param index Index of the environment field.
 *
 * Returns the environment field's name, NULL on error.
 */
extern const char *
bt_ctf_trace_get_environment_field_name(struct bt_ctf_trace *trace,
		int index);

/*
 * bt_ctf_trace_get_environment_field_value_string: get environment field
 *	string value.
 *
 * Get an environment field's string value. The string's ownership is not
 * transferred to the caller.
 *
 * @param trace Trace instance.
 * @param index Index of the environment field.
 *
 * Returns the environment field's string value, NULL on error.
 */
extern const char *
bt_ctf_trace_get_environment_field_value_string(struct bt_ctf_trace *trace,
		int index);

/*
 * bt_ctf_trace_get_environment_field_value_integer: get environment field
 *      integer value.
 *
 * Get an environment field's integer value.
 *
 * @param trace Trace instance.
 * @param index Index of the environment field.
 *
 * Returns the environment field's integer value, a negative value on error.
 */
extern int
bt_ctf_trace_get_environment_field_value_integer(struct bt_ctf_trace *trace,
		int index, int64_t *value);

/*
 * bt_ctf_trace_add_clock: add a clock to the trace.
 *
 * Add a clock to the trace. Clocks assigned to stream classes must be
 * added to the trace beforehand.
 *
 * @param trace Trace instance.
 * @param clock Clock to add to the trace.
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_trace_add_clock(struct bt_ctf_trace *trace,
		struct bt_ctf_clock *clock);

/*
 * bt_ctf_trace_get_clock_count: get the number of clocks
 *	associated with the trace.
 *
 * @param trace Trace instance.
 *
 * Returns the clock count on success, a negative value on error.
 */
extern int bt_ctf_trace_get_clock_count(struct bt_ctf_trace *trace);

/*
 * bt_ctf_trace_get_clock: get a trace's clock at index.
 *
 * @param trace Trace instance.
 * @param index Index of the clock in the given trace.
 *
 * Return a clock instance on success, NULL on error.
 */
extern struct bt_ctf_clock *bt_ctf_trace_get_clock(
		struct bt_ctf_trace *trace, int index);

/*
 * bt_ctf_trace_add_stream_class: add a stream_class to the trace.
 *
 * Add a stream class to the trace.
 *
 * @param trace Trace instance.
 * @param stream_class Stream class to add to the trace.
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_trace_add_stream_class(struct bt_ctf_trace *trace,
		struct bt_ctf_stream_class *stream_class);

/*
 * bt_ctf_trace_get_stream_class_count: get the number of stream classes
 *	associated with the trace.
 *
 * @param trace Trace instance.
 *
 * Returns the stream class count on success, a negative value on error.
 */
extern int bt_ctf_trace_get_stream_class_count(struct bt_ctf_trace *trace);

/*
 * bt_ctf_trace_get_stream_class: get a trace's stream class at index.
 *
 * @param trace Trace instance.
 * @param index Index of the stream class in the given trace.
 *
 * Return a stream class on success, NULL on error.
 */
extern struct bt_ctf_stream_class *bt_ctf_trace_get_stream_class(
		struct bt_ctf_trace *trace, int index);

/*
 * bt_ctf_trace_get_clock_by_name: get a trace's clock by name
 *
 * @param trace	Trace instance.
 * @param name	Name of the clock in the given trace.
 *
 * Return a clock instance on success, NULL on error.
 */
extern struct bt_ctf_clock *bt_ctf_trace_get_clock_by_name(
        struct bt_ctf_trace *trace, const char *name);

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
extern char *bt_ctf_trace_get_metadata_string(struct bt_ctf_trace *trace);

/*
 * bt_ctf_trace_get_byte_order: get a trace's byte order.
 *
 * Get the trace's byte order.
 *
 * @param trace Trace instance.
 *
 * Returns the trace's endianness, BT_CTF_BYTE_ORDER_UNKNOWN on error.
 */
extern enum bt_ctf_byte_order bt_ctf_trace_get_byte_order(
		struct bt_ctf_trace *trace);

/*
 * bt_ctf_trace_set_byte_order: set a trace's byte order.
 *
 * Set the trace's byte order. Defaults to the current host's endianness.
 *
 * @param trace Trace instance.
 * @param byte_order Trace's byte order.
 *
 * Returns 0 on success, a negative value on error.
 *
 * Note: byte_order must not be BT_CTF_BYTE_ORDER_NATIVE since, according
 * to the CTF specification, is defined as "the byte order described in the
 * trace description".
 */
extern int bt_ctf_trace_set_byte_order(struct bt_ctf_trace *trace,
		enum bt_ctf_byte_order byte_order);

/*
 * bt_ctf_trace_get_packet_header_type: get a trace's packet header type.
 *
 * Get the trace's packet header type.
 *
 * @param trace Trace instance.
 *
 * Returns the trace's packet header type (a structure) on success, NULL on
 * 	error.
 */
extern struct bt_ctf_field_type *bt_ctf_trace_get_packet_header_type(
		struct bt_ctf_trace *trace);

/*
 * bt_ctf_trace_set_packet_header_type: set a trace's packet header type.
 *
 * Set the trace's packet header type.
 *
 * @param trace Trace instance.
 * @param packet_header_type Packet header field type (must be a structure).
 *
 * Returns 0 on success, a negative value on error.
 */
extern int bt_ctf_trace_set_packet_header_type(struct bt_ctf_trace *trace,
		struct bt_ctf_field_type *packet_header_type);

/*
 * bt_ctf_trace_get and bt_ctf_trace_put: increment and decrement the
 * trace's reference count.
 *
 * These functions ensure that the trace won't be destroyed while it
 * is in use. The same number of get and put (plus one extra put to
 * release the initial reference done at creation) have to be done to
 * destroy a trace.
 *
 * When the trace's reference count is decremented to 0 by a
 * bt_ctf_trace_put, the trace is freed.
 *
 * @param trace Trace instance.
 */
extern void bt_ctf_trace_get(struct bt_ctf_trace *trace);
extern void bt_ctf_trace_put(struct bt_ctf_trace *trace);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_IR_TRACE_H */
