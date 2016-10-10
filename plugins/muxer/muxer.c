/*
 * muxer.c
 *
 * Babeltrace Trace Muxer
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
#include <babeltrace/plugin/filter.h>
#include <babeltrace/plugin/notification/notification.h>
#include <babeltrace/plugin/notification/iterator.h>
#include <babeltrace/plugin/notification/event.h>
#include "muxer.h"

static
void destroy_muxer_data(struct muxer *muxer)
{
	g_free(muxer);
}

static
struct muxer *create_muxer(void)
{
	struct muxer *muxer;

	muxer = g_new0(struct muxer, 1);
	if (!muxer) {
		goto end;
	}
end:
	return muxer;
}

static
void destroy_muxer(struct bt_component *component)
{
	void *data = bt_component_get_private_data(component);

	destroy_muxer_data(data);
}

enum bt_component_status muxer_component_init(
	struct bt_component *component, struct bt_value *params)
{
	enum bt_component_status ret;
	struct muxer *muxer = create_muxer();

	if (!muxer) {
		ret = BT_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	ret = bt_component_set_destroy_cb(component,
			destroy_muxer);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto error;
	}

	ret = bt_component_set_private_data(component, muxer);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto error;
	}
end:
	return ret;
error:
	destroy_muxer_data(muxer);
	return ret;
}

/* Initialize plug-in entry points. */
BT_PLUGIN_NAME("muxer");
BT_PLUGIN_DESCRIPTION("Babeltrace Trace Muxer Plug-In.");
BT_PLUGIN_AUTHOR("Jérémie Galarneau");
BT_PLUGIN_LICENSE("MIT");

BT_PLUGIN_COMPONENT_CLASSES_BEGIN
BT_PLUGIN_FILTER_COMPONENT_CLASS_ENTRY("muxer",
		"Time-correlate multiple traces.",
		muxer_component_init)
BT_PLUGIN_COMPONENT_CLASSES_END
