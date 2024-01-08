/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#ifndef BABELTRACE_PLUGIN_TEXT_PRETTY_PRETTY_H
#define BABELTRACE_PLUGIN_TEXT_PRETTY_PRETTY_H

#include <glib.h>
#include <stdio.h>
#include <stdbool.h>
#include "common/macros.h"
#include <babeltrace2/babeltrace.h>

/*
 * `bt_field_*_enumeration` are backed by 64 bits integers so the maximum
 * number of bitflags in any enumeration is 64.
 */
#define ENUMERATION_MAX_BITFLAGS_COUNT (sizeof(uint64_t) * 8)

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
	bool print_enum_flags;
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
	bt_message_iterator *iterator;
	FILE *out, *err;
	int depth;	/* nesting, used for tabulation alignment. */
	bool start_line;
	GString *string;
	GString *tmp_string;
	bool use_colors;

	uint64_t last_cycles_timestamp;
	uint64_t delta_cycles;

	uint64_t last_real_timestamp;
	uint64_t delta_real_timestamp;

	bool negative_timestamp_warning_done;

	/*
	 * For each bit of the integer backing the enumeration we have a list
	 * (GPtrArray) of labels (char *) for that bit.
	 *
	 * Allocate all label arrays during the initialization of the component
	 * and reuse the same set of arrays for all enumerations. This prevents
	 * allocation and deallocation everytime the component encounters a
	 * enumeration field. Allocating and deallocating that often could
	 * severely impact performance.
	 */
	GPtrArray *enum_bit_labels[ENUMERATION_MAX_BITFLAGS_COUNT];

	bt_logging_level log_level;
	bt_self_component *self_comp;
};

bt_component_class_initialize_method_status pretty_init(
		bt_self_component_sink *component,
		bt_self_component_sink_configuration *config,
		const bt_value *params,
		void *init_method_data);

bt_component_class_sink_consume_method_status pretty_consume(
		bt_self_component_sink *component);

bt_component_class_sink_graph_is_configured_method_status pretty_graph_is_configured(
		bt_self_component_sink *component);

void pretty_finalize(bt_self_component_sink *component);

int pretty_print_event(struct pretty_component *pretty,
		const bt_message *event_msg);

int pretty_print_discarded_items(struct pretty_component *pretty,
		const bt_message *msg);

void pretty_print_init(void);

#endif /* BABELTRACE_PLUGIN_TEXT_PRETTY_PRETTY_H */
