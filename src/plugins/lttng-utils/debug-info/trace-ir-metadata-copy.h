/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2015 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright (c) 2019 Francis Deslauriers <francis.deslauriers@efficios.com>
 *
 * Babeltrace - Trace IR metadata object copy
 */

#ifndef BABELTRACE_PLUGIN_DEBUG_INFO_TRACE_METADATA_COPY_H
#define BABELTRACE_PLUGIN_DEBUG_INFO_TRACE_METADATA_COPY_H

#include <babeltrace2/babeltrace.h>
#include "common/macros.h"
#include "trace-ir-mapping.h"

BT_HIDDEN
enum debug_info_trace_ir_mapping_status copy_trace_class_content(
		struct trace_ir_maps *trace_ir_maps,
		const bt_trace_class *in_trace_class,
		bt_trace_class *out_trace_class,
		bt_logging_level log_level,
		bt_self_component *self_comp);

BT_HIDDEN
enum debug_info_trace_ir_mapping_status copy_stream_class_content(
		struct trace_ir_maps *trace_ir_maps,
		const bt_stream_class *in_stream_class,
		bt_stream_class *out_stream_class);

BT_HIDDEN
enum debug_info_trace_ir_mapping_status copy_event_class_content(
		struct trace_ir_maps *trace_ir_maps,
		const bt_event_class *in_event_class,
		bt_event_class *out_event_class);

BT_HIDDEN
enum debug_info_trace_ir_mapping_status copy_field_class_content(
		struct trace_ir_metadata_maps *trace_ir_metadata_maps,
		const bt_field_class *in_field_class,
		bt_field_class *out_field_class);

BT_HIDDEN
enum debug_info_trace_ir_mapping_status copy_event_common_context_field_class_content(
		struct trace_ir_metadata_maps *trace_ir_metadata_maps,
		const char *debug_info_field_class_name,
		const bt_field_class *in_field_class,
		bt_field_class *out_field_class);

BT_HIDDEN
bt_field_class *create_field_class_copy(
		struct trace_ir_metadata_maps *trace_ir_metadata_maps,
		const bt_field_class *in_field_class);

#endif /* BABELTRACE_PLUGIN_DEBUG_INFO_TRACE_METADATA_COPY_H */
