/*
 * plugin.c
 *
 * Babeltrace CTF Plug-in Registration Symbols
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
#include "fs-src/fs.h"

#ifndef BT_BUILT_IN_PLUGINS
BT_PLUGIN_MODULE();
#endif

/* Initialize plug-in description. */
BT_PLUGIN(ctf);
BT_PLUGIN_DESCRIPTION("CTF source and sink support");
BT_PLUGIN_AUTHOR("Julien Desfossez, Mathieu Desnoyers, Jérémie Galarneau, Philippe Proulx");
BT_PLUGIN_LICENSE("MIT");

/* ctf.fs source */
BT_PLUGIN_SOURCE_COMPONENT_CLASS(fs, ctf_fs_iterator_next);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_DESCRIPTION(fs,
	"Read CTF traces from the file system.");
BT_PLUGIN_SOURCE_COMPONENT_CLASS_INIT_METHOD(fs, ctf_fs_init);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_QUERY_METHOD(fs, ctf_fs_query);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_FINALIZE_METHOD(fs, ctf_fs_finalize);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_INIT_METHOD(fs,
	ctf_fs_iterator_init);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_FINALIZE_METHOD(fs,
	ctf_fs_iterator_finalize);

#if 0
/* ctf.fs sink */
BT_PLUGIN_SINK_COMPONENT_CLASS(fs, writer_run);
BT_PLUGIN_SINK_COMPONENT_CLASS_INIT_METHOD(fs, writer_component_init);
BT_PLUGIN_SINK_COMPONENT_CLASS_PORT_CONNECTED_METHOD(fs,
		writer_component_port_connected);
BT_PLUGIN_SINK_COMPONENT_CLASS_FINALIZE_METHOD(fs, writer_component_finalize);
BT_PLUGIN_SINK_COMPONENT_CLASS_DESCRIPTION(fs, "Write CTF traces to the file system.");

/* ctf.lttng-live source */
BT_PLUGIN_SOURCE_COMPONENT_CLASS_WITH_ID(auto, lttng_live, "lttng-live",
	lttng_live_iterator_next);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_DESCRIPTION_WITH_ID(auto, lttng_live,
        "Connect to an LTTng relay daemon and receive CTF streams.");
BT_PLUGIN_SOURCE_COMPONENT_CLASS_INIT_METHOD_WITH_ID(auto, lttng_live,
	lttng_live_component_init);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_QUERY_METHOD_WITH_ID(auto, lttng_live,
	lttng_live_query);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_FINALIZE_METHOD_WITH_ID(auto, lttng_live,
	lttng_live_component_finalize);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_ACCEPT_PORT_CONNECTION_METHOD_WITH_ID(auto,
	lttng_live, lttng_live_accept_port_connection);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_INIT_METHOD_WITH_ID(
	auto, lttng_live, lttng_live_iterator_init);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_FINALIZE_METHOD_WITH_ID(
	auto, lttng_live, lttng_live_iterator_finalize);
#endif
