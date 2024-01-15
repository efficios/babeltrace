/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2017 Philippe Proulx <pproulx@efficios.com>
 */

#include <babeltrace2/babeltrace.h>
#include <stdlib.h>
#include <glib.h>

static bt_plugin_initialize_func_status plugin_init(
		bt_self_plugin *plugin __attribute__((unused)))
{
	g_setenv("BT_TEST_PLUGIN_INITIALIZE_CALLED", "1", 1);
	return BT_PLUGIN_INITIALIZE_FUNC_STATUS_OK;
}

static void plugin_finalize(void)
{
	g_setenv("BT_TEST_PLUGIN_FINALIZE_CALLED", "1", 1);
}

BT_PLUGIN_MODULE();
BT_PLUGIN(test_minimal);
BT_PLUGIN_DESCRIPTION("Minimal Babeltrace plugin with no component classes");
BT_PLUGIN_AUTHOR("Janine Sutto");
BT_PLUGIN_LICENSE("Beerware");
BT_PLUGIN_INITIALIZE_FUNC(plugin_init);
BT_PLUGIN_FINALIZE_FUNC(plugin_finalize);
