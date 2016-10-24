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

#include <babeltrace/ctf-ir/field-types.h>
#include <babeltrace/values.h>
#include <babeltrace/babeltrace-internal.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_ctf_trace;
struct bt_ctf_stream;
struct bt_ctf_stream_class;
struct bt_ctf_clock;

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
BT_HIDDEN
struct bt_ctf_trace *bt_ctf_trace_create(void);

/*
 * bt_ctf_trace_set_environment_field: sets an environment field to the
 *	trace.
 *
 * Sets an environment field to the trace. The name parameter is
 * copied, whereas the value parameter's reference count is incremented
 * (if the function succeeds).
 *
 * If a value exists in the environment for the specified name, it is
 * replaced by the new value.
 *
 * The value parameter _must_ be either an integer value object or a
 * string value object. Other object types are not supported.
 *
 * @param trace Trace instance.
 * @param name Name of the environment field (will be copied).
 * @param value Value of the environment field.
 *
 * Returns 0 on success, a negative value on error.
 */
BT_HIDDEN
int bt_ctf_trace_set_environment_field(
		struct bt_ctf_trace *trace, const char *name,
		struct bt_value *value);

/*
 * bt_ctf_trace_set_environment_field_string: sets a string environment
 *	field to the trace.
 *
 * Sets a string environment field to the trace. This is a helper
 * function which corresponds to calling
 * bt_ctf_trace_set_environment_field() with a string object.
 *
 * @param trace Trace instance.
 * @param name Name of the environment field (will be copied).
 * @param value Value of the environment field (will be copied).
 *
 * Returns 0 on success, a negative value on error.
 */
BT_HIDDEN
int bt_ctf_trace_set_environment_field_string(
		struct bt_ctf_trace *trace, const char *name,
		const char *value);

/*
 * bt_ctf_trace_set_environment_field_integer: sets an integer environment
 *	field to the trace.
 *
 * Sets an integer environment field to the trace. This is a helper
 * function which corresponds to calling
 * bt_ctf_trace_set_environment_field() with an integer object.
 *
 * @param trace Trace instance.
 * @param name Name of the environment field (will be copied).
 * @param value Value of the environment field.
 *
 * Returns 0 on success, a negative value on error.
 */
BT_HIDDEN
int bt_ctf_trace_set_environment_field_integer(
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
BT_HIDDEN
int bt_ctf_trace_get_environment_field_count(
		struct bt_ctf_trace *trace);

/*
 * bt_ctf_trace_get_environment_field_name: get environment field name.
 *
 * Get an environment field's name. The string's ownership is not
 * transferred to the caller. The string data is valid as long as
 * this trace's environment is not modified.
 *
 * @param trace Trace instance.
 * @param index Index of the environment field.
 *
 * Returns the environment field's name, NULL on error.
 */
BT_HIDDEN
const char *
bt_ctf_trace_get_environment_field_name(struct bt_ctf_trace *trace,
		int index);

/*
 * bt_ctf_trace_get_environment_field_value: get environment
 *	field value (an object).
 *
 * Get an environment field's value (an object). The returned object's
 * reference count is incremented. When done with the object, the caller
 * must call bt_value_put() on it.
 *
 * @param trace Trace instance.
 * @param index Index of the environment field.
 *
 * Returns the environment field's object value, NULL on error.
 */
BT_HIDDEN
struct bt_value *
bt_ctf_trace_get_environment_field_value(struct bt_ctf_trace *trace,
		int index);

/*
 * bt_ctf_trace_get_environment_field_value_by_name: get environment
 *	field value (an object) by name.
 *
 * Get an environment field's value (an object) by its field name. The
 * returned object's reference count is incremented. When done with the
 * object, the caller must call bt_value_put() on it.
 *
 * @param trace Trace instance.
 * @param name Environment field's name
 *
 * Returns the environment field's object value, NULL on error.
 */
BT_HIDDEN
struct bt_value *
bt_ctf_trace_get_environment_field_value_by_name(struct bt_ctf_trace *trace,
		const char *name);

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
BT_HIDDEN
int bt_ctf_trace_add_clock(struct bt_ctf_trace *trace,
		struct bt_ctf_clock *clock);

/*
 * bt_ctf_trace_get_clock_count: get the number of clocks
 *	associated with the trace.
 *
 * @param trace Trace instance.
 *
 * Returns the clock count on success, a negative value on error.
 */
BT_HIDDEN
int bt_ctf_trace_get_clock_count(struct bt_ctf_trace *trace);

/*
 * bt_ctf_trace_get_clock: get a trace's clock at index.
 *
 * @param trace Trace instance.
 * @param index Index of the clock in the given trace.
 *
 * Return a clock instance on success, NULL on error.
 */
BT_HIDDEN
struct bt_ctf_clock *bt_ctf_trace_get_clock(
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
BT_HIDDEN
int bt_ctf_trace_add_stream_class(struct bt_ctf_trace *trace,
		struct bt_ctf_stream_class *stream_class);

/*
 * bt_ctf_trace_get_stream_class_count: get the number of stream classes
 *	associated with the trace.
 *
 * @param trace Trace instance.
 *
 * Returns the stream class count on success, a negative value on error.
 */
BT_HIDDEN
int bt_ctf_trace_get_stream_class_count(struct bt_ctf_trace *trace);

/*
 * bt_ctf_trace_get_stream_class: get a trace's stream class at index.
 *
 * @param trace Trace instance.
 * @param index Index of the stream class in the given trace.
 *
 * Return a stream class on success, NULL on error.
 */
BT_HIDDEN
struct bt_ctf_stream_class *bt_ctf_trace_get_stream_class(
		struct bt_ctf_trace *trace, int index);

/*
 * bt_ctf_trace_get_stream_class_by_id: get a trace's stream class by ID.
 *
 * @param trace Trace instance.
 * @param index ID of the stream class in the given trace.
 *
 * Return a stream class on success, NULL on error.
 */
BT_HIDDEN
struct bt_ctf_stream_class *bt_ctf_trace_get_stream_class_by_id(
		struct bt_ctf_trace *trace, uint32_t id);

/*
 * bt_ctf_trace_get_clock_by_name: get a trace's clock by name
 *
 * @param trace Trace instance.
 * @param name Name of the clock in the given trace.
 *
 * Return a clock instance on success, NULL on error.
 */
BT_HIDDEN
struct bt_ctf_clock *bt_ctf_trace_get_clock_by_name(
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
BT_HIDDEN
char *bt_ctf_trace_get_metadata_string(struct bt_ctf_trace *trace);

/*
 * bt_ctf_trace_get_byte_order: get a trace's byte order.
 *
 * Get the trace's byte order.
 *
 * @param trace Trace instance.
 *
 * Returns the trace's endianness, BT_CTF_BYTE_ORDER_UNKNOWN on error.
 */
BT_HIDDEN
enum bt_ctf_byte_order bt_ctf_trace_get_byte_order(
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
BT_HIDDEN
int bt_ctf_trace_set_byte_order(struct bt_ctf_trace *trace,
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
BT_HIDDEN
struct bt_ctf_field_type *bt_ctf_trace_get_packet_header_type(
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
BT_HIDDEN
int bt_ctf_trace_set_packet_header_type(struct bt_ctf_trace *trace,
		struct bt_ctf_field_type *packet_header_type);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_IR_TRACE_H */
