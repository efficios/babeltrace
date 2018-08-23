/*
 * Copyright 2016-2018 - Philippe Proulx <pproulx@efficios.com>
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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
 */

#define BT_LOG_TAG "PLUGIN-CTF-METADATA-META-RESOLVE"
#include "logging.h"

#include <babeltrace/babeltrace.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/common-internal.h>
#include <glib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <glib.h>

#include "ctf-meta-visitors.h"

typedef GPtrArray field_type_stack;

/*
 * A stack frame.
 *
 * `type` contains a compound field type (structure, variant, array,
 * or sequence) and `index` indicates the index of the field type in
 * the upper frame (-1 for array and sequence field types). `name`
 * indicates the name of the field type in the upper frame (empty
 * string for array and sequence field types).
 */
struct field_type_stack_frame {
	struct ctf_field_type *ft;
	int64_t index;
};

/*
 * The current context of the resolving engine.
 */
struct resolve_context {
	struct ctf_trace_class *tc;
	struct ctf_stream_class *sc;
	struct ctf_event_class *ec;

	struct {
		struct ctf_field_type *packet_header;
		struct ctf_field_type *packet_context;
		struct ctf_field_type *event_header;
		struct ctf_field_type *event_common_context;
		struct ctf_field_type *event_spec_context;
		struct ctf_field_type *event_payload;
	} scopes;

	/* Root scope being visited */
	enum bt_scope root_scope;
	field_type_stack *field_type_stack;
	struct ctf_field_type *cur_ft;
};

/* TSDL dynamic scope prefixes as defined in CTF Section 7.3.2 */
static const char * const absolute_path_prefixes[] = {
	[BT_SCOPE_PACKET_HEADER]		= "trace.packet.header.",
	[BT_SCOPE_PACKET_CONTEXT]		= "stream.packet.context.",
	[BT_SCOPE_EVENT_HEADER]			= "stream.event.header.",
	[BT_SCOPE_EVENT_COMMON_CONTEXT]		= "stream.event.context.",
	[BT_SCOPE_EVENT_SPECIFIC_CONTEXT]	= "event.context.",
	[BT_SCOPE_EVENT_PAYLOAD]		= "event.fields.",
};

/* Number of path tokens used for the absolute prefixes */
static const uint64_t absolute_path_prefix_ptoken_counts[] = {
	[BT_SCOPE_PACKET_HEADER]		= 3,
	[BT_SCOPE_PACKET_CONTEXT]		= 3,
	[BT_SCOPE_EVENT_HEADER]			= 3,
	[BT_SCOPE_EVENT_COMMON_CONTEXT]		= 3,
	[BT_SCOPE_EVENT_SPECIFIC_CONTEXT]	= 2,
	[BT_SCOPE_EVENT_PAYLOAD]		= 2,
};

static
void destroy_field_type_stack_frame(struct field_type_stack_frame *frame)
{
	if (!frame) {
		return;
	}

	g_free(frame);
}

/*
 * Creates a type stack.
 */
static
field_type_stack *field_type_stack_create(void)
{
	return g_ptr_array_new_with_free_func(
		(GDestroyNotify) destroy_field_type_stack_frame);
}

/*
 * Destroys a type stack.
 */
static
void field_type_stack_destroy(field_type_stack *stack)
{
	if (stack) {
		g_ptr_array_free(stack, TRUE);
	}
}

/*
 * Pushes a field type onto a type stack.
 */
static
int field_type_stack_push(field_type_stack *stack, struct ctf_field_type *ft)
{
	int ret = 0;
	struct field_type_stack_frame *frame = NULL;

	if (!stack || !ft) {
		BT_LOGE("Invalid parameter: stack or type is NULL.");
		ret = -1;
		goto end;
	}

	frame = g_new0(struct field_type_stack_frame, 1);
	if (!frame) {
		BT_LOGE_STR("Failed to allocate one field type stack frame.");
		ret = -1;
		goto end;
	}

	BT_LOGV("Pushing field type on context's stack: "
		"ft-addr=%p, stack-size-before=%u", ft, stack->len);
	frame->ft = ft;
	g_ptr_array_add(stack, frame);

end:
	return ret;
}

/*
 * Checks whether or not `stack` is empty.
 */
static
bool field_type_stack_empty(field_type_stack *stack)
{
	return stack->len == 0;
}

/*
 * Returns the number of frames in `stack`.
 */
static
size_t field_type_stack_size(field_type_stack *stack)
{
	return stack->len;
}

/*
 * Returns the top frame of `stack`.
 */
static
struct field_type_stack_frame *field_type_stack_peek(field_type_stack *stack)
{
	struct field_type_stack_frame *entry = NULL;

	if (!stack || field_type_stack_empty(stack)) {
		goto end;
	}

