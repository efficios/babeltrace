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

#include <babeltrace/ctf-ir/event.h>
#include <babeltrace/ctf-ir/stream-class.h>
#include <babeltrace/ctf-ir/resolve-internal.h>
#include <babeltrace/ctf-ir/event-types-internal.h>
#include <babeltrace/ctf-ir/event-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/babeltrace-internal.h>
#include <glib.h>

typedef GPtrArray type_stack;

struct type_stack_frame {
	struct bt_ctf_field_type *type;
	int index;
};

struct resolve_context {
	struct bt_ctf_field_type *packet_header_type;
	struct bt_ctf_field_type *packet_context_type;
	struct bt_ctf_field_type *event_header_type;
	struct bt_ctf_field_type *stream_event_ctx_type;
	struct bt_ctf_field_type *event_context_type;
	struct bt_ctf_field_type *event_payload_type;
	/* Root node being visited */
	enum bt_ctf_node root_node;
	type_stack *type_stack;
};

/* TSDL dynamic scope prefixes as defined in CTF Section 7.3.2 */
static const char * const absolute_path_prefixes[] = {
	[CTF_NODE_ENV]				= "env.",
	[CTF_NODE_TRACE_PACKET_HEADER]		= "trace.packet.header.",
	[CTF_NODE_STREAM_PACKET_CONTEXT]	= "stream.packet.context.",
	[CTF_NODE_STREAM_EVENT_HEADER]		= "stream.event.header.",
	[CTF_NODE_STREAM_EVENT_CONTEXT]		= "stream.event.context.",
	[CTF_NODE_EVENT_CONTEXT]		= "event.context.",
	[CTF_NODE_EVENT_FIELDS]			= "event.fields.",
};

static const int absolute_path_prefix_ptoken_counts[] = {
	[CTF_NODE_ENV]				= 1,
	[CTF_NODE_TRACE_PACKET_HEADER]		= 3,
	[CTF_NODE_STREAM_PACKET_CONTEXT]	= 3,
	[CTF_NODE_STREAM_EVENT_HEADER]		= 3,
	[CTF_NODE_STREAM_EVENT_CONTEXT]		= 3,
	[CTF_NODE_EVENT_CONTEXT]		= 2,
	[CTF_NODE_EVENT_FIELDS]			= 2,
};

static const char * const type_names[] = {
	[CTF_TYPE_UNKNOWN] = "unknown",
	[CTF_TYPE_INTEGER] = "integer",
	[CTF_TYPE_FLOAT] = "float",
	[CTF_TYPE_ENUM] = "enumeration",
	[CTF_TYPE_STRING] = "string",
	[CTF_TYPE_STRUCT] = "structure",
	[CTF_TYPE_UNTAGGED_VARIANT] = "untagged variant",
	[CTF_TYPE_VARIANT] = "variant",
	[CTF_TYPE_ARRAY] = "array",
	[CTF_TYPE_SEQUENCE] = "sequence",
};

static
void type_stack_destroy_notify(gpointer data)
{
	struct type_stack_frame *frame = data;

	BT_PUT(frame->type);
	g_free(frame);
}

/*
 * Return value is owned by the caller.
 */
static
type_stack *type_stack_create(void)
{
	return g_ptr_array_new_with_free_func(type_stack_destroy_notify);
}

static
void type_stack_destroy(type_stack *stack)
{
	g_ptr_array_free(stack, TRUE);
}

/*
 * `type` is owned by the caller (stack frame gets a new reference).
 */
static
int type_stack_push(type_stack *stack, struct bt_ctf_field_type *type)
{
	int ret = 0;
	struct type_stack_frame *frame = NULL;

	if (!stack || !frame) {
		ret = -1;
		goto end;
	}

	frame = g_new0(struct type_stack_frame, 1);

	if (!frame) {
		ret = -1;
		goto end;
	}

	frame->type = type;
	bt_get(frame->type);
	g_ptr_array_add(stack, frame);

end:
	return ret;
}

static
bool type_stack_empty(type_stack *stack)
{
	return stack->len == 0;
}

static
size_t type_stack_size(type_stack *stack)
{
	return stack->len;
}

