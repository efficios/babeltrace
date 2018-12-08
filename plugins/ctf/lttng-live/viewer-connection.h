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

#include <stdio.h>
#include <glib.h>

#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/compat/socket-internal.h>

//TODO: this should not be used by plugins. Should copy code into plugin
//instead.
#include <babeltrace/object-internal.h>

#define LTTNG_DEFAULT_NETWORK_VIEWER_PORT	5344

#define LTTNG_LIVE_MAJOR			2
#define LTTNG_LIVE_MINOR			4

struct lttng_live_component;

struct bt_live_viewer_connection {
	bt_object obj;

	GString *url;

	GString *relay_hostname;
	GString *target_hostname;
	GString *session_name;

	BT_SOCKET control_sock;
	int port;

	int32_t major;
	int32_t minor;

	struct lttng_live_component *lttng_live;
};

struct packet_index_time {
	int64_t timestamp_begin;
	int64_t timestamp_end;
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

struct bt_live_viewer_connection *
	bt_live_viewer_connection_create(const char *url, struct lttng_live_component *lttng_live);

void bt_live_viewer_connection_destroy(struct bt_live_viewer_connection *conn);

bt_value *bt_live_viewer_connection_list_sessions(struct bt_live_viewer_connection *viewer_connection);

#endif /* LTTNG_LIVE_VIEWER_CONNECTION_H */
