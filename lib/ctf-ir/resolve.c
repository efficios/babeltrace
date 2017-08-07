/*
 * resolve.c
 *
 * Babeltrace - CTF IR: Type resolving internal
 *
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2016 Philippe Proulx <pproulx@efficios.com>
 *
 * Authors: Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *          Philippe Proulx <pproulx@efficios.com>
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

#define BT_LOG_TAG "RESOLVE"
#include <babeltrace/lib-logging-internal.h>

#include <babeltrace/ctf-ir/event.h>
#include <babeltrace/ctf-ir/stream-class.h>
#include <babeltrace/ctf-ir/resolve-internal.h>
#include <babeltrace/ctf-ir/field-types.h>
#include <babeltrace/ctf-ir/field-path.h>
#include <babeltrace/ctf-ir/field-path-internal.h>
#include <babeltrace/ctf-ir/event-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/values.h>
#include <babeltrace/types.h>
#include <limits.h>
#include <inttypes.h>
#include <stdlib.h>
#include <glib.h>

typedef GPtrArray type_stack;

/*
 * A stack frame.
 *
 * `type` contains a compound field type (structure, variant, array,
 * or sequence) and `index` indicates the index of the field type in
 * the upper frame (-1 for array and sequence field types).
 *
 * `type` is owned by the stack frame.
 */
struct type_stack_frame {
	struct bt_ctf_field_type *type;
	int index;
};

/*
 * The current context of the resolving engine.
 *
 * `scopes` contain the 6 CTF scope field types (see CTF, sect. 7.3.2)
 * in the following order:
 *
 *   * Packet header
 *   * Packet context
 *   * Event header
 *   * Stream event context
 *   * Event context
 *   * Event payload
 */
struct resolve_context {
	struct bt_value *environment;
	struct bt_ctf_field_type *scopes[6];

	/* Root scope being visited */
	enum bt_ctf_scope root_scope;
	type_stack *type_stack;
	struct bt_ctf_field_type *cur_field_type;
};

/* TSDL dynamic scope prefixes as defined in CTF Section 7.3.2 */
static const char * const absolute_path_prefixes[] = {
	[BT_CTF_SCOPE_ENV]			= "env.",
	[BT_CTF_SCOPE_TRACE_PACKET_HEADER]	= "trace.packet.header.",
	[BT_CTF_SCOPE_STREAM_PACKET_CONTEXT]	= "stream.packet.context.",
	[BT_CTF_SCOPE_STREAM_EVENT_HEADER]	= "stream.event.header.",
	[BT_CTF_SCOPE_STREAM_EVENT_CONTEXT]	= "stream.event.context.",
	[BT_CTF_SCOPE_EVENT_CONTEXT]		= "event.context.",
	[BT_CTF_SCOPE_EVENT_FIELDS]		= "event.fields.",
};

/* Number of path tokens used for the absolute prefixes */
static const int absolute_path_prefix_ptoken_counts[] = {
	[BT_CTF_SCOPE_ENV]			= 1,
	[BT_CTF_SCOPE_TRACE_PACKET_HEADER]	= 3,
	[BT_CTF_SCOPE_STREAM_PACKET_CONTEXT]	= 3,
	[BT_CTF_SCOPE_STREAM_EVENT_HEADER]	= 3,
	[BT_CTF_SCOPE_STREAM_EVENT_CONTEXT]	= 3,
	[BT_CTF_SCOPE_EVENT_CONTEXT]		= 2,
	[BT_CTF_SCOPE_EVENT_FIELDS]		= 2,
};

/*
 * Destroys a type stack frame.
 */
static
void type_stack_destroy_notify(gpointer data)
{
	struct type_stack_frame *frame = data;

	BT_PUT(frame->type);
	g_free(frame);
}

/*
 * Creates a type stack.
 *
 * Return value is owned by the caller.
 */
static
type_stack *type_stack_create(void)
{
	return g_ptr_array_new_with_free_func(type_stack_destroy_notify);
}

/*
 * Destroys a type stack.
 */
static
void type_stack_destroy(type_stack *stack)
{
	g_ptr_array_free(stack, TRUE);
}

/*
 * Pushes a field type onto a type stack.
 *
 * `type` is owned by the caller (stack frame gets a new reference).
 */
