/*
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2016 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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

#include <babeltrace/babeltrace.h>
#include <babeltrace/compiler-internal.h>
#include <babeltrace/common-internal.h>
#include <plugins-common.h>
#include <stdio.h>
#include <stdbool.h>
#include <glib.h>
#include <babeltrace/assert-internal.h>

#include "pretty.h"

GQuark stream_packet_context_quarks[STREAM_PACKET_CONTEXT_QUARKS_LEN];

static
const char *plugin_options[] = {
	"color",
	"path",
	"no-delta",
	"clock-cycles",
	"clock-seconds",
	"clock-date",
	"clock-gmt",
	"verbose",
	"name-default",		/* show/hide */
	"name-payload",
	"name-context",
	"name-scope",
	"name-header",
	"field-default",	/* show/hide */
	"field-trace",
	"field-trace:hostname",
	"field-trace:domain",
	"field-trace:procname",
	"field-trace:vpid",
	"field-loglevel",
	"field-emf",
	"field-callsite",
};

static
const char * const in_port_name = "in";

static
void destroy_pretty_data(struct pretty_component *pretty)
{
	bt_self_component_port_input_message_iterator_put_ref(pretty->iterator);

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
	g_free(pretty->options.output_path);
	g_free(pretty);
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
bt_self_component_status handle_message(
		struct pretty_component *pretty,
		const bt_message *message)
{
	bt_self_component_status ret = BT_SELF_COMPONENT_STATUS_OK;

	BT_ASSERT(pretty);

	switch (bt_message_get_type(message)) {
	case BT_MESSAGE_TYPE_EVENT:
		if (pretty_print_event(pretty, message)) {
			ret = BT_SELF_COMPONENT_STATUS_ERROR;
		}
		break;
	case BT_MESSAGE_TYPE_MESSAGE_ITERATOR_INACTIVITY:
		fprintf(stderr, "Message iterator inactivity message\n");
		break;
	case BT_MESSAGE_TYPE_DISCARDED_EVENTS:
	case BT_MESSAGE_TYPE_DISCARDED_PACKETS:
		if (pretty_print_discarded_items(pretty, message)) {
			ret = BT_SELF_COMPONENT_STATUS_ERROR;
		}
		break;
	default:
		break;
	}

	return ret;
}

BT_HIDDEN
bt_self_component_status pretty_graph_is_configured(
		bt_self_component_sink *comp)
{
	bt_self_component_status status = BT_SELF_COMPONENT_STATUS_OK;
	struct pretty_component *pretty;

	pretty = bt_self_component_get_data(
			bt_self_component_sink_as_self_component(comp));
	BT_ASSERT(pretty);
	BT_ASSERT(!pretty->iterator);
	pretty->iterator = bt_self_component_port_input_message_iterator_create(
		bt_self_component_sink_borrow_input_port_by_name(comp,
			in_port_name));
	if (!pretty->iterator) {
		status = BT_SELF_COMPONENT_STATUS_NOMEM;
	}

	return status;
}

BT_HIDDEN
bt_self_component_status pretty_consume(
		bt_self_component_sink *comp)
{
	bt_self_component_status ret;
	bt_message_array_const msgs;
	bt_self_component_port_input_message_iterator *it;
	struct pretty_component *pretty = bt_self_component_get_data(
		bt_self_component_sink_as_self_component(comp));
	bt_message_iterator_status it_ret;
	uint64_t count = 0;
	uint64_t i = 0;

	it = pretty->iterator;
	it_ret = bt_self_component_port_input_message_iterator_next(it,
		&msgs, &count);

	switch (it_ret) {
	case BT_MESSAGE_ITERATOR_STATUS_OK:
		break;
	case BT_MESSAGE_ITERATOR_STATUS_NOMEM:
		ret = BT_SELF_COMPONENT_STATUS_NOMEM;
		goto end;
	case BT_MESSAGE_ITERATOR_STATUS_AGAIN:
		ret = BT_SELF_COMPONENT_STATUS_AGAIN;
		goto end;
	case BT_MESSAGE_ITERATOR_STATUS_END:
		ret = BT_SELF_COMPONENT_STATUS_END;
		BT_SELF_COMPONENT_PORT_INPUT_MESSAGE_ITERATOR_PUT_REF_AND_RESET(
			pretty->iterator);
		goto end;
	default:
		ret = BT_SELF_COMPONENT_STATUS_ERROR;
		goto end;
	}

	BT_ASSERT(it_ret == BT_MESSAGE_ITERATOR_STATUS_OK);

	for (i = 0; i < count; i++) {
		ret = handle_message(pretty, msgs[i]);
		if (ret) {
			goto end;
		}

		bt_message_put_ref(msgs[i]);
	}

end:
	for (; i < count; i++) {
		bt_message_put_ref(msgs[i]);
	}

	return ret;
}

static
int add_params_to_map(bt_value *plugin_opt_map)
{
	int ret = 0;
	unsigned int i;

	for (i = 0; i < BT_ARRAY_SIZE(plugin_options); i++) {
		const char *key = plugin_options[i];
		bt_value_status status;

		status = bt_value_map_insert_entry(plugin_opt_map, key,
			bt_value_null);
		switch (status) {
		case BT_VALUE_STATUS_OK:
			break;
		default:
			ret = -1;
			goto end;
		}
	}
end:
	return ret;
}

static
bt_bool check_param_exists(const char *key, const bt_value *object,
		void *data)
{
	struct pretty_component *pretty = data;

	if (!bt_value_map_has_entry(pretty->plugin_opt_map,
				    key)) {
		fprintf(pretty->err,
			"[warning] Parameter \"%s\" unknown to \"text.pretty\" sink component\n", key);
	}
	return BT_TRUE;
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
	if (bt_value_is_null(value)) {
		goto end;
	}
	str = bt_value_string_get(value);
	*option = g_strdup(str);

end:
	return;
}

static
void apply_one_bool(const char *key, const bt_value *params, bool *option,
		bool *found)
{
	const bt_value *value = NULL;
	bt_bool bool_val;

	value = bt_value_map_borrow_entry_value_const(params, key);
	if (!value) {
		goto end;
	}
	bool_val = bt_value_bool_get(value);
	*option = (bool) bool_val;
	if (found) {
		*found = true;
	}

end:
	return;
}

static
void warn_wrong_color_param(struct pretty_component *pretty)
{
	fprintf(pretty->err,
		"[warning] Accepted values for the \"color\" parameter are:\n    \"always\", \"auto\", \"never\"\n");
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

static
int apply_params(struct pretty_component *pretty, const bt_value *params)
{
	int ret = 0;
	bt_value_status status;
	bool value, found;
	char *str = NULL;

	pretty->plugin_opt_map = bt_value_map_create();
	if (!pretty->plugin_opt_map) {
		ret = -1;
		goto end;
	}
	ret = add_params_to_map(pretty->plugin_opt_map);
	if (ret) {
		goto end;
	}
	/* Report unknown parameters. */
	status = bt_value_map_foreach_entry_const(params,
		check_param_exists, pretty);
	switch (status) {
	case BT_VALUE_STATUS_OK:
		break;
	default:
		ret = -1;
		goto end;
	}
	/* Known parameters. */
	pretty->options.color = PRETTY_COLOR_OPT_AUTO;
	if (bt_value_map_has_entry(params, "color")) {
		const bt_value *color_value;
		const char *color;

		color_value = bt_value_map_borrow_entry_value_const(params,
			"color");
		if (!color_value) {
			goto end;
		}

		color = bt_value_string_get(color_value);

		if (strcmp(color, "never") == 0) {
			pretty->options.color = PRETTY_COLOR_OPT_NEVER;
		} else if (strcmp(color, "auto") == 0) {
			pretty->options.color = PRETTY_COLOR_OPT_AUTO;
		} else if (strcmp(color, "always") == 0) {
			pretty->options.color = PRETTY_COLOR_OPT_ALWAYS;
		} else {
			warn_wrong_color_param(pretty);
		}
	}

	apply_one_string("path", params, &pretty->options.output_path);
	ret = open_output_file(pretty);
	if (ret) {
		goto end;
	}

	value = false;		/* Default. */
	apply_one_bool("no-delta", params, &value, NULL);
	pretty->options.print_delta_field = !value;	/* Reverse logic. */

	value = false;		/* Default. */
	apply_one_bool("clock-cycles", params, &value, NULL);
	pretty->options.print_timestamp_cycles = value;

	value = false;		/* Default. */
	apply_one_bool("clock-seconds", params, &value, NULL);
	pretty->options.clock_seconds = value;

	value = false;		/* Default. */
	apply_one_bool("clock-date", params, &value, NULL);
	pretty->options.clock_date = value;

	value = false;		/* Default. */
	apply_one_bool("clock-gmt", params, &value, NULL);
	pretty->options.clock_gmt = value;

	value = false;		/* Default. */
	apply_one_bool("verbose", params, &value, NULL);
	pretty->options.verbose = value;

	/* Names. */
	apply_one_string("name-default", params, &str);
	if (!str) {
		pretty->options.name_default = PRETTY_DEFAULT_UNSET;
	} else if (!strcmp(str, "show")) {
		pretty->options.name_default = PRETTY_DEFAULT_SHOW;
	} else if (!strcmp(str, "hide")) {
		pretty->options.name_default = PRETTY_DEFAULT_HIDE;
	} else {
		ret = -1;
		goto end;
	}
	g_free(str);
	str = NULL;

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
		ret = -1;
		goto end;
	}

	value = false;
	found = false;
	apply_one_bool("name-payload", params, &value, &found);
	if (found) {
		pretty->options.print_payload_field_names = value;
	}

	value = false;
	found = false;
	apply_one_bool("name-context", params, &value, &found);
	if (found) {
		pretty->options.print_context_field_names = value;
	}

	value = false;
	found = false;
	apply_one_bool("name-header", params, &value, &found);
	if (found) {
		pretty->options.print_header_field_names = value;
	}

	value = false;
	found = false;
	apply_one_bool("name-scope", params, &value, &found);
	if (found) {
		pretty->options.print_scope_field_names = value;
	}

	/* Fields. */
	apply_one_string("field-default", params, &str);
	if (!str) {
		pretty->options.field_default = PRETTY_DEFAULT_UNSET;
	} else if (!strcmp(str, "show")) {
		pretty->options.field_default = PRETTY_DEFAULT_SHOW;
	} else if (!strcmp(str, "hide")) {
		pretty->options.field_default = PRETTY_DEFAULT_HIDE;
	} else {
		ret = -1;
		goto end;
	}
	g_free(str);
	str = NULL;

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
		ret = -1;
		goto end;
	}

	value = false;
	found = false;
	apply_one_bool("field-trace", params, &value, &found);
	if (found) {
		pretty->options.print_trace_field = value;
	}

	value = false;
	found = false;
	apply_one_bool("field-trace:hostname", params, &value, &found);
	if (found) {
		pretty->options.print_trace_hostname_field = value;
	}

	value = false;
	found = false;
	apply_one_bool("field-trace:domain", params, &value, &found);
	if (found) {
		pretty->options.print_trace_domain_field = value;
	}

	value = false;
	found = false;
	apply_one_bool("field-trace:procname", params, &value, &found);
	if (found) {
		pretty->options.print_trace_procname_field = value;
	}

	value = false;
	found = false;
	apply_one_bool("field-trace:vpid", params, &value, &found);
	if (found) {
		pretty->options.print_trace_vpid_field = value;
	}

	value = false;
	found = false;
	apply_one_bool("field-loglevel", params, &value, &found);
	if (found) {
		pretty->options.print_loglevel_field = value;
	}

	value = false;
	found = false;
	apply_one_bool("field-emf", params, &value, &found);
	if (found) {
		pretty->options.print_emf_field = value;
	}

	value = false;
	found = false;
	apply_one_bool("field-callsite", params, &value, &found);
	if (found) {
		pretty->options.print_callsite_field = value;
	}

end:
	bt_value_put_ref(pretty->plugin_opt_map);
	pretty->plugin_opt_map = NULL;
	g_free(str);
	return ret;
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

static
void init_stream_packet_context_quarks(void)
{
	stream_packet_context_quarks[Q_TIMESTAMP_BEGIN] =
		g_quark_from_string("timestamp_begin");
	stream_packet_context_quarks[Q_TIMESTAMP_BEGIN] =
		g_quark_from_string("timestamp_begin");
	stream_packet_context_quarks[Q_TIMESTAMP_END] =
		g_quark_from_string("timestamp_end");
	stream_packet_context_quarks[Q_EVENTS_DISCARDED] =
		g_quark_from_string("events_discarded");
	stream_packet_context_quarks[Q_CONTENT_SIZE] =
		g_quark_from_string("content_size");
	stream_packet_context_quarks[Q_PACKET_SIZE] =
		g_quark_from_string("packet_size");
	stream_packet_context_quarks[Q_PACKET_SEQ_NUM] =
		g_quark_from_string("packet_seq_num");
}

BT_HIDDEN
bt_self_component_status pretty_init(
		bt_self_component_sink *comp,
		const bt_value *params,
		UNUSED_VAR void *init_method_data)
{
	bt_self_component_status ret;
	struct pretty_component *pretty = create_pretty();

	if (!pretty) {
		ret = BT_SELF_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	ret = bt_self_component_sink_add_input_port(comp, in_port_name,
		NULL, NULL);
	if (ret != BT_SELF_COMPONENT_STATUS_OK) {
		goto end;
	}

	pretty->out = stdout;
	pretty->err = stderr;

	pretty->delta_cycles = -1ULL;
	pretty->last_cycles_timestamp = -1ULL;

	pretty->delta_real_timestamp = -1ULL;
	pretty->last_real_timestamp = -1ULL;

	if (apply_params(pretty, params)) {
		ret = BT_SELF_COMPONENT_STATUS_ERROR;
		goto error;
	}

	set_use_colors(pretty);
	bt_self_component_set_data(
		bt_self_component_sink_as_self_component(comp), pretty);
	init_stream_packet_context_quarks();

end:
	return ret;

error:
	destroy_pretty_data(pretty);
	return ret;
}
