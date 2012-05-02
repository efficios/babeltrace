#ifndef _BABELTRACE_CTF_IR_METADATA_H
#define _BABELTRACE_CTF_IR_METADATA_H

/*
 * BabelTrace
 *
 * CTF Intermediate Representation Metadata Header
 *
 * Copyright 2011 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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
#include <babeltrace/format.h>
#include <babeltrace/ctf/types.h>
#include <sys/types.h>
#include <dirent.h>
#include <babeltrace/uuid.h>
#include <assert.h>
#include <glib.h>

struct ctf_trace;
struct ctf_stream_declaration;
struct ctf_event_declaration;
struct ctf_clock;

struct ctf_stream_definition {
	struct ctf_stream_declaration *stream_class;
	uint64_t timestamp;			/* Current timestamp, in ns */
	uint64_t event_id;			/* Current event ID */
	int has_timestamp;
	uint64_t stream_id;
	int consumed;				/* Last packet used by caller */

	struct definition_struct *trace_packet_header;
	struct definition_struct *stream_packet_context;
	struct definition_struct *stream_event_header;
	struct definition_struct *stream_event_context;
	GPtrArray *events_by_id;		/* Array of struct ctf_event_definition pointers indexed by id */
	struct definition_scope *parent_def_scope;	/* for initialization */
	int stream_definitions_created;

	struct ctf_clock *current_clock;

	/* Event discarded information */
	uint64_t events_discarded;
	uint64_t prev_timestamp;	/* Start-of-last-packet timestamp */
	uint64_t prev_timestamp_end;	/* End-of-last-packet timestamp */
};

struct ctf_event_definition {
	struct ctf_stream_definition *stream;
	struct definition_struct *event_context;
	struct definition_struct *event_fields;
};

#define CTF_CLOCK_SET_FIELD(ctf_clock, field)				\
	do {								\
		(ctf_clock)->field_mask |= CTF_CLOCK_ ## field;		\
	} while (0)

