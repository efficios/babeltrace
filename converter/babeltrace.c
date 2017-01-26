/*
 * babeltrace.c
 *
 * Babeltrace Trace Converter
 *
 * Copyright 2010-2011 EfficiOS Inc. and Linux Foundation
 *
 * Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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

#include <babeltrace/babeltrace.h>
#include <babeltrace/plugin/plugin.h>
#include <babeltrace/component/component.h>
#include <babeltrace/component/component-source.h>
#include <babeltrace/component/component-sink.h>
#include <babeltrace/component/component-filter.h>
#include <babeltrace/component/component-class.h>
#include <babeltrace/component/notification/iterator.h>
#include <babeltrace/ref.h>
#include <babeltrace/values.h>
#include <stdlib.h>
#include <babeltrace/ctf-ir/metadata.h>	/* for clocks */
#include <popt.h>
#include <string.h>
#include <stdio.h>
#include <glib.h>
#include "babeltrace-cfg.h"
#include "default-cfg.h"

GPtrArray *loaded_plugins;

static
void init_loaded_plugins_array(void)
{
	loaded_plugins = g_ptr_array_new_full(8, bt_put);
}

static
void fini_loaded_plugins_array(void)
{
	g_ptr_array_free(loaded_plugins, TRUE);
}

static
struct bt_plugin *find_plugin(const char *name)
{
	int i;
	struct bt_plugin *plugin = NULL;

	for (i = 0; i < loaded_plugins->len; i++) {
		plugin = g_ptr_array_index(loaded_plugins, i);

		if (strcmp(name, bt_plugin_get_name(plugin)) == 0) {
			break;
		}

		plugin = NULL;
	}

	return bt_get(plugin);
}

static
struct bt_component_class *find_component_class(const char *plugin_name,
		const char *comp_class_name,
		enum bt_component_class_type comp_class_type)
{
	struct bt_component_class *comp_class = NULL;
	struct bt_plugin *plugin = find_plugin(plugin_name);

	if (!plugin) {
		goto end;
	}

	comp_class = bt_plugin_get_component_class_by_name_and_type(plugin,
			comp_class_name, comp_class_type);
	BT_PUT(plugin);
end:
	return comp_class;
}

static
const char *component_type_str(enum bt_component_class_type type)
{
	switch (type) {
	case BT_COMPONENT_CLASS_TYPE_SOURCE:
		return "source";
	case BT_COMPONENT_CLASS_TYPE_SINK:
		return "sink";
	case BT_COMPONENT_CLASS_TYPE_FILTER:
		return "filter";
	case BT_COMPONENT_CLASS_TYPE_UNKNOWN:
	default:
		return "unknown";
	}
}

static
void print_component_classes_found(void)
{
	int plugins_count, component_classes_count = 0, i;

	if (!babeltrace_verbose) {
		return;
	}

	plugins_count = loaded_plugins->len;
	if (plugins_count == 0) {
		fprintf(stderr, "No plugins found. Please make sure your plug-in search path is set correctly.\n");
		return;
	}

	for (i = 0; i < plugins_count; i++) {
		struct bt_plugin *plugin = g_ptr_array_index(loaded_plugins, i);

		component_classes_count += bt_plugin_get_component_class_count(plugin);
	}

	printf_verbose("Found %d component classes in %d plugins.\n",
		component_classes_count, plugins_count);

	for (i = 0; i < plugins_count; i++) {
		int j;
		struct bt_plugin *plugin = g_ptr_array_index(loaded_plugins, i);

		component_classes_count =
			bt_plugin_get_component_class_count(plugin);

		for (j = 0; j < component_classes_count; j++) {
			struct bt_component_class *comp_class =
				bt_plugin_get_component_class(plugin, j);
			const char *plugin_name = bt_plugin_get_name(plugin);
			const char *comp_class_name =
				bt_component_class_get_name(comp_class);
			const char *path = bt_plugin_get_path(plugin);
			const char *author = bt_plugin_get_author(plugin);
			const char *license = bt_plugin_get_license(plugin);
			const char *plugin_description =
				bt_plugin_get_description(plugin);
			const char *comp_class_description =
				bt_component_class_get_description(comp_class);
			enum bt_component_class_type type =
				bt_component_class_get_type(comp_class);

			printf_verbose("[%s - %s (%s)]\n", plugin_name,
				comp_class_name, component_type_str(type));
			printf_verbose("\tpath: %s\n", path ? path : "None");
			printf_verbose("\tauthor: %s\n",
				author ? author : "Unknown");
			printf_verbose("\tlicense: %s\n",
				license ? license : "Unknown");
			printf_verbose("\tplugin description: %s\n",
				plugin_description ? plugin_description : "None");
			printf_verbose("\tcomponent description: %s\n",
				comp_class_description ? comp_class_description : "None");
			bt_put(comp_class);
		}
	}
}

