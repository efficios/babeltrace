/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_PLUGIN_CTF_FS_SINK_FS_SINK_TRACE_H
#define BABELTRACE_PLUGIN_CTF_FS_SINK_FS_SINK_TRACE_H

#include "common/macros.h"
#include <babeltrace2/babeltrace.h>
#include "ctfser/ctfser.h"
#include <glib.h>
#include <stdint.h>

#include "fs-sink-ctf-meta.hpp"

struct fs_sink_comp;

struct fs_sink_trace
{
    bt_logging_level log_level;
    struct fs_sink_comp *fs_sink;

    /* Owned by this */
    struct fs_sink_ctf_trace *trace;

    /*
     * Weak reference: this object does not own it, and `trace`
     * above does not own its trace IR trace and trace class either.
     * Instead, we add a "trace destruction" listener (in
     * create_trace()) so that this object gets destroyed when the
     * trace object is destroyed.
     *
     * Otherwise (with a strong reference), we would keep this trace
     * object alive until the upstream message iterator ends. This
     * could "leak" resources (memory, file descriptors) associated
     * to traces and streams which otherwise would not exist.
     */
    const bt_trace *ir_trace;

    bt_listener_id ir_trace_destruction_listener_id;

    /* Trace's directory */
    GString *path;

    /* `metadata` file path */
    GString *metadata_path;

    /*
     * Hash table of `const bt_stream *` (weak) to
     * `struct fs_sink_stream *` (owned by hash table).
     */
    GHashTable *streams;
};

BT_HIDDEN
struct fs_sink_trace *fs_sink_trace_create(struct fs_sink_comp *fs_sink, const bt_trace *ir_trace);

BT_HIDDEN
void fs_sink_trace_destroy(struct fs_sink_trace *trace);

#endif /* BABELTRACE_PLUGIN_CTF_FS_SINK_FS_SINK_TRACE_H */
