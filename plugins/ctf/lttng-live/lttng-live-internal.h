#ifndef BABELTRACE_PLUGIN_CTF_LTTNG_LIVE_INTERNAL_H
#define BABELTRACE_PLUGIN_CTF_LTTNG_LIVE_INTERNAL_H

/*
 * BabelTrace - LTTng-live client Component
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2016 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * Author: Jérémie Galarneau <jeremie.galarneau@efficios.com>
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
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdbool.h>

#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/graph/component.h>
#include <babeltrace/graph/notification-iterator.h>
#include <babeltrace/graph/clock-class-priority-map.h>
#include "viewer-connection.h"

//TODO: this should not be used by plugins. Should copy code into plugin
//instead.
#include "babeltrace/object-internal.h"
#include "babeltrace/list-internal.h"
#include "../common/metadata/decoder.h"

#define STREAM_NAME_PREFIX	"stream-"
/* Account for u64 max string length. */
#define U64_STR_MAX_LEN		20
#define STREAM_NAME_MAX_LEN	(sizeof(STREAM_NAME_PREFIX) + U64_STR_MAX_LEN)

extern bool lttng_live_debug;

struct lttng_live_component;
struct lttng_live_session;

enum lttng_live_stream_state {
	LTTNG_LIVE_STREAM_ACTIVE_NO_DATA,
	LTTNG_LIVE_STREAM_QUIESCENT_NO_DATA,
	LTTNG_LIVE_STREAM_QUIESCENT,
	LTTNG_LIVE_STREAM_ACTIVE_DATA,
	LTTNG_LIVE_STREAM_EOF,
};

enum live_stream_type {
	LIVE_STREAM_TYPE_NO_STREAM,
	LIVE_STREAM_TYPE_STREAM,
};

struct lttng_live_stream_iterator_generic {
	enum live_stream_type type;
};

/* Iterator over a live stream. */
struct lttng_live_stream_iterator {
	struct lttng_live_stream_iterator_generic p;

	struct bt_ctf_stream *stream;
	struct lttng_live_trace *trace;
	struct bt_private_port *port;

	/* Node of stream list within the trace. */
	struct bt_list_head node;

	/*
	 * Since only a single iterator per viewer connection, we have
	 * only a single notification iterator per stream.
	 */
	struct bt_ctf_notif_iter *notif_iter;

	uint64_t viewer_stream_id;

	uint64_t ctf_stream_class_id;
	uint64_t base_offset;		/* base offset in current index. */
	uint64_t len;			/* len to read in current index. */
	uint64_t offset;		/* offset in current index. */

	int64_t last_returned_inactivity_timestamp;
	int64_t current_inactivity_timestamp;

	enum lttng_live_stream_state state;

	uint64_t current_packet_end_timestamp;
	struct bt_notification *packet_end_notif_queue;

	uint8_t *buf;
	size_t buflen;

	char name[STREAM_NAME_MAX_LEN];
};

struct lttng_live_no_stream_iterator {
	struct lttng_live_stream_iterator_generic p;

	struct lttng_live_component *lttng_live;
	struct bt_private_port *port;
};

struct lttng_live_component_options {
	bool opt_dummy : 1;
};

struct lttng_live_metadata {
	struct lttng_live_trace *trace;
	uint64_t stream_id;
	uint8_t uuid[16];
	bool is_uuid_set;
	int bo;
	char *text;

	struct ctf_metadata_decoder *decoder;

	bool closed;
};

struct lttng_live_trace {
	struct bt_object obj;

	/* Node of trace list within the session. */
	struct bt_list_head node;

	/* Back reference to session. */
	struct lttng_live_session *session;

	uint64_t id;	/* ctf trace ID within the session. */

	struct bt_ctf_trace *trace;

	struct lttng_live_metadata *metadata;
	struct bt_clock_class_priority_map *cc_prio_map;

	/* List of struct lttng_live_stream_iterator */
	struct bt_list_head streams;

	bool new_metadata_needed;
};

struct lttng_live_session {
	/* Node of session list within the component. */
	struct bt_list_head node;

	struct lttng_live_component *lttng_live;

	uint64_t id;

	/* List of struct lttng_live_trace */
	struct bt_list_head traces;

