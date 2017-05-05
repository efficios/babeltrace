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
#include <babeltrace/graph/component.h>
#include <babeltrace/ctf-writer/writer.h>

struct writer_component {
	GString *base_path;
	GString *trace_name_base;
	/* For the directory name suffix. */
	int trace_id;
	/* Map between struct bt_ctf_trace and struct bt_ctf_writer. */
	GHashTable *trace_map;
	/* Map between reader and writer stream. */
	GHashTable *stream_map;
	/* Map between reader and writer stream class. */
	GHashTable *stream_class_map;
	FILE *err;
	struct bt_notification_iterator *input_iterator;
	bool processed_first_event;
	bool error;
};

BT_HIDDEN
enum bt_component_status writer_output_event(struct writer_component *writer,
		struct bt_ctf_event *event);
BT_HIDDEN
enum bt_component_status writer_new_packet(struct writer_component *writer,
		struct bt_ctf_packet *packet);
BT_HIDDEN
enum bt_component_status writer_close_packet(struct writer_component *writer,
		struct bt_ctf_packet *packet);

BT_HIDDEN
enum bt_component_status writer_component_init(
	struct bt_private_component *component, struct bt_value *params,
	void *init_method_data);

BT_HIDDEN
enum bt_component_status writer_run(struct bt_private_component *component);

BT_HIDDEN
void writer_component_port_connected(
		struct bt_private_component *component,
		struct bt_private_port *self_port,
		struct bt_port *other_port);

BT_HIDDEN
void writer_component_finalize(struct bt_private_component *component);

#endif /* BABELTRACE_PLUGIN_WRITER_H */
