/*
 * Copyright 2019 - Francis Deslauriers <francis.deslauriers@efficios.com>
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

#define BT_COMP_LOG_SELF_COMP self_comp
#define BT_LOG_OUTPUT_LEVEL log_level
#define BT_LOG_TAG "PLUGIN/SRC.CTF.LTTNG-LIVE/META"
#include "logging/comp-logging.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <glib.h>
#include "compat/memstream.h"
#include <babeltrace2/babeltrace.h>

#include "metadata.h"
#include "../common/metadata/decoder.h"
#include "../common/metadata/ctf-meta-configure-ir-trace.h"

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
bool stream_classes_all_have_default_clock_class(bt_trace_class *tc,
		bt_logging_level log_level,
		bt_self_component *self_comp)
{
	uint64_t i, sc_count;
	const bt_clock_class *cc = NULL;
	const bt_stream_class *sc;
	bool ret = true;

	sc_count = bt_trace_class_get_stream_class_count(tc);
	for (i = 0; i < sc_count; i++) {
		sc = bt_trace_class_borrow_stream_class_by_index_const(tc, i);

		BT_ASSERT(sc);

		cc = bt_stream_class_borrow_default_clock_class_const(sc);
		if (!cc) {
			ret = false;
			BT_COMP_LOGE_APPEND_CAUSE(self_comp,
				"Stream class doesn't have a default clock class: "
				"sc-id=%" PRIu64 ", sc-name=\"%s\"",
				bt_stream_class_get_id(sc),
				bt_stream_class_get_name(sc));
			goto end;
		}
	}

end:
	return ret;
}
/*
 * Iterate over the stream classes and returns the first clock class
 * encountered. This is useful to create message iterator inactivity message as
 * we don't need a particular clock class.
 */
static
const bt_clock_class *borrow_any_clock_class(bt_trace_class *tc)
{
	uint64_t i, sc_count;
	const bt_clock_class *cc = NULL;
	const bt_stream_class *sc;

	sc_count = bt_trace_class_get_stream_class_count(tc);
	for (i = 0; i < sc_count; i++) {
		sc = bt_trace_class_borrow_stream_class_by_index_const(tc, i);
		BT_ASSERT_DBG(sc);

		cc = bt_stream_class_borrow_default_clock_class_const(sc);
		if (cc) {
			goto end;
		}
	}
end:
	BT_ASSERT_DBG(cc);
	return cc;
}

BT_HIDDEN
enum lttng_live_iterator_status lttng_live_metadata_update(
		struct lttng_live_trace *trace)
{
	struct lttng_live_session *session = trace->session;
	struct lttng_live_metadata *metadata = trace->metadata;
	size_t size, len_read = 0;
	char *metadata_buf = NULL;
	bool keep_receiving;
	FILE *fp = NULL;
	enum ctf_metadata_decoder_status decoder_status;
	enum lttng_live_iterator_status status =
		LTTNG_LIVE_ITERATOR_STATUS_OK;
	bt_logging_level log_level = trace->log_level;
	bt_self_component *self_comp = trace->self_comp;
	enum lttng_live_get_one_metadata_status metadata_status;

	BT_COMP_LOGD("Updating metadata for trace: trace-id=%"PRIu64, trace->id);

	/* No metadata stream yet. */
	if (!metadata) {
		if (session->new_streams_needed) {
			status = LTTNG_LIVE_ITERATOR_STATUS_AGAIN;
		} else {
			session->new_streams_needed = true;
			status = LTTNG_LIVE_ITERATOR_STATUS_CONTINUE;
		}
		goto end;
	}

	if (trace->metadata_stream_state != LTTNG_LIVE_METADATA_STREAM_STATE_NEEDED) {
		goto end;
	}

	/*
	 * Open a new write only file handle to populate the `metadata_buf`
	 * memory buffer so we can write in loop in it easily.
	 */
	fp = bt_open_memstream(&metadata_buf, &size);
	if (!fp) {
		if (errno == EINTR &&
				lttng_live_graph_is_canceled(session->lttng_live_msg_iter)) {
			session->lttng_live_msg_iter->was_interrupted = true;
			status = LTTNG_LIVE_ITERATOR_STATUS_AGAIN;
		} else {
			BT_COMP_LOGE_APPEND_CAUSE_ERRNO(self_comp,
				"Metadata open_memstream", ".");
			status = LTTNG_LIVE_ITERATOR_STATUS_ERROR;
		}
		goto end;
	}

	keep_receiving = true;
	/* Grab all available metadata. */
	while (keep_receiving) {
		size_t reply_len = 0;
		/*
		 * lttng_live_get_one_metadata_packet() asks the Relay Daemon
		 * for new metadata. If new metadata is received, the function
		 * writes it to the provided file handle and updates the
		 * reply_len output parameter. We call this function in loop
		 * until it returns _END meaning that no new metadata is
		 * available.
		 * We may receive a _CLOSED status if the metadata stream we
		 * are requesting is no longer available on the relay.
		 * If we receive an _ERROR status, it means there was a
		 * networking, allocating, or some other unrecoverable error.
		 */
		metadata_status = lttng_live_get_one_metadata_packet(trace, fp,
			&reply_len);

		switch (metadata_status) {
		case LTTNG_LIVE_GET_ONE_METADATA_STATUS_OK:
			len_read += reply_len;
			break;
		case LTTNG_LIVE_GET_ONE_METADATA_STATUS_END:
			keep_receiving = false;
			break;
		case LTTNG_LIVE_GET_ONE_METADATA_STATUS_CLOSED:
			BT_COMP_LOGD("Metadata stream was closed by the Relay, the trace is no longer active: "
				"trace-id=%"PRIu64", metadata-stream-id=%"PRIu64,
				trace->id, metadata->stream_id);
			/*
			 * The stream was closed and we received everything
			 * there was to receive for this metadata stream.
			 * We go on with the decoding of what we received. So
			 * that data stream can be decoded.
			 */
			keep_receiving = false;
			trace->metadata_stream_state = LTTNG_LIVE_METADATA_STREAM_STATE_CLOSED;
			break;
		case LTTNG_LIVE_GET_ONE_METADATA_STATUS_ERROR:
			BT_COMP_LOGE_APPEND_CAUSE(self_comp,
				"Error getting one trace metadata packet: "
				"trace-id=%"PRIu64, trace->id);
			goto error;
		default:
			bt_common_abort();
		}
	}

