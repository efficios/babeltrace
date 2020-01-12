/*
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

#include <babeltrace2/babeltrace.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "common/assert.h"
#include <glib.h>
#include "tap/tap.h"
#include "common.h"

#define NR_TESTS 		38
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
	g_setenv("BT_TEST_PLUGIN_INITIALIZE_CALLED", "0", 1);
	g_setenv("BT_TEST_PLUGIN_FINALIZE_CALLED", "0", 1);
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

static void test_minimal(const char *plugin_dir)
{
	const bt_plugin_set *plugin_set = NULL;
	const bt_plugin *plugin;
	char *minimal_path = get_test_plugin_path(plugin_dir, "minimal");
	bt_plugin_find_all_from_file_status status;

	BT_ASSERT(minimal_path);
	diag("minimal plugin test below");

	reset_test_plugin_env_vars();
	status = bt_plugin_find_all_from_file(minimal_path, BT_FALSE,
		&plugin_set);
	ok(status == BT_PLUGIN_FIND_ALL_FROM_FILE_STATUS_OK,
		"bt_plugin_find_all_from_file() succeeds with a valid file");
	ok(plugin_set,
		"bt_plugin_find_all_from_file() returns a plugin set");
	ok(check_env_var("BT_TEST_PLUGIN_INITIALIZE_CALLED") == 1,
		"plugin's initialization function is called during bt_plugin_find_all_from_file()");
	ok(bt_plugin_set_get_plugin_count(plugin_set) == 1,
		"bt_plugin_find_all_from_file() returns the expected number of plugins");
	plugin = bt_plugin_set_borrow_plugin_by_index_const(plugin_set, 0);
	ok(strcmp(bt_plugin_get_name(plugin), "test_minimal") == 0,
		"bt_plugin_get_name() returns the expected name");
	ok(strcmp(bt_plugin_get_description(plugin),
		"Minimal Babeltrace plugin with no component classes") == 0,
		"bt_plugin_get_description() returns the expected description");
	ok(bt_plugin_get_version(plugin, NULL, NULL, NULL, NULL) ==
		BT_PROPERTY_AVAILABILITY_NOT_AVAILABLE,
		"bt_plugin_get_version() fails when there's no version");
	ok(strcmp(bt_plugin_get_author(plugin), "Janine Sutto") == 0,
		"bt_plugin_get_author() returns the expected author");
	ok(strcmp(bt_plugin_get_license(plugin), "Beerware") == 0,
		"bt_plugin_get_license() returns the expected license");
	ok(strcmp(bt_plugin_get_path(plugin), minimal_path) == 0,
		"bt_plugin_get_path() returns the expected path");
	ok(bt_plugin_get_source_component_class_count(plugin) == 0,
		"bt_plugin_get_source_component_class_count() returns the expected value");
	ok(bt_plugin_get_filter_component_class_count(plugin) == 0,
		"bt_plugin_get_filter_component_class_count() returns the expected value");
	ok(bt_plugin_get_sink_component_class_count(plugin) == 0,
		"bt_plugin_get_sink_component_class_count() returns the expected value");
	bt_plugin_set_put_ref(plugin_set);
	ok(check_env_var("BT_TEST_PLUGIN_FINALIZE_CALLED") == 1,
		"plugin's finalize function is called when the plugin is destroyed");

	free(minimal_path);
}

static void test_sfs(const char *plugin_dir)
{
	const bt_plugin_set *plugin_set = NULL;
	const bt_plugin *plugin;
	const bt_component_class_sink *sink_comp_class;
	const bt_component_class_source *source_comp_class;
	const bt_component_class_filter *filter_comp_class;
	const bt_component_sink *sink_component;
	char *sfs_path = get_test_plugin_path(plugin_dir, "sfs");
	unsigned int major, minor, patch;
	const char *extra;
	bt_value *params;
	const bt_value *results;
	const bt_value *object;
	const bt_value *res_params;
	bt_graph *graph;
	const char *object_str;
	bt_graph_add_component_status graph_ret;
	bt_query_executor *query_exec;
	int ret;
	bt_plugin_find_all_from_file_status status;

	BT_ASSERT(sfs_path);
	diag("sfs plugin test below");

	status = bt_plugin_find_all_from_file(sfs_path, BT_FALSE, &plugin_set);
	BT_ASSERT(status == BT_PLUGIN_FIND_ALL_FROM_FILE_STATUS_OK &&
		plugin_set && bt_plugin_set_get_plugin_count(plugin_set) == 1);
	plugin = bt_plugin_set_borrow_plugin_by_index_const(plugin_set, 0);
	ok(bt_plugin_get_version(plugin, &major, &minor, &patch, &extra) ==
		BT_PROPERTY_AVAILABILITY_AVAILABLE,
		"bt_plugin_get_version() succeeds when there's a version");
	ok(major == 1,
		"bt_plugin_get_version() returns the expected major version");
	ok(minor == 2,
		"bt_plugin_get_version() returns the expected minor version");
	ok(patch == 3,
		"bt_plugin_get_version() returns the expected patch version");
	ok(strcmp(extra, "yes") == 0,
		"bt_plugin_get_version() returns the expected extra version");
	ok(bt_plugin_get_source_component_class_count(plugin) == 1,
		"bt_plugin_get_source_component_class_count() returns the expected value");
	ok(bt_plugin_get_filter_component_class_count(plugin) == 1,
		"bt_plugin_get_filter_component_class_count() returns the expected value");
	ok(bt_plugin_get_sink_component_class_count(plugin) == 1,
		"bt_plugin_get_sink_component_class_count() returns the expected value");

	source_comp_class = bt_plugin_borrow_source_component_class_by_name_const(
		plugin, "source");
	ok(source_comp_class,
		"bt_plugin_borrow_source_component_class_by_name_const() finds a source component class");

	sink_comp_class = bt_plugin_borrow_sink_component_class_by_name_const(
		plugin, "sink");
	ok(sink_comp_class,
		"bt_plugin_borrow_sink_component_class_by_name_const() finds a sink component class");
	ok(strcmp(bt_component_class_get_help(bt_component_class_sink_as_component_class_const(sink_comp_class)),
		  "Bacon ipsum dolor amet strip steak cupim pastrami venison shoulder.\n"
		  "Prosciutto beef ribs flank meatloaf pancetta brisket kielbasa drumstick\n"
		  "venison tenderloin cow tail. Beef short loin shoulder meatball, sirloin\n"
		  "ground round brisket salami cupim pork bresaola turkey bacon boudin.\n") == 0,
		"bt_component_class_get_help() returns the expected help text");

	filter_comp_class = bt_plugin_borrow_filter_component_class_by_name_const(
		plugin, "filter");
	ok(filter_comp_class,
		"bt_plugin_borrow_filter_component_class_by_name_const() finds a filter component class");
	params = bt_value_integer_signed_create_init(23);
	BT_ASSERT(params);
	query_exec = bt_query_executor_create(
		bt_component_class_filter_as_component_class_const(
			filter_comp_class), "get-something", params);
	BT_ASSERT(query_exec);
	ret = bt_query_executor_query(query_exec, &results);
	ok(ret == 0 && results, "bt_query_executor_query() succeeds");
	BT_ASSERT(bt_value_is_array(results) && bt_value_array_get_length(results) == 2);
	object = bt_value_array_borrow_element_by_index_const(results, 0);
	BT_ASSERT(bt_value_is_string(object));
	object_str = bt_value_string_get(object);
	ok(strcmp(object_str, "get-something") == 0,
		"bt_component_class_query() receives the expected object name");
	res_params = bt_value_array_borrow_element_by_index_const(results, 1);
	ok(bt_value_is_equal(res_params, params),
		"bt_component_class_query() receives the expected parameters");

	bt_component_class_sink_get_ref(sink_comp_class);
	BT_PLUGIN_SET_PUT_REF_AND_RESET(plugin_set);
	graph = bt_graph_create(0);
	BT_ASSERT(graph);
	graph_ret = bt_graph_add_sink_component(graph, sink_comp_class,
		"the-sink", NULL, BT_LOGGING_LEVEL_NONE, &sink_component);
	ok(graph_ret == BT_GRAPH_ADD_COMPONENT_STATUS_OK && sink_component,
		"bt_graph_add_sink_component() still works after the plugin object is destroyed");
	bt_graph_put_ref(graph);

	free(sfs_path);
	bt_component_class_sink_put_ref(sink_comp_class);
	bt_value_put_ref(results);
	bt_value_put_ref(params);
	bt_query_executor_put_ref(query_exec);
}

static void test_create_all_from_dir(const char *plugin_dir)
{
	const bt_plugin_set *plugin_set;
	bt_plugin_find_all_from_dir_status status;

	diag("create from all test below");

	status = bt_plugin_find_all_from_dir(NON_EXISTING_PATH, BT_FALSE,
		BT_FALSE, &plugin_set);
	ok(status == BT_PLUGIN_FIND_ALL_FROM_DIR_STATUS_ERROR,
		"bt_plugin_find_all_from_dir() fails with an invalid path");
	bt_current_thread_clear_error();

	plugin_set = NULL;
	status = bt_plugin_find_all_from_dir(plugin_dir, BT_FALSE, BT_FALSE,
		&plugin_set);
	ok(status == BT_PLUGIN_FIND_ALL_FROM_DIR_STATUS_OK,
		"bt_plugin_find_all_from_dir() succeeds with a valid path");
	ok(plugin_set,
		"bt_plugin_find_all_from_dir() returns a plugin set with a valid path");

	/* 2 or 4, if `.la` files are considered or not */
	ok(bt_plugin_set_get_plugin_count(plugin_set) == 2 ||
		bt_plugin_set_get_plugin_count(plugin_set) == 4,
		"bt_plugin_find_all_from_dir() returns the expected number of plugin objects");

	bt_plugin_set_put_ref(plugin_set);
}

