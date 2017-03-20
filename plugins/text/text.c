/*
 * text.c
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

#include <babeltrace/plugin/plugin-dev.h>
#include <babeltrace/graph/component.h>
#include <babeltrace/graph/private-component.h>
#include <babeltrace/graph/component-sink.h>
#include <babeltrace/graph/port.h>
#include <babeltrace/graph/private-port.h>
#include <babeltrace/graph/connection.h>
#include <babeltrace/graph/private-connection.h>
#include <babeltrace/graph/notification.h>
#include <babeltrace/graph/notification-iterator.h>
#include <babeltrace/graph/notification-event.h>
#include <babeltrace/values.h>
#include <babeltrace/compiler.h>
#include <babeltrace/common-internal.h>
#include <plugins-common.h>
#include <stdio.h>
#include <stdbool.h>
#include <glib.h>
#include "text.h"
#include <assert.h>

static
const char *plugin_options[] = {
	"color",
	"output-path",
	"debug-info-dir",
	"debug-info-target-prefix",
	"debug-info-full-path",
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
void destroy_text_data(struct text_component *text)
{
	bt_put(text->input_iterator);
	(void) g_string_free(text->string, TRUE);
	g_free(text->options.output_path);
	g_free(text->options.debug_info_dir);
	g_free(text->options.debug_info_target_prefix);
	g_free(text);
}

static
struct text_component *create_text(void)
{
	struct text_component *text;

	text = g_new0(struct text_component, 1);
	if (!text) {
		goto end;
	}
	text->string = g_string_new("");
	if (!text->string) {
		goto error;
	}
end:
	return text;

error:
	g_free(text);
	return NULL;
}

static
void finalize_text(struct bt_private_component *component)
{
	void *data = bt_private_component_get_user_data(component);

	destroy_text_data(data);
}

static
enum bt_component_status handle_notification(struct text_component *text,
		struct bt_notification *notification)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;

	if (!text) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	switch (bt_notification_get_type(notification)) {
	case BT_NOTIFICATION_TYPE_PACKET_BEGIN:
		break;
	case BT_NOTIFICATION_TYPE_PACKET_END:
		break;
	case BT_NOTIFICATION_TYPE_EVENT:
	{
		struct bt_ctf_event *event = bt_notification_event_get_event(
				notification);

		if (!event) {
			ret = BT_COMPONENT_STATUS_ERROR;
			goto end;
		}
		ret = text_print_event(text, event);
		bt_put(event);
		if (ret != BT_COMPONENT_STATUS_OK) {
			goto end;
		}
		break;
	}
	case BT_NOTIFICATION_TYPE_STREAM_END:
		break;
	default:
		puts("Unhandled notification type");
	}
end:
	return ret;
}

static
enum bt_component_status text_accept_port_connection(
		struct bt_private_component *component,
		struct bt_private_port *self_port,
		struct bt_port *other_port)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_private_connection *connection;
	struct text_component *text;

	text = bt_private_component_get_user_data(component);
	assert(text);
	assert(!text->input_iterator);
	connection = bt_private_port_get_private_connection(self_port);
	assert(connection);
	text->input_iterator =
		bt_private_connection_create_notification_iterator(connection);

	if (!text->input_iterator) {
		ret = BT_COMPONENT_STATUS_ERROR;
	}

	bt_put(connection);
	return ret;
}

static
enum bt_component_status run(struct bt_private_component *component)
{
	enum bt_component_status ret;
	struct bt_notification *notification = NULL;
	struct bt_notification_iterator *it;
	struct text_component *text =
		bt_private_component_get_user_data(component);
	enum bt_notification_iterator_status it_ret;

	it = text->input_iterator;

	it_ret = bt_notification_iterator_next(it);
	switch (it_ret) {
	case BT_NOTIFICATION_ITERATOR_STATUS_ERROR:
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	case BT_NOTIFICATION_ITERATOR_STATUS_END:
		ret = BT_COMPONENT_STATUS_END;
		BT_PUT(text->input_iterator);
		goto end;
	default:
		break;
	}

	notification = bt_notification_iterator_get_notification(it);
	assert(notification);
	ret = handle_notification(text, notification);
	text->processed_first_event = true;
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
bool check_param_exists(const char *key, struct bt_value *object, void *data)
{
	struct text_component *text = data;
	struct bt_value *plugin_opt_map = text->plugin_opt_map;

	if (!bt_value_map_get(plugin_opt_map, key)) {
		fprintf(text->err,
			"[warning] Parameter \"%s\" unknown to \"text\" plugin\n", key);
	}
	return true;
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

	value = bt_value_map_get(params, key);
	if (!value) {
		goto end;
	}
	status = bt_value_bool_get(value, option);
	switch (status) {
	case BT_VALUE_STATUS_OK:
		break;
	default:
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	if (found) {
		*found = true;
	}
end:
	bt_put(value);
	return ret;
}

static
void warn_wrong_color_param(struct text_component *text)
{
	fprintf(text->err,
		"[warning] Accepted values for the \"color\" parameter are:\n    \"always\", \"auto\", \"never\"\n");
}

static
enum bt_component_status apply_params(struct text_component *text,
		struct bt_value *params)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	enum bt_value_status status;
	bool value, found;
	char *str = NULL;

	text->plugin_opt_map = bt_value_map_create();
	if (!text->plugin_opt_map) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	ret = add_params_to_map(text->plugin_opt_map);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	/* Report unknown parameters. */
	status = bt_value_map_foreach(params, check_param_exists, text);
	switch (status) {
	case BT_VALUE_STATUS_OK:
		break;
	default:
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	/* Known parameters. */
	text->options.color = TEXT_COLOR_OPT_AUTO;
	if (bt_value_map_has_key(params, "color")) {
		struct bt_value *color_value;
		const char *color;

		color_value = bt_value_map_get(params, "color");
		if (!color_value) {
			goto end;
		}

		status = bt_value_string_get(color_value, &color);
		if (status) {
			warn_wrong_color_param(text);
		} else {
			if (strcmp(color, "never") == 0) {
				text->options.color = TEXT_COLOR_OPT_NEVER;
			} else if (strcmp(color, "auto") == 0) {
				text->options.color = TEXT_COLOR_OPT_AUTO;
			} else if (strcmp(color, "always") == 0) {
				text->options.color = TEXT_COLOR_OPT_ALWAYS;
			} else {
				warn_wrong_color_param(text);
			}
		}

		bt_put(color_value);
	}

	ret = apply_one_string("output-path",
			params,
			&text->options.output_path);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

	ret = apply_one_string("debug-info-dir",
			params,
			&text->options.debug_info_dir);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

	ret = apply_one_string("debug-info-target-prefix",
			params,
			&text->options.debug_info_target_prefix);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

	value = false;		/* Default. */
	ret = apply_one_bool("debug-info-full-path", params, &value, NULL);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	text->options.debug_info_full_path = value;

	value = false;		/* Default. */
	ret = apply_one_bool("no-delta", params, &value, NULL);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	text->options.print_delta_field = !value;	/* Reverse logic. */

	value = false;		/* Default. */
	ret = apply_one_bool("clock-cycles", params, &value, NULL);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	text->options.print_timestamp_cycles = value;

	value = false;		/* Default. */
	ret = apply_one_bool("clock-seconds", params, &value, NULL);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	text->options.clock_seconds = value;

	value = false;		/* Default. */
	ret = apply_one_bool("clock-date", params, &value, NULL);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	text->options.clock_date = value;

	value = false;		/* Default. */
	ret = apply_one_bool("clock-gmt", params, &value, NULL);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	text->options.clock_gmt = value;

	value = false;		/* Default. */
	ret = apply_one_bool("verbose", params, &value, NULL);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	text->options.verbose = value;

	/* Names. */
	ret = apply_one_string("name-default", params, &str);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	if (!str) {
		text->options.name_default = TEXT_DEFAULT_UNSET;
	} else if (!strcmp(str, "show")) {
		text->options.name_default = TEXT_DEFAULT_SHOW;
	} else if (!strcmp(str, "hide")) {
		text->options.name_default = TEXT_DEFAULT_HIDE;
	} else {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	g_free(str);
	str = NULL;

	switch (text->options.name_default) {
	case TEXT_DEFAULT_UNSET:
		text->options.print_payload_field_names = true;
		text->options.print_context_field_names = true;
		text->options.print_header_field_names = false;
		text->options.print_scope_field_names = false;
		break;
	case TEXT_DEFAULT_SHOW:
		text->options.print_payload_field_names = true;
		text->options.print_context_field_names = true;
		text->options.print_header_field_names = true;
		text->options.print_scope_field_names = true;
		break;
	case TEXT_DEFAULT_HIDE:
		text->options.print_payload_field_names = false;
		text->options.print_context_field_names = false;
		text->options.print_header_field_names = false;
		text->options.print_scope_field_names = false;
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
		text->options.print_payload_field_names = value;
	}

	value = false;
	found = false;
	ret = apply_one_bool("name-context", params, &value, &found);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	if (found) {
		text->options.print_context_field_names = value;
	}

	value = false;
	found = false;
	ret = apply_one_bool("name-header", params, &value, &found);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	if (found) {
		text->options.print_header_field_names = value;
	}

	value = false;
	found = false;
	ret = apply_one_bool("name-scope", params, &value, &found);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	if (found) {
		text->options.print_scope_field_names = value;
	}

	/* Fields. */
	ret = apply_one_string("field-default", params, &str);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	if (!str) {
		text->options.field_default = TEXT_DEFAULT_UNSET;
	} else if (!strcmp(str, "show")) {
		text->options.field_default = TEXT_DEFAULT_SHOW;
	} else if (!strcmp(str, "hide")) {
		text->options.field_default = TEXT_DEFAULT_HIDE;
	} else {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}
	g_free(str);
	str = NULL;

	switch (text->options.field_default) {
	case TEXT_DEFAULT_UNSET:
		text->options.print_trace_field = false;
		text->options.print_trace_hostname_field = true;
		text->options.print_trace_domain_field = false;
		text->options.print_trace_procname_field = true;
		text->options.print_trace_vpid_field = true;
		text->options.print_loglevel_field = false;
		text->options.print_emf_field = false;
		text->options.print_callsite_field = false;
		break;
	case TEXT_DEFAULT_SHOW:
		text->options.print_trace_field = true;
		text->options.print_trace_hostname_field = true;
		text->options.print_trace_domain_field = true;
		text->options.print_trace_procname_field = true;
		text->options.print_trace_vpid_field = true;
		text->options.print_loglevel_field = true;
		text->options.print_emf_field = true;
		text->options.print_callsite_field = true;
		break;
	case TEXT_DEFAULT_HIDE:
		text->options.print_trace_field = false;
		text->options.print_trace_hostname_field = false;
		text->options.print_trace_domain_field = false;
		text->options.print_trace_procname_field = false;
		text->options.print_trace_vpid_field = false;
		text->options.print_loglevel_field = false;
		text->options.print_emf_field = false;
		text->options.print_callsite_field = false;
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
		text->options.print_trace_field = value;
	}

	value = false;
	found = false;
	ret = apply_one_bool("field-trace:hostname", params, &value, &found);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	if (found) {
		text->options.print_trace_hostname_field = value;
	}

	value = false;
	found = false;
	ret = apply_one_bool("field-trace:domain", params, &value, &found);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	if (found) {
		text->options.print_trace_domain_field = value;
	}

	value = false;
	found = false;
	ret = apply_one_bool("field-trace:procname", params, &value, &found);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	if (found) {
		text->options.print_trace_procname_field = value;
	}

	value = false;
	found = false;
	ret = apply_one_bool("field-trace:vpid", params, &value, &found);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	if (found) {
		text->options.print_trace_vpid_field = value;
	}

	value = false;
	found = false;
	ret = apply_one_bool("field-loglevel", params, &value, &found);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	if (found) {
		text->options.print_loglevel_field = value;
	}

	value = false;
	found = false;
	ret = apply_one_bool("field-emf", params, &value, &found);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	if (found) {
		text->options.print_emf_field = value;
	}

	value = false;
	found = false;
	ret = apply_one_bool("field-callsite", params, &value, &found);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}
	if (found) {
		text->options.print_callsite_field = value;
	}

end:
	bt_put(text->plugin_opt_map);
	text->plugin_opt_map = NULL;
	g_free(str);
	return ret;
}

