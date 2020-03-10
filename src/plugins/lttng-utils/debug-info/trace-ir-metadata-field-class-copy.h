/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2015-2019 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2019 Francis Deslauriers <francis.deslauriers@efficios.com>
 *
 * Babeltrace - Trace IR metadata field class copy
 */

#ifndef BABELTRACE_PLUGIN_DEBUG_INFO_FIELD_CLASS_COPY_H
#define BABELTRACE_PLUGIN_DEBUG_INFO_FIELD_CLASS_COPY_H

#include <babeltrace2/babeltrace.h>
#include "common/macros.h"
#include "trace-ir-mapping.h"

BT_HIDDEN
int copy_field_class_content_internal(struct trace_ir_metadata_maps *trace_ir_metadata_maps,
		const bt_field_class *in_field_class,
		bt_field_class *out_field_class);

BT_HIDDEN
bt_field_class *create_field_class_copy_internal(
		struct trace_ir_metadata_maps *trace_ir_metadata_maps,
		const bt_field_class *in_field_class);

#endif /* BABELTRACE_PLUGIN_DEBUG_INFO_FIELD_CLASS_COPY_H */