	entry = g_ptr_array_index(stack, stack->len - 1);
end:
	return entry;
}

/*
 * Returns the frame at index `index` in `stack`.
 */
static
struct field_type_stack_frame *field_type_stack_at(field_type_stack *stack,
		size_t index)
{
	struct field_type_stack_frame *entry = NULL;

	if (!stack || index >= stack->len) {
		goto end;
	}

	entry = g_ptr_array_index(stack, index);

end:
	return entry;
}

/*
 * Removes the top frame of `stack`.
 */
static
void field_type_stack_pop(field_type_stack *stack)
{
	if (!field_type_stack_empty(stack)) {
		/*
		 * This will call the frame's destructor and free it, as
		 * well as put its contained field type.
		 */
		BT_LOGV("Popping context's stack: stack-size-before=%u",
			stack->len);
		g_ptr_array_set_size(stack, stack->len - 1);
	}
}

/*
 * Returns the scope field type of `scope` in the context `ctx`.
 */
static
struct ctf_field_type *borrow_type_from_ctx(struct resolve_context *ctx,
		enum bt_scope scope)
{
	switch (scope) {
	case BT_SCOPE_PACKET_HEADER:
		return ctx->scopes.packet_header;
	case BT_SCOPE_PACKET_CONTEXT:
		return ctx->scopes.packet_context;
	case BT_SCOPE_EVENT_HEADER:
		return ctx->scopes.event_header;
	case BT_SCOPE_EVENT_COMMON_CONTEXT:
		return ctx->scopes.event_common_context;
	case BT_SCOPE_EVENT_SPECIFIC_CONTEXT:
		return ctx->scopes.event_spec_context;
	case BT_SCOPE_EVENT_PAYLOAD:
		return ctx->scopes.event_payload;
	default:
		abort();
	}

	return NULL;
}

/*
 * Returns the CTF scope from a path string. May return -1 if the path
 * is found to be relative.
 */
static
enum bt_scope get_root_scope_from_absolute_pathstr(const char *pathstr)
{
	enum bt_scope scope;
	enum bt_scope ret = -1;
	const size_t prefixes_count = sizeof(absolute_path_prefixes) /
		sizeof(*absolute_path_prefixes);

	for (scope = BT_SCOPE_PACKET_HEADER; scope < BT_SCOPE_PACKET_HEADER +
			prefixes_count; scope++) {
		/*
		 * Chech if path string starts with a known absolute
		 * path prefix.
		 *
		 * Refer to CTF 7.3.2 STATIC AND DYNAMIC SCOPES.
		 */
		if (strncmp(pathstr, absolute_path_prefixes[scope],
				strlen(absolute_path_prefixes[scope]))) {
			/* Prefix does not match: try the next one */
			BT_LOGV("Prefix does not match: trying the next one: "
				"path=\"%s\", path-prefix=\"%s\", scope=%s",
				pathstr, absolute_path_prefixes[scope],
				bt_common_scope_string(scope));
			continue;
		}

		/* Found it! */
		ret = scope;
		BT_LOGV("Found root scope from absolute path: "
			"path=\"%s\", scope=%s", pathstr,
			bt_common_scope_string(scope));
		goto end;
	}

end:
	return ret;
}

/*
 * Destroys a path token.
 */
static
void ptokens_destroy_func(gpointer ptoken, gpointer data)
{
	g_string_free(ptoken, TRUE);
}

/*
 * Destroys a path token list.
 */
static
void ptokens_destroy(GList *ptokens)
{
	if (!ptokens) {
		return;
	}

	g_list_foreach(ptokens, ptokens_destroy_func, NULL);
	g_list_free(ptokens);
}

/*
 * Returns the string contained in a path token.
 */
static
const char *ptoken_get_string(GList *ptoken)
{
	GString *tokenstr = (GString *) ptoken->data;

	return tokenstr->str;
}

/*
 * Converts a path string to a path token list, that is, splits the
 * individual words of a path string into a list of individual
 * strings.
 */
static
GList *pathstr_to_ptokens(const char *pathstr)
{
	const char *at = pathstr;
	const char *last = at;
	GList *ptokens = NULL;

	for (;;) {
		if (*at == '.' || *at == '\0') {
			GString *tokenstr;

			if (at == last) {
				/* Error: empty token */
				BT_LOGE("Empty path token: path=\"%s\", pos=%u",
					pathstr, (unsigned int) (at - pathstr));
				goto error;
			}

			tokenstr = g_string_new(NULL);
			g_string_append_len(tokenstr, last, at - last);
			ptokens = g_list_append(ptokens, tokenstr);
			last = at + 1;
		}

		if (*at == '\0') {
			break;
		}

		at++;
	}

	return ptokens;

error:
	ptokens_destroy(ptokens);
	return NULL;
}

