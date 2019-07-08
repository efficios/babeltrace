#ifndef BABELTRACE_PLUGIN_CTF_FS_SINK_FS_SINK_STREAM_H
#define BABELTRACE_PLUGIN_CTF_FS_SINK_FS_SINK_STREAM_H

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

#include "common/macros.h"
#include <babeltrace2/babeltrace.h>
#include "ctfser/ctfser.h"
#include <glib.h>
#include <stdbool.h>
#include <stdint.h>

#include "fs-sink-ctf-meta.h"

struct fs_sink_trace;

struct fs_sink_stream {
	bt_logging_level log_level;
	struct fs_sink_trace *trace;
	struct bt_ctfser ctfser;

	/* Stream's file name */
	GString *file_name;

	/* Weak */
	const bt_stream *ir_stream;

	struct fs_sink_ctf_stream_class *sc;

	/* Current packet's state */
	struct {
		/*
		 * True if we're, for this stream, within an opened
		 * packet (got a packet beginning message, but no
		 * packet end message yet).
		 */
		bool is_open;

		/*
		 * Current beginning default clock snapshot for the
		 * current packet (`UINT64_C(-1)` if not set).
		 */
		uint64_t beginning_cs;

		/*
		 * Current end default clock snapshot for the current
		 * packet (`UINT64_C(-1)` if not set).
		 */
		uint64_t end_cs;

		/*
		 * Current packet's content size (bits) for the current
		 * packet.
		 */
		uint64_t content_size;

		/*
		 * Current packet's total size (bits) for the current
		 * packet.
		 */
		uint64_t total_size;

		/*
		 * Discarded events (free running) counter for the
		 * current packet.
		 */
		uint64_t discarded_events_counter;

		/* Sequence number (free running) of the current packet */
		uint64_t seq_num;

		/*
		 * Offset of the packet context structure within the
		 * current packet (bits).
		 */
		uint64_t context_offset_bits;

		/*
		 * Owned by this; `NULL` if the current packet is closed
		 * or if the trace IR stream does not support packets.
		 */
		const bt_packet *packet;
	} packet_state;

	/* Previous packet's state */
	struct {
		/* End default clock snapshot (`UINT64_C(-1)` if not set) */
		uint64_t end_cs;

		/* Discarded events (free running) counter */
		uint64_t discarded_events_counter;

		/* Sequence number (free running) */
		uint64_t seq_num;
	} prev_packet_state;

	/* State to handle discarded events */
	struct {
		/*
		 * True if we're in the time range given by a previously
		 * received discarded events message. In this case,
		 * `beginning_cs` and `end_cs` below contain the
		 * beginning and end clock snapshots for this range.
		 *
		 * This is used to validate that, when receiving a
		 * packet end message, the current discarded events time
		 * range matches what's expected for CTF 1.8, that is:
		 *
		 * * Its beginning time is the previous packet's end
		 *   time (or the current packet's beginning time if
		 *   this is the first packet).
		 *
		 * * Its end time is the current packet's end time.
		 */
		bool in_range;

		/*
		 * Beginning and end times of the time range given by a
		 * previously received discarded events message.
		 */
		uint64_t beginning_cs;
		uint64_t end_cs;
	} discarded_events_state;

	/* State to handle discarded packets */
	struct {
		/*
		 * True if we're in the time range given by a previously
		 * received discarded packets message. In this case,
		 * `beginning_cs` and `end_cs` below contain the
		 * beginning and end clock snapshots for this range.
		 *
		 * This is used to validate that, when receiving a
		 * packet beginning message, the current discarded
		 * packets time range matches what's expected for CTF
		 * 1.8, that is:
		 *
		 * * Its beginning time is the previous packet's end
		 *   time.
		 *
		 * * Its end time is the current packet's beginning
		 *   time.
		 */
		bool in_range;

		/*
		 * Beginning and end times of the time range given by a
		 * previously received discarded packets message.
		 */
		uint64_t beginning_cs;
		uint64_t end_cs;
	} discarded_packets_state;
};

BT_HIDDEN
struct fs_sink_stream *fs_sink_stream_create(struct fs_sink_trace *trace,
		const bt_stream *ir_stream);

BT_HIDDEN
void fs_sink_stream_destroy(struct fs_sink_stream *stream);

BT_HIDDEN
int fs_sink_stream_write_event(struct fs_sink_stream *stream,
		const bt_clock_snapshot *cs, const bt_event *event,
		struct fs_sink_ctf_event_class *ec);

BT_HIDDEN
int fs_sink_stream_open_packet(struct fs_sink_stream *stream,
		const bt_clock_snapshot *cs, const bt_packet *packet);

BT_HIDDEN
int fs_sink_stream_close_packet(struct fs_sink_stream *stream,
		const bt_clock_snapshot *cs);

#endif /* BABELTRACE_PLUGIN_CTF_FS_SINK_FS_SINK_STREAM_H */
