/*
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
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

#define BT_LOG_TAG "PLUGIN-CTF-FS-SINK"
#include "logging.h"

#include <babeltrace/babeltrace.h>
#include <stdio.h>
#include <stdbool.h>
#include <glib.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/ctfser-internal.h>

#include "fs-sink.h"
#include "fs-sink-trace.h"
#include "fs-sink-stream.h"
#include "fs-sink-ctf-meta.h"
#include "translate-trace-ir-to-ctf-ir.h"
#include "translate-ctf-ir-to-tsdl.h"

static
const char * const in_port_name = "in";

static
bt_self_component_status ensure_output_dir_exists(
		struct fs_sink_comp *fs_sink)
{
	bt_self_component_status status = BT_SELF_COMPONENT_STATUS_OK;
	int ret;

	ret = g_mkdir_with_parents(fs_sink->output_dir_path->str, 0755);
	if (ret) {
		BT_LOGE_ERRNO("Cannot create directories for output directory",
			": output-dir-path=\"%s\"",
			fs_sink->output_dir_path->str);
		status = BT_SELF_COMPONENT_STATUS_ERROR;
		goto end;
	}

end:
	return status;
}

static
bt_self_component_status configure_component(struct fs_sink_comp *fs_sink,
		const bt_value *params)
{
	bt_self_component_status status = BT_SELF_COMPONENT_STATUS_OK;
	const bt_value *value;

	value = bt_value_map_borrow_entry_value_const(params, "path");
	if (!value) {
		BT_LOGE_STR("Missing mandatory `path` parameter.");
		status = BT_SELF_COMPONENT_STATUS_ERROR;
		goto end;
	}

	if (!bt_value_is_string(value)) {
		BT_LOGE_STR("`path` parameter: expecting a string.");
		status = BT_SELF_COMPONENT_STATUS_ERROR;
		goto end;
	}

	g_string_assign(fs_sink->output_dir_path,
		bt_value_string_get(value));
	value = bt_value_map_borrow_entry_value_const(params,
		"assume-single-trace");
	if (value) {
		if (!bt_value_is_bool(value)) {
			BT_LOGE_STR("`assume-single-trace` parameter: expecting a boolean.");
			status = BT_SELF_COMPONENT_STATUS_ERROR;
			goto end;
		}

		fs_sink->assume_single_trace = (bool) bt_value_bool_get(value);
	}

	value = bt_value_map_borrow_entry_value_const(params,
		"ignore-discarded-events");
	if (value) {
		if (!bt_value_is_bool(value)) {
			BT_LOGE_STR("`ignore-discarded-events` parameter: expecting a boolean.");
			status = BT_SELF_COMPONENT_STATUS_ERROR;
			goto end;
		}

		fs_sink->ignore_discarded_events =
			(bool) bt_value_bool_get(value);
	}

	value = bt_value_map_borrow_entry_value_const(params,
		"ignore-discarded-packets");
	if (value) {
		if (!bt_value_is_bool(value)) {
			BT_LOGE_STR("`ignore-discarded-packets` parameter: expecting a boolean.");
			status = BT_SELF_COMPONENT_STATUS_ERROR;
			goto end;
		}

		fs_sink->ignore_discarded_packets =
			(bool) bt_value_bool_get(value);
	}

	value = bt_value_map_borrow_entry_value_const(params,
		"quiet");
	if (value) {
		if (!bt_value_is_bool(value)) {
			BT_LOGE_STR("`quiet` parameter: expecting a boolean.");
			status = BT_SELF_COMPONENT_STATUS_ERROR;
			goto end;
		}

		fs_sink->quiet = (bool) bt_value_bool_get(value);
	}

end:
	return status;
}

static
void destroy_fs_sink_comp(struct fs_sink_comp *fs_sink)
{
	if (!fs_sink) {
		goto end;
	}

	if (fs_sink->output_dir_path) {
		g_string_free(fs_sink->output_dir_path, TRUE);
		fs_sink->output_dir_path = NULL;
	}

	if (fs_sink->traces) {
		g_hash_table_destroy(fs_sink->traces);
		fs_sink->traces = NULL;
	}

	BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_PUT_REF_AND_RESET(
		fs_sink->upstream_iter);
	g_free(fs_sink);

end:
	return;
}

BT_HIDDEN
bt_self_component_status ctf_fs_sink_init(
		bt_self_component_sink *self_comp, const bt_value *params,
		void *init_method_data)
{
	bt_self_component_status status = BT_SELF_COMPONENT_STATUS_OK;
	struct fs_sink_comp *fs_sink = NULL;

	fs_sink = g_new0(struct fs_sink_comp, 1);
	if (!fs_sink) {
		BT_LOGE_STR("Failed to allocate one CTF FS sink structure.");
		status = BT_SELF_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	fs_sink->output_dir_path = g_string_new(NULL);
	fs_sink->self_comp = self_comp;
	status = configure_component(fs_sink, params);
	if (status != BT_SELF_COMPONENT_STATUS_OK) {
		/* configure_component() logs errors */
		goto end;
	}

	if (fs_sink->assume_single_trace &&
			g_file_test(fs_sink->output_dir_path->str,
				G_FILE_TEST_EXISTS)) {
		BT_LOGE("Single trace mode, but output path exists: "
			"output-path=\"%s\"", fs_sink->output_dir_path->str);
		status = BT_SELF_COMPONENT_STATUS_ERROR;
		goto end;
	}

	status = ensure_output_dir_exists(fs_sink);
	if (status != BT_SELF_COMPONENT_STATUS_OK) {
		/* ensure_output_dir_exists() logs errors */
		goto end;
	}

	fs_sink->traces = g_hash_table_new_full(g_direct_hash, g_direct_equal,
		NULL, (GDestroyNotify) fs_sink_trace_destroy);
	if (!fs_sink->traces) {
		BT_LOGE_STR("Failed to allocate one GHashTable.");
		status = BT_SELF_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	status = bt_self_component_sink_add_input_port(self_comp, in_port_name,
		NULL, NULL);
	if (status != BT_SELF_COMPONENT_STATUS_OK) {
		goto end;
	}

	bt_self_component_set_data(
		bt_self_component_sink_as_self_component(self_comp), fs_sink);

end:
	if (status != BT_SELF_COMPONENT_STATUS_OK) {
		destroy_fs_sink_comp(fs_sink);
	}

	return status;
}

