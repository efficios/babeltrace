/*
 * text.c
 *
 * Babeltrace CTF Text Output Plugin
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

#include <babeltrace/plugin/plugin-macros.h>
#include <babeltrace/plugin/component.h>
#include <babeltrace/plugin/sink.h>
#include <babeltrace/plugin/notification/notification.h>
#include <stdio.h>
#include <stdbool.h>
#include <glib.h>

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

static
const char *loglevel_str [] = {
	[LOGLEVEL_EMERG] =		"TRACE_EMERG",
	[LOGLEVEL_ALERT] =		"TRACE_ALERT",
	[LOGLEVEL_CRIT] =		"TRACE_CRIT",
	[LOGLEVEL_ERR] =		"TRACE_ERR",
	[LOGLEVEL_WARNING] =		"TRACE_WARNING",
	[LOGLEVEL_NOTICE] =		"TRACE_NOTICE",
	[LOGLEVEL_INFO] =		"TRACE_INFO",
	[LOGLEVEL_DEBUG_SYSTEM] =	"TRACE_DEBUG_SYSTEM",
	[LOGLEVEL_DEBUG_PROGRAM] =	"TRACE_DEBUG_PROGRAM",
	[LOGLEVEL_DEBUG_PROCESS] =	"TRACE_DEBUG_PROCESS",
	[LOGLEVEL_DEBUG_MODULE] =	"TRACE_DEBUG_MODULE",
	[LOGLEVEL_DEBUG_UNIT] =		"TRACE_DEBUG_UNIT",
	[LOGLEVEL_DEBUG_FUNCTION] =	"TRACE_DEBUG_FUNCTION",
	[LOGLEVEL_DEBUG_LINE] =		"TRACE_DEBUG_LINE",
	[LOGLEVEL_DEBUG] =		"TRACE_DEBUG",
};

struct text_options {
	bool opt_print_all_field_names : 1;
	bool opt_print_scope_field_names : 1;
	bool opt_print_header_field_names : 1;
	bool opt_print_context_field_names : 1;
	bool opt_print_payload_field_names : 1;
	bool opt_print_all_fields : 1;
	bool opt_print_trace_field : 1;
	bool opt_print_trace_domain_field : 1;
	bool opt_print_trace_procname_field : 1;
	bool opt_print_trace_vpid_field : 1;
	bool opt_print_trace_hostname_field : 1;
	bool opt_print_trace_default_fields : 1;
	bool opt_print_loglevel_field : 1;
	bool opt_print_emf_field : 1;
	bool opt_print_delta_field : 1;
};

struct text_component {
	struct text_options options;
};

static
struct text_component *create_text(void)
{
	return g_new0(struct text_component, 1);
}

static
void destroy_text_data(struct text_component *data)
{
	g_free(data);
}

static void destroy_text(struct bt_component *component)
{
	void *data = bt_component_get_private_data(component);

	destroy_text_data(data);
}

static
enum bt_component_status handle_notification(struct bt_component *component,
	struct bt_notification *notification)
{
	switch (bt_notification_get_type(notification)) {
	case BT_NOTIFICATION_TYPE_PACKET_START:
		puts("<packet>");
		break;
	case BT_NOTIFICATION_TYPE_PACKET_END:
		puts("</packet>");
		break;
	case BT_NOTIFICATION_TYPE_EVENT:
		puts("<event>");
		break;
	default:
		puts("Unhandled notification type");
	}
	return BT_COMPONENT_STATUS_OK;
}

static
enum bt_component_status text_component_init(
	struct bt_component *component, struct bt_value *params)
{
	enum bt_component_status ret;
	struct text_component *text = create_text();

	if (!text) {
		ret = BT_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	ret = bt_component_set_destroy_cb(component,
			destroy_text);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto error;
	}

	ret = bt_component_set_private_data(component, text);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto error;
	}

	ret = bt_component_sink_set_handle_notification_cb(component,
			handle_notification);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto error;
	}
end:
	return ret;
error:
	destroy_text_data(text);
	return ret;
}

/* Initialize plug-in entry points. */
BT_PLUGIN_NAME("text");
BT_PLUGIN_DESCRIPTION("Babeltrace text output plug-in.");
BT_PLUGIN_AUTHOR("Jérémie Galarneau");
BT_PLUGIN_LICENSE("MIT");

BT_PLUGIN_COMPONENT_CLASSES_BEGIN
BT_PLUGIN_SINK_COMPONENT_CLASS_ENTRY("text", "Formats CTF-IR to text. Formerly known as ctf-text.", text_component_init)
BT_PLUGIN_COMPONENT_CLASSES_END