/*
 * Converts a path token list to a field path object. The path token
 * list is relative from `ft`. The index of the source looking for its
 * target within `ft` is indicated by `src_index`. This can be `INT64_MAX`
 * if the source is contained in `ft`.
 *
 * `field_path` is an output parameter owned by the caller that must be
 * filled here.
 */
static
int ptokens_to_field_path(GList *ptokens, struct ctf_field_path *field_path,
		struct ctf_field_type *ft, int64_t src_index)
{
	int ret = 0;
	GList *cur_ptoken = ptokens;
	bool first_level_done = false;

	/* Locate target */
	while (cur_ptoken) {
		int64_t child_index;
		struct ctf_field_type *child_ft;
		const char *ft_name = ptoken_get_string(cur_ptoken);

		BT_LOGV("Current path token: token=\"%s\"", ft_name);

		/* Find to which index corresponds the current path token */
		if (ft->id == CTF_FIELD_TYPE_ID_ARRAY ||
				ft->id == CTF_FIELD_TYPE_ID_SEQUENCE) {
			child_index = -1;
		} else {
			child_index =
				ctf_field_type_compound_get_field_type_index_from_name(
					ft, ft_name);
			if (child_index < 0) {
				/*
				 * Error: field name does not exist or
				 * wrong current type.
				 */
				BT_LOGV("Cannot get index of field type: "
					"field-name=\"%s\", "
					"src-index=%" PRId64 ", "
					"child-index=%" PRId64 ", "
					"first-level-done=%d",
					ft_name, src_index, child_index,
					first_level_done);
				ret = -1;
				goto end;
			} else if (child_index > src_index &&
					!first_level_done) {
				BT_LOGV("Child field type is located after source field type: "
					"field-name=\"%s\", "
					"src-index=%" PRId64 ", "
					"child-index=%" PRId64 ", "
					"first-level-done=%d",
					ft_name, src_index, child_index,
					first_level_done);
				ret = -1;
				goto end;
			}

			/* Next path token */
			cur_ptoken = g_list_next(cur_ptoken);
			first_level_done = true;
		}

		/* Create new field path entry */
		ctf_field_path_append_index(field_path, child_index);

		/* Get child field type */
		child_ft = ctf_field_type_compound_borrow_field_type_by_index(
			ft, child_index);
		BT_ASSERT(child_ft);

		/* Move child type to current type */
		ft = child_ft;
	}

end:
	return ret;
}

/*
 * Converts a known absolute path token list to a field path object
 * within the resolving context `ctx`.
 *
 * `field_path` is an output parameter owned by the caller that must be
 * filled here.
 */
static
int absolute_ptokens_to_field_path(GList *ptokens,
		struct ctf_field_path *field_path,
		struct resolve_context *ctx)
{
	int ret = 0;
	GList *cur_ptoken;
	struct ctf_field_type *ft;

	/*
	 * Make sure we're not referring to a scope within a translated
	 * object.
	 */
	switch (field_path->root) {
	case BT_SCOPE_PACKET_HEADER:
		if (ctx->tc->is_translated) {
			BT_LOGE("Trace class is already translated: "
				"root-scope=%s",
				bt_common_scope_string(field_path->root));
			ret = -1;
			goto end;
		}

		break;
	case BT_SCOPE_PACKET_CONTEXT:
	case BT_SCOPE_EVENT_HEADER:
	case BT_SCOPE_EVENT_COMMON_CONTEXT:
		if (!ctx->sc) {
			BT_LOGE("No current stream class: "
				"root-scope=%s",
				bt_common_scope_string(field_path->root));
			ret = -1;
			goto end;
		}

		if (ctx->sc->is_translated) {
			BT_LOGE("Stream class is already translated: "
				"root-scope=%s",
				bt_common_scope_string(field_path->root));
			ret = -1;
			goto end;
		}

		break;
	case BT_SCOPE_EVENT_SPECIFIC_CONTEXT:
	case BT_SCOPE_EVENT_PAYLOAD:
		if (!ctx->ec) {
			BT_LOGE("No current event class: "
				"root-scope=%s",
				bt_common_scope_string(field_path->root));
			ret = -1;
			goto end;
		}

		if (ctx->ec->is_translated) {
			BT_LOGE("Event class is already translated: "
				"root-scope=%s",
				bt_common_scope_string(field_path->root));
			ret = -1;
			goto end;
		}

		break;

	default:
		abort();
	}

	/* Skip absolute path tokens */
	cur_ptoken = g_list_nth(ptokens,
		absolute_path_prefix_ptoken_counts[field_path->root]);

