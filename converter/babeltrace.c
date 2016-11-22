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
#include <babeltrace/plugin/component-factory.h>
#include <babeltrace/plugin/plugin.h>
#include <babeltrace/plugin/component-class.h>
#include <babeltrace/plugin/notification/iterator.h>
#include <babeltrace/ref.h>
#include <babeltrace/values.h>
#include <stdlib.h>
#include <babeltrace/ctf-ir/metadata.h>	/* for clocks */
#include <popt.h>
#include <string.h>
#include <stdio.h>
#include "babeltrace-cfg.h"

static
const char *component_type_str(enum bt_component_type type)
{
	switch (type) {
	case BT_COMPONENT_TYPE_SOURCE:
		return "source";
	case BT_COMPONENT_TYPE_SINK:
		return "sink";
	case BT_COMPONENT_TYPE_FILTER:
		return "filter";
	case BT_COMPONENT_TYPE_UNKNOWN:
	default:
		return "unknown";
	}
}

static
void print_component_classes_found(struct bt_component_factory *factory)
{
	int count, i;

	if (!babeltrace_verbose) {
		return;
	}

	count = bt_component_factory_get_component_class_count(factory);
	if (count <= 0) {
		fprintf(stderr, "No component classes found. Please make sure your plug-in search path is set correctly.\n");
		return;
	}

	printf_verbose("Found %d component classes.\n", count);
	for (i = 0; i < count; i++) {
		struct bt_component_class *component_class =
				bt_component_factory_get_component_class_index(
				factory, i);
		struct bt_plugin *plugin = bt_component_class_get_plugin(
				component_class);
		const char *plugin_name = bt_plugin_get_name(plugin);
		const char *component_name = bt_component_class_get_name(
				component_class);
		const char *path = bt_plugin_get_path(plugin);
		const char *author = bt_plugin_get_author(plugin);
		const char *license = bt_plugin_get_license(plugin);
		const char *plugin_description = bt_plugin_get_description(
				plugin);
		const char *component_description =
				bt_component_class_get_description(
					component_class);
		enum bt_component_type type = bt_component_class_get_type(
				component_class);

		printf_verbose("[%s - %s (%s)]\n", plugin_name, component_name,
			       component_type_str(type));
		printf_verbose("\tpath: %s\n", path ? path : "None");
		printf_verbose("\tauthor: %s\n", author ? author : "Unknown");
		printf_verbose("\tlicense: %s\n", license ? license : "Unknown");
		printf_verbose("\tplugin description: %s\n",
				plugin_description ? plugin_description : "None");
		printf_verbose("\tcomponent description: %s\n",
				component_description ? component_description : "None");

		bt_put(plugin);
		bt_put(component_class);
	}
}

static
void print_indent(size_t indent)
{
	size_t i;

	for (i = 0; i < indent; i++) {
		printf(" ");
	}
}

static
void print_value(struct bt_value *, size_t, bool);

static
bool print_map_value(const char *key, struct bt_value *object, void *data)
{
	size_t indent = (size_t) data;

	print_indent(indent);
	printf("\"%s\": ", key);
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
		printf("null\n");
		break;
	case BT_VALUE_TYPE_BOOL:
		bt_value_bool_get(value, &bool_val);
		printf("%s\n", bool_val ? "true" : "false");
		break;
	case BT_VALUE_TYPE_INTEGER:
		bt_value_integer_get(value, &int_val);
		printf("%" PRId64 "\n", int_val);
		break;
	case BT_VALUE_TYPE_FLOAT:
		bt_value_float_get(value, &dbl_val);
		printf("%lf\n", dbl_val);
		break;
	case BT_VALUE_TYPE_STRING:
		bt_value_string_get(value, &str_val);
		printf("\"%s\"\n", str_val);
		break;
	case BT_VALUE_TYPE_ARRAY:
		size = bt_value_array_size(value);
		printf("[\n");

		for (i = 0; i < size; i++) {
			struct bt_value *element =
					bt_value_array_get(value, i);

			print_value(element, indent + 2, true);
			BT_PUT(element);
		}

		print_indent(indent);
		printf("]\n");
		break;
	case BT_VALUE_TYPE_MAP:
		if (bt_value_map_is_empty(value)) {
			printf("{}\n");
			return;
		}

		printf("{\n");
		bt_value_map_foreach(value, print_map_value,
			(void *) (indent + 2));
		print_indent(indent);
		printf("}\n");
		break;
	default:
		assert(false);
	}
}

