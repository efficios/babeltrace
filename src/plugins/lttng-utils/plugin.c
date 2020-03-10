/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Babeltrace Debug Info Plug-in
 */

#include <babeltrace2/babeltrace.h>
#include "debug-info/debug-info.h"

#ifndef BT_BUILT_IN_PLUGINS
BT_PLUGIN_MODULE();
#endif

/* Initialize plug-in entry points. */
BT_PLUGIN_WITH_ID(lttng_utils, "lttng-utils");
BT_PLUGIN_DESCRIPTION_WITH_ID(lttng_utils, "LTTng-specific graph utilities");
BT_PLUGIN_AUTHOR_WITH_ID(lttng_utils, "EfficiOS <https://www.efficios.com/>");
BT_PLUGIN_LICENSE_WITH_ID(lttng_utils, "MIT");

BT_PLUGIN_FILTER_COMPONENT_CLASS_WITH_ID(lttng_utils, debug_info, "debug-info",
	debug_info_msg_iter_next);
BT_PLUGIN_FILTER_COMPONENT_CLASS_DESCRIPTION_WITH_ID(lttng_utils, debug_info,
	"Augment compatible events with debugging information.");
BT_PLUGIN_FILTER_COMPONENT_CLASS_HELP_WITH_ID(lttng_utils, debug_info,
	"See the babeltrace2-filter.lttng-utils.debug-info(7) manual page.");
BT_PLUGIN_FILTER_COMPONENT_CLASS_INITIALIZE_METHOD_WITH_ID(lttng_utils,
	debug_info, debug_info_comp_init);
BT_PLUGIN_FILTER_COMPONENT_CLASS_FINALIZE_METHOD_WITH_ID(lttng_utils,
	debug_info, debug_info_comp_finalize);
BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_WITH_ID(
	lttng_utils, debug_info, debug_info_msg_iter_init);
BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHODS_WITH_ID(
	lttng_utils, debug_info,
	debug_info_msg_iter_seek_beginning,
	debug_info_msg_iter_can_seek_beginning);
BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_FINALIZE_METHOD_WITH_ID(
	lttng_utils, debug_info, debug_info_msg_iter_finalize);