	/* Start with root type */
	ft = borrow_type_from_ctx(ctx, field_path->root);
	if (!ft) {
		/* Error: root type is not available */
		BT_LOGE("Root field type is not available: "
			"root-scope=%s",
			bt_common_scope_string(field_path->root));
		ret = -1;
		goto end;
	}

	/* Locate target */
	ret = ptokens_to_field_path(cur_ptoken, field_path, ft, INT64_MAX);

end:
	return ret;
}

/*
 * Converts a known relative path token list to a field path object
 * within the resolving context `ctx`.
 *
 * `field_path` is an output parameter owned by the caller that must be
 * filled here.
 */
static
int relative_ptokens_to_field_path(GList *ptokens,
		struct ctf_field_path *field_path, struct resolve_context *ctx)
{
	int ret = 0;
	int64_t parent_pos_in_stack;
	struct ctf_field_path tail_field_path;

	ctf_field_path_init(&tail_field_path);
	parent_pos_in_stack = field_type_stack_size(ctx->field_type_stack) - 1;

	while (parent_pos_in_stack >= 0) {
		struct ctf_field_type *parent_type =
			field_type_stack_at(ctx->field_type_stack,
				parent_pos_in_stack)->ft;
		int64_t cur_index = field_type_stack_at(ctx->field_type_stack,
			parent_pos_in_stack)->index;

		BT_LOGV("Locating target field type from current parent field type: "
			"parent-pos=%" PRId64 ", parent-ft-addr=%p, "
			"cur-index=%" PRId64,
			parent_pos_in_stack, parent_type, cur_index);

		/* Locate target from current parent type */
		ret = ptokens_to_field_path(ptokens, &tail_field_path,
			parent_type, cur_index);
		if (ret) {
			/* Not found... yet */
			BT_LOGV_STR("Not found at this point.");
			ctf_field_path_clear(&tail_field_path);
		} else {
			/* Found: stitch tail field path to head field path */
			uint64_t i = 0;
			size_t tail_field_path_len =
				tail_field_path.path->len;

			while (BT_TRUE) {
				struct ctf_field_type *cur_type =
					field_type_stack_at(ctx->field_type_stack,
						i)->ft;
				int64_t index = field_type_stack_at(
					ctx->field_type_stack, i)->index;

				if (cur_type == parent_type) {
					break;
				}

				ctf_field_path_append_index(field_path,
					index);
				i++;
			}

			for (i = 0; i < tail_field_path_len; i++) {
				int64_t index =
					ctf_field_path_borrow_index_by_index(
						&tail_field_path, i);

				ctf_field_path_append_index(field_path,
					(int64_t) index);
			}
			break;
		}

		parent_pos_in_stack--;
	}

	if (parent_pos_in_stack < 0) {
		/* Not found */
		ret = -1;
	}

	ctf_field_path_fini(&tail_field_path);
	return ret;
}

/*
 * Converts a path string to a field path object within the resolving
 * context `ctx`.
 */
static
int pathstr_to_field_path(const char *pathstr,
		struct ctf_field_path *field_path, struct resolve_context *ctx)
{
	int ret = 0;
	enum bt_scope root_scope;
	GList *ptokens = NULL;

	/* Convert path string to path tokens */
	ptokens = pathstr_to_ptokens(pathstr);
	if (!ptokens) {
		BT_LOGE("Cannot convert path string to path tokens: "
			"path=\"%s\"", pathstr);
		ret = -1;
		goto end;
	}

	/* Absolute or relative path? */
	root_scope = get_root_scope_from_absolute_pathstr(pathstr);

	if (root_scope == -1) {
		/* Relative path: start with current root scope */
		field_path->root = ctx->root_scope;
		BT_LOGV("Detected relative path: starting with current root scope: "
			"scope=%s", bt_common_scope_string(field_path->root));
		ret = relative_ptokens_to_field_path(ptokens, field_path, ctx);
		if (ret) {
			BT_LOGE("Cannot get relative field path of path string: "
				"path=\"%s\", start-scope=%s, end-scope=%s",
				pathstr, bt_common_scope_string(ctx->root_scope),
				bt_common_scope_string(field_path->root));
			goto end;
		}
	} else {
		/* Absolute path: use found root scope */
		field_path->root = root_scope;
		BT_LOGV("Detected absolute path: using root scope: "
			"scope=%s", bt_common_scope_string(field_path->root));
		ret = absolute_ptokens_to_field_path(ptokens, field_path, ctx);
		if (ret) {
			BT_LOGE("Cannot get absolute field path of path string: "
				"path=\"%s\", root-scope=%s",
				pathstr, bt_common_scope_string(root_scope));
			goto end;
		}
	}

