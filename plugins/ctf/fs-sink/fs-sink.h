#ifndef BABELTRACE_PLUGIN_CTF_FS_SINK_FS_SINK_H
#define BABELTRACE_PLUGIN_CTF_FS_SINK_FS_SINK_H

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
#include <stdbool.h>
#include <glib.h>

struct fs_sink_comp {
	bt_self_component_sink *self_comp;

	/* Owned by this */
	bt_self_component_port_input_message_iterator *upstream_iter;

	/* Base output directory path */
	GString *output_dir_path;

	bool assume_single_trace;
	bool ignore_discarded_events;
	bool ignore_discarded_packets;
	bool quiet;

	/*
	 * Hash table of `const bt_trace *` (weak) to
	 * `struct fs_sink_trace *` (owned by hash table).
	 */
	GHashTable *traces;
};

BT_HIDDEN
bt_self_component_status ctf_fs_sink_init(
		bt_self_component_sink *component,
		const bt_value *params,
		void *init_method_data);

BT_HIDDEN
bt_self_component_status ctf_fs_sink_consume(
		bt_self_component_sink *component);

BT_HIDDEN
bt_self_component_status ctf_fs_sink_graph_is_configured(
		bt_self_component_sink *component);

BT_HIDDEN
void ctf_fs_sink_finalize(bt_self_component_sink *component);

#endif /* BABELTRACE_PLUGIN_CTF_FS_SINK_FS_SINK_H */
