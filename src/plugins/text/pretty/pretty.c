/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2016 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 */

#define BT_COMP_LOG_SELF_COMP (pretty->self_comp)
#define BT_LOG_OUTPUT_LEVEL (pretty->log_level)
#define BT_LOG_TAG "PLUGIN/SINK.TEXT.PRETTY"
#include "logging/comp-logging.h"

#include <babeltrace2/babeltrace.h>
#include "compat/compiler.h"
#include "common/common.h"
#include <stdio.h>
#include <stdbool.h>
#include <glib.h>
#include <string.h>
#include "common/assert.h"
#include "plugins/common/param-validation/param-validation.h"

#include "pretty.h"

static
const char * const in_port_name = "in";

static
void destroy_pretty_data(struct pretty_component *pretty)
{
	uint64_t i;
	if (!pretty) {
		goto end;
	}

	bt_message_iterator_put_ref(pretty->iterator);

	if (pretty->string) {
		(void) g_string_free(pretty->string, TRUE);
	}

	if (pretty->tmp_string) {
		(void) g_string_free(pretty->tmp_string, TRUE);
	}

	if (pretty->out != stdout) {
		int ret;

		ret = fclose(pretty->out);
		if (ret) {
			perror("close output file");
		}
	}

	for (i = 0; i < ENUMERATION_MAX_BITFLAGS_COUNT; i++) {
		if (pretty->enum_bit_labels[i]) {
			g_ptr_array_free(pretty->enum_bit_labels[i], true);
		}
	}

	g_free(pretty->options.output_path);
	g_free(pretty);

end:
	return;
}

static
struct pretty_component *create_pretty(void)
{
	struct pretty_component *pretty;

	pretty = g_new0(struct pretty_component, 1);
	if (!pretty) {
		goto end;
	}
	pretty->string = g_string_new("");
	if (!pretty->string) {
		goto error;
	}
	pretty->tmp_string = g_string_new("");
	if (!pretty->tmp_string) {
		goto error;
	}
end:
	return pretty;

error:
	g_free(pretty);
	return NULL;
}

BT_HIDDEN
void pretty_finalize(bt_self_component_sink *comp)
{
	destroy_pretty_data(
		bt_self_component_get_data(
			bt_self_component_sink_as_self_component(comp)));
}

static
bt_message_iterator_class_next_method_status handle_message(
		struct pretty_component *pretty,
		const bt_message *message)
{
	bt_message_iterator_class_next_method_status ret =
		BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK;

	BT_ASSERT_DBG(pretty);

	switch (bt_message_get_type(message)) {
	case BT_MESSAGE_TYPE_EVENT:
		if (pretty_print_event(pretty, message)) {
			BT_COMP_LOGE_APPEND_CAUSE(pretty->self_comp,
				"Failed to print one event.");
			ret = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR;
		}
		break;
	case BT_MESSAGE_TYPE_DISCARDED_EVENTS:
	case BT_MESSAGE_TYPE_DISCARDED_PACKETS:
		if (pretty_print_discarded_items(pretty, message)) {
			BT_COMP_LOGE_APPEND_CAUSE(pretty->self_comp,
				"Failed to print discarded items.");
			ret = BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR;
		}
		break;
	default:
		break;
	}

	return ret;
}

