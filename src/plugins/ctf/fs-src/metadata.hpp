/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2016 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef CTF_FS_METADATA_H
#define CTF_FS_METADATA_H

#include <stdbool.h>
#include <stdio.h>
#include <glib.h>
#include "common/macros.h"
#include <babeltrace2/babeltrace.h>

#define CTF_FS_METADATA_FILENAME "metadata"

struct ctf_fs_trace;
struct ctf_fs_metadata;

struct ctf_fs_metadata_config
{
    bool force_clock_class_origin_unix_epoch;
    int64_t clock_class_offset_s;
    int64_t clock_class_offset_ns;
};

int ctf_fs_metadata_init(struct ctf_fs_metadata *metadata);

void ctf_fs_metadata_fini(struct ctf_fs_metadata *metadata);

int ctf_fs_metadata_set_trace_class(bt_self_component *self_comp, struct ctf_fs_trace *ctf_fs_trace,
                                    struct ctf_fs_metadata_config *config);

FILE *ctf_fs_metadata_open_file(const char *trace_path, bt_logging_level log_level,
                                bt_self_component_class *comp_class);

bool ctf_metadata_is_packetized(FILE *fp, int *byte_order);

#endif /* CTF_FS_METADATA_H */
