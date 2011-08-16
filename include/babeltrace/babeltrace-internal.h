#ifndef _BABELTRACE_INTERNAL_H
#define _BABELTRACE_INTERNAL_H

#include <stdio.h>
#include <glib.h>

extern int babeltrace_verbose, babeltrace_debug;

#define printf_verbose(fmt, args...)				\
	do {							\
		if (babeltrace_verbose)				\
			printf("[verbose] " fmt, ## args);	\
	} while (0)

#define printf_debug(fmt, args...)				\
	do {							\
		if (babeltrace_debug)				\
			printf("[debug] " fmt, ## args);	\
	} while (0)

#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)

struct trace_descriptor;
struct trace_collection {
	GPtrArray *array;
};

int convert_trace(struct trace_descriptor *td_write,
		  struct trace_collection *trace_collection_read);

extern int opt_field_names;

#endif
