/*
 * visitor.c
 *
 * Babeltrace CTF IR - Trace Visitor
 *
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Author: Jérémie Galarneau <jeremie.galarneau@efficios.com>
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
#include <babeltrace/ctf-ir/visitor-internal.h>
#include <babeltrace/ctf-ir/event-types-internal.h>
#include <babeltrace/ctf-ir/event-internal.h>
#include <babeltrace/ref.h>
#include <babeltrace/babeltrace-internal.h>

/* TSDL dynamic scope prefixes defined in CTF Section 7.3.2 */
static const char * const absolute_path_prefixes[] = {
	[CTF_NODE_ENV] = "env.",
	[CTF_NODE_TRACE_PACKET_HEADER] = "trace.packet.header.",
	[CTF_NODE_STREAM_PACKET_CONTEXT] = "stream.packet.context.",
	[CTF_NODE_STREAM_EVENT_HEADER] = "stream.event.header.",
	[CTF_NODE_STREAM_EVENT_CONTEXT] = "stream.event.context.",
	[CTF_NODE_EVENT_CONTEXT] = "event.context.",
	[CTF_NODE_EVENT_FIELDS] = "event.fields.",
};

const int absolute_path_prefix_token_counts[] = {
	[CTF_NODE_ENV] = 1,
	[CTF_NODE_TRACE_PACKET_HEADER] = 3,
	[CTF_NODE_STREAM_PACKET_CONTEXT] = 3,
	[CTF_NODE_STREAM_EVENT_HEADER] = 3,
	[CTF_NODE_STREAM_EVENT_CONTEXT] = 3,
	[CTF_NODE_EVENT_CONTEXT] = 2,
	[CTF_NODE_EVENT_FIELDS] = 2,
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

struct ctf_type_stack_frame {
	struct bt_ctf_field_type *type;
	int index;
};

typedef GPtrArray ctf_type_stack;

struct ctf_type_visitor_context {
	struct bt_ctf_trace *trace;
	struct bt_ctf_stream_class *stream_class;
	struct bt_ctf_event_class *event_class;
	/* Root node being visited */
	enum bt_ctf_node root_node;
	ctf_type_stack *stack;
};

typedef int (*ctf_type_visitor_func)(struct bt_ctf_field_type *,
	struct ctf_type_visitor_context *);

static
int field_type_visit(struct bt_ctf_field_type *type,
		struct ctf_type_visitor_context *context,
		ctf_type_visitor_func func);

static
int field_type_recursive_visit(struct bt_ctf_field_type *type,
		struct ctf_type_visitor_context *context,
		ctf_type_visitor_func func);

static inline
int get_type_field_count(struct bt_ctf_field_type *type)
{
	int field_count = -1;
	enum ctf_type_id type_id = bt_ctf_field_type_get_type_id(type);

	if (type_id == CTF_TYPE_STRUCT) {
		field_count = bt_ctf_field_type_structure_get_field_count(type);
	} else if (type_id == CTF_TYPE_VARIANT) {
k		field_count = bt_ctf_field_type_variant_get_field_count(type);
	} else if (type_id == CTF_TYPE_ARRAY || type_id == CTF_TYPE_SEQUENCE) {
		/*
		 * Array and sequence types always contain a single member
		 * (the element type).
		 */
		field_count = 1;
	}
	return field_count;
}

static inline
struct bt_ctf_field_type *get_type_field(struct bt_ctf_field_type *type, int i)
{
	struct bt_ctf_field_type *field = NULL;
	enum ctf_type_id type_id = bt_ctf_field_type_get_type_id(type);

	if (type_id == CTF_TYPE_STRUCT) {
		bt_ctf_field_type_structure_get_field(type, NULL,
			&field, i);
	} else if (type_id == CTF_TYPE_VARIANT) {
		bt_ctf_field_type_variant_get_field(type,
			NULL, &field, i);
	} else if (type_id == CTF_TYPE_ARRAY) {
		field = bt_ctf_field_type_array_get_element_type(type);
	} else if (type_id == CTF_TYPE_SEQUENCE) {
		field = bt_ctf_field_type_sequence_get_element_type(type);
	}

	return field;
}

static inline
int set_type_field(struct bt_ctf_field_type *type,
		struct bt_ctf_field_type *field, int i)
{
	int ret = -1;
	enum ctf_type_id type_id = bt_ctf_field_type_get_type_id(type);

	if (type_id == CTF_TYPE_STRUCT) {
		ret = bt_ctf_field_type_structure_set_field_index(
			type, field, i);
	} else if (type_id == CTF_TYPE_VARIANT) {
		ret = bt_ctf_field_type_variant_set_field_index(
			type, field, i);
	} else if (type_id == CTF_TYPE_ARRAY) {
		ret = bt_ctf_field_type_array_set_element_type(type, field);
	} else if (type_id == CTF_TYPE_SEQUENCE) {
		ret = bt_ctf_field_type_sequence_set_element_type(type, field);
	}

	return ret;
}

static inline
int get_type_field_index(struct bt_ctf_field_type *type, const char *name)
{
	int field_index = -1;
	enum ctf_type_id type_id = bt_ctf_field_type_get_type_id(type);

	if (type_id == CTF_TYPE_STRUCT) {
		field_index = bt_ctf_field_type_structure_get_field_name_index(
			type, name);
	} else if (type_id == CTF_TYPE_VARIANT) {
		field_index = bt_ctf_field_type_variant_get_field_name_index(
			type, name);
	}

	return field_index;
}

static
void stack_destroy_notify(gpointer data)
{
	struct ctf_type_stack_frame *frame = data;

	BT_PUT(frame->type);
	g_free(frame);
}

ctf_type_stack *ctf_type_stack_create(void)
{
	return g_ptr_array_new_with_free_func(stack_destroy_notify);
}

static
void ctf_type_stack_destroy(ctf_type_stack *stack)
{
	g_ptr_array_free(stack, TRUE);
}

static
int ctf_type_stack_push(ctf_type_stack *stack,
		struct bt_ctf_field_type *type)
{
	int ret = 0;
	struct ctf_type_stack_frame *frame = NULL;

	if (!stack || !frame) {
		ret = -1;
		goto end;
	}

	frame = g_new0(struct ctf_type_stack_frame, 1);

	if (!frame) {
		ret = -1;
		goto end;
	}

	frame->type = type;
	bt_get(entry->type);
	g_ptr_array_add(stack, entry);

end:
	return ret;
}

static
bool ctf_type_stack_empty(ctf_type_stack *stack)
{
	return stack->len == 0;
}

static
struct ctf_type_stack_frame *ctf_type_stack_peek(ctf_type_stack *stack)
{
	struct ctf_type_stack_frame *entry = NULL;

	if (!stack || ctf_type_stack_empty(stack)) {
		goto end;
	}

	entry = g_ptr_array_index(stack, stack->len - 1);
end:
	return entry;
}

static
struct ctf_type_stack_frame *ctf_type_stack_at(ctf_type_stack *stack,
	size_t index)
{
	struct ctf_type_stack_frame *entry = NULL;

	if (!stack || index >= stack->len) {
		goto end;
	}

	entry = g_ptr_array_index(stack, index);

end:
	return entry;
}

static
void ctf_type_stack_pop(ctf_type_stack *stack)
{
	if (!ctf_type_stack_empty(stack)) {
		/*
		 * This will call the frame's destructor and free it, as
		 * well as put its contained field type.
		 */
		g_ptr_array_set_size(stack, stack->len - 1);
	}
}

static
int field_type_visit(struct bt_ctf_field_type *type,
		struct ctf_type_visitor_context *context,
		ctf_type_visitor_func func)
{
	int ret;
	enum ctf_type_id type_id;
	struct bt_ctf_field_type *top_type = NULL;
	struct ctf_type_stack_frame *frame = NULL;

	ret = func(type, context);
	if (ret) {
		goto end;
	}

	/*
	 * Original type could have been copied and replaced by func(),
	 * so get it again from the stack's top frame.
	 */
	if (!ctf_type_stack_empty(context->stack)) {
		/*
		 * There's at least one frame, so we're not visiting the
		 * root field type frame here.
		 *
		 * `type` was passed as a weak reference, whereas
		 * get_type_field() returns a new reference here, so
		 * `top_type` is put at the end of this function.
		 */
		top_type = get_type_field(frame->type, frame->index);
		assert(top_type);
		type = top_type;
	}

	type_id = bt_ctf_field_type_get_type_id(type);

	if (type_id != CTF_TYPE_STRUCT &&
		type_id != CTF_TYPE_VARIANT &&
		type_id != CTF_TYPE_ARRAY &&
		type_id != CTF_TYPE_SEQUENCE) {
		/* No need to create a new stack frame */
		goto end;
	}

	ret = ctf_type_stack_push(context->stack, type);
	if (ret) {
		g_free(frame);
		goto end;
	}
end:
	BT_PUT(top_type);

	return ret;
}

static
int field_type_dynamic_scope_visit(struct bt_ctf_field_type *type,
		struct ctf_type_visitor_context *context,
		ctf_type_visitor_func func)
{
	int ret = 0;
	struct bt_ctf_field_type *top_type = NULL;

	assert(bt_ctf_field_type_is_structure(type));

	/* Visit root field type */
	ret =  (type, context, func);

	if (ret) {
		goto end;
	}

	while (true) {
		struct bt_ctf_field_type *field;
		struct ctf_type_stack_frame *entry =
			ctf_type_stack_peek(context->stack);
		int field_count = get_type_field_count(entry->type);

		if (field_count <= 0 &&
				!bt_ctf_field_type_is_structure(entry->type)) {
			/*
			 * Propagate error if one was given, else return
			 * -1 since empty variants are invalid
			 * at this point.
			 */
			ret = field_count < 0 ? field_count : -1;
			goto end;
		}

		if (entry->index == field_count) {
			/* This level has been completely visited */
			ctf_type_stack_pop(context->stack);

			if (ctf_type_stack_empty(context->stack)) {
				/* Completed visit */
				break;
			} else {
				continue;
			}
		}

		field = get_type_field(entry->type, entry->index);

		/*
		 * field_type_visit() will push a new frame onto
		 * the stack if the visited type is a compound type.
		 * In this case, the field type (`field` variable)
		 * passed as a weak reference will be taken (new
		 * reference) when pushed onto the stack, or a copy
		 * will be made and pushed (in the case of a variant
		 * or sequence type) because its tag/length type will
		 * be modified as it is resolved. We want to leave the
		 * original field type as is.
		 */
		ret = field_type_visit(field, context, func);
		BT_PUT(field);
		if (ret) {
			goto end;
		}

		entry->index++;
	}
end:
	BT_PUT(top_type);

	return ret;
}

static
int bt_ctf_event_class_visit(struct bt_ctf_event_class *event_class,
		struct bt_ctf_trace *trace,
		struct bt_ctf_stream_class *stream_class,
		ctf_type_visitor_func func)
{
	int ret = 0;
	struct bt_ctf_field_type *type;
	struct ctf_type_visitor_context context = { 0 };

	if (!event_class || !func) {
		ret = -1;
		goto end;
	}

	context.trace = trace;
	context.stream_class = stream_class;
	context.event_class = event_class;
	context.stack = ctf_type_stack_create();
	if (!context.stack) {
		ret = -1;
		goto end;
	}

	/* Visit event context */
	context.root_node = CTF_NODE_EVENT_CONTEXT;
	type = bt_ctf_event_class_get_context_type(event_class);
	if (type) {
		ret = field_type_recursive_visit(type, &context, func);
		bt_ctf_field_type_put(type);
		type = NULL;
		if (ret) {
			goto end;
		}
	}

	/* Visit event payload */
	context.root_node = CTF_NODE_EVENT_FIELDS;
	type = bt_ctf_event_class_get_payload_type(event_class);
	if (type) {
		ret = field_type_recursive_visit(type, &context, func);
		bt_ctf_field_type_put(type);
		type = NULL;
		if (ret) {
			goto end;
		}
	}
end:
	if (context.stack) {
		ctf_type_stack_destroy(context.stack);
	}
	return ret;
}

static
int bt_ctf_stream_class_visit(struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_trace *trace,
		ctf_type_visitor_func func)
{
	int ret = 0;
	struct bt_ctf_field_type *type;
	struct ctf_type_visitor_context context = { 0 };

	if (!stream_class || !func) {
		ret = -1;
		goto end;
	}

	context.trace = trace;
	context.stream_class = stream_class;
	context.stack = ctf_type_stack_create();
	if (!context.stack) {
		ret = -1;
		goto end;
	}

	/* Visit stream packet context header */
	context.root_node = CTF_NODE_STREAM_PACKET_CONTEXT;
	type = bt_ctf_stream_class_get_packet_context_type(stream_class);
	if (type) {
		ret = field_type_recursive_visit(type, &context, func);
		bt_ctf_field_type_put(type);
		type = NULL;
		if (ret) {
			goto end;
		}
	}

	/* Visit stream event header */
	context.root_node = CTF_NODE_STREAM_EVENT_HEADER;
	type = bt_ctf_stream_class_get_event_header_type(stream_class);
	if (type) {
		ret = field_type_recursive_visit(type, &context, func);
		bt_ctf_field_type_put(type);
		type = NULL;
		if (ret) {
			goto end;
		}
	}

	/* Visit stream event context */
	context.root_node = CTF_NODE_STREAM_EVENT_CONTEXT;
	type = bt_ctf_stream_class_get_event_context_type(stream_class);
	if (type) {
		ret = field_type_recursive_visit(type, &context, func);
		bt_ctf_field_type_put(type);
		type = NULL;
		if (ret) {
			goto end;
		}
	}

end:
	if (context.stack) {
		ctf_type_stack_destroy(context.stack);
	}
	return ret;
}

static
void current_stack_to_absolute_path(struct ctf_type_visitor_context *context,
		struct bt_ctf_field_path *field_path)
{
	size_t i;

	/* Set field path's root node: our current root node */
	field_path->root = context->root_node;

	/* Make sure the path is empty */
	g_array_set_size(field_path->path_indexes, 0);

	/*
	 * The context stack is an array of
	 * (base type, index in this type) tuples. The position of the
	 * requesting type in this stack is the field type at the
	 * index in the base type of the top-most stack frame. If we
	 * are here, this field type should be either a sequence type
	 * or a variant type. What we want is the path leading to the
	 * parent of the current field type; in other words, all the
	 * indexes in the stack but the last one.
	 *
	 * Example:
	 *
	 *               4            0             2
	 *     [struct] ---> [array] ---> [struct] ---> [variant]
	 *         ^                          ^             ^
	 *        root of             what we're           requesting
	 *        dynamic scope      looking for           type
	 *
	 *     Stack length: 3 (number of arrows)
	 *     Path indexes: [4, 0]
	 */
	assert(!ctf_type_stack_empty(context->stack));

	for (i = 0; i < context->stack->len - 1; ++i) {
		struct ctf_type_stack_frame *frame =
			ctf_type_stack_peek(context->stack);

		g_array_append_val(field_path->path_indexes, frame->index);
	}
}

static
int relative_tokens_to_absolute_path(struct ctf_type_visitor_context *context,
	struct bt_ctf_field_path *field_path, GList **path_tokens)
{
	int ret = 0;
	struct bt_ctf_field_type *field_type = NULL;
	struct ctf_type_stack_frame *top_frame =
		ctf_type_stack_peek(context->stack);

	if (!top_frame) {
		ret = -1;
		goto end;
	}

	field_type = top_frame->type;
	bt_ctf_field_type_get(field_type);

	/* Initialize field path from current context's stack */
	current_stack_to_absolute_path(context, field_path);

	for (i = 0; i < token_count; i++) {
		struct bt_ctf_field_type *next_field_type = NULL;
		enum ctf_type_id type_id =
			bt_ctf_field_type_get_type_id(field_type);
		int field_index;

		/*
		 * We're only looking into the same level as the
		 * requesting type's parent here. Therefore array and
		 * sequence types are NOT permitted.
		 */
		if (type_id == CTF_TYPE_ARRAY || type_id == CTF_TYPE_SEQUENCE) {
			ret = -1;
			goto end;
		}

		field_index = get_type_field_index(field_type,
			(*path_tokens)->data);

		if (field_index < 0) {
			/* Field name not found, abort */
			printf_verbose("Could not resolve field \"%s\"\n",
				(char *) (*path_tokens)->data);
			ret = -1;
			goto end;
		}

		next_field_type = get_type_field(field_type, field_index);

		if (!next_field_type) {
			ret = -1;
			goto end;
		}

		BT_PUT(field_type);
		BT_MOVE(field_type, next_field_type);
		g_array_append_val(field_path->path_indexes, field_index);

		/*
		 * Free token and remove from list. This function does not
		 * assume the ownership of path_tokens; it is therefore _not_
		 * a leak to leave elements in this list. The caller should
		 * clean-up what is left (in case of error).
		 */
		free((*path_tokens)->data);
		*path_tokens = g_list_delete_link(*path_tokens, *path_tokens);
	}

end:
	BT_PUT(field_type);

	return ret;
}

static
int absolute_tokens_to_absolute_path(struct ctf_type_visitor_context *context,
	struct bt_ctf_field_path *field_path, GList **path_tokens)
{
	int ret = 0;
	struct bt_ctf_field_type *field_type = NULL;
	struct ctf_type_stack_frame *top_frame =
		ctf_type_stack_peek(context->stack);

	if (!top_frame) {
		ret = -1;
		goto end;
	}

	field_type = top_frame->type;
	bt_ctf_field_type_get(field_type);

	/* Initialize field path from current context's stack */
	current_stack_to_absolute_path(context, field_path);

	for (i = 0; i < token_count; i++) {
		struct bt_ctf_field_type *next_field_type = NULL;
		enum ctf_type_id type_id =
			bt_ctf_field_type_get_type_id(field_type);
		int field_index;

		/*
		 * We're only looking into the same level as the
		 * requesting type's parent here. Therefore array and
		 * sequence types are NOT permitted.
		 */
		if (type_id == CTF_TYPE_ARRAY || type_id == CTF_TYPE_SEQUENCE) {
			ret = -1;
			goto end;
		}

		field_index = get_type_field_index(field_type,
			(*path_tokens)->data);

		if (field_index < 0) {
			/* Field name not found, abort */
			printf_verbose("Could not resolve field \"%s\"\n",
				(char *) (*path_tokens)->data);
			ret = -1;
			goto end;
		}

		next_field_type = get_type_field(field_type, field_index);

		if (!next_field_type) {
			ret = -1;
			goto end;
		}

		BT_PUT(field_type);
		BT_MOVE(field_type, next_field_type);
		g_array_append_val(field_path->path_indexes, field_index);

		/*
		 * Free token and remove from list. This function does not
		 * assume the ownership of path_tokens; it is therefore _not_
		 * a leak to leave elements in this list. The caller should
		 * clean-up what is left (in case of error).
		 */
		free((*path_tokens)->data);
		*path_tokens = g_list_delete_link(*path_tokens, *path_tokens);
	}

end:
	BT_PUT(field_type);

	return ret;
}

static
int get_lca_index(struct ctf_type_stack *context_stack,
		struct ctf_type_stack *path_stack,
		int *lca_index)
{
	int i;
	int ret = 0;
	struct ctf_type_stack_frame *frame = NULL;
	struct bt_ctf_field_type *stack_field_type = NULL;
	struct bt_ctf_field_type *path_field_type = NULL;

	for (i = 0; i < path_stack->len; ++i) {
		struct ctf_type_stack_frame *context_frame;
		struct ctf_type_stack_frame *path_frame;

		/*
		 * If we're now at the end of one of the stacks,

		context_frame = ctf_type_stack_at(context_stack, i);
		path_frame = ctf_type_stack_at(path_stack, i);

		/* Compare current types */
		if (path_field_type != stack_field_type) {
			/*
			 * Those types are not the same; therefore the
			 * LCA is their parent. lca_index will never be
			 * set to -1 here because the root types should
			 * always be the same when calling this
			 * function.
			 */
			*lca_index = i - 1;
			goto end;
		}

		/* Get current stack's field type */
		if (i == context->stack->len) {
			/*
			 * We reached the end of the stack. Therefore
			 * the lowest common ancestor is `i`.
			 */
			*lca_index = i;
			goto end;
		}

		stack_field_type =

		/* Get current path's field type */
		path_field_type = get_type_field(field_type, index);
		assert(path_field_type);
		BT_PUT(field_type);
		BT_MOVE(field_type, path_field_type);

		continue;

error:
		BT_PUT(field_type);
		BT_PUT(stack_field_type);
		ret = -1;
		goto end;
	}

end:
	return ret;
}

static
struct bt_ctf_field_type *root_field_type_from_node(
		struct ctf_type_visitor_context *context,
		enum bt_ctf_node node)
{
	struct bt_ctf_field_type *root_field_type = NULL;

	switch (node) {
	case CTF_NODE_TRACE_PACKET_HEADER:
		if (!context->trace) {
			ret = -1;
			goto end;
		}

		root_field_type =
			bt_ctf_trace_get_packet_header_type(context->trace);
		break;
	case CTF_NODE_STREAM_PACKET_CONTEXT:
		if (!context->stream_class) {
			ret = -1;
			goto end;
		}

		root_field_type =
			bt_ctf_stream_class_get_packet_context_type(
				context->stream_class);
		break;
	case CTF_NODE_STREAM_EVENT_HEADER:
		if (!context->stream_class) {
			ret = -1;
			goto end;
		}

		root_field_type = bt_ctf_stream_class_get_event_header_type(
			context->stream_class);
		break;
	case CTF_NODE_STREAM_EVENT_CONTEXT:
		if (!context->stream_class) {
			ret = -1;
			goto end;
		}

		root_field_type =
			bt_ctf_stream_class_get_event_context_type(
				context->stream_class);
		break;
	case CTF_NODE_EVENT_CONTEXT:
		if (!context->event_class) {
			ret = -1;
			goto end;
		}

		root_field_type = bt_ctf_event_class_get_context_type(
			context->event_class);
		break;
	case CTF_NODE_EVENT_FIELDS:
		if (!context->event_class) {
			ret = -1;
			goto end;
		}

		root_field_type = bt_ctf_event_class_get_payload_type(
			context->event_class);
		break;
	default:
		ret = -1;
		goto end;
	}

end:
	return root_field_type;
}

static
int set_field_path_absolute(struct ctf_type_visitor_context *context,
		struct bt_ctf_field_path *field_path,
		GList **path_tokens, struct bt_ctf_field_type **resolved_field)
{
	int ret = 0;
	struct bt_ctf_field_type *resolved_field_type = NULL;
	struct bt_ctf_field_type *root_field_type = NULL;
	size_t token_count = g_list_length(*path_tokens), i;
	struct ctf_type_stack path_stack;

	if (field_path->root > context->root_node) {
		/*
		 * The target path's root is lower in the dynamic scope
		 * hierarchy than the current field being visited. This
		 * is invalid since it would not be possible to have read
		 * the target before the current field.
		 */
		ret = -1;
		printf_verbose("The target path's root is lower in the dynamic scope than the current field.\n");
		goto end;
	}

	/* Set the appropriate root field type */
	root_field_type = root_field_type_from_node(context, field_path->root);

	if (!root_field_type) {
		ret = -1;
		goto end;
	}

	/*
	 * Create a temporary stack which will hold the field types
	 * and indexes leading to the field type to lookup.
	 */
	path_stack = ctf_type_stack_create();

	if (!path_stack) {
		ret = -1;
		goto end;
	}

	/* Push root field type */
	ret = ctf_type_stack_push(path_stack, root_field_type);
	BT_PUT(root_field_type);

	if (ret) {
		goto end;
	}

	for (i = 0; i < token_count; i++) {
		int field_index;
		enum ctf_type_id type_id;
		struct bt_ctf_field_type *cur_field_type = NULL;
		struct ctf_type_stack_frame *frame;

		frame = ctf_type_stack_peek(path_stack);
		assert(bt_ctf_field_type_is_structure(frame->type) ||
			bt_ctf_field_type_is_variant(frame->type));
		frame->index = get_type_field_index(frame->type,
			(*path_tokens)->data);

		if (frame->index < 0) {
			/* Field name not found, abort */
			printf_verbose("Could not resolve field \"%s\"\n",
				(char *) (*path_tokens)->data);
			ret = -1;
			goto end;
		}

		cur_field_type = get_type_field(frame->type, frame->index);

		if (!cur_field_type) {
			ret = -1;
			goto end;
		}

		/* Get current base's type ID */
		type_id = bt_ctf_field_type_get_type_id(cur_field_type);

		/*
		 * Array and sequence types have no path tokens. Find
		 * the next non-array, non-sequence field type.
		 */
		while (type_id == CTF_TYPE_ARRAY ||
				type_id == CTF_TYPE_SEQUENCE) {
			ret = ctf_type_stack_push(path_stack, cur_field_type);
			BT_PUT(cur_field_type);

			if (ret) {
				goto end;
			}

			frame = ctf_type_stack_peek(path_stack);
			cur_field_type = get_type_field(frame->type, 0);
			type_id = bt_ctf_field_type_get_type_id(cur_field_type);
		}

		if (type_id == CTF_TYPE_STRUCT || type_id == CTF_TYPE_VARIANT) {
			/*
			 * The current field type is a compound type.
			 * Therefore we must NOT be looking at the last
			 * path token.
			 */
			if (i == (token_count - 1)) {
				BT_PUT(cur_field_type);
				ret = -1;
				goto end;
			}

			ret = ctf_type_stack_push(path_stack, cur_field_type);
			BT_PUT(cur_field_type);

			if (ret) {
				goto end;
			}
		} else {
			/*
			 * The current field type is a basic type.
			 * Therefore we must be looking at the last
			 * path token.
			 */
			if (i != (token_count - 1)) {
				BT_PUT(cur_field_type);
				ret = -1;
				goto end;
			}
		}

		BT_PUT(cur_field_type);

		/*
		 * Free token and remove from list. This function does not
		 * assume the ownership of path_tokens; it is therefore _not_
		 * a leak to leave elements in this list. The caller should
		 * clean-up what is left (in case of error).
		 */
		free((*path_tokens)->data);
		*path_tokens = g_list_delete_link(*path_tokens, *path_tokens);
	}

	assert(!ctf_type_stack_empty(context->stack));

	if (g_ptr_array_index(context->stack, 0) == root_field_type) {
		/*
		 * The field type to lookup is in the dynamic scope
		 * currently being visited. In this case, we have to
		 * make sure that the looked up field type is not
		 * located _after_ the field type requesting it
		 * (sequence/variant type).
		 *
		 * Array and sequence types may be on the path in this
		 * situation, as long as they are also on the path to
		 * our stack's top frame's field type.
		 *
		 * Invalid example:
		 *
		 *     struct
		 *       array
		 *         struct
		 *           int <-------.
		 *           int         |
		 *       struct          |
		 *         array         |
		 *           struct      |
		 *             sequence -`
		 *               float
		 *
		 * Valid example:
		 *
		 *     struct
		 *       array
		 *         struct
		 *           int
		 *           int
		 *       struct
		 *         array
		 *           struct
		 *             int <-----.
		 *             string    |
		 *             sequence -`
		 *               float
		 */
		struct bt_ctf_field_type *field_type = root_field_type;
		size_t stack_at = 0;

		bt_get(field_type);

		for (i = 0; i < field_path->path_indexes->len; ++i) {
			struct bt_ctf_field_type *stack_field_type = NULL;
			struct bt_ctf_field_type *path_field_type = NULL;
			int index = g_array_index(field_path->path_indexes,
				int, i);

			/* Get current stack's field type */
			if (stack_at == context->stack->len) {
				/*
				 *
				ret = -1;
				goto error;
			}

			/* Get current path's field type */
			path_field_type = get_type_field(field_type, index);
			assert(path_field_type);
			BT_PUT(field_type);
			BT_MOVE(field_type, path_field_type);

			continue;

error:
			BT_PUT(field_type);
			BT_PUT(stack_field_type);
			ret = -1;
			goto end;
		}
	} else {
		/*
		 * The field type to lookup is located _before_ the
		 * dynamic scope currently being visited. In this case,
		 * arrays and sequences _cannot_ be on the path.
		 */
		stack_at = -1;
	}


end:
	if (path_stack) {
		ctf_type_stack_destroy(path_stack);
	}

	BT_PUT(root_field_type);

	if (ret == 0) {
		*resolved_field = resolved_field_type;
	}

	BT_PUT(resolved_field_type);

	return ret;
}

static
int get_field_path(struct ctf_type_visitor_context *context,
		const char *path, struct bt_ctf_field_path **field_path,
		struct bt_ctf_field_type **resolved_field)
{
	int i, ret = 0;
	GList *path_tokens = NULL;
	char *name_copy, *save_ptr, *token;

	/* Tokenize path to a list of strings */
	name_copy = strdup(path);
	if (!name_copy) {
		goto error;
	}

	token = strtok_r(name_copy, ".", &save_ptr);
	while (token) {
		char *token_string = strdup(token);

		if (!token_string) {
			ret = -1;
			goto error;
		}
		path_tokens = g_list_append(path_tokens, token_string);
		token = strtok_r(NULL, ".", &save_ptr);
	}

	if (!path_tokens) {
		ret = -1;
		goto error;
	}

	*field_path = bt_ctf_field_path_create();
	if (!*field_path) {
		ret = -1;
		goto error;
	}

	/* Check if the path is absolute */
	for (i = 0; i < sizeof(absolute_path_prefixes) / sizeof(char *); i++) {
		int j;

		/*
		 * Chech if "path" starts with a known absolute path prefix.
		 * Refer to CTF 7.3.2 STATIC AND DYNAMIC SCOPES.
		 */
		if (strncmp(path, absolute_path_prefixes[i],
			strlen(absolute_path_prefixes[i]))) {
			/* Wrong prefix, try the next one */
			continue;
		}

		/*
		 * Remove the first n tokens of this prefix.
		 * e.g. trace.packet.header: remove the first 3 tokens.
		 */
		for (j = 0; j < absolute_path_prefix_token_counts[i]; j++) {
			free(path_tokens->data);
			path_tokens = g_list_delete_link(
				path_tokens, path_tokens);
		}

		/* i maps to enum bt_ctf_node constants */
		(*field_path)->root = (enum bt_ctf_node) i;
		break;
	}

	if ((*field_path)->root == CTF_NODE_UNKNOWN) {
		/* Relative path */
		ret = set_field_path_relative(context,
			*field_path, &path_tokens, resolved_field);
		if (ret) {
			goto error;
		}
	} else {
		/* Absolute path */
		ret = set_field_path_absolute(context,
			*field_path, &path_tokens, resolved_field);
		if (ret) {
			goto error;
		}
	}
end:
	if (name_copy) {
		g_free(name_copy);
	}
	if (path_tokens) {
		g_list_free_full(path_tokens, free);
	}
	return ret;
error:
	if (*field_path) {
		bt_ctf_field_path_destroy(*field_path);
		*field_path = NULL;
	}
	goto end;
}

void print_path(const char *field_name,
		struct bt_ctf_field_type *resolved_type,
		struct bt_ctf_field_path *field_path)
{
	int i;
	enum ctf_type_id type_id = bt_ctf_field_type_get_type_id(
		resolved_type);

	if (type_id < CTF_TYPE_UNKNOWN || type_id >= NR_CTF_TYPES) {
		type_id = CTF_TYPE_UNKNOWN;
	}

	printf_verbose("Resolved field \"%s\" as type \"%s\", ",
		field_name, type_names[type_id]);
	printf_verbose("path: %s",
		absolute_path_prefixes[field_path->root]);

	for (i = 0; i < field_path->path_indexes->len; i++) {
		printf_verbose(" %d",
			g_array_index(field_path->path_indexes, int, i));
	}
	printf_verbose("\n");
}

static
int type_resolve_func(struct bt_ctf_field_type *type,
		struct ctf_type_visitor_context *context)
{
	int ret = 0;
	enum ctf_type_id type_id = bt_ctf_field_type_get_type_id(type);
	const char *field_name = NULL;
	struct bt_ctf_field_path *field_path = NULL;
	struct bt_ctf_field_type *resolved_type = NULL;
	struct bt_ctf_field_type *type_copy = NULL;
	struct ctf_type_stack_frame *frame;

	if (type_id != CTF_TYPE_SEQUENCE &&
		type_id != CTF_TYPE_VARIANT) {
		goto end;
	}

	field_name = type_id == CTF_TYPE_SEQUENCE ?
		bt_ctf_field_type_sequence_get_length_field_name(type) :
		bt_ctf_field_type_variant_get_tag_name(type);
	if (!field_name) {
		ret = -1;
		goto end;
	}

	ret = get_field_path(context, field_name,
		&field_path, &resolved_type);
	if (ret) {
		goto end;
	}

	assert(field_path && resolved_type);

	/* Print path if in verbose mode */
	print_path(field_name, resolved_type, field_path);

	/*
	 * Set field type's path.
	 *
	 * The original field is copied since it may have been reused
	 * in multiple structures which would cause a conflict.
	 */
	type_copy = bt_ctf_field_type_copy(type);
	if (!type_copy) {
		ret = -1;
		goto end;
	}

	if (type_id == CTF_TYPE_VARIANT) {
		if (bt_ctf_field_type_get_type_id(resolved_type) !=
			CTF_TYPE_ENUM) {
			printf_verbose("Invalid variant tag \"%s\"; expected enum\n", field_name);
			ret = -1;
			goto end;
		}
		ret = bt_ctf_field_type_variant_set_tag(
			type_copy, resolved_type);
		if (ret) {
			goto end;
		}

		ret = bt_ctf_field_type_variant_set_tag_field_path(type_copy,
			field_path);
		if (ret) {
			goto end;
		}
	} else {
		/* Sequence */
		if (bt_ctf_field_type_get_type_id(resolved_type) !=
			CTF_TYPE_INTEGER) {
			printf_verbose("Invalid sequence length field \"%s\"; expected integer\n", field_name);
			ret = -1;
			goto end;
		}

		if (bt_ctf_field_type_integer_get_signed(resolved_type) != 0) {
			printf_verbose("Invalid sequence length field \"%s\"; integer should be unsigned\n", field_name);
			ret = -1;
			goto end;
		}

		ret = bt_ctf_field_type_sequence_set_length_field_path(
			type_copy, field_path);
		if (ret) {
			goto end;
		}
	}

	/*
	 * A copy was made. Replace the original field type in the
	 * current stack top's frame's type (the compound type
	 * containing the original field type).
	 */
	frame = ctf_type_stack_peek(context->stack);
	ret = set_type_field(frame->type, type_copy, frame->index);
	bt_ctf_field_type_put(type_copy);
end:
	return ret;
}

int bt_ctf_trace_visit(struct bt_ctf_trace *trace,
		ctf_type_visitor_func func)
{
	int ret = 0;
	struct bt_ctf_field_type *type = NULL;
	struct ctf_type_visitor_context visitor_ctx = { 0 };

	if (!trace || !func) {
		ret = -1;
		goto end;
	}

	visitor_ctx.trace = trace;
	visitor_ctx.stack = ctf_type_stack_create();
	if (!visitor_ctx.stack) {
		ret = -1;
		goto end;
	}

	/* Visit trace packet header */
	type = bt_ctf_trace_get_packet_header_type(trace);
	if (type) {
		visitor_ctx.root_node = CTF_NODE_TRACE_PACKET_HEADER;
		ret = field_type_recursive_visit(type, &visitor_ctx, func);
		visitor_ctx.root_node = CTF_NODE_UNKNOWN;
		bt_ctf_field_type_put(type);
		type = NULL;
		if (ret) {
			goto end;
		}
	}

end:
	if (visitor_ctx.stack) {
		ctf_type_stack_destroy(visitor_ctx.stack);
	}
	return ret;
}

/*
BT_HIDDEN
int bt_ctf_trace_resolve_types(struct bt_ctf_trace *trace)
{
	return bt_ctf_trace_visit(trace, type_resolve_func);
}

BT_HIDDEN
int bt_ctf_stream_class_resolve_types(struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_trace *trace)
{
	return bt_ctf_stream_class_visit(stream_class, trace,
		type_resolve_func);
}

BT_HIDDEN
int bt_ctf_event_class_resolve_types(struct bt_ctf_event_class *event_class,
		struct bt_ctf_trace *trace,
		struct bt_ctf_stream_class *stream_class)
{
	return bt_ctf_event_class_visit(event_class, trace, stream_class,
		type_resolve_func);
}
*/

BT_HIDDEN
int bt_ctf_trace_resolve_types(
		struct bt_ctf_field_type *packet_header_type)
{
	return 0;
}

BT_HIDDEN
int bt_ctf_stream_class_resolve_types(
		struct bt_ctf_field_type *packet_header_type,
		struct bt_ctf_field_type *packet_context_type,
		struct bt_ctf_field_type *event_header_type,
		struct bt_ctf_field_type *stream_event_ctx_type)
{
	return 0;
}

BT_HIDDEN
int bt_ctf_event_class_resolve_types(
		struct bt_ctf_field_type *packet_header_type,
		struct bt_ctf_field_type *packet_context_type,
		struct bt_ctf_field_type *event_header_type,
		struct bt_ctf_field_type *stream_event_ctx_type,
		struct bt_ctf_field_type *event_context_type,
		struct bt_ctf_field_type *event_payload_type)
{
	return 0;
}
