/*
 * ctf-visitor-semantic-validator.c
 *
 * Common Trace Format Metadata Semantic Validator.
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

#define BT_LOG_TAG "PLUGIN-CTF-METADATA-SEMANTIC-VALIDATOR-VISITOR"
#include "logging.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <babeltrace/assert-internal.h>
#include <glib.h>
#include <inttypes.h>
#include <errno.h>
#include <babeltrace/list-internal.h>
#include "scanner.h"
#include "parser.h"
#include "ast.h"

#define _bt_list_first_entry(ptr, type, member)	\
	bt_list_entry((ptr)->next, type, member)

static
int _ctf_visitor_semantic_check(int depth, struct ctf_node *node);

static
int ctf_visitor_unary_expression(int depth, struct ctf_node *node)
{
	struct ctf_node *iter;
	int is_ctf_exp = 0, is_ctf_exp_left = 0;

	switch (node->parent->type) {
	case NODE_CTF_EXPRESSION:
		is_ctf_exp = 1;
		bt_list_for_each_entry(iter, &node->parent->u.ctf_expression.left,
					siblings) {
			if (iter == node) {
				is_ctf_exp_left = 1;
				/*
				 * We are a left child of a ctf expression.
				 * We are only allowed to be a string.
				 */
				if (node->u.unary_expression.type != UNARY_STRING) {
					_BT_LOGE_LINENO(node->lineno,
						"Left child of a CTF expression is only allowed to be a string.");
					goto errperm;
				}
				break;
			}
		}
		/* Right child of a ctf expression can be any type of unary exp. */
		break;			/* OK */
	case NODE_TYPE_DECLARATOR:
		/*
		 * We are the length of a type declarator.
		 */
		switch (node->u.unary_expression.type) {
		case UNARY_UNSIGNED_CONSTANT:
		case UNARY_STRING:
			break;
		default:
			_BT_LOGE_LINENO(node->lineno,
				"Children of type declarator and `enum` can only be unsigned numeric constants or references to fields (e.g., `a.b.c`).");
			goto errperm;
		}
		break;			/* OK */

	case NODE_STRUCT:
		/*
		 * We are the size of a struct align attribute.
		 */
		switch (node->u.unary_expression.type) {
		case UNARY_UNSIGNED_CONSTANT:
			break;
		default:
			_BT_LOGE_LINENO(node->lineno,
				"Structure alignment attribute can only be an unsigned numeric constant.");
			goto errperm;
		}
		break;

	case NODE_ENUMERATOR:
		/* The enumerator's parent has validated its validity already. */
		break;			/* OK */

	case NODE_UNARY_EXPRESSION:
		/*
		 * We disallow nested unary expressions and "sbrac" unary
		 * expressions.
		 */
		_BT_LOGE_LINENO(node->lineno,
			"Nested unary expressions not allowed (`()` and `[]`).");
		goto errperm;

	case NODE_ROOT:
	case NODE_EVENT:
	case NODE_STREAM:
	case NODE_ENV:
	case NODE_TRACE:
	case NODE_CLOCK:
	case NODE_CALLSITE:
	case NODE_TYPEDEF:
	case NODE_TYPEALIAS_TARGET:
	case NODE_TYPEALIAS_ALIAS:
	case NODE_TYPEALIAS:
	case NODE_TYPE_SPECIFIER:
	case NODE_POINTER:
	case NODE_FLOATING_POINT:
	case NODE_INTEGER:
	case NODE_STRING:
	case NODE_ENUM:
	case NODE_STRUCT_OR_VARIANT_DECLARATION:
	case NODE_VARIANT:
	default:
		goto errinval;
	}

	switch (node->u.unary_expression.link) {
	case UNARY_LINK_UNKNOWN:
		/* We don't allow empty link except on the first node of the list */
		if (is_ctf_exp && _bt_list_first_entry(is_ctf_exp_left ?
					  &node->parent->u.ctf_expression.left :
					  &node->parent->u.ctf_expression.right,
					  struct ctf_node,
					  siblings) != node) {
			_BT_LOGE_LINENO(node->lineno,
				"Empty link is not allowed except on first node of unary expression (need to separate nodes with `.` or `->`).");
			goto errperm;
		}
		break;			/* OK */
	case UNARY_DOTLINK:
	case UNARY_ARROWLINK:
		/* We only allow -> and . links between children of ctf_expression. */
		if (node->parent->type != NODE_CTF_EXPRESSION) {
			_BT_LOGE_LINENO(node->lineno,
				"Links `.` and `->` are only allowed as children of CTF expression.");
			goto errperm;
		}
		/*
		 * Only strings can be separated linked by . or ->.
		 * This includes "", '' and non-quoted identifiers.
		 */
		if (node->u.unary_expression.type != UNARY_STRING) {
			_BT_LOGE_LINENO(node->lineno,
				"Links `.` and `->` are only allowed to separate strings and identifiers.");
			goto errperm;
		}
		/* We don't allow link on the first node of the list */
		if (is_ctf_exp && _bt_list_first_entry(is_ctf_exp_left ?
					  &node->parent->u.ctf_expression.left :
					  &node->parent->u.ctf_expression.right,
					  struct ctf_node,
					  siblings) == node) {
			_BT_LOGE_LINENO(node->lineno,
				"Links `.` and `->` are not allowed before first node of the unary expression list.");
			goto errperm;
		}
		break;
	case UNARY_DOTDOTDOT:
		/* We only allow ... link between children of enumerator. */
		if (node->parent->type != NODE_ENUMERATOR) {
			_BT_LOGE_LINENO(node->lineno,
				"Link `...` is only allowed within enumerator.");
			goto errperm;
		}
		/* We don't allow link on the first node of the list */
		if (_bt_list_first_entry(&node->parent->u.enumerator.values,
					  struct ctf_node,
					  siblings) == node) {
			_BT_LOGE_LINENO(node->lineno,
				"Link `...` is not allowed on the first node of the unary expression list.");
			goto errperm;
		}
		break;
	default:
		_BT_LOGE_LINENO(node->lineno,
			"Unknown expression link type: type=%d",
			node->u.unary_expression.link);
		return -EINVAL;
	}
	return 0;