/*
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

static
void type_stack_pop(type_stack *stack)
{
	if (!type_stack_empty(stack)) {
		/*
		 * This will call the frame's destructor and free it, as
		 * well as put its contained field type.
		 */
		g_ptr_array_set_size(stack, stack->len - 1);
	}
}

/*
 * `type` is owned by the caller.
 */
static
int get_type_field_count(struct bt_ctf_field_type *type)
{
	int field_count = -1;
	enum ctf_type_id type_id = bt_ctf_field_type_get_type_id(type);

	switch (type_id) {
	case CTF_TYPE_STRUCT:
		field_count = bt_ctf_field_type_structure_get_field_count(type);
		break;

	case CTF_TYPE_VARIANT:
		field_count = bt_ctf_field_type_variant_get_field_count(type);
		break;

	case CTF_TYPE_ARRAY:
	case CTF_TYPE_SEQUENCE:
		/*
		 * Array and sequence types always contain a single member
		 * (the element type).
		 */
		field_count = 1;
		break;

	default:
		break;
	}

	return field_count;
}

/*
 * Return value is owned by the caller on success.
 */
static
struct bt_ctf_field_type *get_type_field_at_index(
		struct bt_ctf_field_type *type, int i)
{
	struct bt_ctf_field_type *field = NULL;
	enum ctf_type_id type_id = bt_ctf_field_type_get_type_id(type);

	switch (type_id) {
	case CTF_TYPE_STRUCT:
		bt_ctf_field_type_structure_get_field(type, NULL, &field, i);
		break;

	case CTF_TYPE_VARIANT:
		bt_ctf_field_type_variant_get_field(type, NULL, &field, i);
		break;

	case CTF_TYPE_ARRAY:
		field = bt_ctf_field_type_array_get_element_type(type);
		break;

	case CTF_TYPE_SEQUENCE:
		field = bt_ctf_field_type_sequence_get_element_type(type);
		break;

	default:
		break;
	}

	return field;
}

/*
 * `type` is owned by the caller.
 */
static
int get_type_field_index(struct bt_ctf_field_type *type, const char *name)
{
	int field_index = -1;
	enum ctf_type_id type_id = bt_ctf_field_type_get_type_id(type);

	switch (type_id) {
	case CTF_TYPE_STRUCT:
		field_index = bt_ctf_field_type_structure_get_field_name_index(
			type, name);
		break;

	case CTF_TYPE_VARIANT:
		field_index = bt_ctf_field_type_variant_get_field_name_index(
			type, name);
		break;

	default:
		break;
	}

	return field_index;
}

/*
 * Return value is owned by `ctx` on success.
 */
static
struct bt_ctf_field_type *get_type_from_ctx(struct resolve_context *ctx,
		enum bt_ctf_node node)
{
	struct bt_ctf_field_type *ret = NULL;

	switch (node) {
	case CTF_NODE_TRACE_PACKET_HEADER:
		ret = ctx->packet_header_type;
		break;

	case CTF_NODE_STREAM_PACKET_CONTEXT:
		ret = ctx->packet_context_type;
		break;

	case CTF_NODE_STREAM_EVENT_HEADER:
		ret = ctx->event_header_type;
		break;

	case CTF_NODE_STREAM_EVENT_CONTEXT:
		ret = ctx->stream_event_ctx_type;
		break;

	case CTF_NODE_EVENT_CONTEXT:
		ret = ctx->event_context_type;
		break;

	case CTF_NODE_EVENT_FIELDS:
		ret = ctx->event_payload_type;
		break;

	default:
		assert(0);
	}

	return ret;
}

static
enum bt_ctf_node get_root_node_from_absolute_pathstr(const char *pathstr)
{
	int i;
	enum bt_ctf_node ret = CTF_NODE_UNKNOWN;
	const size_t prefixes_count = sizeof(absolute_path_prefixes) /
		sizeof(*absolute_path_prefixes);

	for (i = 0; i < prefixes_count; i++) {
		/*
		 * Chech if path string starts with a known absolute
		 * path prefix.
		 *
		 * Refer to CTF 7.3.2 STATIC AND DYNAMIC SCOPES.
		 */
		if (strncmp(pathstr, absolute_path_prefixes[i],
			strlen(absolute_path_prefixes[i]))) {
			/* Prefix does not match: try the next one */
			continue;
		}

		/* Found it! */
		ret = i;
		goto end;
	}

end:
	return ret;
}