static inline
struct fs_sink_stream *borrow_stream(struct fs_sink_comp *fs_sink,
		const bt_stream *ir_stream)
{
	const bt_trace *ir_trace = bt_stream_borrow_trace_const(ir_stream);
	struct fs_sink_trace *trace;
	struct fs_sink_stream *stream = NULL;

	trace = g_hash_table_lookup(fs_sink->traces, ir_trace);
	if (unlikely(!trace)) {
		if (fs_sink->assume_single_trace &&
				g_hash_table_size(fs_sink->traces) > 0) {
			BT_LOGE("Single trace mode, but getting more than one trace: "
				"stream-name=\"%s\"",
				bt_stream_get_name(ir_stream));
			goto end;
		}

		trace = fs_sink_trace_create(fs_sink, ir_trace);
		if (!trace) {
			goto end;
		}
	}

	stream = g_hash_table_lookup(trace->streams, ir_stream);
	if (unlikely(!stream)) {
		stream = fs_sink_stream_create(trace, ir_stream);
		if (!stream) {
			goto end;
		}
	}

end:
	return stream;
}

static inline
bt_self_component_status handle_event_msg(struct fs_sink_comp *fs_sink,
		const bt_message *msg)
{
	int ret;
	bt_self_component_status status = BT_SELF_COMPONENT_STATUS_OK;
	const bt_event *ir_event = bt_message_event_borrow_event_const(msg);
	const bt_stream *ir_stream = bt_event_borrow_stream_const(ir_event);
	struct fs_sink_stream *stream;
	struct fs_sink_ctf_event_class *ec = NULL;
	const bt_clock_snapshot *cs = NULL;

	stream = borrow_stream(fs_sink, ir_stream);
	if (unlikely(!stream)) {
		status = BT_SELF_COMPONENT_STATUS_ERROR;
		goto end;
	}

	ret = try_translate_event_class_trace_ir_to_ctf_ir(stream->sc,
		bt_event_borrow_class_const(ir_event), &ec);
	if (ret) {
		status = BT_SELF_COMPONENT_STATUS_ERROR;
		goto end;
	}

	BT_ASSERT(ec);

	if (stream->sc->default_clock_class) {
		(void) bt_message_event_borrow_default_clock_snapshot_const(
			msg, &cs);
	}

	ret = fs_sink_stream_write_event(stream, cs, ir_event, ec);
	if (unlikely(ret)) {
		status = BT_SELF_COMPONENT_STATUS_ERROR;
		goto end;
	}

end:
	return status;
}

