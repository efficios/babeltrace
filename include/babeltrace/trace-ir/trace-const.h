#ifndef BABELTRACE_TRACE_IR_TRACE_CONST_H
#define BABELTRACE_TRACE_IR_TRACE_CONST_H

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

/*
 * For bt_bool, bt_uuid, bt_trace, bt_stream, bt_stream_class,
 * bt_field_class, bt_value
 */
#include <babeltrace/types.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum bt_trace_status {
	BT_TRACE_STATUS_OK = 0,
	BT_TRACE_STATUS_NOMEM = -12,
} bt_trace_status;

typedef void (* bt_trace_is_static_listener_func)(const bt_trace *trace,
		void *data);

typedef void (* bt_trace_listener_removed_func)(const bt_trace *trace,
		void *data);

extern const bt_trace_class *bt_trace_borrow_class_const(
		const bt_trace *trace);

extern const char *bt_trace_get_name(const bt_trace *trace);

extern uint64_t bt_trace_get_stream_count(const bt_trace *trace);

extern const bt_stream *bt_trace_borrow_stream_by_index_const(
		const bt_trace *trace, uint64_t index);

extern const bt_stream *bt_trace_borrow_stream_by_id_const(
		const bt_trace *trace, uint64_t id);

extern bt_bool bt_trace_is_static(const bt_trace *trace);

extern bt_trace_status bt_trace_add_is_static_listener(
		const bt_trace *trace,
		bt_trace_is_static_listener_func listener,
		bt_trace_listener_removed_func listener_removed, void *data,
		uint64_t *listener_id);

extern bt_trace_status bt_trace_remove_is_static_listener(
		const bt_trace *trace, uint64_t listener_id);

extern void bt_trace_get_ref(const bt_trace *trace);

extern void bt_trace_put_ref(const bt_trace *trace);

#define BT_TRACE_PUT_REF_AND_RESET(_var)		\
	do {						\
		bt_trace_put_ref(_var);			\
		(_var) = NULL;				\
	} while (0)

#define BT_TRACE_MOVE_REF(_var_dst, _var_src)		\
	do {						\
		bt_trace_put_ref(_var_dst);		\
		(_var_dst) = (_var_src);		\
		(_var_src) = NULL;			\
	} while (0)

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_TRACE_IR_TRACE_CONST_H */