static
void ptokens_destroy_func(gpointer data)
{
	g_string_free(data, TRUE);
}

static
void ptokens_destroy(GList *ptokens)
{
	if (!ptokens) {
		return;
	}

	g_list_free_full(ptokens, ptokens_destroy_func);
}

static
const char *ptoken_get_string(GList *ptoken)
{
	GString *tokenstr = (GString *) ptoken->data;

	return tokenstr->str;
}

/*
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
 * `ptokens` is owned by the caller, but may be modified here.
 * `field_path` is an output parameter owned by the caller.
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
	bt_get(type);

	if (!type) {
		/* Error: root type is not available */
		ret = -1;
		goto end;
	}

	/* Locate target */
	while (cur_ptoken) {
		struct bt_ctf_field_type *child_type;
		int child_index;
		enum ctf_type_id type_id = bt_ctf_field_type_get_type_id(type);
		const char *field_name = ptoken_get_string(cur_ptoken);

		/* Find to which index corresponds the current path token */
		if (type_id == CTF_TYPE_ARRAY || type_id == CTF_TYPE_SEQUENCE) {
			child_index = -1;
		} else {
			child_index = get_type_field_index(type, field_name);

			if (child_index < 0) {
				/*
				 * Error: field name does not exist or
				 * wrong current type.
				 */
				ret = -1;
				goto end;
			}
		}

		/* Create new field path entry */
		g_array_append_val(field_path->path_indexes, child_index);

		/* Get child field type */
		child_type = get_type_field_at_index(type, child_index);

		if (!child_type) {
			ret = -1;
			goto end;
		}

		/* Move child type to current type */
		BT_PUT(type);
		BT_MOVE(type, child_type);
	}

end:
	BT_PUT(type);

	return ret;
}

/*
 * `ptokens` is owned by the caller, but may be modified here.
 * `field_path` is an output parameter owned by the caller.
 */
static
int relative_ptokens_to_field_path(GList *ptokens,
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
	bt_get(type);

	if (!type) {
		/* Error: root type is not available */
		ret = -1;
		goto end;
	}

	/* Locate target */
	while (cur_ptoken) {
		struct bt_ctf_field_type *child_type;
		int child_index;
		enum ctf_type_id type_id = bt_ctf_field_type_get_type_id(type);
		const char *field_name = ptoken_get_string(cur_ptoken);

		/* Find to which index corresponds the current path token */
		if (type_id == CTF_TYPE_ARRAY || type_id == CTF_TYPE_SEQUENCE) {
			child_index = -1;
		} else {
			child_index = get_type_field_index(type, field_name);

			if (child_index < 0) {
				/*
				 * Error: field name does not exist or
				 * wrong current type.
				 */
				ret = -1;
				goto end;
			}
		}

		/* Create new field path entry */
		g_array_append_val(field_path->path_indexes, child_index);

		/* Get child field type */
		child_type = get_type_field_at_index(type, child_index);

		if (!child_type) {
			ret = -1;
			goto end;
		}

		/* Move child type to current type */
		BT_PUT(type);
		BT_MOVE(type, child_type);
	}

end:
	BT_PUT(type);

	return ret;
}

/*
 * Return value is owned by the caller on success.
 */
static
struct bt_ctf_field_path *pathstr_to_field_path(const char *pathstr,
		struct resolve_context *ctx)
{
	int ret;
	enum bt_ctf_node root_node;
	GList *ptokens = NULL;
	struct bt_ctf_field_path *field_path = NULL;

	/* Create field path */
	field_path = bt_ctf_field_path_create();

	if (!field_path) {
		goto error;
	}

	/* Convert path string to path tokens */
	ptokens = pathstr_to_ptokens(pathstr);

	if (!ptokens) {
		ret = -1;
		goto error;
	}

	/* Absolute or relative path? */
	root_node = get_root_node_from_absolute_pathstr(pathstr);

