/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
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
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/* Types */
struct bt_plugin;
struct bt_plugin_set;

/* Status */
enum bt_plugin_status {
	BT_PLUGIN_STATUS_OK = 0,
	BT_PLUGIN_STATUS_ERROR = -1,
	BT_PLUGIN_STATUS_NOMEM = -4,
};

/* Plugin functions */
struct bt_plugin *bt_plugin_find(const char *plugin_name);
struct bt_component_class *bt_plugin_find_component_class(
		const char *plugin_name, const char *component_class_name,
		enum bt_component_class_type component_class_type);
struct bt_plugin_set *bt_plugin_create_all_from_file(const char *path);
struct bt_plugin_set *bt_plugin_create_all_from_dir(const char *path,
		int recurse);
struct bt_plugin_set *bt_plugin_create_all_from_static(void);
const char *bt_plugin_get_name(struct bt_plugin *plugin);
const char *bt_plugin_get_author(struct bt_plugin *plugin);
const char *bt_plugin_get_license(struct bt_plugin *plugin);
const char *bt_plugin_get_description(struct bt_plugin *plugin);
const char *bt_plugin_get_path(struct bt_plugin *plugin);
enum bt_plugin_status bt_plugin_get_version(struct bt_plugin *plugin,
		unsigned int *OUTPUTINIT, unsigned int *OUTPUTINIT,
		unsigned int *OUTPUTINIT, const char **BTOUTSTR);
int64_t bt_plugin_get_component_class_count(struct bt_plugin *plugin);
struct bt_component_class *bt_plugin_get_component_class_by_index(
		struct bt_plugin *plugin, uint64_t index);
struct bt_component_class *bt_plugin_get_component_class_by_name_and_type(
		struct bt_plugin *plugin, const char *name,
		enum bt_component_class_type type);

/* Plugin set functions */
int64_t bt_plugin_set_get_plugin_count(struct bt_plugin_set *plugin_set);
struct bt_plugin *bt_plugin_set_get_plugin(struct bt_plugin_set *plugin_set,
		uint64_t index);
