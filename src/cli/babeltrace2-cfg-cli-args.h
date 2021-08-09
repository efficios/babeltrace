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

/*
 * Return value of functions that create a bt_config from CLI args and return
 * it through an out parameter.
 */
enum bt_config_cli_args_status
{
	/*
	 * Config was successfully created and returned through the out
	 * parameter.
	 */
	BT_CONFIG_CLI_ARGS_STATUS_OK = 0,

	/*
	 * Config could not be created due to an error. The out parameter is
	 * left unchanged.
	 */
	BT_CONFIG_CLI_ARGS_STATUS_ERROR = -1,

	/*
	 * The arguments caused the function to print some information (help,
	 * version, etc), not config was created.  The out parameter is left
	 * unchanged.
	 */
	BT_CONFIG_CLI_ARGS_STATUS_INFO_ONLY = 1,
};

enum bt_config_cli_args_status bt_config_cli_args_create(int argc,
		const char *argv[],
		struct bt_config **cfg, bool force_omit_system_plugin_path,
		bool force_omit_home_plugin_path,
		const bt_value *initial_plugin_paths,
		const bt_interrupter *interrupter);

#endif /* CLI_BABELTRACE_CFG_CLI_ARGS_H */
