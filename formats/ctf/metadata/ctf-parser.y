%{
/*
 * ctf-parser.y
 *
 * Common Trace Format Metadata Grammar.
 *
 * Copyright 2010 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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

#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <glib.h>
#include <errno.h>
#include <inttypes.h>
#include <babeltrace/list.h>
#include <babeltrace/babeltrace-internal.h>
#include "ctf-scanner.h"
#include "ctf-parser.h"
#include "ctf-ast.h"
#include "objstack.h"

BT_HIDDEN
int yydebug;

/* Join two lists, put "add" at the end of "head".  */
static inline void
_bt_list_splice_tail (struct bt_list_head *add, struct bt_list_head *head)
{
	/* Do nothing if the list which gets added is empty.  */
	if (add != add->next) {
		add->next->prev = head->prev;
		add->prev->next = head;
		head->prev->next = add->next;
		head->prev = add->prev;
	}
}

BT_HIDDEN
int yyparse(struct ctf_scanner *scanner, yyscan_t yyscanner);
BT_HIDDEN
int yylex(union YYSTYPE *yyval, yyscan_t yyscanner);
BT_HIDDEN
int yylex_init_extra(struct ctf_scanner *scanner, yyscan_t * ptr_yy_globals);
BT_HIDDEN
int yylex_destroy(yyscan_t yyscanner);
BT_HIDDEN
void yyrestart(FILE * in_str, yyscan_t yyscanner);
BT_HIDDEN
int yyget_lineno(yyscan_t yyscanner);
BT_HIDDEN
char *yyget_text(yyscan_t yyscanner);

static const char *node_type_to_str[] = {
#define ENTRY(S)	[S] = #S,
	FOREACH_CTF_NODES(ENTRY)
#undef ENTRY
};

/*
 * Static node for out of memory errors. Only "type" is used. lineno is
 * always left at 0. The rest of the node content can be overwritten,
 * but is never used.
 */
static struct ctf_node error_node = {
	.type = NODE_ERROR,
};

BT_HIDDEN
const char *node_type(struct ctf_node *node)
{
	if (node->type < NR_NODE_TYPES)
		return node_type_to_str[node->type];
	else
		return NULL;
}

void setstring(struct ctf_scanner *scanner, YYSTYPE *lvalp, const char *src)
{
	lvalp->s = objstack_alloc(scanner->objstack, strlen(src) + 1);
	strcpy(lvalp->s, src);
}

static
int str_check(size_t str_len, size_t offset, size_t len)
{
	/* check overflow */
	if (offset + len < offset)
		return -1;
	if (offset + len > str_len)
		return -1;
	return 0;
}

static
int bt_isodigit(int c)
{
	switch (c) {
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
		return 1;
	default:
		return 0;
	}
}

static
int parse_base_sequence(const char *src, size_t len, size_t pos,
		char *buffer, size_t *buf_len, int base)
{
	const size_t max_char = 3;
	int nr_char = 0;

	while (!str_check(len, pos, 1) && nr_char < max_char) {
		char c = src[pos++];

		if (base == 8) {
			if (bt_isodigit(c))
				buffer[nr_char++] = c;
			else
				break;
		} else if (base == 16) {
			if (isxdigit(c))
				buffer[nr_char++] = c;
			else
				break;

		} else {
			/* Unsupported base */
			return -1;
		}
	}
	assert(nr_char > 0);
	buffer[nr_char] = '\0';
	*buf_len = nr_char;
	return 0;
}

static
int import_basic_string(struct ctf_scanner *scanner, YYSTYPE *lvalp,
		size_t len, const char *src, char delim)
{
	size_t pos = 0, dpos = 0;

	if (str_check(len, pos, 1))
		return -1;
	if (src[pos++] != delim)
		return -1;

	while (src[pos] != delim) {
		char c;

		if (str_check(len, pos, 1))
			return -1;
		c = src[pos++];
		if (c == '\\') {
			if (str_check(len, pos, 1))
				return -1;
			c = src[pos++];

			switch (c) {
			case 'a':
				c = '\a';
				break;
			case 'b':
				c = '\b';
				break;
			case 'f':
				c = '\f';
				break;
			case 'n':
				c = '\n';
				break;
			case 'r':
				c = '\r';
				break;
			case 't':
				c = '\t';
				break;
			case 'v':
				c = '\v';
				break;
			case '\\':
				c = '\\';
				break;
			case '\'':
				c = '\'';
				break;
			case '\"':
				c = '\"';
				break;
			case '?':
				c = '?';
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			{
				char oct_buffer[4];
				size_t oct_len;

				if (parse_base_sequence(src, len, pos - 1,
						oct_buffer, &oct_len, 8))
					return -1;
				c = strtoul(&oct_buffer[0], NULL, 8);
				pos += oct_len - 1;
				break;
			}
			case 'x':
			{
				char hex_buffer[4];
				size_t hex_len;

				if (parse_base_sequence(src, len, pos,
						hex_buffer, &hex_len, 16))
					return -1;
				c = strtoul(&hex_buffer[0], NULL, 16);
				pos += hex_len;
				break;
			}
			default:
				return -1;
			}
		}
		if (str_check(len, dpos, 1))
			return -1;
		lvalp->s[dpos++] = c;
	}

	if (str_check(len, dpos, 1))
		return -1;
	lvalp->s[dpos++] = '\0';

	if (str_check(len, pos, 1))
		return -1;
	if (src[pos++] != delim)
		return -1;

	if (str_check(len, pos, 1))
		return -1;
	if (src[pos] != '\0')
		return -1;
	return 0;
}

int import_string(struct ctf_scanner *scanner, YYSTYPE *lvalp,
		const char *src, char delim)
{
	size_t len;

	len = strlen(src) + 1;
	lvalp->s = objstack_alloc(scanner->objstack, len);
	if (src[0] == 'L') {
		// TODO: import wide string
		printfl_error(yyget_lineno(scanner),
			"Wide string not supported yet.");
		return -1;
	} else {
		return import_basic_string(scanner, lvalp, len, src, delim);
	}
}

static void init_scope(struct ctf_scanner_scope *scope,
		       struct ctf_scanner_scope *parent)
{
	scope->parent = parent;
	scope->types = g_hash_table_new_full(g_str_hash, g_str_equal,
					     NULL, NULL);
}

static void finalize_scope(struct ctf_scanner_scope *scope)
{
	g_hash_table_destroy(scope->types);
}

static void push_scope(struct ctf_scanner *scanner)
{
	struct ctf_scanner_scope *ns;

	printf_debug("push scope\n");
	ns = malloc(sizeof(struct ctf_scanner_scope));
	init_scope(ns, scanner->cs);
	scanner->cs = ns;
}

static void pop_scope(struct ctf_scanner *scanner)
{
	struct ctf_scanner_scope *os;

	printf_debug("pop scope\n");
	os = scanner->cs;
	scanner->cs = os->parent;
	finalize_scope(os);
	free(os);
}

static int lookup_type(struct ctf_scanner_scope *s, const char *id)
{
	int ret;

	ret = (int) (long) g_hash_table_lookup(s->types, id);
	printf_debug("lookup %p %s %d\n", s, id, ret);
	return ret;
}

BT_HIDDEN
int is_type(struct ctf_scanner *scanner, const char *id)
{
	struct ctf_scanner_scope *it;
	int ret = 0;

	for (it = scanner->cs; it != NULL; it = it->parent) {
		if (lookup_type(it, id)) {
			ret = 1;
			break;
		}
	}
	printf_debug("is type %s %d\n", id, ret);
	return ret;
}

static void add_type(struct ctf_scanner *scanner, char *id)
{
	printf_debug("add type %s\n", id);
	if (lookup_type(scanner->cs, id))
		return;
	g_hash_table_insert(scanner->cs->types, id, id);
}

static struct ctf_node *make_node(struct ctf_scanner *scanner,
				  enum node_type type)
{
	struct ctf_node *node;

	node = objstack_alloc(scanner->objstack, sizeof(*node));
	if (!node) {
		printfl_fatal(yyget_lineno(scanner->scanner), "out of memory");
		return &error_node;
	}
	node->type = type;
	node->lineno = yyget_lineno(scanner->scanner);
	BT_INIT_LIST_HEAD(&node->tmp_head);
	bt_list_add(&node->siblings, &node->tmp_head);

	switch (type) {
	case NODE_ROOT:
		node->type = NODE_ERROR;
		printfn_fatal(node, "trying to create root node");
		break;

	case NODE_EVENT:
		BT_INIT_LIST_HEAD(&node->u.event.declaration_list);
		break;
	case NODE_STREAM:
		BT_INIT_LIST_HEAD(&node->u.stream.declaration_list);
		break;
	case NODE_ENV:
		BT_INIT_LIST_HEAD(&node->u.env.declaration_list);
		break;
	case NODE_TRACE:
		BT_INIT_LIST_HEAD(&node->u.trace.declaration_list);
		break;
	case NODE_CLOCK:
		BT_INIT_LIST_HEAD(&node->u.clock.declaration_list);
		break;
	case NODE_CALLSITE:
		BT_INIT_LIST_HEAD(&node->u.callsite.declaration_list);
		break;

	case NODE_CTF_EXPRESSION:
		BT_INIT_LIST_HEAD(&node->u.ctf_expression.left);
		BT_INIT_LIST_HEAD(&node->u.ctf_expression.right);
		break;
	case NODE_UNARY_EXPRESSION:
		break;

	case NODE_TYPEDEF:
		BT_INIT_LIST_HEAD(&node->u._typedef.type_declarators);
		break;
	case NODE_TYPEALIAS_TARGET:
		BT_INIT_LIST_HEAD(&node->u.typealias_target.type_declarators);
		break;
	case NODE_TYPEALIAS_ALIAS:
		BT_INIT_LIST_HEAD(&node->u.typealias_alias.type_declarators);
		break;
	case NODE_TYPEALIAS:
		break;

	case NODE_TYPE_SPECIFIER:
		break;
	case NODE_TYPE_SPECIFIER_LIST:
		BT_INIT_LIST_HEAD(&node->u.type_specifier_list.head);
		break;
	case NODE_POINTER:
		break;
	case NODE_TYPE_DECLARATOR:
		BT_INIT_LIST_HEAD(&node->u.type_declarator.pointers);
		break;

	case NODE_FLOATING_POINT:
		BT_INIT_LIST_HEAD(&node->u.floating_point.expressions);
		break;
	case NODE_INTEGER:
		BT_INIT_LIST_HEAD(&node->u.integer.expressions);
		break;
	case NODE_STRING:
		BT_INIT_LIST_HEAD(&node->u.string.expressions);
		break;
	case NODE_ENUMERATOR:
		BT_INIT_LIST_HEAD(&node->u.enumerator.values);
		break;
	case NODE_ENUM:
		BT_INIT_LIST_HEAD(&node->u._enum.enumerator_list);
		break;
	case NODE_STRUCT_OR_VARIANT_DECLARATION:
		BT_INIT_LIST_HEAD(&node->u.struct_or_variant_declaration.type_declarators);
		break;
	case NODE_VARIANT:
		BT_INIT_LIST_HEAD(&node->u.variant.declaration_list);
		break;
	case NODE_STRUCT:
		BT_INIT_LIST_HEAD(&node->u._struct.declaration_list);
		BT_INIT_LIST_HEAD(&node->u._struct.min_align);
		break;

	case NODE_UNKNOWN:
	default:
		node->type = NODE_ERROR;
		printfn_fatal(node, "unknown node type '%d'", (int) type);
		break;
	}

	return node;
}

static int reparent_ctf_expression(struct ctf_node *node,
				   struct ctf_node *parent)
{
	switch (parent->type) {
	case NODE_EVENT:
		_bt_list_splice_tail(&node->tmp_head, &parent->u.event.declaration_list);
		break;
	case NODE_STREAM:
		_bt_list_splice_tail(&node->tmp_head, &parent->u.stream.declaration_list);
		break;
	case NODE_ENV:
		_bt_list_splice_tail(&node->tmp_head, &parent->u.env.declaration_list);
		break;
	case NODE_TRACE:
		_bt_list_splice_tail(&node->tmp_head, &parent->u.trace.declaration_list);
		break;
	case NODE_CLOCK:
		_bt_list_splice_tail(&node->tmp_head, &parent->u.clock.declaration_list);
		break;
	case NODE_CALLSITE:
		_bt_list_splice_tail(&node->tmp_head, &parent->u.callsite.declaration_list);
		break;
	case NODE_FLOATING_POINT:
		_bt_list_splice_tail(&node->tmp_head, &parent->u.floating_point.expressions);
		break;
	case NODE_INTEGER:
		_bt_list_splice_tail(&node->tmp_head, &parent->u.integer.expressions);
		break;
	case NODE_STRING:
		_bt_list_splice_tail(&node->tmp_head, &parent->u.string.expressions);
		break;

	case NODE_ROOT:
	case NODE_CTF_EXPRESSION:
	case NODE_TYPEDEF:
	case NODE_TYPEALIAS_TARGET:
	case NODE_TYPEALIAS_ALIAS:
	case NODE_TYPEALIAS:
	case NODE_TYPE_SPECIFIER:
	case NODE_TYPE_SPECIFIER_LIST:
	case NODE_POINTER:
	case NODE_TYPE_DECLARATOR:
	case NODE_ENUMERATOR:
	case NODE_ENUM:
	case NODE_STRUCT_OR_VARIANT_DECLARATION:
	case NODE_VARIANT:
	case NODE_STRUCT:
	case NODE_UNARY_EXPRESSION:
		return -EPERM;

	case NODE_UNKNOWN:
	default:
		printfn_fatal(node, "unknown node type '%d'", (int) parent->type);
		return -EINVAL;
	}
	return 0;
}

static int reparent_typedef(struct ctf_node *node, struct ctf_node *parent)
{
	switch (parent->type) {
	case NODE_ROOT:
		_bt_list_splice_tail(&node->tmp_head, &parent->u.root.declaration_list);
		break;
	case NODE_EVENT:
		_bt_list_splice_tail(&node->tmp_head, &parent->u.event.declaration_list);
		break;
	case NODE_STREAM:
		_bt_list_splice_tail(&node->tmp_head, &parent->u.stream.declaration_list);
		break;
	case NODE_ENV:
		_bt_list_splice_tail(&node->tmp_head, &parent->u.env.declaration_list);
		break;
	case NODE_TRACE:
		_bt_list_splice_tail(&node->tmp_head, &parent->u.trace.declaration_list);
		break;
	case NODE_CLOCK:
		_bt_list_splice_tail(&node->tmp_head, &parent->u.clock.declaration_list);
		break;
	case NODE_CALLSITE:
		_bt_list_splice_tail(&node->tmp_head, &parent->u.callsite.declaration_list);
		break;
	case NODE_VARIANT:
		_bt_list_splice_tail(&node->tmp_head, &parent->u.variant.declaration_list);
		break;
	case NODE_STRUCT:
		_bt_list_splice_tail(&node->tmp_head, &parent->u._struct.declaration_list);
		break;

	case NODE_FLOATING_POINT:
	case NODE_INTEGER:
	case NODE_STRING:
	case NODE_CTF_EXPRESSION:
	case NODE_TYPEDEF:
	case NODE_TYPEALIAS_TARGET:
	case NODE_TYPEALIAS_ALIAS:
	case NODE_TYPEALIAS:
	case NODE_TYPE_SPECIFIER:
	case NODE_TYPE_SPECIFIER_LIST:
	case NODE_POINTER:
	case NODE_TYPE_DECLARATOR:
	case NODE_ENUMERATOR:
	case NODE_ENUM:
	case NODE_STRUCT_OR_VARIANT_DECLARATION:
	case NODE_UNARY_EXPRESSION:
		return -EPERM;

	case NODE_UNKNOWN:
	default:
		printfn_fatal(node, "unknown node type %d", parent->type);
		return -EINVAL;
	}
	return 0;
}

static int reparent_typealias(struct ctf_node *node, struct ctf_node *parent)
{
	switch (parent->type) {
	case NODE_ROOT:
		_bt_list_splice_tail(&node->tmp_head, &parent->u.root.declaration_list);
		break;
	case NODE_EVENT:
		_bt_list_splice_tail(&node->tmp_head, &parent->u.event.declaration_list);
		break;
	case NODE_STREAM:
		_bt_list_splice_tail(&node->tmp_head, &parent->u.stream.declaration_list);
		break;
	case NODE_ENV:
		_bt_list_splice_tail(&node->tmp_head, &parent->u.env.declaration_list);
		break;
	case NODE_TRACE:
		_bt_list_splice_tail(&node->tmp_head, &parent->u.trace.declaration_list);
		break;
	case NODE_CLOCK:
		_bt_list_splice_tail(&node->tmp_head, &parent->u.clock.declaration_list);
		break;
	case NODE_CALLSITE:
		_bt_list_splice_tail(&node->tmp_head, &parent->u.callsite.declaration_list);
		break;
	case NODE_VARIANT:
		_bt_list_splice_tail(&node->tmp_head, &parent->u.variant.declaration_list);
		break;
	case NODE_STRUCT:
		_bt_list_splice_tail(&node->tmp_head, &parent->u._struct.declaration_list);
		break;

	case NODE_FLOATING_POINT:
	case NODE_INTEGER:
	case NODE_STRING:
	case NODE_CTF_EXPRESSION:
	case NODE_TYPEDEF:
	case NODE_TYPEALIAS_TARGET:
	case NODE_TYPEALIAS_ALIAS:
	case NODE_TYPEALIAS:
	case NODE_TYPE_SPECIFIER:
	case NODE_TYPE_SPECIFIER_LIST:
	case NODE_POINTER:
	case NODE_TYPE_DECLARATOR:
	case NODE_ENUMERATOR:
	case NODE_ENUM:
	case NODE_STRUCT_OR_VARIANT_DECLARATION:
	case NODE_UNARY_EXPRESSION:
		return -EPERM;

	case NODE_UNKNOWN:
	default:
		printfn_fatal(node, "unknown node type '%d'", (int) parent->type);
		return -EINVAL;
	}
	return 0;
}

static int reparent_type_specifier(struct ctf_node *node,
				   struct ctf_node *parent)
{
	switch (parent->type) {
	case NODE_TYPE_SPECIFIER_LIST:
		_bt_list_splice_tail(&node->tmp_head, &parent->u.type_specifier_list.head);
		break;

	case NODE_TYPE_SPECIFIER:
	case NODE_EVENT:
	case NODE_STREAM:
	case NODE_ENV:
	case NODE_TRACE:
	case NODE_CLOCK:
	case NODE_CALLSITE:
	case NODE_VARIANT:
	case NODE_STRUCT:
	case NODE_TYPEDEF:
	case NODE_TYPEALIAS_TARGET:
	case NODE_TYPEALIAS_ALIAS:
	case NODE_TYPE_DECLARATOR:
	case NODE_ENUM:
	case NODE_STRUCT_OR_VARIANT_DECLARATION:
	case NODE_TYPEALIAS:
	case NODE_FLOATING_POINT:
	case NODE_INTEGER:
	case NODE_STRING:
	case NODE_CTF_EXPRESSION:
	case NODE_POINTER:
	case NODE_ENUMERATOR:
	case NODE_UNARY_EXPRESSION:
		return -EPERM;

	case NODE_UNKNOWN:
	default:
		printfn_fatal(node, "unknown node type '%d'", (int) parent->type);
		return -EINVAL;
	}
	return 0;
}

static int reparent_type_specifier_list(struct ctf_node *node,
					struct ctf_node *parent)
{
	switch (parent->type) {
	case NODE_ROOT:
		bt_list_add_tail(&node->siblings, &parent->u.root.declaration_list);
		break;
	case NODE_EVENT:
		bt_list_add_tail(&node->siblings, &parent->u.event.declaration_list);
		break;
	case NODE_STREAM:
		bt_list_add_tail(&node->siblings, &parent->u.stream.declaration_list);
		break;
	case NODE_ENV:
		bt_list_add_tail(&node->siblings, &parent->u.env.declaration_list);
		break;
	case NODE_TRACE:
		bt_list_add_tail(&node->siblings, &parent->u.trace.declaration_list);
		break;
	case NODE_CLOCK:
		bt_list_add_tail(&node->siblings, &parent->u.clock.declaration_list);
		break;
	case NODE_CALLSITE:
		bt_list_add_tail(&node->siblings, &parent->u.callsite.declaration_list);
		break;
	case NODE_VARIANT:
		bt_list_add_tail(&node->siblings, &parent->u.variant.declaration_list);
		break;
	case NODE_STRUCT:
		bt_list_add_tail(&node->siblings, &parent->u._struct.declaration_list);
		break;
	case NODE_TYPEDEF:
		parent->u._typedef.type_specifier_list = node;
		break;
	case NODE_TYPEALIAS_TARGET:
		parent->u.typealias_target.type_specifier_list = node;
		break;
	case NODE_TYPEALIAS_ALIAS:
		parent->u.typealias_alias.type_specifier_list = node;
		break;
	case NODE_ENUM:
		parent->u._enum.container_type = node;
		break;
	case NODE_STRUCT_OR_VARIANT_DECLARATION:
		parent->u.struct_or_variant_declaration.type_specifier_list = node;
		break;
	case NODE_TYPE_DECLARATOR:
	case NODE_TYPE_SPECIFIER:
	case NODE_TYPEALIAS:
	case NODE_FLOATING_POINT:
	case NODE_INTEGER:
	case NODE_STRING:
	case NODE_CTF_EXPRESSION:
	case NODE_POINTER:
	case NODE_ENUMERATOR:
	case NODE_UNARY_EXPRESSION:
		return -EPERM;

	case NODE_UNKNOWN:
	default:
		printfn_fatal(node, "unknown node type '%d'", (int) parent->type);
		return -EINVAL;
	}
	return 0;
}

static int reparent_type_declarator(struct ctf_node *node,
				    struct ctf_node *parent)
{
	switch (parent->type) {
	case NODE_TYPE_DECLARATOR:
		parent->u.type_declarator.type = TYPEDEC_NESTED;
		parent->u.type_declarator.u.nested.type_declarator = node;
		break;
	case NODE_STRUCT_OR_VARIANT_DECLARATION:
		_bt_list_splice_tail(&node->tmp_head, &parent->u.struct_or_variant_declaration.type_declarators);
		break;
	case NODE_TYPEDEF:
		_bt_list_splice_tail(&node->tmp_head, &parent->u._typedef.type_declarators);
		break;
	case NODE_TYPEALIAS_TARGET:
		_bt_list_splice_tail(&node->tmp_head, &parent->u.typealias_target.type_declarators);
		break;
	case NODE_TYPEALIAS_ALIAS:
		_bt_list_splice_tail(&node->tmp_head, &parent->u.typealias_alias.type_declarators);
		break;

	case NODE_ROOT:
	case NODE_EVENT:
	case NODE_STREAM:
	case NODE_ENV:
	case NODE_TRACE:
	case NODE_CLOCK:
	case NODE_CALLSITE:
	case NODE_VARIANT:
	case NODE_STRUCT:
	case NODE_TYPEALIAS:
	case NODE_ENUM:
	case NODE_FLOATING_POINT:
	case NODE_INTEGER:
	case NODE_STRING:
	case NODE_CTF_EXPRESSION:
	case NODE_TYPE_SPECIFIER:
	case NODE_TYPE_SPECIFIER_LIST:
	case NODE_POINTER:
	case NODE_ENUMERATOR:
	case NODE_UNARY_EXPRESSION:
		return -EPERM;

	case NODE_UNKNOWN:
	default:
		printfn_fatal(node, "unknown node type '%d'", (int) parent->type);
		return -EINVAL;
	}
	return 0;
}

/*
 * set_parent_node
 *
 * Link node to parent. Returns 0 on success, -EPERM if it is not permitted to
 * create the link declared by the input, -ENOENT if node or parent is NULL,
 * -EINVAL if there is an internal structure problem.
 */
static int set_parent_node(struct ctf_node *node,
			 struct ctf_node *parent)
{
	if (!node || !parent)
		return -ENOENT;

	/* Note: Linking to parent will be done only by an external visitor */

	switch (node->type) {
	case NODE_ROOT:
		printfn_fatal(node, "trying to reparent root node");
		return -EINVAL;

	case NODE_EVENT:
		if (parent->type == NODE_ROOT) {
			_bt_list_splice_tail(&node->tmp_head, &parent->u.root.event);
		} else {
			return -EPERM;
		}
		break;
	case NODE_STREAM:
		if (parent->type == NODE_ROOT) {
			_bt_list_splice_tail(&node->tmp_head, &parent->u.root.stream);
		} else {
			return -EPERM;
		}
		break;
	case NODE_ENV:
		if (parent->type == NODE_ROOT) {
			_bt_list_splice_tail(&node->tmp_head, &parent->u.root.env);
		} else {
			return -EPERM;
		}
		break;
	case NODE_TRACE:
		if (parent->type == NODE_ROOT) {
			_bt_list_splice_tail(&node->tmp_head, &parent->u.root.trace);
		} else {
			return -EPERM;
		}
		break;
	case NODE_CLOCK:
		if (parent->type == NODE_ROOT) {
			_bt_list_splice_tail(&node->tmp_head, &parent->u.root.clock);
		} else {
			return -EPERM;
		}
		break;
	case NODE_CALLSITE:
		if (parent->type == NODE_ROOT) {
			_bt_list_splice_tail(&node->tmp_head, &parent->u.root.callsite);
		} else {
			return -EPERM;
		}
		break;

	case NODE_CTF_EXPRESSION:
		return reparent_ctf_expression(node, parent);
	case NODE_UNARY_EXPRESSION:
		if (parent->type == NODE_TYPE_DECLARATOR)
			parent->u.type_declarator.bitfield_len = node;
		else
			return -EPERM;
		break;

	case NODE_TYPEDEF:
		return reparent_typedef(node, parent);
	case NODE_TYPEALIAS_TARGET:
		if (parent->type == NODE_TYPEALIAS)
			parent->u.typealias.target = node;
		else
			return -EINVAL;
	case NODE_TYPEALIAS_ALIAS:
		if (parent->type == NODE_TYPEALIAS)
			parent->u.typealias.alias = node;
		else
			return -EINVAL;
	case NODE_TYPEALIAS:
		return reparent_typealias(node, parent);

	case NODE_POINTER:
		if (parent->type == NODE_TYPE_DECLARATOR) {
			_bt_list_splice_tail(&node->tmp_head, &parent->u.type_declarator.pointers);
		} else
			return -EPERM;
		break;
	case NODE_TYPE_DECLARATOR:
		return reparent_type_declarator(node, parent);

	case NODE_TYPE_SPECIFIER_LIST:
		return reparent_type_specifier_list(node, parent);

	case NODE_TYPE_SPECIFIER:
		return reparent_type_specifier(node, parent);

	case NODE_FLOATING_POINT:
	case NODE_INTEGER:
	case NODE_STRING:
	case NODE_ENUM:
	case NODE_VARIANT:
	case NODE_STRUCT:
		return -EINVAL;	/* Dealt with internally within grammar */

	case NODE_ENUMERATOR:
		if (parent->type == NODE_ENUM) {
			_bt_list_splice_tail(&node->tmp_head, &parent->u._enum.enumerator_list);
		} else {
			return -EPERM;
		}
		break;
	case NODE_STRUCT_OR_VARIANT_DECLARATION:
		switch (parent->type) {
		case NODE_STRUCT:
			_bt_list_splice_tail(&node->tmp_head, &parent->u._struct.declaration_list);
			break;
		case NODE_VARIANT:
			_bt_list_splice_tail(&node->tmp_head, &parent->u.variant.declaration_list);
			break;
		default:
			return -EINVAL;
		}
		break;

	case NODE_UNKNOWN:
	default:
		printfn_fatal(node, "unknown node type '%d'", (int) parent->type);
		return -EINVAL;
	}
	return 0;
}

BT_HIDDEN
void yyerror(struct ctf_scanner *scanner, yyscan_t yyscanner, const char *str)
{
	printfl_error(yyget_lineno(scanner->scanner),
		"token \"%s\": %s\n",
		yyget_text(scanner->scanner), str);
}
 
BT_HIDDEN
int yywrap(void)
{
	return 1;
} 

#define reparent_error(scanner, str)				\
do {								\
	yyerror(scanner, scanner->scanner, YY_("reparent_error: " str)); \
	YYERROR;						\
} while (0)

static struct ctf_ast *ctf_ast_alloc(struct ctf_scanner *scanner)
{
	struct ctf_ast *ast;

	ast = objstack_alloc(scanner->objstack, sizeof(*ast));
	if (!ast)
		return NULL;
	ast->root.type = NODE_ROOT;
	BT_INIT_LIST_HEAD(&ast->root.tmp_head);
	BT_INIT_LIST_HEAD(&ast->root.u.root.declaration_list);
	BT_INIT_LIST_HEAD(&ast->root.u.root.trace);
	BT_INIT_LIST_HEAD(&ast->root.u.root.env);
	BT_INIT_LIST_HEAD(&ast->root.u.root.stream);
	BT_INIT_LIST_HEAD(&ast->root.u.root.event);
	BT_INIT_LIST_HEAD(&ast->root.u.root.clock);
	BT_INIT_LIST_HEAD(&ast->root.u.root.callsite);
	return ast;
}

int ctf_scanner_append_ast(struct ctf_scanner *scanner, FILE *input)
{
	/* Start processing new stream */
	yyrestart(input, scanner->scanner);
	if (yydebug)
		fprintf(stdout, "Scanner input is a%s.\n",
			isatty(fileno(input)) ? "n interactive tty" :
						" noninteractive file");
	return yyparse(scanner, scanner->scanner);
}

struct ctf_scanner *ctf_scanner_alloc(void)
{
	struct ctf_scanner *scanner;
	int ret;

	yydebug = babeltrace_debug;

	scanner = malloc(sizeof(*scanner));
	if (!scanner)
		return NULL;
	memset(scanner, 0, sizeof(*scanner));
	ret = yylex_init_extra(scanner, &scanner->scanner);
	if (ret) {
		printf_fatal("yylex_init error");
		goto cleanup_scanner;
	}
	scanner->objstack = objstack_create();
	if (!scanner->objstack)
		goto cleanup_lexer;
	scanner->ast = ctf_ast_alloc(scanner);
	if (!scanner->ast)
		goto cleanup_objstack;
	init_scope(&scanner->root_scope, NULL);
	scanner->cs = &scanner->root_scope;

	return scanner;

cleanup_objstack:
	objstack_destroy(scanner->objstack);
cleanup_lexer:
	ret = yylex_destroy(scanner->scanner);
	if (!ret)
		printf_fatal("yylex_destroy error");
cleanup_scanner:
	free(scanner);
	return NULL;
}

void ctf_scanner_free(struct ctf_scanner *scanner)
{
	int ret;

	if (!scanner)
		return;
	finalize_scope(&scanner->root_scope);
	objstack_destroy(scanner->objstack);
	ret = yylex_destroy(scanner->scanner);
	if (ret)
		printf_error("yylex_destroy error");
	free(scanner);
}

%}

