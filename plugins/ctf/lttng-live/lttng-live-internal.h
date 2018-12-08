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
#include <babeltrace/babeltrace.h>
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

	const bt_stream *stream;
	struct lttng_live_trace *trace;
	struct bt_private_port *port;	/* weak ref. */

	/* Node of stream list within the trace. */
	struct bt_list_head node;

	/*
	 * Since only a single iterator per viewer connection, we have
	 * only a single notification iterator per stream.
	 */
	struct bt_notif_iter *notif_iter;

	uint64_t viewer_stream_id;

	uint64_t ctf_stream_class_id;
	uint64_t base_offset;		/* base offset in current index. */
	uint64_t len;			/* len to read in current index. */
	uint64_t offset;		/* offset in current index. */

	int64_t last_returned_inactivity_timestamp;
	int64_t current_inactivity_timestamp;

	enum lttng_live_stream_state state;

	uint64_t current_packet_end_timestamp;
	const bt_notification *packet_end_notif_queue;

	uint8_t *buf;
	size_t buflen;

	char name[STREAM_NAME_MAX_LEN];
};

struct lttng_live_no_stream_iterator {
	struct lttng_live_stream_iterator_generic p;

	struct lttng_live_component *lttng_live;
	struct bt_private_port *port;	/* weak ref. */
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
	bt_object obj;

	/* Node of trace list within the session. */
	struct bt_list_head node;

	/* Back reference to session. */
	struct lttng_live_session *session;

	uint64_t id;	/* ctf trace ID within the session. */

	const bt_trace *trace;

	struct lttng_live_metadata *metadata;
	bt_clock_class_priority_map *cc_prio_map;

	/* List of struct lttng_live_stream_iterator */
	struct bt_list_head streams;

	bool new_metadata_needed;
};

struct lttng_live_session {
	/* Node of session list within the component. */
	struct bt_list_head node;

	struct lttng_live_component *lttng_live;

	GString *hostname;
	GString *session_name;

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
	bt_object obj;
	bt_self_component *private_component;	/* weak */
	struct bt_live_viewer_connection *viewer_connection;

	/* List of struct lttng_live_session */
	struct bt_list_head sessions;

	GString *url;
	size_t max_query_size;
	struct lttng_live_component_options options;

	struct bt_private_port *no_stream_port;	/* weak */
	struct lttng_live_no_stream_iterator *no_stream_iter;

	bt_component *downstream_component;
};

enum bt_lttng_live_iterator_status {
	/** Iterator state has progressed. Continue iteration immediately. */
	BT_LTTNG_LIVE_ITERATOR_STATUS_CONTINUE = 3,
	/** No notification available for now. Try again later. */
	BT_LTTNG_LIVE_ITERATOR_STATUS_AGAIN = 2,
	/** No more CTF_LTTNG_LIVEs to be delivered. */
	BT_LTTNG_LIVE_ITERATOR_STATUS_END = 1,
	/** No error, okay. */
	BT_LTTNG_LIVE_ITERATOR_STATUS_OK = 0,
	/** Invalid arguments. */
	BT_LTTNG_LIVE_ITERATOR_STATUS_INVAL = -1,
	/** General error. */
	BT_LTTNG_LIVE_ITERATOR_STATUS_ERROR = -2,
	/** Out of memory. */
	BT_LTTNG_LIVE_ITERATOR_STATUS_NOMEM = -3,
	/** Unsupported iterator feature. */
	BT_LTTNG_LIVE_ITERATOR_STATUS_UNSUPPORTED = -4,
};

enum bt_component_status lttng_live_component_init(bt_self_component *source,
		bt_value *params, void *init_method_data);

bt_component_class_query_method_return lttng_live_query(
		const bt_component_class *comp_class,
		const bt_query_executor *query_exec,
		const char *object, bt_value *params);

void lttng_live_component_finalize(bt_self_component *component);

bt_notification_iterator_next_method_return lttng_live_iterator_next(
        bt_self_notification_iterator *iterator);

enum bt_component_status lttng_live_accept_port_connection(
		bt_self_component *private_component,
		struct bt_private_port *self_private_port,
		const bt_port *other_port);

enum bt_notification_iterator_status lttng_live_iterator_init(
		bt_self_notification_iterator *it,
		struct bt_private_port *port);

void lttng_live_iterator_finalize(bt_self_notification_iterator *it);

int lttng_live_create_viewer_session(struct lttng_live_component *lttng_live);
int lttng_live_attach_session(struct lttng_live_session *session);
int lttng_live_detach_session(struct lttng_live_session *session);
enum bt_lttng_live_iterator_status lttng_live_get_new_streams(
		struct lttng_live_session *session);

int lttng_live_add_session(struct lttng_live_component *lttng_live,
		uint64_t session_id,
		const char *hostname,
		const char *session_name);

ssize_t lttng_live_get_one_metadata_packet(struct lttng_live_trace *trace,
		FILE *fp);
enum bt_lttng_live_iterator_status lttng_live_get_next_index(
		struct lttng_live_component *lttng_live,
		struct lttng_live_stream_iterator *stream,
		struct packet_index *index);
enum bt_notif_iter_medium_status lttng_live_get_stream_bytes(
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

bt_bool lttng_live_is_canceled(struct lttng_live_component *lttng_live);

#endif /* BABELTRACE_PLUGIN_CTF_LTTNG_LIVE_INTERNAL_H */
