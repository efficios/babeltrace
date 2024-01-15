/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2024 EfficiOS, Inc.
 */

#include <babeltrace2/babeltrace.h>

static bt_plugin_initialize_func_status plugin_init(bt_self_plugin *)
{
    BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_UNKNOWN("plugin-init-fail",
                                                      "This is the error message");
    return BT_PLUGIN_INITIALIZE_FUNC_STATUS_ERROR;
}

BT_PLUGIN_MODULE();
BT_PLUGIN(test_init_fail);
BT_PLUGIN_DESCRIPTION("Babeltrace plugin with init function that fails");
BT_PLUGIN_AUTHOR("Sophie Couturier");
BT_PLUGIN_LICENSE("Beerware");
BT_PLUGIN_INITIALIZE_FUNC(plugin_init);
