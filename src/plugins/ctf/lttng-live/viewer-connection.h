#ifndef LTTNG_LIVE_VIEWER_CONNECTION_H
#define LTTNG_LIVE_VIEWER_CONNECTION_H

/*
 * Copyright 2016 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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
#include <stdio.h>

#include <glib.h>

#include <babeltrace2/babeltrace.h>

#include "common/macros.h"
#include "compat/socket.h"

#define LTTNG_DEFAULT_NETWORK_VIEWER_PORT	5344

#define LTTNG_LIVE_MAJOR			2
#define LTTNG_LIVE_MINOR			4

enum lttng_live_viewer_status {
	LTTNG_LIVE_VIEWER_STATUS_OK		= 0,
	LTTNG_LIVE_VIEWER_STATUS_ERROR		= -1,
	LTTNG_LIVE_VIEWER_STATUS_INTERRUPTED	= -2,
};

enum lttng_live_get_one_metadata_status {
	/* The end of the metadata stream was reached. */
	LTTNG_LIVE_GET_ONE_METADATA_STATUS_END		= 1,
	/* One metadata packet was received and written to file. */
	LTTNG_LIVE_GET_ONE_METADATA_STATUS_OK		= LTTNG_LIVE_VIEWER_STATUS_OK,
	/*
	 * A critical error occurred when contacting the relay or while
	 * handling its response.
	 */
	LTTNG_LIVE_GET_ONE_METADATA_STATUS_ERROR	= LTTNG_LIVE_VIEWER_STATUS_ERROR,

	LTTNG_LIVE_GET_ONE_METADATA_STATUS_INTERRUPTED  = LTTNG_LIVE_VIEWER_STATUS_INTERRUPTED,

	/* The metadata stream was not found on the relay. */
	LTTNG_LIVE_GET_ONE_METADATA_STATUS_CLOSED	= -3,
};

struct lttng_live_component;

struct live_viewer_connection {
	bt_logging_level log_level;
	bt_self_component *self_comp;
	bt_self_component_class *self_comp_class;

	GString *url;

	GString *relay_hostname;
	GString *target_hostname;
	GString *session_name;
	GString *proto;

	BT_SOCKET control_sock;
	int port;

	int32_t major;
	int32_t minor;

	bool in_query;
	struct lttng_live_msg_iter *lttng_live_msg_iter;
};

struct packet_index_time {
	uint64_t timestamp_begin;
	uint64_t timestamp_end;
};

struct packet_index {
	off_t offset;		/* offset of the packet in the file, in bytes */
	int64_t data_offset;	/* offset of data within the packet, in bits */
	uint64_t packet_size;	/* packet size, in bits */
	uint64_t content_size;	/* content size, in bits */
	uint64_t events_discarded;
	uint64_t events_discarded_len;	/* length of the field, in bits */
	struct packet_index_time ts_cycles;	/* timestamp in cycles */
	struct packet_index_time ts_real;	/* realtime timestamp */
	/* CTF_INDEX 1.0 limit */
	uint64_t stream_instance_id;	/* ID of the channel instance */
	uint64_t packet_seq_num;	/* packet sequence number */
};

enum lttng_live_viewer_status live_viewer_connection_create(
		bt_self_component *self_comp,
		bt_self_component_class *self_comp_class,
		bt_logging_level log_level,
		const char *url, bool in_query,
		struct lttng_live_msg_iter *lttng_live_msg_iter,
		struct live_viewer_connection **viewer_connection);

void live_viewer_connection_destroy(
		struct live_viewer_connection *conn);

enum lttng_live_viewer_status lttng_live_create_viewer_session(
		struct lttng_live_msg_iter *lttng_live_msg_iter);

bt_component_class_query_method_status live_viewer_connection_list_sessions(
		struct live_viewer_connection *viewer_connection,
		const bt_value **user_result);

#endif /* LTTNG_LIVE_VIEWER_CONNECTION_H */
