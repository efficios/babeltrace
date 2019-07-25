#ifndef BABELTRACE2_TRACE_IR_TRACE_CLASS_CONST_H
#define BABELTRACE2_TRACE_IR_TRACE_CLASS_CONST_H

/*
 * Copyright (c) 2010-2019 EfficiOS Inc. and Linux Foundation
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

#ifndef __BT_IN_BABELTRACE_H
# error "Please include <babeltrace2/babeltrace.h> instead."
#endif

#include <stdint.h>

#include <babeltrace2/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (* bt_trace_class_destruction_listener_func)(
		const bt_trace_class *trace_class, void *data);

extern bt_bool bt_trace_class_assigns_automatic_stream_class_id(
		const bt_trace_class *trace_class);

extern uint64_t bt_trace_class_get_stream_class_count(
		const bt_trace_class *trace_class);

extern const bt_stream_class *
bt_trace_class_borrow_stream_class_by_index_const(
		const bt_trace_class *trace_class, uint64_t index);

extern const bt_stream_class *bt_trace_class_borrow_stream_class_by_id_const(
		const bt_trace_class *trace_class, uint64_t id);

typedef enum bt_trace_class_add_listener_status {
	BT_TRACE_CLASS_ADD_LISTENER_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
	BT_TRACE_CLASS_ADD_LISTENER_STATUS_OK		= __BT_FUNC_STATUS_OK,
} bt_trace_class_add_listener_status;

extern bt_trace_class_add_listener_status
bt_trace_class_add_destruction_listener(
        const bt_trace_class *trace_class,
        bt_trace_class_destruction_listener_func listener,
        void *data, bt_listener_id *listener_id);

typedef enum bt_trace_class_remove_listener_status {
	BT_TRACE_CLASS_REMOVE_LISTENER_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
	BT_TRACE_CLASS_REMOVE_LISTENER_STATUS_OK		= __BT_FUNC_STATUS_OK,
} bt_trace_class_remove_listener_status;

extern bt_trace_class_remove_listener_status
bt_trace_class_remove_destruction_listener(
        const bt_trace_class *trace_class, bt_listener_id listener_id);

extern void bt_trace_class_get_ref(const bt_trace_class *trace_class);

extern void bt_trace_class_put_ref(const bt_trace_class *trace_class);

#define BT_TRACE_CLASS_PUT_REF_AND_RESET(_var)		\
	do {						\
		bt_trace_class_put_ref(_var);		\
		(_var) = NULL;				\
	} while (0)

#define BT_TRACE_CLASS_MOVE_REF(_var_dst, _var_src)	\
	do {						\
		bt_trace_class_put_ref(_var_dst);	\
		(_var_dst) = (_var_src);		\
		(_var_src) = NULL;			\
	} while (0)

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_TRACE_IR_TRACE_CLASS_CONST_H */