errinval:
	_BT_LOGE_LINENO(node->lineno,
		"Incoherent parent node's type: node-type=%s, parent-node-type=%s",
		node_type(node), node_type(node->parent));
	return -EINVAL;		/* Incoherent structure */

errperm:
	_BT_LOGE_LINENO(node->lineno,
		"Semantic error: node-type=%s, parent-node-type=%s",
		node_type(node), node_type(node->parent));
	return -EPERM;		/* Structure not allowed */
}

static
int ctf_visitor_type_specifier_list(int depth, struct ctf_node *node)
{
	switch (node->parent->type) {
	case NODE_CTF_EXPRESSION:
	case NODE_TYPE_DECLARATOR:
	case NODE_TYPEDEF:
	case NODE_TYPEALIAS_TARGET:
	case NODE_TYPEALIAS_ALIAS:
	case NODE_ENUM:
	case NODE_STRUCT_OR_VARIANT_DECLARATION:
	case NODE_ROOT:
		break;			/* OK */

	case NODE_EVENT:
	case NODE_STREAM:
	case NODE_ENV:
	case NODE_TRACE:
	case NODE_CLOCK:
	case NODE_CALLSITE:
	case NODE_UNARY_EXPRESSION:
	case NODE_TYPEALIAS:
	case NODE_TYPE_SPECIFIER:
	case NODE_TYPE_SPECIFIER_LIST:
	case NODE_POINTER:
	case NODE_FLOATING_POINT:
	case NODE_INTEGER:
	case NODE_STRING:
	case NODE_ENUMERATOR:
	case NODE_VARIANT:
	case NODE_STRUCT:
	default:
		goto errinval;
	}
	return 0;
errinval:
	_BT_LOGE_LINENO(node->lineno,
		"Incoherent parent node's type: node-type=%s, parent-node-type=%s",
		node_type(node), node_type(node->parent));
	return -EINVAL;		/* Incoherent structure */
}

