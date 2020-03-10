/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2019 EfficiOS, Inc.
 */

#ifndef _BABELTRACE_STRING_FORMAT_FORMAT_ERROR_H
#define _BABELTRACE_STRING_FORMAT_FORMAT_ERROR_H

#include <babeltrace2/babeltrace.h>
#include <common/common.h>
#include <common/macros.h>
#include <glib.h>

BT_HIDDEN
gchar *format_bt_error_cause(
		const bt_error_cause *error_cause,
		unsigned int columns,
		bt_logging_level log_level,
		enum bt_common_color_when use_colors);

BT_HIDDEN
gchar *format_bt_error(
		const bt_error *error,
		unsigned int columns,
		bt_logging_level log_level,
		enum bt_common_color_when use_colors);

#endif /* _BABELTRACE_STRING_FORMAT_FORMAT_ERROR_H */
