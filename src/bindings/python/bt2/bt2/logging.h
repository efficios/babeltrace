/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_BINDINGS_PYTHON_BT2_LOGGING_H
#define BABELTRACE_BINDINGS_PYTHON_BT2_LOGGING_H

#define BT_LOG_OUTPUT_LEVEL bt_python_bindings_bt2_log_level
#include "logging/log.h"

BT_LOG_LEVEL_EXTERN_SYMBOL(bt_python_bindings_bt2_log_level);

#endif /* BABELTRACE_BINDINGS_PYTHON_BT2_LOGGING_H */