static inline
bt_self_component_status handle_packet_beginning_msg(
		struct fs_sink_comp *fs_sink, const bt_message *msg)
{
	int ret;
	bt_self_component_status status = BT_SELF_COMPONENT_STATUS_OK;
	const bt_packet *ir_packet =
		bt_message_packet_beginning_borrow_packet_const(msg);
	const bt_stream *ir_stream = bt_packet_borrow_stream_const(ir_packet);
	struct fs_sink_stream *stream;
	const bt_clock_snapshot *cs = NULL;

	stream = borrow_stream(fs_sink, ir_stream);
	if (unlikely(!stream)) {
		status = BT_SELF_COMPONENT_STATUS_ERROR;
		goto end;
	}

	if (stream->sc->default_clock_class) {
		(void) bt_message_packet_beginning_borrow_default_clock_snapshot_const(
			msg, &cs);
		BT_ASSERT(cs);
	}

	if (stream->discarded_events_state.in_range) {
		/*
		 * Make sure that the current discarded events range's
		 * beginning time matches what's expected for CTF 1.8.
		 */
		if (stream->sc->default_clock_class) {
			uint64_t expected_cs;

			if (stream->prev_packet_state.end_cs == UINT64_C(-1)) {
				/* We're opening the first packet */
				expected_cs = bt_clock_snapshot_get_value(cs);
			} else {
				expected_cs = stream->prev_packet_state.end_cs;
			}

			if (stream->discarded_events_state.beginning_cs !=
					expected_cs) {
				BT_LOGE("Incompatible discarded events message: "
					"unexpected beginning time: "
					"beginning-cs-val=%" PRIu64 ", "
					"expected-beginning-cs-val=%" PRIu64 ", "
					"stream-id=%" PRIu64 ", stream-name=\"%s\", "
					"trace-name=\"%s\", path=\"%s/%s\"",
					stream->discarded_events_state.beginning_cs,
					expected_cs,
					bt_stream_get_id(ir_stream),
					bt_stream_get_name(ir_stream),
					bt_trace_get_name(
						bt_stream_borrow_trace_const(ir_stream)),
					stream->trace->path->str, stream->file_name->str);
				status = BT_SELF_COMPONENT_STATUS_ERROR;
				goto end;
			}
		}
	}


