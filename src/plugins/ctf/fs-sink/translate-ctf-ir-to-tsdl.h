/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_PLUGIN_CTF_FS_SINK_TRANSLATE_CTF_IR_TO_TSDL_H
#define BABELTRACE_PLUGIN_CTF_FS_SINK_TRANSLATE_CTF_IR_TO_TSDL_H

#include <glib.h>

#include "common/macros.h"
#include "fs-sink-ctf-meta.h"

BT_HIDDEN
void translate_trace_ctf_ir_to_tsdl(struct fs_sink_ctf_trace *trace,
		GString *tsdl);

#endif /* BABELTRACE_PLUGIN_CTF_FS_SINK_TRANSLATE_CTF_IR_TO_TSDL_H */
