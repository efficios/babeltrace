/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2016-2019 EfficiOS Inc.
 *
 * Babeltrace trace converter - CLI tool's configuration
 */

#ifndef CLI_BABELTRACE_PLUGINS_H
#define CLI_BABELTRACE_PLUGINS_H

#include <babeltrace2/babeltrace.h>
#include "common/macros.h"

void init_loaded_plugins(void);
void fini_loaded_plugins(void);

int require_loaded_plugins(const bt_value *plugin_paths);

size_t get_loaded_plugins_count(void);
const bt_plugin **borrow_loaded_plugins(void);
const bt_plugin *borrow_loaded_plugin_by_index(size_t index);
const bt_plugin *borrow_loaded_plugin_by_name(const char *name);


#endif /* CLI_BABELTRACE_PLUGINS_H */