	if (stream->discarded_packets_state.in_range) {
		if (stream->prev_packet_state.end_cs == UINT64_C(-1)) {
			BT_LOGE("Incompatible discarded packets message "
				"occuring before the stream's first packet: "
				"stream-id=%" PRIu64 ", stream-name=\"%s\", "
				"trace-name=\"%s\", path=\"%s/%s\"",
				bt_stream_get_id(ir_stream),
				bt_stream_get_name(ir_stream),
				bt_trace_get_name(
					bt_stream_borrow_trace_const(ir_stream)),
				stream->trace->path->str, stream->file_name->str);
			status = BT_SELF_COMPONENT_STATUS_ERROR;
			goto end;
		}

		/*
		 * Make sure that the current discarded packets range's
		 * beginning and end times match what's expected for CTF
		 * 1.8.
		 */
		if (stream->sc->default_clock_class) {
			uint64_t expected_end_cs =
				bt_clock_snapshot_get_value(cs);

			if (stream->discarded_packets_state.beginning_cs !=
					stream->prev_packet_state.end_cs) {
				BT_LOGE("Incompatible discarded packets message: "
					"unexpected beginning time: "
					"beginning-cs-val=%" PRIu64 ", "
					"expected-beginning-cs-val=%" PRIu64 ", "
					"stream-id=%" PRIu64 ", stream-name=\"%s\", "
					"trace-name=\"%s\", path=\"%s/%s\"",
					stream->discarded_packets_state.beginning_cs,
					stream->prev_packet_state.end_cs,
					bt_stream_get_id(ir_stream),
					bt_stream_get_name(ir_stream),
					bt_trace_get_name(
						bt_stream_borrow_trace_const(ir_stream)),
					stream->trace->path->str, stream->file_name->str);
				status = BT_SELF_COMPONENT_STATUS_ERROR;
				goto end;
			}

			if (stream->discarded_packets_state.end_cs !=
					expected_end_cs) {
				BT_LOGE("Incompatible discarded packets message: "
					"unexpected end time: "
					"end-cs-val=%" PRIu64 ", "
					"expected-end-cs-val=%" PRIu64 ", "
					"stream-id=%" PRIu64 ", stream-name=\"%s\", "
					"trace-name=\"%s\", path=\"%s/%s\"",
					stream->discarded_packets_state.beginning_cs,
					expected_end_cs,
					bt_stream_get_id(ir_stream),
					bt_stream_get_name(ir_stream),
					bt_trace_get_name(
						bt_stream_borrow_trace_const(ir_stream)),
					stream->trace->path->str, stream->file_name->str);
				status = BT_SELF_COMPONENT_STATUS_ERROR;
				goto end;
			}
		}
	}

	stream->discarded_packets_state.in_range = false;
	ret = fs_sink_stream_open_packet(stream, cs, ir_packet);
	if (ret) {
		status = BT_SELF_COMPONENT_STATUS_ERROR;
		goto end;
	}

end:
	return status;
}

static inline
bt_self_component_status handle_packet_end_msg(
		struct fs_sink_comp *fs_sink, const bt_message *msg)
{
	int ret;
	bt_self_component_status status = BT_SELF_COMPONENT_STATUS_OK;
	const bt_packet *ir_packet =
		bt_message_packet_end_borrow_packet_const(msg);
	const bt_stream *ir_stream = bt_packet_borrow_stream_const(ir_packet);
	struct fs_sink_stream *stream;
	const bt_clock_snapshot *cs = NULL;

	stream = borrow_stream(fs_sink, ir_stream);
	if (unlikely(!stream)) {
		status = BT_SELF_COMPONENT_STATUS_ERROR;
		goto end;
	}

	if (stream->sc->default_clock_class) {
		(void) bt_message_packet_end_borrow_default_clock_snapshot_const(
			msg, &cs);
		BT_ASSERT(cs);
	}

	if (stream->sc->default_clock_class) {
		(void) bt_message_packet_end_borrow_default_clock_snapshot_const(
			msg, &cs);
		BT_ASSERT(cs);
	}

