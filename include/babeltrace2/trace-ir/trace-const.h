#ifndef BABELTRACE2_TRACE_IR_TRACE_CONST_H
#define BABELTRACE2_TRACE_IR_TRACE_CONST_H

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

typedef void (* bt_trace_destruction_listener_func)(
		const bt_trace *trace, void *data);

extern const bt_trace_class *bt_trace_borrow_class_const(
		const bt_trace *trace);

extern const char *bt_trace_get_name(const bt_trace *trace);

extern bt_uuid bt_trace_get_uuid(const bt_trace *trace);

extern uint64_t bt_trace_get_environment_entry_count(const bt_trace *trace);

extern void bt_trace_borrow_environment_entry_by_index_const(
		const bt_trace *trace, uint64_t index,
		const char **name, const bt_value **value);

extern const bt_value *bt_trace_borrow_environment_entry_value_by_name_const(
		const bt_trace *trace, const char *name);

extern uint64_t bt_trace_get_stream_count(const bt_trace *trace);

extern const bt_stream *bt_trace_borrow_stream_by_index_const(
		const bt_trace *trace, uint64_t index);

extern const bt_stream *bt_trace_borrow_stream_by_id_const(
		const bt_trace *trace, uint64_t id);

typedef enum bt_trace_add_listener_status {
	BT_TRACE_ADD_LISTENER_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
	BT_TRACE_ADD_LISTENER_STATUS_OK			= __BT_FUNC_STATUS_OK,
} bt_trace_add_listener_status;

extern bt_trace_add_listener_status bt_trace_add_destruction_listener(
		const bt_trace *trace,
		bt_trace_destruction_listener_func listener,
		void *data, bt_listener_id *listener_id);

typedef enum bt_trace_remove_listener_status {
	BT_TRACE_REMOVE_LISTENER_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
	BT_TRACE_REMOVE_LISTENER_STATUS_OK		= __BT_FUNC_STATUS_OK,
} bt_trace_remove_listener_status;

extern bt_trace_remove_listener_status bt_trace_remove_destruction_listener(
		const bt_trace *trace, bt_listener_id listener_id);

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

#endif /* BABELTRACE2_TRACE_IR_TRACE_CONST_H */