%define api.pure
	/* %locations */
%error-verbose
%parse-param {struct ctf_scanner *scanner}
%parse-param {yyscan_t yyscanner}
%lex-param {yyscan_t yyscanner}
/*
 * Expect two shift-reduce conflicts. Caused by enum name-opt : type {}
 * vs struct { int :value; } (unnamed bit-field). The default is to
 * shift, so whenever we encounter an enumeration, we are doing the
 * proper thing (shift). It is illegal to declare an enumeration
 * "bit-field", so it is OK if this situation ends up in a parsing
 * error.
 */
%expect 2
%start file
%token INTEGER_LITERAL STRING_LITERAL CHARACTER_LITERAL LSBRAC RSBRAC LPAREN RPAREN LBRAC RBRAC RARROW STAR PLUS MINUS LT GT TYPEASSIGN COLON SEMICOLON DOTDOTDOT DOT EQUAL COMMA CONST CHAR DOUBLE ENUM ENV EVENT FLOATING_POINT FLOAT INTEGER INT LONG SHORT SIGNED STREAM STRING STRUCT TRACE CALLSITE CLOCK TYPEALIAS TYPEDEF UNSIGNED VARIANT VOID _BOOL _COMPLEX _IMAGINARY TOK_ALIGN
%token <s> IDENTIFIER ID_TYPE
%token ERROR
%union
{
	long long ll;
	unsigned long long ull;
	char c;
	char *s;
	struct ctf_node *n;
}