	if (stream->discarded_events_state.in_range) {
		/*
		 * Make sure that the current discarded events range's
		 * end time matches what's expected for CTF 1.8.
		 */
		if (stream->sc->default_clock_class) {
			uint64_t expected_cs = bt_clock_snapshot_get_value(cs);

			if (stream->discarded_events_state.end_cs !=
					expected_cs) {
				BT_LOGE("Incompatible discarded events message: "
					"unexpected end time: "
					"end-cs-val=%" PRIu64 ", "
					"expected-end-cs-val=%" PRIu64 ", "
					"stream-id=%" PRIu64 ", stream-name=\"%s\", "
					"trace-name=\"%s\", path=\"%s/%s\"",
					stream->discarded_events_state.end_cs,
					expected_cs,
					bt_stream_get_id(ir_stream),
					bt_stream_get_name(ir_stream),
					bt_trace_get_name(
						bt_stream_borrow_trace_const(ir_stream)),
					stream->trace->path->str, stream->file_name->str);
				status = BT_SELF_COMPONENT_STATUS_ERROR;
				goto end;
			}
		}
	}

	ret = fs_sink_stream_close_packet(stream, cs);
	if (ret) {
		status = BT_SELF_COMPONENT_STATUS_ERROR;
		goto end;
	}

	stream->discarded_events_state.in_range = false;

end:
	return status;
}

static inline
bt_self_component_status handle_stream_beginning_msg(
		struct fs_sink_comp *fs_sink, const bt_message *msg)
{
	bt_self_component_status status = BT_SELF_COMPONENT_STATUS_OK;
	const bt_stream *ir_stream =
		bt_message_stream_beginning_borrow_stream_const(msg);
	struct fs_sink_stream *stream;

	stream = borrow_stream(fs_sink, ir_stream);
	if (!stream) {
		status = BT_SELF_COMPONENT_STATUS_ERROR;
		goto end;
	}

	BT_LOGI("Created new, empty stream file: "
		"stream-id=%" PRIu64 ", stream-name=\"%s\", "
		"trace-name=\"%s\", path=\"%s/%s\"",
		bt_stream_get_id(ir_stream), bt_stream_get_name(ir_stream),
		bt_trace_get_name(bt_stream_borrow_trace_const(ir_stream)),
		stream->trace->path->str, stream->file_name->str);

end:
	return status;
}

static inline
bt_self_component_status handle_stream_end_msg(struct fs_sink_comp *fs_sink,
		const bt_message *msg)
{
	bt_self_component_status status = BT_SELF_COMPONENT_STATUS_OK;
	const bt_stream *ir_stream =
		bt_message_stream_end_borrow_stream_const(msg);
	struct fs_sink_stream *stream;

	stream = borrow_stream(fs_sink, ir_stream);
	if (!stream) {
		status = BT_SELF_COMPONENT_STATUS_ERROR;
		goto end;
	}

	BT_LOGI("Closing stream file: "
		"stream-id=%" PRIu64 ", stream-name=\"%s\", "
		"trace-name=\"%s\", path=\"%s/%s\"",
		bt_stream_get_id(ir_stream), bt_stream_get_name(ir_stream),
		bt_trace_get_name(bt_stream_borrow_trace_const(ir_stream)),
		stream->trace->path->str, stream->file_name->str);

	/*
	 * This destroys the stream object and frees all its resources,
	 * closing the stream file.
	 */
	g_hash_table_remove(stream->trace->streams, ir_stream);

end:
	return status;
}

static inline
bt_self_component_status handle_discarded_events_msg(
		struct fs_sink_comp *fs_sink, const bt_message *msg)
{
	bt_self_component_status status = BT_SELF_COMPONENT_STATUS_OK;
	const bt_stream *ir_stream =
		bt_message_discarded_events_borrow_stream_const(msg);
	struct fs_sink_stream *stream;
	const bt_clock_snapshot *cs = NULL;
	bt_property_availability avail;
	uint64_t count;

	stream = borrow_stream(fs_sink, ir_stream);
	if (!stream) {
		status = BT_SELF_COMPONENT_STATUS_ERROR;
		goto end;
	}

