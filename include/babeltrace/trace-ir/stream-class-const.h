#ifndef BABELTRACE_TRACE_IR_STREAM_CLASS_CONST_H
#define BABELTRACE_TRACE_IR_STREAM_CLASS_CONST_H

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

#include <stdint.h>

/*
 * For bt_bool, bt_trace_class, bt_stream_class, bt_event_class,
 * bt_stream_class
 */
#include <babeltrace/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum bt_stream_class_status {
	BT_STREAM_CLASS_STATUS_OK = 0,
	BT_STREAM_CLASS_STATUS_NOMEM = -12,
} bt_stream_class_status;

extern const bt_trace_class *bt_stream_class_borrow_trace_class_const(
		const bt_stream_class *stream_class);

extern const char *bt_stream_class_get_name(
		const bt_stream_class *stream_class);

extern bt_bool bt_stream_class_assigns_automatic_event_class_id(
		const bt_stream_class *stream_class);

extern bt_bool bt_stream_class_assigns_automatic_stream_id(
		const bt_stream_class *stream_class);

extern uint64_t bt_stream_class_get_id(
		const bt_stream_class *stream_class);

extern const bt_field_class *
bt_stream_class_borrow_packet_context_field_class_const(
		const bt_stream_class *stream_class);

extern const bt_field_class *
bt_stream_class_borrow_event_common_context_field_class_const(
		const bt_stream_class *stream_class);

extern uint64_t bt_stream_class_get_event_class_count(
		const bt_stream_class *stream_class);

extern const bt_event_class *
bt_stream_class_borrow_event_class_by_index_const(
		const bt_stream_class *stream_class, uint64_t index);

extern const bt_event_class *
bt_stream_class_borrow_event_class_by_id_const(
		const bt_stream_class *stream_class, uint64_t id);

extern const bt_clock_class *
bt_stream_class_borrow_default_clock_class_const(
		const bt_stream_class *stream_class);

extern bt_bool bt_stream_class_default_clock_is_always_known(
		const bt_stream_class *stream_class);

extern void bt_stream_class_get_ref(const bt_stream_class *stream_class);

extern void bt_stream_class_put_ref(const bt_stream_class *stream_class);

#define BT_STREAM_CLASS_PUT_REF_AND_RESET(_var)		\
	do {						\
		bt_stream_class_put_ref(_var);		\
		(_var) = NULL;				\
	} while (0)

#define BT_STREAM_CLASS_MOVE_REF(_var_dst, _var_src)	\
	do {						\
		bt_stream_class_put_ref(_var_dst);	\
		(_var_dst) = (_var_src);		\
		(_var_src) = NULL;			\
	} while (0)

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_TRACE_IR_STREAM_CLASS_CONST_H */