static
void print_indent(size_t indent)
{
	size_t i;

	for (i = 0; i < indent; i++) {
		printf_verbose(" ");
	}
}

static
void print_value(struct bt_value *, size_t, bool);

static
bool print_map_value(const char *key, struct bt_value *object, void *data)
{
	size_t indent = (size_t) data;

	print_indent(indent);
	printf_verbose("\"%s\": ", key);
	print_value(object, indent, false);

	return true;
}

static
void print_value(struct bt_value *value, size_t indent, bool do_indent)
{
	bool bool_val;
	int64_t int_val;
	double dbl_val;
	const char *str_val;
	int size;
	int i;

	if (!value) {
		return;
	}

	if (do_indent) {
		print_indent(indent);
	}

	switch (bt_value_get_type(value)) {
	case BT_VALUE_TYPE_NULL:
		printf_verbose("null\n");
		break;
	case BT_VALUE_TYPE_BOOL:
		bt_value_bool_get(value, &bool_val);
		printf_verbose("%s\n", bool_val ? "true" : "false");
		break;
	case BT_VALUE_TYPE_INTEGER:
		bt_value_integer_get(value, &int_val);
		printf_verbose("%" PRId64 "\n", int_val);
		break;
	case BT_VALUE_TYPE_FLOAT:
		bt_value_float_get(value, &dbl_val);
		printf_verbose("%lf\n", dbl_val);
		break;
	case BT_VALUE_TYPE_STRING:
		bt_value_string_get(value, &str_val);
		printf_verbose("\"%s\"\n", str_val);
		break;
	case BT_VALUE_TYPE_ARRAY:
		size = bt_value_array_size(value);
		printf_verbose("[\n");

		for (i = 0; i < size; i++) {
			struct bt_value *element =
					bt_value_array_get(value, i);

			print_value(element, indent + 2, true);
			BT_PUT(element);
		}

		print_indent(indent);
		printf_verbose("]\n");
		break;
	case BT_VALUE_TYPE_MAP:
		if (bt_value_map_is_empty(value)) {
			printf_verbose("{}\n");
			return;
		}

		printf_verbose("{\n");
		bt_value_map_foreach(value, print_map_value,
			(void *) (indent + 2));
		print_indent(indent);
		printf_verbose("}\n");
		break;
	default:
		assert(false);
	}
}

static
void print_bt_config_component(struct bt_config_component *bt_config_component)
{
	printf_verbose("  %s.%s\n", bt_config_component->plugin_name->str,
		bt_config_component->component_name->str);
	printf_verbose("    params:\n");
	print_value(bt_config_component->params, 6, true);
}

static
void print_bt_config_components(GPtrArray *array)
{
	size_t i;

	for (i = 0; i < array->len; i++) {
		struct bt_config_component *cfg_component =
				bt_config_get_component(array, i);
		print_bt_config_component(cfg_component);
		BT_PUT(cfg_component);
	}
}

static
void print_cfg(struct bt_config *cfg)
{
	printf_verbose("debug:           %d\n", cfg->debug);
	printf_verbose("verbose:         %d\n", cfg->verbose);
	printf_verbose("do list:         %d\n", cfg->do_list);
	printf_verbose("force correlate: %d\n", cfg->force_correlate);
	printf_verbose("plugin paths:\n");
	print_value(cfg->plugin_paths, 2, true);
	printf_verbose("sources:\n");
	print_bt_config_components(cfg->sources);
	printf_verbose("sinks:\n");
	print_bt_config_components(cfg->sinks);
}

static
struct bt_component *create_trimmer(struct bt_config_component *source_cfg)
{
	struct bt_component *trimmer = NULL;
	struct bt_component_class *trimmer_class = NULL;
	struct bt_value *trimmer_params = NULL;
	struct bt_value *value;

	trimmer_params = bt_value_map_create();
	if (!trimmer_params) {
		goto end;
	}

