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
#include <babeltrace/object.h>
#include <babeltrace/trace-ir/field-classes-internal.h>
#include <babeltrace/trace-ir/field-path-internal.h>
#include <babeltrace/trace-ir/field-path-const.h>
#include <babeltrace/trace-ir/resolve-field-path-internal.h>
#include <limits.h>
#include <stdint.h>
#include <inttypes.h>
#include <glib.h>

static
bool find_field_class_recursive(struct bt_field_class *fc,
		struct bt_field_class *tgt_fc, struct bt_field_path *field_path)
{
	bool found = false;

	if (tgt_fc == fc) {
		found = true;
		goto end;
	}

	switch (fc->type) {
	case BT_FIELD_CLASS_TYPE_STRUCTURE:
	case BT_FIELD_CLASS_TYPE_VARIANT:
	{
		struct bt_field_class_named_field_class_container *container_fc =
			(void *) fc;
		uint64_t i;

		for (i = 0; i < container_fc->named_fcs->len; i++) {
			struct bt_named_field_class *named_fc =
				BT_FIELD_CLASS_NAMED_FC_AT_INDEX(
					container_fc, i);

			g_array_append_val(field_path->indexes, i);
			found = find_field_class_recursive(named_fc->fc,
				tgt_fc, field_path);
			if (found) {
				goto end;
			}

			g_array_set_size(field_path->indexes,
				field_path->indexes->len - 1);
		}

		break;
	}
	case BT_FIELD_CLASS_TYPE_STATIC_ARRAY:
	case BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY:
	{
		struct bt_field_class_array *array_fc = (void *) fc;

		found = find_field_class_recursive(array_fc->element_fc,
			tgt_fc, field_path);
		break;
	}
	default:
		break;
	}

end:
	return found;
}

static
int find_field_class(struct bt_field_class *root_fc,
		enum bt_scope root_scope, struct bt_field_class *tgt_fc,
		struct bt_field_path **ret_field_path)
{
	int ret = 0;
	struct bt_field_path *field_path = NULL;

	if (!root_fc) {
		goto end;
	}

	field_path = bt_field_path_create();
	if (!field_path) {
		ret = -1;
		goto end;
	}

	field_path->root = root_scope;
	if (!find_field_class_recursive(root_fc, tgt_fc, field_path)) {
		/* Not found here */
		BT_OBJECT_PUT_REF_AND_RESET(field_path);
	}

end:
	*ret_field_path = field_path;
	return ret;
}

static
struct bt_field_path *find_field_class_in_ctx(struct bt_field_class *fc,
		struct bt_resolve_field_path_context *ctx)
{
	struct bt_field_path *field_path = NULL;
	int ret;

	ret = find_field_class(ctx->packet_header, BT_SCOPE_PACKET_HEADER,
		fc, &field_path);
	if (ret || field_path) {
		goto end;
	}

	ret = find_field_class(ctx->packet_context, BT_SCOPE_PACKET_CONTEXT,
		fc, &field_path);
	if (ret || field_path) {
		goto end;
	}

	ret = find_field_class(ctx->event_header, BT_SCOPE_EVENT_HEADER,
		fc, &field_path);
	if (ret || field_path) {
		goto end;
	}

	ret = find_field_class(ctx->event_common_context,
		BT_SCOPE_EVENT_COMMON_CONTEXT, fc, &field_path);
	if (ret || field_path) {
		goto end;
	}

	ret = find_field_class(ctx->event_specific_context,
		BT_SCOPE_EVENT_SPECIFIC_CONTEXT, fc, &field_path);
	if (ret || field_path) {
		goto end;
	}

