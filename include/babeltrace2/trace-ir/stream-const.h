#ifndef BABELTRACE2_TRACE_IR_STREAM_CONST_H
#define BABELTRACE2_TRACE_IR_STREAM_CONST_H

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

extern const bt_value *bt_stream_borrow_user_attributes_const(
		const bt_stream *stream);

extern const bt_stream_class *bt_stream_borrow_class_const(
		const bt_stream *stream);

extern const bt_trace *bt_stream_borrow_trace_const(
		const bt_stream *stream);

extern const char *bt_stream_get_name(const bt_stream *stream);

extern uint64_t bt_stream_get_id(const bt_stream *stream);

extern void bt_stream_get_ref(const bt_stream *stream);

extern void bt_stream_put_ref(const bt_stream *stream);

#define BT_STREAM_PUT_REF_AND_RESET(_var)		\
	do {						\
		bt_stream_put_ref(_var);		\
		(_var) = NULL;				\
	} while (0)

#define BT_STREAM_MOVE_REF(_var_dst, _var_src)		\
	do {						\
		bt_stream_put_ref(_var_dst);		\
		(_var_dst) = (_var_src);		\
		(_var_src) = NULL;			\
	} while (0)

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_TRACE_IR_STREAM_CONST_H */