static
int ctf_visitor_type_specifier(int depth, struct ctf_node *node)
{
	switch (node->parent->type) {
	case NODE_TYPE_SPECIFIER_LIST:
		break;			/* OK */

	case NODE_CTF_EXPRESSION:
	case NODE_TYPE_DECLARATOR:
	case NODE_TYPEDEF:
	case NODE_TYPEALIAS_TARGET:
	case NODE_TYPEALIAS_ALIAS:
	case NODE_ENUM:
	case NODE_STRUCT_OR_VARIANT_DECLARATION:
	case NODE_ROOT:
	case NODE_EVENT:
	case NODE_STREAM:
	case NODE_ENV:
	case NODE_TRACE:
	case NODE_CLOCK:
	case NODE_CALLSITE:
	case NODE_UNARY_EXPRESSION:
	case NODE_TYPEALIAS:
	case NODE_TYPE_SPECIFIER:
	case NODE_POINTER:
	case NODE_FLOATING_POINT:
	case NODE_INTEGER:
	case NODE_STRING:
	case NODE_ENUMERATOR:
	case NODE_VARIANT:
	case NODE_STRUCT:
	default:
		goto errinval;
	}
	return 0;
errinval:
	_BT_LOGE_LINENO(node->lineno,
		"Incoherent parent node's type: node-type=%s, parent-node-type=%s",
		node_type(node), node_type(node->parent));
	return -EINVAL;		/* Incoherent structure */
}

