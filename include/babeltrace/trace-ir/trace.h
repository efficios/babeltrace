#ifndef BABELTRACE_TRACE_IR_TRACE_H
#define BABELTRACE_TRACE_IR_TRACE_H

/*
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

/* For bt_bool, bt_uuid */
#include <babeltrace/types.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_trace;
struct bt_stream;
struct bt_stream_class;
struct bt_field_class;
struct bt_value;

extern bt_bool bt_trace_assigns_automatic_stream_class_id(
		struct bt_trace *trace);

extern const char *bt_trace_get_name(struct bt_trace *trace);

extern bt_uuid bt_trace_get_uuid(struct bt_trace *trace);

extern uint64_t bt_trace_get_environment_entry_count(struct bt_trace *trace);

extern void bt_trace_borrow_environment_entry_by_index(
		struct bt_trace *trace, uint64_t index,
		const char **name, const struct bt_value **value);

extern const struct bt_value *bt_trace_borrow_environment_entry_value_by_name(
		struct bt_trace *trace, const char *name);

extern struct bt_field_class *bt_trace_borrow_packet_header_field_class(
		struct bt_trace *trace);

extern uint64_t bt_trace_get_stream_class_count(struct bt_trace *trace);

extern struct bt_stream_class *bt_trace_borrow_stream_class_by_index(
		struct bt_trace *trace, uint64_t index);

extern struct bt_stream_class *bt_trace_borrow_stream_class_by_id(
		struct bt_trace *trace, uint64_t id);

extern uint64_t bt_trace_get_stream_count(struct bt_trace *trace);

extern struct bt_stream *bt_trace_borrow_stream_by_index(
		struct bt_trace *trace, uint64_t index);

extern struct bt_stream *bt_trace_borrow_stream_by_id(
		struct bt_trace *trace, uint64_t id);

extern bt_bool bt_trace_is_static(struct bt_trace *trace);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_TRACE_IR_TRACE_H */
