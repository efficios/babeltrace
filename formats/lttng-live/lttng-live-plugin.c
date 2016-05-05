/*
 * BabelTrace - LTTng live Output
 *
 * Copyright 2013 Julien Desfossez <jdesfossez@efficios.com>
 *                Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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

#include <babeltrace/ctf-text/types.h>
#include <babeltrace/format.h>
#include <babeltrace/babeltrace-internal.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <glib.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include "lttng-live.h"

static volatile int should_quit;

int lttng_live_should_quit(void)
{
	return should_quit;
}

static
void sighandler(int sig)
{
	switch (sig) {
	case SIGTERM:
	case SIGINT:
		should_quit = 1;
		break;
	default:
		break;
	}
}

/*
 * TODO: Eventually, this signal handler setup should be done at the
 * plugin manager level, rather than within this plugin. Beware, we are
 * not cleaning up the signal handler after plugin execution.
 */
static
int setup_sighandler(void)
{
	struct sigaction sa;
	sigset_t sigset;
	int ret;

	if ((ret = sigemptyset(&sigset)) < 0) {
		perror("sigemptyset");
		return ret;
	}
	sa.sa_handler = sighandler;
	sa.sa_mask = sigset;
	sa.sa_flags = 0;
	if ((ret = sigaction(SIGTERM, &sa, NULL)) < 0) {
		perror("sigaction");
		return ret;
	}
	if ((ret = sigaction(SIGINT, &sa, NULL)) < 0) {
		perror("sigaction");
		return ret;
	}
	return 0;
}

/*
 * hostname parameter needs to hold MAXNAMLEN chars.
 */
static
int parse_url(const char *path, struct lttng_live_ctx *ctx)
{
	char remain[3][MAXNAMLEN];
	int ret = -1, proto, proto_offset = 0;
	size_t path_len = strlen(path);	/* not accounting \0 */

	/*
	 * Since sscanf API does not allow easily checking string length
	 * against a size defined by a macro. Test it beforehand on the
	 * input. We know the output is always <= than the input length.
	 */
	if (path_len >= MAXNAMLEN) {
		goto end;
	}
	ret = sscanf(path, "net%d://", &proto);
	if (ret < 1) {
		proto = 4;
		/* net:// */
		proto_offset = strlen("net://");
	} else {
		/* net4:// or net6:// */
		proto_offset = strlen("netX://");
	}
	if (proto_offset > path_len) {
		goto end;
	}
	if (proto == 6) {
		fprintf(stderr, "[error] IPv6 is currently unsupported by lttng-live\n");
		goto end;
	}
	/* TODO : parse for IPv6 as well */
	/* Parse the hostname or IP */
	ret = sscanf(&path[proto_offset], "%[a-zA-Z.0-9%-]%s",
		ctx->relay_hostname, remain[0]);
	if (ret == 2) {
		/* Optional port number */
		switch (remain[0][0]) {
		case ':':
			ret = sscanf(remain[0], ":%d%s", &ctx->port, remain[1]);
			/* Optional session ID with port number */
			if (ret == 2) {
				ret = sscanf(remain[1], "/%s", remain[2]);
				/* Accept 0 or 1 (optional) */
				if (ret < 0) {
					goto end;
				}
			} else if (ret == 0) {
				fprintf(stderr, "[error] Missing port number after delimitor ':'\n");
				ret = -1;
				goto end;
			}
			break;
		case '/':
			/* Optional session ID */
			ret = sscanf(remain[0], "/%s", remain[2]);
			/* Accept 0 or 1 (optional) */
			if (ret < 0) {
				goto end;
			}
			break;
		default:
			fprintf(stderr, "[error] wrong delimitor : %c\n",
				remain[0][0]);
			ret = -1;
			goto end;
		}
	}

	if (ctx->port < 0) {
		ctx->port = LTTNG_DEFAULT_NETWORK_VIEWER_PORT;
	}

	if (strlen(remain[2]) == 0) {
		printf_verbose("Connecting to hostname : %s, port : %d, "
				"proto : IPv%d\n",
				ctx->relay_hostname, ctx->port, proto);
		ret = 0;
		goto end;
	}
	ret = sscanf(remain[2], "host/%[a-zA-Z.0-9%-]/%s",
			ctx->traced_hostname, ctx->session_name);
	if (ret != 2) {
		fprintf(stderr, "[error] Format : "
			"net://<hostname>/host/<traced_hostname>/<session_name>\n");
		goto end;
	}

	printf_verbose("Connecting to hostname : %s, port : %d, "
			"traced hostname : %s, session name : %s, "
			"proto : IPv%d\n",
			ctx->relay_hostname, ctx->port, ctx->traced_hostname,
			ctx->session_name, proto);
	ret = 0;

end:
	return ret;
}

