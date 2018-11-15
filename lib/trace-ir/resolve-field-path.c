/*
 * Copyright 2018 Philippe Proulx <pproulx@efficios.com>
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

#define BT_LOG_TAG "RESOLVE-FIELD-PATH"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/assert-pre-internal.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/trace-ir/field-types-internal.h>
#include <babeltrace/trace-ir/field-path-internal.h>
#include <babeltrace/trace-ir/field-path.h>
#include <babeltrace/trace-ir/resolve-field-path-internal.h>
#include <limits.h>
#include <stdint.h>
#include <inttypes.h>
#include <glib.h>

static
bool find_field_type_recursive(struct bt_field_type *ft,
		struct bt_field_type *tgt_ft, struct bt_field_path *field_path)
{
	bool found = false;

	if (tgt_ft == ft) {
		found = true;
		goto end;
	}

	switch (ft->id) {
	case BT_FIELD_TYPE_ID_STRUCTURE:
	case BT_FIELD_TYPE_ID_VARIANT:
	{
		struct bt_field_type_named_field_types_container *container_ft =
			(void *) ft;
		uint64_t i;

		for (i = 0; i < container_ft->named_fts->len; i++) {
			struct bt_named_field_type *named_ft =
				BT_FIELD_TYPE_NAMED_FT_AT_INDEX(
					container_ft, i);

			g_array_append_val(field_path->indexes, i);
			found = find_field_type_recursive(named_ft->ft,
				tgt_ft, field_path);
			if (found) {
				goto end;
			}

			g_array_set_size(field_path->indexes,
				field_path->indexes->len - 1);
		}

		break;
	}
	case BT_FIELD_TYPE_ID_STATIC_ARRAY:
	case BT_FIELD_TYPE_ID_DYNAMIC_ARRAY:
	{
		struct bt_field_type_array *array_ft = (void *) ft;

		found = find_field_type_recursive(array_ft->element_ft,
			tgt_ft, field_path);
		break;
	}
	default:
		break;
	}

end:
	return found;
}

static
int find_field_type(struct bt_field_type *root_ft,
		enum bt_scope root_scope, struct bt_field_type *tgt_ft,
		struct bt_field_path **ret_field_path)
{
	int ret = 0;
	struct bt_field_path *field_path = NULL;

	if (!root_ft) {
		goto end;
	}

	field_path = bt_field_path_create();
	if (!field_path) {
		ret = -1;
		goto end;
	}

	field_path->root = root_scope;
	if (!find_field_type_recursive(root_ft, tgt_ft, field_path)) {
		/* Not found here */
		BT_PUT(field_path);
	}

end:
	*ret_field_path = field_path;
	return ret;
}

static
struct bt_field_path *find_field_type_in_ctx(struct bt_field_type *ft,
		struct bt_resolve_field_path_context *ctx)
{
	struct bt_field_path *field_path = NULL;
	int ret;

	ret = find_field_type(ctx->packet_header, BT_SCOPE_PACKET_HEADER,
		ft, &field_path);
	if (ret || field_path) {
		goto end;
	}

	ret = find_field_type(ctx->packet_context, BT_SCOPE_PACKET_CONTEXT,
		ft, &field_path);
	if (ret || field_path) {
		goto end;
	}

	ret = find_field_type(ctx->event_header, BT_SCOPE_EVENT_HEADER,
		ft, &field_path);
	if (ret || field_path) {
		goto end;
	}

	ret = find_field_type(ctx->event_common_context,
		BT_SCOPE_EVENT_COMMON_CONTEXT, ft, &field_path);
	if (ret || field_path) {
		goto end;
	}

	ret = find_field_type(ctx->event_specific_context,
		BT_SCOPE_EVENT_SPECIFIC_CONTEXT, ft, &field_path);
	if (ret || field_path) {
		goto end;
	}

	ret = find_field_type(ctx->event_payload, BT_SCOPE_EVENT_PAYLOAD,
		ft, &field_path);
	if (ret || field_path) {
		goto end;
	}

end:
	return field_path;
}

BT_ASSERT_PRE_FUNC
static inline
bool target_is_before_source(struct bt_field_path *src_field_path,
		struct bt_field_path *tgt_field_path)
{
	bool is_valid = true;
	uint64_t src_i = 0, tgt_i = 0;

	if (tgt_field_path->root < src_field_path->root) {
		goto end;
	}