	if (fs_sink->ignore_discarded_events) {
		BT_LOGI("Ignoring discarded events message: "
			"stream-id=%" PRIu64 ", stream-name=\"%s\", "
			"trace-name=\"%s\", path=\"%s/%s\"",
			bt_stream_get_id(ir_stream),
			bt_stream_get_name(ir_stream),
			bt_trace_get_name(
				bt_stream_borrow_trace_const(ir_stream)),
			stream->trace->path->str, stream->file_name->str);
		goto end;
	}

	if (stream->discarded_events_state.in_range) {
		BT_LOGE("Unsupported contiguous discarded events message: "
			"stream-id=%" PRIu64 ", stream-name=\"%s\", "
			"trace-name=\"%s\", path=\"%s/%s\"",
			bt_stream_get_id(ir_stream),
			bt_stream_get_name(ir_stream),
			bt_trace_get_name(
				bt_stream_borrow_trace_const(ir_stream)),
			stream->trace->path->str, stream->file_name->str);
		status = BT_SELF_COMPONENT_STATUS_ERROR;
		goto end;
	}

	if (stream->packet_state.is_open) {
		BT_LOGE("Unsupported discarded events message occuring "
			"within a packet: "
			"stream-id=%" PRIu64 ", stream-name=\"%s\", "
			"trace-name=\"%s\", path=\"%s/%s\"",
			bt_stream_get_id(ir_stream),
			bt_stream_get_name(ir_stream),
			bt_trace_get_name(
				bt_stream_borrow_trace_const(ir_stream)),
			stream->trace->path->str, stream->file_name->str);
		status = BT_SELF_COMPONENT_STATUS_ERROR;
		goto end;
	}

	stream->discarded_events_state.in_range = true;

	if (stream->sc->default_clock_class) {
		/*
		 * The clock snapshot values will be validated when
		 * handling the next "packet beginning" message.
		 */
		(void) bt_message_discarded_events_borrow_default_beginning_clock_snapshot_const(
			msg, &cs);
		BT_ASSERT(cs);
		stream->discarded_events_state.beginning_cs =
			bt_clock_snapshot_get_value(cs);
		cs = NULL;
		(void) bt_message_discarded_events_borrow_default_end_clock_snapshot_const(
			msg, &cs);
		BT_ASSERT(cs);
		stream->discarded_events_state.end_cs =
			bt_clock_snapshot_get_value(cs);
	} else {
		stream->discarded_events_state.beginning_cs = UINT64_C(-1);
		stream->discarded_events_state.end_cs = UINT64_C(-1);
	}

	avail = bt_message_discarded_events_get_count(msg, &count);
	if (avail != BT_PROPERTY_AVAILABILITY_AVAILABLE) {
		count = 1;
	}

	stream->packet_state.discarded_events_counter += count;

end:
	return status;
}

static inline
bt_self_component_status handle_discarded_packets_msg(
		struct fs_sink_comp *fs_sink, const bt_message *msg)
{
	bt_self_component_status status = BT_SELF_COMPONENT_STATUS_OK;
	const bt_stream *ir_stream =
		bt_message_discarded_packets_borrow_stream_const(msg);
	struct fs_sink_stream *stream;
	const bt_clock_snapshot *cs = NULL;
	bt_property_availability avail;
	uint64_t count;

	stream = borrow_stream(fs_sink, ir_stream);
	if (!stream) {
		status = BT_SELF_COMPONENT_STATUS_ERROR;
		goto end;
	}

	if (fs_sink->ignore_discarded_packets) {
		BT_LOGI("Ignoring discarded packets message: "
			"stream-id=%" PRIu64 ", stream-name=\"%s\", "
			"trace-name=\"%s\", path=\"%s/%s\"",
			bt_stream_get_id(ir_stream),
			bt_stream_get_name(ir_stream),
			bt_trace_get_name(
				bt_stream_borrow_trace_const(ir_stream)),
			stream->trace->path->str, stream->file_name->str);
		goto end;
	}