static
void set_use_colors(struct text_component *text)
{
	switch (text->options.color) {
	case TEXT_COLOR_OPT_ALWAYS:
		text->use_colors = true;
		break;
	case TEXT_COLOR_OPT_AUTO:
		text->use_colors = text->out == stdout &&
			bt_common_colors_supported();
		break;
	case TEXT_COLOR_OPT_NEVER:
		text->use_colors = false;
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

static
enum bt_component_status text_component_init(
		struct bt_private_component *component,
		struct bt_value *params,
		UNUSED_VAR void *init_method_data)
{
	enum bt_component_status ret;
	struct text_component *text = create_text();

	if (!text) {
		ret = BT_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	text->out = stdout;
	text->err = stderr;

	text->delta_cycles = -1ULL;
	text->last_cycles_timestamp = -1ULL;

	text->delta_real_timestamp = -1ULL;
	text->last_real_timestamp = -1ULL;

	ret = apply_params(text, params);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto error;
	}

	set_use_colors(text);

	ret = bt_private_component_set_user_data(component, text);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto error;
	}

	init_stream_packet_context_quarks();

end:
	return ret;
error:
	destroy_text_data(text);
	return ret;
}

/* Initialize plug-in entry points. */
BT_PLUGIN(text);
BT_PLUGIN_DESCRIPTION("Babeltrace text output plug-in.");
BT_PLUGIN_AUTHOR("Jérémie Galarneau");
BT_PLUGIN_LICENSE("MIT");
BT_PLUGIN_SINK_COMPONENT_CLASS(text, run);
BT_PLUGIN_SINK_COMPONENT_CLASS_INIT_METHOD(text, text_component_init);
BT_PLUGIN_SINK_COMPONENT_CLASS_ACCEPT_PORT_CONNECTION_METHOD(text, text_accept_port_connection);
BT_PLUGIN_SINK_COMPONENT_CLASS_FINALIZE_METHOD(text, finalize_text);
BT_PLUGIN_SINK_COMPONENT_CLASS_DESCRIPTION(text,
	"Formats CTF-IR to text. Formerly known as ctf-text.");