BT_HIDDEN
bt_component_class_sink_graph_is_configured_method_status
pretty_graph_is_configured(bt_self_component_sink *self_comp_sink)
{
	bt_component_class_sink_graph_is_configured_method_status status;
	bt_message_iterator_create_from_sink_component_status
		msg_iter_status;
	struct pretty_component *pretty;
	bt_self_component *self_comp =
		bt_self_component_sink_as_self_component(self_comp_sink);
	bt_self_component_port_input *in_port;

	pretty = bt_self_component_get_data(self_comp);
	BT_ASSERT(pretty);
	BT_ASSERT(!pretty->iterator);

	in_port = bt_self_component_sink_borrow_input_port_by_name(self_comp_sink,
		in_port_name);
	if (!bt_port_is_connected(bt_port_input_as_port_const(
			bt_self_component_port_input_as_port_input(in_port)))) {
		BT_COMP_LOGE_APPEND_CAUSE(self_comp, "Single input port is not connected: "
			"port-name=\"%s\"", in_port_name);
		status = BT_COMPONENT_CLASS_SINK_GRAPH_IS_CONFIGURED_METHOD_STATUS_ERROR;
		goto end;
	}

	msg_iter_status = bt_message_iterator_create_from_sink_component(
		self_comp_sink, in_port, &pretty->iterator);
	if (msg_iter_status != BT_MESSAGE_ITERATOR_CREATE_FROM_SINK_COMPONENT_STATUS_OK) {
		status = (int) msg_iter_status;
		goto end;
	}

	status = BT_COMPONENT_CLASS_SINK_GRAPH_IS_CONFIGURED_METHOD_STATUS_OK;

end:
	return status;
}

BT_HIDDEN
bt_component_class_sink_consume_method_status pretty_consume(
		bt_self_component_sink *comp)
{
	bt_component_class_sink_consume_method_status status;
	bt_message_array_const msgs;
	bt_message_iterator *it;
	struct pretty_component *pretty = bt_self_component_get_data(
		bt_self_component_sink_as_self_component(comp));
	bt_message_iterator_next_status next_status;
	uint64_t count = 0;
	uint64_t i = 0;

	it = pretty->iterator;
	next_status = bt_message_iterator_next(it,
		&msgs, &count);
	if (next_status != BT_MESSAGE_ITERATOR_NEXT_STATUS_OK) {
		status = (int) next_status;
		goto end;
	}

	for (i = 0; i < count; i++) {
		status = (int) handle_message(pretty, msgs[i]);
		if (status) {
			goto end;
		}

		bt_message_put_ref(msgs[i]);
	}

	status = BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_OK;

end:
	for (; i < count; i++) {
		bt_message_put_ref(msgs[i]);
	}

	return status;
}

static
void apply_one_string(const char *key, const bt_value *params, char **option)
{
	const bt_value *value = NULL;
	const char *str;

	value = bt_value_map_borrow_entry_value_const(params, key);
	if (!value) {
		goto end;
	}

	str = bt_value_string_get(value);
	*option = g_strdup(str);

end:
	return;
}

/*
 * Apply parameter with key `key` to `option`.  Use `def` as the value, if
 * the parameter is not specified.
 */

static
void apply_one_bool_with_default(const char *key, const bt_value *params,
		bool *option, bool def)
{
	const bt_value *value;

	value = bt_value_map_borrow_entry_value_const(params, key);
	if (value) {
		bt_bool bool_val = bt_value_bool_get(value);

		*option = (bool) bool_val;
	} else {
		*option = def;
	}
}

static
void apply_one_bool_if_specified(const char *key, const bt_value *params, bool *option)
{
	const bt_value *value;

	value = bt_value_map_borrow_entry_value_const(params, key);
	if (value) {
		bt_bool bool_val = bt_value_bool_get(value);

		*option = (bool) bool_val;
	}
}

static
int open_output_file(struct pretty_component *pretty)
{
	int ret = 0;

	if (!pretty->options.output_path) {
		goto end;
	}

	pretty->out = fopen(pretty->options.output_path, "w");
	if (!pretty->out) {
		goto error;
	}

	goto end;

error:
	ret = -1;

end:
	return ret;
}

static const char *color_choices[] = { "never", "auto", "always", NULL };
static const char *show_hide_choices[] = { "show", "hide", NULL };

