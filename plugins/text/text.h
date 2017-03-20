#ifndef BABELTRACE_PLUGIN_TEXT_H
#define BABELTRACE_PLUGIN_TEXT_H

/*
 * BabelTrace - CTF Text Output Plug-in
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

enum text_default {
	TEXT_DEFAULT_UNSET,
	TEXT_DEFAULT_SHOW,
	TEXT_DEFAULT_HIDE,
};

enum text_color_option {
	TEXT_COLOR_OPT_NEVER,
	TEXT_COLOR_OPT_AUTO,
	TEXT_COLOR_OPT_ALWAYS,
};

struct text_options {
	char *output_path;
	char *debug_info_dir;
	char *debug_info_target_prefix;

	enum text_default name_default;
	enum text_default field_default;

	bool print_scope_field_names;
	bool print_header_field_names;
	bool print_context_field_names;
	bool print_payload_field_names;

	bool print_delta_field;
	bool print_loglevel_field;
	bool print_emf_field;
	bool print_callsite_field;
	bool print_trace_field;
	bool print_trace_domain_field;
	bool print_trace_procname_field;
	bool print_trace_vpid_field;
	bool print_trace_hostname_field;

	bool print_timestamp_cycles;
	bool clock_seconds;
	bool clock_date;
	bool clock_gmt;
	bool debug_info_full_path;
	enum text_color_option color;
	bool verbose;
};

struct text_component {
	struct text_options options;
	struct bt_notification_iterator *input_iterator;
	FILE *out, *err;
	bool processed_first_event; /* Should be per-iterator. */
	int depth;	/* nesting, used for tabulation alignment. */
	bool start_line;
	GString *string;
	struct bt_value *plugin_opt_map;	/* Temporary parameter map. */
	bool use_colors;

	uint64_t last_cycles_timestamp;
	uint64_t delta_cycles;

	uint64_t last_real_timestamp;
	uint64_t delta_real_timestamp;
};

enum stream_packet_context_quarks_enum {
	Q_TIMESTAMP_BEGIN,
	Q_TIMESTAMP_END,
	Q_EVENTS_DISCARDED,
	Q_CONTENT_SIZE,
	Q_PACKET_SIZE,
	Q_PACKET_SEQ_NUM,
	STREAM_PACKET_CONTEXT_QUARKS_LEN, /* Always the last one of this enum. */
};

GQuark stream_packet_context_quarks[STREAM_PACKET_CONTEXT_QUARKS_LEN];

BT_HIDDEN
enum bt_component_status text_print_event(struct text_component *text,
		struct bt_ctf_event *event);

#endif /* BABELTRACE_PLUGIN_TEXT_H */
