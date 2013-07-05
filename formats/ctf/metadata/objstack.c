/*
 * objstack.c
 *
 * Common Trace Format Object Stack.
 *
 * Copyright 2013 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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

#include <stdlib.h>
#include <babeltrace/list.h>
#include <babeltrace/babeltrace-internal.h>

#define OBJSTACK_INIT_LEN		128
#define OBJSTACK_POISON			0xcc

struct objstack {
	struct bt_list_head head;	/* list of struct objstack_node */
};

struct objstack_node {
	struct bt_list_head node;
	size_t len;
	size_t used_len;
	char data[];
};

BT_HIDDEN
struct objstack *objstack_create(void)
{
	struct objstack *objstack;
	struct objstack_node *node;

	objstack = calloc(1, sizeof(*objstack));
	if (!objstack)
		return NULL;
	node = calloc(sizeof(struct objstack_node) + OBJSTACK_INIT_LEN,
			sizeof(char));
	if (!node) {
		free(objstack);
		return NULL;
	}
	BT_INIT_LIST_HEAD(&objstack->head);
	bt_list_add_tail(&node->node, &objstack->head);
	node->len = OBJSTACK_INIT_LEN;
	return objstack;
}

static
void objstack_node_free(struct objstack_node *node)
{
	size_t offset, len;
	char *p;

	if (!node)
		return;
	p = (char *) node;
	len = sizeof(*node) + node->len;
	for (offset = 0; offset < len; offset++)
		p[offset] = OBJSTACK_POISON;
	free(node);
}

BT_HIDDEN
void objstack_destroy(struct objstack *objstack)
{
	struct objstack_node *node, *p;

	if (!objstack)
		return;
	bt_list_for_each_entry_safe(node, p, &objstack->head, node) {
		bt_list_del(&node->node);
		objstack_node_free(node);
	}
	free(objstack);
}

static
struct objstack_node *objstack_append_node(struct objstack *objstack)
{
	struct objstack_node *last_node, *new_node;

	/* Get last node */
	last_node = bt_list_entry(objstack->head.prev,
			struct objstack_node, node);

	/* Allocate new node with double of size of last node */
	new_node = calloc(sizeof(struct objstack_node) + (last_node->len << 1),
			sizeof(char));
	if (!new_node) {
		return NULL;
	}
	bt_list_add_tail(&new_node->node, &objstack->head);
	new_node->len = last_node->len << 1;
	return new_node;
}

BT_HIDDEN
void *objstack_alloc(struct objstack *objstack, size_t len)
{
	struct objstack_node *last_node;
	void *p;

	/* Get last node */
	last_node = bt_list_entry(objstack->head.prev,
			struct objstack_node, node);
	while (last_node->len - last_node->used_len < len) {
		last_node = objstack_append_node(objstack);
		if (!last_node) {
			return NULL;
		}
	}
	p = &last_node->data[last_node->used_len];
	last_node->used_len += len;
	return p;
}
