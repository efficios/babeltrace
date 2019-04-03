#ifndef BABELTRACE_PLUGIN_DEBUG_INFO_FIELD_CLASS_COPY_H
#define BABELTRACE_PLUGIN_DEBUG_INFO_FIELD_CLASS_COPY_H

/*
 * Babeltrace - Trace IR metadata field class copy
 *
 * Copyright (c) 2015-2019 EfficiOS Inc. and Linux Foundation
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
int copy_field_class_content_internal(struct trace_ir_metadata_maps *trace_ir_metadata_maps,
		const bt_field_class *in_field_class,
		bt_field_class *out_field_class);

BT_HIDDEN
bt_field_class *create_field_class_copy_internal(
		struct trace_ir_metadata_maps *trace_ir_metadata_maps,
		const bt_field_class *in_field_class);

#endif /* BABELTRACE_PLUGIN_DEBUG_INFO_FIELD_CLASS_COPY_H */
