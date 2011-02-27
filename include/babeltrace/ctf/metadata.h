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
#include <glib.h>

struct ctf_trace;
struct ctf_stream;
struct ctf_event;

struct ctf_trace {
	struct declaration_scope *scope;	/* root scope */
	GArray *streams;	/* Array of struct ctf_stream */

	uint64_t major;
	uint64_t minor;
	uint8_t uuid[16];
	uint64_t word_size;
};

struct ctf_stream {
	struct declaration_scope *scope;	/* parent is trace scope */
	GArray *events_by_id;	/* Array of struct ctf_event indexed by id */
	GHashTable *event_quark_to_id;	/* GQuark to numeric id */

	uint64_t stream_id;
	struct declaration_struct *event_header;
	struct declaration_struct *event_context;
	struct declaration_struct *packet_context;
};


struct ctf_event {
	struct declaration_scope *scope;	/* parent is stream scope */
	GQuark name;
	uint64_t id;	/* Numeric identifier within the stream */
	uint64_t stream_id;
	struct declaration_struct *context;
	struct declaration_struct *fields;
};

#endif /* _BABELTRACE_CTF_METADATA_H */
