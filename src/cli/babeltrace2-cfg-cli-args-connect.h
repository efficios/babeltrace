/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef CLI_BABELTRACE_CFG_CLI_ARGS_CONNECT_H
#define CLI_BABELTRACE_CFG_CLI_ARGS_CONNECT_H

#include <stdlib.h>
#include <stdint.h>
#include <babeltrace2/value.h>
#include <glib.h>
#include "babeltrace2-cfg.h"

int bt_config_cli_args_create_connections(struct bt_config *cfg,
		const bt_value *connection_args,
		char *error_buf, size_t error_buf_size);

#endif /* CLI_BABELTRACE_CFG_CLI_ARGS_CONNECT_H */
