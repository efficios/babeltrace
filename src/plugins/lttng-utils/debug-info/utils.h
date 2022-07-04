/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Babeltrace - Debug Info Utilities
 */

#ifndef BABELTRACE_PLUGIN_DEBUG_INFO_UTILS_H
#define BABELTRACE_PLUGIN_DEBUG_INFO_UTILS_H

#include <stdbool.h>

#include <babeltrace2/babeltrace.h>

#include "common/macros.h"

/*
 * Return the location of a path's file (the last element of the path).
 * Returns the original path on error.
 */
const char *get_filename_from_path(const char *path);

bool is_event_common_ctx_dbg_info_compatible(
		const bt_field_class *in_field_class,
		const char *debug_info_field_class_name);

#endif	/* BABELTRACE_PLUGIN_DEBUG_INFO_UTILS_H */