	bool attached;
	bool new_streams_needed;
	bool lazy_stream_notif_init;
	bool closed;
};

/*
 * A component instance is an iterator on a single session.
 */
struct lttng_live_component {
	struct bt_object obj;
	struct bt_private_component *private_component;	/* weak */
	struct bt_live_viewer_connection *viewer_connection;

	/* List of struct lttng_live_session */
	struct bt_list_head sessions;

	GString *url;
	FILE *error_fp;
	size_t max_query_size;
	struct lttng_live_component_options options;

	struct bt_private_port *no_stream_port;
	struct lttng_live_no_stream_iterator *no_stream_iter;
};

enum bt_ctf_lttng_live_iterator_status {
	/** Iterator state has progressed. Continue iteration immediately. */
	BT_CTF_LTTNG_LIVE_ITERATOR_STATUS_CONTINUE = 3,
	/** No notification available for now. Try again later. */
	BT_CTF_LTTNG_LIVE_ITERATOR_STATUS_AGAIN = 2,
	/** No more CTF_LTTNG_LIVEs to be delivered. */
	BT_CTF_LTTNG_LIVE_ITERATOR_STATUS_END = 1,
	/** No error, okay. */
	BT_CTF_LTTNG_LIVE_ITERATOR_STATUS_OK = 0,
	/** Invalid arguments. */
	BT_CTF_LTTNG_LIVE_ITERATOR_STATUS_INVAL = -1,
	/** General error. */
	BT_CTF_LTTNG_LIVE_ITERATOR_STATUS_ERROR = -2,
	/** Out of memory. */
	BT_CTF_LTTNG_LIVE_ITERATOR_STATUS_NOMEM = -3,
	/** Unsupported iterator feature. */
	BT_CTF_LTTNG_LIVE_ITERATOR_STATUS_UNSUPPORTED = -4,
};

BT_HIDDEN
enum bt_component_status lttng_live_component_init(struct bt_private_component *source,
		struct bt_value *params, void *init_method_data);

struct bt_value *lttng_live_query(struct bt_component_class *comp_class,
		const char *object, struct bt_value *params);

void lttng_live_component_finalize(struct bt_private_component *component);

BT_HIDDEN
struct bt_notification_iterator_next_return lttng_live_iterator_next(
        struct bt_private_notification_iterator *iterator);


enum bt_notification_iterator_status lttng_live_iterator_init(
		struct bt_private_notification_iterator *it,
		struct bt_private_port *port);

void lttng_live_iterator_finalize(struct bt_private_notification_iterator *it);

int lttng_live_create_viewer_session(struct lttng_live_component *lttng_live);
int lttng_live_attach_session(struct lttng_live_session *session);
int lttng_live_detach_session(struct lttng_live_session *session);
enum bt_ctf_lttng_live_iterator_status lttng_live_get_new_streams(
		struct lttng_live_session *session);

int lttng_live_add_session(struct lttng_live_component *lttng_live, uint64_t session_id);

ssize_t lttng_live_get_one_metadata_packet(struct lttng_live_trace *trace,
		FILE *fp);
enum bt_ctf_lttng_live_iterator_status lttng_live_get_next_index(
		struct lttng_live_component *lttng_live,
		struct lttng_live_stream_iterator *stream,
		struct packet_index *index);
enum bt_ctf_notif_iter_medium_status lttng_live_get_stream_bytes(
		struct lttng_live_component *lttng_live,
		struct lttng_live_stream_iterator *stream, uint8_t *buf, uint64_t offset,
		uint64_t req_len, uint64_t *recv_len);

int lttng_live_add_port(struct lttng_live_component *lttng_live,
		struct lttng_live_stream_iterator *stream_iter);
int lttng_live_remove_port(struct lttng_live_component *lttng_live,
		struct bt_private_port *port);

struct lttng_live_trace *lttng_live_ref_trace(
		struct lttng_live_session *session, uint64_t trace_id);
void lttng_live_unref_trace(struct lttng_live_trace *trace);
void lttng_live_need_new_streams(struct lttng_live_component *lttng_live);

#endif /* BABELTRACE_PLUGIN_CTF_LTTNG_LIVE_INTERNAL_H */
