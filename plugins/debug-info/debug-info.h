#ifndef BABELTRACE_PLUGIN_DEBUG_INFO_H
#define BABELTRACE_PLUGIN_DEBUG_INFO_H

/*
 * Babeltrace - Debug information Plug-in
 *
 * Copyright (c) 2015 EfficiOS Inc.
 * Copyright (c) 2015 Antoine Busque <abusque@efficios.com>
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
#include <stdint.h>
#include <stddef.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/ctf-ir/event.h>
#include <babeltrace/ctf-ir/trace.h>
#include <babeltrace/ctf-ir/fields.h>
#include <babeltrace/ctf-ir/event-class.h>

struct debug_info_component {
	FILE *err;
	char *arg_debug_info_field_name;
	const char *arg_debug_dir;
	bool arg_full_path;
	const char *arg_target_prefix;
};

struct debug_info_iterator {
	struct debug_info_component *debug_info_component;
	/* Map between struct bt_ctf_trace and struct bt_ctf_writer. */
	GHashTable *trace_map;
	/* Map between reader and writer stream. */
	GHashTable *stream_map;
	/* Map between reader and writer stream class. */
	GHashTable *stream_class_map;
	/* Map between reader and writer stream class. */
	GHashTable *packet_map;
	/* Map between a trace_class and its corresponding debug_info. */
	GHashTable *trace_debug_map;
	/* Input iterators associated with this output iterator. */
	GPtrArray *input_iterator_group;
	struct bt_notification *current_notification;
	struct bt_notification_iterator *input_iterator;
	FILE *err;
};

struct debug_info_source {
	/* Strings are owned by debug_info_source. */
	char *func;
	uint64_t line_no;
	char *src_path;
	/* short_src_path points inside src_path, no need to free. */
	const char *short_src_path;
	char *bin_path;
	/* short_bin_path points inside bin_path, no need to free. */
	const char *short_bin_path;
	/*
	 * Location within the binary. Either absolute (@0x1234) or
	 * relative (+0x4321).
	 */
	char *bin_loc;
};

BT_HIDDEN
struct debug_info *debug_info_create(void);

BT_HIDDEN
void debug_info_destroy(struct debug_info *debug_info);

BT_HIDDEN
struct debug_info_source *debug_info_query(struct debug_info *debug_info,
		int64_t vpid, uint64_t ip);

BT_HIDDEN
void debug_info_handle_event(FILE *err, struct bt_ctf_event *event,
		struct debug_info *debug_info);

#if 0
static inline
void trace_debug_info_destroy(struct bt_ctf_trace *trace)
{
	debug_info_destroy(trace->debug_info);
}
#endif

#endif /* BABELTRACE_PLUGIN_DEBUG_INFO_H */
