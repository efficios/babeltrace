#ifndef BABELTRACE_PLUGIN_CTF_FS_SINK_TRANSLATE_TRACE_IR_TO_CTF_IR_H
#define BABELTRACE_PLUGIN_CTF_FS_SINK_TRANSLATE_TRACE_IR_TO_CTF_IR_H

/*
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
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

#include "common/macros.h"
#include <babeltrace2/babeltrace.h>

#include "fs-sink.h"
#include "fs-sink-ctf-meta.h"

BT_HIDDEN
int try_translate_event_class_trace_ir_to_ctf_ir(
		struct fs_sink_comp *fs_sink,
		struct fs_sink_ctf_stream_class *sc,
		const bt_event_class *ir_ec,
		struct fs_sink_ctf_event_class **out_ec);

BT_HIDDEN
int try_translate_stream_class_trace_ir_to_ctf_ir(
		struct fs_sink_comp *fs_sink,
		struct fs_sink_ctf_trace *trace,
		const bt_stream_class *ir_sc,
		struct fs_sink_ctf_stream_class **out_sc);

BT_HIDDEN
struct fs_sink_ctf_trace *translate_trace_trace_ir_to_ctf_ir(
		struct fs_sink_comp *fs_sink, const bt_trace *ir_trace);

#endif /* BABELTRACE_PLUGIN_CTF_FS_SINK_TRANSLATE_TRACE_IR_TO_CTF_IR_H */
