/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_PLUGINS_TEXT_DETAILS_OBJ_LIFETIME_MGMT_H
#define BABELTRACE_PLUGINS_TEXT_DETAILS_OBJ_LIFETIME_MGMT_H

#include <glib.h>
#include <babeltrace2/babeltrace.h>
#include <stdbool.h>

#include "details.h"
#include "write.h"

/*
 * Returns whether or not stream class or event class `obj`, which
 * belongs to `tc`, needs to be written.
 */
bool details_need_to_write_meta_object(struct details_write_ctx *ctx,
		const bt_trace_class *tc, const void *obj);

/*
 * Marks stream class or event class `obj`, which belongs to `tc`, as
 * written.
 */
void details_did_write_meta_object(struct details_write_ctx *ctx,
		const bt_trace_class *tc, const void *obj);

/*
 * Returns whether or not trace class `tc` needs to be written.
 */
bool details_need_to_write_trace_class(struct details_write_ctx *ctx,
		const bt_trace_class *tc);

/*
 * Marks trace class `tc` as written.
 */
int details_did_write_trace_class(struct details_write_ctx *ctx,
		const bt_trace_class *tc);

/*
 * Writes the unique trace ID of `trace` to `*unique_id`, allocating a
 * new unique ID if none exists.
 */
int details_trace_unique_id(struct details_write_ctx *ctx,
		const bt_trace *trace, uint64_t *unique_id);

#endif /* BABELTRACE_PLUGINS_TEXT_DETAILS_OBJ_LIFETIME_MGMT_H */
