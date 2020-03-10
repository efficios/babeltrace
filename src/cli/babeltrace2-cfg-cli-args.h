/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2016-2017 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef CLI_BABELTRACE_CFG_CLI_ARGS_H
#define CLI_BABELTRACE_CFG_CLI_ARGS_H

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <babeltrace2/value.h>
#include "lib/object.h"
#include "compat/compiler.h"
#include <babeltrace2/graph/component.h>
#include <glib.h>

#include "babeltrace2-cfg.h"

struct bt_config *bt_config_cli_args_create(int argc, const char *argv[],
		int *retcode, bool force_omit_system_plugin_path,
		bool force_omit_home_plugin_path,
		const bt_value *initial_plugin_paths,
		const bt_interrupter *interrupter);

#endif /* CLI_BABELTRACE_CFG_CLI_ARGS_H */
