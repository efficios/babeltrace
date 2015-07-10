/*
 * sink.c
 *
 * Babeltrace Sink Component
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

#include <babeltrace/compiler.h>
#include <babeltrace/plugin/sink-internal.h>
#include <babeltrace/plugin/component-internal.h>

static
void bt_component_sink_destroy(struct bt_component *component)
{
	struct bt_component_sink *sink;

	if (!component) {
		return;
	}

	sink = container_of(component, struct bt_component_sink, parent);
	g_free(sink);
}

struct bt_component *bt_component_sink_create(const char *name,
		void *private_data, bt_component_destroy_cb destroy_func,
		bt_component_sink_handle_notification_cb notification_cb)
{
	struct bt_component_sink *sink = NULL;
	enum bt_component_status ret;

	if (!notification_cb) {
		goto end;
	}

	sink = g_new0(struct bt_component_sink, 1);
	if (!sink) {
		goto end;
	}

	ret = bt_component_init(&sink->parent, name, private_data,
				destroy_func, BT_COMPONENT_TYPE_SINK,
				bt_component_sink_destroy);
	if (ret != BT_COMPONENT_STATUS_OK) {
		g_free(sink);
		sink = NULL;
		goto end;
	}

	sink->handle_notification = notification_cb;
end:
	return sink ? &sink->parent : NULL;
}

enum bt_component_status bt_component_sink_handle_notification(
		struct bt_component *component,
		struct bt_notification *notification)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct bt_component_sink *sink = NULL;

	if (!component || !notification) {
		ret = BT_COMPONENT_STATUS_INVAL;
		goto end;
	}

	if (bt_component_get_type(component) != BT_COMPONENT_TYPE_SINK) {
		ret = BT_COMPONENT_STATUS_UNSUPPORTED;
		goto end;
	}

	sink = container_of(component, struct bt_component_sink, parent);
	assert(sink->handle_notification);
	ret = sink->handle_notification(component, notification);
end:
	return ret;
}
