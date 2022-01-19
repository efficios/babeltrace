/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2019 Francis Deslauriers <francis.deslauriers@efficios.com>
 */

#ifndef BABELTRACE_PLUGIN_COMMON_MUXING_H
#define BABELTRACE_PLUGIN_COMMON_MUXING_H

#include <babeltrace2/babeltrace.h>
#include "common/macros.h"

BT_EXTERN_C BT_HIDDEN
int common_muxing_compare_messages(const bt_message *left_msg,
		const bt_message *right_msg);

#endif /* BABELTRACE_PLUGIN_COMMON_MUXING_H */