	if (root_node == CTF_NODE_UNKNOWN) {
		/* Relative path: start with current root node */
		field_path->root = ctx->root_node;
		ret = relative_ptokens_to_field_path(ptokens, field_path, ctx);

		if (ret) {
			goto error;
		}
	} else {
		/* Absolute path */
		field_path->root = root_node;
		ret = absolute_ptokens_to_field_path(ptokens, field_path, ctx);

		if (ret) {
			goto error;
		}
	}

	return field_path;

error:
	bt_ctf_field_path_destroy(field_path);
	ptokens_destroy(ptokens);

	return NULL;
}

/*
 * `field_path` is owned by the caller.
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
		goto error;
	}

	/* Locate target */
	for (i = 0; i < field_path->path_indexes->len; ++i) {
		struct bt_ctf_field_type *child_type;
		int child_index =
			g_array_index(field_path->path_indexes, int, i);

		/* Get child field type */
		child_type = get_type_field_at_index(type, child_index);

		if (!child_type) {
			goto error;
		}

		/* Move child type to current type */
		BT_PUT(type);
		BT_MOVE(type, child_type);
	}

	return type;

error:
	BT_PUT(type);

	return NULL;
}

/*
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
		goto error;
	}

	field_path->root = ctx->root_node;

	for (i = 0; i < type_stack_size(ctx->type_stack); ++i) {
		struct type_stack_frame *frame;

		frame = type_stack_at(ctx->type_stack, i);
		g_array_append_val(field_path->path_indexes, frame->index);
	}

	return field_path;

error:
	bt_ctf_field_path_destroy(field_path);

	return NULL;
}

/*
 * `type` is owned by the caller.
 */
static
int resolve_sequence_or_variant_type(struct bt_ctf_field_type *type,
		struct resolve_context *ctx)
{
	int ret = 0;
	const char *pathstr;
	int type_id = bt_ctf_field_type_get_type_id(type);
	struct bt_ctf_field_path *target_field_path = NULL;
	struct bt_ctf_field_path *ctx_field_path = NULL;
	struct bt_ctf_field_type *target_type = NULL;

	/* Get path string */
	switch (type_id) {
	case CTF_TYPE_SEQUENCE:
		pathstr =
			bt_ctf_field_type_sequence_get_length_field_name(type);
		break;

	case CTF_TYPE_VARIANT:
		pathstr =
			bt_ctf_field_type_variant_get_tag_name(type);
		break;

	default:
		ret = -1;
		goto end;
	}

	/* Get target field path out of path string */
	target_field_path = pathstr_to_field_path(pathstr, ctx);

	if (!target_field_path) {
		ret = -1;
		goto end;
	}

	/* Get context field path */
	ctx_field_path = get_ctx_stack_field_path(ctx);

	if (!ctx_field_path) {
		ret = -1;
		goto end;
	}

	// VALIDATION STEPS HERE

	/* Get target field type */
	target_type = field_path_to_field_type(target_field_path, ctx);

	if (!target_type) {
		ret = -1;
		goto end;
	}

	/* Set target field path and target field type */
	if (type_id == CTF_TYPE_SEQUENCE) {
		ret = bt_ctf_field_type_sequence_set_length_field_path(
			type, target_field_path);

		if (ret) {
			goto end;
		}

		target_field_path = NULL;
	} else if (type_id == CTF_TYPE_VARIANT) {
		ret = bt_ctf_field_type_variant_set_tag_field_path(
			type, target_field_path);

		if (ret) {
			goto end;
		}

		target_field_path = NULL;

		ret = bt_ctf_field_type_variant_set_tag_field_type(
			type, target_type);

		if (ret) {
			goto end;
		}
	}

end:
	bt_ctf_field_path_destroy(target_field_path);
	bt_ctf_field_path_destroy(ctx_field_path);
	BT_PUT(target_type);

	return ret;
}

/*
 * `type` is owned by the caller.
 */
