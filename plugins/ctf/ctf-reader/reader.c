/*
 * reader.c
 *
 * Babeltrace CTF Reader Plugin
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
#include <glib.h>
#include <stdio.h>

const char *plugin_name = "ctf";

struct ctf_reader {
	int a;
};

enum bt_plugin_type bt_plugin_lib_get_type(void)
{
	return BT_PLUGIN_TYPE_SOURCE;
}

const char *bt_plugin_lib_get_format_name(void)
{
	return plugin_name;
}

static
void ctf_reader_destroy(struct bt_plugin *plugin)
{
	struct ctf_reader *reader;

	if (!plugin) {
		return;
	}

	reader = bt_plugin_get_private_data(plugin);
	if (!reader) {
		return;
	}

	g_free(reader);
}

static
struct bt_notification_iterator *ctf_reader_iterator_create(
		struct bt_plugin *plugin)
{
	return NULL;
}

struct bt_plugin *bt_plugin_lib_create(struct bt_object *params)
{
	struct bt_plugin *plugin = NULL;
	struct ctf_reader *reader = g_new0(struct ctf_reader, 1);

	plugin = bt_plugin_source_create(plugin_name, reader,
		ctf_reader_destroy, ctf_reader_iterator_create);
	if (!plugin) {
		goto error;
	}

end:
	return plugin;
error:
	if (reader) {
		g_free(reader);
	}
	goto end;
}