	/* The memory buffer `metadata_buf` contains all the metadata. */
	if (bt_close_memstream(&metadata_buf, &size, fp)) {
		BT_COMP_LOGW_ERRNO("Metadata bt_close_memstream", ".");
	}

	fp = NULL;

	if (len_read == 0) {
		if (!trace->trace) {
			status = LTTNG_LIVE_ITERATOR_STATUS_AGAIN;
			goto end;
		}

		/* The relay sent zero bytes of metdata. */
		trace->metadata_stream_state = LTTNG_LIVE_METADATA_STREAM_STATE_NOT_NEEDED;
		goto end;
	}

	/*
	 * Open a new reading file handle on the `metadata_buf` and pass it to
	 * the metadata decoder.
	 */
	fp = bt_fmemopen(metadata_buf, len_read, "rb");
	if (!fp) {
		if (errno == EINTR &&
				lttng_live_graph_is_canceled(session->lttng_live_msg_iter)) {
			session->lttng_live_msg_iter->was_interrupted = true;
			status = LTTNG_LIVE_ITERATOR_STATUS_AGAIN;
		} else {
			BT_COMP_LOGE_APPEND_CAUSE_ERRNO(self_comp,
				"Cannot memory-open metadata buffer", ".");
			status = LTTNG_LIVE_ITERATOR_STATUS_ERROR;
		}
		goto end;
	}

	/*
	 * The call to ctf_metadata_decoder_append_content() will append
	 * new metadata to our current trace class.
	 */
	BT_COMP_LOGD("Appending new metadata to the ctf_trace class");
	decoder_status = ctf_metadata_decoder_append_content(
		metadata->decoder, fp);
	switch (decoder_status) {
	case CTF_METADATA_DECODER_STATUS_OK:
		if (!trace->trace_class) {
			struct ctf_trace_class *tc =
				ctf_metadata_decoder_borrow_ctf_trace_class(
					metadata->decoder);

			trace->trace_class =
				ctf_metadata_decoder_get_ir_trace_class(
						metadata->decoder);
			trace->trace = bt_trace_create(trace->trace_class);
			if (!trace->trace) {
				BT_COMP_LOGE_APPEND_CAUSE(self_comp,
					"Failed to create bt_trace");
				goto error;
			}
			if (ctf_trace_class_configure_ir_trace(tc,
					trace->trace)) {
				BT_COMP_LOGE_APPEND_CAUSE(self_comp,
					"Failed to configure ctf trace class");
				goto error;
			}
			if (!stream_classes_all_have_default_clock_class(
					trace->trace_class, log_level,
					self_comp)) {
				/* Error logged in function. */
				goto error;
			}
			trace->clock_class =
				borrow_any_clock_class(trace->trace_class);
		}

		/* The metadata was updated succesfully. */
		trace->metadata_stream_state = LTTNG_LIVE_METADATA_STREAM_STATE_NOT_NEEDED;

		break;
	default:
		goto error;
	}

	goto end;

error:
	status = LTTNG_LIVE_ITERATOR_STATUS_ERROR;
end:
	if (fp) {
		int closeret;

		closeret = fclose(fp);
		if (closeret) {
			BT_COMP_LOGW_ERRNO("Error on fclose", ".");
		}
	}
	free(metadata_buf);
	return status;
}

BT_HIDDEN
int lttng_live_metadata_create_stream(struct lttng_live_session *session,
		uint64_t ctf_trace_id, uint64_t stream_id,
		const char *trace_name)
{
	bt_self_component *self_comp = session->self_comp;
	bt_logging_level log_level = session->log_level;
	struct lttng_live_metadata *metadata = NULL;
	struct lttng_live_trace *trace;
	struct ctf_metadata_decoder_config cfg = {
		.log_level = session->log_level,
		.self_comp = session->self_comp,
		.clock_class_offset_s = 0,
		.clock_class_offset_ns = 0,
		.create_trace_class = true,
	};

	metadata = g_new0(struct lttng_live_metadata, 1);
	if (!metadata) {
		return -1;
	}
	metadata->log_level = session->log_level;
	metadata->self_comp = session->self_comp;
	metadata->stream_id = stream_id;

	metadata->decoder = ctf_metadata_decoder_create(&cfg);
	if (!metadata->decoder) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Failed to create CTF metadata decoder");
		goto error;
	}
	trace = lttng_live_session_borrow_or_create_trace_by_id(session,
		ctf_trace_id);
	if (!trace) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp,
			"Failed to borrow trace");
		goto error;
	}
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
	ctf_metadata_decoder_destroy(metadata->decoder);
	trace->metadata = NULL;
	g_free(metadata);
}