	ret = find_field_class(ctx->event_payload, BT_SCOPE_EVENT_PAYLOAD,
		fc, &field_path);
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
struct bt_field_class *borrow_root_field_class(
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
struct bt_field_class *borrow_child_field_class(struct bt_field_class *parent_fc,
		uint64_t index, bool *advance)
{
	struct bt_field_class *child_fc = NULL;

	switch (parent_fc->type) {
	case BT_FIELD_CLASS_TYPE_STRUCTURE:
	case BT_FIELD_CLASS_TYPE_VARIANT:
	{
		struct bt_named_field_class *named_fc =
			BT_FIELD_CLASS_NAMED_FC_AT_INDEX(parent_fc, index);

		child_fc = named_fc->fc;
		*advance = true;
		break;
	}
	case BT_FIELD_CLASS_TYPE_STATIC_ARRAY:
	case BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY:
	{
		struct bt_field_class_array *array_fc = (void *) parent_fc;

		child_fc = array_fc->element_fc;
		*advance = false;
		break;
	}
	default:
		break;
	}

	return child_fc;
}

BT_ASSERT_PRE_FUNC
static inline
bool target_field_path_in_different_scope_has_struct_fc_only(
		struct bt_field_path *src_field_path,
		struct bt_field_path *tgt_field_path,
		struct bt_resolve_field_path_context *ctx)
{
	bool is_valid = true;
	uint64_t i = 0;
	struct bt_field_class *fc;

	if (src_field_path->root == tgt_field_path->root) {
		goto end;
	}

	fc = borrow_root_field_class(ctx, tgt_field_path->root);

	while (i < tgt_field_path->indexes->len) {
		uint64_t index = bt_field_path_get_index_by_index_inline(
			tgt_field_path, i);
		bool advance;

		if (fc->type == BT_FIELD_CLASS_TYPE_STATIC_ARRAY ||
				fc->type == BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY ||
				fc->type == BT_FIELD_CLASS_TYPE_VARIANT) {
			is_valid = false;
			goto end;
		}

		fc = borrow_child_field_class(fc, index, &advance);

		if (advance) {
			i++;
		}
	}

end:
	return is_valid;
}

BT_ASSERT_PRE_FUNC
static inline
bool lca_is_structure_field_class(struct bt_field_path *src_field_path,
		struct bt_field_path *tgt_field_path,
		struct bt_resolve_field_path_context *ctx)
{
	bool is_valid = true;
	struct bt_field_class *src_fc;
	struct bt_field_class *tgt_fc;
	struct bt_field_class *prev_fc = NULL;
	uint64_t src_i = 0, tgt_i = 0;

	if (src_field_path->root != tgt_field_path->root) {
		goto end;
	}

	src_fc = borrow_root_field_class(ctx, src_field_path->root);
	tgt_fc = borrow_root_field_class(ctx, tgt_field_path->root);
	BT_ASSERT(src_fc);
	BT_ASSERT(tgt_fc);

	while (src_i < src_field_path->indexes->len &&
			tgt_i < tgt_field_path->indexes->len) {
		bool advance;
		uint64_t src_index = bt_field_path_get_index_by_index_inline(
			src_field_path, src_i);
		uint64_t tgt_index = bt_field_path_get_index_by_index_inline(
			tgt_field_path, tgt_i);

		if (src_fc != tgt_fc) {
			if (!prev_fc) {
				/*
				 * This is correct: the LCA is the root
				 * scope field class, which must be a
				 * structure field class.
				 */
				break;
			}

			if (prev_fc->type != BT_FIELD_CLASS_TYPE_STRUCTURE) {
				is_valid = false;
			}

			break;
		}

		prev_fc = src_fc;
		src_fc = borrow_child_field_class(src_fc, src_index, &advance);

		if (advance) {
			src_i++;
		}

		tgt_fc = borrow_child_field_class(tgt_fc, tgt_index, &advance);

		if (advance) {
			tgt_i++;
		}
	}

end:
	return is_valid;
}

BT_ASSERT_PRE_FUNC
static inline
bool lca_to_target_has_struct_fc_only(struct bt_field_path *src_field_path,
		struct bt_field_path *tgt_field_path,
		struct bt_resolve_field_path_context *ctx)
{
	bool is_valid = true;
	struct bt_field_class *src_fc;
	struct bt_field_class *tgt_fc;
	uint64_t src_i = 0, tgt_i = 0;

	if (src_field_path->root != tgt_field_path->root) {
		goto end;
	}

	src_fc = borrow_root_field_class(ctx, src_field_path->root);
	tgt_fc = borrow_root_field_class(ctx, tgt_field_path->root);
	BT_ASSERT(src_fc);
	BT_ASSERT(tgt_fc);
	BT_ASSERT(src_fc == tgt_fc);

	/* Find LCA */
	while (src_i < src_field_path->indexes->len &&
			tgt_i < tgt_field_path->indexes->len) {
		bool advance;
		uint64_t src_index = bt_field_path_get_index_by_index_inline(
			src_field_path, src_i);
		uint64_t tgt_index = bt_field_path_get_index_by_index_inline(
			tgt_field_path, tgt_i);

		if (src_i != tgt_i) {
			/* Next field class is different: LCA is `tgt_fc` */
			break;
		}

		src_fc = borrow_child_field_class(src_fc, src_index, &advance);

		if (advance) {
			src_i++;
		}

		tgt_fc = borrow_child_field_class(tgt_fc, tgt_index, &advance);

		if (advance) {
			tgt_i++;
		}
	}

	/* Only structure field classes to the target */
	while (tgt_i < tgt_field_path->indexes->len) {
		bool advance;
		uint64_t tgt_index = bt_field_path_get_index_by_index_inline(
			tgt_field_path, tgt_i);

		if (tgt_fc->type == BT_FIELD_CLASS_TYPE_STATIC_ARRAY ||
				tgt_fc->type == BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY ||
				tgt_fc->type == BT_FIELD_CLASS_TYPE_VARIANT) {
			is_valid = false;
			goto end;
		}

		tgt_fc = borrow_child_field_class(tgt_fc, tgt_index, &advance);

		if (advance) {
			tgt_i++;
		}
	}

end:
	return is_valid;
}

BT_ASSERT_PRE_FUNC
static inline
bool field_path_is_valid(struct bt_field_class *src_fc,
		struct bt_field_class *tgt_fc,
		struct bt_resolve_field_path_context *ctx)
{
	bool is_valid = true;
	struct bt_field_path *src_field_path = find_field_class_in_ctx(
		src_fc, ctx);
	struct bt_field_path *tgt_field_path = find_field_class_in_ctx(
		tgt_fc, ctx);

	if (!src_field_path) {
		BT_ASSERT_PRE_MSG("Cannot find requesting field class in "
			"resolving context: %!+F", src_fc);
		is_valid = false;
		goto end;
	}

	if (!tgt_field_path) {
		BT_ASSERT_PRE_MSG("Cannot find target field class in "
			"resolving context: %!+F", tgt_fc);
		is_valid = false;
		goto end;
	}

	/* Target must be before source */
	if (!target_is_before_source(src_field_path, tgt_field_path)) {
		BT_ASSERT_PRE_MSG("Target field class is located after "
			"requesting field class: %![req-fc-]+F, %![tgt-fc-]+F",
			src_fc, tgt_fc);
		is_valid = false;
		goto end;
	}

	/*
	 * If target is in a different scope than source, there are no
	 * array or variant field classes on the way to the target.
	 */
	if (!target_field_path_in_different_scope_has_struct_fc_only(
			src_field_path, tgt_field_path, ctx)) {
		BT_ASSERT_PRE_MSG("Target field class is located in a "
			"different scope than requesting field class, "
			"but within an array or a variant field class: "
			"%![req-fc-]+F, %![tgt-fc-]+F",
			src_fc, tgt_fc);
		is_valid = false;
		goto end;
	}

	/* Same scope: LCA must be a structure field class */
	if (!lca_is_structure_field_class(src_field_path, tgt_field_path, ctx)) {
		BT_ASSERT_PRE_MSG("Lowest common ancestor of target and "
			"requesting field classes is not a structure field class: "
			"%![req-fc-]+F, %![tgt-fc-]+F",
			src_fc, tgt_fc);
		is_valid = false;
		goto end;
	}

	/* Same scope: path from LCA to target has no array/variant FTs */
	if (!lca_to_target_has_struct_fc_only(src_field_path, tgt_field_path,
			ctx)) {
		BT_ASSERT_PRE_MSG("Path from lowest common ancestor of target "
			"and requesting field classes to target field class "
			"contains an array or a variant field class: "
			"%![req-fc-]+F, %![tgt-fc-]+F", src_fc, tgt_fc);
		is_valid = false;
		goto end;
	}

end:
	bt_object_put_ref(src_field_path);
	bt_object_put_ref(tgt_field_path);
	return is_valid;
}

static
struct bt_field_path *resolve_field_path(struct bt_field_class *src_fc,
		struct bt_field_class *tgt_fc,
		struct bt_resolve_field_path_context *ctx)
{
	BT_ASSERT_PRE(field_path_is_valid(src_fc, tgt_fc, ctx),
		"Invalid target field class: %![req-fc-]+F, %![tgt-fc-]+F",
		src_fc, tgt_fc);
	return find_field_class_in_ctx(tgt_fc, ctx);
}

BT_HIDDEN
int bt_resolve_field_paths(struct bt_field_class *fc,
		struct bt_resolve_field_path_context *ctx)
{
	int ret = 0;

	BT_ASSERT(fc);

	/* Resolving part for dynamic array and variant field classes */
	switch (fc->type) {
	case BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY:
	{
		struct bt_field_class_dynamic_array *dyn_array_fc = (void *) fc;

		if (dyn_array_fc->length_fc) {
			BT_ASSERT(!dyn_array_fc->length_field_path);
			dyn_array_fc->length_field_path = resolve_field_path(
				fc, dyn_array_fc->length_fc, ctx);
			if (!dyn_array_fc->length_field_path) {
				ret = -1;
				goto end;
			}
		}

		break;
	}
	case BT_FIELD_CLASS_TYPE_VARIANT:
	{
		struct bt_field_class_variant *var_fc = (void *) fc;

		if (var_fc->selector_fc) {
			BT_ASSERT(!var_fc->selector_field_path);
			var_fc->selector_field_path =
				resolve_field_path(fc,
					var_fc->selector_fc, ctx);
			if (!var_fc->selector_field_path) {
				ret = -1;
				goto end;
			}
		}
	}
	default:
		break;
	}

	/* Recursive part */
	switch (fc->type) {
	case BT_FIELD_CLASS_TYPE_STRUCTURE:
	case BT_FIELD_CLASS_TYPE_VARIANT:
	{
		struct bt_field_class_named_field_class_container *container_fc =
			(void *) fc;
		uint64_t i;

		for (i = 0; i < container_fc->named_fcs->len; i++) {
			struct bt_named_field_class *named_fc =
				BT_FIELD_CLASS_NAMED_FC_AT_INDEX(
					container_fc, i);

			ret = bt_resolve_field_paths(named_fc->fc, ctx);
			if (ret) {
				goto end;
			}
		}

		break;
	}
	case BT_FIELD_CLASS_TYPE_STATIC_ARRAY:
	case BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY:
	{
		struct bt_field_class_array *array_fc = (void *) fc;

		ret = bt_resolve_field_paths(array_fc->element_fc, ctx);
		break;
	}
	default:
		break;
	}

end:
	return ret;
}
