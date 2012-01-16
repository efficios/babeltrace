/*
 * babeltrace.c
 *
 * Babeltrace Library
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

#include <babeltrace/babeltrace.h>
#include <babeltrace/ctf-text/types.h>
#include <stdlib.h>

int babeltrace_verbose, babeltrace_debug;

int convert_trace(struct trace_descriptor *td_write,
		  struct trace_collection *trace_collection_read)
{
	struct babeltrace_iter *iter;
	struct ctf_stream *stream;
	struct ctf_stream_event *event;
	struct ctf_text_stream_pos *sout;
	struct trace_collection_pos begin_pos;
	int ret = 0;

	sout = container_of(td_write, struct ctf_text_stream_pos,
			trace_descriptor);

	begin_pos.type = BT_SEEK_BEGIN;
	iter = babeltrace_iter_create(trace_collection_read, &begin_pos, NULL);
	while (babeltrace_iter_read_event(iter, &stream, &event) == 0) {
		ret = sout->parent.event_cb(&sout->parent, stream);
		if (ret) {
			fprintf(stdout, "[error] Writing event failed.\n");
			goto end;
		}
		ret = babeltrace_iter_next(iter);
		if (ret < 0)
			goto end;
	}
end:
	babeltrace_iter_destroy(iter);
	return ret;
}

static
void __attribute__((constructor)) init_babeltrace_lib(void)
{
	if (getenv("BABELTRACE_VERBOSE"))
		babeltrace_verbose = 1;
	if (getenv("BABELTRACE_DEBUG"))
		babeltrace_debug = 1;
}
