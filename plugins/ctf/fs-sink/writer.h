#ifndef BABELTRACE_PLUGIN_WRITER_H
#define BABELTRACE_PLUGIN_WRITER_H

/*
 * BabelTrace - CTF Writer Output Plug-in
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Author: Jérémie Galarneau <jeremie.galarneau@efficios.com>
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
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/babeltrace.h>

struct writer_component {
	GString *base_path;
	GString *trace_name_base;
	/* For the directory name suffix. */
	int trace_id;
	/* Map between bt_trace and struct fs_writer. */
	GHashTable *trace_map;
	FILE *err;
	bt_notification_iterator *input_iterator;
	bool error;
	bool single_trace;
	unsigned int nr_traces;
};

enum fs_writer_stream_state {
	/*
	 * We know the stream exists but we have never received a
	 * stream_begin notification for it.
	 */
	FS_WRITER_UNKNOWN_STREAM,
	/* We know this stream is active (between stream_begin and _end). */
	FS_WRITER_ACTIVE_STREAM,
	/* We have received a stream_end for this stream. */
	FS_WRITER_COMPLETED_STREAM,
};

struct fs_writer {
	struct bt_ctf_writer *writer;
	const bt_trace *trace;
	const bt_trace *writer_trace;
	struct writer_component *writer_component;
	int static_listener_id;
	int trace_static;
	/* Map between reader and writer stream. */
	GHashTable *stream_map;
	/* Map between reader and writer stream class. */
	GHashTable *stream_class_map;
	GHashTable *stream_states;
};

BT_HIDDEN
void writer_close(struct writer_component *writer_component,
		struct fs_writer *fs_writer);
BT_HIDDEN
enum bt_component_status writer_output_event(struct writer_component *writer,
		const bt_event *event);
BT_HIDDEN
enum bt_component_status writer_new_packet(struct writer_component *writer,
		const bt_packet *packet);
BT_HIDDEN
enum bt_component_status writer_close_packet(struct writer_component *writer,
		const bt_packet *packet);
BT_HIDDEN
enum bt_component_status writer_stream_begin(struct writer_component *writer,
		const bt_stream *stream);
BT_HIDDEN
enum bt_component_status writer_stream_end(struct writer_component *writer,
		const bt_stream *stream);

BT_HIDDEN
enum bt_component_status writer_component_init(
	bt_self_component *component, bt_value *params,
	void *init_method_data);

BT_HIDDEN
enum bt_component_status writer_run(bt_self_component *component);

BT_HIDDEN
void writer_component_port_connected(
		bt_self_component *component,
		struct bt_private_port *self_port,
		const bt_port *other_port);

BT_HIDDEN
void writer_component_finalize(bt_self_component *component);

#endif /* BABELTRACE_PLUGIN_WRITER_H */