%type <s> STRING_LITERAL CHARACTER_LITERAL

%type <s> keywords

%type <ull> INTEGER_LITERAL
%type <n> postfix_expression unary_expression unary_expression_or_range

%type <n> declaration
%type <n> event_declaration
%type <n> stream_declaration
%type <n> env_declaration
%type <n> trace_declaration
%type <n> clock_declaration
%type <n> callsite_declaration
%type <n> integer_declaration_specifiers
%type <n> declaration_specifiers
%type <n> alias_declaration_specifiers

%type <n> type_declarator_list
%type <n> integer_type_specifier
%type <n> type_specifier
%type <n> struct_type_specifier
%type <n> variant_type_specifier
%type <n> enum_type_specifier
%type <n> struct_or_variant_declaration_list
%type <n> struct_or_variant_declaration
%type <n> struct_or_variant_declarator_list
%type <n> struct_or_variant_declarator
%type <n> enumerator_list
%type <n> enumerator
%type <n> abstract_declarator_list
%type <n> abstract_declarator
%type <n> direct_abstract_declarator
%type <n> alias_abstract_declarator_list
%type <n> alias_abstract_declarator
%type <n> direct_alias_abstract_declarator
%type <n> declarator
%type <n> direct_declarator
%type <n> type_declarator
%type <n> direct_type_declarator
%type <n> pointer	
%type <n> ctf_assignment_expression_list
%type <n> ctf_assignment_expression

%%

file:
		declaration
		{
			if (set_parent_node($1, &ctf_scanner_get_ast(scanner)->root))
				reparent_error(scanner, "error reparenting to root");
		}
	|	file declaration
		{
			if (set_parent_node($2, &ctf_scanner_get_ast(scanner)->root))
				reparent_error(scanner, "error reparenting to root");
		}
	;