	if (tgt_field_path->root > src_field_path->root) {
		is_valid = false;
		goto end;
	}

	BT_ASSERT(tgt_field_path->root == src_field_path->root);

	while (src_i < src_field_path->indexes->len &&
			tgt_i < tgt_field_path->indexes->len) {
		uint64_t src_index = bt_field_path_get_index_by_index_inline(
			src_field_path, src_i);
		uint64_t tgt_index = bt_field_path_get_index_by_index_inline(
			tgt_field_path, tgt_i);

		if (tgt_index > src_index) {
			is_valid = false;
			goto end;
		}

		src_i++;
		tgt_i++;
	}

end:
	return is_valid;
}

BT_ASSERT_PRE_FUNC
static inline
struct bt_field_type *borrow_root_field_type(
		struct bt_resolve_field_path_context *ctx, enum bt_scope scope)
{
	switch (scope) {
	case BT_SCOPE_PACKET_HEADER:
		return ctx->packet_header;
	case BT_SCOPE_PACKET_CONTEXT:
		return ctx->packet_context;
	case BT_SCOPE_EVENT_HEADER:
		return ctx->event_header;
	case BT_SCOPE_EVENT_COMMON_CONTEXT:
		return ctx->event_common_context;
	case BT_SCOPE_EVENT_SPECIFIC_CONTEXT:
		return ctx->event_specific_context;
	case BT_SCOPE_EVENT_PAYLOAD:
		return ctx->event_payload;
	default:
		abort();
	}

	return NULL;
}

BT_ASSERT_PRE_FUNC
static inline
struct bt_field_type *borrow_child_field_type(struct bt_field_type *parent_ft,
		uint64_t index, bool *advance)
{
	struct bt_field_type *child_ft = NULL;

	switch (parent_ft->id) {
	case BT_FIELD_TYPE_ID_STRUCTURE:
	case BT_FIELD_TYPE_ID_VARIANT:
	{
		struct bt_named_field_type *named_ft =
			BT_FIELD_TYPE_NAMED_FT_AT_INDEX(parent_ft, index);

		child_ft = named_ft->ft;
		*advance = true;
		break;
	}
	case BT_FIELD_TYPE_ID_STATIC_ARRAY:
	case BT_FIELD_TYPE_ID_DYNAMIC_ARRAY:
	{
		struct bt_field_type_array *array_ft = (void *) parent_ft;

		child_ft = array_ft->element_ft;
		*advance = false;
		break;
	}
	default:
		break;
	}

	return child_ft;
}

BT_ASSERT_PRE_FUNC
static inline
bool target_field_path_in_different_scope_has_struct_ft_only(
		struct bt_field_path *src_field_path,
		struct bt_field_path *tgt_field_path,
		struct bt_resolve_field_path_context *ctx)
{
	bool is_valid = true;
	uint64_t i = 0;
	struct bt_field_type *ft;

	if (src_field_path->root == tgt_field_path->root) {
		goto end;
	}

	ft = borrow_root_field_type(ctx, tgt_field_path->root);

	while (i < tgt_field_path->indexes->len) {
		uint64_t index = bt_field_path_get_index_by_index_inline(
			tgt_field_path, i);
		bool advance;

		if (ft->id == BT_FIELD_TYPE_ID_STATIC_ARRAY ||
				ft->id == BT_FIELD_TYPE_ID_DYNAMIC_ARRAY ||
				ft->id == BT_FIELD_TYPE_ID_VARIANT) {
			is_valid = false;
			goto end;
		}

		ft = borrow_child_field_type(ft, index, &advance);

		if (advance) {
			i++;
		}
	}

end:
	return is_valid;
}

BT_ASSERT_PRE_FUNC
static inline
bool lca_is_structure_field_type(struct bt_field_path *src_field_path,
		struct bt_field_path *tgt_field_path,
		struct bt_resolve_field_path_context *ctx)
{
	bool is_valid = true;
	struct bt_field_type *src_ft;
	struct bt_field_type *tgt_ft;
	struct bt_field_type *prev_ft = NULL;
	uint64_t src_i = 0, tgt_i = 0;

	if (src_field_path->root != tgt_field_path->root) {
		goto end;
	}

	src_ft = borrow_root_field_type(ctx, src_field_path->root);
	tgt_ft = borrow_root_field_type(ctx, tgt_field_path->root);
	BT_ASSERT(src_ft);
	BT_ASSERT(tgt_ft);

