/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Babeltrace Trace Converter - Default Configuration
 */

#ifndef CLI_BABELTRACE_CFG_CLI_ARGS_DEFAULT_H
#define CLI_BABELTRACE_CFG_CLI_ARGS_DEFAULT_H

#include "babeltrace2-cfg.h"

struct bt_config *bt_config_cli_args_create_with_default(int argc,
		const char *argv[], int *retcode,
		const bt_interrupter *interrupter);

#endif /* CLI_BABELTRACE_CFG_CLI_ARGS_DEFAULT_H */
