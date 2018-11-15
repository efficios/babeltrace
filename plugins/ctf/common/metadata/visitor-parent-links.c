/*
 * ctf-visitor-parent-links.c
 *
 * Common Trace Format Metadata Parent Link Creator.
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

#define BT_LOG_TAG "PLUGIN-CTF-METADATA-PARENT-LINKS-VISITOR"
#include "logging.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <babeltrace/assert-internal.h>
#include <glib.h>
#include <inttypes.h>
#include <errno.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/list-internal.h>
#include "scanner.h"
#include "parser.h"
#include "ast.h"

static
int ctf_visitor_unary_expression(int depth, struct ctf_node *node)
{
	int ret = 0;

	switch (node->u.unary_expression.link) {
	case UNARY_LINK_UNKNOWN:
	case UNARY_DOTLINK:
	case UNARY_ARROWLINK:
	case UNARY_DOTDOTDOT:
		break;
	default:
		_BT_LOGE_LINENO(node->lineno,
			"Unknown expression link type: type=%d\n",
			node->u.unary_expression.link);
		return -EINVAL;
	}

	switch (node->u.unary_expression.type) {
	case UNARY_STRING:
	case UNARY_SIGNED_CONSTANT:
	case UNARY_UNSIGNED_CONSTANT:
		break;
	case UNARY_SBRAC:
		node->u.unary_expression.u.sbrac_exp->parent = node;
		ret = ctf_visitor_unary_expression(depth + 1,
			node->u.unary_expression.u.sbrac_exp);
		if (ret)
			return ret;
		break;

	case UNARY_UNKNOWN:
	default:
		_BT_LOGE_LINENO(node->lineno,
			"Unknown expression link type: type=%d\n",
			node->u.unary_expression.link);
		return -EINVAL;
	}
	return 0;
}

static
int ctf_visitor_type_specifier(int depth, struct ctf_node *node)
{
	int ret;

	switch (node->u.field_class_specifier.type) {
	case TYPESPEC_VOID:
	case TYPESPEC_CHAR:
	case TYPESPEC_SHORT:
	case TYPESPEC_INT:
	case TYPESPEC_LONG:
	case TYPESPEC_FLOAT:
	case TYPESPEC_DOUBLE:
	case TYPESPEC_SIGNED:
	case TYPESPEC_UNSIGNED:
	case TYPESPEC_BOOL:
	case TYPESPEC_COMPLEX:
	case TYPESPEC_IMAGINARY:
	case TYPESPEC_CONST:
	case TYPESPEC_ID_TYPE:
		break;
	case TYPESPEC_FLOATING_POINT:
	case TYPESPEC_INTEGER:
	case TYPESPEC_STRING:
	case TYPESPEC_STRUCT:
	case TYPESPEC_VARIANT:
	case TYPESPEC_ENUM:
		node->u.field_class_specifier.node->parent = node;
		ret = ctf_visitor_parent_links(depth + 1, node->u.field_class_specifier.node);
		if (ret)
			return ret;
		break;

	case TYPESPEC_UNKNOWN:
	default:
		_BT_LOGE_LINENO(node->lineno,
			"Unknown type specifier: type=%d\n",
			node->u.field_class_specifier.type);
		return -EINVAL;
	}
	return 0;
}

static
int ctf_visitor_field_class_declarator(int depth, struct ctf_node *node)
{
	int ret = 0;
	struct ctf_node *iter;

	depth++;

	bt_list_for_each_entry(iter, &node->u.field_class_declarator.pointers,
				siblings) {
		iter->parent = node;
		ret = ctf_visitor_parent_links(depth + 1, iter);
		if (ret)
			return ret;
	}

	switch (node->u.field_class_declarator.type) {
	case TYPEDEC_ID:
		break;
	case TYPEDEC_NESTED:
		if (node->u.field_class_declarator.u.nested.field_class_declarator) {
			node->u.field_class_declarator.u.nested.field_class_declarator->parent = node;
			ret = ctf_visitor_parent_links(depth + 1,
				node->u.field_class_declarator.u.nested.field_class_declarator);
			if (ret)
				return ret;
		}
		if (!node->u.field_class_declarator.u.nested.abstract_array) {
			bt_list_for_each_entry(iter, &node->u.field_class_declarator.u.nested.length,
						siblings) {
				iter->parent = node;
				ret = ctf_visitor_parent_links(depth + 1, iter);
				if (ret)
					return ret;
			}
		}
		if (node->u.field_class_declarator.bitfield_len) {
			node->u.field_class_declarator.bitfield_len = node;
			ret = ctf_visitor_parent_links(depth + 1,
				node->u.field_class_declarator.bitfield_len);
			if (ret)
				return ret;
		}
		break;
	case TYPEDEC_UNKNOWN:
	default:
		_BT_LOGE_LINENO(node->lineno,
			"Unknown type declarator: type=%d\n",
			node->u.field_class_declarator.type);
		return -EINVAL;
	}
	depth--;
	return 0;
}

int ctf_visitor_parent_links(int depth, struct ctf_node *node)
{
	int ret = 0;
	struct ctf_node *iter;

	if (node->visited)
		return 0;

	switch (node->type) {
	case NODE_ROOT:
		bt_list_for_each_entry(iter, &node->u.root.declaration_list, siblings) {
			iter->parent = node;
			ret = ctf_visitor_parent_links(depth + 1, iter);
			if (ret)
				return ret;
		}
		bt_list_for_each_entry(iter, &node->u.root.trace, siblings) {
			iter->parent = node;
			ret = ctf_visitor_parent_links(depth + 1, iter);
			if (ret)
				return ret;
		}
		bt_list_for_each_entry(iter, &node->u.root.stream, siblings) {
			iter->parent = node;
			ret = ctf_visitor_parent_links(depth + 1, iter);
			if (ret)
				return ret;
		}
		bt_list_for_each_entry(iter, &node->u.root.event, siblings) {
			iter->parent = node;
			ret = ctf_visitor_parent_links(depth + 1, iter);
			if (ret)
				return ret;
		}
		bt_list_for_each_entry(iter, &node->u.root.clock, siblings) {
			iter->parent = node;
			ret = ctf_visitor_parent_links(depth + 1, iter);
			if (ret)
				return ret;
		}
		bt_list_for_each_entry(iter, &node->u.root.callsite, siblings) {
			iter->parent = node;
			ret = ctf_visitor_parent_links(depth + 1, iter);
			if (ret)
				return ret;
		}
		break;

	case NODE_EVENT:
		bt_list_for_each_entry(iter, &node->u.event.declaration_list, siblings) {
			iter->parent = node;
			ret = ctf_visitor_parent_links(depth + 1, iter);
			if (ret)
				return ret;
		}
		break;
	case NODE_STREAM:
		bt_list_for_each_entry(iter, &node->u.stream.declaration_list, siblings) {
			iter->parent = node;
			ret = ctf_visitor_parent_links(depth + 1, iter);
			if (ret)
				return ret;
		}
		break;
	case NODE_ENV:
		bt_list_for_each_entry(iter, &node->u.env.declaration_list, siblings) {
			iter->parent = node;
			ret = ctf_visitor_parent_links(depth + 1, iter);
			if (ret)
				return ret;
		}
		break;
	case NODE_TRACE:
		bt_list_for_each_entry(iter, &node->u.trace.declaration_list, siblings) {
			iter->parent = node;
			ret = ctf_visitor_parent_links(depth + 1, iter);
			if (ret)
				return ret;
		}
		break;
	case NODE_CLOCK:
		bt_list_for_each_entry(iter, &node->u.clock.declaration_list, siblings) {
			iter->parent = node;
			ret = ctf_visitor_parent_links(depth + 1, iter);
			if (ret)
				return ret;
		}
		break;
	case NODE_CALLSITE:
		bt_list_for_each_entry(iter, &node->u.callsite.declaration_list, siblings) {
			iter->parent = node;
			ret = ctf_visitor_parent_links(depth + 1, iter);
			if (ret)
				return ret;
		}
		break;

	case NODE_CTF_EXPRESSION:
		depth++;
		bt_list_for_each_entry(iter, &node->u.ctf_expression.left, siblings) {
			iter->parent = node;
			ret = ctf_visitor_parent_links(depth + 1, iter);
			if (ret)
				return ret;
		}
		bt_list_for_each_entry(iter, &node->u.ctf_expression.right, siblings) {
			iter->parent = node;
			ret = ctf_visitor_parent_links(depth + 1, iter);
			if (ret)
				return ret;
		}
		depth--;
		break;
	case NODE_UNARY_EXPRESSION:
		return ctf_visitor_unary_expression(depth, node);

	case NODE_TYPEDEF:
		depth++;
		node->u.field_class_def.field_class_specifier_list->parent = node;
		ret = ctf_visitor_parent_links(depth + 1, node->u.field_class_def.field_class_specifier_list);
		if (ret)
			return ret;
		bt_list_for_each_entry(iter, &node->u.field_class_def.field_class_declarators, siblings) {
			iter->parent = node;
			ret = ctf_visitor_parent_links(depth + 1, iter);
			if (ret)
				return ret;
		}
		depth--;
		break;
	case NODE_TYPEALIAS_TARGET:
		depth++;
		node->u.field_class_alias_target.field_class_specifier_list->parent = node;
		ret = ctf_visitor_parent_links(depth + 1, node->u.field_class_alias_target.field_class_specifier_list);
		if (ret)
			return ret;
		bt_list_for_each_entry(iter, &node->u.field_class_alias_target.field_class_declarators, siblings) {
			iter->parent = node;
			ret = ctf_visitor_parent_links(depth + 1, iter);
			if (ret)
				return ret;
		}
		depth--;
		break;
	case NODE_TYPEALIAS_ALIAS:
		depth++;
		node->u.field_class_alias_name.field_class_specifier_list->parent = node;
		ret = ctf_visitor_parent_links(depth + 1, node->u.field_class_alias_name.field_class_specifier_list);
		if (ret)
			return ret;
		bt_list_for_each_entry(iter, &node->u.field_class_alias_name.field_class_declarators, siblings) {
			iter->parent = node;
			ret = ctf_visitor_parent_links(depth + 1, iter);
			if (ret)
				return ret;
		}
		depth--;
		break;
	case NODE_TYPEALIAS:
		node->u.field_class_alias.target->parent = node;
		ret = ctf_visitor_parent_links(depth + 1, node->u.field_class_alias.target);
		if (ret)
			return ret;
		node->u.field_class_alias.alias->parent = node;
		ret = ctf_visitor_parent_links(depth + 1, node->u.field_class_alias.alias);
		if (ret)
			return ret;
		break;

	case NODE_TYPE_SPECIFIER_LIST:
		bt_list_for_each_entry(iter, &node->u.field_class_specifier_list.head, siblings) {
			iter->parent = node;
			ret = ctf_visitor_parent_links(depth + 1, iter);
			if (ret)
				return ret;
		}
		break;

	case NODE_TYPE_SPECIFIER:
		ret = ctf_visitor_type_specifier(depth, node);
		if (ret)
			return ret;
		break;
	case NODE_POINTER:
		break;
	case NODE_TYPE_DECLARATOR:
		ret = ctf_visitor_field_class_declarator(depth, node);
		if (ret)
			return ret;
		break;

	case NODE_FLOATING_POINT:
		bt_list_for_each_entry(iter, &node->u.floating_point.expressions, siblings) {
			iter->parent = node;
			ret = ctf_visitor_parent_links(depth + 1, iter);
			if (ret)
				return ret;
		}
		break;
	case NODE_INTEGER:
		bt_list_for_each_entry(iter, &node->u.integer.expressions, siblings) {
			iter->parent = node;
			ret = ctf_visitor_parent_links(depth + 1, iter);
			if (ret)
				return ret;
		}
		break;
	case NODE_STRING:
		bt_list_for_each_entry(iter, &node->u.string.expressions, siblings) {
			iter->parent = node;
			ret = ctf_visitor_parent_links(depth + 1, iter);
			if (ret)
				return ret;
		}
		break;
	case NODE_ENUMERATOR:
		bt_list_for_each_entry(iter, &node->u.enumerator.values, siblings) {
			iter->parent = node;
			ret = ctf_visitor_parent_links(depth + 1, iter);
			if (ret)
				return ret;
		}
		break;
	case NODE_ENUM:
		depth++;
		if (node->u._enum.container_field_class) {
			ret = ctf_visitor_parent_links(depth + 1, node->u._enum.container_field_class);
			if (ret)
				return ret;
		}

		bt_list_for_each_entry(iter, &node->u._enum.enumerator_list, siblings) {
			iter->parent = node;
			ret = ctf_visitor_parent_links(depth + 1, iter);
			if (ret)
				return ret;
		}
		depth--;
		break;
	case NODE_STRUCT_OR_VARIANT_DECLARATION:
		node->u.struct_or_variant_declaration.field_class_specifier_list->parent = node;
		ret = ctf_visitor_parent_links(depth + 1,
			node->u.struct_or_variant_declaration.field_class_specifier_list);
		if (ret)
			return ret;
		bt_list_for_each_entry(iter, &node->u.struct_or_variant_declaration.field_class_declarators, siblings) {
			iter->parent = node;
			ret = ctf_visitor_parent_links(depth + 1, iter);
			if (ret)
				return ret;
		}
		break;
	case NODE_VARIANT:
		bt_list_for_each_entry(iter, &node->u.variant.declaration_list, siblings) {
			iter->parent = node;
			ret = ctf_visitor_parent_links(depth + 1, iter);
			if (ret)
				return ret;
		}
		break;
	case NODE_STRUCT:
		bt_list_for_each_entry(iter, &node->u._struct.declaration_list, siblings) {
			iter->parent = node;
			ret = ctf_visitor_parent_links(depth + 1, iter);
			if (ret)
				return ret;
		}
		bt_list_for_each_entry(iter, &node->u._struct.min_align,
					siblings) {
			iter->parent = node;
			ret = ctf_visitor_parent_links(depth + 1, iter);
			if (ret)
				return ret;
		}
		break;

	case NODE_UNKNOWN:
	default:
		_BT_LOGE_LINENO(node->lineno,
			"Unknown node type: type=%d\n", node->type);
		return -EINVAL;
	}
	return ret;
}
