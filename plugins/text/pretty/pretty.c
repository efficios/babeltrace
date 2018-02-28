/*
 * pretty.c
 *
 * Babeltrace CTF Text Output Plugin
 *
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
#include <babeltrace/values.h>
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
void destroy_pretty_data(struct pretty_component *pretty)
{
	bt_put(pretty->input_iterator);

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
void pretty_finalize(struct bt_private_component *component)
{
	void *data = bt_private_component_get_user_data(component);

	destroy_pretty_data(data);
}

static
enum bt_component_status handle_notification(struct pretty_component *pretty,
		struct bt_notification *notification)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;

	BT_ASSERT(pretty);

	switch (bt_notification_get_type(notification)) {
	case BT_NOTIFICATION_TYPE_EVENT:
		ret = pretty_print_event(pretty, notification);
		break;
	case BT_NOTIFICATION_TYPE_INACTIVITY:
		fprintf(stderr, "Inactivity notification\n");
		break;
	case BT_NOTIFICATION_TYPE_PACKET_BEGIN:
	case BT_NOTIFICATION_TYPE_PACKET_END:
	case BT_NOTIFICATION_TYPE_STREAM_BEGIN:
	case BT_NOTIFICATION_TYPE_STREAM_END:
		break;
	case BT_NOTIFICATION_TYPE_DISCARDED_PACKETS:
	case BT_NOTIFICATION_TYPE_DISCARDED_EVENTS:
		ret = pretty_print_discarded_elements(pretty, notification);
		break;
	default:
		fprintf(stderr, "Unhandled notification type\n");
	}

	return ret;
}

BT_HIDDEN
void pretty_port_connected(
		struct bt_private_component *component,
		struct bt_private_port *self_port,
		struct bt_port *other_port)
{
	enum bt_connection_status conn_status;
	struct bt_private_connection *connection;
	struct pretty_component *pretty;
	static const enum bt_notification_type notif_types[] = {
		BT_NOTIFICATION_TYPE_EVENT,
		BT_NOTIFICATION_TYPE_DISCARDED_PACKETS,
		BT_NOTIFICATION_TYPE_DISCARDED_EVENTS,
		BT_NOTIFICATION_TYPE_SENTINEL,
	};

	pretty = bt_private_component_get_user_data(component);
	BT_ASSERT(pretty);
	BT_ASSERT(!pretty->input_iterator);
	connection = bt_private_port_get_private_connection(self_port);
	BT_ASSERT(connection);
	conn_status = bt_private_connection_create_notification_iterator(
		connection, notif_types, &pretty->input_iterator);
	if (conn_status != BT_CONNECTION_STATUS_OK) {
		pretty->error = true;
	}

	bt_put(connection);
}

BT_HIDDEN
enum bt_component_status pretty_consume(struct bt_private_component *component)
{
	enum bt_component_status ret;
	struct bt_notification *notification = NULL;
	struct bt_notification_iterator *it;
	struct pretty_component *pretty =
		bt_private_component_get_user_data(component);
	enum bt_notification_iterator_status it_ret;

	if (unlikely(pretty->error)) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	it = pretty->input_iterator;
	it_ret = bt_notification_iterator_next(it);

	switch (it_ret) {
	case BT_NOTIFICATION_ITERATOR_STATUS_END:
		ret = BT_COMPONENT_STATUS_END;
		BT_PUT(pretty->input_iterator);
		goto end;
	case BT_NOTIFICATION_ITERATOR_STATUS_AGAIN:
		ret = BT_COMPONENT_STATUS_AGAIN;
		goto end;
	case BT_NOTIFICATION_ITERATOR_STATUS_OK:
		break;
	default:
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	notification = bt_notification_iterator_get_notification(it);
	BT_ASSERT(notification);
	ret = handle_notification(pretty, notification);

end:
	bt_put(notification);
	return ret;
}

static
enum bt_component_status add_params_to_map(struct bt_value *plugin_opt_map)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	unsigned int i;

	for (i = 0; i < BT_ARRAY_SIZE(plugin_options); i++) {
		const char *key = plugin_options[i];
		enum bt_value_status status;

		status = bt_value_map_insert(plugin_opt_map, key, bt_value_null);
		switch (status) {
		case BT_VALUE_STATUS_OK:
			break;
		default:
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}
	}
end:
	return ret;
}

static
bt_bool check_param_exists(const char *key, struct bt_value *object, void *data)
{
	struct pretty_component *pretty = data;
	struct bt_value *plugin_opt_map = pretty->plugin_opt_map;

	if (!bt_value_map_get(plugin_opt_map, key)) {
		fprintf(pretty->err,
			"[warning] Parameter \"%s\" unknown to \"text.pretty\" sink component\n", key);
	}
	return BT_TRUE;
}

static
enum bt_component_status apply_one_string(const char *key,
		struct bt_value *params,
		char **option)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_value *value = NULL;
	enum bt_value_status status;
	const char *str;

	value = bt_value_map_get(params, key);
	if (!value) {
		goto end;
	}
	if (bt_value_is_null(value)) {
		goto end;
	}
	status = bt_value_string_get(value, &str);
	switch (status) {
	case BT_VALUE_STATUS_OK:
		break;
	default:
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	*option = g_strdup(str);
end:
	bt_put(value);
	return ret;
}

static
enum bt_component_status apply_one_bool(const char *key,
		struct bt_value *params,
		bool *option,
		bool *found)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_value *value = NULL;
	enum bt_value_status status;
	bt_bool bool_val;

	value = bt_value_map_get(params, key);
	if (!value) {
		goto end;
	}
	status = bt_value_bool_get(value, &bool_val);
	switch (status) {
	case BT_VALUE_STATUS_OK:
		break;
	default:
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	*option = (bool) bool_val;
	if (found) {
		*found = true;
	}
end:
	bt_put(value);
	return ret;
}

static
void warn_wrong_color_param(struct pretty_component *pretty)
{
	fprintf(pretty->err,
		"[warning] Accepted values for the \"color\" parameter are:\n    \"always\", \"auto\", \"never\"\n");
}

static
enum bt_component_status open_output_file(struct pretty_component *pretty)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;

	if (!pretty->options.output_path) {
		goto end;
	}

	pretty->out = fopen(pretty->options.output_path, "w");
	if (!pretty->out) {
		goto error;
	}

	goto end;

error:
	ret = BT_COMPONENT_STATUS_ERROR;
end:
	return ret;
}

static
enum bt_component_status apply_params(struct pretty_component *pretty,
		struct bt_value *params)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	enum bt_value_status status;
	bool value, found;
	char *str = NULL;

	pretty->plugin_opt_map = bt_value_map_create();
	if (!pretty->plugin_opt_map) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	ret = add_params_to_map(pretty->plugin_opt_map);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	/* Report unknown parameters. */
	status = bt_value_map_foreach(params, check_param_exists, pretty);
	switch (status) {
	case BT_VALUE_STATUS_OK:
		break;
	default:
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	/* Known parameters. */
	pretty->options.color = PRETTY_COLOR_OPT_AUTO;
	if (bt_value_map_has_key(params, "color")) {
		struct bt_value *color_value;
		const char *color;

		color_value = bt_value_map_get(params, "color");
		if (!color_value) {
			goto end;
		}

		status = bt_value_string_get(color_value, &color);
		if (status) {
			warn_wrong_color_param(pretty);
		} else {
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

		bt_put(color_value);
	}

	ret = apply_one_string("path", params, &pretty->options.output_path);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	ret = open_output_file(pretty);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

	value = false;		/* Default. */
	ret = apply_one_bool("no-delta", params, &value, NULL);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	pretty->options.print_delta_field = !value;	/* Reverse logic. */

	value = false;		/* Default. */
	ret = apply_one_bool("clock-cycles", params, &value, NULL);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	pretty->options.print_timestamp_cycles = value;

	value = false;		/* Default. */
	ret = apply_one_bool("clock-seconds", params, &value, NULL);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	pretty->options.clock_seconds = value;

	value = false;		/* Default. */
	ret = apply_one_bool("clock-date", params, &value, NULL);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	pretty->options.clock_date = value;

	value = false;		/* Default. */
	ret = apply_one_bool("clock-gmt", params, &value, NULL);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	pretty->options.clock_gmt = value;

	value = false;		/* Default. */
	ret = apply_one_bool("verbose", params, &value, NULL);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	pretty->options.verbose = value;

	/* Names. */
	ret = apply_one_string("name-default", params, &str);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	if (!str) {
		pretty->options.name_default = PRETTY_DEFAULT_UNSET;
	} else if (!strcmp(str, "show")) {
		pretty->options.name_default = PRETTY_DEFAULT_SHOW;
	} else if (!strcmp(str, "hide")) {
		pretty->options.name_default = PRETTY_DEFAULT_HIDE;
	} else {
		ret = BT_COMPONENT_STATUS_ERROR;
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
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	value = false;
	found = false;
	ret = apply_one_bool("name-payload", params, &value, &found);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	if (found) {
		pretty->options.print_payload_field_names = value;
	}

	value = false;
	found = false;
	ret = apply_one_bool("name-context", params, &value, &found);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	if (found) {
		pretty->options.print_context_field_names = value;
	}

	value = false;
	found = false;
	ret = apply_one_bool("name-header", params, &value, &found);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	if (found) {
		pretty->options.print_header_field_names = value;
	}

	value = false;
	found = false;
	ret = apply_one_bool("name-scope", params, &value, &found);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	if (found) {
		pretty->options.print_scope_field_names = value;
	}

	/* Fields. */
	ret = apply_one_string("field-default", params, &str);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	if (!str) {
		pretty->options.field_default = PRETTY_DEFAULT_UNSET;
	} else if (!strcmp(str, "show")) {
		pretty->options.field_default = PRETTY_DEFAULT_SHOW;
	} else if (!strcmp(str, "hide")) {
		pretty->options.field_default = PRETTY_DEFAULT_HIDE;
	} else {
		ret = BT_COMPONENT_STATUS_ERROR;
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
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	value = false;
	found = false;
	ret = apply_one_bool("field-trace", params, &value, &found);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	if (found) {
		pretty->options.print_trace_field = value;
	}

	value = false;
	found = false;
	ret = apply_one_bool("field-trace:hostname", params, &value, &found);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	if (found) {
		pretty->options.print_trace_hostname_field = value;
	}

	value = false;
	found = false;
	ret = apply_one_bool("field-trace:domain", params, &value, &found);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	if (found) {
		pretty->options.print_trace_domain_field = value;
	}

	value = false;
	found = false;
	ret = apply_one_bool("field-trace:procname", params, &value, &found);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	if (found) {
		pretty->options.print_trace_procname_field = value;
	}

	value = false;
	found = false;
	ret = apply_one_bool("field-trace:vpid", params, &value, &found);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	if (found) {
		pretty->options.print_trace_vpid_field = value;
	}

	value = false;
	found = false;
	ret = apply_one_bool("field-loglevel", params, &value, &found);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	if (found) {
		pretty->options.print_loglevel_field = value;
	}

	value = false;
	found = false;
	ret = apply_one_bool("field-emf", params, &value, &found);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	if (found) {
		pretty->options.print_emf_field = value;
	}

	value = false;
	found = false;
	ret = apply_one_bool("field-callsite", params, &value, &found);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	if (found) {
		pretty->options.print_callsite_field = value;
	}

end:
	bt_put(pretty->plugin_opt_map);
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
enum bt_component_status pretty_init(
		struct bt_private_component *component,
		struct bt_value *params,
		UNUSED_VAR void *init_method_data)
{
	enum bt_component_status ret;
	struct pretty_component *pretty = create_pretty();

	if (!pretty) {
		ret = BT_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	ret = bt_private_component_sink_add_input_private_port(component,
		"in", NULL, NULL);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

	pretty->out = stdout;
	pretty->err = stderr;

	pretty->delta_cycles = -1ULL;
	pretty->last_cycles_timestamp = -1ULL;

	pretty->delta_real_timestamp = -1ULL;
	pretty->last_real_timestamp = -1ULL;

	ret = apply_params(pretty, params);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto error;
	}

	set_use_colors(pretty);
	ret = bt_private_component_set_user_data(component, pretty);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto error;
	}

	init_stream_packet_context_quarks();

end:
	return ret;
error:
	destroy_pretty_data(pretty);
	return ret;
}
