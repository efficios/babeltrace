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

#include <babeltrace2/babeltrace.h>

#include "fs-src/fs.h"
#include "fs-sink/fs-sink.h"
#include "lttng-live/lttng-live.h"

#ifndef BT_BUILT_IN_PLUGINS
BT_PLUGIN_MODULE();
#endif

/* Initialize plug-in description. */
BT_PLUGIN(ctf);
BT_PLUGIN_DESCRIPTION("CTF input and output");
BT_PLUGIN_AUTHOR("EfficiOS <https://www.efficios.com/>");
BT_PLUGIN_LICENSE("MIT");

/* ctf.fs source */
BT_PLUGIN_SOURCE_COMPONENT_CLASS(fs, ctf_fs_iterator_next);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_DESCRIPTION(fs,
	"Read CTF traces from the file system.");
BT_PLUGIN_SOURCE_COMPONENT_CLASS_HELP(fs,
	"See the babeltrace2-source.ctf.fs(7) manual page.");
BT_PLUGIN_SOURCE_COMPONENT_CLASS_INITIALIZE_METHOD(fs, ctf_fs_init);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_QUERY_METHOD(fs, ctf_fs_query);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_FINALIZE_METHOD(fs, ctf_fs_finalize);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD(fs,
	ctf_fs_iterator_init);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_FINALIZE_METHOD(fs,
	ctf_fs_iterator_finalize);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHODS(fs,
	ctf_fs_iterator_seek_beginning, NULL);

/* ctf.fs sink */
BT_PLUGIN_SINK_COMPONENT_CLASS(fs, ctf_fs_sink_consume);
BT_PLUGIN_SINK_COMPONENT_CLASS_INITIALIZE_METHOD(fs, ctf_fs_sink_init);
BT_PLUGIN_SINK_COMPONENT_CLASS_FINALIZE_METHOD(fs, ctf_fs_sink_finalize);
BT_PLUGIN_SINK_COMPONENT_CLASS_GRAPH_IS_CONFIGURED_METHOD(fs,
	ctf_fs_sink_graph_is_configured);
BT_PLUGIN_SINK_COMPONENT_CLASS_DESCRIPTION(fs,
	"Write CTF traces to the file system.");
BT_PLUGIN_SINK_COMPONENT_CLASS_HELP(fs,
	"See the babeltrace2-sink.ctf.fs(7) manual page.");

/* ctf.lttng-live source */
BT_PLUGIN_SOURCE_COMPONENT_CLASS_WITH_ID(auto, lttng_live, "lttng-live",
	lttng_live_msg_iter_next);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_DESCRIPTION_WITH_ID(auto, lttng_live,
	"Connect to an LTTng relay daemon and receive CTF streams.");
BT_PLUGIN_SOURCE_COMPONENT_CLASS_HELP_WITH_ID(auto, lttng_live,
	"See the babeltrace2-source.ctf.lttng-live(7) manual page.");
BT_PLUGIN_SOURCE_COMPONENT_CLASS_INITIALIZE_METHOD_WITH_ID(auto, lttng_live,
	lttng_live_component_init);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_QUERY_METHOD_WITH_ID(auto, lttng_live,
	lttng_live_query);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_FINALIZE_METHOD_WITH_ID(auto, lttng_live,
	lttng_live_component_finalize);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_WITH_ID(auto,
	lttng_live, lttng_live_msg_iter_init);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_FINALIZE_METHOD_WITH_ID(auto,
	lttng_live, lttng_live_msg_iter_finalize);
