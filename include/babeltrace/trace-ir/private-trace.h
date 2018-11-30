#ifndef BABELTRACE_TRACE_IR_PRIVATE_TRACE_H
#define BABELTRACE_TRACE_IR_PRIVATE_TRACE_H

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
struct bt_private_trace;
struct bt_private_stream;
struct bt_private_stream_class;
struct bt_private_field_class;
struct bt_private_value;
struct bt_private_packet_header_field;

typedef void (* bt_private_trace_is_static_listener)(
	struct bt_private_trace *trace, void *data);

typedef void (* bt_private_trace_listener_removed)(
	struct bt_private_trace *trace, void *data);

static inline
struct bt_trace *bt_private_trace_borrow_trace(
		struct bt_private_trace *priv_trace)
{
	return (void *) priv_trace;
}

extern struct bt_private_trace *bt_private_trace_create(void);

extern void bt_private_trace_set_assigns_automatic_stream_class_id(
		struct bt_private_trace *trace, bt_bool value);

extern int bt_private_trace_set_name(struct bt_private_trace *trace,
		const char *name);

extern void bt_private_trace_set_uuid(struct bt_private_trace *trace,
		bt_uuid uuid);

extern void bt_private_trace_borrow_environment_entry_by_index(
		struct bt_private_trace *trace, uint64_t index,
		const char **name, struct bt_private_value **value);

extern struct bt_private_value *
bt_private_trace_borrow_environment_entry_value_by_name(
		struct bt_private_trace *trace, const char *name);

extern int bt_private_trace_set_environment_entry_integer(
		struct bt_private_trace *trace, const char *name,
		int64_t value);

extern int bt_private_trace_set_environment_entry_string(
		struct bt_private_trace *trace, const char *name,
		const char *value);

extern struct bt_private_field_class *
bt_private_trace_borrow_packet_header_field_class(
		struct bt_private_trace *trace);

extern int bt_private_trace_set_packet_header_field_class(
		struct bt_private_trace *trace,
		struct bt_private_field_class *packet_header_field_class);

extern struct bt_private_stream_class *
bt_private_trace_borrow_stream_class_by_index(
		struct bt_private_trace *trace, uint64_t index);

extern struct bt_private_stream_class *
bt_private_trace_borrow_stream_class_by_id(
		struct bt_private_trace *trace, uint64_t id);

extern struct bt_private_stream *
bt_private_trace_borrow_stream_by_index(
		struct bt_private_trace *trace, uint64_t index);

extern struct bt_private_stream *bt_private_trace_borrow_stream_by_id(
		struct bt_private_trace *trace, uint64_t id);

extern int bt_private_trace_make_static(struct bt_private_trace *trace);

extern int bt_private_trace_add_is_static_listener(
		struct bt_private_trace *trace,
		bt_private_trace_is_static_listener listener,
		bt_private_trace_listener_removed listener_removed, void *data,
		uint64_t *listener_id);

extern int bt_private_trace_remove_is_static_listener(
		struct bt_private_trace *trace, uint64_t listener_id);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_TRACE_IR_PRIVATE_TRACE_H */
