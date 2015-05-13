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
		field_count = bt_ctf_field_type_variant_get_field_count(type);
	}
	return field_count;
}

static inline
struct bt_ctf_field_type *get_type_field(struct bt_ctf_field_type *type, int i)
{
	struct bt_ctf_field_type *field = NULL;
	const char *unused_name;
	enum ctf_type_id type_id = bt_ctf_field_type_get_type_id(type);

	if (type_id == CTF_TYPE_STRUCT) {
		bt_ctf_field_type_structure_get_field(type, &unused_name,
			&field, i);
	} else if (type_id == CTF_TYPE_VARIANT) {
		bt_ctf_field_type_variant_get_field(type,
			&unused_name, &field, i);
	}

	return field;
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

BT_HIDDEN
ctf_type_stack *ctf_type_stack_create(void)
{
	return g_ptr_array_new();
}

BT_HIDDEN
void ctf_type_stack_destroy(
		ctf_type_stack *stack)
{
	g_ptr_array_free(stack, TRUE);
}

BT_HIDDEN
int ctf_type_stack_push(ctf_type_stack *stack,
		struct ctf_type_stack_frame *entry)
{
	int ret = 0;

	if (!stack || !entry) {
		ret = -1;
		goto end;
	}

	g_ptr_array_add(stack, entry);
end:
	return ret;
}

BT_HIDDEN
struct ctf_type_stack_frame *ctf_type_stack_peek(ctf_type_stack *stack)
{
	struct ctf_type_stack_frame *entry = NULL;

	if (!stack || stack->len == 0) {
		goto end;
	}

	entry = g_ptr_array_index(stack, stack->len - 1);
end:
	return entry;
}

BT_HIDDEN
struct ctf_type_stack_frame *ctf_type_stack_pop(ctf_type_stack *stack)
{
	struct ctf_type_stack_frame *entry = NULL;

