/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Babeltrace CTF Plug-in Registration Symbols
 */

#include <babeltrace2/babeltrace.h>

#include "fs-sink/fs-sink.hpp"
#include "fs-src/fs.hpp"
#include "lttng-live/lttng-live.hpp"

#ifndef BT_BUILT_IN_PLUGINS
BT_PLUGIN_MODULE();
#endif

/* Initialize plug-in description. */
BT_PLUGIN(ctf);
BT_PLUGIN_DESCRIPTION("CTF input and output");
BT_PLUGIN_AUTHOR("EfficiOS <https://www.efficios.com/>");
BT_PLUGIN_LICENSE("MIT");
BT_PLUGIN_VERSION(2, 1, 0, NULL);

/* ctf.fs source */
BT_PLUGIN_SOURCE_COMPONENT_CLASS(fs, ctf_fs_iterator_next);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_DESCRIPTION(fs, "Read CTF traces from the file system.");
BT_PLUGIN_SOURCE_COMPONENT_CLASS_HELP(fs, "See the babeltrace2-source.ctf.fs(7) manual page.");
BT_PLUGIN_SOURCE_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD(
    fs, ctf_fs_get_supported_mip_versions);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_INITIALIZE_METHOD(fs, ctf_fs_init);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_QUERY_METHOD(fs, ctf_fs_query);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_FINALIZE_METHOD(fs, ctf_fs_finalize);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD(fs, ctf_fs_iterator_init);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_FINALIZE_METHOD(fs,
                                                                        ctf_fs_iterator_finalize);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHODS(
    fs, ctf_fs_iterator_seek_beginning, NULL);

/* ctf.fs sink */
BT_PLUGIN_SINK_COMPONENT_CLASS(fs, ctf_fs_sink_consume);
BT_PLUGIN_SINK_COMPONENT_CLASS_INITIALIZE_METHOD(fs, ctf_fs_sink_init);
BT_PLUGIN_SINK_COMPONENT_CLASS_FINALIZE_METHOD(fs, ctf_fs_sink_finalize);
BT_PLUGIN_SINK_COMPONENT_CLASS_GRAPH_IS_CONFIGURED_METHOD(fs, ctf_fs_sink_graph_is_configured);
BT_PLUGIN_SINK_COMPONENT_CLASS_DESCRIPTION(fs, "Write CTF traces to the file system.");
BT_PLUGIN_SINK_COMPONENT_CLASS_HELP(fs, "See the babeltrace2-sink.ctf.fs(7) manual page.");
BT_PLUGIN_SINK_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD(
    fs, ctf_fs_sink_supported_mip_versions);

/* ctf.lttng-live source */
BT_PLUGIN_SOURCE_COMPONENT_CLASS_WITH_ID(auto, lttng_live, "lttng-live", lttng_live_msg_iter_next);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_DESCRIPTION_WITH_ID(
    auto, lttng_live, "Connect to an LTTng relay daemon and receive CTF streams.");
BT_PLUGIN_SOURCE_COMPONENT_CLASS_HELP_WITH_ID(
    auto, lttng_live, "See the babeltrace2-source.ctf.lttng-live(7) manual page.");
BT_PLUGIN_SOURCE_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD_WITH_ID(
    auto, lttng_live, lttng_live_get_supported_mip_versions);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_INITIALIZE_METHOD_WITH_ID(auto, lttng_live,
                                                           lttng_live_component_init);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_QUERY_METHOD_WITH_ID(auto, lttng_live, lttng_live_query);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_FINALIZE_METHOD_WITH_ID(auto, lttng_live,
                                                         lttng_live_component_finalize);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_WITH_ID(
    auto, lttng_live, lttng_live_msg_iter_init);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_FINALIZE_METHOD_WITH_ID(
    auto, lttng_live, lttng_live_msg_iter_finalize);
