#ifndef BABELTRACE2_TRACE_IR_STREAM_H
#define BABELTRACE2_TRACE_IR_STREAM_H

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

extern bt_stream *bt_stream_create(bt_stream_class *stream_class,
		bt_trace *trace);

extern bt_stream *bt_stream_create_with_id(
		bt_stream_class *stream_class,
		bt_trace *trace, uint64_t id);

extern bt_trace *bt_stream_borrow_trace(bt_stream *stream);

extern bt_stream_class *bt_stream_borrow_class(bt_stream *stream);

typedef enum bt_stream_set_name_status {
	BT_STREAM_SET_NAME_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
	BT_STREAM_SET_NAME_STATUS_OK		= __BT_FUNC_STATUS_OK,
} bt_stream_set_name_status;

extern bt_stream_set_name_status bt_stream_set_name(bt_stream *stream,
		const char *name);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_TRACE_IR_STREAM_H */
