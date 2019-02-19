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

#include <babeltrace/babeltrace.h>
#include "pretty/pretty.h"
#include "dmesg/dmesg.h"

#ifndef BT_BUILT_IN_PLUGINS
BT_PLUGIN_MODULE();
#endif

BT_PLUGIN(text);
BT_PLUGIN_DESCRIPTION("Plain text component classes");
BT_PLUGIN_AUTHOR("Julien Desfossez, Mathieu Desnoyers, Philippe Proulx");
BT_PLUGIN_LICENSE("MIT");

/* pretty sink */
BT_PLUGIN_SINK_COMPONENT_CLASS(pretty, pretty_consume);
BT_PLUGIN_SINK_COMPONENT_CLASS_INIT_METHOD(pretty, pretty_init);
BT_PLUGIN_SINK_COMPONENT_CLASS_FINALIZE_METHOD(pretty, pretty_finalize);
BT_PLUGIN_SINK_COMPONENT_CLASS_INPUT_PORT_CONNECTED_METHOD(pretty,
	pretty_port_connected);
BT_PLUGIN_SINK_COMPONENT_CLASS_DESCRIPTION(pretty,
	"Pretty-print messages (`text` format of Babeltrace 1).");

/* dmesg source */
BT_PLUGIN_SOURCE_COMPONENT_CLASS(dmesg, dmesg_msg_iter_next);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_DESCRIPTION(dmesg,
	"Read a dmesg output from a file or from standard input.");
BT_PLUGIN_SOURCE_COMPONENT_CLASS_INIT_METHOD(dmesg, dmesg_init);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_FINALIZE_METHOD(dmesg, dmesg_finalize);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_INIT_METHOD(dmesg,
	dmesg_msg_iter_init);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_FINALIZE_METHOD(dmesg,
	dmesg_msg_iter_finalize);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_SEEK_BEGINNING_METHOD(dmesg,
	dmesg_msg_iter_seek_beginning);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CAN_SEEK_BEGINNING_METHOD(dmesg,
	dmesg_msg_iter_can_seek_beginning);
