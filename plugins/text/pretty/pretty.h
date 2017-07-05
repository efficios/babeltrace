#ifndef BABELTRACE_PLUGIN_TEXT_PRETTY_PRETTY_H
#define BABELTRACE_PLUGIN_TEXT_PRETTY_PRETTY_H

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
#include <babeltrace/graph/private-component.h>
#include <babeltrace/graph/private-port.h>
#include <babeltrace/ctf-ir/event.h>

enum pretty_default {
	PRETTY_DEFAULT_UNSET,
	PRETTY_DEFAULT_SHOW,
	PRETTY_DEFAULT_HIDE,
};

enum pretty_color_option {
	PRETTY_COLOR_OPT_NEVER,
	PRETTY_COLOR_OPT_AUTO,
	PRETTY_COLOR_OPT_ALWAYS,
};

struct pretty_options {
	char *output_path;

	enum pretty_default name_default;
	enum pretty_default field_default;

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
	enum pretty_color_option color;
	bool verbose;
};

struct pretty_component {
	struct pretty_options options;
	struct bt_notification_iterator *input_iterator;
	FILE *out, *err;
	int depth;	/* nesting, used for tabulation alignment. */
	bool start_line;
	GString *string;
	GString *tmp_string;
	struct bt_value *plugin_opt_map;	/* Temporary parameter map. */
	bool use_colors;
	bool error;

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
enum bt_component_status pretty_init(
		struct bt_private_component *component,
		struct bt_value *params,
		void *init_method_data);

BT_HIDDEN
enum bt_component_status pretty_consume(struct bt_private_component *component);

BT_HIDDEN
void pretty_port_connected(
		struct bt_private_component *component,
		struct bt_private_port *self_port,
		struct bt_port *other_port);

BT_HIDDEN
void pretty_finalize(struct bt_private_component *component);

BT_HIDDEN
enum bt_component_status pretty_print_event(struct pretty_component *pretty,
		struct bt_notification *event_notif);

BT_HIDDEN
enum bt_component_status pretty_print_discarded_elements(
		struct pretty_component *pretty,
		struct bt_notification *notif);

#endif /* BABELTRACE_PLUGIN_TEXT_PRETTY_PRETTY_H */
