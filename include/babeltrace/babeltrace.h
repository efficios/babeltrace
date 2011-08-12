#ifndef _BABELTRACE_H
#define _BABELTRACE_H

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

struct trace_descriptor;
struct trace_collection {
	GPtrArray *array;
};

int convert_trace(struct trace_descriptor *td_write,
		  struct trace_collection *trace_collection_read);

extern int opt_field_names;

#endif
