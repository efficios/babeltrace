/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_PLUGIN_CTF_FS_SINK_TRANSLATE_TRACE_IR_TO_CTF_IR_H
#define BABELTRACE_PLUGIN_CTF_FS_SINK_TRANSLATE_TRACE_IR_TO_CTF_IR_H

#include <babeltrace2/babeltrace.h>

int try_translate_event_class_trace_ir_to_ctf_ir(struct fs_sink_comp *fs_sink,
                                                 struct fs_sink_ctf_stream_class *sc,
                                                 const bt_event_class *ir_ec,
                                                 struct fs_sink_ctf_event_class **out_ec);

int try_translate_stream_class_trace_ir_to_ctf_ir(struct fs_sink_comp *fs_sink,
                                                  struct fs_sink_ctf_trace *trace,
                                                  const bt_stream_class *ir_sc,
                                                  struct fs_sink_ctf_stream_class **out_sc);

struct fs_sink_ctf_trace *translate_trace_trace_ir_to_ctf_ir(struct fs_sink_comp *fs_sink,
                                                             const bt_trace *ir_trace);

#endif /* BABELTRACE_PLUGIN_CTF_FS_SINK_TRANSLATE_TRACE_IR_TO_CTF_IR_H */