	if (stream->discarded_packets_state.in_range) {
		BT_LOGE("Unsupported contiguous discarded packets message: "
			"stream-id=%" PRIu64 ", stream-name=\"%s\", "
			"trace-name=\"%s\", path=\"%s/%s\"",
			bt_stream_get_id(ir_stream),
			bt_stream_get_name(ir_stream),
			bt_trace_get_name(
				bt_stream_borrow_trace_const(ir_stream)),
			stream->trace->path->str, stream->file_name->str);
		status = BT_SELF_COMPONENT_STATUS_ERROR;
		goto end;
	}

	if (stream->packet_state.is_open) {
		BT_LOGE("Unsupported discarded packets message occuring "
			"within a packet: "
			"stream-id=%" PRIu64 ", stream-name=\"%s\", "
			"trace-name=\"%s\", path=\"%s/%s\"",
			bt_stream_get_id(ir_stream),
			bt_stream_get_name(ir_stream),
			bt_trace_get_name(
				bt_stream_borrow_trace_const(ir_stream)),
			stream->trace->path->str, stream->file_name->str);
		status = BT_SELF_COMPONENT_STATUS_ERROR;
		goto end;
	}

	stream->discarded_packets_state.in_range = true;

	if (stream->sc->default_clock_class) {
		/*
		 * The clock snapshot values will be validated when
		 * handling the next "packet beginning" message.
		 */
		(void) bt_message_discarded_packets_borrow_default_beginning_clock_snapshot_const(
			msg, &cs);
		BT_ASSERT(cs);
		stream->discarded_packets_state.beginning_cs =
			bt_clock_snapshot_get_value(cs);
		cs = NULL;
		(void) bt_message_discarded_packets_borrow_default_end_clock_snapshot_const(
			msg, &cs);
		BT_ASSERT(cs);
		stream->discarded_packets_state.end_cs =
			bt_clock_snapshot_get_value(cs);
	} else {
		stream->discarded_packets_state.beginning_cs = UINT64_C(-1);
		stream->discarded_packets_state.end_cs = UINT64_C(-1);
	}

	avail = bt_message_discarded_packets_get_count(msg, &count);
	if (avail != BT_PROPERTY_AVAILABILITY_AVAILABLE) {
		count = 1;
	}

	stream->packet_state.seq_num += count;

end:
	return status;
}

static inline
void put_messages(bt_message_array_const msgs, uint64_t count)
{
	uint64_t i;

	for (i = 0; i < count; i++) {
		BT_MESSAGE_PUT_REF_AND_RESET(msgs[i]);
	}
}

