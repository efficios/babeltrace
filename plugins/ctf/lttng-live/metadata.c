/*
 * Copyright 2016 - Philippe Proulx <pproulx@efficios.com>
 * Copyright 2010-2011 - EfficiOS Inc. and Linux Foundation
 *
 * Some functions are based on older functions written by Mathieu Desnoyers.
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

#define BT_LOG_TAG "PLUGIN-CTF-LTTNG-LIVE-SRC-METADATA"
#include "logging.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <glib.h>
#include <babeltrace/compat/uuid-internal.h>
#include <babeltrace/compat/memstream-internal.h>
#include <babeltrace/babeltrace.h>

#include "metadata.h"
#include "../common/metadata/decoder.h"

#define TSDL_MAGIC	0x75d11d57

struct packet_header {
	uint32_t magic;
	uint8_t  uuid[16];
	uint32_t checksum;
	uint32_t content_size;
	uint32_t packet_size;
	uint8_t  compression_scheme;
	uint8_t  encryption_scheme;
	uint8_t  checksum_scheme;
	uint8_t  major;
	uint8_t  minor;
} __attribute__((__packed__));

static
bt_lttng_live_iterator_status lttng_live_update_clock_map(
		struct lttng_live_trace *trace)
{
	bt_lttng_live_iterator_status status =
			BT_LTTNG_LIVE_ITERATOR_STATUS_OK;
	size_t i;
	int count, ret;

	BT_OBJECT_PUT_REF_AND_RESET(trace->cc_prio_map);
	trace->cc_prio_map = bt_clock_class_priority_map_create();
	if (!trace->cc_prio_map) {
		goto error;
	}

	count = bt_trace_get_clock_class_count(trace->trace);
	BT_ASSERT(count >= 0);

	for (i = 0; i < count; i++) {
		const bt_clock_class *clock_class =
			bt_trace_get_clock_class_by_index(trace->trace, i);

		BT_ASSERT(clock_class);
		ret = bt_clock_class_priority_map_add_clock_class(
			trace->cc_prio_map, clock_class, 0);
		BT_CLOCK_CLASS_PUT_REF_AND_RESET(clock_class);

		if (ret) {
			goto error;
		}
	}

	goto end;
error:
	status = BT_LTTNG_LIVE_ITERATOR_STATUS_ERROR;
end:
	return status;
}

BT_HIDDEN
bt_lttng_live_iterator_status lttng_live_metadata_update(
		struct lttng_live_trace *trace)
{
	struct lttng_live_session *session = trace->session;
	struct lttng_live_metadata *metadata = trace->metadata;
	ssize_t ret = 0;
	size_t size, len_read = 0;
	char *metadata_buf = NULL;
	FILE *fp = NULL;
	enum ctf_metadata_decoder_status decoder_status;
	bt_lttng_live_iterator_status status =
		BT_LTTNG_LIVE_ITERATOR_STATUS_OK;

	/* No metadata stream yet. */
	if (!metadata) {
		if (session->new_streams_needed) {
			status = BT_LTTNG_LIVE_ITERATOR_STATUS_AGAIN;
		} else {
			session->new_streams_needed = true;
			status = BT_LTTNG_LIVE_ITERATOR_STATUS_CONTINUE;
		}
		goto end;
	}

	if (!metadata->trace) {
		trace->new_metadata_needed = false;
	}

	if (!trace->new_metadata_needed) {
		goto end;
	}

	/* Open for writing */
	fp = bt_open_memstream(&metadata_buf, &size);
	if (!fp) {
		BT_LOGE("Metadata open_memstream: %s", strerror(errno));
		goto error;
	}

	/* Grab all available metadata. */
	do {
		/*
		 * get_one_metadata_packet returns the number of bytes
		 * received, 0 when we have received everything, a
		 * negative value on error.
		 */
		ret = lttng_live_get_one_metadata_packet(trace, fp);
		if (ret > 0) {
			len_read += ret;
		}
	} while (ret > 0);

	/*
	 * Consider metadata closed as soon as we get an error reading
	 * it (e.g. cannot be found).
	 */
	if (ret < 0) {
		if (!metadata->closed) {
			metadata->closed = true;
			/*
			 * Release our reference on the trace as soon as
			 * we know the metadata stream is not available
			 * anymore. This won't necessarily teardown the
			 * metadata objects immediately, but only when
			 * the data streams are done.
			 */
			lttng_live_unref_trace(metadata->trace);
			metadata->trace = NULL;
		}
		if (errno == EINTR) {
			if (lttng_live_is_canceled(session->lttng_live)) {
				status = BT_LTTNG_LIVE_ITERATOR_STATUS_AGAIN;
				goto end;
			}
		}
	}

	if (bt_close_memstream(&metadata_buf, &size, fp)) {
		BT_LOGE("bt_close_memstream: %s", strerror(errno));
	}
	ret = 0;
	fp = NULL;

	if (len_read == 0) {
		if (!trace->trace) {
			status = BT_LTTNG_LIVE_ITERATOR_STATUS_AGAIN;
			goto end;
		}
		trace->new_metadata_needed = false;
		goto end;
	}

	fp = bt_fmemopen(metadata_buf, len_read, "rb");
	if (!fp) {
		BT_LOGE("Cannot memory-open metadata buffer: %s",
			strerror(errno));
		goto error;
	}

	decoder_status = ctf_metadata_decoder_decode(metadata->decoder, fp);
	switch (decoder_status) {
	case CTF_METADATA_DECODER_STATUS_OK:
		BT_OBJECT_PUT_REF_AND_RESET(trace->trace);
		trace->trace = ctf_metadata_decoder_get_trace(metadata->decoder);
		trace->new_metadata_needed = false;
		status = lttng_live_update_clock_map(trace);
		if (status != BT_LTTNG_LIVE_ITERATOR_STATUS_OK) {
			goto end;
		}
		break;
	case CTF_METADATA_DECODER_STATUS_INCOMPLETE:
		status = BT_LTTNG_LIVE_ITERATOR_STATUS_AGAIN;
		break;
	case CTF_METADATA_DECODER_STATUS_ERROR:
	case CTF_METADATA_DECODER_STATUS_INVAL_VERSION:
	case CTF_METADATA_DECODER_STATUS_IR_VISITOR_ERROR:
		goto error;
	}

	goto end;