keywords:
		VOID
		{	$$ = yylval.s;		}
	|	CHAR
		{	$$ = yylval.s;		}
	|	SHORT
		{	$$ = yylval.s;		}
	|	INT
		{	$$ = yylval.s;		}
	|	LONG
		{	$$ = yylval.s;		}
	|	FLOAT
		{	$$ = yylval.s;		}
	|	DOUBLE
		{	$$ = yylval.s;		}
	|	SIGNED
		{	$$ = yylval.s;		}
	|	UNSIGNED
		{	$$ = yylval.s;		}
	|	_BOOL
		{	$$ = yylval.s;		}
	|	_COMPLEX
		{	$$ = yylval.s;		}
	|	_IMAGINARY
		{	$$ = yylval.s;		}
	|	FLOATING_POINT
		{	$$ = yylval.s;		}
	|	INTEGER
		{	$$ = yylval.s;		}
	|	STRING
		{	$$ = yylval.s;		}
	|	ENUM
		{	$$ = yylval.s;		}
	|	VARIANT
		{	$$ = yylval.s;		}
	|	STRUCT
		{	$$ = yylval.s;		}
	|	CONST
		{	$$ = yylval.s;		}
	|	TYPEDEF
		{	$$ = yylval.s;		}
	|	EVENT
		{	$$ = yylval.s;		}
	|	STREAM
		{	$$ = yylval.s;		}
	|	ENV
		{	$$ = yylval.s;		}
	|	TRACE
		{	$$ = yylval.s;		}
	|	CLOCK
		{	$$ = yylval.s;		}
	|	CALLSITE
		{	$$ = yylval.s;		}
	|	TOK_ALIGN
		{	$$ = yylval.s;		}
	;


/* 2: Phrase structure grammar */

postfix_expression:
		IDENTIFIER
		{
			$$ = make_node(scanner, NODE_UNARY_EXPRESSION);
			$$->u.unary_expression.type = UNARY_STRING;
			$$->u.unary_expression.u.string = yylval.s;
		}
	|	ID_TYPE
		{
			$$ = make_node(scanner, NODE_UNARY_EXPRESSION);
			$$->u.unary_expression.type = UNARY_STRING;
			$$->u.unary_expression.u.string = yylval.s;
		}
	|	keywords
		{
			$$ = make_node(scanner, NODE_UNARY_EXPRESSION);
			$$->u.unary_expression.type = UNARY_STRING;
			$$->u.unary_expression.u.string = yylval.s;
		}
	|	INTEGER_LITERAL
		{
			$$ = make_node(scanner, NODE_UNARY_EXPRESSION);
			$$->u.unary_expression.type = UNARY_UNSIGNED_CONSTANT;
			$$->u.unary_expression.u.unsigned_constant = $1;
		}
	|	STRING_LITERAL
		{
			$$ = make_node(scanner, NODE_UNARY_EXPRESSION);
			$$->u.unary_expression.type = UNARY_STRING;
			$$->u.unary_expression.u.string = $1;
		}
	|	CHARACTER_LITERAL
		{
			$$ = make_node(scanner, NODE_UNARY_EXPRESSION);
			$$->u.unary_expression.type = UNARY_STRING;
			$$->u.unary_expression.u.string = $1;
		}
	|	LPAREN unary_expression RPAREN
		{
			$$ = $2;
		}
	|	postfix_expression LSBRAC unary_expression RSBRAC
		{
			$$ = make_node(scanner, NODE_UNARY_EXPRESSION);
			$$->u.unary_expression.type = UNARY_SBRAC;
			$$->u.unary_expression.u.sbrac_exp = $3;
			bt_list_splice(&($1)->tmp_head, &($$)->tmp_head);
			bt_list_add_tail(&($$)->siblings, &($$)->tmp_head);
		}
	|	postfix_expression DOT IDENTIFIER
		{
			$$ = make_node(scanner, NODE_UNARY_EXPRESSION);
			$$->u.unary_expression.type = UNARY_STRING;
			$$->u.unary_expression.u.string = yylval.s;
			$$->u.unary_expression.link = UNARY_DOTLINK;
			bt_list_splice(&($1)->tmp_head, &($$)->tmp_head);
			bt_list_add_tail(&($$)->siblings, &($$)->tmp_head);
		}
	|	postfix_expression DOT ID_TYPE
		{
			$$ = make_node(scanner, NODE_UNARY_EXPRESSION);
			$$->u.unary_expression.type = UNARY_STRING;
			$$->u.unary_expression.u.string = yylval.s;
			$$->u.unary_expression.link = UNARY_DOTLINK;
			bt_list_splice(&($1)->tmp_head, &($$)->tmp_head);
			bt_list_add_tail(&($$)->siblings, &($$)->tmp_head);
		}
	|	postfix_expression RARROW IDENTIFIER
		{
			$$ = make_node(scanner, NODE_UNARY_EXPRESSION);
			$$->u.unary_expression.type = UNARY_STRING;
			$$->u.unary_expression.u.string = yylval.s;
			$$->u.unary_expression.link = UNARY_ARROWLINK;
			bt_list_splice(&($1)->tmp_head, &($$)->tmp_head);
			bt_list_add_tail(&($$)->siblings, &($$)->tmp_head);
		}
	|	postfix_expression RARROW ID_TYPE
		{
			$$ = make_node(scanner, NODE_UNARY_EXPRESSION);
			$$->u.unary_expression.type = UNARY_STRING;
			$$->u.unary_expression.u.string = yylval.s;
			$$->u.unary_expression.link = UNARY_ARROWLINK;
			bt_list_splice(&($1)->tmp_head, &($$)->tmp_head);
			bt_list_add_tail(&($$)->siblings, &($$)->tmp_head);
		}
	;

unary_expression:
		postfix_expression
		{	$$ = $1;				}
	|	PLUS postfix_expression
		{
			$$ = $2;
			if ($$->u.unary_expression.type != UNARY_UNSIGNED_CONSTANT
				&& $$->u.unary_expression.type != UNARY_SIGNED_CONSTANT) {
				reparent_error(scanner, "expecting numeric constant");
			}
		}
	|	MINUS postfix_expression
		{
			$$ = $2;
			if ($$->u.unary_expression.type == UNARY_UNSIGNED_CONSTANT) {
				$$->u.unary_expression.type = UNARY_SIGNED_CONSTANT;
				$$->u.unary_expression.u.signed_constant =
					-($$->u.unary_expression.u.unsigned_constant);
			} else if ($$->u.unary_expression.type == UNARY_UNSIGNED_CONSTANT) {
				$$->u.unary_expression.u.signed_constant =
					-($$->u.unary_expression.u.signed_constant);
			} else {
				reparent_error(scanner, "expecting numeric constant");
			}
		}
	;

unary_expression_or_range:
		unary_expression DOTDOTDOT unary_expression
		{
			$$ = $1;
			_bt_list_splice_tail(&($3)->tmp_head, &($$)->tmp_head);
			$3->u.unary_expression.link = UNARY_DOTDOTDOT;
		}
	|	unary_expression
		{	$$ = $1;		}
	;

/* 2.2: Declarations */

declaration:
		declaration_specifiers SEMICOLON
		{	$$ = $1;	}
	|	event_declaration
		{	$$ = $1;	}
	|	stream_declaration
		{	$$ = $1;	}
	|	env_declaration
		{	$$ = $1;	}
	|	trace_declaration
		{	$$ = $1;	}
	|	clock_declaration
		{	$$ = $1;	}
	|	callsite_declaration
		{	$$ = $1;	}
	|	declaration_specifiers TYPEDEF declaration_specifiers type_declarator_list SEMICOLON
		{
			struct ctf_node *list;

			$$ = make_node(scanner, NODE_TYPEDEF);
			list = make_node(scanner, NODE_TYPE_SPECIFIER_LIST);
			$$->u._typedef.type_specifier_list = list;
			_bt_list_splice_tail(&($1)->u.type_specifier_list.head, &list->u.type_specifier_list.head);
			_bt_list_splice_tail(&($3)->u.type_specifier_list.head, &list->u.type_specifier_list.head);
			_bt_list_splice_tail(&($4)->tmp_head, &($$)->u._typedef.type_declarators);
		}
	|	TYPEDEF declaration_specifiers type_declarator_list SEMICOLON
		{
			struct ctf_node *list;

			$$ = make_node(scanner, NODE_TYPEDEF);
			list = make_node(scanner, NODE_TYPE_SPECIFIER_LIST);
			$$->u._typedef.type_specifier_list = list;
			_bt_list_splice_tail(&($2)->u.type_specifier_list.head, &list->u.type_specifier_list.head);
			_bt_list_splice_tail(&($3)->tmp_head, &($$)->u._typedef.type_declarators);
		}
	|	declaration_specifiers TYPEDEF type_declarator_list SEMICOLON
		{
			struct ctf_node *list;

			$$ = make_node(scanner, NODE_TYPEDEF);
			list = make_node(scanner, NODE_TYPE_SPECIFIER_LIST);
			$$->u._typedef.type_specifier_list = list;
			_bt_list_splice_tail(&($1)->u.type_specifier_list.head, &list->u.type_specifier_list.head);
			_bt_list_splice_tail(&($3)->tmp_head, &($$)->u._typedef.type_declarators);
		}
	|	TYPEALIAS declaration_specifiers abstract_declarator_list TYPEASSIGN alias_declaration_specifiers alias_abstract_declarator_list SEMICOLON
		{
			struct ctf_node *list;

			$$ = make_node(scanner, NODE_TYPEALIAS);
			$$->u.typealias.target = make_node(scanner, NODE_TYPEALIAS_TARGET);
			$$->u.typealias.alias = make_node(scanner, NODE_TYPEALIAS_ALIAS);

			list = make_node(scanner, NODE_TYPE_SPECIFIER_LIST);
			$$->u.typealias.target->u.typealias_target.type_specifier_list = list;
			_bt_list_splice_tail(&($2)->u.type_specifier_list.head, &list->u.type_specifier_list.head);
			_bt_list_splice_tail(&($3)->tmp_head, &($$)->u.typealias.target->u.typealias_target.type_declarators);

			list = make_node(scanner, NODE_TYPE_SPECIFIER_LIST);
			$$->u.typealias.alias->u.typealias_alias.type_specifier_list = list;
			_bt_list_splice_tail(&($5)->u.type_specifier_list.head, &list->u.type_specifier_list.head);
			_bt_list_splice_tail(&($6)->tmp_head, &($$)->u.typealias.alias->u.typealias_alias.type_declarators);
		}
	;

event_declaration:
		event_declaration_begin event_declaration_end
		{
			$$ = make_node(scanner, NODE_EVENT);
		}
	|	event_declaration_begin ctf_assignment_expression_list event_declaration_end
		{
			$$ = make_node(scanner, NODE_EVENT);
			if (set_parent_node($2, $$))
				reparent_error(scanner, "event_declaration");
		}
	;

event_declaration_begin:
		EVENT LBRAC
		{	push_scope(scanner);	}
	;

event_declaration_end:
		RBRAC SEMICOLON
		{	pop_scope(scanner);	}
	;


stream_declaration:
		stream_declaration_begin stream_declaration_end
		{
			$$ = make_node(scanner, NODE_STREAM);
		}
	|	stream_declaration_begin ctf_assignment_expression_list stream_declaration_end
		{
			$$ = make_node(scanner, NODE_STREAM);
			if (set_parent_node($2, $$))
				reparent_error(scanner, "stream_declaration");
		}
	;

