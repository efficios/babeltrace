#ifndef BABELTRACE_PLUGIN_DEBUG_INFO_TRACE_METADATA_COPY_H
#define BABELTRACE_PLUGIN_DEBUG_INFO_TRACE_METADATA_COPY_H

/*
 * Babeltrace - Trace IR metadata object copy
 *
 * Copyright (c) 2015 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright (c) 2019 Francis Deslauriers <francis.deslauriers@efficios.com>
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

#include <babeltrace/babeltrace.h>
#include "trace-ir-mapping.h"

BT_HIDDEN
int copy_trace_class_content(const bt_trace_class *in_trace_class,
		bt_trace_class *out_trace_class);

BT_HIDDEN
int copy_stream_class_content(struct trace_ir_maps *trace_ir_maps,
		const bt_stream_class *in_stream_class,
		bt_stream_class *out_stream_class);

BT_HIDDEN
int copy_event_class_content(struct trace_ir_maps *trace_ir_maps,
		const bt_event_class *in_event_class,
		bt_event_class *out_event_class);

BT_HIDDEN
int copy_field_class_content(struct trace_ir_metadata_maps *trace_ir_metadata_maps,
		const bt_field_class *in_field_class,
		bt_field_class *out_field_class);

BT_HIDDEN
int copy_event_common_context_field_class_content(
		struct trace_ir_metadata_maps *trace_ir_metadata_maps,
		const char *debug_info_field_class_name,
		const bt_field_class *in_field_class,
		bt_field_class *out_field_class);

BT_HIDDEN
bt_field_class *create_field_class_copy(
		struct trace_ir_metadata_maps *trace_ir_metadata_maps,
		const bt_field_class *in_field_class);

#endif /* BABELTRACE_PLUGIN_DEBUG_INFO_TRACE_METADATA_COPY_H */
