#ifndef _BABELTRACE_INTERNAL_H
#define _BABELTRACE_INTERNAL_H

/*
 * babeltrace/babeltrace-internal.h
 *
 * Copyright 2012 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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
#include <stdio.h>
#include <glib.h>
#include <stdint.h>

extern int babeltrace_verbose, babeltrace_debug;

#define printf_verbose(fmt, args...)					\
	do {								\
		if (babeltrace_verbose)					\
			fprintf(stdout, "[verbose] " fmt, ## args);	\
	} while (0)

#define printf_debug(fmt, args...)					\
	do {								\
		if (babeltrace_debug)					\
			fprintf(stdout, "[debug] " fmt, ## args);	\
	} while (0)

#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)

struct trace_descriptor;
struct trace_collection {
	GPtrArray *array;	/* struct trace_descriptor */
	GHashTable *clocks;	/* struct ctf_clock */

	uint64_t single_clock_offset_avg;
	uint64_t offset_first;
	int64_t delta_offset_first_sum;
	int offset_nr;
	int clock_use_offset_avg;
};

extern int opt_all_field_names,
	opt_scope_field_names,
	opt_header_field_names,
	opt_context_field_names,
	opt_payload_field_names,
	opt_all_fields,
	opt_trace_field,
	opt_trace_domain_field,
	opt_trace_procname_field,
	opt_trace_vpid_field,
	opt_trace_hostname_field,
	opt_trace_default_fields,
	opt_loglevel_field,
	opt_emf_field,
	opt_callsite_field,
	opt_delta_field,
	opt_clock_cycles,
	opt_clock_seconds,
	opt_clock_date,
	opt_clock_gmt,
	opt_clock_force_correlate;

extern uint64_t opt_clock_offset;

#endif