stream_declaration_begin:
		STREAM LBRAC
		{	push_scope(scanner);	}
	;

stream_declaration_end:
		RBRAC SEMICOLON
		{	pop_scope(scanner);	}
	;

env_declaration:
		env_declaration_begin env_declaration_end
		{
			$$ = make_node(scanner, NODE_ENV);
		}
	|	env_declaration_begin ctf_assignment_expression_list env_declaration_end
		{
			$$ = make_node(scanner, NODE_ENV);
			if (set_parent_node($2, $$))
				reparent_error(scanner, "env declaration");
		}
	;

env_declaration_begin:
		ENV LBRAC
		{	push_scope(scanner);	}
	;

env_declaration_end:
		RBRAC SEMICOLON
		{	pop_scope(scanner);	}
	;

trace_declaration:
		trace_declaration_begin trace_declaration_end
		{
			$$ = make_node(scanner, NODE_TRACE);
		}
	|	trace_declaration_begin ctf_assignment_expression_list trace_declaration_end
		{
			$$ = make_node(scanner, NODE_TRACE);
			if (set_parent_node($2, $$))
				reparent_error(scanner, "trace_declaration");
		}
	;

trace_declaration_begin:
		TRACE LBRAC
		{	push_scope(scanner);	}
	;

trace_declaration_end:
		RBRAC SEMICOLON
		{	pop_scope(scanner);	}
	;

clock_declaration:
		CLOCK clock_declaration_begin clock_declaration_end
		{
			$$ = make_node(scanner, NODE_CLOCK);
		}
	|	CLOCK clock_declaration_begin ctf_assignment_expression_list clock_declaration_end
		{
			$$ = make_node(scanner, NODE_CLOCK);
			if (set_parent_node($3, $$))
				reparent_error(scanner, "trace_declaration");
		}
	;

clock_declaration_begin:
		LBRAC
		{	push_scope(scanner);	}
	;

clock_declaration_end:
		RBRAC SEMICOLON
		{	pop_scope(scanner);	}
	;

callsite_declaration:
		CALLSITE callsite_declaration_begin callsite_declaration_end
		{
			$$ = make_node(scanner, NODE_CALLSITE);
		}
	|	CALLSITE callsite_declaration_begin ctf_assignment_expression_list callsite_declaration_end
		{
			$$ = make_node(scanner, NODE_CALLSITE);
			if (set_parent_node($3, $$))
				reparent_error(scanner, "trace_declaration");
		}
	;

callsite_declaration_begin:
		LBRAC
		{	push_scope(scanner);	}
	;

callsite_declaration_end:
		RBRAC SEMICOLON
		{	pop_scope(scanner);	}
	;

integer_declaration_specifiers:
		CONST
		{
			struct ctf_node *node;

			$$ = make_node(scanner, NODE_TYPE_SPECIFIER_LIST);
			node = make_node(scanner, NODE_TYPE_SPECIFIER);
			node->u.type_specifier.type = TYPESPEC_CONST;
			bt_list_add_tail(&node->siblings, &($$)->u.type_specifier_list.head);
		}
	|	integer_type_specifier
		{
			struct ctf_node *node;

			$$ = make_node(scanner, NODE_TYPE_SPECIFIER_LIST);
			node = $1;
			bt_list_add_tail(&node->siblings, &($$)->u.type_specifier_list.head);
		}
	|	integer_declaration_specifiers CONST
		{
			struct ctf_node *node;

			$$ = $1;
			node = make_node(scanner, NODE_TYPE_SPECIFIER);
			node->u.type_specifier.type = TYPESPEC_CONST;
			bt_list_add_tail(&node->siblings, &($$)->u.type_specifier_list.head);
		}
	|	integer_declaration_specifiers integer_type_specifier
		{
			$$ = $1;
			bt_list_add_tail(&($2)->siblings, &($$)->u.type_specifier_list.head);
		}
	;

declaration_specifiers:
		CONST
		{
			struct ctf_node *node;

			$$ = make_node(scanner, NODE_TYPE_SPECIFIER_LIST);
			node = make_node(scanner, NODE_TYPE_SPECIFIER);
			node->u.type_specifier.type = TYPESPEC_CONST;
			bt_list_add_tail(&node->siblings, &($$)->u.type_specifier_list.head);
		}
	|	type_specifier
		{
			struct ctf_node *node;

			$$ = make_node(scanner, NODE_TYPE_SPECIFIER_LIST);
			node = $1;
			bt_list_add_tail(&node->siblings, &($$)->u.type_specifier_list.head);
		}
	|	declaration_specifiers CONST
		{
			struct ctf_node *node;

			$$ = $1;
			node = make_node(scanner, NODE_TYPE_SPECIFIER);
			node->u.type_specifier.type = TYPESPEC_CONST;
			bt_list_add_tail(&node->siblings, &($$)->u.type_specifier_list.head);
		}
	|	declaration_specifiers type_specifier
		{
			$$ = $1;
			bt_list_add_tail(&($2)->siblings, &($$)->u.type_specifier_list.head);
		}
	;

type_declarator_list:
		type_declarator
		{	$$ = $1;	}
	|	type_declarator_list COMMA type_declarator
		{
			$$ = $1;
			bt_list_add_tail(&($3)->siblings, &($$)->tmp_head);
		}
	;

integer_type_specifier:
		CHAR
		{
			$$ = make_node(scanner, NODE_TYPE_SPECIFIER);
			$$->u.type_specifier.type = TYPESPEC_CHAR;
		}
	|	SHORT
		{
			$$ = make_node(scanner, NODE_TYPE_SPECIFIER);
			$$->u.type_specifier.type = TYPESPEC_SHORT;
		}
	|	INT
		{
			$$ = make_node(scanner, NODE_TYPE_SPECIFIER);
			$$->u.type_specifier.type = TYPESPEC_INT;
		}
	|	LONG
		{
			$$ = make_node(scanner, NODE_TYPE_SPECIFIER);
			$$->u.type_specifier.type = TYPESPEC_LONG;
		}
	|	SIGNED
		{
			$$ = make_node(scanner, NODE_TYPE_SPECIFIER);
			$$->u.type_specifier.type = TYPESPEC_SIGNED;
		}
	|	UNSIGNED
		{
			$$ = make_node(scanner, NODE_TYPE_SPECIFIER);
			$$->u.type_specifier.type = TYPESPEC_UNSIGNED;
		}
	|	_BOOL
		{
			$$ = make_node(scanner, NODE_TYPE_SPECIFIER);
			$$->u.type_specifier.type = TYPESPEC_BOOL;
		}
	|	ID_TYPE
		{
			$$ = make_node(scanner, NODE_TYPE_SPECIFIER);
			$$->u.type_specifier.type = TYPESPEC_ID_TYPE;
			$$->u.type_specifier.id_type = yylval.s;
		}
	|	INTEGER LBRAC RBRAC
		{
			$$ = make_node(scanner, NODE_TYPE_SPECIFIER);
			$$->u.type_specifier.type = TYPESPEC_INTEGER;
			$$->u.type_specifier.node = make_node(scanner, NODE_INTEGER);
		}
	|	INTEGER LBRAC ctf_assignment_expression_list RBRAC
		{
			$$ = make_node(scanner, NODE_TYPE_SPECIFIER);
			$$->u.type_specifier.type = TYPESPEC_INTEGER;
			$$->u.type_specifier.node = make_node(scanner, NODE_INTEGER);
			if (set_parent_node($3, $$->u.type_specifier.node))
				reparent_error(scanner, "integer reparent error");
		}
	;

type_specifier:
		VOID
		{
			$$ = make_node(scanner, NODE_TYPE_SPECIFIER);
			$$->u.type_specifier.type = TYPESPEC_VOID;
		}
	|	CHAR
		{
			$$ = make_node(scanner, NODE_TYPE_SPECIFIER);
			$$->u.type_specifier.type = TYPESPEC_CHAR;
		}
	|	SHORT
		{
			$$ = make_node(scanner, NODE_TYPE_SPECIFIER);
			$$->u.type_specifier.type = TYPESPEC_SHORT;
		}
	|	INT
		{
			$$ = make_node(scanner, NODE_TYPE_SPECIFIER);
			$$->u.type_specifier.type = TYPESPEC_INT;
		}
	|	LONG
		{
			$$ = make_node(scanner, NODE_TYPE_SPECIFIER);
			$$->u.type_specifier.type = TYPESPEC_LONG;
		}
	|	FLOAT
		{
			$$ = make_node(scanner, NODE_TYPE_SPECIFIER);
			$$->u.type_specifier.type = TYPESPEC_FLOAT;
		}
	|	DOUBLE
		{
			$$ = make_node(scanner, NODE_TYPE_SPECIFIER);
			$$->u.type_specifier.type = TYPESPEC_DOUBLE;
		}
	|	SIGNED
		{
			$$ = make_node(scanner, NODE_TYPE_SPECIFIER);
			$$->u.type_specifier.type = TYPESPEC_SIGNED;
		}
	|	UNSIGNED
		{
			$$ = make_node(scanner, NODE_TYPE_SPECIFIER);
			$$->u.type_specifier.type = TYPESPEC_UNSIGNED;
		}
	|	_BOOL
		{
			$$ = make_node(scanner, NODE_TYPE_SPECIFIER);
			$$->u.type_specifier.type = TYPESPEC_BOOL;
		}
	|	_COMPLEX
		{
			$$ = make_node(scanner, NODE_TYPE_SPECIFIER);
			$$->u.type_specifier.type = TYPESPEC_COMPLEX;
		}
	|	_IMAGINARY
		{
			$$ = make_node(scanner, NODE_TYPE_SPECIFIER);
			$$->u.type_specifier.type = TYPESPEC_IMAGINARY;
		}
	|	ID_TYPE
		{
			$$ = make_node(scanner, NODE_TYPE_SPECIFIER);
			$$->u.type_specifier.type = TYPESPEC_ID_TYPE;
			$$->u.type_specifier.id_type = yylval.s;
		}
	|	FLOATING_POINT LBRAC RBRAC
		{
			$$ = make_node(scanner, NODE_TYPE_SPECIFIER);
			$$->u.type_specifier.type = TYPESPEC_FLOATING_POINT;
			$$->u.type_specifier.node = make_node(scanner, NODE_FLOATING_POINT);
		}
	|	FLOATING_POINT LBRAC ctf_assignment_expression_list RBRAC
		{
			$$ = make_node(scanner, NODE_TYPE_SPECIFIER);
			$$->u.type_specifier.type = TYPESPEC_FLOATING_POINT;
			$$->u.type_specifier.node = make_node(scanner, NODE_FLOATING_POINT);
			if (set_parent_node($3, $$->u.type_specifier.node))
				reparent_error(scanner, "floating point reparent error");
		}
	|	INTEGER LBRAC RBRAC
		{
			$$ = make_node(scanner, NODE_TYPE_SPECIFIER);
			$$->u.type_specifier.type = TYPESPEC_INTEGER;
			$$->u.type_specifier.node = make_node(scanner, NODE_INTEGER);
		}
	|	INTEGER LBRAC ctf_assignment_expression_list RBRAC
		{
			$$ = make_node(scanner, NODE_TYPE_SPECIFIER);
			$$->u.type_specifier.type = TYPESPEC_INTEGER;
			$$->u.type_specifier.node = make_node(scanner, NODE_INTEGER);
			if (set_parent_node($3, $$->u.type_specifier.node))
				reparent_error(scanner, "integer reparent error");
		}
	|	STRING
		{
			$$ = make_node(scanner, NODE_TYPE_SPECIFIER);
			$$->u.type_specifier.type = TYPESPEC_STRING;
			$$->u.type_specifier.node = make_node(scanner, NODE_STRING);
		}
	|	STRING LBRAC RBRAC
		{
			$$ = make_node(scanner, NODE_TYPE_SPECIFIER);
			$$->u.type_specifier.type = TYPESPEC_STRING;
			$$->u.type_specifier.node = make_node(scanner, NODE_STRING);
		}
	|	STRING LBRAC ctf_assignment_expression_list RBRAC
		{
			$$ = make_node(scanner, NODE_TYPE_SPECIFIER);
			$$->u.type_specifier.type = TYPESPEC_STRING;
			$$->u.type_specifier.node = make_node(scanner, NODE_STRING);
			if (set_parent_node($3, $$->u.type_specifier.node))
				reparent_error(scanner, "string reparent error");
		}
	|	ENUM enum_type_specifier
		{
			$$ = make_node(scanner, NODE_TYPE_SPECIFIER);
			$$->u.type_specifier.type = TYPESPEC_ENUM;
			$$->u.type_specifier.node = $2;
		}
	|	VARIANT variant_type_specifier
		{
			$$ = make_node(scanner, NODE_TYPE_SPECIFIER);
			$$->u.type_specifier.type = TYPESPEC_VARIANT;
			$$->u.type_specifier.node = $2;
		}
	|	STRUCT struct_type_specifier
		{
			$$ = make_node(scanner, NODE_TYPE_SPECIFIER);
			$$->u.type_specifier.type = TYPESPEC_STRUCT;
			$$->u.type_specifier.node = $2;
		}
	;

