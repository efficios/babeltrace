/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Philippe Proulx <pproulx@efficios.com>
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
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

%{
#include <babeltrace/ctf-ir/trace.h>
%}

/* Type */
struct bt_ctf_trace;

/* Functions */
struct bt_ctf_trace *bt_ctf_trace_create(void);
const char *bt_ctf_trace_get_name(struct bt_ctf_trace *trace);
int bt_ctf_trace_set_name(struct bt_ctf_trace *trace,
		const char *name);
enum bt_ctf_byte_order bt_ctf_trace_get_byte_order(
		struct bt_ctf_trace *trace);
int bt_ctf_trace_set_byte_order(struct bt_ctf_trace *trace,
		enum bt_ctf_byte_order byte_order);
int bt_ctf_trace_get_environment_field_count(
		struct bt_ctf_trace *trace);
const char *
bt_ctf_trace_get_environment_field_name(struct bt_ctf_trace *trace,
		int index);
struct bt_value *
bt_ctf_trace_get_environment_field_value(struct bt_ctf_trace *trace,
		int index);
struct bt_value *
bt_ctf_trace_get_environment_field_value_by_name(
		struct bt_ctf_trace *trace, const char *name);
int bt_ctf_trace_set_environment_field(
		struct bt_ctf_trace *trace, const char *name,
		struct bt_value *value);
struct bt_ctf_field_type *bt_ctf_trace_get_packet_header_type(
		struct bt_ctf_trace *trace);
int bt_ctf_trace_set_packet_header_type(struct bt_ctf_trace *trace,
		struct bt_ctf_field_type *packet_header_type);
int bt_ctf_trace_get_clock_class_count(struct bt_ctf_trace *trace);
struct bt_ctf_clock_class *bt_ctf_trace_get_clock_class(
		struct bt_ctf_trace *trace, int index);
struct bt_ctf_clock_class *bt_ctf_trace_get_clock_class_by_name(
		struct bt_ctf_trace *trace, const char *name);
int bt_ctf_trace_add_clock_class(struct bt_ctf_trace *trace,
		struct bt_ctf_clock_class *clock_class);
int bt_ctf_trace_get_stream_class_count(struct bt_ctf_trace *trace);
struct bt_ctf_stream_class *bt_ctf_trace_get_stream_class(
		struct bt_ctf_trace *trace, int index);
struct bt_ctf_stream_class *bt_ctf_trace_get_stream_class_by_id(
		struct bt_ctf_trace *trace, uint32_t id);
int bt_ctf_trace_add_stream_class(struct bt_ctf_trace *trace,
		struct bt_ctf_stream_class *stream_class);
