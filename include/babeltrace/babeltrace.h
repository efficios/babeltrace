#ifndef _BABELTRACE_H
#define _BABELTRACE_H

#include <stdio.h>

#define BABELTRACE_VERSION_MAJOR	0
#define BABELTRACE_VERSION_MINOR	1

#ifndef __stringify
#define __stringify_1(a...)	#a
#define __stringify(a...)	__stringify_1(a)
#endif

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

int convert_trace(struct trace_descriptor *td_write,
		  struct trace_descriptor *td_read);

#endif