	value = bt_value_map_get(source_cfg->params, "begin");
	if (value) {
	        enum bt_value_status ret;

		ret = bt_value_map_insert(trimmer_params, "begin",
				value);
	        BT_PUT(value);
		if (ret) {
			goto end;
		}
	}
	value = bt_value_map_get(source_cfg->params, "end");
	if (value) {
		enum bt_value_status ret;

		ret = bt_value_map_insert(trimmer_params, "end",
				value);
		BT_PUT(value);
		if (ret) {
			goto end;
		}
	}
	value = bt_value_map_get(source_cfg->params, "clock-gmt");
	if (value) {
		enum bt_value_status ret;

		ret = bt_value_map_insert(trimmer_params, "clock-gmt",
				value);
		BT_PUT(value);
		if (ret) {
			goto end;
		}
	}

	trimmer_class = find_component_class("utils", "trimmer",
		BT_COMPONENT_CLASS_TYPE_FILTER);
	if (!trimmer_class) {
		fprintf(stderr, "Could not find trimmer component class. Aborting...\n");
		goto end;
	}
	trimmer = bt_component_create(trimmer_class, "source_trimmer",
			trimmer_params);
	if (!trimmer) {
		goto end;
	}
end:
	bt_put(trimmer_params);
	bt_put(trimmer_class);
	return trimmer;
}

static
int connect_source_sink(struct bt_component *source,
		struct bt_config_component *source_cfg,
		struct bt_component *sink)
{
	int ret = 0;
	enum bt_component_status sink_status;
	struct bt_component *trimmer = NULL;
	struct bt_notification_iterator *source_it = NULL;
	struct bt_notification_iterator *to_sink_it = NULL;

	source_it = bt_component_source_create_iterator(source);
	if (!source_it) {
		fprintf(stderr, "Failed to instantiate source iterator. Aborting...\n");
                ret = -1;
                goto end;
        }

	if (bt_value_map_has_key(source_cfg->params, "begin")
			|| bt_value_map_has_key(source_cfg->params, "end")) {
		/* A trimmer must be inserted in the graph. */
		enum bt_component_status trimmer_status;

		trimmer = create_trimmer(source_cfg);
		if (!trimmer) {
			fprintf(stderr, "Failed to create trimmer component. Aborting...\n");
			ret = -1;
			goto end;
		}

		trimmer_status = bt_component_filter_add_iterator(trimmer,
				source_it);
		BT_PUT(source_it);
		if (trimmer_status != BT_COMPONENT_STATUS_OK) {
			fprintf(stderr, "Failed to connect source to trimmer. Aborting...\n");
			ret = -1;
			goto end;
		}

		to_sink_it = bt_component_filter_create_iterator(trimmer);
		if (!to_sink_it) {
			fprintf(stderr, "Failed to instantiate trimmer iterator. Aborting...\n");
			ret = -1;
			goto end;
		}
	} else {
		BT_MOVE(to_sink_it, source_it);
	}

	sink_status = bt_component_sink_add_iterator(sink, to_sink_it);
	if (sink_status != BT_COMPONENT_STATUS_OK) {
		fprintf(stderr, "Failed to connect to sink component. Aborting...\n");
		ret = -1;
		goto end;
	}
end:
	bt_put(trimmer);
	bt_put(source_it);
	bt_put(to_sink_it);
	return ret;
}

static
void add_to_loaded_plugins(struct bt_plugin **plugins)
{
	while (*plugins) {
		struct bt_plugin *plugin = *plugins;
		/* Check if it's already loaded (from another path). */
		struct bt_plugin *loaded_plugin =
				find_plugin(bt_plugin_get_name(plugin));

		if (loaded_plugin) {
			printf_verbose("Not loading plugin `%s`: already loaded from `%s`\n",
					bt_plugin_get_path(plugin),
					bt_plugin_get_path(loaded_plugin));
			BT_PUT(loaded_plugin);
			BT_PUT(plugin);
		} else {
			/* Transfer ownership to global array. */
			g_ptr_array_add(loaded_plugins, plugin);
		}
		*(plugins++) = NULL;
	}
}

static
int load_dynamic_plugins(struct bt_config *cfg)
{
	int nr_paths, i, ret = 0;

	nr_paths = bt_value_array_size(cfg->plugin_paths);
	if (nr_paths < 0) {
		ret = -1;
		goto end;
	}

	for (i = 0; i < nr_paths; i++) {
		struct bt_value *plugin_path_value = NULL;
		const char *plugin_path;
		struct bt_plugin **plugins;

		plugin_path_value = bt_value_array_get(cfg->plugin_paths, i);
		if (bt_value_string_get(plugin_path_value,
				&plugin_path)) {
			BT_PUT(plugin_path_value);
			continue;
		}

		plugins = bt_plugin_create_all_from_dir(plugin_path, true);
		if (!plugins) {
			printf_debug("Unable to dynamically load plugins from path %s.\n",
				plugin_path);
			BT_PUT(plugin_path_value);
			continue;
		}

		add_to_loaded_plugins(plugins);
		free(plugins);

		BT_PUT(plugin_path_value);
	}
end:
	return ret;
}