static
int type_stack_push(type_stack *stack, struct bt_ctf_field_type *type)
{
	int ret = 0;
	struct type_stack_frame *frame = NULL;

	if (!stack || !type) {
		BT_LOGW("Invalid parameter: stack or type is NULL.");
		ret = -1;
		goto end;
	}

	frame = g_new0(struct type_stack_frame, 1);
	if (!frame) {
		BT_LOGE_STR("Failed to allocate one field type stack frame.");
		ret = -1;
		goto end;
	}

	BT_LOGV("Pushing field type on context's stack: "
		"ft-addr=%p, stack-size-before=%u", type, stack->len);
	frame->type = bt_get(type);
	g_ptr_array_add(stack, frame);

end:
	return ret;
}

/*
 * Checks whether or not `stack` is empty.
 */
static
bt_bool type_stack_empty(type_stack *stack)
{
	return stack->len == 0;
}

/*
 * Returns the number of frames in `stack`.
 */
static
size_t type_stack_size(type_stack *stack)
{
	return stack->len;
}

/*
 * Returns the top frame of `stack`.
 *
 * Return value is owned by `stack`.
 */
static
struct type_stack_frame *type_stack_peek(type_stack *stack)
{
	struct type_stack_frame *entry = NULL;

	if (!stack || type_stack_empty(stack)) {
		goto end;
	}

	entry = g_ptr_array_index(stack, stack->len - 1);
end:
	return entry;
}

/*
 * Returns the frame at index `index` in `stack`.
 *
 * Return value is owned by `stack`.
 */