static void test_find(const char *plugin_dir)
{
	int ret;
	const bt_plugin *plugin;
	char *plugin_path;
	bt_plugin_find_status status;

	ok(bt_plugin_find(NON_EXISTING_PATH, BT_TRUE, BT_FALSE, BT_FALSE,
		BT_FALSE, BT_FALSE, &plugin) == BT_PLUGIN_FIND_STATUS_NOT_FOUND,
		"bt_plugin_find() returns BT_PLUGIN_STATUS_NOT_FOUND with an unknown plugin name");
	ret = asprintf(&plugin_path, "%s" G_SEARCHPATH_SEPARATOR_S
			G_DIR_SEPARATOR_S "ec1d09e5-696c-442e-b1c3-f9c6cf7f5958"
			G_SEARCHPATH_SEPARATOR_S G_SEARCHPATH_SEPARATOR_S
			G_SEARCHPATH_SEPARATOR_S "%s" G_SEARCHPATH_SEPARATOR_S
			"8db46494-a398-466a-9649-c765ae077629"
			G_SEARCHPATH_SEPARATOR_S,
		NON_EXISTING_PATH, plugin_dir);
	BT_ASSERT(ret > 0 && plugin_path);
	g_setenv("BABELTRACE_PLUGIN_PATH", plugin_path, 1);
	plugin = NULL;
	status = bt_plugin_find("test_minimal", BT_TRUE, BT_FALSE, BT_FALSE,
		BT_FALSE, BT_FALSE, &plugin);
	ok(status == BT_PLUGIN_FIND_STATUS_OK,
		"bt_plugin_find() succeeds with a plugin name it can find");
	ok(plugin, "bt_plugin_find() returns a plugin object");
	ok(strcmp(bt_plugin_get_author(plugin), "Janine Sutto") == 0,
		"bt_plugin_find() finds the correct plugin for a given name");
	BT_PLUGIN_PUT_REF_AND_RESET(plugin);
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
	test_minimal(plugin_dir);
	test_sfs(plugin_dir);
	test_create_all_from_dir(plugin_dir);
	test_find(plugin_dir);
	ret = exit_status();
end:
	return ret;
}
