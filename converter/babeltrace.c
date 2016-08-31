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
#include <babeltrace/ref.h>
#include <babeltrace/values.h>
#include <stdlib.h>
#include <babeltrace/ctf-ir/metadata.h>	/* for clocks */
#include <popt.h>
#include <string.h>
#include <stdio.h>


static char *opt_plugin_path;
static char *opt_input_path;

enum {
	OPT_NONE = 0,
	OPT_PLUGIN_PATH,
	OPT_INPUT_PATH,
	OPT_VERBOSE,
	OPT_DEBUG,
	OPT_HELP,
};

/*
 * We are _not_ using POPT_ARG_STRING's ability to store directly into
 * variables, because we want to cast the return to non-const, which is
 * not possible without using poptGetOptArg explicitly. This helps us
 * controlling memory allocation correctly without making assumptions
 * about undocumented behaviors. poptGetOptArg is documented as
 * requiring the returned const char * to be freed by the caller.
 */
static struct poptOption long_options[] = {
	/* longName, shortName, argInfo, argPtr, value, descrip, argDesc */
	{ "plugin-path", 0, POPT_ARG_STRING, NULL, OPT_PLUGIN_PATH, NULL, NULL },
	{ "input-path", 0, POPT_ARG_STRING, NULL, OPT_INPUT_PATH, NULL, NULL },
	{ "verbose", 'v', POPT_ARG_NONE, NULL, OPT_VERBOSE, NULL, NULL },
	{ "debug", 'd', POPT_ARG_NONE, NULL, OPT_DEBUG, NULL, NULL },
	{ NULL, 0, 0, NULL, 0, NULL, NULL },
};

/*
 * Return 0 if caller should continue, < 0 if caller should return
 * error, > 0 if caller should exit without reporting error.
 */
static int parse_options(int argc, char **argv)
{
	poptContext pc;
	int opt, ret = 0;

	if (argc == 1) {
		return 1;	/* exit cleanly */
	}

	pc = poptGetContext(NULL, argc, (const char **) argv, long_options, 0);
	poptReadDefaultConfig(pc, 0);

	/* set default */
	opt_context_field_names = 1;
	opt_payload_field_names = 1;

	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		case OPT_HELP:
			ret = 1;	/* exit cleanly */
			goto end;
		case OPT_PLUGIN_PATH:
			opt_plugin_path = (char *) poptGetOptArg(pc);
			if (!opt_plugin_path) {
				ret = -EINVAL;
				goto end;
			}
			break;
		case OPT_VERBOSE:
			babeltrace_verbose = 1;
			break;
		case OPT_DEBUG:
			babeltrace_debug = 1;
			break;
		case OPT_INPUT_PATH:
			opt_input_path = (char *) poptGetOptArg(pc);
			if (!opt_input_path) {
				ret = -EINVAL;
				goto end;
			}
			break;
		default:
			ret = -EINVAL;
			goto end;
		}
	}

end:
	if (pc) {
		poptFreeContext(pc);
	}
	return ret;
}

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
void print_found_component_classes(struct bt_component_factory *factory)
{
	int count, i;

	if (!babeltrace_verbose) {
		return;
	}

	count = bt_component_factory_get_component_class_count(factory);
	if (count <= 0) {
		fprintf(stderr, "No component classes found. Please make sure your plug-in search path is set correctly.");
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
		printf_verbose("\tpath: %s\n", path);
		printf_verbose("\tauthor: %s\n", author);
		printf_verbose("\tlicense: %s\n", license);
		printf_verbose("\tplugin description: %s\n",
				plugin_description ? plugin_description : "None");
		printf_verbose("\tcomponent description: %s\n",
				component_description ? component_description : "None");

		bt_put(plugin);
		bt_put(component_class);
	}
}

int main(int argc, char **argv)
{
	int ret;
	enum bt_value_status value_status;
	struct bt_component_factory *component_factory = NULL;
	struct bt_component_class *source_class = NULL, *sink_class = NULL;
	struct bt_component *source = NULL, *sink = NULL;
	struct bt_value *source_params = NULL, *sink_params = NULL;

	ret = parse_options(argc, argv);
	if (ret < 0) {
		fprintf(stderr, "Error parsing options.\n\n");
		exit(EXIT_FAILURE);
	} else if (ret > 0) {
		exit(EXIT_SUCCESS);
	}

	source_params = bt_value_map_create();
	if (!source_params) {
		fprintf(stderr, "Failed to create source parameters map, aborting...\n");
		ret = -1;
		goto end;
	}

	value_status = bt_value_map_insert_string(source_params, "path",
			opt_input_path);
	if (value_status != BT_VALUE_STATUS_OK) {
		ret = -1;
		goto end;
	}

	printf_verbose("Verbose mode active.\n");
	printf_debug("Debug mode active.\n");

	if (!opt_plugin_path) {
		fprintf(stderr, "No plugin path specified, aborting...\n");
		ret = -1;
		goto end;
	}
	printf_verbose("Looking-up plugins at %s\n",
			opt_plugin_path ? opt_plugin_path : "Invalid");
	component_factory = bt_component_factory_create();
	if (!component_factory) {
		fprintf(stderr, "Failed to create component factory.\n");
		ret = -1;
		goto end;
	}

	ret = bt_component_factory_load_recursive(component_factory, opt_plugin_path);
	if (ret) {
		fprintf(stderr, "Failed to load plugins.\n");
		goto end;
	}

	print_found_component_classes(component_factory);

	source_class = bt_component_factory_get_component_class(
			component_factory, "ctf", BT_COMPONENT_TYPE_SOURCE,
			"fs");
	if (!source_class) {
		fprintf(stderr, "Could not find ctf-fs output component class. Aborting...\n");
		ret = -1;
		goto end;
	}

	sink_class = bt_component_factory_get_component_class(component_factory,
			"text", BT_COMPONENT_TYPE_SINK, "text");
	if (!sink_class) {
		fprintf(stderr, "Could not find text output component class. Aborting...\n");
		ret = -1;
		goto end;
	}

	source = bt_component_create(source_class, "ctf-fs", source_params);
	if (!source) {
		fprintf(stderr, "Failed to instantiate source component. Aborting...\n");
		ret = -1;
		goto end;
	}

	sink = bt_component_create(sink_class, "bt_text_output", sink_params);
	if (!sink) {
		fprintf(stderr, "Failed to instantiate output component. Aborting...\n");
		ret = -1;
		goto end;
	}

	/* teardown and exit */
end:
	BT_PUT(component_factory);
	BT_PUT(sink_class);
	BT_PUT(source_class);
	BT_PUT(source);
	BT_PUT(sink);
	BT_PUT(source_params);
	BT_PUT(sink_params);
	return ret ? 1 : 0;
}