static
struct type_stack_frame *type_stack_at(type_stack *stack,
		size_t index)
{
	struct type_stack_frame *entry = NULL;

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
void type_stack_pop(type_stack *stack)
{
	if (!type_stack_empty(stack)) {
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
 *
 * Return value is owned by `ctx` on success.
 */
static
struct bt_ctf_field_type *get_type_from_ctx(struct resolve_context *ctx,
		enum bt_ctf_scope scope)
{
	assert(scope >= BT_CTF_SCOPE_TRACE_PACKET_HEADER &&
		scope <= BT_CTF_SCOPE_EVENT_FIELDS);

	return ctx->scopes[scope - BT_CTF_SCOPE_TRACE_PACKET_HEADER];
}

/*
 * Returns the CTF scope from a path string. May return
 * CTF_NODE_UNKNOWN if the path is found to be relative.
 */
static
enum bt_ctf_scope get_root_scope_from_absolute_pathstr(const char *pathstr)
{
	enum bt_ctf_scope scope;
	enum bt_ctf_scope ret = BT_CTF_SCOPE_UNKNOWN;
	const size_t prefixes_count = sizeof(absolute_path_prefixes) /
		sizeof(*absolute_path_prefixes);

	for (scope = BT_CTF_SCOPE_ENV; scope < BT_CTF_SCOPE_ENV +
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
				bt_ctf_scope_string(scope));
			continue;
		}

		/* Found it! */
		ret = scope;
		BT_LOGV("Found root scope from absolute path: "
			"path=\"%s\", scope=%s", pathstr,
			bt_ctf_scope_string(scope));
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
 *
 * Return value is owned by the caller on success.
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
				BT_LOGW("Empty path token: path=\"%s\", pos=%u",
					pathstr, (int) (at - pathstr));
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
 * list is relative from `type`. The index of the source looking for
 * its target within `type` is indicated by `src_index`. This can be
 * `INT_MAX` if the source is contained in `type`.
 *
 * `ptokens` is owned by the caller. `field_path` is an output parameter
 * owned by the caller that must be filled here. `type` is owned by the
 * caller.
 */
static
int ptokens_to_field_path(GList *ptokens, struct bt_ctf_field_path *field_path,
		struct bt_ctf_field_type *type, int src_index)
{
	int ret = 0;
	GList *cur_ptoken = ptokens;
	bt_bool first_level_done = BT_FALSE;

	/* Get our own reference */
	bt_get(type);

	/* Locate target */
	while (cur_ptoken) {
		int child_index;
		struct bt_ctf_field_type *child_type;
		const char *field_name = ptoken_get_string(cur_ptoken);
		enum bt_ctf_field_type_id type_id =
			bt_ctf_field_type_get_type_id(type);

		BT_LOGV("Current path token: token=\"%s\"", field_name);

		/* Find to which index corresponds the current path token */
		if (type_id == BT_CTF_FIELD_TYPE_ID_ARRAY ||
				type_id == BT_CTF_FIELD_TYPE_ID_SEQUENCE) {
			child_index = -1;
		} else {
			child_index = bt_ctf_field_type_get_field_index(type,
				field_name);
			if (child_index < 0) {
				/*
				 * Error: field name does not exist or
				 * wrong current type.
				 */
				BT_LOGW("Cannot get index of field type: "
					"field-name=\"%s\", src-index=%d, child-index=%d, first-level-done=%d",
					field_name, src_index, child_index, first_level_done);
				ret = -1;
				goto end;
			} else if (child_index > src_index &&
					!first_level_done) {
				BT_LOGW("Child field type is located after source field type: "
					"field-name=\"%s\", src-index=%d, child-index=%d, first-level-done=%d",
					field_name, src_index, child_index, first_level_done);
				ret = -1;
				goto end;
			}

			/* Next path token */
			cur_ptoken = g_list_next(cur_ptoken);
			first_level_done = BT_TRUE;
		}

		/* Create new field path entry */
		g_array_append_val(field_path->indexes, child_index);

		/* Get child field type */
		child_type = bt_ctf_field_type_get_field_at_index(type,
			child_index);
		if (!child_type) {
			BT_LOGW("Cannot get child field type: "
				"field-name=\"%s\", src-index=%d, child-index=%d, first-level-done=%d",
				field_name, src_index, child_index, first_level_done);
			ret = -1;
			goto end;
		}

		/* Move child type to current type */
		BT_MOVE(type, child_type);
	}

end:
	bt_put(type);
	return ret;
}

/*
 * Converts a known absolute path token list to a field path object
 * within the resolving context `ctx`.
 *
 * `ptokens` is owned by the caller. `field_path` is an output parameter
 * owned by the caller that must be filled here.
 */
static
int absolute_ptokens_to_field_path(GList *ptokens,
		struct bt_ctf_field_path *field_path,
		struct resolve_context *ctx)
{
	int ret = 0;
	GList *cur_ptoken;
	struct bt_ctf_field_type *type;

	/* Skip absolute path tokens */
	cur_ptoken = g_list_nth(ptokens,
		absolute_path_prefix_ptoken_counts[field_path->root]);

	/* Start with root type */
	type = get_type_from_ctx(ctx, field_path->root);
	if (!type) {
		/* Error: root type is not available */
		BT_LOGW("Root field type is not available: "
			"root-scope=%s",
			bt_ctf_scope_string(field_path->root));
		ret = -1;
		goto end;
	}

	/* Locate target */
	ret = ptokens_to_field_path(cur_ptoken, field_path, type, INT_MAX);

end:
	return ret;
}

/*
 * Converts a known relative path token list to a field path object
 * within the resolving context `ctx`.
 *
 * `ptokens` is owned by the caller. `field_path` is an output parameter
 * owned by the caller that must be filled here.
 */
static
int relative_ptokens_to_field_path(GList *ptokens,
		struct bt_ctf_field_path *field_path,
		struct resolve_context *ctx)
{
	int ret = 0;
	int parent_pos_in_stack;
	struct bt_ctf_field_path *tail_field_path = bt_ctf_field_path_create();

	if (!tail_field_path) {
		BT_LOGE_STR("Cannot create empty field path.");
		ret = -1;
		goto end;
	}

	parent_pos_in_stack = type_stack_size(ctx->type_stack) - 1;

	while (parent_pos_in_stack >= 0) {
		struct bt_ctf_field_type *parent_type =
			type_stack_at(ctx->type_stack,
				parent_pos_in_stack)->type;
		int cur_index = type_stack_at(ctx->type_stack,
			parent_pos_in_stack)->index;

		BT_LOGV("Locating target field type from current parent field type: "
			"parent-pos=%d, parent-ft-addr=%p, cur-index=%d",
			parent_pos_in_stack, parent_type, cur_index);

		/* Locate target from current parent type */
		ret = ptokens_to_field_path(ptokens, tail_field_path,
			parent_type, cur_index);
		if (ret) {
			/* Not found... yet */
			BT_LOGV_STR("Not found at this point.");
			bt_ctf_field_path_clear(tail_field_path);
		} else {
			/* Found: stitch tail field path to head field path */
			int i = 0;
			int tail_field_path_len =
				tail_field_path->indexes->len;

			while (BT_TRUE) {
				struct bt_ctf_field_type *cur_type =
					type_stack_at(ctx->type_stack, i)->type;
				int index = type_stack_at(
					ctx->type_stack, i)->index;

				if (cur_type == parent_type) {
					break;
				}

				g_array_append_val(field_path->indexes,
					index);
				i++;
			}

			for (i = 0; i < tail_field_path_len; i++) {
				int index = g_array_index(
					tail_field_path->indexes,
					int, i);

				g_array_append_val(field_path->indexes,
					index);
			}
			break;
		}

		parent_pos_in_stack--;
	}

	if (parent_pos_in_stack < 0) {
		/* Not found: look in previous scopes */
		field_path->root--;

		while (field_path->root >= BT_CTF_SCOPE_TRACE_PACKET_HEADER) {
			struct bt_ctf_field_type *root_type;
			bt_ctf_field_path_clear(field_path);

			BT_LOGV("Looking into potential root scope: scope=%s",
				bt_ctf_scope_string(field_path->root));
			root_type = get_type_from_ctx(ctx, field_path->root);
			if (!root_type) {
				field_path->root--;
				continue;
			}

			/* Locate target in previous scope */
			ret = ptokens_to_field_path(ptokens, field_path,
				root_type, INT_MAX);
			if (ret) {
				/* Not found yet */
				BT_LOGV_STR("Not found in this scope.");
				field_path->root--;
				continue;
			}

			/* Found */
			BT_LOGV_STR("Found in this scope.");
			break;
		}
	}

end:
	BT_PUT(tail_field_path);
	return ret;
}

/*
 * Converts a path string to a field path object within the resolving
 * context `ctx`.
 *
 * Return value is owned by the caller on success.
 */
static
struct bt_ctf_field_path *pathstr_to_field_path(const char *pathstr,
		struct resolve_context *ctx)
{
	int ret;
	enum bt_ctf_scope root_scope;
	GList *ptokens = NULL;
	struct bt_ctf_field_path *field_path = NULL;

	/* Create field path */
	field_path = bt_ctf_field_path_create();
	if (!field_path) {
		BT_LOGE_STR("Cannot create empty field path.");
		ret = -1;
		goto end;
	}

	/* Convert path string to path tokens */
	ptokens = pathstr_to_ptokens(pathstr);
	if (!ptokens) {
		BT_LOGW("Cannot convert path string to path tokens: "
			"path=\"%s\"", pathstr);
		ret = -1;
		goto end;
	}

	/* Absolute or relative path? */
	root_scope = get_root_scope_from_absolute_pathstr(pathstr);

	if (root_scope == BT_CTF_SCOPE_UNKNOWN) {
		/* Relative path: start with current root scope */
		field_path->root = ctx->root_scope;
		BT_LOGV("Detected relative path: starting with current root scope: "
			"scope=%s", bt_ctf_scope_string(field_path->root));
		ret = relative_ptokens_to_field_path(ptokens, field_path, ctx);
		if (ret) {
			BT_LOGW("Cannot get relative field path of path string: "
				"path=\"%s\", start-scope=%s, end-scope=%s",
				pathstr, bt_ctf_scope_string(ctx->root_scope),
				bt_ctf_scope_string(field_path->root));
			goto end;
		}
	} else if (root_scope == BT_CTF_SCOPE_ENV) {
		BT_LOGW("Sequence field types referring the trace environment are not supported as of this version: "
			"path=\"%s\"", pathstr);
		ret = -1;
		goto end;
	} else {
		/* Absolute path: use found root scope */
		field_path->root = root_scope;
		BT_LOGV("Detected absolute path: using root scope: "
			"scope=%s", bt_ctf_scope_string(field_path->root));
		ret = absolute_ptokens_to_field_path(ptokens, field_path, ctx);
		if (ret) {
			BT_LOGW("Cannot get absolute field path of path string: "
				"path=\"%s\", root-scope=%s",
				pathstr, bt_ctf_scope_string(root_scope));
			goto end;
		}
	}

	if (ret == 0) {
		GString *field_path_pretty =
			bt_ctf_field_path_string(field_path);
		const char *field_path_pretty_str =
			field_path_pretty ? field_path_pretty->str : NULL;

		BT_LOGV("Found field path: path=\"%s\", field-path=\"%s\"",
			pathstr, field_path_pretty_str);

		if (field_path_pretty) {
			g_string_free(field_path_pretty, TRUE);
		}
	}

end:
	if (ret) {
		BT_PUT(field_path);
	}

	ptokens_destroy(ptokens);
	return field_path;
}

/*
 * Retrieves a field type by following the field path `field_path` in
 * the resolving context `ctx`.
 *
 * Return value is owned by the caller on success.
 */
static
struct bt_ctf_field_type *field_path_to_field_type(
		struct bt_ctf_field_path *field_path,
		struct resolve_context *ctx)
{
	int i;
	struct bt_ctf_field_type *type;

	/* Start with root type */
	type = get_type_from_ctx(ctx, field_path->root);
	bt_get(type);
	if (!type) {
		/* Error: root type is not available */
		BT_LOGW("Root field type is not available: root-scope=%s",
			bt_ctf_scope_string(field_path->root));
		goto error;
	}

	/* Locate target */
	for (i = 0; i < field_path->indexes->len; i++) {
		struct bt_ctf_field_type *child_type;
		int child_index =
			g_array_index(field_path->indexes, int, i);

		/* Get child field type */
		child_type = bt_ctf_field_type_get_field_at_index(type,
			child_index);
		if (!child_type) {
			BT_LOGW("Cannot get field type: "
				"parent-ft-addr=%p, index=%d", type, i);
			goto error;
		}

		/* Move child type to current type */
		BT_MOVE(type, child_type);
	}

	return type;

error:
	BT_PUT(type);
	return type;
}

/*
 * Returns the equivalent field path object of the context type stack.
 *
 * Return value is owned by the caller on success.
 */
static
struct bt_ctf_field_path *get_ctx_stack_field_path(struct resolve_context *ctx)
{
	int i;
	struct bt_ctf_field_path *field_path;

	/* Create field path */
	field_path = bt_ctf_field_path_create();
	if (!field_path) {
		BT_LOGE_STR("Cannot create empty field path.");
		goto error;
	}

	field_path->root = ctx->root_scope;

	for (i = 0; i < type_stack_size(ctx->type_stack); i++) {
		struct type_stack_frame *frame;

		frame = type_stack_at(ctx->type_stack, i);
		g_array_append_val(field_path->indexes, frame->index);
	}

	return field_path;

error:
	BT_PUT(field_path);
	return field_path;
}

/*
 * Returns the lowest common ancestor of two field path objects
 * having the same root scope.
 *
 * `field_path1` and `field_path2` are owned by the caller.
 */
int get_field_paths_lca_index(struct bt_ctf_field_path *field_path1,
		struct bt_ctf_field_path *field_path2)
{
	int lca_index = 0;
	int field_path1_len, field_path2_len;

	if (BT_LOG_ON_VERBOSE) {
		GString *field_path1_pretty =
			bt_ctf_field_path_string(field_path1);
		GString *field_path2_pretty =
			bt_ctf_field_path_string(field_path2);
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
	assert(field_path1->root == field_path2->root);
	field_path1_len = field_path1->indexes->len;
	field_path2_len = field_path2->indexes->len;

	while (BT_TRUE) {
		int target_index, ctx_index;

		if (lca_index == field_path2_len ||
				lca_index == field_path1_len) {
			/*
			 * This means that both field paths never split.
			 * This is invalid because the target cannot be
			 * an ancestor of the source.
			 */
			BT_LOGW("Source field type is an ancestor of target field type or vice versa: "
				"lca-index=%d, field-path-1-len=%d, "
				"field-path-2-len=%d",
				lca_index, field_path1_len, field_path2_len);
			lca_index = -1;
			break;
		}

		target_index = g_array_index(field_path1->indexes, int,
			lca_index);
		ctx_index = g_array_index(field_path2->indexes, int,
			lca_index);

		if (target_index != ctx_index) {
			/* LCA index is the previous */
			break;
		}

		lca_index++;
	}

	BT_LOGV("Found LCA: lca-index=%d", lca_index);
	return lca_index;
}

/*
 * Validates a target field path.
 *
 * `target_field_path` and `target_type` are owned by the caller.
 */
static
int validate_target_field_path(struct bt_ctf_field_path *target_field_path,
		struct bt_ctf_field_type *target_type,
		struct resolve_context *ctx)
{
	int ret = 0;
	struct bt_ctf_field_path *ctx_field_path;
	int target_field_path_len = target_field_path->indexes->len;
	int lca_index;
	enum bt_ctf_field_type_id ctx_cur_field_type_id;
	enum bt_ctf_field_type_id target_type_id;

	/* Get context field path */
	ctx_field_path = get_ctx_stack_field_path(ctx);
	if (!ctx_field_path) {
		BT_LOGW_STR("Cannot get field path from context's stack.");
		ret = -1;
		goto end;
	}

	/*
	 * Make sure the target is not a root.
	 */
	if (target_field_path_len == 0) {
		BT_LOGW_STR("Target field path's length is 0 (targeting the root).");
		ret = -1;
		goto end;
	}

	/*
	 * Make sure the root of the target field path is not located
	 * after the context field path's root.
	 */
	if (target_field_path->root > ctx_field_path->root) {
		BT_LOGW("Target field type is located after source field type: "
			"target-root=%s, source-root=%s",
			bt_ctf_scope_string(target_field_path->root),
			bt_ctf_scope_string(ctx_field_path->root));
		ret = -1;
		goto end;
	}

	if (target_field_path->root == ctx_field_path->root) {
		int target_index, ctx_index;

		/*
		 * Find the index of the lowest common ancestor of both field
		 * paths.
		 */
		lca_index = get_field_paths_lca_index(target_field_path,
			ctx_field_path);
		if (lca_index < 0) {
			BT_LOGW_STR("Cannot get least common ancestor.");
			ret = -1;
			goto end;
		}

		/*
		 * Make sure the target field path is located before the
		 * context field path.
		 */
		target_index = g_array_index(target_field_path->indexes,
			int, lca_index);
		ctx_index = g_array_index(ctx_field_path->indexes,
			int, lca_index);

		if (target_index >= ctx_index) {
			BT_LOGW("Target field type's index is greater than or equal to source field type's index in LCA: "
				"lca-index=%d, target-index=%d, source-index=%d",
				lca_index, target_index, ctx_index);
			ret = -1;
			goto end;
		}
	}

	/*
	 * Make sure the target type has the right type and properties.
	 */
	ctx_cur_field_type_id = bt_ctf_field_type_get_type_id(
		ctx->cur_field_type);
	target_type_id = bt_ctf_field_type_get_type_id(target_type);

	switch (ctx_cur_field_type_id) {
	case BT_CTF_FIELD_TYPE_ID_VARIANT:
		if (target_type_id != BT_CTF_FIELD_TYPE_ID_ENUM) {
			BT_LOGW("Variant field type's tag field type is not an enumeration field type: "
				"tag-ft-addr=%p, tag-ft-id=%s",
				target_type,
				bt_ctf_field_type_id_string(target_type_id));
			ret = -1;
			goto end;
		}
		break;
	case BT_CTF_FIELD_TYPE_ID_SEQUENCE:
		if (target_type_id != BT_CTF_FIELD_TYPE_ID_INTEGER ||
				bt_ctf_field_type_integer_get_signed(
					target_type)) {
			BT_LOGW("Sequence field type's length field type is not an unsigned integer field type: "
				"length-ft-addr=%p, length-ft-id=%s",
				target_type,
				bt_ctf_field_type_id_string(target_type_id));
			ret = -1;
			goto end;
		}
		break;
	default:
		abort();
	}

end:
	BT_PUT(ctx_field_path);
	return ret;
}

/*
 * Resolves a variant or sequence field type `type`.
 *
 * `type` is owned by the caller.
 */
static
int resolve_sequence_or_variant_type(struct bt_ctf_field_type *type,
		struct resolve_context *ctx)
{
	int ret = 0;
	const char *pathstr;
	enum bt_ctf_field_type_id type_id = bt_ctf_field_type_get_type_id(type);
	struct bt_ctf_field_path *target_field_path = NULL;
	struct bt_ctf_field_type *target_type = NULL;
	GString *target_field_path_pretty = NULL;
	const char *target_field_path_pretty_str;


	/* Get path string */
	switch (type_id) {
	case BT_CTF_FIELD_TYPE_ID_SEQUENCE:
		pathstr =
			bt_ctf_field_type_sequence_get_length_field_name(type);
		break;
	case BT_CTF_FIELD_TYPE_ID_VARIANT:
		pathstr =
			bt_ctf_field_type_variant_get_tag_name(type);
		break;
	default:
		abort();
	}

	if (!pathstr) {
		BT_LOGW("Cannot get path string");
		ret = -1;
		goto end;
	}

	/* Get target field path out of path string */
	target_field_path = pathstr_to_field_path(pathstr, ctx);
	if (!target_field_path) {
		BT_LOGW("Cannot get target field path for path string: "
			"path=\"%s\"", pathstr);
		ret = -1;
		goto end;
	}

	target_field_path_pretty = bt_ctf_field_path_string(target_field_path);
	target_field_path_pretty_str =
		target_field_path_pretty ? target_field_path_pretty->str : NULL;

	/* Get target field type */
	target_type = field_path_to_field_type(target_field_path, ctx);
	if (!target_type) {
		BT_LOGW("Cannot get target field type for path string: "
			"path=\"%s\", target-field-path=\"%s\"",
			pathstr, target_field_path_pretty_str);
		ret = -1;
		goto end;
	}

	ret = validate_target_field_path(target_field_path, target_type, ctx);
	if (ret) {
		BT_LOGW("Invalid target field path for path string: "
			"path=\"%s\", target-field-path=\"%s\"",
			pathstr, target_field_path_pretty_str);
		goto end;
	}

	/* Set target field path and target field type */
	switch (type_id) {
	case BT_CTF_FIELD_TYPE_ID_SEQUENCE:
		ret = bt_ctf_field_type_sequence_set_length_field_path(
			type, target_field_path);
		if (ret) {
			BT_LOGW("Cannot set sequence field type's length field path: "
				"ret=%d, ft-addr=%p, path=\"%s\", target-field-path=\"%s\"",
				ret, type, pathstr,
				target_field_path_pretty_str);
			goto end;
		}
		break;
	case BT_CTF_FIELD_TYPE_ID_VARIANT:
		ret = bt_ctf_field_type_variant_set_tag_field_path(
			type, target_field_path);
		if (ret) {
			BT_LOGW("Cannot set varaint field type's tag field path: "
				"ret=%d, ft-addr=%p, path=\"%s\", target-field-path=\"%s\"",
				ret, type, pathstr,
				target_field_path_pretty_str);
			goto end;
		}

		ret = bt_ctf_field_type_variant_set_tag_field_type(
			type, target_type);
		if (ret) {
			BT_LOGW("Cannot set varaint field type's tag field type: "
				"ret=%d, ft-addr=%p, path=\"%s\", target-field-path=\"%s\"",
				ret, type, pathstr,
				target_field_path_pretty_str);
			goto end;
		}
		break;
	default:
		abort();
	}

end:
	if (target_field_path_pretty) {
		g_string_free(target_field_path_pretty, TRUE);
	}

	BT_PUT(target_field_path);
	BT_PUT(target_type);
	return ret;
}

/*
 * Resolves a field type `type`.
 *
 * `type` is owned by the caller.
 */
static
int resolve_type(struct bt_ctf_field_type *type, struct resolve_context *ctx)
{
	int ret = 0;
	enum bt_ctf_field_type_id type_id;

	if (!type) {
		/* Type is not available; still valid */
		goto end;
	}

	type_id = bt_ctf_field_type_get_type_id(type);
	ctx->cur_field_type = type;

	/* Resolve sequence/variant field type */
	switch (type_id) {
	case BT_CTF_FIELD_TYPE_ID_SEQUENCE:
	case BT_CTF_FIELD_TYPE_ID_VARIANT:
		ret = resolve_sequence_or_variant_type(type, ctx);
		if (ret) {
			BT_LOGW("Cannot resolve sequence field type's length or variant field type's tag: "
				"ret=%d, ft-addr=%p", ret, type);
			goto end;
		}
		break;
	default:
		break;
	}

	/* Recurse into compound types */
	switch (type_id) {
	case BT_CTF_FIELD_TYPE_ID_STRUCT:
	case BT_CTF_FIELD_TYPE_ID_VARIANT:
	case BT_CTF_FIELD_TYPE_ID_SEQUENCE:
	case BT_CTF_FIELD_TYPE_ID_ARRAY:
	{
		int64_t field_count, f_index;

		ret = type_stack_push(ctx->type_stack, type);
		if (ret) {
			BT_LOGW("Cannot push field type on context's stack: "
				"ft-addr=%p", type);
			goto end;
		}

		field_count = bt_ctf_field_type_get_field_count(type);
		if (field_count < 0) {
			BT_LOGW("Cannot get field type's field count: "
				"ret=%" PRId64 ", ft-addr=%p",
				field_count, type);
			ret = field_count;
			goto end;
		}

		for (f_index = 0; f_index < field_count; f_index++) {
			struct bt_ctf_field_type *child_type =
				bt_ctf_field_type_get_field_at_index(type,
					f_index);

			if (!child_type) {
				BT_LOGW("Cannot get field type's child field: "
					"ft-addr=%p, index=%" PRId64 ", "
					"count=%" PRId64, type, f_index,
					field_count);
				ret = -1;
				goto end;
			}

			if (type_id == BT_CTF_FIELD_TYPE_ID_ARRAY||
					type_id == BT_CTF_FIELD_TYPE_ID_SEQUENCE) {
				type_stack_peek(ctx->type_stack)->index = -1;
			} else {
				type_stack_peek(ctx->type_stack)->index =
					f_index;
			}

			BT_LOGV("Resolving field type's child field type: "
				"parent-ft-addr=%p, child-ft-addr=%p, "
				"index=%" PRId64 ", count=%" PRId64,
				type, child_type, f_index, field_count);
			ret = resolve_type(child_type, ctx);
			BT_PUT(child_type);
			if (ret) {
				goto end;
			}
		}

		type_stack_pop(ctx->type_stack);
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
int resolve_root_type(enum bt_ctf_scope root_scope, struct resolve_context *ctx)
{
	int ret;

	assert(type_stack_size(ctx->type_stack) == 0);
	ctx->root_scope = root_scope;
	ret = resolve_type(get_type_from_ctx(ctx, root_scope), ctx);
	ctx->root_scope = BT_CTF_SCOPE_UNKNOWN;

	return ret;
}

BT_HIDDEN
int bt_ctf_resolve_types(
		struct bt_value *environment,
		struct bt_ctf_field_type *packet_header_type,
		struct bt_ctf_field_type *packet_context_type,
		struct bt_ctf_field_type *event_header_type,
		struct bt_ctf_field_type *stream_event_ctx_type,
		struct bt_ctf_field_type *event_context_type,
		struct bt_ctf_field_type *event_payload_type,
		enum bt_ctf_resolve_flag flags)
{
	int ret = 0;
	struct resolve_context ctx = {
		.environment = environment,
		.scopes = {
			packet_header_type,
			packet_context_type,
			event_header_type,
			stream_event_ctx_type,
			event_context_type,
			event_payload_type,
		},
		.root_scope = BT_CTF_SCOPE_UNKNOWN,
	};

	BT_LOGV("Resolving field types: "
		"packet-header-ft-addr=%p, "
		"packet-context-ft-addr=%p, "
		"event-header-ft-addr=%p, "
		"stream-event-context-ft-addr=%p, "
		"event-context-ft-addr=%p, "
		"event-payload-ft-addr=%p",
		packet_header_type, packet_context_type, event_header_type,
		stream_event_ctx_type, event_context_type, event_payload_type);

	/* Initialize type stack */
	ctx.type_stack = type_stack_create();
	if (!ctx.type_stack) {
		BT_LOGE_STR("Cannot create field type stack.");
		ret = -1;
		goto end;
	}

	/* Resolve packet header type */
	if (flags & BT_CTF_RESOLVE_FLAG_PACKET_HEADER) {
		ret = resolve_root_type(BT_CTF_SCOPE_TRACE_PACKET_HEADER, &ctx);
		if (ret) {
			BT_LOGW("Cannot resolve trace packet header field type: "
				"ret=%d", ret);
			goto end;
		}
	}

	/* Resolve packet context type */
	if (flags & BT_CTF_RESOLVE_FLAG_PACKET_CONTEXT) {
		ret = resolve_root_type(BT_CTF_SCOPE_STREAM_PACKET_CONTEXT, &ctx);
		if (ret) {
			BT_LOGW("Cannot resolve stream packet context field type: "
				"ret=%d", ret);
			goto end;
		}
	}

	/* Resolve event header type */
	if (flags & BT_CTF_RESOLVE_FLAG_EVENT_HEADER) {
		ret = resolve_root_type(BT_CTF_SCOPE_STREAM_EVENT_HEADER, &ctx);
		if (ret) {
			BT_LOGW("Cannot resolve stream event header field type: "
				"ret=%d", ret);
			goto end;
		}
	}

	/* Resolve stream event context type */
	if (flags & BT_CTF_RESOLVE_FLAG_STREAM_EVENT_CTX) {
		ret = resolve_root_type(BT_CTF_SCOPE_STREAM_EVENT_CONTEXT, &ctx);
		if (ret) {
			BT_LOGW("Cannot resolve stream event context field type: "
				"ret=%d", ret);
			goto end;
		}
	}

	/* Resolve event context type */
	if (flags & BT_CTF_RESOLVE_FLAG_EVENT_CONTEXT) {
		ret = resolve_root_type(BT_CTF_SCOPE_EVENT_CONTEXT, &ctx);
		if (ret) {
			BT_LOGW("Cannot resolve event context field type: "
				"ret=%d", ret);
			goto end;
		}
	}

	/* Resolve event payload type */
	if (flags & BT_CTF_RESOLVE_FLAG_EVENT_PAYLOAD) {
		ret = resolve_root_type(BT_CTF_SCOPE_EVENT_FIELDS, &ctx);
		if (ret) {
			BT_LOGW("Cannot resolve event payload field type: "
				"ret=%d", ret);
			goto end;
		}
	}

	BT_LOGV_STR("Resolved field types.");

end:
	type_stack_destroy(ctx.type_stack);

	return ret;
}
