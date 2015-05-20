/*
 * plugin.c
 *
 * Babeltrace Plugin
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

const char *bt_plugin_get_name(struct bt_plugin *plugin)
{
	const char *ret = NULL;

	if (!plugin) {
		goto end;
	}

	ret = plugin->name->str;
end:
	return ret;
}

enum bt_plugin_status bt_plugin_set_name(struct bt_plugin *plugin,
		const char *name)
{
	enum bt_plugin_status ret = BT_PLUGIN_STATUS_OK;

	if (!plugin || !name || name[0] == '\0') {
		ret = BT_PLUGIN_STATUS_INVAL;
		goto end;
	}

	g_string_assign(plugin->name, name);
end:
	return ret;
}

enum bt_plugin_type bt_plugin_get_type(struct bt_plugin *plugin)
{
	enum bt_plugin_type type = BT_PLUGIN_TYPE_UNKNOWN;

	if (!plugin) {
		goto end;
	}

	type = plugin->type;
end:
	return type;
}

enum bt_plugin_status bt_plugin_set_error_stream(
		struct bt_plugin *plugin, FILE *stream)
{
	enum bt_plugin_status ret = BT_PLUGIN_STATUS_OK;

	if (!plugin) {
		ret = BT_PLUGIN_STATUS_INVAL;
		goto end;
	}

	plugin->error_stream = stream;
end:
	return ret;
}

void bt_plugin_get(struct bt_plugin *plugin)
{
	if (!plugin) {
		return;
	}

	bt_ctf_ref_get(&plugin->ref_count);
}

void bt_plugin_put(struct bt_plugin *plugin)
{
	if (!plugin) {
		return;
	}

	bt_ctf_ref_put(&plugin->ref_count);
}
