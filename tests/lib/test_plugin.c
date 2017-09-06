/*
 * test_plugin.c
 *
 * CTF IR Reference Count test
 *
 * Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <babeltrace/plugin/plugin.h>
#include <babeltrace/ref.h>
#include <babeltrace/values.h>
#include <babeltrace/graph/component.h>
#include <babeltrace/graph/graph.h>
#include <babeltrace/graph/query-executor.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <glib.h>
#include "tap/tap.h"
#include "common.h"

#define NR_TESTS 		58
#define NON_EXISTING_PATH	"/this/hopefully/does/not/exist/5bc75f8d-0dba-4043-a509-d7984b97e42b.so"

/* Those symbols are written to by some test plugins */
static int check_env_var(const char *name)
{
	const char *val = getenv(name);

	if (!val) {
		return -1;
	}

	return atoi(val);
}

static void reset_test_plugin_env_vars(void)
{
	g_setenv("BT_TEST_PLUGIN_INIT_CALLED", "0", 1);
	g_setenv("BT_TEST_PLUGIN_EXIT_CALLED", "0", 1);
}

static char *get_test_plugin_path(const char *plugin_dir,
		const char *plugin_name)
{
	char *ret;
	char *plugin_file_name;

	if (asprintf(&plugin_file_name, "plugin-%s." G_MODULE_SUFFIX,
			plugin_name) == -1) {
		abort();
	}

	ret = g_build_filename(plugin_dir, plugin_file_name, NULL);
	free(plugin_file_name);

	return ret;
}

static void test_invalid(const char *plugin_dir)
{
	struct bt_plugin_set *plugin_set;

	diag("invalid plugin test below");

	plugin_set = bt_plugin_create_all_from_file(NON_EXISTING_PATH);
	ok(!plugin_set, "bt_plugin_create_all_from_file() fails with a non-existing file");

	plugin_set = bt_plugin_create_all_from_file(plugin_dir);
	ok(!plugin_set, "bt_plugin_create_all_from_file() fails with a directory");

	ok(!bt_plugin_create_all_from_file(NULL),
		"bt_plugin_create_all_from_file() handles NULL correctly");
	ok(!bt_plugin_create_all_from_dir(NULL, BT_FALSE),
		"bt_plugin_create_all_from_dir() handles NULL correctly");
	ok(!bt_plugin_get_name(NULL),
		"bt_plugin_get_name() handles NULL correctly");
	ok(!bt_plugin_get_description(NULL),
		"bt_plugin_get_description() handles NULL correctly");
	ok(bt_plugin_get_version(NULL, NULL, NULL, NULL, NULL) !=
		BT_PLUGIN_STATUS_OK,
		"bt_plugin_get_version() handles NULL correctly");
	ok(!bt_plugin_get_author(NULL),
		"bt_plugin_get_author() handles NULL correctly");
	ok(!bt_plugin_get_license(NULL),
		"bt_plugin_get_license() handles NULL correctly");
	ok(!bt_plugin_get_path(NULL),
		"bt_plugin_get_path() handles NULL correctly");
	ok(bt_plugin_get_component_class_count(NULL) < 0,
		"bt_plugin_get_component_class_count() handles NULL correctly");
	ok(!bt_plugin_get_component_class_by_index(NULL, 0),
		"bt_plugin_get_component_class_by_index() handles NULL correctly");
	ok(!bt_plugin_get_component_class_by_name_and_type(NULL, NULL, 0),
		"bt_plugin_get_component_class_by_name_and_type() handles NULL correctly");
}