static
int ctf_visitor_type_declarator(int depth, struct ctf_node *node)
{
	int ret = 0;
	struct ctf_node *iter;

	depth++;

	switch (node->parent->type) {
	case NODE_TYPE_DECLARATOR:
		/*
		 * A nested type declarator is not allowed to contain pointers.
		 */
		if (!bt_list_empty(&node->u.type_declarator.pointers))
			goto errperm;
		break;			/* OK */
	case NODE_TYPEALIAS_TARGET:
		break;			/* OK */
	case NODE_TYPEALIAS_ALIAS:
		/*
		 * Only accept alias name containing:
		 * - identifier
		 * - identifier *   (any number of pointers)
		 * NOT accepting alias names containing [] (would otherwise
		 * cause semantic clash for later declarations of
		 * arrays/sequences of elements, where elements could be
		 * arrays/sequences themselves (if allowed in typealias).
		 * NOT accepting alias with identifier. The declarator should
		 * be either empty or contain pointer(s).
		 */
		if (node->u.type_declarator.type == TYPEDEC_NESTED)
			goto errperm;
		bt_list_for_each_entry(iter, &node->parent->u.typealias_alias.type_specifier_list->u.type_specifier_list.head,
					siblings) {
			switch (iter->u.type_specifier.type) {
			case TYPESPEC_FLOATING_POINT:
			case TYPESPEC_INTEGER:
			case TYPESPEC_STRING:
			case TYPESPEC_STRUCT:
			case TYPESPEC_VARIANT:
			case TYPESPEC_ENUM:
				if (bt_list_empty(&node->u.type_declarator.pointers))
					goto errperm;
				break;
			default:
				break;
			}
		}
		if (node->u.type_declarator.type == TYPEDEC_ID &&
		    node->u.type_declarator.u.id != NULL)
			goto errperm;
		break;			/* OK */
	case NODE_TYPEDEF:
	case NODE_STRUCT_OR_VARIANT_DECLARATION:
		break;			/* OK */

	case NODE_ROOT:
	case NODE_EVENT:
	case NODE_STREAM:
	case NODE_ENV:
	case NODE_TRACE:
	case NODE_CLOCK:
	case NODE_CALLSITE:
	case NODE_CTF_EXPRESSION:
	case NODE_UNARY_EXPRESSION:
	case NODE_TYPEALIAS:
	case NODE_TYPE_SPECIFIER:
	case NODE_POINTER:
	case NODE_FLOATING_POINT:
	case NODE_INTEGER:
	case NODE_STRING:
	case NODE_ENUMERATOR:
	case NODE_ENUM:
	case NODE_VARIANT:
	case NODE_STRUCT:
	default:
		goto errinval;
	}

	bt_list_for_each_entry(iter, &node->u.type_declarator.pointers,
				siblings) {
		ret = _ctf_visitor_semantic_check(depth + 1, iter);
		if (ret)
			return ret;
	}

	switch (node->u.type_declarator.type) {
	case TYPEDEC_ID:
		break;
	case TYPEDEC_NESTED:
	{
		if (node->u.type_declarator.u.nested.type_declarator) {
			ret = _ctf_visitor_semantic_check(depth + 1,
				node->u.type_declarator.u.nested.type_declarator);
			if (ret)
				return ret;
		}
		if (!node->u.type_declarator.u.nested.abstract_array) {
			bt_list_for_each_entry(iter, &node->u.type_declarator.u.nested.length,
						siblings) {
				if (iter->type != NODE_UNARY_EXPRESSION) {
					_BT_LOGE_LINENO(node->lineno,
						"Expecting unary expression as length: node-type=%s",
						node_type(iter));
					return -EINVAL;
				}
				ret = _ctf_visitor_semantic_check(depth + 1, iter);
				if (ret)
					return ret;
			}
		} else {
			if (node->parent->type == NODE_TYPEALIAS_TARGET) {
				_BT_LOGE_LINENO(node->lineno,
					"Abstract array declarator not permitted as target of type alias.");
				return -EINVAL;
			}
		}
		if (node->u.type_declarator.bitfield_len) {
			ret = _ctf_visitor_semantic_check(depth + 1,
				node->u.type_declarator.bitfield_len);
			if (ret)
				return ret;
		}
		break;
	}
	case TYPEDEC_UNKNOWN:
	default:
		_BT_LOGE_LINENO(node->lineno,
			"Unknown type declarator: type=%d",
			node->u.type_declarator.type);
		return -EINVAL;
	}
	depth--;
	return 0;

errinval:
	_BT_LOGE_LINENO(node->lineno,
		"Incoherent parent node's type: node-type=%s, parent-node-type=%s",
		node_type(node), node_type(node->parent));
	return -EINVAL;		/* Incoherent structure */

errperm:
	_BT_LOGE_LINENO(node->lineno,
		"Semantic error: node-type=%s, parent-node-type=%s",
		node_type(node), node_type(node->parent));
	return -EPERM;		/* Structure not allowed */
}