struct_type_specifier:
		struct_declaration_begin struct_or_variant_declaration_list struct_declaration_end
		{
			$$ = make_node(scanner, NODE_STRUCT);
			$$->u._struct.has_body = 1;
			if ($2 && set_parent_node($2, $$))
				reparent_error(scanner, "struct reparent error");
		}
	|	IDENTIFIER struct_declaration_begin struct_or_variant_declaration_list struct_declaration_end
		{
			$$ = make_node(scanner, NODE_STRUCT);
			$$->u._struct.has_body = 1;
			$$->u._struct.name = $1;
			if ($3 && set_parent_node($3, $$))
				reparent_error(scanner, "struct reparent error");
		}
	|	ID_TYPE struct_declaration_begin struct_or_variant_declaration_list struct_declaration_end
		{
			$$ = make_node(scanner, NODE_STRUCT);
			$$->u._struct.has_body = 1;
			$$->u._struct.name = $1;
			if ($3 && set_parent_node($3, $$))
				reparent_error(scanner, "struct reparent error");
		}
	|	IDENTIFIER
		{
			$$ = make_node(scanner, NODE_STRUCT);
			$$->u._struct.has_body = 0;
			$$->u._struct.name = $1;
		}
	|	ID_TYPE
		{
			$$ = make_node(scanner, NODE_STRUCT);
			$$->u._struct.has_body = 0;
			$$->u._struct.name = $1;
		}
	|	struct_declaration_begin struct_or_variant_declaration_list struct_declaration_end TOK_ALIGN LPAREN unary_expression RPAREN
		{
			$$ = make_node(scanner, NODE_STRUCT);
			$$->u._struct.has_body = 1;
			bt_list_add_tail(&($6)->siblings, &$$->u._struct.min_align);
			if ($2 && set_parent_node($2, $$))
				reparent_error(scanner, "struct reparent error");
		}
	|	IDENTIFIER struct_declaration_begin struct_or_variant_declaration_list struct_declaration_end TOK_ALIGN LPAREN unary_expression RPAREN
		{
			$$ = make_node(scanner, NODE_STRUCT);
			$$->u._struct.has_body = 1;
			$$->u._struct.name = $1;
			bt_list_add_tail(&($7)->siblings, &$$->u._struct.min_align);
			if ($3 && set_parent_node($3, $$))
				reparent_error(scanner, "struct reparent error");
		}
	|	ID_TYPE struct_declaration_begin struct_or_variant_declaration_list struct_declaration_end TOK_ALIGN LPAREN unary_expression RPAREN
		{
			$$ = make_node(scanner, NODE_STRUCT);
			$$->u._struct.has_body = 1;
			$$->u._struct.name = $1;
			bt_list_add_tail(&($7)->siblings, &$$->u._struct.min_align);
			if ($3 && set_parent_node($3, $$))
				reparent_error(scanner, "struct reparent error");
		}
	;

struct_declaration_begin:
		LBRAC
		{	push_scope(scanner);	}
	;

struct_declaration_end:
		RBRAC
		{	pop_scope(scanner);	}
	;

variant_type_specifier:
		variant_declaration_begin struct_or_variant_declaration_list variant_declaration_end
		{
			$$ = make_node(scanner, NODE_VARIANT);
			$$->u.variant.has_body = 1;
			if ($2 && set_parent_node($2, $$))
				reparent_error(scanner, "variant reparent error");
		}
	|	LT IDENTIFIER GT variant_declaration_begin struct_or_variant_declaration_list variant_declaration_end
		{
			$$ = make_node(scanner, NODE_VARIANT);
			$$->u.variant.has_body = 1;
			$$->u.variant.choice = $2;
			if ($5 && set_parent_node($5, $$))
				reparent_error(scanner, "variant reparent error");
		}
	|	LT ID_TYPE GT variant_declaration_begin struct_or_variant_declaration_list variant_declaration_end
		{
			$$ = make_node(scanner, NODE_VARIANT);
			$$->u.variant.has_body = 1;
			$$->u.variant.choice = $2;
			if ($5 && set_parent_node($5, $$))
				reparent_error(scanner, "variant reparent error");
		}
	|	IDENTIFIER variant_declaration_begin struct_or_variant_declaration_list variant_declaration_end
		{
			$$ = make_node(scanner, NODE_VARIANT);
			$$->u.variant.has_body = 1;
			$$->u.variant.name = $1;
			if ($3 && set_parent_node($3, $$))
				reparent_error(scanner, "variant reparent error");
		}
	|	IDENTIFIER LT IDENTIFIER GT variant_declaration_begin struct_or_variant_declaration_list variant_declaration_end
		{
			$$ = make_node(scanner, NODE_VARIANT);
			$$->u.variant.has_body = 1;
			$$->u.variant.name = $1;
			$$->u.variant.choice = $3;
			if ($6 && set_parent_node($6, $$))
				reparent_error(scanner, "variant reparent error");
		}
	|	IDENTIFIER LT IDENTIFIER GT
		{
			$$ = make_node(scanner, NODE_VARIANT);
			$$->u.variant.has_body = 0;
			$$->u.variant.name = $1;
			$$->u.variant.choice = $3;
		}
	|	IDENTIFIER LT ID_TYPE GT variant_declaration_begin struct_or_variant_declaration_list variant_declaration_end
		{
			$$ = make_node(scanner, NODE_VARIANT);
			$$->u.variant.has_body = 1;
			$$->u.variant.name = $1;
			$$->u.variant.choice = $3;
			if ($6 && set_parent_node($6, $$))
				reparent_error(scanner, "variant reparent error");
		}
	|	IDENTIFIER LT ID_TYPE GT
		{
			$$ = make_node(scanner, NODE_VARIANT);
			$$->u.variant.has_body = 0;
			$$->u.variant.name = $1;
			$$->u.variant.choice = $3;
		}
	|	ID_TYPE variant_declaration_begin struct_or_variant_declaration_list variant_declaration_end
		{
			$$ = make_node(scanner, NODE_VARIANT);
			$$->u.variant.has_body = 1;
			$$->u.variant.name = $1;
			if ($3 && set_parent_node($3, $$))
				reparent_error(scanner, "variant reparent error");
		}
	|	ID_TYPE LT IDENTIFIER GT variant_declaration_begin struct_or_variant_declaration_list variant_declaration_end
		{
			$$ = make_node(scanner, NODE_VARIANT);
			$$->u.variant.has_body = 1;
			$$->u.variant.name = $1;
			$$->u.variant.choice = $3;
			if ($6 && set_parent_node($6, $$))
				reparent_error(scanner, "variant reparent error");
		}
	|	ID_TYPE LT IDENTIFIER GT
		{
			$$ = make_node(scanner, NODE_VARIANT);
			$$->u.variant.has_body = 0;
			$$->u.variant.name = $1;
			$$->u.variant.choice = $3;
		}
	|	ID_TYPE LT ID_TYPE GT variant_declaration_begin struct_or_variant_declaration_list variant_declaration_end
		{
			$$ = make_node(scanner, NODE_VARIANT);
			$$->u.variant.has_body = 1;
			$$->u.variant.name = $1;
			$$->u.variant.choice = $3;
			if ($6 && set_parent_node($6, $$))
				reparent_error(scanner, "variant reparent error");
		}
	|	ID_TYPE LT ID_TYPE GT
		{
			$$ = make_node(scanner, NODE_VARIANT);
			$$->u.variant.has_body = 0;
			$$->u.variant.name = $1;
			$$->u.variant.choice = $3;
		}
	;

variant_declaration_begin:
		LBRAC
		{	push_scope(scanner);	}
	;

variant_declaration_end:
		RBRAC
		{	pop_scope(scanner);	}
	;

