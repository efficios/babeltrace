#ifndef BABELTRACE_PLUGIN_CTF_LTTNG_LIVE_H
#define BABELTRACE_PLUGIN_CTF_LTTNG_LIVE_H

/*
 * BabelTrace - LTTng-live client Component
 *
 * Copyright 2019 Francis Deslauriers <francis.deslauriers@efficios.com>
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
#include <stdint.h>

#include <glib.h>

#include <babeltrace2/babeltrace.h>

#include "common/macros.h"
#include "../common/metadata/decoder.h"
#include "../common/msg-iter/msg-iter.h"
#include "viewer-connection.h"

struct lttng_live_component;
struct lttng_live_session;
struct lttng_live_msg_iter;

enum lttng_live_stream_state {
	/* This stream won't have data until some known time in the future. */
	LTTNG_LIVE_STREAM_QUIESCENT,
	/*
	 * This stream won't have data until some known time in the future and
	 * the message iterator inactivity message was already sent downstream.
	 */
	LTTNG_LIVE_STREAM_QUIESCENT_NO_DATA, /* */
	/* This stream has data ready to be consumed. */
	LTTNG_LIVE_STREAM_ACTIVE_DATA,
	/*
	 * This stream has no data left to consume. We should asked the relay
	 * for more.
	 */
	LTTNG_LIVE_STREAM_ACTIVE_NO_DATA,
	/* This stream won't have anymore data, ever. */
	LTTNG_LIVE_STREAM_EOF,
};

/* Iterator over a live stream. */
struct lttng_live_stream_iterator {
	bt_logging_level log_level;
	bt_self_component *self_comp;

	/* Owned by this. */
	bt_stream *stream;

	/* Weak reference. */
	struct lttng_live_trace *trace;

	/*
	 * Since only a single iterator per viewer connection, we have
	 * only a single message iterator per stream.
	 */
	struct ctf_msg_iter *msg_iter;

	uint64_t viewer_stream_id;

	uint64_t ctf_stream_class_id;

	/* base offset in current index. */
	uint64_t base_offset;
	/* len to read in current index. */
	uint64_t len;
	/* offset in current index. */
	uint64_t offset;

	/*
	 * Clock Snapshot value of the last message iterator inactivity message
	 * sent downstream.
	 */
	uint64_t last_inactivity_ts;

	/*
	 * Clock Snapshot value of the current message iterator inactivity
	 * message we might want to send downstream.
	 */
	uint64_t current_inactivity_ts;

	enum lttng_live_stream_state state;

	/*
	 * The current message produced by this live stream iterator. Owned by
	 * this.
	 */
	const bt_message *current_msg;

	/* Timestamp in nanoseconds of the current message (current_msg). */
	int64_t current_msg_ts_ns;

	/* Owned by this. */
	uint8_t *buf;
	size_t buflen;

	/* Owned by this. */
	GString *name;

	bool has_stream_hung_up;
};

struct lttng_live_metadata {
	bt_logging_level log_level;
	bt_self_component *self_comp;

	uint64_t stream_id;
	/* Weak reference. */
	struct ctf_metadata_decoder *decoder;
};

enum lttng_live_metadata_stream_state {
	/*
	 * The metadata needs to be updated. This is either because we just
	 * created the trace and haven't asked yet, or the relay specifically
	 * told us that new metadata is available.
	 */
	LTTNG_LIVE_METADATA_STREAM_STATE_NEEDED,
	/*
	 * The metadata was updated and the relay has not told us we need to
	 * update it yet.
	 */
	LTTNG_LIVE_METADATA_STREAM_STATE_NOT_NEEDED,
	/*
	 * The relay has closed this metadata stream. We set this in reaction
	 * to a LTTNG_VIEWER_METADATA_ERR reply to a LTTNG_VIEWER_GET_METADATA
	 * command to the relay. If this field is set, we have received all the
	 * metadata that we are ever going to get for that metadata stream.
	 */
	LTTNG_LIVE_METADATA_STREAM_STATE_CLOSED,
};

struct lttng_live_trace {
	bt_logging_level log_level;
	bt_self_component *self_comp;

	/* Back reference to session. */
	struct lttng_live_session *session;

	/* ctf trace ID within the session. */
	uint64_t id;

	/* Owned by this. */
	bt_trace *trace;

	/* Weak reference. */
	bt_trace_class *trace_class;

	struct lttng_live_metadata *metadata;

	const bt_clock_class *clock_class;

	/* Array of pointers to struct lttng_live_stream_iterator. */
	/* Owned by this. */
	GPtrArray *stream_iterators;

	enum lttng_live_metadata_stream_state metadata_stream_state;
};

struct lttng_live_session {
	bt_logging_level log_level;
	bt_self_component *self_comp;

	/* Weak reference. */
	struct lttng_live_msg_iter *lttng_live_msg_iter;

	/* Owned by this. */
	GString *hostname;

	/* Owned by this. */
	GString *session_name;

	uint64_t id;

	/* Array of pointers to struct lttng_live_trace. */
	GPtrArray *traces;

	bool attached;
	bool new_streams_needed;
	bool lazy_stream_msg_init;
	bool closed;
};

