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
#include <babeltrace/plugin/notification/iterator.h>
#include <babeltrace/plugin/notification/event.h>
#include <stdio.h>
#include <stdbool.h>
#include <glib.h>
#include "text.h"

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

static
void destroy_text_data(struct text_component *text)
{
	(void) g_string_free(text->string, TRUE);
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
void destroy_text(struct bt_component *component)
{
	void *data = bt_component_get_private_data(component);

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
	case BT_NOTIFICATION_TYPE_PACKET_START:
		puts("<packet>");
		break;
	case BT_NOTIFICATION_TYPE_PACKET_END:
		puts("</packet>");
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
		puts("</stream>");
		break;
	default:
		puts("Unhandled notification type");
	}
end:
	return ret;
}

static
enum bt_component_status run(struct bt_component *component)
{
	enum bt_component_status ret;
	struct bt_notification *notification = NULL;
	struct bt_notification_iterator *it;
	struct text_component *text = bt_component_get_private_data(component);

	ret = bt_component_sink_get_input_iterator(component, 0, &it);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto end;
	}

	if (!text->processed_first_event) {
		ret = bt_notification_iterator_next(it);
		if (ret != BT_COMPONENT_STATUS_OK) {
			goto end;
		}
	} else {
		text->processed_first_event = true;
	}

	notification = bt_notification_iterator_get_notification(it);
	if (!notification) {
		ret = BT_COMPONENT_STATUS_ERROR;
		goto end;
	}

	ret = handle_notification(text, notification);
end:
	bt_put(it);
	bt_put(notification);
	return ret;
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

	ret = bt_component_sink_set_consume_cb(component,
			run);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto error;
	}

	text->out = stdout;
	text->err = stderr;
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
BT_PLUGIN_SINK_COMPONENT_CLASS_ENTRY("text",
		"Formats CTF-IR to text. Formerly known as ctf-text.",
		text_component_init)
BT_PLUGIN_COMPONENT_CLASSES_END
