/*
 * Copyright (C) 2013 - Julien Desfossez <jdesfossez@efficios.com>
 *                      Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>
#include <fcntl.h>
#include <poll.h>

#include <babeltrace/ctf/ctf-index.h>

#include <babeltrace/babeltrace.h>
#include <babeltrace/endian.h>
#include <babeltrace/ctf/events.h>
#include <babeltrace/ctf/callbacks.h>
#include <babeltrace/ctf/iterator.h>

/* for packet_index */
#include <babeltrace/ctf/types.h>

#include <babeltrace/ctf/metadata.h>
#include <babeltrace/ctf-text/types.h>
#include <babeltrace/ctf/events-internal.h>
#include <formats/ctf/events-private.h>

#include <babeltrace/compat/memstream.h>
#include <babeltrace/compat/send.h>
#include <babeltrace/compat/string.h>
#include <babeltrace/compat/mman.h>

#include "lttng-live.h"
#include "lttng-viewer-abi.h"

#define ACTIVE_POLL_DELAY	100	/* ms */

/*
 * Memory allocation zeroed
 */
#define zmalloc(x) calloc(1, x)

#ifndef max_t
#define max_t(type, a, b)	\
	((type) (a) > (type) (b) ? (type) (a) : (type) (b))
#endif

static void ctf_live_packet_seek(struct bt_stream_pos *stream_pos,
		size_t index, int whence);
static int add_traces(struct lttng_live_ctx *ctx);
static int del_traces(gpointer key, gpointer value, gpointer user_data);
static int get_new_metadata(struct lttng_live_ctx *ctx,
		struct lttng_live_viewer_stream *viewer_stream,
		char **metadata_buf);

static
ssize_t lttng_live_recv(int fd, void *buf, size_t len)
{
	ssize_t ret;
	size_t copied = 0, to_copy = len;

	do {
		ret = recv(fd, buf + copied, to_copy, 0);
		if (ret > 0) {
			assert(ret <= to_copy);
			copied += ret;
			to_copy -= ret;
		}
	} while ((ret > 0 && to_copy > 0)
		|| (ret < 0 && errno == EINTR));
	if (ret > 0)
		ret = copied;
	/* ret = 0 means orderly shutdown, ret < 0 is error. */
	return ret;
}

static
ssize_t lttng_live_send(int fd, const void *buf, size_t len)
{
	ssize_t ret;

	do {
		ret = bt_send_nosigpipe(fd, buf, len);
	} while (ret < 0 && errno == EINTR);
	return ret;
}

