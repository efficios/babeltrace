/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_PLUGIN_CTF_FS_SINK_FS_SINK_H
#define BABELTRACE_PLUGIN_CTF_FS_SINK_FS_SINK_H

#include <glib.h>

#include <babeltrace2/babeltrace.h>

struct fs_sink_comp
{
    bt_logging_level log_level;
    bt_self_component *self_comp;

    /* Owned by this */
    bt_message_iterator *upstream_iter;

    /* Base output directory path */
    GString *output_dir_path;

    /*
     * True if the component assumes that it will only write a
     * single CTF trace (which can contain one or more data
     * streams). This makes the component write the stream files
     * directly in the output directory (`output_dir_path` above).
     */
    bool assume_single_trace;

    /* True to completely ignore discarded events messages */
    bool ignore_discarded_events;

    /* True to completely ignore discarded packets messages */
    bool ignore_discarded_packets;

    /*
     * True to make the component quiet (nothing printed to the
     * standard output).
     */
    bool quiet;

    /*
     * Hash table of `const bt_trace *` (weak) to
     * `struct fs_sink_trace *` (owned by hash table).
     */
    GHashTable *traces;
};

bt_component_class_initialize_method_status
ctf_fs_sink_init(bt_self_component_sink *component, bt_self_component_sink_configuration *config,
                 const bt_value *params, void *init_method_data);

bt_component_class_sink_consume_method_status
ctf_fs_sink_consume(bt_self_component_sink *component);

bt_component_class_sink_graph_is_configured_method_status
ctf_fs_sink_graph_is_configured(bt_self_component_sink *component);

void ctf_fs_sink_finalize(bt_self_component_sink *component);

#endif /* BABELTRACE_PLUGIN_CTF_FS_SINK_FS_SINK_H */
