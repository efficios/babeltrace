/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2018 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef _CTF_META_VISITORS_H
#define _CTF_META_VISITORS_H

#include <babeltrace2/babeltrace.h>
#include "common/macros.h"

#include "ctf-meta.hpp"

struct meta_log_config;

BT_HIDDEN
int ctf_trace_class_resolve_field_classes(struct ctf_trace_class *tc,
                                          struct meta_log_config *log_cfg);

BT_HIDDEN
int ctf_trace_class_translate(bt_self_component *self_comp, bt_trace_class *ir_tc,
                              struct ctf_trace_class *tc);

BT_HIDDEN
int ctf_trace_class_update_default_clock_classes(struct ctf_trace_class *ctf_tc,
                                                 struct meta_log_config *log_cfg);

BT_HIDDEN
int ctf_trace_class_update_in_ir(struct ctf_trace_class *ctf_tc);

BT_HIDDEN
int ctf_trace_class_update_meanings(struct ctf_trace_class *ctf_tc);

BT_HIDDEN
int ctf_trace_class_update_text_array_sequence(struct ctf_trace_class *ctf_tc);

BT_HIDDEN
int ctf_trace_class_update_alignments(struct ctf_trace_class *ctf_tc);

BT_HIDDEN
int ctf_trace_class_update_value_storing_indexes(struct ctf_trace_class *ctf_tc);

BT_HIDDEN
int ctf_trace_class_update_stream_class_config(struct ctf_trace_class *ctf_tc);

BT_HIDDEN
int ctf_trace_class_validate(struct ctf_trace_class *ctf_tc, struct meta_log_config *log_cfg);

BT_HIDDEN
void ctf_trace_class_warn_meaningless_header_fields(struct ctf_trace_class *ctf_tc,
                                                    struct meta_log_config *log_cfg);

#endif /* _CTF_META_VISITORS_H */
