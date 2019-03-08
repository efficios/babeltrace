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

#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/babeltrace.h>
#include <babeltrace/ctfser-internal.h>
#include <glib.h>
#include <stdbool.h>
#include <stdint.h>

#include "fs-sink-ctf-meta.h"

struct fs_sink_trace;

struct fs_sink_stream {
	struct fs_sink_trace *trace;
	struct bt_ctfser ctfser;

	/* Stream's file name */
	GString *file_name;

	/* Weak */
	const bt_stream *ir_stream;

	struct fs_sink_ctf_stream_class *sc;

	struct {
		bool is_open;
		uint64_t beginning_cs;
		uint64_t end_cs;
		uint64_t content_size;
		uint64_t total_size;
		uint64_t discarded_events_counter;
		uint64_t seq_num;
		uint64_t context_offset_bits;

		/* Owned by this */
		const bt_packet *packet;
	} packet_state;

	struct {
		uint64_t end_cs;
		uint64_t discarded_events_counter;
		uint64_t seq_num;
	} prev_packet_state;

	struct {
		bool in_range;
		uint64_t beginning_cs;
		uint64_t end_cs;
	} discarded_events_state;

	struct {
		bool in_range;
		uint64_t beginning_cs;
		uint64_t end_cs;
	} discarded_packets_state;

	bool in_discarded_events_range;
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
