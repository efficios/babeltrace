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
 */

#include <babeltrace/ctf/events.h>
#include <babeltrace/ctf-ir/metadata.h>

static inline
uint64_t ctf_get_timestamp_raw(struct ctf_stream_definition *stream,
			uint64_t timestamp)
{
	uint64_t ts_nsec;

	if (stream->current_clock->freq == 1000000000ULL) {
		ts_nsec = timestamp;
	} else {
		ts_nsec = (uint64_t) ((double) timestamp * 1000000000.0
				/ (double) stream->current_clock->freq);
	}
	return ts_nsec;
}

static inline
uint64_t ctf_get_timestamp(struct ctf_stream_definition *stream,
			uint64_t timestamp)
{
	uint64_t ts_nsec;
	struct ctf_trace *trace = stream->stream_class->trace;
	struct trace_collection *tc = trace->collection;
	uint64_t tc_offset = tc->single_clock_offset_avg;

	ts_nsec = ctf_get_timestamp_raw(stream, timestamp);
	ts_nsec += tc_offset;	/* Add offset */
	return ts_nsec;
}

#endif /* _CTF_EVENTS_PRIVATE_H */
