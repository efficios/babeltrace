/*
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
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

#include <babeltrace2/babeltrace.h>
#include "pretty/pretty.h"
#include "dmesg/dmesg.h"
#include "details/details.h"

#ifndef BT_BUILT_IN_PLUGINS
BT_PLUGIN_MODULE();
#endif

BT_PLUGIN(text);
BT_PLUGIN_DESCRIPTION("Plain text input and output");
BT_PLUGIN_AUTHOR("EfficiOS <https://www.efficios.com/>");
BT_PLUGIN_LICENSE("MIT");

/* pretty sink */
BT_PLUGIN_SINK_COMPONENT_CLASS(pretty, pretty_consume);
BT_PLUGIN_SINK_COMPONENT_CLASS_INITIALIZE_METHOD(pretty, pretty_init);
BT_PLUGIN_SINK_COMPONENT_CLASS_FINALIZE_METHOD(pretty, pretty_finalize);
BT_PLUGIN_SINK_COMPONENT_CLASS_GRAPH_IS_CONFIGURED_METHOD(pretty,
	pretty_graph_is_configured);
BT_PLUGIN_SINK_COMPONENT_CLASS_DESCRIPTION(pretty,
	"Pretty-print messages (`text` format of Babeltrace 1).");
BT_PLUGIN_SINK_COMPONENT_CLASS_HELP(pretty,
	"See the babeltrace2-sink.text.pretty(7) manual page.");

/* dmesg source */
BT_PLUGIN_SOURCE_COMPONENT_CLASS(dmesg, dmesg_msg_iter_next);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_DESCRIPTION(dmesg,
	"Read Linux ring buffer lines (dmesg(1) output) from a file or from standard input.");
BT_PLUGIN_SOURCE_COMPONENT_CLASS_HELP(dmesg,
	"See the babeltrace2-source.text.dmesg(7) manual page.");
BT_PLUGIN_SOURCE_COMPONENT_CLASS_INITIALIZE_METHOD(dmesg, dmesg_init);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_FINALIZE_METHOD(dmesg, dmesg_finalize);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD(dmesg,
	dmesg_msg_iter_init);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_FINALIZE_METHOD(dmesg,
	dmesg_msg_iter_finalize);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHODS(dmesg,
	dmesg_msg_iter_seek_beginning, dmesg_msg_iter_can_seek_beginning);

/* details sink */
BT_PLUGIN_SINK_COMPONENT_CLASS(details, details_consume);
BT_PLUGIN_SINK_COMPONENT_CLASS_INITIALIZE_METHOD(details, details_init);
BT_PLUGIN_SINK_COMPONENT_CLASS_FINALIZE_METHOD(details, details_finalize);
BT_PLUGIN_SINK_COMPONENT_CLASS_GRAPH_IS_CONFIGURED_METHOD(details,
	details_graph_is_configured);
BT_PLUGIN_SINK_COMPONENT_CLASS_DESCRIPTION(details,
	"Print messages with details.");
BT_PLUGIN_SINK_COMPONENT_CLASS_HELP(details,
	"See the babeltrace2-sink.text.details(7) manual page.");