static void test_minimal(const char *plugin_dir)
{
	struct bt_plugin_set *plugin_set;
	struct bt_plugin *plugin;
	char *minimal_path = get_test_plugin_path(plugin_dir, "minimal");

	assert(minimal_path);
	diag("minimal plugin test below");

	reset_test_plugin_env_vars();
	plugin_set = bt_plugin_create_all_from_file(minimal_path);
	ok(plugin_set && bt_plugin_set_get_plugin_count(plugin_set) == 1,
		"bt_plugin_create_all_from_file() succeeds with a valid file");
	ok(check_env_var("BT_TEST_PLUGIN_INIT_CALLED") == 1,
		"plugin's initialization function is called during bt_plugin_create_all_from_file()");
	ok(bt_plugin_set_get_plugin_count(plugin_set) == 1,
		"bt_plugin_create_all_from_file() returns the expected number of plugins");
	plugin = bt_plugin_set_get_plugin(plugin_set, 0);
	ok(strcmp(bt_plugin_get_name(plugin), "test_minimal") == 0,
		"bt_plugin_get_name() returns the expected name");
	ok(strcmp(bt_plugin_get_description(plugin),
		"Minimal Babeltrace plugin with no component classes") == 0,
		"bt_plugin_get_description() returns the expected description");
	ok(bt_plugin_get_version(plugin, NULL, NULL, NULL, NULL) !=
		BT_PLUGIN_STATUS_OK,
		"bt_plugin_get_version() fails when there's no version");
	ok(strcmp(bt_plugin_get_author(plugin), "Janine Sutto") == 0,
		"bt_plugin_get_author() returns the expected author");
	ok(strcmp(bt_plugin_get_license(plugin), "Beerware") == 0,
		"bt_plugin_get_license() returns the expected license");
	ok(strcmp(bt_plugin_get_path(plugin), minimal_path) == 0,
		"bt_plugin_get_path() returns the expected path");
	ok(bt_plugin_get_component_class_count(plugin) == 0,
		"bt_plugin_get_component_class_count() returns the expected value");
	bt_put(plugin);
	bt_put(plugin_set);
	ok(check_env_var("BT_TEST_PLUGIN_EXIT_CALLED") == 1,
		"plugin's exit function is called when the plugin is destroyed");

	free(minimal_path);
}

