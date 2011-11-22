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

#include <babeltrace/types.h>
#include <babeltrace/ctf/types.h>
#include <stdint.h>
#include <stdio.h>
#include <glib.h>

/* Parent trace descriptor */
struct trace_descriptor {
};

struct mmap_stream {
	int fd;
	struct cds_list_head list;
};

struct mmap_stream_list {
	struct cds_list_head head;
};

struct format {
	GQuark name;

	struct trace_descriptor *(*open_trace)(const char *collection_path,
			const char *path, int flags,
			void (*move_pos_slow)(struct ctf_stream_pos *pos, size_t offset,
				int whence), FILE *metadata_fp);
	struct trace_descriptor *(*open_mmap_trace)(
			struct mmap_stream_list *mmap_list,
			void (*move_pos_slow)(struct ctf_stream_pos *pos, size_t offset,
				int whence), FILE *metadata_fp);
	void (*close_trace)(struct trace_descriptor *descriptor);
};

extern struct format *bt_lookup_format(GQuark qname);
extern void bt_fprintf_format_list(FILE *fp);
extern int bt_register_format(struct format *format);

/* TBD: format unregistration */

#endif /* _BABELTRACE_FORMAT_H */
