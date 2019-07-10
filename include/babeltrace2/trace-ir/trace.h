#ifndef BABELTRACE2_TRACE_IR_TRACE_H
#define BABELTRACE2_TRACE_IR_TRACE_H

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

#ifndef __BT_IN_BABELTRACE_H
# error "Please include <babeltrace2/babeltrace.h> instead."
#endif

#include <stdint.h>

/* For bt_bool, bt_uuid, bt_trace, bt_trace_class, bt_stream */
#include <babeltrace2/types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern bt_trace_class *bt_trace_borrow_class(bt_trace *trace);

extern bt_trace *bt_trace_create(bt_trace_class *trace_class);

typedef enum bt_trace_set_name_status {
	BT_TRACE_SET_NAME_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
	BT_TRACE_SET_NAME_STATUS_OK		= __BT_FUNC_STATUS_OK,
} bt_trace_set_name_status;

extern bt_trace_set_name_status bt_trace_set_name(bt_trace *trace,
		const char *name);

extern void bt_trace_set_uuid(bt_trace *trace, bt_uuid uuid);

typedef enum bt_trace_set_environment_entry_status {
	BT_TRACE_SET_ENVIRONMENT_ENTRY_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
	BT_TRACE_SET_ENVIRONMENT_ENTRY_STATUS_OK		= __BT_FUNC_STATUS_OK,
} bt_trace_set_environment_entry_status;

extern bt_trace_set_environment_entry_status
bt_trace_set_environment_entry_integer(bt_trace *trace, const char *name,
		int64_t value);

extern bt_trace_set_environment_entry_status
bt_trace_set_environment_entry_string(bt_trace *trace, const char *name,
		const char *value);

extern bt_stream *bt_trace_borrow_stream_by_index(bt_trace *trace,
		uint64_t index);

extern bt_stream *bt_trace_borrow_stream_by_id(bt_trace *trace,
		uint64_t id);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_TRACE_IR_TRACE_H */
