#ifndef _CTF_EVENTS_PRIVATE_H
#define _CTF_EVENTS_PRIVATE_H

/*
 * ctf/events-private.h
 *
 * Babeltrace Library
 *
 * Copyright 2011-2012 EfficiOS Inc. and Linux Foundation
 *
 * Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *         Julien Desfossez <julien.desfossez@efficios.com>
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

#include <babeltrace/ctf/events.h>
#include <babeltrace/ctf-ir/metadata.h>
#include <babeltrace/clock-internal.h>

static inline
uint64_t ctf_get_real_timestamp(struct ctf_stream_definition *stream,
			uint64_t timestamp)
{
	uint64_t ts_nsec;
	struct ctf_trace *trace = stream->stream_class->trace;
	struct trace_collection *tc = trace->parent.collection;
	uint64_t tc_offset;

	if (tc->clock_use_offset_avg)
		tc_offset = tc->single_clock_offset_avg;
	else
		tc_offset = clock_offset_ns(trace->parent.single_clock);

	ts_nsec = clock_cycles_to_ns(stream->current_clock, timestamp);
	ts_nsec += tc_offset;	/* Add offset */
	return ts_nsec;
}

#endif /* _CTF_EVENTS_PRIVATE_H */