enum_type_specifier:
		LBRAC enumerator_list RBRAC
		{
			$$ = make_node(scanner, NODE_ENUM);
			$$->u._enum.has_body = 1;
			_bt_list_splice_tail(&($2)->tmp_head, &($$)->u._enum.enumerator_list);
		}
	|	COLON integer_declaration_specifiers LBRAC enumerator_list RBRAC
		{
			$$ = make_node(scanner, NODE_ENUM);
			$$->u._enum.has_body = 1;
			($$)->u._enum.container_type = $2;
			_bt_list_splice_tail(&($4)->tmp_head, &($$)->u._enum.enumerator_list);
		}
	|	IDENTIFIER LBRAC enumerator_list RBRAC
		{
			$$ = make_node(scanner, NODE_ENUM);
			$$->u._enum.has_body = 1;
			$$->u._enum.enum_id = $1;
			_bt_list_splice_tail(&($3)->tmp_head, &($$)->u._enum.enumerator_list);
		}
	|	IDENTIFIER COLON integer_declaration_specifiers LBRAC enumerator_list RBRAC
		{
			$$ = make_node(scanner, NODE_ENUM);
			$$->u._enum.has_body = 1;
			$$->u._enum.enum_id = $1;
			($$)->u._enum.container_type = $3;
			_bt_list_splice_tail(&($5)->tmp_head, &($$)->u._enum.enumerator_list);
		}
	|	ID_TYPE LBRAC enumerator_list RBRAC
		{
			$$ = make_node(scanner, NODE_ENUM);
			$$->u._enum.has_body = 1;
			$$->u._enum.enum_id = $1;
			_bt_list_splice_tail(&($3)->tmp_head, &($$)->u._enum.enumerator_list);
		}
	|	ID_TYPE COLON integer_declaration_specifiers LBRAC enumerator_list RBRAC
		{
			$$ = make_node(scanner, NODE_ENUM);
			$$->u._enum.has_body = 1;
			$$->u._enum.enum_id = $1;
			($$)->u._enum.container_type = $3;
			_bt_list_splice_tail(&($5)->tmp_head, &($$)->u._enum.enumerator_list);
		}
	|	LBRAC enumerator_list COMMA RBRAC
		{
			$$ = make_node(scanner, NODE_ENUM);
			$$->u._enum.has_body = 1;
			_bt_list_splice_tail(&($2)->tmp_head, &($$)->u._enum.enumerator_list);
		}
	|	COLON integer_declaration_specifiers LBRAC enumerator_list COMMA RBRAC
		{
			$$ = make_node(scanner, NODE_ENUM);
			$$->u._enum.has_body = 1;
			($$)->u._enum.container_type = $2;
			_bt_list_splice_tail(&($4)->tmp_head, &($$)->u._enum.enumerator_list);
		}
	|	IDENTIFIER LBRAC enumerator_list COMMA RBRAC
		{
			$$ = make_node(scanner, NODE_ENUM);
			$$->u._enum.has_body = 1;
			$$->u._enum.enum_id = $1;
			_bt_list_splice_tail(&($3)->tmp_head, &($$)->u._enum.enumerator_list);
		}
	|	IDENTIFIER COLON integer_declaration_specifiers LBRAC enumerator_list COMMA RBRAC
		{
			$$ = make_node(scanner, NODE_ENUM);
			$$->u._enum.has_body = 1;
			$$->u._enum.enum_id = $1;
			($$)->u._enum.container_type = $3;
			_bt_list_splice_tail(&($5)->tmp_head, &($$)->u._enum.enumerator_list);
		}
	|	IDENTIFIER
		{
			$$ = make_node(scanner, NODE_ENUM);
			$$->u._enum.has_body = 0;
			$$->u._enum.enum_id = $1;
		}
	|	ID_TYPE LBRAC enumerator_list COMMA RBRAC
		{
			$$ = make_node(scanner, NODE_ENUM);
			$$->u._enum.has_body = 1;
			$$->u._enum.enum_id = $1;
			_bt_list_splice_tail(&($3)->tmp_head, &($$)->u._enum.enumerator_list);
		}
	|	ID_TYPE COLON integer_declaration_specifiers LBRAC enumerator_list COMMA RBRAC
		{
			$$ = make_node(scanner, NODE_ENUM);
			$$->u._enum.has_body = 1;
			$$->u._enum.enum_id = $1;
			($$)->u._enum.container_type = $3;
			_bt_list_splice_tail(&($5)->tmp_head, &($$)->u._enum.enumerator_list);
		}
	|	ID_TYPE
		{
			$$ = make_node(scanner, NODE_ENUM);
			$$->u._enum.has_body = 0;
			$$->u._enum.enum_id = $1;
		}
	;

struct_or_variant_declaration_list:
		/* empty */
		{	$$ = NULL;	}
	|	struct_or_variant_declaration_list struct_or_variant_declaration
		{
			if ($1) {
				$$ = $1;
				bt_list_add_tail(&($2)->siblings, &($$)->tmp_head);
			} else {
				$$ = $2;
				bt_list_add_tail(&($$)->siblings, &($$)->tmp_head);
			}
		}
	;

struct_or_variant_declaration:
		declaration_specifiers struct_or_variant_declarator_list SEMICOLON
		{
			struct ctf_node *list;

			list = make_node(scanner, NODE_TYPE_SPECIFIER_LIST);
			_bt_list_splice_tail(&($1)->u.type_specifier_list.head, &list->u.type_specifier_list.head);
			$$ = make_node(scanner, NODE_STRUCT_OR_VARIANT_DECLARATION);
			($$)->u.struct_or_variant_declaration.type_specifier_list = list;
			_bt_list_splice_tail(&($2)->tmp_head, &($$)->u.struct_or_variant_declaration.type_declarators);
		}
	|	declaration_specifiers TYPEDEF declaration_specifiers type_declarator_list SEMICOLON
		{
			struct ctf_node *list;

			$$ = make_node(scanner, NODE_TYPEDEF);
			list = make_node(scanner, NODE_TYPE_SPECIFIER_LIST);
			$$->u._typedef.type_specifier_list = list;
			_bt_list_splice_tail(&($1)->u.type_specifier_list.head, &list->u.type_specifier_list.head);
			_bt_list_splice_tail(&($3)->u.type_specifier_list.head, &list->u.type_specifier_list.head);
			_bt_list_splice_tail(&($4)->tmp_head, &($$)->u._typedef.type_declarators);
		}
	|	TYPEDEF declaration_specifiers type_declarator_list SEMICOLON
		{
			struct ctf_node *list;

			$$ = make_node(scanner, NODE_TYPEDEF);
			list = make_node(scanner, NODE_TYPE_SPECIFIER_LIST);
			$$->u._typedef.type_specifier_list = list;
			_bt_list_splice_tail(&($2)->u.type_specifier_list.head, &list->u.type_specifier_list.head);
			_bt_list_splice_tail(&($3)->tmp_head, &($$)->u._typedef.type_declarators);
		}
	|	declaration_specifiers TYPEDEF type_declarator_list SEMICOLON
		{
			struct ctf_node *list;

			list = make_node(scanner, NODE_TYPE_SPECIFIER_LIST);
			_bt_list_splice_tail(&($1)->u.type_specifier_list.head, &list->u.type_specifier_list.head);
			$$ = make_node(scanner, NODE_TYPEDEF);
			($$)->u.struct_or_variant_declaration.type_specifier_list = list;
			_bt_list_splice_tail(&($3)->tmp_head, &($$)->u._typedef.type_declarators);
		}
	|	TYPEALIAS declaration_specifiers abstract_declarator_list TYPEASSIGN alias_declaration_specifiers alias_abstract_declarator_list SEMICOLON
		{
			struct ctf_node *list;

			$$ = make_node(scanner, NODE_TYPEALIAS);
			$$->u.typealias.target = make_node(scanner, NODE_TYPEALIAS_TARGET);
			$$->u.typealias.alias = make_node(scanner, NODE_TYPEALIAS_ALIAS);

			list = make_node(scanner, NODE_TYPE_SPECIFIER_LIST);
			$$->u.typealias.target->u.typealias_target.type_specifier_list = list;
			_bt_list_splice_tail(&($2)->u.type_specifier_list.head, &list->u.type_specifier_list.head);
			_bt_list_splice_tail(&($3)->tmp_head, &($$)->u.typealias.target->u.typealias_target.type_declarators);

			list = make_node(scanner, NODE_TYPE_SPECIFIER_LIST);
			$$->u.typealias.alias->u.typealias_alias.type_specifier_list = list;
			_bt_list_splice_tail(&($5)->u.type_specifier_list.head, &list->u.type_specifier_list.head);
			_bt_list_splice_tail(&($6)->tmp_head, &($$)->u.typealias.alias->u.typealias_alias.type_declarators);
		}
	;

alias_declaration_specifiers:
		CONST
		{
			struct ctf_node *node;

			$$ = make_node(scanner, NODE_TYPE_SPECIFIER_LIST);
			node = make_node(scanner, NODE_TYPE_SPECIFIER);
			node->u.type_specifier.type = TYPESPEC_CONST;
			bt_list_add_tail(&node->siblings, &($$)->u.type_specifier_list.head);
		}
	|	type_specifier
		{
			struct ctf_node *node;

			$$ = make_node(scanner, NODE_TYPE_SPECIFIER_LIST);
			node = $1;
			bt_list_add_tail(&node->siblings, &($$)->u.type_specifier_list.head);
		}
	|	IDENTIFIER
		{
			struct ctf_node *node;

			add_type(scanner, $1);
			$$ = make_node(scanner, NODE_TYPE_SPECIFIER_LIST);
			node = make_node(scanner, NODE_TYPE_SPECIFIER);
			node->u.type_specifier.type = TYPESPEC_ID_TYPE;
			node->u.type_specifier.id_type = yylval.s;
			bt_list_add_tail(&node->siblings, &($$)->u.type_specifier_list.head);
		}
	|	alias_declaration_specifiers CONST
		{
			struct ctf_node *node;

			$$ = $1;
			node = make_node(scanner, NODE_TYPE_SPECIFIER);
			node->u.type_specifier.type = TYPESPEC_CONST;
			bt_list_add_tail(&node->siblings, &($$)->u.type_specifier_list.head);
		}
	|	alias_declaration_specifiers type_specifier
		{
			$$ = $1;
			bt_list_add_tail(&($2)->siblings, &($$)->u.type_specifier_list.head);
		}
	|	alias_declaration_specifiers IDENTIFIER
		{
			struct ctf_node *node;

			add_type(scanner, $2);
			$$ = $1;
			node = make_node(scanner, NODE_TYPE_SPECIFIER);
			node->u.type_specifier.type = TYPESPEC_ID_TYPE;
			node->u.type_specifier.id_type = yylval.s;
			bt_list_add_tail(&node->siblings, &($$)->u.type_specifier_list.head);
		}
	;

struct_or_variant_declarator_list:
		struct_or_variant_declarator
		{	$$ = $1;	}
	|	struct_or_variant_declarator_list COMMA struct_or_variant_declarator
		{
			$$ = $1;
			bt_list_add_tail(&($3)->siblings, &($$)->tmp_head);
		}
	;

struct_or_variant_declarator:
		declarator
		{	$$ = $1;	}
	|	COLON unary_expression
		{	$$ = $2;	}
	|	declarator COLON unary_expression
		{
			$$ = $1;
			if (set_parent_node($3, $1))
				reparent_error(scanner, "struct_or_variant_declarator");
		}
	;

enumerator_list:
		enumerator
		{	$$ = $1;	}
	|	enumerator_list COMMA enumerator
		{
			$$ = $1;
			bt_list_add_tail(&($3)->siblings, &($$)->tmp_head);
		}
	;

enumerator:
		IDENTIFIER
		{
			$$ = make_node(scanner, NODE_ENUMERATOR);
			$$->u.enumerator.id = $1;
		}
	|	ID_TYPE
		{
			$$ = make_node(scanner, NODE_ENUMERATOR);
			$$->u.enumerator.id = $1;
		}
	|	keywords
		{
			$$ = make_node(scanner, NODE_ENUMERATOR);
			$$->u.enumerator.id = $1;
		}
	|	STRING_LITERAL
		{
			$$ = make_node(scanner, NODE_ENUMERATOR);
			$$->u.enumerator.id = $1;
		}
	|	IDENTIFIER EQUAL unary_expression_or_range
		{
			$$ = make_node(scanner, NODE_ENUMERATOR);
			$$->u.enumerator.id = $1;
			bt_list_splice(&($3)->tmp_head, &($$)->u.enumerator.values);
		}
	|	ID_TYPE EQUAL unary_expression_or_range
		{
			$$ = make_node(scanner, NODE_ENUMERATOR);
			$$->u.enumerator.id = $1;
			bt_list_splice(&($3)->tmp_head, &($$)->u.enumerator.values);
		}
	|	keywords EQUAL unary_expression_or_range
		{
			$$ = make_node(scanner, NODE_ENUMERATOR);
			$$->u.enumerator.id = $1;
			bt_list_splice(&($3)->tmp_head, &($$)->u.enumerator.values);
		}
	|	STRING_LITERAL EQUAL unary_expression_or_range
		{
			$$ = make_node(scanner, NODE_ENUMERATOR);
			$$->u.enumerator.id = $1;
			bt_list_splice(&($3)->tmp_head, &($$)->u.enumerator.values);
		}
	;

abstract_declarator_list:
		abstract_declarator
		{	$$ = $1;	}
	|	abstract_declarator_list COMMA abstract_declarator
		{
			$$ = $1;
			bt_list_add_tail(&($3)->siblings, &($$)->tmp_head);
		}
	;