static
struct bt_param_validation_map_value_entry_descr pretty_params[] = {
	{ "color", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL, { BT_VALUE_TYPE_STRING, .string = {
		.choices = color_choices,
	} } },
	{ "path", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL, { .type = BT_VALUE_TYPE_STRING } },
	{ "no-delta", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL, { .type = BT_VALUE_TYPE_BOOL } },
	{ "clock-cycles", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL, { .type = BT_VALUE_TYPE_BOOL } },
	{ "clock-seconds", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL, { .type = BT_VALUE_TYPE_BOOL } },
	{ "clock-date", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL, { .type = BT_VALUE_TYPE_BOOL } },
	{ "clock-gmt", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL, { .type = BT_VALUE_TYPE_BOOL } },
	{ "verbose", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL, { .type = BT_VALUE_TYPE_BOOL } },

	{ "name-default", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL, { BT_VALUE_TYPE_STRING, .string = {
		.choices = show_hide_choices,
	} } },
	{ "name-payload", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL, { .type = BT_VALUE_TYPE_BOOL } },
	{ "name-context", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL, { .type = BT_VALUE_TYPE_BOOL } },
	{ "name-scope", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL, { .type = BT_VALUE_TYPE_BOOL } },
	{ "name-header", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL, { .type = BT_VALUE_TYPE_BOOL } },

	{ "field-default", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL, { BT_VALUE_TYPE_STRING, .string = {
		.choices = show_hide_choices,
	} } },
	{ "field-trace", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL, { .type = BT_VALUE_TYPE_BOOL } },
	{ "field-trace:hostname", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL, { .type = BT_VALUE_TYPE_BOOL } },
	{ "field-trace:domain", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL, { .type = BT_VALUE_TYPE_BOOL } },
	{ "field-trace:procname", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL, { .type = BT_VALUE_TYPE_BOOL } },
	{ "field-trace:vpid", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL, { .type = BT_VALUE_TYPE_BOOL } },
	{ "field-loglevel", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL, { .type = BT_VALUE_TYPE_BOOL } },
	{ "field-emf", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL, { .type = BT_VALUE_TYPE_BOOL } },
	{ "field-callsite", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL, { .type = BT_VALUE_TYPE_BOOL } },
	{ "print-enum-flags", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL, { .type = BT_VALUE_TYPE_BOOL } },
	BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END
};

static
bt_component_class_initialize_method_status apply_params(
		struct pretty_component *pretty, const bt_value *params)
{
	int ret;
	const bt_value *value;
	bt_component_class_initialize_method_status status;
	enum bt_param_validation_status validation_status;
	gchar *validate_error = NULL;

	validation_status = bt_param_validation_validate(params,
		pretty_params, &validate_error);
	if (validation_status == BT_PARAM_VALIDATION_STATUS_MEMORY_ERROR) {
		status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR;
		goto end;
	} else if (validation_status == BT_PARAM_VALIDATION_STATUS_VALIDATION_ERROR) {
		status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
		BT_COMP_LOGE_APPEND_CAUSE(pretty->self_comp, "%s", validate_error);
		goto end;
	}

	/* Known parameters. */
	pretty->options.color = PRETTY_COLOR_OPT_AUTO;
	value = bt_value_map_borrow_entry_value_const(params, "color");
	if (value) {
		const char *color = bt_value_string_get(value);

		if (strcmp(color, "never") == 0) {
			pretty->options.color = PRETTY_COLOR_OPT_NEVER;
		} else if (strcmp(color, "auto") == 0) {
			pretty->options.color = PRETTY_COLOR_OPT_AUTO;
		} else {
			BT_ASSERT(strcmp(color, "always") == 0);
			pretty->options.color = PRETTY_COLOR_OPT_ALWAYS;
		}
	}

	apply_one_string("path", params, &pretty->options.output_path);
	ret = open_output_file(pretty);
	if (ret) {
		BT_COMP_LOGE_APPEND_CAUSE(pretty->self_comp,
			"Failed to open output file: %s", validate_error);
		status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
		goto end;
	}

	apply_one_bool_with_default("no-delta", params,
		&pretty->options.print_delta_field, false);
	/* Reverse logic. */
	pretty->options.print_delta_field = !pretty->options.print_delta_field;

	apply_one_bool_with_default("clock-cycles", params,
		&pretty->options.print_timestamp_cycles, false);