static
int resolve_type(struct bt_ctf_field_type *type, struct resolve_context *ctx)
{
	int ret = 0;
	int type_id;

	if (!type) {
		/* Type is not available; still valid */
		goto end;
	}

	type_id = bt_ctf_field_type_get_type_id(type);

	/* Resolve sequence/variant field type */
	switch (type_id) {
	case CTF_TYPE_SEQUENCE:
	case CTF_TYPE_VARIANT:
		ret = resolve_sequence_or_variant_type(type, ctx);

		if (ret) {
			goto end;
		}
		break;

	default:
		break;
	}

	/* Recurse */
	switch (type_id) {
	case CTF_TYPE_STRUCT:
	case CTF_TYPE_VARIANT:
	case CTF_TYPE_SEQUENCE:
	case CTF_TYPE_ARRAY:
	{
		int field_count, f_index;

		ret = type_stack_push(ctx->type_stack, type);

		if (ret) {
			goto end;
		}

		field_count = get_type_field_count(type);

		if (field_count < 0) {
			ret = field_count;
			goto end;
		}

		for (f_index = 0; f_index < field_count; ++f_index) {
			struct bt_ctf_field_type *child_type =
				get_type_field_at_index(type, f_index);

			if (!child_type) {
				ret = -1;
				goto end;
			}

			type_stack_peek(ctx->type_stack)->index = f_index;
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

static
int resolve_root_type(enum ctf_type_id root_node, struct resolve_context *ctx)
{
	int ret;

	assert(type_stack_size(ctx->type_stack) == 0);
	ctx->root_node = root_node;
	ret = resolve_type(get_type_from_ctx(ctx, root_node), ctx);
	ctx->root_node = CTF_NODE_UNKNOWN;

	return ret;
}

/*
 * All `*_type` parameters are owned by the caller.
 */
BT_HIDDEN
int bt_ctf_resolve_types(
		struct bt_ctf_field_type *packet_header_type,
		struct bt_ctf_field_type *packet_context_type,
		struct bt_ctf_field_type *event_header_type,
		struct bt_ctf_field_type *stream_event_ctx_type,
		struct bt_ctf_field_type *event_context_type,
		struct bt_ctf_field_type *event_payload_type,
		enum bt_ctf_resolve_flag flags)
{
	int ret = 0;
	struct resolve_context ctx = {0};

	return 0;

	/* Initialize resolving context */
	ctx.packet_header_type = packet_header_type;
	ctx.packet_context_type = packet_context_type;
	ctx.event_header_type = event_header_type;
	ctx.stream_event_ctx_type = stream_event_ctx_type;
	ctx.event_context_type = event_context_type;
	ctx.event_payload_type = event_payload_type;
	ctx.root_node = CTF_NODE_UNKNOWN;
	ctx.type_stack = type_stack_create();

	if (!ctx.type_stack) {
		ret = -1;
		goto end;
	}

	/* Resolve packet header type */
	if (flags & BT_CTF_RESOLVE_FLAG_PACKET_HEADER) {
		ret = resolve_root_type(CTF_NODE_TRACE_PACKET_HEADER, &ctx);

		if (ret) {
			goto end;
		}
	}

	/* Resolve packet context type */
	if (flags & BT_CTF_RESOLVE_FLAG_PACKET_CONTEXT) {
		ret = resolve_root_type(CTF_NODE_STREAM_PACKET_CONTEXT, &ctx);

		if (ret) {
			goto end;
		}
	}

	/* Resolve event header type */
	if (flags & BT_CTF_RESOLVE_FLAG_EVENT_HEADER) {
		ret = resolve_root_type(CTF_NODE_STREAM_EVENT_HEADER, &ctx);

		if (ret) {
			goto end;
		}
	}

	/* Resolve stream event context type */
	if (flags & BT_CTF_RESOLVE_FLAG_STREAM_EVENT_CTX) {
		ret = resolve_root_type(CTF_NODE_STREAM_EVENT_CONTEXT, &ctx);

		if (ret) {
			goto end;
		}
	}

	/* Resolve event context type */
	if (flags & BT_CTF_RESOLVE_FLAG_EVENT_CONTEXT) {
		ret = resolve_root_type(CTF_NODE_EVENT_CONTEXT, &ctx);

		if (ret) {
			goto end;
		}
	}

	/* Resolve packet header type */
	if (flags & BT_CTF_RESOLVE_FLAG_EVENT_PAYLOAD) {
		ret = resolve_root_type(CTF_NODE_EVENT_FIELDS, &ctx);

		if (ret) {
			goto end;
		}
	}

end:
	type_stack_destroy(ctx.type_stack);

	return ret;
}
