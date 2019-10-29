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

#include <stdbool.h>

#include <babeltrace2/babeltrace.h>

#include "common/common.h"
#include "common/assert.h"
#include "compat/glib.h"

#include "details.h"
#include "write.h"
#include "obj-lifetime-mgmt.h"

static
void trace_class_destruction_listener(const bt_trace_class *tc, void *data)
{
	struct details_comp *details_comp = data;

	BT_ASSERT(details_comp);
	BT_ASSERT(details_comp->meta);

	/* Remove from hash table, which also destroys the value */
	g_hash_table_remove(details_comp->meta, tc);
}

static
struct details_trace_class_meta *borrow_trace_class_meta(
		struct details_write_ctx *ctx, const bt_trace_class *tc)
{
	struct details_trace_class_meta *details_tc_meta;

	BT_ASSERT_DBG(ctx->details_comp->cfg.with_meta);
	BT_ASSERT_DBG(ctx->details_comp->meta);
	details_tc_meta = g_hash_table_lookup(ctx->details_comp->meta, tc);
	if (!details_tc_meta) {
		/* Not found: create one */
		details_tc_meta = details_create_details_trace_class_meta();
		if (!details_tc_meta) {
			goto error;
		}

		/* Register trace class destruction listener */
		if (bt_trace_class_add_destruction_listener(tc,
				trace_class_destruction_listener,
				ctx->details_comp,
				&details_tc_meta->tc_destruction_listener_id)) {
			goto error;
		}

		/* Insert into hash table (becomes the owner) */
		g_hash_table_insert(ctx->details_comp->meta, (void *) tc,
			details_tc_meta);
	}

	goto end;

error:
	details_destroy_details_trace_class_meta(details_tc_meta);
	details_tc_meta = NULL;

end:
	return details_tc_meta;
}

BT_HIDDEN
bool details_need_to_write_meta_object(struct details_write_ctx *ctx,
		const bt_trace_class *tc, const void *obj)
{
	bool need_to_write;
	struct details_trace_class_meta *details_tc_meta;

	if (!ctx->details_comp->cfg.with_meta) {
		need_to_write = false;
		goto end;
	}

	BT_ASSERT_DBG(ctx->details_comp->meta);
	details_tc_meta = g_hash_table_lookup(ctx->details_comp->meta, tc);
	BT_ASSERT_DBG(details_tc_meta);
	need_to_write =
		!g_hash_table_lookup(details_tc_meta->objects, obj);

end:
	return need_to_write;
}

BT_HIDDEN
void details_did_write_meta_object(struct details_write_ctx *ctx,
		const bt_trace_class *tc, const void *obj)
{
	struct details_trace_class_meta *details_tc_meta;

	BT_ASSERT(ctx->details_comp->cfg.with_meta);
	details_tc_meta = borrow_trace_class_meta(ctx, tc);
	BT_ASSERT(details_tc_meta);
	g_hash_table_insert(details_tc_meta->objects, (gpointer) obj,
		GUINT_TO_POINTER(1));
}

BT_HIDDEN
bool details_need_to_write_trace_class(struct details_write_ctx *ctx,
		const bt_trace_class *tc)
{
	struct details_trace_class_meta *details_tc_meta;
	bool need_to_write;

	if (!ctx->details_comp->cfg.with_meta) {
		need_to_write = false;
		goto end;
	}

	BT_ASSERT_DBG(ctx->details_comp->meta);
	details_tc_meta = g_hash_table_lookup(ctx->details_comp->meta, tc);
	need_to_write = !details_tc_meta;

end:
	return need_to_write;
}

BT_HIDDEN
int details_did_write_trace_class(struct details_write_ctx *ctx,
		const bt_trace_class *tc)
{
	int ret = 0;
	struct details_trace_class_meta *details_tc_meta;

	BT_ASSERT(ctx->details_comp->cfg.with_meta);

	/* borrow_trace_class_meta() creates an entry if none exists */
	details_tc_meta = borrow_trace_class_meta(ctx, tc);
	if (!details_tc_meta) {
		ret = -1;
	}

	return ret;
}

static
void trace_destruction_listener(const bt_trace *trace, void *data)
{
	struct details_comp *details_comp = data;

	BT_ASSERT(details_comp);
	BT_ASSERT(details_comp->traces);

	/* Remove from hash table, which also destroys the value */
	g_hash_table_remove(details_comp->traces, trace);
}

static
struct details_trace *create_details_trace(uint64_t unique_id)
{
	struct details_trace *details_trace = g_new0(struct details_trace, 1);

	if (!details_trace) {
		goto end;
	}

	details_trace->unique_id = unique_id;
	details_trace->trace_destruction_listener_id = UINT64_C(-1);

end:
	return details_trace;
}


BT_HIDDEN
int details_trace_unique_id(struct details_write_ctx *ctx,
		const bt_trace *trace, uint64_t *unique_id)
{
	int ret = 0;
	struct details_trace *details_trace = NULL;

	BT_ASSERT_DBG(unique_id);
	BT_ASSERT_DBG(ctx->details_comp->traces);
	if (!bt_g_hash_table_contains(ctx->details_comp->traces,
			trace)) {
		/* Not found: create one */
		*unique_id = ctx->details_comp->next_unique_trace_id;
		details_trace = create_details_trace(*unique_id);
		if (!details_trace) {
			goto error;
		}

		ctx->details_comp->next_unique_trace_id++;

		/* Register trace destruction listener if there's none */
		if (bt_trace_add_destruction_listener(trace,
				trace_destruction_listener,
				ctx->details_comp,
				&details_trace->trace_destruction_listener_id)) {
			goto error;
		}

		BT_ASSERT(details_trace->trace_destruction_listener_id !=
			UINT64_C(-1));

		/* Move to hash table */
		g_hash_table_insert(ctx->details_comp->traces, (gpointer) trace,
			details_trace);
		details_trace = NULL;
	} else {
		/* Found */
		details_trace = g_hash_table_lookup(
			ctx->details_comp->traces, trace);
		*unique_id = details_trace->unique_id;
		details_trace = NULL;
	}

	goto end;

error:
	ret = -1;

end:
	g_free(details_trace);

	return ret;
}
