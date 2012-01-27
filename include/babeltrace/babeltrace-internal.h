#ifndef _BABELTRACE_INTERNAL_H
#define _BABELTRACE_INTERNAL_H

#include <stdio.h>
#include <glib.h>

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
	GPtrArray *array; /* struct trace_descriptor */
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
	opt_loglevel_field,
	opt_delta_field;

#endif