error:
	status = BT_LTTNG_LIVE_ITERATOR_STATUS_ERROR;
end:
	if (fp) {
		int closeret;

		closeret = fclose(fp);
		if (closeret) {
			BT_LOGE("Error on fclose");
		}
	}
	free(metadata_buf);
	return status;
}

BT_HIDDEN
int lttng_live_metadata_create_stream(struct lttng_live_session *session,
		uint64_t ctf_trace_id,
		uint64_t stream_id,
		const char *trace_name)
{
	struct lttng_live_metadata *metadata = NULL;
	struct lttng_live_trace *trace;
	const char *match;

	metadata = g_new0(struct lttng_live_metadata, 1);
	if (!metadata) {
		return -1;
	}
	metadata->stream_id = stream_id;
	//TODO: add clock offset option
	match = strstr(trace_name, session->session_name->str);
	if (!match) {
		goto error;
	}
	metadata->decoder = ctf_metadata_decoder_create(NULL,
		match);
	if (!metadata->decoder) {
		goto error;
	}
	trace = lttng_live_ref_trace(session, ctf_trace_id);
	if (!trace) {
		goto error;
	}
	metadata->trace = trace;
	trace->metadata = metadata;
	return 0;

error:
	ctf_metadata_decoder_destroy(metadata->decoder);
	g_free(metadata);
	return -1;
}

BT_HIDDEN
void lttng_live_metadata_fini(struct lttng_live_trace *trace)
{
	struct lttng_live_metadata *metadata = trace->metadata;

	if (!metadata) {
		return;
	}
	if (metadata->text) {
		free(metadata->text);
	}
	ctf_metadata_decoder_destroy(metadata->decoder);
	trace->metadata = NULL;
	if (!metadata->closed) {
		lttng_live_unref_trace(metadata->trace);
	}
	g_free(metadata);
}