	apply_one_bool_with_default("clock-seconds", params,
		&pretty->options.clock_seconds , false);

	apply_one_bool_with_default("clock-date", params,
		&pretty->options.clock_date, false);

	apply_one_bool_with_default("clock-gmt", params,
		&pretty->options.clock_gmt, false);

	apply_one_bool_with_default("verbose", params,
		&pretty->options.verbose, false);

	apply_one_bool_with_default("print-enum-flags", params,
		&pretty->options.print_enum_flags, false);

	/* Names. */
	value = bt_value_map_borrow_entry_value_const(params, "name-default");
	if (value) {
		const char *str = bt_value_string_get(value);

		if (strcmp(str, "show") == 0) {
			pretty->options.name_default = PRETTY_DEFAULT_SHOW;
		} else {
			BT_ASSERT(strcmp(str, "hide") == 0);
			pretty->options.name_default = PRETTY_DEFAULT_HIDE;
		}
	} else {
		pretty->options.name_default = PRETTY_DEFAULT_UNSET;
	}

	switch (pretty->options.name_default) {
	case PRETTY_DEFAULT_UNSET:
		pretty->options.print_payload_field_names = true;
		pretty->options.print_context_field_names = true;
		pretty->options.print_header_field_names = false;
		pretty->options.print_scope_field_names = false;
		break;
	case PRETTY_DEFAULT_SHOW:
		pretty->options.print_payload_field_names = true;
		pretty->options.print_context_field_names = true;
		pretty->options.print_header_field_names = true;
		pretty->options.print_scope_field_names = true;
		break;
	case PRETTY_DEFAULT_HIDE:
		pretty->options.print_payload_field_names = false;
		pretty->options.print_context_field_names = false;
		pretty->options.print_header_field_names = false;
		pretty->options.print_scope_field_names = false;
		break;
	default:
		bt_common_abort();
	}

	apply_one_bool_if_specified("name-payload", params,
		&pretty->options.print_payload_field_names);

	apply_one_bool_if_specified("name-context", params,
		&pretty->options.print_context_field_names);

	apply_one_bool_if_specified("name-header", params,
		&pretty->options.print_header_field_names);

	apply_one_bool_if_specified("name-scope", params,
		&pretty->options.print_scope_field_names);

	/* Fields. */
	value = bt_value_map_borrow_entry_value_const(params, "field-default");
	if (value) {
		const char *str = bt_value_string_get(value);

		if (strcmp(str, "show") == 0) {
			pretty->options.field_default = PRETTY_DEFAULT_SHOW;
		} else {
			BT_ASSERT(strcmp(str, "hide") == 0);
			pretty->options.field_default = PRETTY_DEFAULT_HIDE;
		}
	} else {
		pretty->options.field_default = PRETTY_DEFAULT_UNSET;
	}

	switch (pretty->options.field_default) {
	case PRETTY_DEFAULT_UNSET:
		pretty->options.print_trace_field = false;
		pretty->options.print_trace_hostname_field = true;
		pretty->options.print_trace_domain_field = false;
		pretty->options.print_trace_procname_field = true;
		pretty->options.print_trace_vpid_field = true;
		pretty->options.print_loglevel_field = false;
		pretty->options.print_emf_field = false;
		pretty->options.print_callsite_field = false;
		break;
	case PRETTY_DEFAULT_SHOW:
		pretty->options.print_trace_field = true;
		pretty->options.print_trace_hostname_field = true;
		pretty->options.print_trace_domain_field = true;
		pretty->options.print_trace_procname_field = true;
		pretty->options.print_trace_vpid_field = true;
		pretty->options.print_loglevel_field = true;
		pretty->options.print_emf_field = true;
		pretty->options.print_callsite_field = true;
		break;
	case PRETTY_DEFAULT_HIDE:
		pretty->options.print_trace_field = false;
		pretty->options.print_trace_hostname_field = false;
		pretty->options.print_trace_domain_field = false;
		pretty->options.print_trace_procname_field = false;
		pretty->options.print_trace_vpid_field = false;
		pretty->options.print_loglevel_field = false;
		pretty->options.print_emf_field = false;
		pretty->options.print_callsite_field = false;
		break;
	default:
		bt_common_abort();
	}