	entry = ctf_type_stack_peek(stack);
	if (entry) {
		g_ptr_array_set_size(stack, stack->len - 1);
	}
	return entry;
}

static
int field_type_visit(struct bt_ctf_field_type *type,
		struct ctf_type_visitor_context *context,
		ctf_type_visitor_func func)
{
	int ret;
	enum ctf_type_id type_id;
	struct ctf_type_stack_frame *frame = NULL;

	ret = func(type, context);
	if (ret) {
		goto end;
	}

	type_id = bt_ctf_field_type_get_type_id(type);
	if (type_id == CTF_TYPE_SEQUENCE || type_id == CTF_TYPE_ARRAY) {
		struct bt_ctf_field_type *element =
			type_id == CTF_TYPE_SEQUENCE ?
			bt_ctf_field_type_sequence_get_element_type(type) :
			bt_ctf_field_type_array_get_element_type(type);

		ret = field_type_recursive_visit(element, context, func);
		bt_ctf_field_type_put(element);
		if (ret) {
			goto end;
		}
	}

	if (type_id != CTF_TYPE_STRUCT &&
		type_id != CTF_TYPE_VARIANT) {
		/* No need to create a new stack frame */
		goto end;
	}

	frame = g_new0(struct ctf_type_stack_frame, 1);
	if (!frame) {
		ret = -1;
		goto end;
	}

	frame->type = type;
	ret = ctf_type_stack_push(context->stack, frame);
	if (ret) {
		g_free(frame);
		goto end;
	}
end:
	return ret;
}

static
int field_type_recursive_visit(struct bt_ctf_field_type *type,
		struct ctf_type_visitor_context *context,
		ctf_type_visitor_func func)
{
	int ret = 0;
	struct ctf_type_stack_frame *stack_marker = NULL;

	ret = field_type_visit(type, context, func);
	if (ret) {
		goto end;
	}

	stack_marker = ctf_type_stack_peek(context->stack);
	if (!stack_marker || stack_marker->type != type) {
		/* No need for a recursive visit */
		goto end;
	}

	while (true) {
		struct bt_ctf_field_type *field;
		struct ctf_type_stack_frame *entry =
			ctf_type_stack_peek(context->stack);
		int field_count = get_type_field_count(entry->type);

		if (field_count <= 0) {
			/*
			 * Propagate error if one was given, else return
			 * -1 since empty structures or variants are invalid
			 * at this point.
			 */
			ret = field_count < 0 ? field_count : -1;
			goto end;
		}

		if (entry->index == field_count) {
			/* This level has been completely visited */
			entry = ctf_type_stack_pop(context->stack);
			if (entry) {
				g_free(entry);
			}

			if (entry == stack_marker) {
				/* Completed visit */
				break;
			} else {
				continue;
			}
		}

		field = get_type_field(entry->type, entry->index);
		/* Will push a new stack frame if field is struct or variant */
		ret = field_type_visit(field, context, func);
		bt_ctf_field_type_put(field);
		if (ret) {
			goto end;
		}

		entry->index++;
	}
end:
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
	int i, ret = 0, event_count;
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

	/* Visit event classes */
	event_count = bt_ctf_stream_class_get_event_class_count(stream_class);
	if (event_count < 0) {
		ret = event_count;
		goto end;
	}
	for (i = 0; i < event_count; i++) {
		struct bt_ctf_event_class *event_class =
			bt_ctf_stream_class_get_event_class(stream_class, i);

		ret = bt_ctf_event_class_visit(event_class, trace,
			stream_class, func);
		bt_ctf_event_class_put(event_class);
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
int set_field_path_relative(struct ctf_type_visitor_context *context,
		struct bt_ctf_field_path *field_path,
		GList **path_tokens, struct bt_ctf_field_type **resolved_field)
{
	int ret = 0;
	GArray *root_path;
	struct bt_ctf_field_type *field = NULL;
	struct ctf_type_stack_frame *frame =
		ctf_type_stack_peek(context->stack);
	size_t token_count = g_list_length(*path_tokens), i;

	if (!frame) {
		ret = -1;
		goto end;
	}

	field = frame->type;
	bt_ctf_field_type_get(field);
	for (i = 0; i < token_count; i++) {
		struct bt_ctf_field_type *next_field = NULL;
		int field_index = get_type_field_index(field,
			(*path_tokens)->data);

		if (field_index < 0) {
			/* Field name not found, abort */
			printf_verbose("Could not resolve field \"%s\"\n",
				(char *) (*path_tokens)->data);
			ret = -1;
			goto end;
		}

		if (field_index >= frame->index) {
			printf_verbose("Invalid relative path refers to a member after the current one\n");
			ret = -1;
			goto end;
		}

		next_field = get_type_field(field, field_index);
		if (!next_field) {
			ret = -1;
			goto end;
		}

		bt_ctf_field_type_put(field);
		field = next_field;
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

	root_path = g_array_sized_new(FALSE, FALSE,
		sizeof(int), context->stack->len - 1);
	if (!root_path) {
		ret = -1;
		goto end;
	}

	/* Set the current root node as the resolved type's root */
	field_path->root = context->root_node;
	/*
	 * Prepend the current fields' path to the relative path that
	 * was found by walking the stack.
	 */
	for (i = 0; i < context->stack->len - 1; i++) {
		int index;
		struct ctf_type_stack_frame *frame =
			g_ptr_array_index(context->stack, i);

		/* Decrement "index" since it points to the next field */
		index = frame->index - 1;
		g_array_append_val(root_path, index);
	}
	g_array_prepend_vals(field_path->path_indexes, root_path->data,
		root_path->len);
	g_array_free(root_path, TRUE);
end:
	if (field) {
		bt_ctf_field_type_put(field);
		*resolved_field = field;
	}

	return ret;
}

static
int set_field_path_absolute(struct ctf_type_visitor_context *context,
		struct bt_ctf_field_path *field_path,
		GList **path_tokens, struct bt_ctf_field_type **resolved_field)
{
	int ret = 0;
	struct bt_ctf_field_type *field = NULL;
	size_t token_count = g_list_length(*path_tokens), i;

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

	/* Set the appropriate root field */
	switch (field_path->root) {
	case CTF_NODE_TRACE_PACKET_HEADER:
		field = bt_ctf_trace_get_packet_header_type(context->trace);
		break;
	case CTF_NODE_STREAM_PACKET_CONTEXT:
		field = bt_ctf_stream_class_get_packet_context_type(
			context->stream_class);
		break;
	case CTF_NODE_STREAM_EVENT_HEADER:
		field = bt_ctf_stream_class_get_event_header_type(
			context->stream_class);
		break;
	case CTF_NODE_STREAM_EVENT_CONTEXT:
		field = bt_ctf_stream_class_get_event_context_type(
			context->stream_class);
		break;
	case CTF_NODE_EVENT_CONTEXT:
		field = bt_ctf_event_class_get_context_type(
			context->event_class);
		break;
	case CTF_NODE_EVENT_FIELDS:
		field = bt_ctf_event_class_get_payload_type(
			context->event_class);
		break;
	default:
		ret = -1;
		goto end;
	}

	if (!field) {
		ret = -1;
		goto end;
	}

	for (i = 0; i < token_count; i++) {
		int field_index = get_type_field_index(field,
			(*path_tokens)->data);
		struct bt_ctf_field_type *next_field = NULL;

		if (field_index < 0) {
			/* Field name not found, abort */
			printf_verbose("Could not resolve field \"%s\"\n",
				(char *) (*path_tokens)->data);
			ret = -1;
			goto end;
		}

		next_field = get_type_field(field, field_index);
		if (!next_field) {
			ret = -1;
			goto end;
		}

		bt_ctf_field_type_put(field);
		field = next_field;
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
	if (field) {
		bt_ctf_field_type_put(field);
		*resolved_field = field;
	}
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

	/* Set type's path */
	if (type_id == CTF_TYPE_VARIANT) {
		if (bt_ctf_field_type_get_type_id(resolved_type) !=
			CTF_TYPE_ENUM) {
			printf_verbose("Invalid variant tag \"%s\"; expected enum\n", field_name);
			ret = -1;
			goto end;
		}
		ret = bt_ctf_field_type_variant_set_tag(type, resolved_type);
		if (ret) {
			goto end;
		}

		ret = bt_ctf_field_type_variant_set_tag_field_path(type,
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

		ret = bt_ctf_field_type_sequence_set_length_field_path(type,
			field_path);
		if (ret) {
			goto end;
		}
	}
end:
	return ret;
}

BT_HIDDEN
int bt_ctf_trace_visit(struct bt_ctf_trace *trace,
		ctf_type_visitor_func func)
{
	int i, stream_count, ret = 0;
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

	stream_count = bt_ctf_trace_get_stream_class_count(trace);
	for (i = 0; i < stream_count; i++) {
		struct bt_ctf_stream_class *stream_class =
			bt_ctf_trace_get_stream_class(trace, i);

		/* Visit streams */
		ret = bt_ctf_stream_class_visit(stream_class, trace,
			func);
		bt_ctf_stream_class_put(stream_class);
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