	if (BT_LOG_ON_VERBOSE && ret == 0) {
		GString *field_path_pretty = ctf_field_path_string(field_path);
		const char *field_path_pretty_str =
			field_path_pretty ? field_path_pretty->str : NULL;

		BT_LOGV("Found field path: path=\"%s\", field-path=\"%s\"",
			pathstr, field_path_pretty_str);

		if (field_path_pretty) {
			g_string_free(field_path_pretty, TRUE);
		}
	}

end:
	ptokens_destroy(ptokens);
	return ret;
}

/*
 * Retrieves a field type by following the field path `field_path` in
 * the resolving context `ctx`.
 */
static
struct ctf_field_type *field_path_to_field_type(
		struct ctf_field_path *field_path, struct resolve_context *ctx)
{
	uint64_t i;
	struct ctf_field_type *ft;

	/* Start with root type */
	ft = borrow_type_from_ctx(ctx, field_path->root);
	if (!ft) {
		/* Error: root type is not available */
		BT_LOGE("Root field type is not available: root-scope=%s",
			bt_common_scope_string(field_path->root));
		goto end;
	}

	/* Locate target */
	for (i = 0; i < field_path->path->len; i++) {
		struct ctf_field_type *child_ft;
		int64_t child_index =
			ctf_field_path_borrow_index_by_index(field_path, i);

		/* Get child field type */
		child_ft = ctf_field_type_compound_borrow_field_type_by_index(
			ft, child_index);
		BT_ASSERT(child_ft);

		/* Move child type to current type */
		ft = child_ft;
	}

end:
	return ft;
}

/*
 * Fills the equivalent field path object of the context type stack.
 */
static
void get_ctx_stack_field_path(struct resolve_context *ctx,
		struct ctf_field_path *field_path)
{
	uint64_t i;

	BT_ASSERT(field_path);
	field_path->root = ctx->root_scope;
	ctf_field_path_clear(field_path);

	for (i = 0; i < field_type_stack_size(ctx->field_type_stack); i++) {
		struct field_type_stack_frame *frame =
			field_type_stack_at(ctx->field_type_stack, i);

		ctf_field_path_append_index(field_path, frame->index);
	}
}

/*
 * Returns the index of the lowest common ancestor of two field path
 * objects having the same root scope.
 */
int64_t get_field_paths_lca_index(struct ctf_field_path *field_path1,
		struct ctf_field_path *field_path2)
{
	int64_t lca_index = 0;
	uint64_t field_path1_len, field_path2_len;

	if (BT_LOG_ON_VERBOSE) {
		GString *field_path1_pretty =
			ctf_field_path_string(field_path1);
		GString *field_path2_pretty =
			ctf_field_path_string(field_path2);
		const char *field_path1_pretty_str =
			field_path1_pretty ? field_path1_pretty->str : NULL;
		const char *field_path2_pretty_str =
			field_path2_pretty ? field_path2_pretty->str : NULL;

		BT_LOGV("Finding lowest common ancestor (LCA) between two field paths: "
			"field-path-1=\"%s\", field-path-2=\"%s\"",
			field_path1_pretty_str, field_path2_pretty_str);

		if (field_path1_pretty) {
			g_string_free(field_path1_pretty, TRUE);
		}

		if (field_path2_pretty) {
			g_string_free(field_path2_pretty, TRUE);
		}
	}

	/*
	 * Start from both roots and find the first mismatch.
	 */
	BT_ASSERT(field_path1->root == field_path2->root);
	field_path1_len = field_path1->path->len;
	field_path2_len = field_path2->path->len;

	while (true) {
		int64_t target_index, ctx_index;

		if (lca_index == (int64_t) field_path2_len ||
				lca_index == (int64_t) field_path1_len) {
			/*
			 * This means that both field paths never split.
			 * This is invalid because the target cannot be
			 * an ancestor of the source.
			 */
			BT_LOGE("Source field type is an ancestor of target field type or vice versa: "
				"lca-index=%" PRId64 ", "
				"field-path-1-len=%" PRIu64 ", "
				"field-path-2-len=%" PRIu64,
				lca_index, field_path1_len, field_path2_len);
			lca_index = -1;
			break;
		}

		target_index = ctf_field_path_borrow_index_by_index(field_path1,
			lca_index);
		ctx_index = ctf_field_path_borrow_index_by_index(field_path2,
			lca_index);

		if (target_index != ctx_index) {
			/* LCA index is the previous */
			break;
		}

		lca_index++;
	}

	BT_LOGV("Found LCA: lca-index=%" PRId64, lca_index);
	return lca_index;
}

/*
 * Validates a target field path.
 */
static
int validate_target_field_path(struct ctf_field_path *target_field_path,
		struct ctf_field_type *target_ft,
		struct resolve_context *ctx)
{
	int ret = 0;
	struct ctf_field_path ctx_field_path;
	uint64_t target_field_path_len = target_field_path->path->len;
	int64_t lca_index;