static
int load_static_plugins(void)
{
	int ret = 0;
	struct bt_plugin **plugins;

	plugins = bt_plugin_create_all_from_static();
	if (!plugins) {
		printf_debug("Unable to load static plugins.\n");
		ret = -1;
		goto end;
	}

	add_to_loaded_plugins(plugins);
	free(plugins);
end:
	return ret;
}

int main(int argc, const char **argv)
{
	int ret;
	struct bt_component_class *source_class = NULL;
	struct bt_component_class *sink_class = NULL;
	struct bt_component *source = NULL, *sink = NULL;
	struct bt_value *source_params = NULL, *sink_params = NULL;
	struct bt_config *cfg;
	enum bt_component_status sink_status;
	struct bt_config_component *source_cfg = NULL, *sink_cfg = NULL;

	init_loaded_plugins_array();

	cfg = bt_config_create();
	if (!cfg) {
		fprintf(stderr, "Failed to create Babeltrace configuration\n");
		ret = 1;
		goto end;
	}

	ret = set_default_config(cfg);
	if (ret) {
		goto end;
	}

	ret = bt_config_init_from_args(cfg, argc, argv);
	if (ret == 0) {
		babeltrace_verbose = cfg->verbose;
		babeltrace_debug = cfg->debug;
		print_cfg(cfg);
	} else {
		goto end;
	}

	/* TODO handle more than 1 source and 1 sink. */
	if (cfg->sources->len != 1 || cfg->sinks->len != 1) {
		ret = -1;
		goto end;
	}

	printf_verbose("Verbose mode active.\n");
	printf_debug("Debug mode active.\n");

	if (load_dynamic_plugins(cfg)) {
		fprintf(stderr, "Failed to load dynamic plugins.\n");
		ret = -1;
		goto end;
	}

	if (load_static_plugins()) {
		fprintf(stderr, "Failed to load static plugins.\n");
		goto end;
	}

	print_component_classes_found();
	source_cfg = bt_config_get_component(cfg->sources, 0);
	source_params = bt_get(source_cfg->params);
	source_class = find_component_class(source_cfg->plugin_name->str,
			source_cfg->component_name->str,
			BT_COMPONENT_CLASS_TYPE_SOURCE);
	if (!source_class) {
		fprintf(stderr, "Could not find %s.%s source component class. Aborting...\n",
				source_cfg->plugin_name->str,
				source_cfg->component_name->str);
		ret = -1;
		goto end;
	}

	sink_cfg = bt_config_get_component(cfg->sinks, 0);
	sink_params = bt_get(sink_cfg->params);
	sink_class = find_component_class(sink_cfg->plugin_name->str,
			sink_cfg->component_name->str,
			BT_COMPONENT_CLASS_TYPE_SINK);
	if (!sink_class) {
		fprintf(stderr, "Could not find %s.%s output component class. Aborting...\n",
				sink_cfg->plugin_name->str,
				sink_cfg->component_name->str);
		ret = -1;
		goto end;
	}

	source = bt_component_create(source_class, "source", source_params);
	if (!source) {
		fprintf(stderr, "Failed to instantiate selected source component. Aborting...\n");
                ret = -1;
                goto end;
        }

	sink = bt_component_create(sink_class, "sink", sink_params);
	if (!sink) {
		fprintf(stderr, "Failed to instantiate selected output component. Aborting...\n");
		ret = -1;
		goto end;
	}

	ret = connect_source_sink(source, source_cfg, sink);
	if (ret) {
		goto end;
	}

	while (true) {
		sink_status = bt_component_sink_consume(sink);
		switch (sink_status) {
		case BT_COMPONENT_STATUS_AGAIN:
			/* Wait for an arbitraty 500 ms. */
			usleep(500000);
			break;
		case BT_COMPONENT_STATUS_OK:
			break;
		case BT_COMPONENT_STATUS_END:
			goto end;
		default:
			fprintf(stderr, "Sink component returned an error, aborting...\n");
			ret = -1;
			goto end;
		}
	}
end:
	BT_PUT(sink_class);
	BT_PUT(source_class);
	BT_PUT(source);
	BT_PUT(sink);
	BT_PUT(source_params);
	BT_PUT(sink_params);
	BT_PUT(cfg);
	BT_PUT(sink_cfg);
	BT_PUT(source_cfg);
	fini_loaded_plugins_array();
	return ret ? 1 : 0;
}
