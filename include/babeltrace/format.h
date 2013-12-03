#ifndef _BABELTRACE_FORMAT_H
#define _BABELTRACE_FORMAT_H

/*
 * BabelTrace
 *
 * Trace Format Header
 *
 * Copyright 2010-2011 EfficiOS Inc. and Linux Foundation
 *
 * Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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

#include <babeltrace/list.h>
#include <babeltrace/clock-types.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int bt_intern_str;

/* forward declaration */
struct bt_stream_pos;
struct bt_context;
struct bt_trace_handle;
struct bt_trace_descriptor;

struct bt_mmap_stream {
	int fd;
	struct bt_list_head list;
	void *priv;
};

struct bt_mmap_stream_list {
	struct bt_list_head head;
};

struct bt_format {
	bt_intern_str name;

	struct bt_trace_descriptor *(*open_trace)(const char *path, int flags,
			void (*packet_seek)(struct bt_stream_pos *pos,
				size_t index, int whence),
			FILE *metadata_fp);
	struct bt_trace_descriptor *(*open_mmap_trace)(
			struct bt_mmap_stream_list *mmap_list,
			void (*packet_seek)(struct bt_stream_pos *pos,
				size_t index, int whence),
			FILE *metadata_fp);
	int (*close_trace)(struct bt_trace_descriptor *descriptor);
	void (*set_context)(struct bt_trace_descriptor *descriptor,
			struct bt_context *ctx);
	void (*set_handle)(struct bt_trace_descriptor *descriptor,
			struct bt_trace_handle *handle);
	uint64_t (*timestamp_begin)(struct bt_trace_descriptor *descriptor,
			struct bt_trace_handle *handle, enum bt_clock_type type);
	uint64_t (*timestamp_end)(struct bt_trace_descriptor *descriptor,
			struct bt_trace_handle *handle, enum bt_clock_type type);
	int (*convert_index_timestamp)(struct bt_trace_descriptor *descriptor);
};

extern struct bt_format *bt_lookup_format(bt_intern_str qname);
extern void bt_fprintf_format_list(FILE *fp);
extern int bt_register_format(struct bt_format *format);
extern void bt_unregister_format(struct bt_format *format);

#ifdef __cplusplus
}
#endif

#endif /* _BABELTRACE_FORMAT_H */
