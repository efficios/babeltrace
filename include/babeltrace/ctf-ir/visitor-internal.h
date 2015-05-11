#ifndef BABELTRACE_CTF_IR_VISITOR_INTERNAL_H
#define BABELTRACE_CTF_IR_VISITOR_INTERNAL_H

/*
 * BabelTrace - CTF IR: Visitor internal
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

#include <babeltrace/ctf-ir/event-types.h>
#include <babeltrace/ctf-ir/event-types-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <glib.h>

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

BT_HIDDEN
ctf_type_stack *ctf_type_stack_create(void);

BT_HIDDEN
void ctf_type_stack_destroy(ctf_type_stack *stack);

BT_HIDDEN
int ctf_type_stack_push(ctf_type_stack *stack,
		struct ctf_type_stack_frame *entry);

BT_HIDDEN
struct ctf_type_stack_frame *ctf_type_stack_peek(
		ctf_type_stack *stack);

BT_HIDDEN
struct ctf_type_stack_frame *ctf_type_stack_pop(ctf_type_stack *stack);

BT_HIDDEN
int bt_ctf_trace_visit(struct bt_ctf_trace *trace,
		ctf_type_visitor_func func);

BT_HIDDEN
int bt_ctf_trace_resolve_types(struct bt_ctf_trace *trace);

BT_HIDDEN
int bt_ctf_stream_class_resolve_types(struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_trace *trace);

BT_HIDDEN
int bt_ctf_event_class_resolve_types(struct bt_ctf_event_class *event_class,
		struct bt_ctf_trace *trace,
		struct bt_ctf_stream_class *stream_class);

#endif /* BABELTRACE_CTF_IR_VISITOR_INTERNAL_H */