int lttng_live_connect_viewer(struct lttng_live_ctx *ctx)
{
	struct hostent *host;
	struct sockaddr_in server_addr;
	int ret;

	if (lttng_live_should_quit()) {
		ret = -1;
		goto end;
	}

	host = gethostbyname(ctx->relay_hostname);
	if (!host) {
		fprintf(stderr, "[error] Cannot lookup hostname %s\n",
			ctx->relay_hostname);
		goto error;
	}

	if ((ctx->control_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("Socket");
		goto error;
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(ctx->port);
	server_addr.sin_addr = *((struct in_addr *) host->h_addr);
	memset(&(server_addr.sin_zero), 0, 8);

	if (connect(ctx->control_sock, (struct sockaddr *) &server_addr,
				sizeof(struct sockaddr)) == -1) {
		perror("Connect");
		goto error;
	}

	ret = 0;

end:
	return ret;

error:
	fprintf(stderr, "[error] Connection failed\n");
	return -1;
}

int lttng_live_establish_connection(struct lttng_live_ctx *ctx)
{
	struct lttng_viewer_cmd cmd;
	struct lttng_viewer_connect connect;
	const size_t cmd_buf_len = sizeof(cmd) + sizeof(connect);
	char cmd_buf[cmd_buf_len];
	int ret;
	ssize_t ret_len;

	if (lttng_live_should_quit()) {
		ret = -1;
		goto end;
	}

	cmd.cmd = htobe32(LTTNG_VIEWER_CONNECT);
	cmd.data_size = htobe64((uint64_t) sizeof(connect));
	cmd.cmd_version = htobe32(0);

	connect.viewer_session_id = -1ULL;	/* will be set on recv */
	connect.major = htobe32(LTTNG_LIVE_MAJOR);
	connect.minor = htobe32(LTTNG_LIVE_MINOR);
	connect.type = htobe32(LTTNG_VIEWER_CLIENT_COMMAND);

	/*
	 * Merge the cmd and connection request to prevent a write-write
	 * sequence on the TCP socket. Otherwise, a delayed ACK will prevent the
	 * second write to be performed quickly in presence of Nagle's algorithm.
	 */
	memcpy(cmd_buf, &cmd, sizeof(cmd));
	memcpy(cmd_buf + sizeof(cmd), &connect, sizeof(connect));

	ret_len = lttng_live_send(ctx->control_sock, cmd_buf, cmd_buf_len);
	if (ret_len < 0) {
		perror("[error] Error sending cmd for establishing session");
		goto error;
	}
	assert(ret_len == cmd_buf_len);

	ret_len = lttng_live_recv(ctx->control_sock, &connect, sizeof(connect));
	if (ret_len == 0) {
		fprintf(stderr, "[error] Remote side has closed connection\n");
		goto error;
	}
	if (ret_len < 0) {
		perror("[error] Error receiving version");
		goto error;
	}
	assert(ret_len == sizeof(connect));

	printf_verbose("Received viewer session ID : %" PRIu64 "\n",
			be64toh(connect.viewer_session_id));
	printf_verbose("Relayd version : %u.%u\n", be32toh(connect.major),
			be32toh(connect.minor));

	if (LTTNG_LIVE_MAJOR != be32toh(connect.major)) {
		fprintf(stderr, "[error] Incompatible lttng-relayd protocol\n");
		goto error;
	}
	/* Use the smallest protocol version implemented. */
	if (LTTNG_LIVE_MINOR > be32toh(connect.minor)) {
		ctx->minor =  be32toh(connect.minor);
	} else {
		ctx->minor =  LTTNG_LIVE_MINOR;
	}
	ctx->major = LTTNG_LIVE_MAJOR;
	ret = 0;
end:
	return ret;

error:
	fprintf(stderr, "[error] Unable to establish connection\n");
	return -1;
}

static
void free_session_list(GPtrArray *session_list)
{
	int i;
	struct lttng_live_relay_session *relay_session;

	for (i = 0; i < session_list->len; i++) {
		relay_session = g_ptr_array_index(session_list, i);
		free(relay_session->name);
		free(relay_session->hostname);
	}
	g_ptr_array_free(session_list, TRUE);
}

static
void print_session_list(GPtrArray *session_list, const char *path)
{
	int i;
	struct lttng_live_relay_session *relay_session;

	for (i = 0; i < session_list->len; i++) {
		relay_session = g_ptr_array_index(session_list, i);
		fprintf(LTTNG_LIVE_OUTPUT_FP, "%s/host/%s/%s (timer = %u, "
				"%u stream(s), %u client(s) connected)\n",
				path, relay_session->hostname,
				relay_session->name, relay_session->timer,
				relay_session->streams, relay_session->clients);
	}
}

static
void update_session_list(GPtrArray *session_list, char *hostname,
		char *session_name, uint32_t streams, uint32_t clients,
		uint32_t timer)
{
	int i, found = 0;
	struct lttng_live_relay_session *relay_session;

	for (i = 0; i < session_list->len; i++) {
		relay_session = g_ptr_array_index(session_list, i);
		if ((strncmp(relay_session->hostname, hostname, MAXNAMLEN) == 0) &&
				strncmp(relay_session->name,
					session_name, MAXNAMLEN) == 0) {
			relay_session->streams += streams;
			if (relay_session->clients < clients)
				relay_session->clients = clients;
			found = 1;
			break;
		}
	}
	if (found)
		return;

	relay_session = g_new0(struct lttng_live_relay_session, 1);
	relay_session->hostname = bt_strndup(hostname, MAXNAMLEN);
	relay_session->name = bt_strndup(session_name, MAXNAMLEN);
	relay_session->clients = clients;
	relay_session->streams = streams;
	relay_session->timer = timer;
	g_ptr_array_add(session_list, relay_session);
}

int lttng_live_list_sessions(struct lttng_live_ctx *ctx, const char *path)
{
	struct lttng_viewer_cmd cmd;
	struct lttng_viewer_list_sessions list;
	struct lttng_viewer_session lsession;
	int i, ret, sessions_count, print_list = 0;
	ssize_t ret_len;
	uint64_t session_id;
	GPtrArray *session_list = NULL;

	if (lttng_live_should_quit()) {
		ret = -1;
		goto end;
	}

	if (strlen(ctx->session_name) == 0) {
		print_list = 1;
		session_list = g_ptr_array_new();
	}

	cmd.cmd = htobe32(LTTNG_VIEWER_LIST_SESSIONS);
	cmd.data_size = htobe64((uint64_t) 0);
	cmd.cmd_version = htobe32(0);

	ret_len = lttng_live_send(ctx->control_sock, &cmd, sizeof(cmd));
	if (ret_len < 0) {
		perror("[error] Error sending cmd");
		goto error;
	}
	assert(ret_len == sizeof(cmd));

	ret_len = lttng_live_recv(ctx->control_sock, &list, sizeof(list));
	if (ret_len == 0) {
		fprintf(stderr, "[error] Remote side has closed connection\n");
		goto error;
	}
	if (ret_len < 0) {
		perror("[error] Error receiving session list");
		goto error;
	}
	assert(ret_len == sizeof(list));

	sessions_count = be32toh(list.sessions_count);
	for (i = 0; i < sessions_count; i++) {
		ret_len = lttng_live_recv(ctx->control_sock, &lsession, sizeof(lsession));
		if (ret_len == 0) {
			fprintf(stderr, "[error] Remote side has closed connection\n");
			goto error;
		}
		if (ret_len < 0) {
			perror("[error] Error receiving session");
			goto error;
		}
		assert(ret_len == sizeof(lsession));
		lsession.hostname[LTTNG_VIEWER_HOST_NAME_MAX - 1] = '\0';
		lsession.session_name[LTTNG_VIEWER_NAME_MAX - 1] = '\0';
		session_id = be64toh(lsession.id);

		if (print_list) {
			update_session_list(session_list,
					lsession.hostname,
					lsession.session_name,
					be32toh(lsession.streams),
					be32toh(lsession.clients),
					be32toh(lsession.live_timer));
		} else {
			if ((strncmp(lsession.session_name, ctx->session_name,
				MAXNAMLEN) == 0) && (strncmp(lsession.hostname,
					ctx->traced_hostname, MAXNAMLEN) == 0)) {
				printf_verbose("Reading from session %" PRIu64 "\n",
						session_id);
				g_array_append_val(ctx->session_ids,
						session_id);
			}
		}
	}

	if (print_list) {
		print_session_list(session_list, path);
		free_session_list(session_list);
	}
	ret = 0;
end:
	return ret;

error:
	fprintf(stderr, "[error] Unable to list sessions\n");
	return -1;
}

int lttng_live_ctf_trace_assign(struct lttng_live_viewer_stream *stream,
		uint64_t ctf_trace_id)
{
	struct lttng_live_ctf_trace *trace;
	int ret = 0;

	trace = g_hash_table_lookup(stream->session->ctf_traces,
			&ctf_trace_id);
	if (!trace) {
		trace = g_new0(struct lttng_live_ctf_trace, 1);
		trace->ctf_trace_id = ctf_trace_id;
		printf_verbose("Create trace ctf_trace_id %" PRIu64 "\n", ctf_trace_id);
		BT_INIT_LIST_HEAD(&trace->stream_list);
		g_hash_table_insert(stream->session->ctf_traces,
				&trace->ctf_trace_id,
				trace);
	}
	if (stream->metadata_flag)
		trace->metadata_stream = stream;

	assert(!stream->in_trace);
	stream->in_trace = 1;
	stream->ctf_trace = trace;
	bt_list_add(&stream->trace_stream_node, &trace->stream_list);

	return ret;
}

static
int open_metadata_fp_write(struct lttng_live_viewer_stream *stream,
		char **metadata_buf, size_t *size)
{
	int ret = 0;

	stream->metadata_fp_write =
		babeltrace_open_memstream(metadata_buf, size);
	if (!stream->metadata_fp_write) {
		perror("Metadata open_memstream");
		ret = -1;
	}

	return ret;
}

int lttng_live_attach_session(struct lttng_live_ctx *ctx, uint64_t id)
{
	struct lttng_viewer_cmd cmd;
	struct lttng_viewer_attach_session_request rq;
	struct lttng_viewer_attach_session_response rp;
	struct lttng_viewer_stream stream;
	const size_t cmd_buf_len = sizeof(cmd) + sizeof(rq);
	char cmd_buf[cmd_buf_len];
	int ret, i;
	ssize_t ret_len;

	if (lttng_live_should_quit()) {
		ret = -1;
		goto end;
	}

	cmd.cmd = htobe32(LTTNG_VIEWER_ATTACH_SESSION);
	cmd.data_size = htobe64((uint64_t) sizeof(rq));
	cmd.cmd_version = htobe32(0);

	memset(&rq, 0, sizeof(rq));
	rq.session_id = htobe64(id);
	// TODO: add cmd line parameter to select seek beginning
	// rq.seek = htobe32(LTTNG_VIEWER_SEEK_BEGINNING);
	rq.seek = htobe32(LTTNG_VIEWER_SEEK_LAST);

	/*
	 * Merge the cmd and connection request to prevent a write-write
	 * sequence on the TCP socket. Otherwise, a delayed ACK will prevent the
	 * second write to be performed quickly in presence of Nagle's algorithm.
	 */
	memcpy(cmd_buf, &cmd, sizeof(cmd));
	memcpy(cmd_buf + sizeof(cmd), &rq, sizeof(rq));

	ret_len = lttng_live_send(ctx->control_sock, cmd_buf, cmd_buf_len);
	if (ret_len < 0) {
		perror("[error] Error sending attach command and request");
		goto error;
	}
	assert(ret_len == cmd_buf_len);

	ret_len = lttng_live_recv(ctx->control_sock, &rp, sizeof(rp));
	if (ret_len == 0) {
		fprintf(stderr, "[error] Remote side has closed connection\n");
		goto error;
	}
	if (ret_len < 0) {
		perror("[error] Error receiving attach response");
		goto error;
	}
	assert(ret_len == sizeof(rp));

	switch(be32toh(rp.status)) {
	case LTTNG_VIEWER_ATTACH_OK:
		break;
	case LTTNG_VIEWER_ATTACH_UNK:
		ret = -LTTNG_VIEWER_ATTACH_UNK;
		goto end;
	case LTTNG_VIEWER_ATTACH_ALREADY:
		fprintf(stderr, "[error] There is already a viewer attached to this session\n");
		goto error;
	case LTTNG_VIEWER_ATTACH_NOT_LIVE:
		fprintf(stderr, "[error] Not a live session\n");
		goto error;
	case LTTNG_VIEWER_ATTACH_SEEK_ERR:
		fprintf(stderr, "[error] Wrong seek parameter\n");
		goto error;
	default:
		fprintf(stderr, "[error] Unknown attach return code %u\n",
				be32toh(rp.status));
		goto error;
	}
	if (be32toh(rp.status) != LTTNG_VIEWER_ATTACH_OK) {
		goto error;
	}

	ctx->session->stream_count += be32toh(rp.streams_count);
	/*
	 * When the session is created but not started, we do an active wait
	 * until it starts. It allows the viewer to start processing the trace
	 * as soon as the session starts.
	 */
	if (ctx->session->stream_count == 0) {
		ret = 0;
		goto end;
	}
	printf_verbose("Waiting for %d streams:\n",
		be32toh(rp.streams_count));
	for (i = 0; i < be32toh(rp.streams_count); i++) {
		struct lttng_live_viewer_stream *lvstream;

		lvstream = g_new0(struct lttng_live_viewer_stream, 1);
		ret_len = lttng_live_recv(ctx->control_sock, &stream,
				sizeof(stream));
		if (ret_len == 0) {
			fprintf(stderr, "[error] Remote side has closed connection\n");
			g_free(lvstream);
			goto error;
		}
		if (ret_len < 0) {
			perror("[error] Error receiving stream");
			g_free(lvstream);
			goto error;
		}
		assert(ret_len == sizeof(stream));
		stream.path_name[LTTNG_VIEWER_PATH_MAX - 1] = '\0';
		stream.channel_name[LTTNG_VIEWER_NAME_MAX - 1] = '\0';

		printf_verbose("    stream %" PRIu64 " : %s/%s\n",
				be64toh(stream.id), stream.path_name,
				stream.channel_name);
		lvstream->id = be64toh(stream.id);
		lvstream->session = ctx->session;

		lvstream->mmap_size = 0;
		lvstream->ctf_stream_id = -1ULL;

		if (be32toh(stream.metadata_flag)) {
			lvstream->metadata_flag = 1;
		}
		ret = lttng_live_ctf_trace_assign(lvstream,
				be64toh(stream.ctf_trace_id));
		if (ret < 0) {
			g_free(lvstream);
			goto error;
		}
		bt_list_add(&lvstream->session_stream_node,
			&ctx->session->stream_list);
	}
	ret = 0;
end:
	return ret;

error:
	return -1;
}

/*
 * Ask the relay for new streams.
 *
 * Returns the number of new streams received or a negative value on error.
 */
static
int ask_new_streams(struct lttng_live_ctx *ctx)
{
	int i, ret = 0, nb_streams = 0;
	uint64_t id;

restart:
	for (i = 0; i < ctx->session_ids->len; i++) {
		id = g_array_index(ctx->session_ids, uint64_t, i);
		ret = lttng_live_get_new_streams(ctx, id);
		printf_verbose("Asking for new streams returns %d\n", ret);
		if (lttng_live_should_quit()) {
			ret = -1;
			goto end;
		}
		if (ret < 0) {
			if (ret == -LTTNG_VIEWER_NEW_STREAMS_HUP) {
				printf_verbose("Session %" PRIu64 " closed\n",
						id);
				/*
				 * The streams have already been closed during
				 * the reading, so we only need to get rid of
				 * the trace in our internal table of sessions.
				 */
				g_array_remove_index(ctx->session_ids, i);
				/*
				 * We can't continue iterating on the g_array
				 * after a remove, we have to start again.
				 */
				goto restart;
			} else {
				ret = -1;
				goto end;
			}
		} else {
			nb_streams += ret;
		}
	}
	if (ctx->session_ids->len == 0) {
		/* All sessions are closed. */
		ret = -1;
	} else {
		ret = nb_streams;
	}

end:
	return ret;
}

static
int append_metadata(struct lttng_live_ctx *ctx,
		struct lttng_live_viewer_stream *viewer_stream)
{
	int ret;
	struct lttng_live_viewer_stream *metadata;
	char *metadata_buf = NULL;

	if (!viewer_stream->ctf_trace->handle) {
		printf_verbose("append_metadata: trace handle not ready yet.\n");
		return 0;
	}

	printf_verbose("get_next_index: new metadata needed\n");
	ret = get_new_metadata(ctx, viewer_stream, &metadata_buf);
	if (ret < 0) {
		free(metadata_buf);
		goto error;
	}

	metadata = viewer_stream->ctf_trace->metadata_stream;
	metadata->ctf_trace->metadata_fp =
		babeltrace_fmemopen(metadata_buf,
				metadata->metadata_len, "rb");
	if (!metadata->ctf_trace->metadata_fp) {
		perror("Metadata fmemopen");
		free(metadata_buf);
		ret = -1;
		goto error;
	}
	ret = ctf_append_trace_metadata(
			viewer_stream->ctf_trace->handle->td,
			metadata->ctf_trace->metadata_fp);
	/* We accept empty metadata packets */
	if (ret != 0 && ret != -ENOENT) {
		fprintf(stderr, "[error] Appending metadata\n");
		goto error;
	}
	ret = 0;

error:
	return ret;
}

static
int get_data_packet(struct lttng_live_ctx *ctx,
		struct ctf_stream_pos *pos,
		struct lttng_live_viewer_stream *stream, uint64_t offset,
		uint64_t len)
{
	struct lttng_viewer_cmd cmd;
	struct lttng_viewer_get_packet rq;
	struct lttng_viewer_trace_packet rp;
	const size_t cmd_buf_len = sizeof(cmd) + sizeof(rq);
	char cmd_buf[cmd_buf_len];
	ssize_t ret_len;
	int ret;

retry:
	if (lttng_live_should_quit()) {
		ret = -1;
		goto end;
	}

	cmd.cmd = htobe32(LTTNG_VIEWER_GET_PACKET);
	cmd.data_size = htobe64((uint64_t) sizeof(rq));
	cmd.cmd_version = htobe32(0);

	memset(&rq, 0, sizeof(rq));
	rq.stream_id = htobe64(stream->id);
	rq.offset = htobe64(offset);
	rq.len = htobe32(len);

	/*
	 * Merge the cmd and connection request to prevent a write-write
	 * sequence on the TCP socket. Otherwise, a delayed ACK will prevent the
	 * second write to be performed quickly in presence of Nagle's algorithm.
	 */
	memcpy(cmd_buf, &cmd, sizeof(cmd));
	memcpy(cmd_buf + sizeof(cmd), &rq, sizeof(rq));

	ret_len = lttng_live_send(ctx->control_sock, cmd_buf, cmd_buf_len);
	if (ret_len < 0) {
		perror("[error] Error sending get_data_packet cmd and request");
		goto error;
	}
	assert(ret_len == cmd_buf_len);

	ret_len = lttng_live_recv(ctx->control_sock, &rp, sizeof(rp));
	if (ret_len == 0) {
		fprintf(stderr, "[error] Remote side has closed connection\n");
		goto error;
	}
	if (ret_len < 0) {
		perror("[error] Error receiving data response");
		goto error;
	}
	if (ret_len != sizeof(rp)) {
		fprintf(stderr, "[error] get_data_packet: expected %zu"
				", received %zd\n", sizeof(rp),
				ret_len);
		goto error;
	}

	rp.flags = be32toh(rp.flags);

	switch (be32toh(rp.status)) {
	case LTTNG_VIEWER_GET_PACKET_OK:
		len = be32toh(rp.len);
		printf_verbose("get_data_packet: Ok, packet size : %" PRIu64
				"\n", len);
		break;
	case LTTNG_VIEWER_GET_PACKET_RETRY:
		/* Unimplemented by relay daemon */
		printf_verbose("get_data_packet: retry\n");
		goto error;
	case LTTNG_VIEWER_GET_PACKET_ERR:
		if (rp.flags & LTTNG_VIEWER_FLAG_NEW_METADATA) {
			printf_verbose("get_data_packet: new metadata needed\n");
			ret = append_metadata(ctx, stream);
			if (ret)
				goto error;
		}
		if (rp.flags & LTTNG_VIEWER_FLAG_NEW_STREAM) {
			printf_verbose("get_data_packet: new streams needed\n");
			ret = ask_new_streams(ctx);
			if (ret < 0) {
				goto error;
			} else if (ret > 0) {
				ret = add_traces(ctx);
				if (ret < 0) {
					goto error;
				}
			}
		}
		if (rp.flags & (LTTNG_VIEWER_FLAG_NEW_METADATA
				| LTTNG_VIEWER_FLAG_NEW_STREAM)) {
			goto retry;
		}
		fprintf(stderr, "[error] get_data_packet: error\n");
		goto error;
	case LTTNG_VIEWER_GET_PACKET_EOF:
		ret = -2;
		goto end;
	default:
		printf_verbose("get_data_packet: unknown\n");
		goto error;
	}

	if (len == 0) {
		goto error;
	}

	if (len > stream->mmap_size) {
		uint64_t new_size;

		new_size = max_t(uint64_t, len, stream->mmap_size << 1);
		if (pos->base_mma) {
			/* unmap old base */
			ret = munmap_align(pos->base_mma);
			if (ret) {
				perror("[error] Unable to unmap old base");
				goto error;
			}
			pos->base_mma = NULL;
		}
		pos->base_mma = mmap_align(new_size,
				PROT_READ | PROT_WRITE,
				MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (pos->base_mma == MAP_FAILED) {
			perror("[error] mmap error");
			pos->base_mma = NULL;
			goto error;
		}

		stream->mmap_size = new_size;
		printf_verbose("Expanding stream mmap size to %" PRIu64 " bytes\n",
				stream->mmap_size);
	}

	ret_len = lttng_live_recv(ctx->control_sock,
			mmap_align_addr(pos->base_mma), len);
	if (ret_len == 0) {
		fprintf(stderr, "[error] Remote side has closed connection\n");
		goto error;
	}
	if (ret_len < 0) {
		perror("[error] Error receiving trace packet");
		goto error;
	}
	assert(ret_len == len);
	ret = 0;
end:
	return ret;

error:
	return -1;
}

static
int get_one_metadata_packet(struct lttng_live_ctx *ctx,
		struct lttng_live_viewer_stream *metadata_stream)
{
	uint64_t len = 0;
	int ret;
	struct lttng_viewer_cmd cmd;
	struct lttng_viewer_get_metadata rq;
	struct lttng_viewer_metadata_packet rp;
	char *data = NULL;
	ssize_t ret_len;
	const size_t cmd_buf_len = sizeof(cmd) + sizeof(rq);
	char cmd_buf[cmd_buf_len];

	if (lttng_live_should_quit()) {
		ret = -1;
		goto end;
	}

	rq.stream_id = htobe64(metadata_stream->id);
	cmd.cmd = htobe32(LTTNG_VIEWER_GET_METADATA);
	cmd.data_size = htobe64((uint64_t) sizeof(rq));
	cmd.cmd_version = htobe32(0);

	/*
	 * Merge the cmd and connection request to prevent a write-write
	 * sequence on the TCP socket. Otherwise, a delayed ACK will prevent the
	 * second write to be performed quickly in presence of Nagle's algorithm.
	 */
	memcpy(cmd_buf, &cmd, sizeof(cmd));
	memcpy(cmd_buf + sizeof(cmd), &rq, sizeof(rq));

	printf_verbose("get_metadata for trace_id: %d, ctf_trace_id: %" PRIu64 "\n",
			metadata_stream->ctf_trace->trace_id,
			metadata_stream->ctf_trace->ctf_trace_id);
	ret_len = lttng_live_send(ctx->control_sock, cmd_buf, cmd_buf_len);
	if (ret_len < 0) {
		perror("[error] Error sending get_metadata cmd and request");
		goto error;
	}
	assert(ret_len == cmd_buf_len);

	ret_len = lttng_live_recv(ctx->control_sock, &rp, sizeof(rp));
	if (ret_len == 0) {
		fprintf(stderr, "[error] Remote side has closed connection\n");
		goto error;
	}
	if (ret_len < 0) {
		perror("[error] Error receiving metadata response");
		goto error;
	}
	assert(ret_len == sizeof(rp));

	switch (be32toh(rp.status)) {
		case LTTNG_VIEWER_METADATA_OK:
			printf_verbose("get_metadata : OK\n");
			break;
		case LTTNG_VIEWER_NO_NEW_METADATA:
			printf_verbose("get_metadata : NO NEW\n");
			ret = 0;
			goto end;
		case LTTNG_VIEWER_METADATA_ERR:
			printf_verbose("get_metadata : ERR\n");
			goto error;
		default:
			printf_verbose("get_metadata : UNKNOWN\n");
			goto error;
	}

	len = be64toh(rp.len);
	printf_verbose("Writing %" PRIu64" bytes to metadata\n", len);
	if (len <= 0) {
		goto error;
	}

	data = zmalloc(len);
	if (!data) {
		perror("relay data zmalloc");
		goto error;
	}
	ret_len = lttng_live_recv(ctx->control_sock, data, len);
	if (ret_len == 0) {
		fprintf(stderr, "[error] Remote side has closed connection\n");
		goto error_free_data;
	}
	if (ret_len < 0) {
		perror("[error] Error receiving trace packet");
		goto error_free_data;
	}
	assert(ret_len == len);

	do {
		ret_len = fwrite(data, 1, len,
				metadata_stream->metadata_fp_write);
	} while (ret_len < 0 && errno == EINTR);
	if (ret_len < 0) {
		fprintf(stderr, "[error] Writing in the metadata fp\n");
		goto error_free_data;
	}
	assert(ret_len == len);
	metadata_stream->metadata_len += len;
	free(data);
	ret = len;
end:
	return ret;

error_free_data:
	free(data);
error:
	return -1;
}

/*
 * Return 0 on success, a negative value on error.
 */
static
int get_new_metadata(struct lttng_live_ctx *ctx,
		struct lttng_live_viewer_stream *viewer_stream,
		char **metadata_buf)
{
	int ret = 0;
	struct lttng_live_viewer_stream *metadata_stream;
	size_t size, len_read = 0;

	metadata_stream = viewer_stream->ctf_trace->metadata_stream;
	if (!metadata_stream) {
		fprintf(stderr, "[error] No metadata stream\n");
		ret = -1;
		goto error;
	}
	metadata_stream->metadata_len = 0;
	ret = open_metadata_fp_write(metadata_stream, metadata_buf, &size);
	if (ret < 0) {
		goto error;
	}

	do {
		if (lttng_live_should_quit()) {
			ret = -1;
			goto error;
		}
		/*
		 * get_one_metadata_packet returns the number of bytes
		 * received, 0 when we have received everything, a
		 * negative value on error.
		 */
		ret = get_one_metadata_packet(ctx, metadata_stream);
		if (ret > 0) {
			len_read += ret;
		}
		if (!len_read) {
			(void) poll(NULL, 0, ACTIVE_POLL_DELAY);
		}
		if (ret < 0) {
			break;	/* Stop on error. */
		}
	} while (ret > 0 || !len_read);

	if (babeltrace_close_memstream(metadata_buf, &size,
			metadata_stream->metadata_fp_write)) {
		perror("babeltrace_close_memstream");
	}
	metadata_stream->metadata_fp_write = NULL;

error:
	return ret;
}

/*
 * Assign the fields from a lttng_viewer_index to a packet_index.
 */
static
void lttng_index_to_packet_index(struct lttng_viewer_index *lindex,
		struct packet_index *pindex)
{
	assert(lindex);
	assert(pindex);

	pindex->offset = be64toh(lindex->offset);
	pindex->packet_size = be64toh(lindex->packet_size);
	pindex->content_size = be64toh(lindex->content_size);
	pindex->ts_cycles.timestamp_begin = be64toh(lindex->timestamp_begin);
	pindex->ts_cycles.timestamp_end = be64toh(lindex->timestamp_end);
	pindex->events_discarded = be64toh(lindex->events_discarded);
}

/*
 * Get one index for a stream.
 *
 * Returns 0 on success or a negative value on error.
 */
static
int get_next_index(struct lttng_live_ctx *ctx,
		struct lttng_live_viewer_stream *viewer_stream,
		struct packet_index *index, uint64_t *stream_id)
{
	struct lttng_viewer_cmd cmd;
	struct lttng_viewer_get_next_index rq;
	int ret;
	ssize_t ret_len;
	struct lttng_viewer_index *rp = &viewer_stream->current_index;
	const size_t cmd_buf_len = sizeof(cmd) + sizeof(rq);
	char cmd_buf[cmd_buf_len];

	cmd.cmd = htobe32(LTTNG_VIEWER_GET_NEXT_INDEX);
	cmd.data_size = htobe64((uint64_t) sizeof(rq));
	cmd.cmd_version = htobe32(0);

	memset(&rq, 0, sizeof(rq));
	rq.stream_id = htobe64(viewer_stream->id);

	/*
	 * Merge the cmd and connection request to prevent a write-write
	 * sequence on the TCP socket. Otherwise, a delayed ACK will prevent the
	 * second write to be performed quickly in presence of Nagle's algorithm.
	 */
	memcpy(cmd_buf, &cmd, sizeof(cmd));
	memcpy(cmd_buf + sizeof(cmd), &rq, sizeof(rq));
retry:
	if (lttng_live_should_quit()) {
		ret = -1;
		goto end;
	}
	ret_len = lttng_live_send(ctx->control_sock, &cmd_buf, cmd_buf_len);
	if (ret_len < 0) {
		perror("[error] Error sending get_next_index cmd and request");
		goto error;
	}
	assert(ret_len == cmd_buf_len);

	ret_len = lttng_live_recv(ctx->control_sock, rp, sizeof(*rp));
	if (ret_len == 0) {
		fprintf(stderr, "[error] Remote side has closed connection\n");
		goto error;
	}
	if (ret_len < 0) {
		perror("[error] Error receiving index response");
		goto error;
	}
	assert(ret_len == sizeof(*rp));

	rp->flags = be32toh(rp->flags);

	switch (be32toh(rp->status)) {
	case LTTNG_VIEWER_INDEX_INACTIVE:
		printf_verbose("get_next_index: inactive\n");

		if (index->ts_cycles.timestamp_end ==
				be64toh(rp->timestamp_end)) {
			/* Already seen this timestamp. */
			(void) poll(NULL, 0, ACTIVE_POLL_DELAY);
		}

		memset(index, 0, sizeof(struct packet_index));
		index->ts_cycles.timestamp_end = be64toh(rp->timestamp_end);
		*stream_id = be64toh(rp->stream_id);
		break;
	case LTTNG_VIEWER_INDEX_OK:
		printf_verbose("get_next_index: Ok, need metadata update : %u\n",
				rp->flags & LTTNG_VIEWER_FLAG_NEW_METADATA);
		lttng_index_to_packet_index(rp, index);
		*stream_id = be64toh(rp->stream_id);
		viewer_stream->data_pending = 1;

		if (rp->flags & LTTNG_VIEWER_FLAG_NEW_METADATA) {
			ret = append_metadata(ctx, viewer_stream);
			if (ret)
				goto error;
		}
		if (rp->flags & LTTNG_VIEWER_FLAG_NEW_STREAM) {
			printf_verbose("get_next_index: need new streams\n");
			ret = ask_new_streams(ctx);
			if (ret < 0) {
				goto error;
			} else if (ret > 0) {
				ret = add_traces(ctx);
				if (ret < 0) {
					goto error;
				}
			}
		}
		break;
	case LTTNG_VIEWER_INDEX_RETRY:
		printf_verbose("get_next_index: retry\n");
		(void) poll(NULL, 0, ACTIVE_POLL_DELAY);
		goto retry;
	case LTTNG_VIEWER_INDEX_HUP:
		printf_verbose("get_next_index: stream hung up\n");
		viewer_stream->id = -1ULL;
		index->offset = EOF;
		ctx->session->stream_count--;
		viewer_stream->in_trace = 0;
		bt_list_del(&viewer_stream->trace_stream_node);
		bt_list_del(&viewer_stream->session_stream_node);
		g_free(viewer_stream);
		*stream_id = be64toh(rp->stream_id);
		break;
	case LTTNG_VIEWER_INDEX_ERR:
		fprintf(stderr, "[error] get_next_index: error\n");
		goto error;
	default:
		fprintf(stderr, "[error] get_next_index: unkwown value\n");
		goto error;
	}
	ret = 0;
end:
	return ret;

error:
	return -1;
}

static
void read_packet_header(struct ctf_stream_pos *pos,
		struct ctf_file_stream *file_stream)
{
	int ret;

	/* update trace_packet_header and stream_packet_context */
	if (!(pos->prot & PROT_WRITE) &&
		file_stream->parent.trace_packet_header) {
		/* Read packet header */
		ret = generic_rw(&pos->parent,
				&file_stream->parent.trace_packet_header->p);
		if (ret) {
			pos->offset = EOF;
			fprintf(stderr, "[error] trace packet "
					"header read failed\n");
			goto end;
		}
	}
	if (!(pos->prot & PROT_WRITE) &&
		file_stream->parent.stream_packet_context) {
		/* Read packet context */
		ret = generic_rw(&pos->parent,
				&file_stream->parent.stream_packet_context->p);
		if (ret) {
			pos->offset = EOF;
			fprintf(stderr, "[error] stream packet "
					"context read failed\n");
			goto end;
		}
	}
	pos->data_offset = pos->offset;

end:
	return;
}

/*
 * Handle the seek parameters.
 * Returns 0 if the packet_seek can continue, a positive value to
 * cleanly exit the packet_seek, a negative value on error.
 */
static
int handle_seek_position(size_t index, int whence,
		struct lttng_live_viewer_stream *viewer_stream,
		struct ctf_stream_pos *pos,
		struct ctf_file_stream *file_stream)
{
	int ret = 0;

	switch (whence) {
	case SEEK_CUR:
		ret = 0;
		goto end;
	case SEEK_SET:
		/*
		 * We only allow to seek to 0.
		 */
		if (index != 0) {
			fprintf(stderr, "[error] Arbitrary seek in lttng-live "
					"trace not supported\n");
			pos->offset = EOF;
			ret = -1;
			goto end;
		}

		ret = 0;
		goto end;

	default:
		fprintf(stderr, "[error] Invalid seek parameter\n");
		assert(0);
	}

end:
	return ret;
}

static
void ctf_live_packet_seek(struct bt_stream_pos *stream_pos, size_t index,
		int whence)
{
	struct ctf_stream_pos *pos;
	struct ctf_file_stream *file_stream;
	struct packet_index *prev_index = NULL, *cur_index;
	struct lttng_live_viewer_stream *viewer_stream;
	struct lttng_live_session *session;
	uint64_t stream_id = -1ULL;
	int ret;

	pos = ctf_pos(stream_pos);
	file_stream = container_of(pos, struct ctf_file_stream, pos);
	viewer_stream = (struct lttng_live_viewer_stream *) pos->priv;
	session = viewer_stream->session;

	ret = handle_seek_position(index, whence, viewer_stream, pos,
			file_stream);
	if (ret != 0) {
		ret = -1;
		goto end;
	}

retry:
	switch (pos->packet_index->len) {
	case 0:
		g_array_set_size(pos->packet_index, 1);
		cur_index = &g_array_index(pos->packet_index,
				struct packet_index, 0);
		break;
	case 1:
		g_array_set_size(pos->packet_index, 2);
		prev_index = &g_array_index(pos->packet_index,
				struct packet_index, 0);
		cur_index = &g_array_index(pos->packet_index,
				struct packet_index, 1);
		break;
	case 2:
		g_array_index(pos->packet_index,
			struct packet_index, 0) =
				g_array_index(pos->packet_index,
					struct packet_index, 1);
		prev_index = &g_array_index(pos->packet_index,
				struct packet_index, 0);
		cur_index = &g_array_index(pos->packet_index,
				struct packet_index, 1);
		break;
	default:
		abort();
		break;
	}

	if (viewer_stream->data_pending) {
		lttng_index_to_packet_index(&viewer_stream->current_index, cur_index);
	} else {
		printf_verbose("get_next_index for stream %" PRIu64 "\n", viewer_stream->id);
		ret = get_next_index(session->ctx, viewer_stream, cur_index, &stream_id);
		if (ret < 0) {
			pos->offset = EOF;
			if (!lttng_live_should_quit()) {
				fprintf(stderr, "[error] get_next_index failed\n");
			}
			ret = -1;
			goto end;
		}
		printf_verbose("Index received : packet_size : %" PRIu64
				", offset %" PRIu64 ", content_size %" PRIu64
				", timestamp_end : %" PRIu64 "\n",
				cur_index->packet_size, cur_index->offset,
				cur_index->content_size,
				cur_index->ts_cycles.timestamp_end);

	}

	/*
	 * On the first time we receive an index, the stream_id needs to
	 * be set for the stream in order to use it, we don't want any
	 * data at this stage.
	 */
	if (file_stream->parent.stream_id == -1ULL) {
		/*
		 * Warning: with lttng-tools < 2.4.2, the beacon does not
		 * contain the real stream ID, it is memset to 0, so this
		 * might create a problem when a session has multiple
		 * channels. We can't detect it at this stage, lttng-tools
		 * has to be upgraded to fix this problem.
		 */
		printf_verbose("Assigning stream_id %" PRIu64 "\n",
				stream_id);
		file_stream->parent.stream_id = stream_id;
		viewer_stream->ctf_stream_id = stream_id;

		ret = 0;
		goto end;
	}

	pos->packet_size = cur_index->packet_size;
	pos->content_size = cur_index->content_size;
	pos->mmap_base_offset = 0;
	if (cur_index->offset == EOF) {
		pos->offset = EOF;
	} else {
		pos->offset = 0;
	}

	if (cur_index->content_size == 0) {
		/* Beacon packet index */
		if (file_stream->parent.stream_class) {
			file_stream->parent.cycles_timestamp =
				cur_index->ts_cycles.timestamp_end;
			file_stream->parent.real_timestamp = ctf_get_real_timestamp(
					&file_stream->parent,
					cur_index->ts_cycles.timestamp_end);

			/*
			 * Duplicate the data from the previous index, because
			 * the one we just received is only a beacon with no
			 * relevant information except the timestamp_end. We
			 * don't need to keep this timestamp_end because we already
			 * updated the file_stream timestamps, so we only need
			 * to keep the last real index data as prev_index. That
			 * way, we keep the original prev timestamps and
			 * discarded events counter. This is the same behaviour
			 * as if we were reading a local trace, we would not
			 * have fake indexes between real indexes.
			 */
			memcpy(cur_index, prev_index, sizeof(struct packet_index));
		}
	} else {
		/* Real packet index */
		if (file_stream->parent.stream_class) {
			/* Convert the timestamps and append to the real_index. */
			cur_index->ts_real.timestamp_begin = ctf_get_real_timestamp(
					&file_stream->parent,
					cur_index->ts_cycles.timestamp_begin);
			cur_index->ts_real.timestamp_end = ctf_get_real_timestamp(
					&file_stream->parent,
					cur_index->ts_cycles.timestamp_end);
		}

		ctf_update_current_packet_index(&file_stream->parent,
				prev_index, cur_index);

		/*
		 * We need to check if we are in trace read or called
		 * from packet indexing.  In this last case, the
		 * collection is not there, so we cannot print the
		 * timestamps.
		 */
		if ((&file_stream->parent)->stream_class->trace->parent.collection) {
			ctf_print_discarded_lost(stderr, &file_stream->parent);
		}

		file_stream->parent.cycles_timestamp =
				cur_index->ts_cycles.timestamp_begin;
		file_stream->parent.real_timestamp =
				cur_index->ts_real.timestamp_begin;
	}

	/*
	 * Flush the output between attempts to grab a packet, thus
	 * ensuring we flush at least at the periodical timer period.
	 * This ensures the output remains reactive for interactive users and
	 * that the output is flushed when redirected to a file by the shell.
	 */
	if (fflush(LTTNG_LIVE_OUTPUT_FP) < 0) {
		perror("fflush");
		goto end;
	}

	if (pos->packet_size == 0 || pos->offset == EOF) {
		goto end;
	}

	printf_verbose("get_data_packet for stream %" PRIu64 "\n",
			viewer_stream->id);
	ret = get_data_packet(session->ctx, pos, viewer_stream,
			cur_index->offset,
			cur_index->packet_size / CHAR_BIT);
	if (ret == -2) {
		goto retry;
	} else if (ret < 0) {
		pos->offset = EOF;
		if (!lttng_live_should_quit()) {
			fprintf(stderr, "[error] get_data_packet failed\n");
			ret = -1;
		} else {
			ret = 0;
		}
		goto end;
	}
	viewer_stream->data_pending = 0;

	read_packet_header(pos, file_stream);
	ret = 0;
end:
	bt_packet_seek_set_error(ret);
}

int lttng_live_create_viewer_session(struct lttng_live_ctx *ctx)
{
	struct lttng_viewer_cmd cmd;
	struct lttng_viewer_create_session_response resp;
	int ret;
	ssize_t ret_len;

	if (lttng_live_should_quit()) {
		ret = -1;
		goto end;
	}

	cmd.cmd = htobe32(LTTNG_VIEWER_CREATE_SESSION);
	cmd.data_size = htobe64((uint64_t) 0);
	cmd.cmd_version = htobe32(0);

	ret_len = lttng_live_send(ctx->control_sock, &cmd, sizeof(cmd));
	if (ret_len < 0) {
		perror("[error] Error sending cmd");
		goto error;
	}
	assert(ret_len == sizeof(cmd));

	ret_len = lttng_live_recv(ctx->control_sock, &resp, sizeof(resp));
	if (ret_len == 0) {
		fprintf(stderr, "[error] Remote side has closed connection\n");
		goto error;
	}
	if (ret_len < 0) {
		perror("[error] Error receiving create session reply");
		goto error;
	}
	assert(ret_len == sizeof(resp));

	if (be32toh(resp.status) != LTTNG_VIEWER_CREATE_SESSION_OK) {
		fprintf(stderr, "[error] Error creating viewer session\n");
		goto error;
	}
	ret = 0;
end:
	return ret;

error:
	return -1;
}

static
int del_traces(gpointer key, gpointer value, gpointer user_data)
{
	struct bt_context *bt_ctx = user_data;
	struct lttng_live_ctf_trace *trace = value;
	int ret;
	struct lttng_live_viewer_stream *lvstream, *tmp;

	/*
	 * We don't have ownership of the live viewer stream, just
	 * remove them from our list.
	 */
	bt_list_for_each_entry_safe(lvstream, tmp, &trace->stream_list,
			trace_stream_node) {
		lvstream->in_trace = 0;
		bt_list_del(&lvstream->trace_stream_node);
	}
	if (trace->in_use) {
		ret = bt_context_remove_trace(bt_ctx, trace->trace_id);
		if (ret < 0)
			fprintf(stderr, "[error] removing trace from context\n");
	}

	/* remove the key/value pair from the HT. */
	return 1;
}

static
int add_one_trace(struct lttng_live_ctx *ctx,
		struct lttng_live_ctf_trace *trace)
{
	int ret;
	struct bt_context *bt_ctx = ctx->bt_ctx;
	struct lttng_live_viewer_stream *stream;
	struct bt_mmap_stream *new_mmap_stream;
	struct bt_mmap_stream_list mmap_list;
	struct bt_trace_descriptor *td;
	struct bt_trace_handle *handle;

	printf_verbose("Add one trace ctf_trace_id: %" PRIu64
			" (metadata_stream: %p)\n",
			trace->ctf_trace_id, trace->metadata_stream);
	/*
	 * We don't know how many streams we will receive for a trace, so
	 * once we are done receiving the traces, we add all the traces
	 * received to the bt_context.
	 * We can receive streams during the attach command or the
	 * get_new_streams, so we have to make sure not to add multiple
	 * times the same traces.
	 * If a trace is already in the context, we just skip this function.
	 */
	if (trace->in_use) {
		printf_verbose("Trace already in use\n");
		ret = 0;
		goto end;
	}
	/*
	 * add_one_trace can be called recursively if during the
	 * bt_context_add_trace call we need to fetch new streams, so we need to
	 * prevent a recursive call to process our current trace.
	 */
	trace->in_use = 1;

	BT_INIT_LIST_HEAD(&mmap_list.head);

	bt_list_for_each_entry(stream, &trace->stream_list, trace_stream_node) {
		if (!stream->metadata_flag) {
			new_mmap_stream = zmalloc(sizeof(struct bt_mmap_stream));
			new_mmap_stream->priv = (void *) stream;
			new_mmap_stream->fd = -1;
			bt_list_add(&new_mmap_stream->list, &mmap_list.head);
		} else {
			char *metadata_buf = NULL;

			/* Get all possible metadata before starting */
			ret = get_new_metadata(ctx, stream, &metadata_buf);
			if (ret) {
				free(metadata_buf);
				goto end_free;
			}
			if (!stream->metadata_len) {
				fprintf(stderr, "[error] empty metadata\n");
				ret = -1;
				free(metadata_buf);
				goto end_free;
			}

			printf_verbose("Metadata stream found\n");
			trace->metadata_fp = babeltrace_fmemopen(metadata_buf,
					stream->metadata_len, "rb");
			if (!trace->metadata_fp) {
				perror("Metadata fmemopen");
				ret = -1;
				free(metadata_buf);
				goto end_free;
			}
		}
	}

	if (!trace->metadata_fp) {
		fprintf(stderr, "[error] No metadata stream opened\n");
		ret = -1;
		goto end_free;
	}

	ret = bt_context_add_trace(bt_ctx, NULL, "ctf",
			ctf_live_packet_seek, &mmap_list, trace->metadata_fp);
	if (ret < 0) {
		fprintf(stderr, "[error] Error adding trace\n");
		ret = -1;
		goto end_free;
	}
	trace->metadata_stream->metadata_len = 0;

	handle = (struct bt_trace_handle *) g_hash_table_lookup(
			bt_ctx->trace_handles,
			(gpointer) (unsigned long) ret);
	td = handle->td;
	trace->handle = handle;
	if (bt_ctx->current_iterator) {
		bt_iter_add_trace(bt_ctx->current_iterator, td);
	}

	trace->trace_id = ret;
	printf_verbose("Trace now in use, id = %d\n", trace->trace_id);

	goto end;

end_free:
	bt_context_put(bt_ctx);
end:
	return ret;
}

/*
 * Make sure all the traces we know have a metadata stream or loop on
 * ask_new_streams until it is done. This must be called before we call
 * add_one_trace.
 *
 * Return 0 when all known traces have a metadata stream, a negative value
 * on error.
 */
static
int check_traces_metadata(struct lttng_live_ctx *ctx)
{
	int ret;
	struct lttng_live_ctf_trace *trace;
	GHashTableIter it;
	gpointer key;
	gpointer value;

retry:
	g_hash_table_iter_init(&it, ctx->session->ctf_traces);
	while (g_hash_table_iter_next(&it, &key, &value)) {
		trace = (struct lttng_live_ctf_trace *) value;
		printf_verbose("Check trace %" PRIu64 " metadata\n", trace->ctf_trace_id);
		while (!trace->metadata_stream) {
			printf_verbose("Waiting for metadata stream\n");
			if (lttng_live_should_quit()) {
				ret = 0;
				goto end;
			}
			ret = ask_new_streams(ctx);
			if (ret < 0) {
				goto end;
			} else if (ret == 0) {
				(void) poll(NULL, 0, ACTIVE_POLL_DELAY);
			} else {
				/*
				 * If ask_new_stream got streams from a trace we did not know
				 * about until now, we have to reinitialize the iterator.
				 */
				goto retry;
			}
		}
	}

	ret = 0;

end:
	printf_verbose("End check traces metadata\n");
	return ret;
}

static
int add_traces(struct lttng_live_ctx *ctx)
{
	int ret;
	struct lttng_live_ctf_trace *trace;
	GHashTableIter it;
	gpointer key;
	gpointer value;
	unsigned int nr_traces;

	printf_verbose("Begin add traces\n");

retry:
	nr_traces = g_hash_table_size(ctx->session->ctf_traces);

	ret = check_traces_metadata(ctx);
	if (ret < 0) {
		goto end;
	}

	g_hash_table_iter_init(&it, ctx->session->ctf_traces);
	while (g_hash_table_iter_next(&it, &key, &value)) {
		trace = (struct lttng_live_ctf_trace *) value;
		ret = add_one_trace(ctx, trace);
		if (ret < 0) {
			goto end;
		}
		/*
		 * If a new trace got added while we were adding the trace, the
		 * iterator is invalid and we have to restart.
		 */
		if (g_hash_table_size(ctx->session->ctf_traces) != nr_traces) {
			printf_verbose("New trace(s) added during add_one_trace()\n");
			printf_verbose("JORAJ: GREP HERE\n");
			goto retry;
		}
	}

	ret = 0;

end:
	printf_verbose("End add traces\n");
	return ret;
}

/*
 * Request new streams for a session.
 * Returns the number of streams received or a negative value on error.
 */
int lttng_live_get_new_streams(struct lttng_live_ctx *ctx, uint64_t id)
{
	struct lttng_viewer_cmd cmd;
	struct lttng_viewer_new_streams_request rq;
	struct lttng_viewer_new_streams_response rp;
	struct lttng_viewer_stream stream;
	int ret, i, nb_streams = 0;
	ssize_t ret_len;
	uint32_t stream_count;
	const size_t cmd_buf_len = sizeof(cmd) + sizeof(rq);
	char cmd_buf[cmd_buf_len];

	if (lttng_live_should_quit()) {
		ret = -1;
		goto end;
	}

	cmd.cmd = htobe32(LTTNG_VIEWER_GET_NEW_STREAMS);
	cmd.data_size = htobe64((uint64_t) sizeof(rq));
	cmd.cmd_version = htobe32(0);

	memset(&rq, 0, sizeof(rq));
	rq.session_id = htobe64(id);

	/*
	 * Merge the cmd and connection request to prevent a write-write
	 * sequence on the TCP socket. Otherwise, a delayed ACK will prevent the
	 * second write to be performed quickly in presence of Nagle's algorithm.
	 */
	memcpy(cmd_buf, &cmd, sizeof(cmd));
	memcpy(cmd_buf + sizeof(cmd), &rq, sizeof(rq));

	ret_len = lttng_live_send(ctx->control_sock, cmd_buf, cmd_buf_len);
	if (ret_len < 0) {
		perror("[error] Error sending get_new_streams cmd and request");
		goto error;
	}
	assert(ret_len == cmd_buf_len);

	ret_len = lttng_live_recv(ctx->control_sock, &rp, sizeof(rp));
	if (ret_len == 0) {
		fprintf(stderr, "[error] Remote side has closed connection\n");
		goto error;
	}
	if (ret_len < 0) {
		perror("[error] Error receiving get_new_streams response");
		goto error;
	}
	assert(ret_len == sizeof(rp));

	switch(be32toh(rp.status)) {
	case LTTNG_VIEWER_NEW_STREAMS_OK:
		break;
	case LTTNG_VIEWER_NEW_STREAMS_NO_NEW:
		ret = 0;
		goto end;
	case LTTNG_VIEWER_NEW_STREAMS_HUP:
		ret = -LTTNG_VIEWER_NEW_STREAMS_HUP;
		goto end;
	case LTTNG_VIEWER_NEW_STREAMS_ERR:
		fprintf(stderr, "[error] get_new_streams error\n");
		goto error;
	default:
		fprintf(stderr, "[error] Unknown return code %u\n",
				be32toh(rp.status));
		goto error;
	}

	stream_count = be32toh(rp.streams_count);
	ctx->session->stream_count += stream_count;
	/*
	 * When the session is created but not started, we do an active wait
	 * until it starts. It allows the viewer to start processing the trace
	 * as soon as the session starts.
	 */
	if (ctx->session->stream_count == 0) {
		ret = 0;
		goto end;
	}
	printf_verbose("Waiting for %d streams:\n", stream_count);

	for (i = 0; i < stream_count; i++) {
		struct lttng_live_viewer_stream *lvstream;

		lvstream = g_new0(struct lttng_live_viewer_stream, 1);
		ret_len = lttng_live_recv(ctx->control_sock, &stream,
				sizeof(stream));
		if (ret_len == 0) {
			fprintf(stderr, "[error] Remote side has closed connection\n");
			g_free(lvstream);
			goto error;
		}
		if (ret_len < 0) {
			perror("[error] Error receiving stream");
			g_free(lvstream);
			goto error;
		}
		assert(ret_len == sizeof(stream));
		stream.path_name[LTTNG_VIEWER_PATH_MAX - 1] = '\0';
		stream.channel_name[LTTNG_VIEWER_NAME_MAX - 1] = '\0';

		printf_verbose("    stream %" PRIu64 " : %s/%s\n",
				be64toh(stream.id), stream.path_name,
				stream.channel_name);
		lvstream->id = be64toh(stream.id);
		lvstream->session = ctx->session;

		lvstream->mmap_size = 0;
		lvstream->ctf_stream_id = -1ULL;

		if (be32toh(stream.metadata_flag)) {
			lvstream->metadata_flag = 1;
		}
		ret = lttng_live_ctf_trace_assign(lvstream,
				be64toh(stream.ctf_trace_id));
		if (ret < 0) {
			g_free(lvstream);
			goto error;
		}
		nb_streams++;
		bt_list_add(&lvstream->session_stream_node,
			&ctx->session->stream_list);
	}
	ret = nb_streams;
end:
	return ret;

error:
	return -1;
}

int lttng_live_read(struct lttng_live_ctx *ctx)
{
	int ret = -1;
	int i;
	struct bt_ctf_iter *iter;
	const struct bt_ctf_event *event;
	struct bt_iter_pos begin_pos;
	struct bt_trace_descriptor *td_write;
	struct bt_format *fmt_write;
	struct ctf_text_stream_pos *sout;
	uint64_t id;

	ctx->bt_ctx = bt_context_create();
	if (!ctx->bt_ctx) {
		fprintf(stderr, "[error] bt_context_create allocation\n");
		goto end;
	}

	fmt_write = bt_lookup_format(g_quark_from_static_string("text"));
	if (!fmt_write) {
		fprintf(stderr, "[error] ctf-text error\n");
		goto end_free;
	}

	td_write = fmt_write->open_trace(NULL, O_RDWR, NULL, NULL);
	if (!td_write) {
		fprintf(stderr, "[error] Error opening output trace\n");
		goto end_free;
	}

	sout = container_of(td_write, struct ctf_text_stream_pos,
			trace_descriptor);
	if (!sout->parent.event_cb) {
		goto end_free;
	}

	ret = lttng_live_create_viewer_session(ctx);
	if (ret < 0) {
		goto end_free;
	}

	for (i = 0; i < ctx->session_ids->len; i++) {
		id = g_array_index(ctx->session_ids, uint64_t, i);
		printf_verbose("Attaching to session %" PRIu64 "\n", id);
		ret = lttng_live_attach_session(ctx, id);
		printf_verbose("Attaching session returns %d\n", ret);
		if (ret < 0) {
			if (ret == -LTTNG_VIEWER_ATTACH_UNK) {
				fprintf(stderr, "[error] Unknown session ID\n");
			}
			goto end_free;
		}
	}

	/*
	 * As long as the session is active, we try to get new streams.
	 */
	for (;;) {
		int flags;

		if (lttng_live_should_quit()) {
			ret = 0;
			goto end_free;
		}

		while (!ctx->session->stream_count) {
			if (lttng_live_should_quit()
					|| ctx->session_ids->len == 0) {
				ret = 0;
				goto end_free;
			}
			ret = ask_new_streams(ctx);
			if (ret < 0) {
				ret = 0;
				goto end_free;
			}
			if (!ctx->session->stream_count) {
				(void) poll(NULL, 0, ACTIVE_POLL_DELAY);
			}
		}

		ret = add_traces(ctx);
		if (ret < 0) {
			goto end_free;
		}

		begin_pos.type = BT_SEEK_BEGIN;
		iter = bt_ctf_iter_create(ctx->bt_ctx, &begin_pos, NULL);
		if (!iter) {
			if (lttng_live_should_quit()) {
				ret = 0;
				goto end_free;
			}
			fprintf(stderr, "[error] Iterator creation error\n");
			goto end_free;
		}
		for (;;) {
			if (lttng_live_should_quit()) {
				ret = 0;
				goto end_free;
			}
			event = bt_ctf_iter_read_event_flags(iter, &flags);
			if (!(flags & BT_ITER_FLAG_RETRY)) {
				if (!event) {
					/* End of trace */
					break;
				}
				ret = sout->parent.event_cb(&sout->parent,
						event->parent->stream);
				if (ret) {
					fprintf(stderr, "[error] Writing "
							"event failed.\n");
					goto end_free;
				}
			}
			ret = bt_iter_next(bt_ctf_get_iter(iter));
			if (ret < 0) {
				goto end_free;
			}
		}
		bt_ctf_iter_destroy(iter);
		g_hash_table_foreach_remove(ctx->session->ctf_traces,
				del_traces, ctx->bt_ctx);
		ctx->session->stream_count = 0;
	}

end_free:
	g_hash_table_foreach_remove(ctx->session->ctf_traces,
			del_traces, ctx->bt_ctx);
	bt_context_put(ctx->bt_ctx);
end:
	if (lttng_live_should_quit()) {
		ret = 0;
	}
	return ret;
}