static
void print_bt_config_component(struct bt_config_component *bt_config_component)
{
	printf("  %s.%s\n", bt_config_component->plugin_name->str,
		bt_config_component->component_name->str);
	printf("    params:\n");
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
	printf("debug:           %d\n", cfg->debug);
	printf("verbose:         %d\n", cfg->verbose);
	printf("do list:         %d\n", cfg->do_list);
	printf("force correlate: %d\n", cfg->force_correlate);
	printf("plugin paths:\n");
	print_value(cfg->plugin_paths, 2, true);
	printf("sources:\n");
	print_bt_config_components(cfg->sources);
	printf("sinks:\n");
	print_bt_config_components(cfg->sinks);
}

int main(int argc, char **argv)
{
	int ret;
	struct bt_component_factory *component_factory = NULL;
	struct bt_component_class *source_class = NULL;
	struct bt_component_class *sink_class = NULL;
	struct bt_component *source = NULL, *sink = NULL;
	struct bt_value *source_params = NULL, *sink_params = NULL;
	struct bt_config *cfg;
	struct bt_notification_iterator *it = NULL;
	enum bt_component_status sink_status;
	struct bt_value *first_plugin_path_value = NULL;
	const char *first_plugin_path;
	struct bt_config_component *cfg_component;

	cfg = bt_config_from_args(argc, argv, &ret);
	if (cfg) {
		print_cfg(cfg);
	} else {
		goto end;
	}

	babeltrace_verbose = cfg->verbose;
	babeltrace_debug = cfg->debug;

	/* TODO handle more than 1 source and 1 sink. */
	if (cfg->sources->len != 1 || cfg->sinks->len != 1) {
		fprintf(stderr, "Unexpected configuration, aborting...\n");
		ret = -1;
		goto end;
	}
	cfg_component = bt_config_get_component(cfg->sources, 0);
	source_params = bt_get(cfg_component->params);
	BT_PUT(cfg_component);
	cfg_component = bt_config_get_component(cfg->sinks, 0);
	sink_params = bt_get(cfg_component->params);
	BT_PUT(cfg_component);

	printf_verbose("Verbose mode active.\n");
	printf_debug("Debug mode active.\n");
	component_factory = bt_component_factory_create();
	if (!component_factory) {
		fprintf(stderr, "Failed to create component factory.\n");
		ret = -1;
		goto end;
	}

	if (cfg->plugin_paths && !bt_value_array_is_empty(cfg->plugin_paths)) {
		first_plugin_path_value = bt_value_array_get(
				cfg->plugin_paths, 0);
		bt_value_string_get(first_plugin_path_value,
				&first_plugin_path);
		ret = bt_component_factory_load_recursive(component_factory,
				first_plugin_path);
		if (ret) {
			fprintf(stderr, "Failed to dynamically load plugins.\n");
			goto end;
		}
	}

	ret = bt_component_factory_load_static(component_factory);
	if (ret) {
		fprintf(stderr, "Failed to load static plugins.\n");
		goto end;
	}

	print_component_classes_found(component_factory);
	source_class = bt_component_factory_get_component_class(
			component_factory, "ctf", BT_COMPONENT_TYPE_SOURCE,
			"fs");
	if (!source_class) {
		fprintf(stderr, "Could not find ctf-fs source component class. Aborting...\n");
		ret = -1;
		goto end;
	}

	sink_class = bt_component_factory_get_component_class(component_factory,
			NULL, BT_COMPONENT_TYPE_SINK, "text");
	if (!sink_class) {
		fprintf(stderr, "Could not find text output component class. Aborting...\n");
		ret = -1;
		goto end;
	}

	source = bt_component_create(source_class, "ctf-fs", source_params);
	if (!source) {
		fprintf(stderr, "Failed to instantiate ctf-fs source component. Aborting...\n");
                ret = -1;
                goto end;
        }

	sink = bt_component_create(sink_class, "text", sink_params);
	if (!sink) {
		fprintf(stderr, "Failed to instanciate text output component. Aborting...\n");
		ret = -1;
		goto end;
	}

	it = bt_component_source_create_iterator(source);
	if (!it) {
		fprintf(stderr, "Failed to instantiate source iterator. Aborting...\n");
                ret = -1;
                goto end;
        }

	sink_status = bt_component_sink_add_iterator(sink, it);
	if (sink_status != BT_COMPONENT_STATUS_OK) {
		ret = -1;
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
	BT_PUT(component_factory);
	BT_PUT(sink_class);
	BT_PUT(source_class);
	BT_PUT(source);
	BT_PUT(sink);
	BT_PUT(source_params);
	BT_PUT(sink_params);
	BT_PUT(it);
	BT_PUT(cfg);
	BT_PUT(first_plugin_path_value);
	return ret ? 1 : 0;
}
