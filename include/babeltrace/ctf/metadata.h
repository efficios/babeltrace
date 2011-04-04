#ifndef _BABELTRACE_CTF_METADATA_H
#define _BABELTRACE_CTF_METADATA_H

/*
 * BabelTrace
 *
 * CTF Metadata Header
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
#include <uuid/uuid.h>
#include <assert.h>
#include <glib.h>

struct ctf_trace;
struct ctf_stream;
struct ctf_event;

#define CTF_TRACE_SET_FIELD(ctf_trace, field, value)			\
	do {								\
		(ctf_trace)->(field) = (value);				\
		(ctf_trace)->field_mask |= CTF_TRACE_ ## field;		\
	} while (0)

#define CTF_TRACE_FIELD_IS_SET(ctf_trace, field)			\
		((ctf_trace)->field_mask & CTF_TRACE_ ## field)

#define CTF_TRACE_GET_FIELD(ctf_trace, field)				\
	({								\
		assert(CTF_TRACE_FIELD_IS_SET(ctf_trace, field));	\
		(ctf_trace)->(field);					\
	})


struct ctf_trace {
	/* root scope */
	struct declaration_scope *root_declaration_scope;
	/* root scope */
	struct definition_scope *root_definition_scope;

	struct declaration_scope *declaration_scope;
	struct definition_scope *definition_scope;
	GPtrArray *streams;			/* Array of struct ctf_stream pointers*/

	uint64_t major;
	uint64_t minor;
	uuid_t uuid;
	uint64_t word_size;

	enum {					/* Fields populated mask */
		CTF_TRACE_major	=	(1U << 0),
		CTF_TRACE_minor =	(1U << 1),
		CTF_TRACE_uuid	=	(1U << 2),
		CTF_TRACE_word_size =	(1U << 3),
	} field_mask;
};

#define CTF_STREAM_SET_FIELD(ctf_stream, field, value)			\
	do {								\
		(ctf_stream)->(field) = (value);			\
		(ctf_stream)->field_mask |= CTF_STREAM_ ## field;	\
	} while (0)

#define CTF_STREAM_FIELD_IS_SET(ctf_stream, field)			\
		((ctf_stream)->field_mask & CTF_STREAM_ ## field)

#define CTF_STREAM_GET_FIELD(ctf_stream, field)				\
	({								\
		assert(CTF_STREAM_FIELD_IS_SET(ctf_stream, field));	\
		(ctf_stream)->(field);					\
	})

struct ctf_stream {
	struct ctf_trace *trace;
	/* parent is lexical scope conaining the stream scope */
	struct declaration_scope *declaration_scope;
	/* parent is trace scope */
	struct definition_scope *definition_scope;
	GPtrArray *events_by_id;		/* Array of struct ctf_event pointers indexed by id */
	GHashTable *event_quark_to_id;		/* GQuark to numeric id */

	struct definition_struct *event_header;
	struct definition_struct *event_context;
	struct definition_struct *packet_context;

	uint64_t stream_id;

	enum {					/* Fields populated mask */
		CTF_STREAM_stream_id =	(1 << 0),
	} field_mask;
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

struct ctf_event {
	/* stream mapped by stream_id */
	struct ctf_stream *stream;
	/* parent is lexical scope conaining the event scope */
	struct declaration_scope *declaration_scope;
	/* parent is stream scope */
	struct definition_scope *definition_scope;
	struct definition_struct *context;
	struct definition_struct *fields;

	GQuark name;
	uint64_t id;		/* Numeric identifier within the stream */
	uint64_t stream_id;

	enum {					/* Fields populated mask */
		CTF_EVENT_name	=	(1 << 0),
		CTF_EVENT_id 	= 	(1 << 1),
		CTF_EVENT_stream_id = 	(1 << 2),
	} field_mask;
};

#endif /* _BABELTRACE_CTF_METADATA_H */
