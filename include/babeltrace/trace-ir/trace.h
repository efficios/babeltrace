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
struct bt_packet_header_field;

extern struct bt_trace *bt_trace_create(void);

extern void bt_trace_set_assigns_automatic_stream_class_id(
		struct bt_trace *trace, bt_bool value);

extern int bt_trace_set_name(struct bt_trace *trace, const char *name);

extern void bt_trace_set_uuid(struct bt_trace *trace, bt_uuid uuid);

extern int bt_trace_set_environment_entry_integer(struct bt_trace *trace,
		const char *name, int64_t value);

extern int bt_trace_set_environment_entry_string(struct bt_trace *trace,
		const char *name, const char *value);

extern int bt_trace_set_packet_header_field_class(
		struct bt_trace *trace,
		struct bt_field_class *packet_header_field_class);

extern struct bt_stream_class *bt_trace_borrow_stream_class_by_index(
		struct bt_trace *trace, uint64_t index);

extern struct bt_stream_class *bt_trace_borrow_stream_class_by_id(
		struct bt_trace *trace, uint64_t id);

extern struct bt_stream *bt_trace_borrow_stream_by_index(struct bt_trace *trace,
		uint64_t index);

extern struct bt_stream *bt_trace_borrow_stream_by_id(struct bt_trace *trace,
		uint64_t id);

extern int bt_trace_make_static(struct bt_trace *trace);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_TRACE_IR_TRACE_H */
