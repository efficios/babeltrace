#ifndef _BABELTRACE_CTF_EVENTS_INTERNAL_H
#define _BABELTRACE_CTF_EVENTS_INTERNAL_H

/*
 * BabelTrace
 *
 * CTF events API (internal)
 *
 * Copyright 2011-2012 EfficiOS Inc. and Linux Foundation
 *
 * Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *         Julien Desfossez <julien.desfossez@efficios.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <babeltrace/iterator-internal.h>
#include <babeltrace/ctf/callbacks.h>
#include <babeltrace/ctf/callbacks-internal.h>
#include <babeltrace/ctf-ir/metadata.h>
#include <glib.h>

struct ctf_stream_definition;

/*
 * These structures are public mappings to internal ctf_event structures.
 */
struct bt_ctf_event {
	struct ctf_event_definition *parent;
};

struct bt_ctf_event_decl {
	struct ctf_event_declaration parent;
	GPtrArray *context_decl;
	GPtrArray *fields_decl;
	GPtrArray *packet_header_decl;
	GPtrArray *event_context_decl;
	GPtrArray *event_header_decl;
	GPtrArray *packet_context_decl;
};

struct bt_ctf_iter {
	struct bt_iter parent;
	struct bt_ctf_event current_ctf_event;	/* last read event */
	GArray *callbacks;				/* Array of struct bt_stream_callbacks */
	struct bt_callback_chain main_callbacks;	/* For all events */
	/*
	 * Flag indicating if dependency graph needs to be recalculated.
	 * Set by bt_iter_add_callback(), and checked (and
	 * cleared) by upon entry into bt_iter_read_event().
	 * bt_iter_read_event() is responsible for calling dep
	 * graph calculation if it sees this flag set.
	 */
	int recalculate_dep_graph;
	/*
	 * Array of pointers to struct bt_dependencies, for garbage
	 * collection. We're not using a linked list here because each
	 * struct bt_dependencies can belong to more than one
	 * bt_iter.
	 */
	GPtrArray *dep_gc;
	uint64_t events_lost;
};

#endif /*_BABELTRACE_CTF_EVENTS_INTERNAL_H */
