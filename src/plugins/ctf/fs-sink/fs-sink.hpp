/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_PLUGINS_CTF_FS_SINK_FS_SINK_HPP
#define BABELTRACE_PLUGINS_CTF_FS_SINK_FS_SINK_HPP

#include <glib.h>

#include <babeltrace2/babeltrace.h>

#include "cpp-common/bt2c/logging.hpp"

struct fs_sink_comp
{
    explicit fs_sink_comp(const bt2::SelfSinkComponent selfSinkComp) :
        logger {selfSinkComp, "PLUGIN/SINK.CTF.FS/COMP"}
    {
    }

    bt2c::Logger logger;

    /* Owned by this */
    bt_message_iterator *upstream_iter = nullptr;

    /* Base output directory path */
    GString *output_dir_path = nullptr;

    /*
     * True if the component assumes that it will only write a
     * single CTF trace (which can contain one or more data
     * streams). This makes the component write the stream files
     * directly in the output directory (`output_dir_path` above).
     */
    bool assume_single_trace = false;

    /* True to completely ignore discarded events messages */
    bool ignore_discarded_events = false;

    /* True to completely ignore discarded packets messages */
    bool ignore_discarded_packets = false;

    /* Create the LTTng index */
    bool create_lttng_index = false;

    /*
     * True to make the component quiet (nothing printed to the
     * standard output).
     */
    bool quiet = false;

    /*
     * CTF version to generate (1 or 2).
     *
     * CTF 1.8 implies MIP 0 and CTF 2 implies MIP 1.
     */
    unsigned int ctf_version = 2;

    /*
     * Hash table of `const bt_trace *` (weak) to
     * `struct fs_sink_trace *` (owned by hash table).
     */
    GHashTable *traces = nullptr;
};

bt_component_class_initialize_method_status
ctf_fs_sink_init(bt_self_component_sink *component, bt_self_component_sink_configuration *config,
                 const bt_value *params, void *init_method_data);

bt_component_class_sink_consume_method_status
ctf_fs_sink_consume(bt_self_component_sink *component);

bt_component_class_sink_graph_is_configured_method_status
ctf_fs_sink_graph_is_configured(bt_self_component_sink *component);

bt_component_class_get_supported_mip_versions_method_status
ctf_fs_sink_supported_mip_versions(bt_self_component_class_sink *self_component_class,
                                   const bt_value *params, void *initialize_method_data,
                                   bt_logging_level logging_level,
                                   bt_integer_range_set_unsigned *supported_versions);

void ctf_fs_sink_finalize(bt_self_component_sink *component);

#endif /* BABELTRACE_PLUGINS_CTF_FS_SINK_FS_SINK_HPP */