enum session_not_found_action {
	SESSION_NOT_FOUND_ACTION_CONTINUE,
	SESSION_NOT_FOUND_ACTION_FAIL,
	SESSION_NOT_FOUND_ACTION_END,
};

/*
 * A component instance is an iterator on a single session.
 */
struct lttng_live_component {
	bt_logging_level log_level;

	/* Weak reference. */
	bt_self_component *self_comp;

	struct {
		GString *url;
		enum session_not_found_action sess_not_found_act;
	} params;

	size_t max_query_size;

	/*
	 * Keeps track of whether the downstream component already has a
	 * message iterator on this component.
	 */
	bool has_msg_iter;
};

struct lttng_live_msg_iter {
	bt_logging_level log_level;
	bt_self_component *self_comp;

	/* Weak reference. */
	struct lttng_live_component *lttng_live_comp;

	/* Weak reference. */
	bt_self_message_iterator *self_msg_iter;

	/* Owned by this. */
	struct live_viewer_connection *viewer_connection;

	/* Array of pointers to struct lttng_live_session. */
	GPtrArray *sessions;

	/* Number of live stream iterator this message iterator has.*/
	uint64_t active_stream_iter;

	/* Timestamp in nanosecond of the last message sent downstream. */
	int64_t last_msg_ts_ns;

	/* True if the iterator was interrupted. */
	bool was_interrupted;
};

enum lttng_live_iterator_status {
	/** Iterator state has progressed. Continue iteration immediately. */
	LTTNG_LIVE_ITERATOR_STATUS_CONTINUE = 3,
	/** No message available for now. Try again later. */
	LTTNG_LIVE_ITERATOR_STATUS_AGAIN = 2,
	/** No more CTF_LTTNG_LIVEs to be delivered. */
	LTTNG_LIVE_ITERATOR_STATUS_END = 1,
	/** No error, okay. */
	LTTNG_LIVE_ITERATOR_STATUS_OK = 0,
	/** Invalid arguments. */
	LTTNG_LIVE_ITERATOR_STATUS_INVAL = -1,
	/** General error. */
	LTTNG_LIVE_ITERATOR_STATUS_ERROR = -2,
	/** Out of memory. */
	LTTNG_LIVE_ITERATOR_STATUS_NOMEM = -3,
	/** Unsupported iterator feature. */
	LTTNG_LIVE_ITERATOR_STATUS_UNSUPPORTED = -4,
};

bt_component_class_initialize_method_status lttng_live_component_init(
		bt_self_component_source *self_comp,
		bt_self_component_source_configuration *config,
		const bt_value *params, void *init_method_data);

bt_component_class_query_method_status lttng_live_query(
		bt_self_component_class_source *comp_class,
		bt_private_query_executor *priv_query_exec,
		const char *object, const bt_value *params,
		void *method_data, const bt_value **result);

void lttng_live_component_finalize(bt_self_component_source *component);

bt_message_iterator_class_next_method_status lttng_live_msg_iter_next(
		bt_self_message_iterator *iterator,
		bt_message_array_const msgs, uint64_t capacity,
		uint64_t *count);

bt_message_iterator_class_initialize_method_status lttng_live_msg_iter_init(
		bt_self_message_iterator *self_msg_it,
		bt_self_message_iterator_configuration *config,
		bt_self_component_port_output *self_port);

void lttng_live_msg_iter_finalize(bt_self_message_iterator *it);

enum lttng_live_viewer_status lttng_live_session_attach(
		struct lttng_live_session *session,
		bt_self_message_iterator *self_msg_iter);

enum lttng_live_viewer_status lttng_live_session_detach(
		struct lttng_live_session *session);

enum lttng_live_iterator_status lttng_live_session_get_new_streams(
		struct lttng_live_session *session,
		bt_self_message_iterator *self_msg_iter);

struct lttng_live_trace *lttng_live_session_borrow_or_create_trace_by_id(
		struct lttng_live_session *session, uint64_t trace_id);

int lttng_live_add_session(struct lttng_live_msg_iter *lttng_live_msg_iter,
		uint64_t session_id,
		const char *hostname,
		const char *session_name);

/*
 * lttng_live_get_one_metadata_packet() asks the Relay Daemon for new metadata.
 * If new metadata is received, the function writes it to the provided file
 * handle and updates the reply_len output parameter. This function should be
 * called in loop until _END status is received to ensure all metadata is
 * written to the file.
 */
enum lttng_live_get_one_metadata_status lttng_live_get_one_metadata_packet(
		struct lttng_live_trace *trace, FILE *fp, size_t *reply_len);

enum lttng_live_iterator_status lttng_live_get_next_index(
		struct lttng_live_msg_iter *lttng_live_msg_iter,
		struct lttng_live_stream_iterator *stream,
		struct packet_index *index);

enum ctf_msg_iter_medium_status lttng_live_get_stream_bytes(
		struct lttng_live_msg_iter *lttng_live_msg_iter,
		struct lttng_live_stream_iterator *stream, uint8_t *buf,
		uint64_t offset, uint64_t req_len, uint64_t *recv_len);

bool lttng_live_graph_is_canceled(struct lttng_live_msg_iter *msg_iter);

#endif /* BABELTRACE_PLUGIN_CTF_LTTNG_LIVE_H */