	while (src_i < src_field_path->indexes->len &&
			tgt_i < tgt_field_path->indexes->len) {
		bool advance;
		uint64_t src_index = bt_field_path_get_index_by_index_inline(
			src_field_path, src_i);
		uint64_t tgt_index = bt_field_path_get_index_by_index_inline(
			tgt_field_path, tgt_i);

		if (src_ft != tgt_ft) {
			if (!prev_ft) {
				/*
				 * This is correct: the LCA is the root
				 * scope field type, which must be a
				 * structure field type.
				 */
				break;
			}

			if (prev_ft->id != BT_FIELD_TYPE_ID_STRUCTURE) {
				is_valid = false;
			}

			break;
		}

		prev_ft = src_ft;
		src_ft = borrow_child_field_type(src_ft, src_index, &advance);

		if (advance) {
			src_i++;
		}

		tgt_ft = borrow_child_field_type(tgt_ft, tgt_index, &advance);

		if (advance) {
			tgt_i++;
		}
	}

end:
	return is_valid;
}

BT_ASSERT_PRE_FUNC
static inline
bool lca_to_target_has_struct_ft_only(struct bt_field_path *src_field_path,
		struct bt_field_path *tgt_field_path,
		struct bt_resolve_field_path_context *ctx)
{
	bool is_valid = true;
	struct bt_field_type *src_ft;
	struct bt_field_type *tgt_ft;
	uint64_t src_i = 0, tgt_i = 0;

	if (src_field_path->root != tgt_field_path->root) {
		goto end;
	}

	src_ft = borrow_root_field_type(ctx, src_field_path->root);
	tgt_ft = borrow_root_field_type(ctx, tgt_field_path->root);
	BT_ASSERT(src_ft);
	BT_ASSERT(tgt_ft);
	BT_ASSERT(src_ft == tgt_ft);

	/* Find LCA */
	while (src_i < src_field_path->indexes->len &&
			tgt_i < tgt_field_path->indexes->len) {
		bool advance;
		uint64_t src_index = bt_field_path_get_index_by_index_inline(
			src_field_path, src_i);
		uint64_t tgt_index = bt_field_path_get_index_by_index_inline(
			tgt_field_path, tgt_i);

		if (src_i != tgt_i) {
			/* Next FT is different: LCA is `tgt_ft` */
			break;
		}

		src_ft = borrow_child_field_type(src_ft, src_index, &advance);

		if (advance) {
			src_i++;
		}

		tgt_ft = borrow_child_field_type(tgt_ft, tgt_index, &advance);

		if (advance) {
			tgt_i++;
		}
	}

	/* Only structure field types to the target */
	while (tgt_i < tgt_field_path->indexes->len) {
		bool advance;
		uint64_t tgt_index = bt_field_path_get_index_by_index_inline(
			tgt_field_path, tgt_i);

		if (tgt_ft->id == BT_FIELD_TYPE_ID_STATIC_ARRAY ||
				tgt_ft->id == BT_FIELD_TYPE_ID_DYNAMIC_ARRAY ||
				tgt_ft->id == BT_FIELD_TYPE_ID_VARIANT) {
			is_valid = false;
			goto end;
		}

		tgt_ft = borrow_child_field_type(tgt_ft, tgt_index, &advance);

		if (advance) {
			tgt_i++;
		}
	}

end:
	return is_valid;
}

BT_ASSERT_PRE_FUNC
static inline
bool field_path_is_valid(struct bt_field_type *src_ft,
		struct bt_field_type *tgt_ft,
		struct bt_resolve_field_path_context *ctx)
{
	bool is_valid = true;
	struct bt_field_path *src_field_path = find_field_type_in_ctx(
		src_ft, ctx);
	struct bt_field_path *tgt_field_path = find_field_type_in_ctx(
		tgt_ft, ctx);

	if (!src_field_path) {
		BT_ASSERT_PRE_MSG("Cannot find requesting field type in "
			"resolving context: %!+F", src_ft);
		is_valid = false;
		goto end;
	}

	if (!tgt_field_path) {
		BT_ASSERT_PRE_MSG("Cannot find target field type in "
			"resolving context: %!+F", tgt_ft);
		is_valid = false;
		goto end;
	}

	/* Target must be before source */
	if (!target_is_before_source(src_field_path, tgt_field_path)) {
		BT_ASSERT_PRE_MSG("Target field type is located after "
			"requesting field type: %![req-ft-]+F, %![tgt-ft-]+F",
			src_ft, tgt_ft);
		is_valid = false;
		goto end;
	}

