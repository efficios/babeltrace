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
 */

#include <babeltrace/list.h>
#include <stdint.h>
#include <stdio.h>

typedef int bt_intern_str;

/* forward declaration */
struct stream_pos;
struct bt_context;
struct bt_trace_handle;

/* Parent trace descriptor */
struct trace_descriptor {
};

struct mmap_stream {
	int fd;
	struct bt_list_head list;
};

struct mmap_stream_list {
	struct bt_list_head head;
};

struct format {
	bt_intern_str name;

	struct trace_descriptor *(*open_trace)(const char *path, int flags,
			void (*packet_seek)(struct stream_pos *pos,
				size_t index, int whence),
			FILE *metadata_fp);
	struct trace_descriptor *(*open_mmap_trace)(
			struct mmap_stream_list *mmap_list,
			void (*packet_seek)(struct stream_pos *pos,
				size_t index, int whence),
			FILE *metadata_fp);
	void (*close_trace)(struct trace_descriptor *descriptor);
	void (*set_context)(struct trace_descriptor *descriptor,
			struct bt_context *ctx);
	void (*set_handle)(struct trace_descriptor *descriptor,
			struct bt_trace_handle *handle);
	uint64_t (*timestamp_begin)(struct trace_descriptor *descriptor,
			struct bt_trace_handle *handle);
	uint64_t (*timestamp_end)(struct trace_descriptor *descriptor,
			struct bt_trace_handle *handle);
};

extern struct format *bt_lookup_format(bt_intern_str qname);
extern void bt_fprintf_format_list(FILE *fp);
extern int bt_register_format(struct format *format);

/* TBD: format unregistration */

#endif /* _BABELTRACE_FORMAT_H */