	/* Get context field path */
	ctf_field_path_init(&ctx_field_path);
	get_ctx_stack_field_path(ctx, &ctx_field_path);

	/*
	 * Make sure the target is not a root.
	 */
	if (target_field_path_len == 0) {
		BT_LOGE_STR("Target field path's length is 0 (targeting the root).");
		ret = -1;
		goto end;
	}

	/*
	 * Make sure the root of the target field path is not located
	 * after the context field path's root.
	 */
	if (target_field_path->root > ctx_field_path.root) {
		BT_LOGE("Target field type is located after source field type: "
			"target-root=%s, source-root=%s",
			bt_common_scope_string(target_field_path->root),
			bt_common_scope_string(ctx_field_path.root));
		ret = -1;
		goto end;
	}

	if (target_field_path->root == ctx_field_path.root) {
		int64_t target_index, ctx_index;

		/*
		 * Find the index of the lowest common ancestor of both field
		 * paths.
		 */
		lca_index = get_field_paths_lca_index(target_field_path,
			&ctx_field_path);
		if (lca_index < 0) {
			BT_LOGE_STR("Cannot get least common ancestor.");
			ret = -1;
			goto end;
		}

		/*
		 * Make sure the target field path is located before the
		 * context field path.
		 */
		target_index = ctf_field_path_borrow_index_by_index(
			target_field_path, (uint64_t) lca_index);
		ctx_index = ctf_field_path_borrow_index_by_index(
			&ctx_field_path, (uint64_t) lca_index);

		if (target_index >= ctx_index) {
			BT_LOGE("Target field type's index is greater than or equal to source field type's index in LCA: "
				"lca-index=%" PRId64 ", "
				"target-index=%" PRId64 ", "
				"source-index=%" PRId64,
				lca_index, target_index, ctx_index);
			ret = -1;
			goto end;
		}
	}

	/*
	 * Make sure the target type has the right type and properties.
	 */
	switch (ctx->cur_ft->id) {
	case CTF_FIELD_TYPE_ID_VARIANT:
		if (target_ft->id != CTF_FIELD_TYPE_ID_ENUM) {
			BT_LOGE("Variant field type's tag field type is not an enumeration field type: "
				"tag-ft-addr=%p, tag-ft-id=%d",
				target_ft, target_ft->id);
			ret = -1;
			goto end;
		}
		break;
	case CTF_FIELD_TYPE_ID_SEQUENCE:
	{
		struct ctf_field_type_int *int_ft = (void *) target_ft;

		if (target_ft->id != CTF_FIELD_TYPE_ID_INT &&
				target_ft->id != CTF_FIELD_TYPE_ID_ENUM) {
			BT_LOGE("Sequence field type's length field type is not an unsigned integer field type: "
				"length-ft-addr=%p, length-ft-id=%d",
				target_ft, target_ft->id);
			ret = -1;
			goto end;
		}

		if (int_ft->is_signed) {
			BT_LOGE("Sequence field type's length field type is not an unsigned integer field type: "
				"length-ft-addr=%p, length-ft-id=%d",
				target_ft, target_ft->id);
			ret = -1;
			goto end;
		}
		break;
	}
	default:
		abort();
	}

end:
	ctf_field_path_fini(&ctx_field_path);
	return ret;
}

/*
 * Resolves a variant or sequence field type `ft`.
 */
static
int resolve_sequence_or_variant_field_type(struct ctf_field_type *ft,
		struct resolve_context *ctx)
{
	int ret = 0;
	const char *pathstr;
	struct ctf_field_path target_field_path;
	struct ctf_field_type *target_ft = NULL;
	GString *target_field_path_pretty = NULL;
	const char *target_field_path_pretty_str;

	ctf_field_path_init(&target_field_path);

	/* Get path string */
	switch (ft->id) {
	case CTF_FIELD_TYPE_ID_SEQUENCE:
	{
		struct ctf_field_type_sequence *seq_ft = (void *) ft;
		pathstr = seq_ft->length_ref->str;
		break;
	}
	case CTF_FIELD_TYPE_ID_VARIANT:
	{
		struct ctf_field_type_variant *var_ft = (void *) ft;
		pathstr = var_ft->tag_ref->str;
		break;
	}
	default:
		abort();
	}

	if (!pathstr) {
		BT_LOGE_STR("Cannot get path string.");
		ret = -1;
		goto end;
	}

	/* Get target field path out of path string */
	ret = pathstr_to_field_path(pathstr, &target_field_path, ctx);
	if (ret) {
		BT_LOGE("Cannot get target field path for path string: "
			"path=\"%s\"", pathstr);
		goto end;
	}

	target_field_path_pretty = ctf_field_path_string(
		&target_field_path);
	target_field_path_pretty_str =
		target_field_path_pretty ? target_field_path_pretty->str : NULL;