	/*
	 * If target is in a different scope than source, there are no
	 * array or variant field types on the way to the target.
	 */
	if (!target_field_path_in_different_scope_has_struct_ft_only(
			src_field_path, tgt_field_path, ctx)) {
		BT_ASSERT_PRE_MSG("Target field type is located in a "
			"different scope than requesting field type, "
			"but within an array or a variant field type: "
			"%![req-ft-]+F, %![tgt-ft-]+F",
			src_ft, tgt_ft);
		is_valid = false;
		goto end;
	}

	/* Same scope: LCA must be a structure field type */
	if (!lca_is_structure_field_type(src_field_path, tgt_field_path, ctx)) {
		BT_ASSERT_PRE_MSG("Lowest common ancestor of target and "
			"requesting field types is not a structure field type: "
			"%![req-ft-]+F, %![tgt-ft-]+F",
			src_ft, tgt_ft);
		is_valid = false;
		goto end;
	}

	/* Same scope: path from LCA to target has no array/variant FTs */
	if (!lca_to_target_has_struct_ft_only(src_field_path, tgt_field_path,
			ctx)) {
		BT_ASSERT_PRE_MSG("Path from lowest common ancestor of target "
			"and requesting field types to target field type "
			"contains an array or a variant field type: "
			"%![req-ft-]+F, %![tgt-ft-]+F", src_ft, tgt_ft);
		is_valid = false;
		goto end;
	}

end:
	bt_put(src_field_path);
	bt_put(tgt_field_path);
	return is_valid;
}

static
struct bt_field_path *resolve_field_path(struct bt_field_type *src_ft,
		struct bt_field_type *tgt_ft,
		struct bt_resolve_field_path_context *ctx)
{
	BT_ASSERT_PRE(field_path_is_valid(src_ft, tgt_ft, ctx),
		"Invalid target field type: %![req-ft-]+F, %![tgt-ft-]+F",
		src_ft, tgt_ft);
	return find_field_type_in_ctx(tgt_ft, ctx);
}

BT_HIDDEN
int bt_resolve_field_paths(struct bt_field_type *ft,
		struct bt_resolve_field_path_context *ctx)
{
	int ret = 0;

	BT_ASSERT(ft);

	/* Resolving part for dynamic array and variant field types */
	switch (ft->id) {
	case BT_FIELD_TYPE_ID_DYNAMIC_ARRAY:
	{
		struct bt_field_type_dynamic_array *dyn_array_ft = (void *) ft;

		if (dyn_array_ft->length_ft) {
			BT_ASSERT(!dyn_array_ft->length_field_path);
			dyn_array_ft->length_field_path = resolve_field_path(
				ft, dyn_array_ft->length_ft, ctx);
			if (!dyn_array_ft->length_field_path) {
				ret = -1;
				goto end;
			}
		}

		break;
	}
	case BT_FIELD_TYPE_ID_VARIANT:
	{
		struct bt_field_type_variant *var_ft = (void *) ft;

		if (var_ft->selector_ft) {
			BT_ASSERT(!var_ft->selector_field_path);
			var_ft->selector_field_path =
				resolve_field_path(ft,
					var_ft->selector_ft, ctx);
			if (!var_ft->selector_field_path) {
				ret = -1;
				goto end;
			}
		}
	}
	default:
		break;
	}

	/* Recursive part */
	switch (ft->id) {
	case BT_FIELD_TYPE_ID_STRUCTURE:
	case BT_FIELD_TYPE_ID_VARIANT:
	{
		struct bt_field_type_named_field_types_container *container_ft =
			(void *) ft;
		uint64_t i;

		for (i = 0; i < container_ft->named_fts->len; i++) {
			struct bt_named_field_type *named_ft =
				BT_FIELD_TYPE_NAMED_FT_AT_INDEX(
					container_ft, i);

			ret = bt_resolve_field_paths(named_ft->ft, ctx);
			if (ret) {
				goto end;
			}
		}

		break;
	}
	case BT_FIELD_TYPE_ID_STATIC_ARRAY:
	case BT_FIELD_TYPE_ID_DYNAMIC_ARRAY:
	{
		struct bt_field_type_array *array_ft = (void *) ft;

		ret = bt_resolve_field_paths(array_ft->element_ft, ctx);
		break;
	}
	default:
		break;
	}

end:
	return ret;
}