static void test_sfs(const char *plugin_dir)
{
	struct bt_plugin_set *plugin_set;
	struct bt_plugin *plugin;
	struct bt_component_class *sink_comp_class;
	struct bt_component_class *source_comp_class;
	struct bt_component_class *filter_comp_class;
	struct bt_component *sink_component;
	char *sfs_path = get_test_plugin_path(plugin_dir, "sfs");
	unsigned int major, minor, patch;
	const char *extra;
	struct bt_value *params;
	struct bt_value *results;
	struct bt_value *object;
	struct bt_value *res_params;
	struct bt_graph *graph;
	const char *object_str;
	enum bt_value_status value_ret;
	enum bt_graph_status graph_ret;
	struct bt_query_executor *query_exec = bt_query_executor_create();
	int ret;

	assert(query_exec);
	assert(sfs_path);
	diag("sfs plugin test below");

	plugin_set = bt_plugin_create_all_from_file(sfs_path);
	assert(plugin_set && bt_plugin_set_get_plugin_count(plugin_set) == 1);
	plugin = bt_plugin_set_get_plugin(plugin_set, 0);
	ok(bt_plugin_get_version(plugin, &major, &minor, &patch, &extra) ==
		BT_PLUGIN_STATUS_OK,
		"bt_plugin_get_version() succeeds when there's a version");
	ok(major == 1,
		"bt_plugin_get_version() returns the expected major version");
	ok(minor == 2,
		"bt_plugin_get_version() returns the expected minor version");
	ok(patch == 3,
		"bt_plugin_get_version() returns the expected patch version");
	ok(strcmp(extra, "yes") == 0,
		"bt_plugin_get_version() returns the expected extra version");
	ok(bt_plugin_get_component_class_count(plugin) == 3,
		"bt_plugin_get_component_class_count() returns the expected value");

	source_comp_class = bt_plugin_get_component_class_by_name_and_type(
		plugin, "source", BT_COMPONENT_CLASS_TYPE_SOURCE);
	ok(source_comp_class,
		"bt_plugin_get_component_class_by_name_and_type() finds a source component class");

	sink_comp_class = bt_plugin_get_component_class_by_name_and_type(
		plugin, "sink", BT_COMPONENT_CLASS_TYPE_SINK);
	ok(sink_comp_class,
		"bt_plugin_get_component_class_by_name_and_type() finds a sink component class");
	ok(strcmp(bt_component_class_get_help(sink_comp_class),
		"Bacon ipsum dolor amet strip steak cupim pastrami venison shoulder.\n"
		"Prosciutto beef ribs flank meatloaf pancetta brisket kielbasa drumstick\n"
		"venison tenderloin cow tail. Beef short loin shoulder meatball, sirloin\n"
		"ground round brisket salami cupim pork bresaola turkey bacon boudin.\n") == 0,
		"bt_component_class_get_help() returns the expected help text");

	filter_comp_class = bt_plugin_get_component_class_by_name_and_type(
		plugin, "filter", BT_COMPONENT_CLASS_TYPE_FILTER);
	ok(filter_comp_class,
		"bt_plugin_get_component_class_by_name_and_type() finds a filter component class");
	ok(!bt_plugin_get_component_class_by_name_and_type(plugin, "filter",
		BT_COMPONENT_CLASS_TYPE_SOURCE),
		"bt_plugin_get_component_class_by_name_and_type() does not find a component class given the wrong type");
	params = bt_value_integer_create_init(23);
	assert(params);
	ret = bt_query_executor_query(NULL, filter_comp_class, "object",
		params, &results);
	ok (ret, "bt_query_executor_query() handles NULL (query executor)");
	ret = bt_query_executor_query(query_exec, NULL, "object",
		params, &results);
	ok (ret, "bt_query_executor_query() handles NULL (component class)");
	ret = bt_query_executor_query(query_exec, filter_comp_class, NULL,
		params, &results);
	ok (ret, "bt_query_executor_query() handles NULL (object)");
	ret = bt_query_executor_query(query_exec, filter_comp_class,
		"get-something", params, &results);
	ok(ret == 0 && results, "bt_query_executor_query() succeeds");
	assert(bt_value_is_array(results) && bt_value_array_size(results) == 2);
	object = bt_value_array_get(results, 0);
	assert(object && bt_value_is_string(object));
	value_ret = bt_value_string_get(object, &object_str);
	assert(value_ret == BT_VALUE_STATUS_OK);
	ok(strcmp(object_str, "get-something") == 0,
		"bt_component_class_query() receives the expected object name");
	res_params = bt_value_array_get(results, 1);
	ok(res_params == params,
		"bt_component_class_query() receives the expected parameters");

	diag("> putting the plugin object here");
	BT_PUT(plugin);
	graph = bt_graph_create();
	assert(graph);
	graph_ret = bt_graph_add_component(graph, sink_comp_class, "the-sink",
		NULL, &sink_component);
	ok(graph_ret == BT_GRAPH_STATUS_OK && sink_component,
		"bt_graph_add_component() still works after the plugin object is destroyed");
	BT_PUT(sink_component);
	BT_PUT(source_comp_class);
	bt_put(graph);
	graph = bt_graph_create();
	assert(graph);
	graph_ret = bt_graph_add_component(graph, sink_comp_class, "the-sink",
		NULL, &sink_component);
	ok(graph_ret == BT_GRAPH_STATUS_OK && sink_component,
		"bt_graph_add_component() still works after the source component class object is destroyed");
	BT_PUT(sink_component);
	BT_PUT(filter_comp_class);
	bt_put(graph);
	graph = bt_graph_create();
	assert(graph);
	graph_ret = bt_graph_add_component(graph, sink_comp_class, "the-sink",
		NULL, &sink_component);
	ok(graph_ret == BT_GRAPH_STATUS_OK && sink_component,
		"bt_graph_add_component() still works after the filter component class object is destroyed");
	BT_PUT(sink_comp_class);
	BT_PUT(sink_component);

	free(sfs_path);
	bt_put(graph);
	bt_put(plugin_set);
	bt_put(object);
	bt_put(res_params);
	bt_put(results);
	bt_put(params);
	bt_put(query_exec);
}

static void test_create_all_from_dir(const char *plugin_dir)
{
	struct bt_plugin_set *plugin_set;

	diag("create from all test below");

	plugin_set = bt_plugin_create_all_from_dir(NON_EXISTING_PATH, BT_FALSE);
	ok(!plugin_set,
		"bt_plugin_create_all_from_dir() fails with an invalid path");

	plugin_set = bt_plugin_create_all_from_dir(plugin_dir, BT_FALSE);
	ok(plugin_set, "bt_plugin_create_all_from_dir() succeeds with a valid path");

	/* 2 or 4, if `.la` files are considered or not */
	ok(bt_plugin_set_get_plugin_count(plugin_set) == 2 ||
		bt_plugin_set_get_plugin_count(plugin_set) == 4,
		"bt_plugin_create_all_from_dir() returns the expected number of plugin objects");

	bt_put(plugin_set);
}