	/* Get target field type */
	target_ft = field_path_to_field_type(&target_field_path, ctx);
	if (!target_ft) {
		BT_LOGE("Cannot get target field type for path string: "
			"path=\"%s\", target-field-path=\"%s\"",
			pathstr, target_field_path_pretty_str);
		ret = -1;
		goto end;
	}

	ret = validate_target_field_path(&target_field_path,
		target_ft, ctx);
	if (ret) {
		BT_LOGE("Invalid target field path for path string: "
			"path=\"%s\", target-field-path=\"%s\"",
			pathstr, target_field_path_pretty_str);
		goto end;
	}

	/* Set target field path and target field type */
	switch (ft->id) {
	case CTF_FIELD_TYPE_ID_SEQUENCE:
	{
		struct ctf_field_type_sequence *seq_ft = (void *) ft;

		ctf_field_path_copy_content(&seq_ft->length_path,
			&target_field_path);
		seq_ft->length_ft = (void *) target_ft;
		break;
	}
	case CTF_FIELD_TYPE_ID_VARIANT:
	{
		struct ctf_field_type_variant *var_ft = (void *) ft;

		ctf_field_path_copy_content(&var_ft->tag_path,
			&target_field_path);
		ctf_field_type_variant_set_tag_field_type(var_ft,
			(void *) target_ft);
		break;
	}
	default:
		abort();
	}

end:
	if (target_field_path_pretty) {
		g_string_free(target_field_path_pretty, TRUE);
	}

	ctf_field_path_fini(&target_field_path);
	return ret;
}

/*
 * Resolves a field type `ft`.
 */
static
int resolve_field_type(struct ctf_field_type *ft, struct resolve_context *ctx)
{
	int ret = 0;

	if (!ft) {
		/* Type is not available; still valid */
		goto end;
	}

	ctx->cur_ft = ft;

	/* Resolve sequence/variant field type */
	switch (ft->id) {
	case CTF_FIELD_TYPE_ID_SEQUENCE:
	case CTF_FIELD_TYPE_ID_VARIANT:
		ret = resolve_sequence_or_variant_field_type(ft, ctx);
		if (ret) {
			BT_LOGE("Cannot resolve sequence field type's length or variant field type's tag: "
				"ret=%d, ft-addr=%p", ret, ft);
			goto end;
		}

		break;
	default:
		break;
	}

	/* Recurse into compound types */
	switch (ft->id) {
	case CTF_FIELD_TYPE_ID_STRUCT:
	case CTF_FIELD_TYPE_ID_VARIANT:
	case CTF_FIELD_TYPE_ID_SEQUENCE:
	case CTF_FIELD_TYPE_ID_ARRAY:
	{
		uint64_t i;
		uint64_t field_count =
			ctf_field_type_compound_get_field_type_count(ft);

		ret = field_type_stack_push(ctx->field_type_stack, ft);
		if (ret) {
			BT_LOGE("Cannot push field type on context's stack: "
				"ft-addr=%p", ft);
			goto end;
		}

		for (i = 0; i < field_count; i++) {
			struct ctf_field_type *child_ft =
				ctf_field_type_compound_borrow_field_type_by_index(
					ft, i);

			BT_ASSERT(child_ft);

			if (ft->id == CTF_FIELD_TYPE_ID_ARRAY||
					ft->id == CTF_FIELD_TYPE_ID_SEQUENCE) {
				field_type_stack_peek(
					ctx->field_type_stack)->index = -1;
			} else {
				field_type_stack_peek(
					ctx->field_type_stack)->index =
						(int64_t) i;
			}

			BT_LOGV("Resolving field type's child field type: "
				"parent-ft-addr=%p, child-ft-addr=%p, "
				"index=%" PRIu64 ", count=%" PRIu64,
				ft, child_ft, i, field_count);
			ret = resolve_field_type(child_ft, ctx);
			if (ret) {
				goto end;
			}
		}

		field_type_stack_pop(ctx->field_type_stack);
		break;
	}
	default:
		break;
	}

end:
	return ret;
}

/*
 * Resolves the root field type corresponding to the scope `root_scope`.
 */
static
int resolve_root_type(enum bt_scope root_scope, struct resolve_context *ctx)
{
	int ret;

	BT_ASSERT(field_type_stack_size(ctx->field_type_stack) == 0);
	ctx->root_scope = root_scope;
	ret = resolve_field_type(borrow_type_from_ctx(ctx, root_scope), ctx);
	ctx->root_scope = -1;
	return ret;
}

static
int resolve_event_class_field_types(struct resolve_context *ctx,
		struct ctf_event_class *ec)
{
	int ret = 0;

	BT_ASSERT(!ctx->scopes.event_spec_context);
	BT_ASSERT(!ctx->scopes.event_payload);