static
guint g_uint64p_hash(gconstpointer key)
{
	uint64_t v = *(uint64_t *) key;

	if (sizeof(gconstpointer) == sizeof(uint64_t)) {
		return g_direct_hash((gconstpointer) (unsigned long) v);
	} else {
		return g_direct_hash((gconstpointer) (unsigned long) (v >> 32))
			^ g_direct_hash((gconstpointer) (unsigned long) v);
	}
}

static
gboolean g_uint64p_equal(gconstpointer a, gconstpointer b)
{
	uint64_t va = *(uint64_t *) a;
	uint64_t vb = *(uint64_t *) b;

	if (va != vb)
		return FALSE;
	return TRUE;
}

static int lttng_live_open_trace_read(const char *path)
{
	int ret = 0;
	struct lttng_live_ctx *ctx;

	ctx = g_new0(struct lttng_live_ctx, 1);
	ctx->session = g_new0(struct lttng_live_session, 1);

	/* We need a pointer to the context from the packet_seek function. */
	ctx->session->ctx = ctx;

	/* HT to store the CTF traces. */
	ctx->session->ctf_traces = g_hash_table_new(g_uint64p_hash,
			g_uint64p_equal);
	ctx->port = -1;
	ctx->session_ids = g_array_new(FALSE, TRUE, sizeof(uint64_t));

	ret = parse_url(path, ctx);
	if (ret < 0) {
		goto end_free;
	}
	ret = setup_sighandler();
	if (ret < 0) {
		goto end_free;
	}
	ret = lttng_live_connect_viewer(ctx);
	if (ret < 0) {
		goto end_free;
	}
	printf_verbose("LTTng-live connected to relayd\n");

	ret = lttng_live_establish_connection(ctx);
	if (ret < 0) {
		goto end_free;
	}

	printf_verbose("Listing sessions\n");
	ret = lttng_live_list_sessions(ctx, path);
	if (ret < 0) {
		goto end_free;
	}

	if (ctx->session_ids->len > 0) {
		ret = lttng_live_read(ctx);
	}

end_free:
	g_hash_table_destroy(ctx->session->ctf_traces);
	g_free(ctx->session);
	g_free(ctx->session->streams);
	g_free(ctx);

	if (lttng_live_should_quit()) {
		ret = 0;
	}
	return ret;
}

static
struct bt_trace_descriptor *lttng_live_open_trace(const char *path, int flags,
		void (*packet_seek)(struct bt_stream_pos *pos, size_t index,
			int whence), FILE *metadata_fp)
{
	struct ctf_text_stream_pos *pos;

	switch (flags & O_ACCMODE) {
	case O_RDONLY:
		/* OK */
		break;
	case O_RDWR:
		fprintf(stderr, "[error] lttng live plugin cannot be used as output plugin.\n");
		goto error;
	default:
		fprintf(stderr, "[error] Incorrect open flags.\n");
		goto error;
	}

	pos = g_new0(struct ctf_text_stream_pos, 1);
	pos->parent.rw_table = NULL;
	pos->parent.event_cb = NULL;
	pos->parent.trace = &pos->trace_descriptor;
	/*
	 * Since we do *everything* in this function, we are skipping
	 * the output plugin handling that is part of Babeltrace 1.x.
	 * Therefore, don't expect the --output cmd line option to work.
	 * This limits the output of lttng-live to stderr and stdout.
	 */
	if (lttng_live_open_trace_read(path) < 0) {
		goto error;
	}
	return &pos->trace_descriptor;

error:
	return NULL;
}

static
int lttng_live_close_trace(struct bt_trace_descriptor *td)
{
	struct ctf_text_stream_pos *pos =
		container_of(td, struct ctf_text_stream_pos,
			trace_descriptor);
	g_free(pos);
	return 0;
}

static
struct bt_format lttng_live_format = {
	.open_trace = lttng_live_open_trace,
	.close_trace = lttng_live_close_trace,
};

static
void __attribute__((constructor)) lttng_live_init(void)
{
	int ret;

	lttng_live_format.name = g_quark_from_static_string("lttng-live");
	ret = bt_register_format(&lttng_live_format);
	assert(!ret);
}

static
void __attribute__((destructor)) lttng_live_exit(void)
{
	bt_unregister_format(&lttng_live_format);
}
