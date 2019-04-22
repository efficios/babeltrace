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

/* From plugin-const.h */

extern const bt_plugin *bt_plugin_find(const char *plugin_name);

extern const bt_plugin_set *bt_plugin_find_all_from_file(
		const char *path);

extern const bt_plugin_set *bt_plugin_find_all_from_dir(
		const char *path, bt_bool recurse);

extern const bt_plugin_set *bt_plugin_find_all_from_static(void);

extern const char *bt_plugin_get_name(const bt_plugin *plugin);

extern const char *bt_plugin_get_author(const bt_plugin *plugin);

extern const char *bt_plugin_get_license(const bt_plugin *plugin);

extern const char *bt_plugin_get_description(const bt_plugin *plugin);

extern const char *bt_plugin_get_path(const bt_plugin *plugin);

extern bt_property_availability bt_plugin_get_version(
		const bt_plugin *plugin, unsigned int *OUT,
		unsigned int *OUT, unsigned int *OUT, const char **OUT);

extern uint64_t bt_plugin_get_source_component_class_count(
		const bt_plugin *plugin);

extern uint64_t bt_plugin_get_filter_component_class_count(
		const bt_plugin *plugin);

extern uint64_t bt_plugin_get_sink_component_class_count(
		const bt_plugin *plugin);

extern const bt_component_class_source *
bt_plugin_borrow_source_component_class_by_index_const(
		const bt_plugin *plugin, uint64_t index);

extern const bt_component_class_filter *
bt_plugin_borrow_filter_component_class_by_index_const(
		const bt_plugin *plugin, uint64_t index);

extern const bt_component_class_sink *
bt_plugin_borrow_sink_component_class_by_index_const(
		const bt_plugin *plugin, uint64_t index);

extern const bt_component_class_source *
bt_plugin_borrow_source_component_class_by_name_const(
		const bt_plugin *plugin, const char *name);

extern const bt_component_class_filter *
bt_plugin_borrow_filter_component_class_by_name_const(
		const bt_plugin *plugin, const char *name);

extern const bt_component_class_sink *
bt_plugin_borrow_sink_component_class_by_name_const(
		const bt_plugin *plugin, const char *name);

extern void bt_plugin_get_ref(const bt_plugin *plugin);

extern void bt_plugin_put_ref(const bt_plugin *plugin);

/* From plugin-set-const.h */

extern uint64_t bt_plugin_set_get_plugin_count(
		const bt_plugin_set *plugin_set);

extern const bt_plugin *bt_plugin_set_borrow_plugin_by_index_const(
		const bt_plugin_set *plugin_set, uint64_t index);

extern void bt_plugin_set_get_ref(const bt_plugin_set *plugin_set);

extern void bt_plugin_set_put_ref(const bt_plugin_set *plugin_set);