static void test_find(const char *plugin_dir)
{
	int ret;
	struct bt_plugin *plugin;
	struct bt_component_class *comp_cls_sink;
	struct bt_component_class *comp_cls_source;
	char *plugin_path;

	ok(!bt_plugin_find(NULL),
		"bt_plugin_find() handles NULL");
	ok(!bt_plugin_find(NON_EXISTING_PATH),
		"bt_plugin_find() returns NULL with an unknown plugin name");
	ret = asprintf(&plugin_path, "%s" G_SEARCHPATH_SEPARATOR_S
			G_DIR_SEPARATOR_S "ec1d09e5-696c-442e-b1c3-f9c6cf7f5958"
			G_SEARCHPATH_SEPARATOR_S G_SEARCHPATH_SEPARATOR_S
			G_SEARCHPATH_SEPARATOR_S "%s" G_SEARCHPATH_SEPARATOR_S
			"8db46494-a398-466a-9649-c765ae077629"
			G_SEARCHPATH_SEPARATOR_S,
		NON_EXISTING_PATH, plugin_dir);
	assert(ret > 0 && plugin_path);
	g_setenv("BABELTRACE_PLUGIN_PATH", plugin_path, 1);
	plugin = bt_plugin_find("test_minimal");
	ok(plugin,
		"bt_plugin_find() succeeds with a plugin name it can find");
	ok(strcmp(bt_plugin_get_author(plugin), "Janine Sutto") == 0,
		"bt_plugin_find() finds the correct plugin for a given name");
	BT_PUT(plugin);
	comp_cls_sink = bt_plugin_find_component_class(NULL, "sink",
		BT_COMPONENT_CLASS_TYPE_SINK);
	ok(!comp_cls_sink, "bt_plugin_find_component_class() handles NULL (plugin name)");
	comp_cls_sink = bt_plugin_find_component_class("test_sfs", NULL,
		BT_COMPONENT_CLASS_TYPE_SINK);
	ok(!comp_cls_sink, "bt_plugin_find_component_class() handles NULL (component class name)");
	comp_cls_sink = bt_plugin_find_component_class("test_sfs", "sink2",
		BT_COMPONENT_CLASS_TYPE_SINK);
	ok(!comp_cls_sink, "bt_plugin_find_component_class() fails with an unknown component class name");
	comp_cls_sink = bt_plugin_find_component_class("test_sfs", "sink",
		BT_COMPONENT_CLASS_TYPE_SINK);
	ok(comp_cls_sink, "bt_plugin_find_component_class() succeeds with valid parameters");
	ok(strcmp(bt_component_class_get_name(comp_cls_sink), "sink") == 0,
		"bt_plugin_find_component_class() returns the appropriate component class (sink)");
	comp_cls_source = bt_plugin_find_component_class("test_sfs", "source",
		BT_COMPONENT_CLASS_TYPE_SOURCE);
	ok(comp_cls_sink, "bt_plugin_find_component_class() succeeds with another component class name (same plugin)");
	ok(strcmp(bt_component_class_get_name(comp_cls_source), "source") == 0,
		"bt_plugin_find_component_class() returns the appropriate component class (source)");
	BT_PUT(comp_cls_sink);
	BT_PUT(comp_cls_source);
	free(plugin_path);
}

int main(int argc, char **argv)
{
	int ret;
	const char *plugin_dir;

	if (argc != 2) {
		puts("Usage: test_plugin plugin_directory");
		ret = 1;
		goto end;
	}

	plugin_dir = argv[1];
	plan_tests(NR_TESTS);
	test_invalid(plugin_dir);
	test_minimal(plugin_dir);
	test_sfs(plugin_dir);
	test_create_all_from_dir(plugin_dir);
	test_find(plugin_dir);
	ret = exit_status();
end:
	return ret;
}
