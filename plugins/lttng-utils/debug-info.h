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
#include <babeltrace/babeltrace.h>

#define VPID_FIELD_NAME		"vpid"
#define IP_FIELD_NAME		"ip"
#define BADDR_FIELD_NAME	"baddr"
#define CRC32_FIELD_NAME	"crc32"
#define BUILD_ID_FIELD_NAME	"build_id"
#define FILENAME_FIELD_NAME	"filename"
#define IS_PIC_FIELD_NAME	"is_pic"
#define MEMSZ_FIELD_NAME	"memsz"
#define PATH_FIELD_NAME		"path"

enum debug_info_stream_state {
	/*
	 * We know the stream exists but we have never received a
	 * stream_begin message for it.
	 */
	DEBUG_INFO_UNKNOWN_STREAM,
	/* We know this stream is active (between stream_begin and _end). */
	DEBUG_INFO_ACTIVE_STREAM,
	/* We have received a stream_end for this stream. */
	DEBUG_INFO_COMPLETED_STREAM,
};

struct debug_info_component {
	FILE *err;
	char *arg_debug_info_field_name;
	const char *arg_debug_dir;
	bool arg_full_path;
	const char *arg_target_prefix;
};

struct debug_info_iterator {
	struct debug_info_component *debug_info_component;
	/* Map between bt_trace and struct bt_writer. */
	GHashTable *trace_map;
	/* Input iterators associated with this output iterator. */
	GPtrArray *input_iterator_group;
	const bt_message *current_message;
	bt_message_iterator *input_iterator;
	FILE *err;
};

struct debug_info_trace {
	const bt_trace *trace;
	const bt_trace *writer_trace;
	struct debug_info_component *debug_info_component;
	struct debug_info_iterator *debug_it;
	int static_listener_id;
	int trace_static;
	/* Map between reader and writer stream. */
	GHashTable *stream_map;
	/* Map between reader and writer stream class. */
	GHashTable *stream_class_map;
	/* Map between reader and writer stream class. */
	GHashTable *packet_map;
	/* Map between a trace_class and its corresponding debug_info. */
	GHashTable *trace_debug_map;
	/* Map between a stream and enum debug_info_stream_state. */
	GHashTable *stream_states;
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
struct debug_info *debug_info_create(struct debug_info_component *comp);

BT_HIDDEN
void debug_info_destroy(struct debug_info *debug_info);

BT_HIDDEN
struct debug_info_source *debug_info_query(struct debug_info *debug_info,
		int64_t vpid, uint64_t ip);

BT_HIDDEN
void debug_info_handle_event(FILE *err, const bt_event *event,
		struct debug_info *debug_info);

BT_HIDDEN
void debug_info_close_trace(struct debug_info_iterator *debug_it,
		struct debug_info_trace *di_trace);

#endif /* BABELTRACE_PLUGIN_DEBUG_INFO_H */
