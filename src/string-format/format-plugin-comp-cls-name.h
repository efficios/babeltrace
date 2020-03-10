/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2019 EfficiOS, Inc.
 */

#ifndef _BABELTRACE_STRING_FORMAT_FORMAT_PLUGIN_COMP_CLS_NAME_H
#define _BABELTRACE_STRING_FORMAT_FORMAT_PLUGIN_COMP_CLS_NAME_H

#include <babeltrace2/babeltrace.h>
#include <common/common.h>
#include <common/macros.h>
#include <glib.h>

BT_HIDDEN
gchar *format_plugin_comp_cls_opt(const char *plugin_name,
		const char *comp_cls_name, bt_component_class_type type,
		enum bt_common_color_when use_colors);

#endif /* _BABELTRACE_STRING_FORMAT_FORMAT_PLUGIN_COMP_CLS_NAME_H */
