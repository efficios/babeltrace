/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2017 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef CLI_LOGGING_H
#define CLI_LOGGING_H

#define BT_LOG_OUTPUT_LEVEL bt_cli_log_level
#include "logging/log.h"

BT_LOG_LEVEL_EXTERN_SYMBOL(bt_cli_log_level);

#define BT_CLI_LOG_AND_APPEND(_lvl, _fmt, ...)				\
	do {								\
		BT_LOG_WRITE(_lvl, BT_LOG_TAG, _fmt, ##__VA_ARGS__);	\
		(void) BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_UNKNOWN( \
			"Babeltrace CLI", _fmt, ##__VA_ARGS__);		\
	} while (0)

#define BT_CLI_LOGE_APPEND_CAUSE(_fmt, ...)				\
	BT_CLI_LOG_AND_APPEND(BT_LOG_ERROR, _fmt, ##__VA_ARGS__)
#define BT_CLI_LOGW_APPEND_CAUSE(_fmt, ...)				\
	BT_CLI_LOG_AND_APPEND(BT_LOG_WARNING, _fmt, ##__VA_ARGS__)

#endif /* CLI_LOGGING_H */