	apply_one_bool_if_specified("field-trace", params,
		&pretty->options.print_trace_field);

	apply_one_bool_if_specified("field-trace:hostname", params,
		&pretty->options.print_trace_hostname_field);

	apply_one_bool_if_specified("field-trace:domain", params,
		&pretty->options.print_trace_domain_field);

	apply_one_bool_if_specified("field-trace:procname", params,
		&pretty->options.print_trace_procname_field);

	apply_one_bool_if_specified("field-trace:vpid", params,
		&pretty->options.print_trace_vpid_field);

	apply_one_bool_if_specified("field-loglevel", params,
		&pretty->options.print_loglevel_field);

	apply_one_bool_if_specified("field-emf", params,
		&pretty->options.print_emf_field);

	apply_one_bool_if_specified("field-callsite", params,
		&pretty->options.print_callsite_field);

	pretty_print_init();
	status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK;

end:
	g_free(validate_error);

	return status;
}

static
void set_use_colors(struct pretty_component *pretty)
{
	switch (pretty->options.color) {
	case PRETTY_COLOR_OPT_ALWAYS:
		pretty->use_colors = true;
		break;
	case PRETTY_COLOR_OPT_AUTO:
		pretty->use_colors = pretty->out == stdout &&
			bt_common_colors_supported();
		break;
	case PRETTY_COLOR_OPT_NEVER:
		pretty->use_colors = false;
		break;
	}
}

BT_HIDDEN
bt_component_class_initialize_method_status pretty_init(
		bt_self_component_sink *self_comp_sink,
		bt_self_component_sink_configuration *config,
		const bt_value *params,
		__attribute__((unused)) void *init_method_data)
{
	bt_component_class_initialize_method_status status;
	bt_self_component_add_port_status add_port_status;
	struct pretty_component *pretty = create_pretty();
	bt_self_component *self_comp =
		bt_self_component_sink_as_self_component(self_comp_sink);
	const bt_component *comp = bt_self_component_as_component(self_comp);
	bt_logging_level log_level = bt_component_get_logging_level(comp);

	if (!pretty) {
		/*
		 * Don't use BT_COMP_LOGE_APPEND_CAUSE, as `pretty` is not
		 * initialized yet.
		 */
		BT_COMP_LOG_CUR_LVL(BT_LOG_ERROR, log_level, self_comp,
			"Failed to allocate component.");
		BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_COMPONENT(
			self_comp, "Failed to allocate component.");
		status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR;
		goto error;
	}

	pretty->self_comp = self_comp;
	pretty->log_level = log_level;

	add_port_status = bt_self_component_sink_add_input_port(self_comp_sink,
		in_port_name, NULL, NULL);
	if (add_port_status != BT_SELF_COMPONENT_ADD_PORT_STATUS_OK) {
		status = (int) add_port_status;
		goto error;
	}

	pretty->out = stdout;
	pretty->err = stderr;

	pretty->delta_cycles = -1ULL;
	pretty->last_cycles_timestamp = -1ULL;

	pretty->delta_real_timestamp = -1ULL;
	pretty->last_real_timestamp = -1ULL;

	status = apply_params(pretty, params);
	if (status != BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK) {
		goto error;
	}

	set_use_colors(pretty);

	if (pretty->options.print_enum_flags) {
		uint64_t i;
		/*
		 * Allocate all label arrays during the initialization of the
		 * component and reuse the same set of arrays for all
		 * enumerations.
		 */
		for (i = 0; i < ENUMERATION_MAX_BITFLAGS_COUNT; i++) {
			pretty->enum_bit_labels[i] = g_ptr_array_new();
		}
	}
	bt_self_component_set_data(self_comp, pretty);

	status = BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK;
	goto end;

error:
	destroy_pretty_data(pretty);

end:
	return status;
}
