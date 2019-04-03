/*
 * plugin.c
 *
 * Babeltrace Debug Info Plug-in
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Author: Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <babeltrace/babeltrace.h>
#include "debug-info/debug-info.h"

#ifndef BT_BUILT_IN_PLUGINS
BT_PLUGIN_MODULE();
#endif

/* Initialize plug-in entry points. */
BT_PLUGIN_WITH_ID(lttng_utils, "lttng-utils");
BT_PLUGIN_DESCRIPTION_WITH_ID(lttng_utils, "LTTng utilities");
BT_PLUGIN_AUTHOR_WITH_ID(lttng_utils, "Julien Desfossez");
BT_PLUGIN_LICENSE_WITH_ID(lttng_utils, "MIT");

BT_PLUGIN_FILTER_COMPONENT_CLASS_WITH_ID(lttng_utils, debug_info, "debug-info",
	debug_info_msg_iter_next);
BT_PLUGIN_FILTER_COMPONENT_CLASS_DESCRIPTION_WITH_ID(lttng_utils, debug_info,
	"Augment compatible events with debugging information.");
BT_PLUGIN_FILTER_COMPONENT_CLASS_INIT_METHOD_WITH_ID(lttng_utils,
	debug_info, debug_info_comp_init);
BT_PLUGIN_FILTER_COMPONENT_CLASS_FINALIZE_METHOD_WITH_ID(lttng_utils,
	debug_info, debug_info_comp_finalize);
BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_INIT_METHOD_WITH_ID(
	lttng_utils, debug_info, debug_info_msg_iter_init);
BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_SEEK_BEGINNING_METHOD_WITH_ID(
	lttng_utils, debug_info, debug_info_msg_iter_seek_beginning);
BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_CAN_SEEK_BEGINNING_METHOD_WITH_ID(
	lttng_utils, debug_info, debug_info_msg_iter_can_seek_beginning);
BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_FINALIZE_METHOD_WITH_ID(
	lttng_utils, debug_info, debug_info_msg_iter_finalize);