#define CTF_CLOCK_FIELD_IS_SET(ctf_clock, field)			\
		((ctf_clock)->field_mask & CTF_CLOCK_ ## field)

#define CTF_CLOCK_GET_FIELD(ctf_clock, field)				\
	({								\
		assert(CTF_CLOCK_FIELD_IS_SET(ctf_clock, field));	\
		(ctf_clock)->(field);					\
	})

struct ctf_clock {
	GQuark name;
	GQuark uuid;
	char *description;
	uint64_t freq;	/* frequency, in HZ */
	/* precision in seconds is: precision * (1/freq) */
	uint64_t precision;
	/*
	 * The offset from Epoch is: offset_s + (offset * (1/freq))
	 * Coarse clock offset from Epoch (in seconds).
	 */
	uint64_t offset_s;
	/* Fine clock offset from Epoch, in (1/freq) units. */
	uint64_t offset;
	int absolute;

	enum {					/* Fields populated mask */
		CTF_CLOCK_name		=	(1U << 0),
		CTF_CLOCK_freq		=	(1U << 1),
	} field_mask;
};

#define CTF_TRACE_SET_FIELD(ctf_trace, field)				\
	do {								\
		(ctf_trace)->field_mask |= CTF_TRACE_ ## field;		\
	} while (0)

#define CTF_TRACE_FIELD_IS_SET(ctf_trace, field)			\
		((ctf_trace)->field_mask & CTF_TRACE_ ## field)

#define CTF_TRACE_GET_FIELD(ctf_trace, field)				\
	({								\
		assert(CTF_TRACE_FIELD_IS_SET(ctf_trace, field));	\
		(ctf_trace)->(field);					\
	})

#define TRACER_ENV_LEN	128

/* tracer-specific environment */
struct ctf_tracer_env {
	int vpid;		/* negative if unset */

	/* All strings below: "" if unset. */
	char procname[TRACER_ENV_LEN];
	char domain[TRACER_ENV_LEN];
	char sysname[TRACER_ENV_LEN];
	char release[TRACER_ENV_LEN];
	char version[TRACER_ENV_LEN];
};

struct ctf_trace {
	struct trace_descriptor parent;
	/* root scope */
	struct declaration_scope *root_declaration_scope;

	struct declaration_scope *declaration_scope;
	/* innermost definition scope. to be used as parent of stream. */
	struct definition_scope *definition_scope;
	GPtrArray *streams;			/* Array of struct ctf_stream_declaration pointers */
	struct ctf_stream_definition *metadata;
	GHashTable *clocks;
	struct ctf_clock *single_clock;		/* currently supports only one clock */
	struct trace_collection *collection;	/* Container of this trace */
	GPtrArray *event_declarations;		/* Array of all the struct bt_ctf_event_decl */

	struct declaration_struct *packet_header_decl;

	uint64_t major;
	uint64_t minor;
	unsigned char uuid[BABELTRACE_UUID_LEN];
	int byte_order;		/* trace BYTE_ORDER. 0 if unset. */
	struct ctf_tracer_env env;

	enum {					/* Fields populated mask */
		CTF_TRACE_major		=	(1U << 0),
		CTF_TRACE_minor		=	(1U << 1),
		CTF_TRACE_uuid		=	(1U << 2),
		CTF_TRACE_byte_order	=	(1U << 3),
		CTF_TRACE_packet_header	=	(1U << 4),
	} field_mask;

	/* Information about trace backing directory and files */
	DIR *dir;
	int dirfd;
	int flags;		/* open flags */

	/* Heap of streams, ordered to always get the lowest timestam */
	struct ptr_heap *stream_heap;
	char path[PATH_MAX];

	struct bt_context *ctx;
	struct bt_trace_handle *handle;
};

#define CTF_STREAM_SET_FIELD(ctf_stream, field)				\
	do {								\
		(ctf_stream)->field_mask |= CTF_STREAM_ ## field;	\
	} while (0)

#define CTF_STREAM_FIELD_IS_SET(ctf_stream, field)			\
		((ctf_stream)->field_mask & CTF_STREAM_ ## field)

#define CTF_STREAM_GET_FIELD(ctf_stream, field)				\
	({								\
		assert(CTF_STREAM_FIELD_IS_SET(ctf_stream, field));	\
		(ctf_stream)->(field);					\
	})

struct ctf_stream_declaration {
	struct ctf_trace *trace;
	/* parent is lexical scope conaining the stream scope */
	struct declaration_scope *declaration_scope;
	/* innermost definition scope. to be used as parent of event. */
	struct definition_scope *definition_scope;
	GPtrArray *events_by_id;		/* Array of struct ctf_event_declaration pointers indexed by id */
	GHashTable *event_quark_to_id;		/* GQuark to numeric id */

	struct declaration_struct *packet_context_decl;
	struct declaration_struct *event_header_decl;
	struct declaration_struct *event_context_decl;

	uint64_t stream_id;

	enum {					/* Fields populated mask */
		CTF_STREAM_stream_id =	(1 << 0),
	} field_mask;

	GPtrArray *streams;	/* Array of struct ctf_stream_definition pointers */
};

#define CTF_EVENT_SET_FIELD(ctf_event, field)				\
	do {								\
		(ctf_event)->field_mask |= CTF_EVENT_ ## field;		\
	} while (0)

#define CTF_EVENT_FIELD_IS_SET(ctf_event, field)			\
		((ctf_event)->field_mask & CTF_EVENT_ ## field)

#define CTF_EVENT_GET_FIELD(ctf_event, field)				\
	({								\
		assert(CTF_EVENT_FIELD_IS_SET(ctf_event, field));	\
		(ctf_event)->(field);					\
	})

struct ctf_event_declaration {
	/* stream mapped by stream_id */
	struct ctf_stream_declaration *stream;
	/* parent is lexical scope conaining the event scope */
	struct declaration_scope *declaration_scope;

	struct declaration_struct *context_decl;
	struct declaration_struct *fields_decl;

	GQuark name;
	uint64_t id;		/* Numeric identifier within the stream */
	uint64_t stream_id;
	int loglevel;

	enum {					/* Fields populated mask */
		CTF_EVENT_name	=		(1 << 0),
		CTF_EVENT_id 	= 		(1 << 1),
		CTF_EVENT_stream_id = 		(1 << 2),
		CTF_EVENT_loglevel =		(1 << 4),
	} field_mask;
};

#endif /* _BABELTRACE_CTF_IR_METADATA_H */