static
int _ctf_visitor_semantic_check(int depth, struct ctf_node *node)
{
	int ret = 0;
	struct ctf_node *iter;

	if (node->visited)
		return 0;

	switch (node->type) {
	case NODE_ROOT:
		bt_list_for_each_entry(iter, &node->u.root.declaration_list, siblings) {
			ret = _ctf_visitor_semantic_check(depth + 1, iter);
			if (ret)
				return ret;
		}
		bt_list_for_each_entry(iter, &node->u.root.trace, siblings) {
			ret = _ctf_visitor_semantic_check(depth + 1, iter);
			if (ret)
				return ret;
		}
		bt_list_for_each_entry(iter, &node->u.root.stream, siblings) {
			ret = _ctf_visitor_semantic_check(depth + 1, iter);
			if (ret)
				return ret;
		}
		bt_list_for_each_entry(iter, &node->u.root.event, siblings) {
			ret = _ctf_visitor_semantic_check(depth + 1, iter);
			if (ret)
				return ret;
		}
		break;

	case NODE_EVENT:
		switch (node->parent->type) {
		case NODE_ROOT:
			break;			/* OK */
		default:
			goto errinval;
		}

		bt_list_for_each_entry(iter, &node->u.event.declaration_list, siblings) {
			ret = _ctf_visitor_semantic_check(depth + 1, iter);
			if (ret)
				return ret;
		}
		break;
	case NODE_STREAM:
		switch (node->parent->type) {
		case NODE_ROOT:
			break;			/* OK */
		default:
			goto errinval;
		}

		bt_list_for_each_entry(iter, &node->u.stream.declaration_list, siblings) {
			ret = _ctf_visitor_semantic_check(depth + 1, iter);
			if (ret)
				return ret;
		}
		break;
	case NODE_ENV:
		switch (node->parent->type) {
		case NODE_ROOT:
			break;			/* OK */
		default:
			goto errinval;
		}

		bt_list_for_each_entry(iter, &node->u.env.declaration_list, siblings) {
			ret = _ctf_visitor_semantic_check(depth + 1, iter);
			if (ret)
				return ret;
		}
		break;
	case NODE_TRACE:
		switch (node->parent->type) {
		case NODE_ROOT:
			break;			/* OK */
		default:
			goto errinval;
		}

		bt_list_for_each_entry(iter, &node->u.trace.declaration_list, siblings) {
			ret = _ctf_visitor_semantic_check(depth + 1, iter);
			if (ret)
				return ret;
		}
		break;
	case NODE_CLOCK:
		switch (node->parent->type) {
		case NODE_ROOT:
			break;			/* OK */
		default:
			goto errinval;
		}

		bt_list_for_each_entry(iter, &node->u.clock.declaration_list, siblings) {
			ret = _ctf_visitor_semantic_check(depth + 1, iter);
			if (ret)
				return ret;
		}
		break;
	case NODE_CALLSITE:
		switch (node->parent->type) {
		case NODE_ROOT:
			break;			/* OK */
		default:
			goto errinval;
		}

		bt_list_for_each_entry(iter, &node->u.callsite.declaration_list, siblings) {
			ret = _ctf_visitor_semantic_check(depth + 1, iter);
			if (ret)
				return ret;
		}
		break;

	case NODE_CTF_EXPRESSION:
		switch (node->parent->type) {
		case NODE_ROOT:
		case NODE_EVENT:
		case NODE_STREAM:
		case NODE_ENV:
		case NODE_TRACE:
		case NODE_CLOCK:
		case NODE_CALLSITE:
		case NODE_FLOATING_POINT:
		case NODE_INTEGER:
		case NODE_STRING:
			break;			/* OK */

		case NODE_CTF_EXPRESSION:
		case NODE_UNARY_EXPRESSION:
		case NODE_TYPEDEF:
		case NODE_TYPEALIAS_TARGET:
		case NODE_TYPEALIAS_ALIAS:
		case NODE_STRUCT_OR_VARIANT_DECLARATION:
		case NODE_TYPEALIAS:
		case NODE_TYPE_SPECIFIER:
		case NODE_TYPE_SPECIFIER_LIST:
		case NODE_POINTER:
		case NODE_TYPE_DECLARATOR:
		case NODE_ENUMERATOR:
		case NODE_ENUM:
		case NODE_VARIANT:
		case NODE_STRUCT:
		default:
			goto errinval;
		}

		depth++;
		bt_list_for_each_entry(iter, &node->u.ctf_expression.left, siblings) {
			ret = _ctf_visitor_semantic_check(depth + 1, iter);
			if (ret)
				return ret;
		}
		bt_list_for_each_entry(iter, &node->u.ctf_expression.right, siblings) {
			ret = _ctf_visitor_semantic_check(depth + 1, iter);
			if (ret)
				return ret;
		}
		depth--;
		break;
	case NODE_UNARY_EXPRESSION:
		return ctf_visitor_unary_expression(depth, node);

	case NODE_TYPEDEF:
		switch (node->parent->type) {
		case NODE_ROOT:
		case NODE_EVENT:
		case NODE_STREAM:
		case NODE_TRACE:
		case NODE_VARIANT:
		case NODE_STRUCT:
			break;			/* OK */

		case NODE_CTF_EXPRESSION:
		case NODE_UNARY_EXPRESSION:
		case NODE_TYPEDEF:
		case NODE_TYPEALIAS_TARGET:
		case NODE_TYPEALIAS_ALIAS:
		case NODE_TYPEALIAS:
		case NODE_STRUCT_OR_VARIANT_DECLARATION:
		case NODE_TYPE_SPECIFIER:
		case NODE_TYPE_SPECIFIER_LIST:
		case NODE_POINTER:
		case NODE_TYPE_DECLARATOR:
		case NODE_FLOATING_POINT:
		case NODE_INTEGER:
		case NODE_STRING:
		case NODE_ENUMERATOR:
		case NODE_ENUM:
		case NODE_CLOCK:
		case NODE_CALLSITE:
		case NODE_ENV:
		default:
			goto errinval;
		}

		depth++;
		ret = _ctf_visitor_semantic_check(depth + 1,
			node->u._typedef.type_specifier_list);
		if (ret)
			return ret;
		bt_list_for_each_entry(iter, &node->u._typedef.type_declarators, siblings) {
			ret = _ctf_visitor_semantic_check(depth + 1, iter);
			if (ret)
				return ret;
		}
		depth--;
		break;
	case NODE_TYPEALIAS_TARGET:
	{
		int nr_declarators;

		switch (node->parent->type) {
		case NODE_TYPEALIAS:
			break;			/* OK */
		default:
			goto errinval;
		}

		depth++;
		ret = _ctf_visitor_semantic_check(depth + 1,
			node->u.typealias_target.type_specifier_list);
		if (ret)
			return ret;
		nr_declarators = 0;
		bt_list_for_each_entry(iter, &node->u.typealias_target.type_declarators, siblings) {
			ret = _ctf_visitor_semantic_check(depth + 1, iter);
			if (ret)
				return ret;
			nr_declarators++;
		}
		if (nr_declarators > 1) {
			_BT_LOGE_LINENO(node->lineno,
				"Too many declarators in type alias's name (maximum is 1): count=%d",
				nr_declarators);
			return -EINVAL;
		}
		depth--;
		break;
	}
	case NODE_TYPEALIAS_ALIAS:
	{
		int nr_declarators;

		switch (node->parent->type) {
		case NODE_TYPEALIAS:
			break;			/* OK */
		default:
			goto errinval;
		}

		depth++;
		ret = _ctf_visitor_semantic_check(depth + 1,
			node->u.typealias_alias.type_specifier_list);
		if (ret)
			return ret;
		nr_declarators = 0;
		bt_list_for_each_entry(iter, &node->u.typealias_alias.type_declarators, siblings) {
			ret = _ctf_visitor_semantic_check(depth + 1, iter);
			if (ret)
				return ret;
			nr_declarators++;
		}
		if (nr_declarators > 1) {
			_BT_LOGE_LINENO(node->lineno,
				"Too many declarators in type alias's name (maximum is 1): count=%d",
				nr_declarators);
			return -EINVAL;
		}
		depth--;
		break;
	}
	case NODE_TYPEALIAS:
		switch (node->parent->type) {
		case NODE_ROOT:
		case NODE_EVENT:
		case NODE_STREAM:
		case NODE_TRACE:
		case NODE_VARIANT:
		case NODE_STRUCT:
			break;			/* OK */

		case NODE_CTF_EXPRESSION:
		case NODE_UNARY_EXPRESSION:
		case NODE_TYPEDEF:
		case NODE_TYPEALIAS_TARGET:
		case NODE_TYPEALIAS_ALIAS:
		case NODE_TYPEALIAS:
		case NODE_STRUCT_OR_VARIANT_DECLARATION:
		case NODE_TYPE_SPECIFIER:
		case NODE_TYPE_SPECIFIER_LIST:
		case NODE_POINTER:
		case NODE_TYPE_DECLARATOR:
		case NODE_FLOATING_POINT:
		case NODE_INTEGER:
		case NODE_STRING:
		case NODE_ENUMERATOR:
		case NODE_ENUM:
		case NODE_CLOCK:
		case NODE_CALLSITE:
		case NODE_ENV:
		default:
			goto errinval;
		}

		ret = _ctf_visitor_semantic_check(depth + 1, node->u.typealias.target);
		if (ret)
			return ret;
		ret = _ctf_visitor_semantic_check(depth + 1, node->u.typealias.alias);
		if (ret)
			return ret;
		break;

	case NODE_TYPE_SPECIFIER_LIST:
		ret = ctf_visitor_type_specifier_list(depth, node);
		if (ret)
			return ret;
		break;
	case NODE_TYPE_SPECIFIER:
		ret = ctf_visitor_type_specifier(depth, node);
		if (ret)
			return ret;
		break;
	case NODE_POINTER:
		switch (node->parent->type) {
		case NODE_TYPE_DECLARATOR:
			break;			/* OK */
		default:
			goto errinval;
		}
		break;
	case NODE_TYPE_DECLARATOR:
		ret = ctf_visitor_type_declarator(depth, node);
		if (ret)
			return ret;
		break;

	case NODE_FLOATING_POINT:
		switch (node->parent->type) {
		case NODE_TYPE_SPECIFIER:
			break;			/* OK */
		default:
			goto errinval;

		case NODE_UNARY_EXPRESSION:
			goto errperm;
		}
		bt_list_for_each_entry(iter, &node->u.floating_point.expressions, siblings) {
			ret = _ctf_visitor_semantic_check(depth + 1, iter);
			if (ret)
				return ret;
		}
		break;
	case NODE_INTEGER:
		switch (node->parent->type) {
		case NODE_TYPE_SPECIFIER:
			break;			/* OK */
		default:
			goto errinval;

		}

		bt_list_for_each_entry(iter, &node->u.integer.expressions, siblings) {
			ret = _ctf_visitor_semantic_check(depth + 1, iter);
			if (ret)
				return ret;
		}
		break;
	case NODE_STRING:
		switch (node->parent->type) {
		case NODE_TYPE_SPECIFIER:
			break;			/* OK */
		default:
			goto errinval;

		case NODE_UNARY_EXPRESSION:
			goto errperm;
		}

		bt_list_for_each_entry(iter, &node->u.string.expressions, siblings) {
			ret = _ctf_visitor_semantic_check(depth + 1, iter);
			if (ret)
				return ret;
		}
		break;
	case NODE_ENUMERATOR:
		switch (node->parent->type) {
		case NODE_ENUM:
			break;
		default:
			goto errinval;
		}
		/*
		 * Enumerators are only allows to contain:
		 *    numeric unary expression
		 * or num. unary exp. ... num. unary exp
		 */
		{
			int count = 0;

			bt_list_for_each_entry(iter, &node->u.enumerator.values,
						siblings) {
				switch (count++) {
				case 0:	if (iter->type != NODE_UNARY_EXPRESSION
					    || (iter->u.unary_expression.type != UNARY_SIGNED_CONSTANT
						&& iter->u.unary_expression.type != UNARY_UNSIGNED_CONSTANT)
					    || iter->u.unary_expression.link != UNARY_LINK_UNKNOWN) {
						_BT_LOGE_LINENO(iter->lineno,
							"First unary expression of enumerator is unexpected.");
						goto errperm;
					}
					break;
				case 1:	if (iter->type != NODE_UNARY_EXPRESSION
					    || (iter->u.unary_expression.type != UNARY_SIGNED_CONSTANT
						&& iter->u.unary_expression.type != UNARY_UNSIGNED_CONSTANT)
					    || iter->u.unary_expression.link != UNARY_DOTDOTDOT) {
						_BT_LOGE_LINENO(iter->lineno,
							"Second unary expression of enumerator is unexpected.");
						goto errperm;
					}
					break;
				default:
					goto errperm;
				}
			}
		}

		bt_list_for_each_entry(iter, &node->u.enumerator.values, siblings) {
			ret = _ctf_visitor_semantic_check(depth + 1, iter);
			if (ret)
				return ret;
		}
		break;
	case NODE_ENUM:
		switch (node->parent->type) {
		case NODE_TYPE_SPECIFIER:
			break;			/* OK */
		default:
			goto errinval;

		case NODE_UNARY_EXPRESSION:
			goto errperm;
		}

		depth++;
		ret = _ctf_visitor_semantic_check(depth + 1, node->u._enum.container_type);
		if (ret)
			return ret;

		bt_list_for_each_entry(iter, &node->u._enum.enumerator_list, siblings) {
			ret = _ctf_visitor_semantic_check(depth + 1, iter);
			if (ret)
				return ret;
		}
		depth--;
		break;
	case NODE_STRUCT_OR_VARIANT_DECLARATION:
		switch (node->parent->type) {
		case NODE_STRUCT:
		case NODE_VARIANT:
			break;
		default:
			goto errinval;
		}
		ret = _ctf_visitor_semantic_check(depth + 1,
			node->u.struct_or_variant_declaration.type_specifier_list);
		if (ret)
			return ret;
		bt_list_for_each_entry(iter, &node->u.struct_or_variant_declaration.type_declarators, siblings) {
			ret = _ctf_visitor_semantic_check(depth + 1, iter);
			if (ret)
				return ret;
		}
		break;
	case NODE_VARIANT:
		switch (node->parent->type) {
		case NODE_TYPE_SPECIFIER:
			break;			/* OK */
		default:
			goto errinval;

		case NODE_UNARY_EXPRESSION:
			goto errperm;
		}
		bt_list_for_each_entry(iter, &node->u.variant.declaration_list, siblings) {
			ret = _ctf_visitor_semantic_check(depth + 1, iter);
			if (ret)
				return ret;
		}
		break;

	case NODE_STRUCT:
		switch (node->parent->type) {
		case NODE_TYPE_SPECIFIER:
			break;			/* OK */
		default:
			goto errinval;

		case NODE_UNARY_EXPRESSION:
			goto errperm;
		}
		bt_list_for_each_entry(iter, &node->u._struct.declaration_list, siblings) {
			ret = _ctf_visitor_semantic_check(depth + 1, iter);
			if (ret)
				return ret;
		}
		break;

	case NODE_UNKNOWN:
	default:
		_BT_LOGE_LINENO(node->lineno,
			"Unknown node type: type=%d", node->type);
		return -EINVAL;
	}
	return ret;

errinval:
	_BT_LOGE_LINENO(node->lineno,
		"Incoherent parent node's type: node-type=%s, parent-node-type=%s",
		node_type(node), node_type(node->parent));
	return -EINVAL;		/* Incoherent structure */

errperm:
	_BT_LOGE_LINENO(node->lineno,
		"Semantic error: node-type=%s, parent-node-type=%s",
		node_type(node), node_type(node->parent));
	return -EPERM;		/* Structure not allowed */
}

int ctf_visitor_semantic_check(int depth, struct ctf_node *node)
{
	int ret = 0;

	/*
	 * First make sure we create the parent links for all children. Let's
	 * take the safe route and recreate them at each validation, just in
	 * case the structure has changed.
	 */
	ret = ctf_visitor_parent_links(depth, node);
	if (ret) {
		_BT_LOGE_LINENO(node->lineno,
			"Cannot create parent links in metadata's AST: "
			"ret=%d", ret);
		goto end;
	}

	ret = _ctf_visitor_semantic_check(depth, node);
	if (ret) {
		_BT_LOGE_LINENO(node->lineno,
			"Cannot check metadata's AST semantics: "
			"ret=%d", ret);
		goto end;
	}

end:
	return ret;
}