abstract_declarator:
		direct_abstract_declarator
		{	$$ = $1;	}
	|	pointer direct_abstract_declarator
		{
			$$ = $2;
			bt_list_splice(&($1)->tmp_head, &($$)->u.type_declarator.pointers);
		}
	;

direct_abstract_declarator:
		/* empty */
		{
			$$ = make_node(scanner, NODE_TYPE_DECLARATOR);
                        $$->u.type_declarator.type = TYPEDEC_ID;
			/* id is NULL */
		}
	|	IDENTIFIER
		{
			$$ = make_node(scanner, NODE_TYPE_DECLARATOR);
			$$->u.type_declarator.type = TYPEDEC_ID;
			$$->u.type_declarator.u.id = $1;
		}
	|	LPAREN abstract_declarator RPAREN
		{
			$$ = make_node(scanner, NODE_TYPE_DECLARATOR);
			$$->u.type_declarator.type = TYPEDEC_NESTED;
			$$->u.type_declarator.u.nested.type_declarator = $2;
		}
	|	direct_abstract_declarator LSBRAC unary_expression RSBRAC
		{
			$$ = make_node(scanner, NODE_TYPE_DECLARATOR);
			$$->u.type_declarator.type = TYPEDEC_NESTED;
			$$->u.type_declarator.u.nested.type_declarator = $1;
			BT_INIT_LIST_HEAD(&($$)->u.type_declarator.u.nested.length);
			_bt_list_splice_tail(&($3)->tmp_head, &($$)->u.type_declarator.u.nested.length);
		}
	|	direct_abstract_declarator LSBRAC RSBRAC
		{
			$$ = make_node(scanner, NODE_TYPE_DECLARATOR);
			$$->u.type_declarator.type = TYPEDEC_NESTED;
			$$->u.type_declarator.u.nested.type_declarator = $1;
			$$->u.type_declarator.u.nested.abstract_array = 1;
		}
	;

alias_abstract_declarator_list:
		alias_abstract_declarator
		{	$$ = $1;	}
	|	alias_abstract_declarator_list COMMA alias_abstract_declarator
		{
			$$ = $1;
			bt_list_add_tail(&($3)->siblings, &($$)->tmp_head);
		}
	;

alias_abstract_declarator:
		direct_alias_abstract_declarator
		{	$$ = $1;	}
	|	pointer direct_alias_abstract_declarator
		{
			$$ = $2;
			bt_list_splice(&($1)->tmp_head, &($$)->u.type_declarator.pointers);
		}
	;

direct_alias_abstract_declarator:
		/* empty */
		{
			$$ = make_node(scanner, NODE_TYPE_DECLARATOR);
                        $$->u.type_declarator.type = TYPEDEC_ID;
			/* id is NULL */
		}
	|	LPAREN alias_abstract_declarator RPAREN
		{
			$$ = make_node(scanner, NODE_TYPE_DECLARATOR);
			$$->u.type_declarator.type = TYPEDEC_NESTED;
			$$->u.type_declarator.u.nested.type_declarator = $2;
		}
	|	direct_alias_abstract_declarator LSBRAC unary_expression RSBRAC
		{
			$$ = make_node(scanner, NODE_TYPE_DECLARATOR);
			$$->u.type_declarator.type = TYPEDEC_NESTED;
			$$->u.type_declarator.u.nested.type_declarator = $1;
			BT_INIT_LIST_HEAD(&($$)->u.type_declarator.u.nested.length);
			_bt_list_splice_tail(&($3)->tmp_head, &($$)->u.type_declarator.u.nested.length);
		}
	|	direct_alias_abstract_declarator LSBRAC RSBRAC
		{
			$$ = make_node(scanner, NODE_TYPE_DECLARATOR);
			$$->u.type_declarator.type = TYPEDEC_NESTED;
			$$->u.type_declarator.u.nested.type_declarator = $1;
			$$->u.type_declarator.u.nested.abstract_array = 1;
		}
	;

declarator:
		direct_declarator
		{	$$ = $1;	}
	|	pointer direct_declarator
		{
			$$ = $2;
			bt_list_splice(&($1)->tmp_head, &($$)->u.type_declarator.pointers);
		}
	;

direct_declarator:
		IDENTIFIER
		{
			$$ = make_node(scanner, NODE_TYPE_DECLARATOR);
			$$->u.type_declarator.type = TYPEDEC_ID;
			$$->u.type_declarator.u.id = $1;
		}
	|	LPAREN declarator RPAREN
		{
			$$ = make_node(scanner, NODE_TYPE_DECLARATOR);
			$$->u.type_declarator.type = TYPEDEC_NESTED;
			$$->u.type_declarator.u.nested.type_declarator = $2;
		}
	|	direct_declarator LSBRAC unary_expression RSBRAC
		{
			$$ = make_node(scanner, NODE_TYPE_DECLARATOR);
			$$->u.type_declarator.type = TYPEDEC_NESTED;
			$$->u.type_declarator.u.nested.type_declarator = $1;
			BT_INIT_LIST_HEAD(&($$)->u.type_declarator.u.nested.length);
			_bt_list_splice_tail(&($3)->tmp_head, &($$)->u.type_declarator.u.nested.length);
		}
	;

type_declarator:
		direct_type_declarator
		{	$$ = $1;	}
	|	pointer direct_type_declarator
		{
			$$ = $2;
			bt_list_splice(&($1)->tmp_head, &($$)->u.type_declarator.pointers);
		}
	;

direct_type_declarator:
		IDENTIFIER
		{
			add_type(scanner, $1);
			$$ = make_node(scanner, NODE_TYPE_DECLARATOR);
			$$->u.type_declarator.type = TYPEDEC_ID;
			$$->u.type_declarator.u.id = $1;
		}
	|	LPAREN type_declarator RPAREN
		{
			$$ = make_node(scanner, NODE_TYPE_DECLARATOR);
			$$->u.type_declarator.type = TYPEDEC_NESTED;
			$$->u.type_declarator.u.nested.type_declarator = $2;
		}
	|	direct_type_declarator LSBRAC unary_expression RSBRAC
		{
			$$ = make_node(scanner, NODE_TYPE_DECLARATOR);
			$$->u.type_declarator.type = TYPEDEC_NESTED;
			$$->u.type_declarator.u.nested.type_declarator = $1;
			BT_INIT_LIST_HEAD(&($$)->u.type_declarator.u.nested.length);
			_bt_list_splice_tail(&($3)->tmp_head, &($$)->u.type_declarator.u.nested.length);
		}
	;

pointer:	
		STAR
		{
			$$ = make_node(scanner, NODE_POINTER);
		}
	|	STAR pointer
		{
			$$ = make_node(scanner, NODE_POINTER);
			bt_list_splice(&($2)->tmp_head, &($$)->tmp_head);
		}
	|	STAR type_qualifier_list pointer
		{
			$$ = make_node(scanner, NODE_POINTER);
			$$->u.pointer.const_qualifier = 1;
			bt_list_splice(&($3)->tmp_head, &($$)->tmp_head);
		}
	;

type_qualifier_list:
		/* pointer assumes only const type qualifier */
		CONST
	|	type_qualifier_list CONST
	;

/* 2.3: CTF-specific declarations */

ctf_assignment_expression_list:
		ctf_assignment_expression SEMICOLON
		{	$$ = $1;	}
	|	ctf_assignment_expression_list ctf_assignment_expression SEMICOLON
		{
			$$ = $1;
			bt_list_add_tail(&($2)->siblings, &($$)->tmp_head);
		}
	;

ctf_assignment_expression:
		unary_expression EQUAL unary_expression
		{
			/*
			 * Because we have left and right, cannot use
			 * set_parent_node.
			 */
			$$ = make_node(scanner, NODE_CTF_EXPRESSION);
			_bt_list_splice_tail(&($1)->tmp_head, &($$)->u.ctf_expression.left);
			if ($1->u.unary_expression.type != UNARY_STRING)
				reparent_error(scanner, "ctf_assignment_expression left expects string");
			_bt_list_splice_tail(&($3)->tmp_head, &($$)->u.ctf_expression.right);
		}
	|	unary_expression TYPEASSIGN declaration_specifiers	/* Only allow struct */
		{
			/*
			 * Because we have left and right, cannot use
			 * set_parent_node.
			 */
			$$ = make_node(scanner, NODE_CTF_EXPRESSION);
			_bt_list_splice_tail(&($1)->tmp_head, &($$)->u.ctf_expression.left);
			if ($1->u.unary_expression.type != UNARY_STRING)
				reparent_error(scanner, "ctf_assignment_expression left expects string");
			bt_list_add_tail(&($3)->siblings, &($$)->u.ctf_expression.right);
		}
	|	declaration_specifiers TYPEDEF declaration_specifiers type_declarator_list
		{
			struct ctf_node *list;

			list = make_node(scanner, NODE_TYPE_SPECIFIER_LIST);
			_bt_list_splice_tail(&($1)->u.type_specifier_list.head, &list->u.type_specifier_list.head);
			_bt_list_splice_tail(&($3)->u.type_specifier_list.head, &list->u.type_specifier_list.head);
			$$ = make_node(scanner, NODE_TYPEDEF);
			($$)->u.struct_or_variant_declaration.type_specifier_list = list;
			_bt_list_splice_tail(&($4)->tmp_head, &($$)->u._typedef.type_declarators);
		}
	|	TYPEDEF declaration_specifiers type_declarator_list
		{
			struct ctf_node *list;

			$$ = make_node(scanner, NODE_TYPEDEF);
			list = make_node(scanner, NODE_TYPE_SPECIFIER_LIST);
			$$->u._typedef.type_specifier_list = list;
			_bt_list_splice_tail(&($2)->u.type_specifier_list.head, &list->u.type_specifier_list.head);
			_bt_list_splice_tail(&($3)->tmp_head, &($$)->u._typedef.type_declarators);
		}
	|	declaration_specifiers TYPEDEF type_declarator_list
		{
			struct ctf_node *list;

			list = make_node(scanner, NODE_TYPE_SPECIFIER_LIST);
			_bt_list_splice_tail(&($1)->u.type_specifier_list.head, &list->u.type_specifier_list.head);
			$$ = make_node(scanner, NODE_TYPEDEF);
			($$)->u.struct_or_variant_declaration.type_specifier_list = list;
			_bt_list_splice_tail(&($3)->tmp_head, &($$)->u._typedef.type_declarators);
		}
	|	TYPEALIAS declaration_specifiers abstract_declarator_list TYPEASSIGN alias_declaration_specifiers alias_abstract_declarator_list
		{
			struct ctf_node *list;

			$$ = make_node(scanner, NODE_TYPEALIAS);
			$$->u.typealias.target = make_node(scanner, NODE_TYPEALIAS_TARGET);
			$$->u.typealias.alias = make_node(scanner, NODE_TYPEALIAS_ALIAS);

			list = make_node(scanner, NODE_TYPE_SPECIFIER_LIST);
			$$->u.typealias.target->u.typealias_target.type_specifier_list = list;
			_bt_list_splice_tail(&($2)->u.type_specifier_list.head, &list->u.type_specifier_list.head);
			_bt_list_splice_tail(&($3)->tmp_head, &($$)->u.typealias.target->u.typealias_target.type_declarators);

			list = make_node(scanner, NODE_TYPE_SPECIFIER_LIST);
			$$->u.typealias.alias->u.typealias_alias.type_specifier_list = list;
			_bt_list_splice_tail(&($5)->u.type_specifier_list.head, &list->u.type_specifier_list.head);
			_bt_list_splice_tail(&($6)->tmp_head, &($$)->u.typealias.alias->u.typealias_alias.type_declarators);
		}
	;
