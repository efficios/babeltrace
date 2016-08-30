/*
 * fs.c
 *
 * Babeltrace CTF file system Reader Component
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

#include <babeltrace/plugin/plugin-system.h>
#include <babeltrace/plugin/notification/iterator.h>
#include <glib.h>
#include <assert.h>
#include "fs-internal.h"

static bool ctf_fs_debug;

static
struct bt_notification *ctf_fs_iterator_get(
		struct bt_notification_iterator *iterator)
{
	return NULL;
}

static
enum bt_notification_iterator_status ctf_fs_iterator_next(
		struct bt_notification_iterator *iterator)
{
	return BT_NOTIFICATION_ITERATOR_STATUS_UNSUPPORTED;
}

static
void ctf_fs_iterator_destroy_data(struct ctf_fs_iterator *ctf_it)
{
	g_free(ctf_it);
}

static
void ctf_fs_iterator_destroy(struct bt_notification_iterator *it)
{
	void *data = bt_notification_iterator_get_private_data(it);

	ctf_fs_iterator_destroy_data(data);
}

static
enum bt_component_status ctf_fs_iterator_init(struct bt_component *source,
		struct bt_notification_iterator *it)
{
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;
	struct ctf_fs_iterator *ctf_it;

	assert(source && it);
	ctf_it = g_new0(struct ctf_fs_iterator, 1);
	if (!ctf_it) {
		ret = BT_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	ret = bt_notification_iterator_set_get_cb(it, ctf_fs_iterator_get);
	if (ret) {
		goto error;
	}

	ret = bt_notification_iterator_set_next_cb(it, ctf_fs_iterator_next);
	if (ret) {
		goto error;
	}

	ret = bt_notification_iterator_set_destroy_cb(it,
			ctf_fs_iterator_destroy);
	if (ret) {
		goto error;
	}

	ret = bt_notification_iterator_set_private_data(it, ctf_it);
	if (ret) {
		goto error;
	}
end:
	return ret;
error:
	(void) bt_notification_iterator_set_private_data(it, NULL);
	ctf_fs_iterator_destroy_data(ctf_it);
	return ret;
}

static
struct ctf_fs_component *ctf_fs_create(struct bt_value *params)
{
	return g_new0(struct ctf_fs_component, 1);
}

static
void ctf_fs_destroy_data(struct ctf_fs_component *component)
{
	g_free(component);
}

static
void ctf_fs_destroy(struct bt_component *component)
{
	void *data = bt_component_get_private_data(component);

	ctf_fs_destroy_data(data);
}

BT_HIDDEN
enum bt_component_status ctf_fs_init(struct bt_component *source,
		struct bt_value *params)
{
	struct ctf_fs_component *ctf_fs;
	enum bt_component_status ret = BT_COMPONENT_STATUS_OK;

	assert(source);
	ctf_fs_debug = g_strcmp0(getenv("CTF_FS_DEBUG"), "1") == 0;
	ctf_fs = ctf_fs_create(params);
	if (!ctf_fs) {
		ret = BT_COMPONENT_STATUS_NOMEM;
		goto end;
	}

	ret = bt_component_set_destroy_cb(source, ctf_fs_destroy);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto error;
	}

	ret = bt_component_set_private_data(source, ctf_fs);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto error;
	}

	ret = bt_component_source_set_iterator_init_cb(source,
			ctf_fs_iterator_init);
	if (ret != BT_COMPONENT_STATUS_OK) {
		goto error;
	}
end:
	return ret;
error:
	(void) bt_component_set_private_data(source, NULL);
        ctf_fs_destroy_data(ctf_fs);
	return ret;
}
