#ifndef BABELTRACE_PLUGINS_TEXT_DETAILS_OBJ_LIFETIME_MGMT_H
#define BABELTRACE_PLUGINS_TEXT_DETAILS_OBJ_LIFETIME_MGMT_H

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

#include <glib.h>
#include <babeltrace2/babeltrace.h>
#include <stdbool.h>

#include "details.h"
#include "write.h"

/*
 * Returns whether or not stream class or event class `obj`, which
 * belongs to `tc`, needs to be written.
 */
BT_HIDDEN
bool details_need_to_write_meta_object(struct details_write_ctx *ctx,
		const bt_trace_class *tc, const void *obj);

/*
 * Marks stream class or event class `obj`, which belongs to `tc`, as
 * written.
 */
BT_HIDDEN
void details_did_write_meta_object(struct details_write_ctx *ctx,
		const bt_trace_class *tc, const void *obj);

/*
 * Returns whether or not trace class `tc` needs to be written.
 */
BT_HIDDEN
bool details_need_to_write_trace_class(struct details_write_ctx *ctx,
		const bt_trace_class *tc);

/*
 * Marks trace class `tc` as written.
 */
BT_HIDDEN
int details_did_write_trace_class(struct details_write_ctx *ctx,
		const bt_trace_class *tc);

/*
 * Writes the unique trace ID of `trace` to `*unique_id`, allocating a
 * new unique ID if none exists.
 */
BT_HIDDEN
int details_trace_unique_id(struct details_write_ctx *ctx,
		const bt_trace *trace, uint64_t *unique_id);

#endif /* BABELTRACE_PLUGINS_TEXT_DETAILS_OBJ_LIFETIME_MGMT_H */
