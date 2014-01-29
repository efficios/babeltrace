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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <babeltrace/ctf/ctf-index.h>

#include <babeltrace/babeltrace.h>
#include <babeltrace/ctf/events.h>
#include <babeltrace/ctf/callbacks.h>
#include <babeltrace/ctf/iterator.h>

/* for packet_index */
#include <babeltrace/ctf/types.h>

#include <babeltrace/ctf/metadata.h>
#include <babeltrace/ctf-text/types.h>
#include <babeltrace/ctf/events-internal.h>
#include <formats/ctf/events-private.h>

#include "lttng-live-functions.h"
#include "lttng-viewer.h"

/*
 * Memory allocation zeroed
 */
#define zmalloc(x) calloc(1, x)

#ifndef max_t
#define max_t(type, a, b)	\
	((type) (a) > (type) (b) ? (type) (a) : (type) (b))
#endif

int lttng_live_connect_viewer(struct lttng_live_ctx *ctx, char *hostname,
		int port)
{
	struct hostent *host;
	struct sockaddr_in server_addr;
	int ret;

	host = gethostbyname(hostname);
	if (!host) {
		ret = -1;
		goto end;
	}

	if ((ctx->control_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("Socket");
		ret = -1;
		goto end;
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr = *((struct in_addr *) host->h_addr);
	bzero(&(server_addr.sin_zero), 8);

	if (connect(ctx->control_sock, (struct sockaddr *) &server_addr,
				sizeof(struct sockaddr)) == -1) {
		perror("Connect");
		ret = -1;
		goto end;
	}

	ret = 0;

end:
	return ret;
}

int lttng_live_establish_connection(struct lttng_live_ctx *ctx)
{
	struct lttng_viewer_cmd cmd;
	struct lttng_viewer_connect connect;
	int ret;
	ssize_t ret_len;

	cmd.cmd = htobe32(LTTNG_VIEWER_CONNECT);
	cmd.data_size = sizeof(connect);
	cmd.cmd_version = 0;

	connect.viewer_session_id = -1ULL;	/* will be set on recv */
	connect.major = htobe32(LTTNG_LIVE_MAJOR);
	connect.minor = htobe32(LTTNG_LIVE_MINOR);
	connect.type = htobe32(LTTNG_VIEWER_CLIENT_COMMAND);

	do {
		ret_len = send(ctx->control_sock, &cmd, sizeof(cmd), 0);
	} while (ret_len < 0 && errno == EINTR);
	if (ret_len < 0) {
		fprintf(stderr, "[error] Error sending cmd\n");
		ret = ret_len;
		goto error;
	}
	assert(ret_len == sizeof(cmd));

	do {
		ret_len = send(ctx->control_sock, &connect, sizeof(connect), 0);
	} while (ret_len < 0 && errno == EINTR);
	if (ret_len < 0) {
		fprintf(stderr, "[error] Error sending version\n");
		ret = ret_len;
		goto error;
	}
	assert(ret_len == sizeof(connect));

	do {
		ret_len = recv(ctx->control_sock, &connect, sizeof(connect), 0);
	} while (ret_len < 0 && errno == EINTR);
	if (ret_len < 0) {
		fprintf(stderr, "[error] Error receiving version\n");
		ret = ret_len;
		goto error;
	}
	assert(ret_len == sizeof(connect));

	printf_verbose("Received viewer session ID : %" PRIu64 "\n",
			be64toh(connect.viewer_session_id));
	printf_verbose("Relayd version : %u.%u\n", be32toh(connect.major),
			be32toh(connect.minor));

	ret = 0;

error:
	return ret;
}

int lttng_live_list_sessions(struct lttng_live_ctx *ctx, const char *path)
{
	struct lttng_viewer_cmd cmd;
	struct lttng_viewer_list_sessions list;
	struct lttng_viewer_session lsession;
	int i, ret;
	ssize_t ret_len;
	int sessions_count;

	cmd.cmd = htobe32(LTTNG_VIEWER_LIST_SESSIONS);
	cmd.data_size = 0;
	cmd.cmd_version = 0;

	do {
		ret_len = send(ctx->control_sock, &cmd, sizeof(cmd), 0);
	} while (ret_len < 0 && errno == EINTR);
	if (ret_len < 0) {
		fprintf(stderr, "[error] Error sending cmd\n");
		ret = ret_len;
		goto error;
	}
	assert(ret_len == sizeof(cmd));

	do {
		ret_len = recv(ctx->control_sock, &list, sizeof(list), 0);
	} while (ret_len < 0 && errno == EINTR);
	if (ret_len < 0) {
		fprintf(stderr, "[error] Error receiving session list\n");
		ret = ret_len;
		goto error;
	}
	assert(ret_len == sizeof(list));

	sessions_count = be32toh(list.sessions_count);
	fprintf(stdout, "%u active session(s)%c\n", sessions_count,
			sessions_count > 0 ? ':' : ' ');
	for (i = 0; i < sessions_count; i++) {
		do {
			ret_len = recv(ctx->control_sock, &lsession, sizeof(lsession), 0);
		} while (ret_len < 0 && errno == EINTR);
		if (ret_len < 0) {
			fprintf(stderr, "[error] Error receiving session\n");
			ret = ret_len;
			goto error;
		}
		assert(ret_len == sizeof(lsession));
		lsession.hostname[LTTNG_VIEWER_HOST_NAME_MAX - 1] = '\0';
		lsession.session_name[LTTNG_VIEWER_NAME_MAX - 1] = '\0';

		fprintf(stdout, "%s/%" PRIu64 " : %s on host %s (timer = %u, "
				"%u stream(s), %u client(s) connected)\n",
				path, be64toh(lsession.id),
				lsession.session_name, lsession.hostname,
				be32toh(lsession.live_timer),
				be32toh(lsession.streams),
				be32toh(lsession.clients));
	}

	ret = 0;

error:
	return ret;
}

int lttng_live_ctf_trace_assign(struct lttng_live_viewer_stream *stream,
		uint64_t ctf_trace_id)
{
	struct lttng_live_ctf_trace *trace;
	int ret = 0;

	trace = g_hash_table_lookup(stream->session->ctf_traces,
			(gpointer) ctf_trace_id);
	if (!trace) {
		trace = g_new0(struct lttng_live_ctf_trace, 1);
		trace->ctf_trace_id = ctf_trace_id;
		trace->streams = g_ptr_array_new();
		g_hash_table_insert(stream->session->ctf_traces,
				(gpointer) ctf_trace_id,
				trace);
	}
	if (stream->metadata_flag)
		trace->metadata_stream = stream;

	stream->ctf_trace = trace;
	g_ptr_array_add(trace->streams, stream);

	return ret;
}

int lttng_live_attach_session(struct lttng_live_ctx *ctx, uint64_t id)
{
	struct lttng_viewer_cmd cmd;
	struct lttng_viewer_attach_session_request rq;
	struct lttng_viewer_attach_session_response rp;
	struct lttng_viewer_stream stream;
	int ret, i;
	ssize_t ret_len;

	cmd.cmd = htobe32(LTTNG_VIEWER_ATTACH_SESSION);
	cmd.data_size = sizeof(rq);
	cmd.cmd_version = 0;

	memset(&rq, 0, sizeof(rq));
	rq.session_id = htobe64(id);
	// TODO: add cmd line parameter to select seek beginning
	// rq.seek = htobe32(LTTNG_VIEWER_SEEK_BEGINNING);
	rq.seek = htobe32(LTTNG_VIEWER_SEEK_LAST);

	do {
		ret_len = send(ctx->control_sock, &cmd, sizeof(cmd), 0);
	} while (ret_len < 0 && errno == EINTR);
	if (ret_len < 0) {
		fprintf(stderr, "[error] Error sending cmd\n");
		ret = ret_len;
		goto error;
	}
	assert(ret_len == sizeof(cmd));

	do {
		ret_len = send(ctx->control_sock, &rq, sizeof(rq), 0);
	} while (ret_len < 0 && errno == EINTR);
	if (ret_len < 0) {
		fprintf(stderr, "[error] Error sending attach request\n");
		ret = ret_len;
		goto error;
	}
	assert(ret_len == sizeof(rq));

	do {
		ret_len = recv(ctx->control_sock, &rp, sizeof(rp), 0);
	} while (ret_len < 0 && errno == EINTR);
	if (ret_len < 0) {
		fprintf(stderr, "[error] Error receiving attach response\n");
		ret = ret_len;
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
		fprintf(stderr, "[error] Already a viewer attached\n");
		ret = -1;
		goto end;
	case LTTNG_VIEWER_ATTACH_NOT_LIVE:
		fprintf(stderr, "[error] Not a live session\n");
		ret = -1;
		goto end;
	case LTTNG_VIEWER_ATTACH_SEEK_ERR:
		fprintf(stderr, "[error] Wrong seek parameter\n");
		ret = -1;
		goto end;
	default:
		fprintf(stderr, "[error] Unknown attach return code %u\n",
				be32toh(rp.status));
		ret = -1;
		goto end;
	}
	if (be32toh(rp.status) != LTTNG_VIEWER_ATTACH_OK) {
		ret = -1;
		goto end;
	}

	ctx->session->stream_count = be32toh(rp.streams_count);
	/*
	 * When the session is created but not started, we do an active wait
	 * until it starts. It allows the viewer to start processing the trace
	 * as soon as the session starts.
	 */
	if (ctx->session->stream_count == 0) {
		ret = 0;
		goto end;
	}
	printf_verbose("Waiting for %" PRIu64 " streams:\n",
		ctx->session->stream_count);
	ctx->session->streams = g_new0(struct lttng_live_viewer_stream,
			ctx->session->stream_count);
	for (i = 0; i < be32toh(rp.streams_count); i++) {
		do {
			ret_len = recv(ctx->control_sock, &stream, sizeof(stream), 0);
		} while (ret_len < 0 && errno == EINTR);
		if (ret_len < 0) {
			fprintf(stderr, "[error] Error receiving stream\n");
			ret = ret_len;
			goto error;
		}
		assert(ret_len == sizeof(stream));
		stream.path_name[LTTNG_VIEWER_PATH_MAX - 1] = '\0';
		stream.channel_name[LTTNG_VIEWER_NAME_MAX - 1] = '\0';

		printf_verbose("    stream %" PRIu64 " : %s/%s\n",
				be64toh(stream.id), stream.path_name,
				stream.channel_name);
		ctx->session->streams[i].id = be64toh(stream.id);
		ctx->session->streams[i].session = ctx->session;

		ctx->session->streams[i].first_read = 1;
		ctx->session->streams[i].mmap_size = 0;

		if (be32toh(stream.metadata_flag)) {
			char *path;

			path = strdup(LTTNG_METADATA_PATH_TEMPLATE);
			if (!path) {
				perror("strdup");
				ret = -1;
				goto error;
			}
			if (!mkdtemp(path)) {
				perror("mkdtemp");
				free(path);
				ret = -1;
				goto error;
			}
			ctx->session->streams[i].metadata_flag = 1;
			snprintf(ctx->session->streams[i].path,
					sizeof(ctx->session->streams[i].path),
					"%s/%s", path,
					stream.channel_name);
			ret = open(ctx->session->streams[i].path,
					O_WRONLY | O_CREAT | O_TRUNC,
					S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
			if (ret < 0) {
				perror("open");
				free(path);
				goto error;
			}
			ctx->session->streams[i].fd = ret;
			free(path);
		}
		ret = lttng_live_ctf_trace_assign(&ctx->session->streams[i],
				be64toh(stream.ctf_trace_id));
		if (ret < 0) {
			goto error;
		}

	}
	ret = 0;

end:
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
	ssize_t ret_len;
	int ret;

	cmd.cmd = htobe32(LTTNG_VIEWER_GET_PACKET);
	cmd.data_size = sizeof(rq);
	cmd.cmd_version = 0;

	memset(&rq, 0, sizeof(rq));
	rq.stream_id = htobe64(stream->id);
	/* Already in big endian. */
	rq.offset = offset;
	rq.len = htobe32(len);

	do {
		ret_len = send(ctx->control_sock, &cmd, sizeof(cmd), 0);
	} while (ret_len < 0 && errno == EINTR);
	if (ret_len < 0) {
		fprintf(stderr, "[error] Error sending cmd\n");
		ret = ret_len;
		goto error;
	}
	assert(ret_len == sizeof(cmd));

	do {
		ret_len = send(ctx->control_sock, &rq, sizeof(rq), 0);
	} while (ret_len < 0 && errno == EINTR);
	if (ret_len < 0) {
		fprintf(stderr, "[error] Error sending get_data_packet request\n");
		ret = ret_len;
		goto error;
	}
	assert(ret_len == sizeof(rq));

	do {
		ret_len = recv(ctx->control_sock, &rp, sizeof(rp), 0);
	} while (ret_len < 0 && errno == EINTR);
	if (ret_len < 0) {
		fprintf(stderr, "[error] Error receiving data response\n");
		ret = ret_len;
		goto error;
	}
	if (ret_len != sizeof(rp)) {
		fprintf(stderr, "[error] get_data_packet: expected %" PRId64
				", received %" PRId64 "\n", ret_len,
				sizeof(rp));
		ret = -1;
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
		printf_verbose("get_data_packet: retry\n");
		ret = -1;
		goto end;
	case LTTNG_VIEWER_GET_PACKET_ERR:
		if (rp.flags & LTTNG_VIEWER_FLAG_NEW_METADATA) {
			printf_verbose("get_data_packet: new metadata needed\n");
			ret = 0;
			goto end;
		}
		fprintf(stderr, "[error] get_data_packet: error\n");
		ret = -1;
		goto end;
	case LTTNG_VIEWER_GET_PACKET_EOF:
		ret = -2;
		goto error;
	default:
		printf_verbose("get_data_packet: unknown\n");
		assert(0);
		ret = -1;
		goto end;
	}

	if (len <= 0) {
		ret = -1;
		goto end;
	}

	if (len > stream->mmap_size) {
		uint64_t new_size;

		new_size = max_t(uint64_t, len, stream->mmap_size << 1);
		if (pos->base_mma) {
			/* unmap old base */
			ret = munmap_align(pos->base_mma);
			if (ret) {
				fprintf(stderr, "[error] Unable to unmap old base: %s.\n",
					strerror(errno));
				ret = -1;
				goto error;
			}
			pos->base_mma = NULL;
		}
		pos->base_mma = mmap_align(new_size,
				PROT_READ | PROT_WRITE,
				MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (pos->base_mma == MAP_FAILED) {
			fprintf(stderr, "[error] mmap error %s.\n",
				strerror(errno));
			pos->base_mma = NULL;
			ret = -1;
			goto error;
		}

		stream->mmap_size = new_size;
		printf_verbose("Expanding stream mmap size to %" PRIu64 " bytes\n",
				stream->mmap_size);
	}

	do {
		ret_len = recv(ctx->control_sock,
			mmap_align_addr(pos->base_mma), len,
			MSG_WAITALL);
	} while (ret_len < 0 && errno == EINTR);
	if (ret_len < 0) {
		fprintf(stderr, "[error] Error receiving trace packet\n");
		ret = ret_len;
		goto error;
	}
	assert(ret_len == len);

end:
error:
	return ret;
}

/*
 * Return number of metadata bytes written or a negative value on error.
 */
static
int get_new_metadata(struct lttng_live_ctx *ctx,
		struct lttng_live_viewer_stream *viewer_stream,
		uint64_t *metadata_len)
{
	uint64_t len = 0;
	int ret;
	struct lttng_viewer_cmd cmd;
	struct lttng_viewer_get_metadata rq;
	struct lttng_viewer_metadata_packet rp;
	struct lttng_live_viewer_stream *metadata_stream;
	char *data = NULL;
	ssize_t ret_len;

	cmd.cmd = htobe32(LTTNG_VIEWER_GET_METADATA);
	cmd.data_size = sizeof(rq);
	cmd.cmd_version = 0;

	metadata_stream = viewer_stream->ctf_trace->metadata_stream;
	rq.stream_id = htobe64(metadata_stream->id);

	do {
		ret_len = send(ctx->control_sock, &cmd, sizeof(cmd), 0);
	} while (ret_len < 0 && errno == EINTR);
	if (ret_len < 0) {
		fprintf(stderr, "[error] Error sending cmd\n");
		ret = ret_len;
		goto error;
	}
	assert(ret_len == sizeof(cmd));

	do {
		ret_len = send(ctx->control_sock, &rq, sizeof(rq), 0);
	} while (ret_len < 0 && errno == EINTR);
	if (ret_len < 0) {
		fprintf(stderr, "[error] Error sending get_metadata request\n");
		ret = ret_len;
		goto error;
	}
	assert(ret_len == sizeof(rq));

	do {
		ret_len = recv(ctx->control_sock, &rp, sizeof(rp), 0);
	} while (ret_len < 0 && errno == EINTR);
	if (ret_len < 0) {
		fprintf(stderr, "[error] Error receiving metadata response\n");
		ret = ret_len;
		goto error;
	}
	assert(ret_len == sizeof(rp));

	switch (be32toh(rp.status)) {
		case LTTNG_VIEWER_METADATA_OK:
			printf_verbose("get_metadata : OK\n");
			break;
		case LTTNG_VIEWER_NO_NEW_METADATA:
			printf_verbose("get_metadata : NO NEW\n");
			ret = -1;
			goto end;
		case LTTNG_VIEWER_METADATA_ERR:
			printf_verbose("get_metadata : ERR\n");
			ret = -1;
			goto end;
		default:
			printf_verbose("get_metadata : UNKNOWN\n");
			ret = -1;
			goto end;
	}

	len = be64toh(rp.len);
	printf_verbose("Writing %" PRIu64" bytes to metadata\n", len);
	if (len <= 0) {
		ret = -1;
		goto end;
	}

	data = zmalloc(len);
	if (!data) {
		perror("relay data zmalloc");
		ret = -1;
		goto error;
	}
	do {
		ret_len = recv(ctx->control_sock, data, len, MSG_WAITALL);
	} while (ret_len < 0 && errno == EINTR);
	if (ret_len < 0) {
		fprintf(stderr, "[error] Error receiving trace packet\n");
		ret = ret_len;
		free(data);
		goto error;
	}
	assert(ret_len == len);

	do {
		ret_len = write(metadata_stream->fd, data, len);
	} while (ret_len < 0 && errno == EINTR);
	if (ret_len < 0) {
		free(data);
		ret = ret_len;
		goto error;
	}
	assert(ret_len == len);

	free(data);

	*metadata_len = len;
	ret = 0;
end:
error:
	return ret;
}

/*
 * Get one index for a stream.
 *
 * Returns 0 on success or a negative value on error.
 */
static
int get_next_index(struct lttng_live_ctx *ctx,
		struct lttng_live_viewer_stream *viewer_stream,
		struct packet_index *index)
{
	struct lttng_viewer_cmd cmd;
	struct lttng_viewer_get_next_index rq;
	struct lttng_viewer_index rp;
	int ret;
	uint64_t metadata_len;
	ssize_t ret_len;

	cmd.cmd = htobe32(LTTNG_VIEWER_GET_NEXT_INDEX);
	cmd.data_size = sizeof(rq);
	cmd.cmd_version = 0;

	memset(&rq, 0, sizeof(rq));
	rq.stream_id = htobe64(viewer_stream->id);

retry:
	do {
		ret_len = send(ctx->control_sock, &cmd, sizeof(cmd), 0);
	} while (ret_len < 0 && errno == EINTR);
	if (ret_len < 0) {
		fprintf(stderr, "[error] Error sending cmd\n");
		ret = ret_len;
		goto error;
	}
	assert(ret_len == sizeof(cmd));

	do {
		ret_len = send(ctx->control_sock, &rq, sizeof(rq), 0);
	} while (ret_len < 0 && errno == EINTR);
	if (ret_len < 0) {
		fprintf(stderr, "[error] Error sending get_next_index request\n");
		ret = ret_len;
		goto error;
	}
	assert(ret_len == sizeof(rq));

	do {
		ret_len = recv(ctx->control_sock, &rp, sizeof(rp), 0);
	} while (ret_len < 0 && errno == EINTR);
	if (ret_len < 0) {
		fprintf(stderr, "[error] Error receiving index response\n");
		ret = ret_len;
		goto error;
	}
	assert(ret_len == sizeof(rp));

	rp.flags = be32toh(rp.flags);

	switch (be32toh(rp.status)) {
	case LTTNG_VIEWER_INDEX_INACTIVE:
		printf_verbose("get_next_index: inactive\n");
		memset(index, 0, sizeof(struct packet_index));
		index->ts_cycles.timestamp_end = be64toh(rp.timestamp_end);
		break;
	case LTTNG_VIEWER_INDEX_OK:
		printf_verbose("get_next_index: Ok, need metadata update : %u\n",
				rp.flags & LTTNG_VIEWER_FLAG_NEW_METADATA);
		index->offset = be64toh(rp.offset);
		index->packet_size = be64toh(rp.packet_size);
		index->content_size = be64toh(rp.content_size);
		index->ts_cycles.timestamp_begin = be64toh(rp.timestamp_begin);
		index->ts_cycles.timestamp_end = be64toh(rp.timestamp_end);
		index->events_discarded = be64toh(rp.events_discarded);

		if (rp.flags & LTTNG_VIEWER_FLAG_NEW_METADATA) {
			printf_verbose("get_next_index: new metadata needed\n");
			ret = get_new_metadata(ctx, viewer_stream,
					&metadata_len);
			if (ret < 0) {
				goto error;
			}
		}
		break;
	case LTTNG_VIEWER_INDEX_RETRY:
		printf_verbose("get_next_index: retry\n");
		sleep(1);
		goto retry;
	case LTTNG_VIEWER_INDEX_HUP:
		printf_verbose("get_next_index: stream hung up\n");
		viewer_stream->id = -1ULL;
		viewer_stream->fd = -1;
		index->offset = EOF;
		break;
	case LTTNG_VIEWER_INDEX_ERR:
		fprintf(stderr, "[error] get_next_index: error\n");
		ret = -1;
		goto error;
	default:
		fprintf(stderr, "[error] get_next_index: unkwown value\n");
		ret = -1;
		goto error;
	}

	ret = 0;

error:
	return ret;
}

void ctf_live_packet_seek(struct bt_stream_pos *stream_pos, size_t index,
		int whence)
{
	struct ctf_stream_pos *pos;
	struct ctf_file_stream *file_stream;
	struct packet_index *prev_index = NULL, *cur_index;
	struct lttng_live_viewer_stream *viewer_stream;
	struct lttng_live_session *session;
	int ret;

retry:
	pos = ctf_pos(stream_pos);
	file_stream = container_of(pos, struct ctf_file_stream, pos);
	viewer_stream = (struct lttng_live_viewer_stream *) pos->priv;
	session = viewer_stream->session;

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
	printf_verbose("get_next_index for stream %" PRIu64 "\n", viewer_stream->id);
	ret = get_next_index(session->ctx, viewer_stream, cur_index);
	if (ret < 0) {
		pos->offset = EOF;
		fprintf(stderr, "[error] get_next_index failed\n");
		return;
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
		file_stream->parent.cycles_timestamp =
				cur_index->ts_cycles.timestamp_end;
		file_stream->parent.real_timestamp = ctf_get_real_timestamp(
				&file_stream->parent,
				cur_index->ts_cycles.timestamp_end);
	} else {
		/* Convert the timestamps and append to the real_index. */
		cur_index->ts_real.timestamp_begin = ctf_get_real_timestamp(
				&file_stream->parent,
				cur_index->ts_cycles.timestamp_begin);
		cur_index->ts_real.timestamp_end = ctf_get_real_timestamp(
				&file_stream->parent,
				cur_index->ts_cycles.timestamp_end);

		ctf_update_current_packet_index(&file_stream->parent,
				prev_index, cur_index);

		file_stream->parent.cycles_timestamp =
				cur_index->ts_cycles.timestamp_begin;
		file_stream->parent.real_timestamp =
				cur_index->ts_real.timestamp_begin;
	}

	if (pos->packet_size == 0 || pos->offset == EOF) {
		goto end;
	}

	printf_verbose("get_data_packet for stream %" PRIu64 "\n",
			viewer_stream->id);
	ret = get_data_packet(session->ctx, pos, viewer_stream,
			be64toh(cur_index->offset),
			cur_index->packet_size / CHAR_BIT);
	if (ret == -2) {
		goto retry;
	} else if (ret < 0) {
		pos->offset = EOF;
		fprintf(stderr, "[error] get_data_packet failed\n");
		return;
	}

	printf_verbose("Index received : packet_size : %" PRIu64
			", offset %" PRIu64 ", content_size %" PRIu64
			", timestamp_end : %" PRIu64 "\n",
			cur_index->packet_size, cur_index->offset,
			cur_index->content_size,
			cur_index->ts_cycles.timestamp_end);

	/* update trace_packet_header and stream_packet_context */
	if (pos->prot != PROT_WRITE && file_stream->parent.trace_packet_header) {
		/* Read packet header */
		ret = generic_rw(&pos->parent, &file_stream->parent.trace_packet_header->p);
		if (ret) {
			pos->offset = EOF;
			fprintf(stderr, "[error] trace packet header read failed\n");
			goto end;
		}
	}
	if (pos->prot != PROT_WRITE && file_stream->parent.stream_packet_context) {
		/* Read packet context */
		ret = generic_rw(&pos->parent, &file_stream->parent.stream_packet_context->p);
		if (ret) {
			pos->offset = EOF;
			fprintf(stderr, "[error] stream packet context read failed\n");
			goto end;
		}
	}
	pos->data_offset = pos->offset;

end:
	return;
}

static int del_traces(gpointer key, gpointer value, gpointer user_data)
{
	struct bt_context *bt_ctx = user_data;
	struct lttng_live_ctf_trace *trace = value;
	int ret;

	ret = bt_context_remove_trace(bt_ctx, trace->trace_id);
	if (ret < 0)
		fprintf(stderr, "[error] removing trace from context\n");

	/* remove the key/value pair from the HT. */
	return 1;
}

static void add_traces(gpointer key, gpointer value, gpointer user_data)
{
	int i, ret, total_metadata = 0;
	uint64_t metadata_len;
	struct bt_context *bt_ctx = user_data;
	struct lttng_live_ctf_trace *trace = value;
	struct lttng_live_viewer_stream *stream;
	struct bt_mmap_stream *new_mmap_stream;
	struct bt_mmap_stream_list mmap_list;
	struct lttng_live_ctx *ctx = NULL;

	BT_INIT_LIST_HEAD(&mmap_list.head);

	for (i = 0; i < trace->streams->len; i++) {
		stream = g_ptr_array_index(trace->streams, i);
		ctx = stream->session->ctx;

		if (!stream->metadata_flag) {
			new_mmap_stream = zmalloc(sizeof(struct bt_mmap_stream));
			new_mmap_stream->priv = (void *) stream;
			new_mmap_stream->fd = -1;
			bt_list_add(&new_mmap_stream->list, &mmap_list.head);
		} else {
			/* Get all possible metadata before starting */
			do {
				ret = get_new_metadata(ctx, stream,
						&metadata_len);
				if (ret == 0) {
					total_metadata += metadata_len;
				}
			} while (ret == 0 || total_metadata == 0);
			trace->metadata_fp = fopen(stream->path, "r");
		}
	}

	if (!trace->metadata_fp) {
		fprintf(stderr, "[error] No metadata stream opened\n");
		goto end_free;
	}

	ret = bt_context_add_trace(bt_ctx, NULL, "ctf",
			ctf_live_packet_seek, &mmap_list, trace->metadata_fp);
	if (ret < 0) {
		fprintf(stderr, "[error] Error adding trace\n");
		goto end_free;
	}
	trace->trace_id = ret;

	goto end;

end_free:
	bt_context_put(bt_ctx);
end:
	return;
}

void lttng_live_read(struct lttng_live_ctx *ctx, uint64_t session_id)
{
	int ret, active_session = 0;
	struct bt_context *bt_ctx;
	struct bt_ctf_iter *iter;
	const struct bt_ctf_event *event;
	struct bt_iter_pos begin_pos;
	struct bt_trace_descriptor *td_write;
	struct bt_format *fmt_write;
	struct ctf_text_stream_pos *sout;

	bt_ctx = bt_context_create();
	if (!bt_ctx) {
		fprintf(stderr, "[error] bt_context_create allocation\n");
		goto end;
	}

	fmt_write = bt_lookup_format(g_quark_from_static_string("text"));
	if (!fmt_write) {
		fprintf(stderr, "[error] ctf-text error\n");
		goto end;
	}

	td_write = fmt_write->open_trace(NULL, O_RDWR, NULL, NULL);
	if (!td_write) {
		fprintf(stderr, "[error] Error opening output trace\n");
		goto end_free;
	}

	sout = container_of(td_write, struct ctf_text_stream_pos,
			trace_descriptor);
	if (!sout->parent.event_cb)
		goto end_free;

	/*
	 * As long as the session is active, we try to reattach to it,
	 * even if all the streams get closed.
	 */
	do {
		int flags;

		do {
			ret = lttng_live_attach_session(ctx, session_id);
			printf_verbose("Attaching session returns %d\n", ret);
			if (ret < 0) {
				if (ret == -LTTNG_VIEWER_ATTACH_UNK) {
					if (active_session)
						goto end_free;
					fprintf(stderr, "[error] Unknown "
							"session ID\n");
				}
				goto end_free;
			} else {
				active_session = 1;
			}
		} while (ctx->session->stream_count == 0);

		g_hash_table_foreach(ctx->session->ctf_traces, add_traces, bt_ctx);

		begin_pos.type = BT_SEEK_BEGIN;
		iter = bt_ctf_iter_create(bt_ctx, &begin_pos, NULL);
		if (!iter) {
			fprintf(stderr, "[error] Iterator creation error\n");
			goto end;
		}
		for (;;) {
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
		g_hash_table_foreach_remove(ctx->session->ctf_traces, del_traces, bt_ctx);
	} while (active_session);

end_free:
	bt_context_put(bt_ctx);
end:
	return;
}
