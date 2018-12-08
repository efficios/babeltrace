#ifndef BABELTRACE_TRACE_IR_TRACE_CLASS_H
#define BABELTRACE_TRACE_IR_TRACE_CLASS_H

/*
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

/* For bt_bool, bt_uuid, bt_trace_class, bt_stream_class, bt_field_class */
#include <babeltrace/types.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern bt_trace_class *bt_trace_class_create(void);

extern void bt_trace_class_set_assigns_automatic_stream_class_id(
		bt_trace_class *trace_class, bt_bool value);

extern int bt_trace_class_set_name(bt_trace_class *trace_class,
		const char *name);

extern void bt_trace_class_set_uuid(bt_trace_class *trace_class,
		bt_uuid uuid);

extern int bt_trace_class_set_environment_entry_integer(
		bt_trace_class *trace_class,
		const char *name, int64_t value);

extern int bt_trace_class_set_environment_entry_string(
		bt_trace_class *trace_class,
		const char *name, const char *value);

extern int bt_trace_class_set_packet_header_field_class(
		bt_trace_class *trace_class,
		bt_field_class *packet_header_field_class);

extern bt_stream_class *bt_trace_class_borrow_stream_class_by_index(
		bt_trace_class *trace_class, uint64_t index);

extern bt_stream_class *bt_trace_class_borrow_stream_class_by_id(
		bt_trace_class *trace_class, uint64_t id);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_TRACE_IR_TRACE_CLASS_H */