BT_HIDDEN
bt_self_component_status ctf_fs_sink_consume(bt_self_component_sink *self_comp)
{
	bt_self_component_status status = BT_SELF_COMPONENT_STATUS_OK;
	struct fs_sink_comp *fs_sink;
	bt_message_iterator_status it_status;
	uint64_t msg_count = 0;
	bt_message_array_const msgs;

	fs_sink = bt_self_component_get_data(
			bt_self_component_sink_as_self_component(self_comp));
	BT_ASSERT(fs_sink);
	BT_ASSERT(fs_sink->upstream_iter);

	/* Consume messages */
	it_status = bt_self_component_port_input_message_iterator_next(
		fs_sink->upstream_iter, &msgs, &msg_count);
	if (it_status < 0) {
		status = BT_SELF_COMPONENT_STATUS_ERROR;
		goto end;
	}

	switch (it_status) {
	case BT_MESSAGE_ITERATOR_STATUS_OK:
	{
		uint64_t i;

		for (i = 0; i < msg_count; i++) {
			const bt_message *msg = msgs[i];

			BT_ASSERT(msg);

			switch (bt_message_get_type(msg)) {
			case BT_MESSAGE_TYPE_EVENT:
				status = handle_event_msg(fs_sink, msg);
				break;
			case BT_MESSAGE_TYPE_PACKET_BEGINNING:
				status = handle_packet_beginning_msg(
					fs_sink, msg);
				break;
			case BT_MESSAGE_TYPE_PACKET_END:
				status = handle_packet_end_msg(
					fs_sink, msg);
				break;
			case BT_MESSAGE_TYPE_MESSAGE_ITERATOR_INACTIVITY:
				/* Ignore */
				BT_LOGD_STR("Ignoring message iterator inactivity message.");
				break;
			case BT_MESSAGE_TYPE_STREAM_BEGINNING:
				status = handle_stream_beginning_msg(
					fs_sink, msg);
				break;
			case BT_MESSAGE_TYPE_STREAM_END:
				status = handle_stream_end_msg(
					fs_sink, msg);
				break;
			case BT_MESSAGE_TYPE_STREAM_ACTIVITY_BEGINNING:
			case BT_MESSAGE_TYPE_STREAM_ACTIVITY_END:
				/* Not supported by CTF 1.8 */
				BT_LOGD_STR("Ignoring stream activity message.");
				break;
			case BT_MESSAGE_TYPE_DISCARDED_EVENTS:
				status = handle_discarded_events_msg(
					fs_sink, msg);
				break;
			case BT_MESSAGE_TYPE_DISCARDED_PACKETS:
				status = handle_discarded_packets_msg(
					fs_sink, msg);
				break;
			default:
				abort();
			}

			BT_MESSAGE_PUT_REF_AND_RESET(msgs[i]);

			if (status != BT_SELF_COMPONENT_STATUS_OK) {
				BT_LOGE("Failed to handle message: "
					"generated CTF traces could be incomplete: "
					"output-dir-path=\"%s\"",
					fs_sink->output_dir_path->str);
				goto error;
			}
		}

		break;
	}
	case BT_MESSAGE_ITERATOR_STATUS_AGAIN:
		status = BT_SELF_COMPONENT_STATUS_AGAIN;
		break;
	case BT_MESSAGE_ITERATOR_STATUS_END:
		/* TODO: Finalize all traces (should already be done?) */
		status = BT_SELF_COMPONENT_STATUS_END;
		break;
	case BT_MESSAGE_ITERATOR_STATUS_NOMEM:
		status = BT_SELF_COMPONENT_STATUS_NOMEM;
		break;
	case BT_MESSAGE_ITERATOR_STATUS_ERROR:
		status = BT_SELF_COMPONENT_STATUS_NOMEM;
		break;
	default:
		break;
	}

	goto end;

error:
	BT_ASSERT(status != BT_SELF_COMPONENT_STATUS_OK);
	put_messages(msgs, msg_count);

end:
	return status;
}

BT_HIDDEN
bt_self_component_status ctf_fs_sink_graph_is_configured(
		bt_self_component_sink *self_comp)
{
	bt_self_component_status status = BT_SELF_COMPONENT_STATUS_OK;
	struct fs_sink_comp *fs_sink = bt_self_component_get_data(
			bt_self_component_sink_as_self_component(self_comp));

	fs_sink->upstream_iter =
		bt_self_component_port_input_message_iterator_create(
			bt_self_component_sink_borrow_input_port_by_name(
				self_comp, in_port_name));
	if (!fs_sink->upstream_iter) {
		status = BT_SELF_COMPONENT_STATUS_NOMEM;
		goto end;
	}

end:
	return status;
}

BT_HIDDEN
void ctf_fs_sink_finalize(bt_self_component_sink *self_comp)
{
	struct fs_sink_comp *fs_sink = bt_self_component_get_data(
			bt_self_component_sink_as_self_component(self_comp));

	destroy_fs_sink_comp(fs_sink);
}