	if (ec->is_translated) {
		goto end;
	}

	ctx->ec = ec;
	ctx->scopes.event_spec_context = ec->spec_context_ft;
	ret = resolve_root_type(BT_SCOPE_EVENT_COMMON_CONTEXT, ctx);
	if (ret) {
		BT_LOGE("Cannot resolve event specific context field type: "
			"ret=%d", ret);
		goto end;
	}

	ctx->scopes.event_payload = ec->payload_ft;
	ret = resolve_root_type(BT_SCOPE_EVENT_PAYLOAD, ctx);
	if (ret) {
		BT_LOGE("Cannot resolve event payload field type: "
			"ret=%d", ret);
		goto end;
	}

end:
	ctx->scopes.event_spec_context = NULL;
	ctx->scopes.event_payload = NULL;
	ctx->ec = NULL;
	return ret;
}

static
int resolve_stream_class_field_types(struct resolve_context *ctx,
		struct ctf_stream_class *sc)
{
	int ret = 0;
	uint64_t i;

	BT_ASSERT(!ctx->scopes.packet_context);
	BT_ASSERT(!ctx->scopes.event_header);
	BT_ASSERT(!ctx->scopes.event_common_context);
	ctx->sc = sc;

	if (!sc->is_translated) {
		ctx->scopes.packet_context = sc->packet_context_ft;
		ret = resolve_root_type(BT_SCOPE_PACKET_CONTEXT, ctx);
		if (ret) {
			BT_LOGE("Cannot resolve packet context field type: "
				"ret=%d", ret);
			goto end;
		}

		ctx->scopes.event_header = sc->event_header_ft;
		ret = resolve_root_type(BT_SCOPE_EVENT_HEADER, ctx);
		if (ret) {
			BT_LOGE("Cannot resolve event header field type: "
				"ret=%d", ret);
			goto end;
		}

		ctx->scopes.event_common_context = sc->event_common_context_ft;
		ret = resolve_root_type(BT_SCOPE_EVENT_SPECIFIC_CONTEXT, ctx);
		if (ret) {
			BT_LOGE("Cannot resolve event common context field type: "
				"ret=%d", ret);
			goto end;
		}
	}

	ctx->scopes.packet_context = sc->packet_context_ft;
	ctx->scopes.event_header = sc->event_header_ft;
	ctx->scopes.event_common_context = sc->event_common_context_ft;

	for (i = 0; i < sc->event_classes->len; i++) {
		struct ctf_event_class *ec = sc->event_classes->pdata[i];

		ret = resolve_event_class_field_types(ctx, ec);
		if (ret) {
			BT_LOGE("Cannot resolve event class's field types: "
				"ec-id=%" PRIu64 ", ec-name=\"%s\"",
				ec->id, ec->name->str);
			goto end;
		}
	}

end:
	ctx->scopes.packet_context = NULL;
	ctx->scopes.event_header = NULL;
	ctx->scopes.event_common_context = NULL;
	ctx->sc = NULL;
	return ret;
}

BT_HIDDEN
int ctf_trace_class_resolve_field_types(struct ctf_trace_class *tc)
{
	int ret = 0;
	uint64_t i;
	struct resolve_context ctx = {
		.tc = tc,
		.sc = NULL,
		.ec = NULL,
		.scopes = {
			.packet_header = tc->packet_header_ft,
			.packet_context = NULL,
			.event_header = NULL,
			.event_common_context = NULL,
			.event_spec_context = NULL,
			.event_payload = NULL,
		},
		.root_scope = BT_SCOPE_PACKET_HEADER,
		.cur_ft = NULL,
	};

	/* Initialize type stack */
	ctx.field_type_stack = field_type_stack_create();
	if (!ctx.field_type_stack) {
		BT_LOGE_STR("Cannot create field type stack.");
		ret = -1;
		goto end;
	}

	if (!tc->is_translated) {
		ctx.scopes.packet_header = tc->packet_header_ft;
		ret = resolve_root_type(BT_SCOPE_PACKET_HEADER, &ctx);
		if (ret) {
			BT_LOGE("Cannot resolve packet header field type: "
				"ret=%d", ret);
			goto end;
		}
	}

	ctx.scopes.packet_header = tc->packet_header_ft;

	for (i = 0; i < tc->stream_classes->len; i++) {
		struct ctf_stream_class *sc = tc->stream_classes->pdata[i];

		ret = resolve_stream_class_field_types(&ctx, sc);
		if (ret) {
			BT_LOGE("Cannot resolve stream class's field types: "
				"sc-id=%" PRIu64, sc->id);
			goto end;
		}
	}

end:
	field_type_stack_destroy(ctx.field_type_stack);
	return ret;
}
