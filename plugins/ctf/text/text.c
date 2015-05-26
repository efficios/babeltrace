/*
 * text.c
 *
 * Babeltrace CTF Text Output Plugin
 *
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/plugin/plugin-lib.h>
#include <babeltrace/plugin/plugin-system.h>
#include <babeltrace/plugin/plugin.h>
#include <babeltrace/plugin/sink.h>
#include <glib.h>
#include <stdio.h>

const char *plugin_name = "ctf-text";

enum loglevel {
        LOGLEVEL_EMERG                  = 0,
        LOGLEVEL_ALERT                  = 1,
        LOGLEVEL_CRIT                   = 2,
        LOGLEVEL_ERR                    = 3,
        LOGLEVEL_WARNING                = 4,
        LOGLEVEL_NOTICE                 = 5,
        LOGLEVEL_INFO                   = 6,
        LOGLEVEL_DEBUG_SYSTEM           = 7,
        LOGLEVEL_DEBUG_PROGRAM          = 8,
        LOGLEVEL_DEBUG_PROCESS          = 9,
        LOGLEVEL_DEBUG_MODULE           = 10,
        LOGLEVEL_DEBUG_UNIT             = 11,
        LOGLEVEL_DEBUG_FUNCTION         = 12,
        LOGLEVEL_DEBUG_LINE             = 13,
        LOGLEVEL_DEBUG                  = 14,
};

const char *loglevel_str [] = {
	[LOGLEVEL_EMERG] = "TRACE_EMERG",
	[LOGLEVEL_ALERT] = "TRACE_ALERT",
	[LOGLEVEL_CRIT] = "TRACE_CRIT",
	[LOGLEVEL_ERR] = "TRACE_ERR",
	[LOGLEVEL_WARNING] = "TRACE_WARNING",
	[LOGLEVEL_NOTICE] = "TRACE_NOTICE",
	[LOGLEVEL_INFO] = "TRACE_INFO",
	[LOGLEVEL_DEBUG_SYSTEM] = "TRACE_DEBUG_SYSTEM",
	[LOGLEVEL_DEBUG_PROGRAM] = "TRACE_DEBUG_PROGRAM",
	[LOGLEVEL_DEBUG_PROCESS] = "TRACE_DEBUG_PROCESS",
	[LOGLEVEL_DEBUG_MODULE] = "TRACE_DEBUG_MODULE",
	[LOGLEVEL_DEBUG_UNIT] = "TRACE_DEBUG_UNIT",
	[LOGLEVEL_DEBUG_FUNCTION] = "TRACE_DEBUG_FUNCTION",
	[LOGLEVEL_DEBUG_LINE] = "TRACE_DEBUG_LINE",
	[LOGLEVEL_DEBUG] = "TRACE_DEBUG",
};

struct ctf_text {
	int opt_print_all_field_names;
	int opt_print_scope_field_names;
	int opt_print_header_field_names;
	int opt_print_context_field_names;
	int opt_print_payload_field_names;
	int opt_print_all_fields;
	int opt_print_trace_field;
	int opt_print_trace_domain_field;
	int opt_print_trace_procname_field;
	int opt_print_trace_vpid_field;
	int opt_print_trace_hostname_field;
	int opt_print_trace_default_fields;
	int opt_print_loglevel_field;
	int opt_print_emf_field;
	int opt_print_callsite_field;
	int opt_print_delta_field;
};

static
void ctf_text_destroy(struct bt_plugin *plugin)
{
	struct ctf_text *text;

	if (!plugin) {
		return;
	}

        text = bt_plugin_get_private_data(plugin);
	if (!text) {
		return;
	}

	g_free(text);
}

static
int ctf_text_handle_notification(struct bt_plugin *plugin,
		struct bt_notification *notification)
{
	return BT_PLUGIN_STATUS_OK;
}

enum bt_plugin_type bt_plugin_lib_get_type(void)
{
	return BT_PLUGIN_TYPE_SINK;
}

const char *bt_plugin_lib_get_format_name(void)
{
	return plugin_name;
}

static
int text_init(struct ctf_text *text, struct bt_object *params)
{
	int ret = 0;

	if (!text || !params) {
		ret = -1;
		goto end;
	}

	text->opt_print_trace_default_fields = 1;
	text->opt_print_delta_field = 1;
end:
	return ret;
}

struct bt_plugin *bt_plugin_lib_create(struct bt_object *params)
{
	int ret;
	struct bt_plugin *plugin = NULL;
	struct ctf_text *text = g_new0(struct ctf_text, 1);

	/* Set default text output options */
	ret = text_init(text, params);
	if (ret) {
		goto error;
	}

	plugin = bt_plugin_sink_create(plugin_name, text,
	        ctf_text_destroy, ctf_text_handle_notification);
	if (!plugin) {
		goto error;
	}

end:
	return plugin;
error:
	if (text) {
		g_free(text);
	}
	goto end;
}
