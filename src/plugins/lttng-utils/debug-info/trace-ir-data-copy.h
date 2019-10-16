#ifndef BABELTRACE_PLUGIN_DEBUG_INFO_TRACE_DATA_COPY_H
#define BABELTRACE_PLUGIN_DEBUG_INFO_TRACE_DATA_COPY_H

/*
 * Babeltrace - Trace IR data object copy
 *
 * Copyright (c) 2019 EfficiOS Inc. and Linux Foundation
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

#include <babeltrace2/babeltrace.h>

#include "common/macros.h"
#include "trace-ir-mapping.h"

BT_HIDDEN
enum debug_info_trace_ir_mapping_status copy_trace_content(
		const bt_trace *in_trace, bt_trace *out_trace,
		bt_logging_level log_level, bt_self_component *self_comp);
BT_HIDDEN
enum debug_info_trace_ir_mapping_status copy_stream_content(
		const bt_stream *in_stream, bt_stream *out_stream,
		bt_logging_level log_level, bt_self_component *self_comp);
BT_HIDDEN
enum debug_info_trace_ir_mapping_status copy_packet_content(
		const bt_packet *in_packet, bt_packet *out_packet,
		bt_logging_level log_level, bt_self_component *self_comp);
BT_HIDDEN
enum debug_info_trace_ir_mapping_status copy_event_content(
		const bt_event *in_event, bt_event *out_event,
		bt_logging_level log_level, bt_self_component *self_comp);
BT_HIDDEN
enum debug_info_trace_ir_mapping_status copy_field_content(
		const bt_field *in_field, bt_field *out_field,
		bt_logging_level log_level, bt_self_component *self_comp);

#endif /* BABELTRACE_PLUGIN_DEBUG_INFO_TRACE_DATA_COPY_H */
