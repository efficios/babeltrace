/*
 * ctf-visitor-generate-io-struct.c
 *
 * Common Trace Format Metadata Visitor (generate I/O structures).
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
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <glib.h>
#include <inttypes.h>
#include <errno.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/list.h>
#include <babeltrace/types.h>
#include <babeltrace/ctf/metadata.h>
#include <babeltrace/compat/uuid.h>
#include <babeltrace/endian.h>
#include <babeltrace/ctf/events-internal.h>
#include "ctf-scanner.h"
#include "ctf-parser.h"
#include "ctf-ast.h"

#define fprintf_dbg(fd, fmt, args...)	fprintf(fd, "%s: " fmt, __func__, ## args)

#define _bt_list_first_entry(ptr, type, member)	\
	bt_list_entry((ptr)->next, type, member)

struct last_enum_value {
	union {
		int64_t s;
		uint64_t u;
	} u;
};

int opt_clock_force_correlate;

static
struct bt_declaration *ctf_type_specifier_list_visit(FILE *fd,
		int depth, struct ctf_node *type_specifier_list,
		struct declaration_scope *declaration_scope,
		struct ctf_trace *trace);

static
int ctf_stream_visit(FILE *fd, int depth, struct ctf_node *node,
		     struct declaration_scope *parent_declaration_scope, struct ctf_trace *trace);

static
int is_unary_string(struct bt_list_head *head)
{
	struct ctf_node *node;

	bt_list_for_each_entry(node, head, siblings) {
		if (node->type != NODE_UNARY_EXPRESSION)
			return 0;
		if (node->u.unary_expression.type != UNARY_STRING)
			return 0;
	}
	return 1;
}

/*
 * String returned must be freed by the caller using g_free.
 */
static
char *concatenate_unary_strings(struct bt_list_head *head)
{
	struct ctf_node *node;
	GString *str;
	int i = 0;

	str = g_string_new("");
	bt_list_for_each_entry(node, head, siblings) {
		char *src_string;

		if (node->type != NODE_UNARY_EXPRESSION
				|| node->u.unary_expression.type != UNARY_STRING
				|| !((node->u.unary_expression.link != UNARY_LINK_UNKNOWN)
					^ (i == 0)))
			return NULL;
		switch (node->u.unary_expression.link) {
		case UNARY_DOTLINK:
			g_string_append(str, ".");
			break;
		case UNARY_ARROWLINK:
			g_string_append(str, "->");
			break;
		case UNARY_DOTDOTDOT:
			g_string_append(str, "...");
			break;
		default:
			break;
		}
		src_string = node->u.unary_expression.u.string;
		g_string_append(str, src_string);
		i++;
	}
	return g_string_free(str, FALSE);
}

static
GQuark get_map_clock_name_value(struct bt_list_head *head)
{
	struct ctf_node *node;
	const char *name = NULL;
	int i = 0;

	bt_list_for_each_entry(node, head, siblings) {
		char *src_string;

		if (node->type != NODE_UNARY_EXPRESSION
			|| node->u.unary_expression.type != UNARY_STRING
			|| !((node->u.unary_expression.link != UNARY_LINK_UNKNOWN)
				^ (i == 0)))
			return 0;
		/* needs to be chained with . */
		switch (node->u.unary_expression.link) {
		case UNARY_DOTLINK:
			break;
		case UNARY_ARROWLINK:
		case UNARY_DOTDOTDOT:
			return 0;
		default:
			break;
		}
		src_string = node->u.unary_expression.u.string;
		switch (i) {
		case 0:	if (strcmp("clock", src_string) != 0) {
				return 0;
			}
			break;
		case 1:	name = src_string;
			break;
		case 2:	if (strcmp("value", src_string) != 0) {
				return 0;
			}
			break;
		default:
			return 0;	/* extra identifier, unknown */
		}
		i++;
	}
	return g_quark_from_string(name);
}

static
int is_unary_unsigned(struct bt_list_head *head)
{
	struct ctf_node *node;

	bt_list_for_each_entry(node, head, siblings) {
		if (node->type != NODE_UNARY_EXPRESSION)
			return 0;
		if (node->u.unary_expression.type != UNARY_UNSIGNED_CONSTANT)
			return 0;
	}
	return 1;
}

static
int get_unary_unsigned(struct bt_list_head *head, uint64_t *value)
{
	struct ctf_node *node;
	int i = 0;

	bt_list_for_each_entry(node, head, siblings) {
		if (node->type != NODE_UNARY_EXPRESSION
				|| node->u.unary_expression.type != UNARY_UNSIGNED_CONSTANT
				|| node->u.unary_expression.link != UNARY_LINK_UNKNOWN
				|| i != 0)
			return -EINVAL;
		*value = node->u.unary_expression.u.unsigned_constant;
		i++;
	}
	return 0;
}

static
int is_unary_signed(struct bt_list_head *head)
{
	struct ctf_node *node;

	bt_list_for_each_entry(node, head, siblings) {
		if (node->type != NODE_UNARY_EXPRESSION)
			return 0;
		if (node->u.unary_expression.type != UNARY_SIGNED_CONSTANT)
			return 0;
	}
	return 1;
}

static
int get_unary_signed(struct bt_list_head *head, int64_t *value)
{
	struct ctf_node *node;
	int i = 0;

	bt_list_for_each_entry(node, head, siblings) {
		if (node->type != NODE_UNARY_EXPRESSION
				|| node->u.unary_expression.type != UNARY_UNSIGNED_CONSTANT
				|| (node->u.unary_expression.type != UNARY_UNSIGNED_CONSTANT && node->u.unary_expression.type != UNARY_SIGNED_CONSTANT)
				|| node->u.unary_expression.link != UNARY_LINK_UNKNOWN
				|| i != 0)
			return -EINVAL;
		switch (node->u.unary_expression.type) {
		case UNARY_UNSIGNED_CONSTANT:
			*value = (int64_t) node->u.unary_expression.u.unsigned_constant;
			break;
		case UNARY_SIGNED_CONSTANT:
			*value = node->u.unary_expression.u.signed_constant;
			break;
		default:
			return -EINVAL;
		}
		i++;
	}
	return 0;
}

static
int get_unary_uuid(struct bt_list_head *head, unsigned char *uuid)
{
	struct ctf_node *node;
	int i = 0;
	int ret = -1;

	bt_list_for_each_entry(node, head, siblings) {
		const char *src_string;

		if (node->type != NODE_UNARY_EXPRESSION
				|| node->u.unary_expression.type != UNARY_STRING
				|| node->u.unary_expression.link != UNARY_LINK_UNKNOWN
				|| i != 0)
			return -EINVAL;
		src_string = node->u.unary_expression.u.string;
		ret = bt_uuid_parse(src_string, uuid);
	}
	return ret;
}

static
struct ctf_stream_declaration *trace_stream_lookup(struct ctf_trace *trace, uint64_t stream_id)
{
	if (trace->streams->len <= stream_id)
		return NULL;
	return g_ptr_array_index(trace->streams, stream_id);
}

static
struct ctf_event_declaration *stream_event_lookup(struct ctf_stream_declaration *stream, uint64_t event_id)
{
	if (stream->events_by_id->len <= event_id)
		return NULL;
	return g_ptr_array_index(stream->events_by_id, event_id);
}

static
struct ctf_clock *trace_clock_lookup(struct ctf_trace *trace, GQuark clock_name)
{
	return g_hash_table_lookup(trace->parent.clocks, (gpointer) (unsigned long) clock_name);
}

static
int visit_type_specifier(FILE *fd, struct ctf_node *type_specifier, GString *str)
{
	if (type_specifier->type != NODE_TYPE_SPECIFIER)
		return -EINVAL;

	switch (type_specifier->u.type_specifier.type) {
	case TYPESPEC_VOID:
		g_string_append(str, "void");
		break;
	case TYPESPEC_CHAR:
		g_string_append(str, "char");
		break;
	case TYPESPEC_SHORT:
		g_string_append(str, "short");
		break;
	case TYPESPEC_INT:
		g_string_append(str, "int");
		break;
	case TYPESPEC_LONG:
		g_string_append(str, "long");
		break;
	case TYPESPEC_FLOAT:
		g_string_append(str, "float");
		break;
	case TYPESPEC_DOUBLE:
		g_string_append(str, "double");
		break;
	case TYPESPEC_SIGNED:
		g_string_append(str, "signed");
		break;
	case TYPESPEC_UNSIGNED:
		g_string_append(str, "unsigned");
		break;
	case TYPESPEC_BOOL:
		g_string_append(str, "bool");
		break;
	case TYPESPEC_COMPLEX:
		g_string_append(str, "_Complex");
		break;
	case TYPESPEC_IMAGINARY:
		g_string_append(str, "_Imaginary");
		break;
	case TYPESPEC_CONST:
		g_string_append(str, "const");
		break;
	case TYPESPEC_ID_TYPE:
		if (type_specifier->u.type_specifier.id_type)
			g_string_append(str, type_specifier->u.type_specifier.id_type);
		break;
	case TYPESPEC_STRUCT:
	{
		struct ctf_node *node = type_specifier->u.type_specifier.node;

		if (!node->u._struct.name) {
			fprintf(fd, "[error] %s: unexpected empty variant name\n", __func__);
			return -EINVAL;
		}
		g_string_append(str, "struct ");
		g_string_append(str, node->u._struct.name);
		break;
	}
	case TYPESPEC_VARIANT:
	{
		struct ctf_node *node = type_specifier->u.type_specifier.node;

		if (!node->u.variant.name) {
			fprintf(fd, "[error] %s: unexpected empty variant name\n", __func__);
			return -EINVAL;
		}
		g_string_append(str, "variant ");
		g_string_append(str, node->u.variant.name);
		break;
	}
	case TYPESPEC_ENUM:
	{
		struct ctf_node *node = type_specifier->u.type_specifier.node;

		if (!node->u._enum.enum_id) {
			fprintf(fd, "[error] %s: unexpected empty enum ID\n", __func__);
			return -EINVAL;
		}
		g_string_append(str, "enum ");
		g_string_append(str, node->u._enum.enum_id);
		break;
	}
	case TYPESPEC_FLOATING_POINT:
	case TYPESPEC_INTEGER:
	case TYPESPEC_STRING:
	default:
		fprintf(fd, "[error] %s: unknown specifier\n", __func__);
		return -EINVAL;
	}
	return 0;
}

static
int visit_type_specifier_list(FILE *fd, struct ctf_node *type_specifier_list, GString *str)
{
	struct ctf_node *iter;
	int alias_item_nr = 0;
	int ret;

	bt_list_for_each_entry(iter, &type_specifier_list->u.type_specifier_list.head, siblings) {
		if (alias_item_nr != 0)
			g_string_append(str, " ");
		alias_item_nr++;
		ret = visit_type_specifier(fd, iter, str);
		if (ret)
			return ret;
	}
	return 0;
}

static
GQuark create_typealias_identifier(FILE *fd, int depth,
	struct ctf_node *type_specifier_list,
	struct ctf_node *node_type_declarator)
{
	struct ctf_node *iter;
	GString *str;
	char *str_c;
	GQuark alias_q;
	int ret;

	str = g_string_new("");
	ret = visit_type_specifier_list(fd, type_specifier_list, str);
	if (ret) {
		g_string_free(str, TRUE);
		return 0;
	}
	bt_list_for_each_entry(iter, &node_type_declarator->u.type_declarator.pointers, siblings) {
		g_string_append(str, " *");
		if (iter->u.pointer.const_qualifier)
			g_string_append(str, " const");
	}
	str_c = g_string_free(str, FALSE);
	alias_q = g_quark_from_string(str_c);
	g_free(str_c);
	return alias_q;
}

static
struct bt_declaration *ctf_type_declarator_visit(FILE *fd, int depth,
	struct ctf_node *type_specifier_list,
	GQuark *field_name,
	struct ctf_node *node_type_declarator,
	struct declaration_scope *declaration_scope,
	struct bt_declaration *nested_declaration,
	struct ctf_trace *trace)
{
	/*
	 * Visit type declarator by first taking care of sequence/array
	 * (recursively). Then, when we get to the identifier, take care
	 * of pointers.
	 */

	if (node_type_declarator) {
		if (node_type_declarator->u.type_declarator.type == TYPEDEC_UNKNOWN) {
			return NULL;
		}

		/* TODO: gcc bitfields not supported yet. */
		if (node_type_declarator->u.type_declarator.bitfield_len != NULL) {
			fprintf(fd, "[error] %s: gcc bitfields are not supported yet.\n", __func__);
			return NULL;
		}
	}

	if (!nested_declaration) {
		if (node_type_declarator && !bt_list_empty(&node_type_declarator->u.type_declarator.pointers)) {
			GQuark alias_q;

			/*
			 * If we have a pointer declarator, it _has_ to be present in
			 * the typealiases (else fail).
			 */
			alias_q = create_typealias_identifier(fd, depth,
				type_specifier_list, node_type_declarator);
			nested_declaration = bt_lookup_declaration(alias_q, declaration_scope);
			if (!nested_declaration) {
				fprintf(fd, "[error] %s: cannot find typealias \"%s\".\n", __func__, g_quark_to_string(alias_q));
				return NULL;
			}
			if (nested_declaration->id == CTF_TYPE_INTEGER) {
				struct declaration_integer *integer_declaration =
					container_of(nested_declaration, struct declaration_integer, p);
				/* For base to 16 for pointers (expected pretty-print) */
				if (!integer_declaration->base) {
					/*
					 * We need to do a copy of the
					 * integer declaration to modify it. There could be other references to
					 * it.
					 */
					integer_declaration = bt_integer_declaration_new(integer_declaration->len,
						integer_declaration->byte_order, integer_declaration->signedness,
						integer_declaration->p.alignment, 16, integer_declaration->encoding,
						integer_declaration->clock);
					nested_declaration = &integer_declaration->p;
				}
			}
		} else {
			nested_declaration = ctf_type_specifier_list_visit(fd, depth,
				type_specifier_list, declaration_scope, trace);
		}
	}

	if (!node_type_declarator)
		return nested_declaration;

	if (node_type_declarator->u.type_declarator.type == TYPEDEC_ID) {
		if (node_type_declarator->u.type_declarator.u.id)
			*field_name = g_quark_from_string(node_type_declarator->u.type_declarator.u.id);
		else
			*field_name = 0;
		return nested_declaration;
	} else {
		struct bt_declaration *declaration;
		struct ctf_node *first;

		/* TYPEDEC_NESTED */

		if (!nested_declaration) {
			fprintf(fd, "[error] %s: nested type is unknown.\n", __func__);
			return NULL;
		}

		/* create array/sequence, pass nested_declaration as child. */
		if (bt_list_empty(&node_type_declarator->u.type_declarator.u.nested.length)) {
			fprintf(fd, "[error] %s: expecting length field reference or value.\n", __func__);
			return NULL;
		}
		first = _bt_list_first_entry(&node_type_declarator->u.type_declarator.u.nested.length, 
				struct ctf_node, siblings);
		if (first->type != NODE_UNARY_EXPRESSION) {
			return NULL;
		}

		switch (first->u.unary_expression.type) {
		case UNARY_UNSIGNED_CONSTANT:
		{
			struct declaration_array *array_declaration;
			size_t len;

			len = first->u.unary_expression.u.unsigned_constant;
			array_declaration = bt_array_declaration_new(len, nested_declaration,
						declaration_scope);

			if (!array_declaration) {
				fprintf(fd, "[error] %s: cannot create array declaration.\n", __func__);
				return NULL;
			}
			bt_declaration_unref(nested_declaration);
			declaration = &array_declaration->p;
			break;
		}
		case UNARY_STRING:
		{
			/* Lookup unsigned integer definition, create sequence */
			char *length_name = concatenate_unary_strings(&node_type_declarator->u.type_declarator.u.nested.length);
			struct declaration_sequence *sequence_declaration;

			if (!length_name)
				return NULL;
			sequence_declaration = bt_sequence_declaration_new(length_name, nested_declaration, declaration_scope);
			if (!sequence_declaration) {
				fprintf(fd, "[error] %s: cannot create sequence declaration.\n", __func__);
				g_free(length_name);
				return NULL;
			}
			bt_declaration_unref(nested_declaration);
			declaration = &sequence_declaration->p;
			g_free(length_name);
			break;
		}
		default:
			return NULL;
		}

		/* Pass it as content of outer container */
		declaration = ctf_type_declarator_visit(fd, depth,
				type_specifier_list, field_name,
				node_type_declarator->u.type_declarator.u.nested.type_declarator,
				declaration_scope, declaration, trace);
		return declaration;
	}
}

static
int ctf_struct_type_declarators_visit(FILE *fd, int depth,
	struct declaration_struct *struct_declaration,
	struct ctf_node *type_specifier_list,
	struct bt_list_head *type_declarators,
	struct declaration_scope *declaration_scope,
	struct ctf_trace *trace)
{
	struct ctf_node *iter;
	GQuark field_name;

	bt_list_for_each_entry(iter, type_declarators, siblings) {
		struct bt_declaration *field_declaration;

		field_declaration = ctf_type_declarator_visit(fd, depth,
						type_specifier_list,
						&field_name, iter,
						struct_declaration->scope,
						NULL, trace);
		if (!field_declaration) {
			fprintf(fd, "[error] %s: unable to find struct field declaration type\n", __func__);
			return -EINVAL;
		}

		/* Check if field with same name already exists */
		if (bt_struct_declaration_lookup_field_index(struct_declaration, field_name) >= 0) {
			fprintf(fd, "[error] %s: duplicate field %s in struct\n", __func__, g_quark_to_string(field_name));
			return -EINVAL;
		}

		bt_struct_declaration_add_field(struct_declaration,
					     g_quark_to_string(field_name),
					     field_declaration);
		bt_declaration_unref(field_declaration);
	}
	return 0;
}

static
int ctf_variant_type_declarators_visit(FILE *fd, int depth,
	struct declaration_untagged_variant *untagged_variant_declaration,
	struct ctf_node *type_specifier_list,
	struct bt_list_head *type_declarators,
	struct declaration_scope *declaration_scope,
	struct ctf_trace *trace)
{
	struct ctf_node *iter;
	GQuark field_name;

	bt_list_for_each_entry(iter, type_declarators, siblings) {
		struct bt_declaration *field_declaration;

		field_declaration = ctf_type_declarator_visit(fd, depth,
						type_specifier_list,
						&field_name, iter,
						untagged_variant_declaration->scope,
						NULL, trace);
		if (!field_declaration) {
			fprintf(fd, "[error] %s: unable to find variant field declaration type\n", __func__);
			return -EINVAL;
		}

		if (bt_untagged_variant_declaration_get_field_from_tag(untagged_variant_declaration, field_name) != NULL) {
			fprintf(fd, "[error] %s: duplicate field %s in variant\n", __func__, g_quark_to_string(field_name));
			return -EINVAL;
		}

		bt_untagged_variant_declaration_add_field(untagged_variant_declaration,
					      g_quark_to_string(field_name),
					      field_declaration);
		bt_declaration_unref(field_declaration);
	}
	return 0;
}

static
int ctf_typedef_visit(FILE *fd, int depth, struct declaration_scope *scope,
		struct ctf_node *type_specifier_list,
		struct bt_list_head *type_declarators,
		struct ctf_trace *trace)
{
	struct ctf_node *iter;
	GQuark identifier;

	bt_list_for_each_entry(iter, type_declarators, siblings) {
		struct bt_declaration *type_declaration;
		int ret;

		type_declaration = ctf_type_declarator_visit(fd, depth,
					type_specifier_list,
					&identifier, iter,
					scope, NULL, trace);
		if (!type_declaration) {
			fprintf(fd, "[error] %s: problem creating type declaration\n", __func__);
			return -EINVAL;
		}
		/*
		 * Don't allow typedef and typealias of untagged
		 * variants.
		 */
		if (type_declaration->id == CTF_TYPE_UNTAGGED_VARIANT) {
			fprintf(fd, "[error] %s: typedef of untagged variant is not permitted.\n", __func__);
			bt_declaration_unref(type_declaration);
			return -EPERM;
		}
		ret = bt_register_declaration(identifier, type_declaration, scope);
		if (ret) {
			type_declaration->declaration_free(type_declaration);
			return ret;
		}
		bt_declaration_unref(type_declaration);
	}
	return 0;
}

static
int ctf_typealias_visit(FILE *fd, int depth, struct declaration_scope *scope,
		struct ctf_node *target, struct ctf_node *alias,
		struct ctf_trace *trace)
{
	struct bt_declaration *type_declaration;
	struct ctf_node *node;
	GQuark dummy_id;
	GQuark alias_q;
	int err;

	/* See ctf_visitor_type_declarator() in the semantic validator. */

	/*
	 * Create target type declaration.
	 */

	if (bt_list_empty(&target->u.typealias_target.type_declarators))
		node = NULL;
	else
		node = _bt_list_first_entry(&target->u.typealias_target.type_declarators,
				struct ctf_node, siblings);
	type_declaration = ctf_type_declarator_visit(fd, depth,
		target->u.typealias_target.type_specifier_list,
		&dummy_id, node,
		scope, NULL, trace);
	if (!type_declaration) {
		fprintf(fd, "[error] %s: problem creating type declaration\n", __func__);
		err = -EINVAL;
		goto error;
	}
	/*
	 * Don't allow typedef and typealias of untagged
	 * variants.
	 */
	if (type_declaration->id == CTF_TYPE_UNTAGGED_VARIANT) {
		fprintf(fd, "[error] %s: typedef of untagged variant is not permitted.\n", __func__);
		bt_declaration_unref(type_declaration);
		return -EPERM;
	}
	/*
	 * The semantic validator does not check whether the target is
	 * abstract or not (if it has an identifier). Check it here.
	 */
	if (dummy_id != 0) {
		fprintf(fd, "[error] %s: expecting empty identifier\n", __func__);
		err = -EINVAL;
		goto error;
	}
	/*
	 * Create alias identifier.
	 */

	node = _bt_list_first_entry(&alias->u.typealias_alias.type_declarators,
				struct ctf_node, siblings);
	alias_q = create_typealias_identifier(fd, depth,
			alias->u.typealias_alias.type_specifier_list, node);
	err = bt_register_declaration(alias_q, type_declaration, scope);
	if (err)
		goto error;
	bt_declaration_unref(type_declaration);
	return 0;

error:
	if (type_declaration) {
		type_declaration->declaration_free(type_declaration);
	}
	return err;
}

static
int ctf_struct_declaration_list_visit(FILE *fd, int depth,
	struct ctf_node *iter, struct declaration_struct *struct_declaration,
	struct ctf_trace *trace)
{
	int ret;

	switch (iter->type) {
	case NODE_TYPEDEF:
		/* For each declarator, declare type and add type to struct bt_declaration scope */
		ret = ctf_typedef_visit(fd, depth,
			struct_declaration->scope,
			iter->u._typedef.type_specifier_list,
			&iter->u._typedef.type_declarators, trace);
		if (ret)
			return ret;
		break;
	case NODE_TYPEALIAS:
		/* Declare type with declarator and add type to struct bt_declaration scope */
		ret = ctf_typealias_visit(fd, depth,
			struct_declaration->scope,
			iter->u.typealias.target,
			iter->u.typealias.alias, trace);
		if (ret)
			return ret;
		break;
	case NODE_STRUCT_OR_VARIANT_DECLARATION:
		/* Add field to structure declaration */
		ret = ctf_struct_type_declarators_visit(fd, depth,
				struct_declaration,
				iter->u.struct_or_variant_declaration.type_specifier_list,
				&iter->u.struct_or_variant_declaration.type_declarators,
				struct_declaration->scope, trace);
		if (ret)
			return ret;
		break;
	default:
		fprintf(fd, "[error] %s: unexpected node type %d\n", __func__, (int) iter->type);
		return -EINVAL;
	}
	return 0;
}

static
int ctf_variant_declaration_list_visit(FILE *fd, int depth,
	struct ctf_node *iter,
	struct declaration_untagged_variant *untagged_variant_declaration,
	struct ctf_trace *trace)
{
	int ret;

	switch (iter->type) {
	case NODE_TYPEDEF:
		/* For each declarator, declare type and add type to variant declaration scope */
		ret = ctf_typedef_visit(fd, depth,
			untagged_variant_declaration->scope,
			iter->u._typedef.type_specifier_list,
			&iter->u._typedef.type_declarators, trace);
		if (ret)
			return ret;
		break;
	case NODE_TYPEALIAS:
		/* Declare type with declarator and add type to variant declaration scope */
		ret = ctf_typealias_visit(fd, depth,
			untagged_variant_declaration->scope,
			iter->u.typealias.target,
			iter->u.typealias.alias, trace);
		if (ret)
			return ret;
		break;
	case NODE_STRUCT_OR_VARIANT_DECLARATION:
		/* Add field to structure declaration */
		ret = ctf_variant_type_declarators_visit(fd, depth,
				untagged_variant_declaration,
				iter->u.struct_or_variant_declaration.type_specifier_list,
				&iter->u.struct_or_variant_declaration.type_declarators,
				untagged_variant_declaration->scope, trace);
		if (ret)
			return ret;
		break;
	default:
		fprintf(fd, "[error] %s: unexpected node type %d\n", __func__, (int) iter->type);
		return -EINVAL;
	}
	return 0;
}

static
struct bt_declaration *ctf_declaration_struct_visit(FILE *fd,
	int depth, const char *name, struct bt_list_head *declaration_list,
	int has_body, struct bt_list_head *min_align,
	struct declaration_scope *declaration_scope,
	struct ctf_trace *trace)
{
	struct declaration_struct *struct_declaration;
	struct ctf_node *iter;

	/*
	 * For named struct (without body), lookup in
	 * declaration scope. Don't take reference on struct
	 * declaration: ref is only taken upon definition.
	 */
	if (!has_body) {
		if (!name)
			return NULL;
		struct_declaration =
			bt_lookup_struct_declaration(g_quark_from_string(name),
						  declaration_scope);
		bt_declaration_ref(&struct_declaration->p);
		return &struct_declaration->p;
	} else {
		uint64_t min_align_value = 0;

		/* For unnamed struct, create type */
		/* For named struct (with body), create type and add to declaration scope */
		if (name) {
			if (bt_lookup_struct_declaration(g_quark_from_string(name),
						      declaration_scope)) {
				fprintf(fd, "[error] %s: struct %s already declared in scope\n", __func__, name);
				return NULL;
			}
		}
		if (!bt_list_empty(min_align)) {
			int ret;

			ret = get_unary_unsigned(min_align, &min_align_value);
			if (ret) {
				fprintf(fd, "[error] %s: unexpected unary expression for structure \"align\" attribute\n", __func__);
				goto error;
			}
		}
		struct_declaration = bt_struct_declaration_new(declaration_scope,
							    min_align_value);
		bt_list_for_each_entry(iter, declaration_list, siblings) {
			int ret;

			ret = ctf_struct_declaration_list_visit(fd, depth + 1, iter,
				struct_declaration, trace);
			if (ret)
				goto error_free_declaration;
		}
		if (name) {
			int ret;

			ret = bt_register_struct_declaration(g_quark_from_string(name),
					struct_declaration,
					declaration_scope);
			if (ret)
				return NULL;
		}
		return &struct_declaration->p;
	}
error_free_declaration:
	struct_declaration->p.declaration_free(&struct_declaration->p);
error:
	return NULL;
}

static
struct bt_declaration *ctf_declaration_variant_visit(FILE *fd,
	int depth, const char *name, const char *choice,
	struct bt_list_head *declaration_list,
	int has_body, struct declaration_scope *declaration_scope,
	struct ctf_trace *trace)
{
	struct declaration_untagged_variant *untagged_variant_declaration;
	struct declaration_variant *variant_declaration;
	struct ctf_node *iter;

	/*
	 * For named variant (without body), lookup in
	 * declaration scope. Don't take reference on variant
	 * declaration: ref is only taken upon definition.
	 */
	if (!has_body) {
		if (!name)
			return NULL;
		untagged_variant_declaration =
			bt_lookup_variant_declaration(g_quark_from_string(name),
						   declaration_scope);
		bt_declaration_ref(&untagged_variant_declaration->p);
	} else {
		/* For unnamed variant, create type */
		/* For named variant (with body), create type and add to declaration scope */
		if (name) {
			if (bt_lookup_variant_declaration(g_quark_from_string(name),
						       declaration_scope)) {
				fprintf(fd, "[error] %s: variant %s already declared in scope\n", __func__, name);
				return NULL;
			}
		}
		untagged_variant_declaration = bt_untagged_bt_variant_declaration_new(declaration_scope);
		bt_list_for_each_entry(iter, declaration_list, siblings) {
			int ret;

			ret = ctf_variant_declaration_list_visit(fd, depth + 1, iter,
				untagged_variant_declaration, trace);
			if (ret)
				goto error;
		}
		if (name) {
			int ret;

			ret = bt_register_variant_declaration(g_quark_from_string(name),
					untagged_variant_declaration,
					declaration_scope);
			if (ret)
				return NULL;
		}
	}
	/*
	 * if tagged, create tagged variant and return. else return
	 * untagged variant.
	 */
	if (!choice) {
		return &untagged_variant_declaration->p;
	} else {
		variant_declaration = bt_variant_declaration_new(untagged_variant_declaration, choice);
		if (!variant_declaration)
			goto error;
		bt_declaration_unref(&untagged_variant_declaration->p);
		return &variant_declaration->p;
	}
error:
	untagged_variant_declaration->p.declaration_free(&untagged_variant_declaration->p);
	return NULL;
}

static
int ctf_enumerator_list_visit(FILE *fd, int depth,
		struct ctf_node *enumerator,
		struct declaration_enum *enum_declaration,
		struct last_enum_value *last)
{
	GQuark q;
	struct ctf_node *iter;

	q = g_quark_from_string(enumerator->u.enumerator.id);
	if (enum_declaration->integer_declaration->signedness) {
		int64_t start = 0, end = 0;
		int nr_vals = 0;

		bt_list_for_each_entry(iter, &enumerator->u.enumerator.values, siblings) {
			int64_t *target;

			if (iter->type != NODE_UNARY_EXPRESSION)
				return -EINVAL;
			if (nr_vals == 0)
				target = &start;
			else
				target = &end;

			switch (iter->u.unary_expression.type) {
			case UNARY_SIGNED_CONSTANT:
				*target = iter->u.unary_expression.u.signed_constant;
				break;
			case UNARY_UNSIGNED_CONSTANT:
				*target = iter->u.unary_expression.u.unsigned_constant;
				break;
			default:
				fprintf(fd, "[error] %s: invalid enumerator\n", __func__);
				return -EINVAL;
			}
			if (nr_vals > 1) {
				fprintf(fd, "[error] %s: invalid enumerator\n", __func__);
				return -EINVAL;
			}
			nr_vals++;
		}
		if (nr_vals == 0)
			start = last->u.s;
		if (nr_vals <= 1)
			end = start;
		last->u.s = end + 1;
		bt_enum_signed_insert(enum_declaration, start, end, q);
	} else {
		uint64_t start = 0, end = 0;
		int nr_vals = 0;

		bt_list_for_each_entry(iter, &enumerator->u.enumerator.values, siblings) {
			uint64_t *target;

			if (iter->type != NODE_UNARY_EXPRESSION)
				return -EINVAL;
			if (nr_vals == 0)
				target = &start;
			else
				target = &end;

			switch (iter->u.unary_expression.type) {
			case UNARY_UNSIGNED_CONSTANT:
				*target = iter->u.unary_expression.u.unsigned_constant;
				break;
			case UNARY_SIGNED_CONSTANT:
				/*
				 * We don't accept signed constants for enums with unsigned
				 * container type.
				 */
				fprintf(fd, "[error] %s: invalid enumerator (signed constant encountered, but enum container type is unsigned)\n", __func__);
				return -EINVAL;
			default:
				fprintf(fd, "[error] %s: invalid enumerator\n", __func__);
				return -EINVAL;
			}
			if (nr_vals > 1) {
				fprintf(fd, "[error] %s: invalid enumerator\n", __func__);
				return -EINVAL;
			}
			nr_vals++;
		}
		if (nr_vals == 0)
			start = last->u.u;
		if (nr_vals <= 1)
			end = start;
		last->u.u = end + 1;
		bt_enum_unsigned_insert(enum_declaration, start, end, q);
	}
	return 0;
}

static
struct bt_declaration *ctf_declaration_enum_visit(FILE *fd, int depth,
			const char *name,
			struct ctf_node *container_type,
			struct bt_list_head *enumerator_list,
			int has_body,
			struct declaration_scope *declaration_scope,
			struct ctf_trace *trace)
{
	struct bt_declaration *declaration;
	struct declaration_enum *enum_declaration;
	struct declaration_integer *integer_declaration;
	struct last_enum_value last_value;
	struct ctf_node *iter;
	GQuark dummy_id;

	/*
	 * For named enum (without body), lookup in
	 * declaration scope. Don't take reference on enum
	 * declaration: ref is only taken upon definition.
	 */
	if (!has_body) {
		if (!name)
			return NULL;
		enum_declaration =
			bt_lookup_enum_declaration(g_quark_from_string(name),
						declaration_scope);
		bt_declaration_ref(&enum_declaration->p);
		return &enum_declaration->p;
	} else {
		/* For unnamed enum, create type */
		/* For named enum (with body), create type and add to declaration scope */
		if (name) {
			if (bt_lookup_enum_declaration(g_quark_from_string(name),
						    declaration_scope)) {
				fprintf(fd, "[error] %s: enum %s already declared in scope\n", __func__, name);
				return NULL;
			}
		}
		if (!container_type) {
			declaration = bt_lookup_declaration(g_quark_from_static_string("int"),
							 declaration_scope);
			if (!declaration) {
				fprintf(fd, "[error] %s: \"int\" type declaration missing for enumeration\n", __func__);
				return NULL;
			}
		} else {
			declaration = ctf_type_declarator_visit(fd, depth,
						container_type,
						&dummy_id, NULL,
						declaration_scope,
						NULL, trace);
		}
		if (!declaration) {
			fprintf(fd, "[error] %s: unable to create container type for enumeration\n", __func__);
			return NULL;
		}
		if (declaration->id != CTF_TYPE_INTEGER) {
			fprintf(fd, "[error] %s: container type for enumeration is not integer\n", __func__);
			return NULL;
		}
		integer_declaration = container_of(declaration, struct declaration_integer, p);
		enum_declaration = bt_enum_declaration_new(integer_declaration);
		bt_declaration_unref(&integer_declaration->p);	/* leave ref to enum */
		if (enum_declaration->integer_declaration->signedness) {
			last_value.u.s = 0;
		} else {
			last_value.u.u = 0;
		}
		bt_list_for_each_entry(iter, enumerator_list, siblings) {
			int ret;

			ret = ctf_enumerator_list_visit(fd, depth + 1, iter, enum_declaration,
					&last_value);
			if (ret)
				goto error;
		}
		if (name) {
			int ret;

			ret = bt_register_enum_declaration(g_quark_from_string(name),
					enum_declaration,
					declaration_scope);
			if (ret)
				return NULL;
			bt_declaration_unref(&enum_declaration->p);
		}
		return &enum_declaration->p;
	}
error:
	enum_declaration->p.declaration_free(&enum_declaration->p);
	return NULL;
}

static
struct bt_declaration *ctf_declaration_type_specifier_visit(FILE *fd, int depth,
		struct ctf_node *type_specifier_list,
		struct declaration_scope *declaration_scope)
{
	GString *str;
	struct bt_declaration *declaration;
	char *str_c;
	int ret;
	GQuark id_q;

	str = g_string_new("");
	ret = visit_type_specifier_list(fd, type_specifier_list, str);
	if (ret) {
		(void) g_string_free(str, TRUE);
		return NULL;
	}
	str_c = g_string_free(str, FALSE);
	id_q = g_quark_from_string(str_c);
	g_free(str_c);
	declaration = bt_lookup_declaration(id_q, declaration_scope);
	if (!declaration)
		return NULL;
	bt_declaration_ref(declaration);
	return declaration;
}

/*
 * Returns 0/1 boolean, or < 0 on error.
 */
static
int get_boolean(FILE *fd, int depth, struct ctf_node *unary_expression)
{
	if (unary_expression->type != NODE_UNARY_EXPRESSION) {
		fprintf(fd, "[error] %s: expecting unary expression\n",
			__func__);
		return -EINVAL;
	}
	switch (unary_expression->u.unary_expression.type) {
	case UNARY_UNSIGNED_CONSTANT:
		if (unary_expression->u.unary_expression.u.unsigned_constant == 0)
			return 0;
		else
			return 1;
	case UNARY_SIGNED_CONSTANT:
		if (unary_expression->u.unary_expression.u.signed_constant == 0)
			return 0;
		else
			return 1;
	case UNARY_STRING:
		if (!strcmp(unary_expression->u.unary_expression.u.string, "true"))
			return 1;
		else if (!strcmp(unary_expression->u.unary_expression.u.string, "TRUE"))
			return 1;
		else if (!strcmp(unary_expression->u.unary_expression.u.string, "false"))
			return 0;
		else if (!strcmp(unary_expression->u.unary_expression.u.string, "FALSE"))
			return 0;
		else {
			fprintf(fd, "[error] %s: unexpected string \"%s\"\n",
				__func__, unary_expression->u.unary_expression.u.string);
			return -EINVAL;
		}
		break;
	default:
		fprintf(fd, "[error] %s: unexpected unary expression type\n",
			__func__);
		return -EINVAL;
	}

}

static
int get_trace_byte_order(FILE *fd, int depth, struct ctf_node *unary_expression)
{
	int byte_order;

	if (unary_expression->u.unary_expression.type != UNARY_STRING) {
		fprintf(fd, "[error] %s: byte_order: expecting string\n",
			__func__);
		return -EINVAL;
	}
	if (!strcmp(unary_expression->u.unary_expression.u.string, "be"))
		byte_order = BIG_ENDIAN;
	else if (!strcmp(unary_expression->u.unary_expression.u.string, "le"))
		byte_order = LITTLE_ENDIAN;
	else {
		fprintf(fd, "[error] %s: unexpected string \"%s\". Should be \"be\" or \"le\".\n",
			__func__, unary_expression->u.unary_expression.u.string);
		return -EINVAL;
	}
	return byte_order;
}

static
int get_byte_order(FILE *fd, int depth, struct ctf_node *unary_expression,
		struct ctf_trace *trace)
{
	int byte_order;

	if (unary_expression->u.unary_expression.type != UNARY_STRING) {
		fprintf(fd, "[error] %s: byte_order: expecting string\n",
			__func__);
		return -EINVAL;
	}
	if (!strcmp(unary_expression->u.unary_expression.u.string, "native"))
		byte_order = trace->byte_order;
	else if (!strcmp(unary_expression->u.unary_expression.u.string, "network"))
		byte_order = BIG_ENDIAN;
	else if (!strcmp(unary_expression->u.unary_expression.u.string, "be"))
		byte_order = BIG_ENDIAN;
	else if (!strcmp(unary_expression->u.unary_expression.u.string, "le"))
		byte_order = LITTLE_ENDIAN;
	else {
		fprintf(fd, "[error] %s: unexpected string \"%s\". Should be \"native\", \"network\", \"be\" or \"le\".\n",
			__func__, unary_expression->u.unary_expression.u.string);
		return -EINVAL;
	}
	return byte_order;
}

static
struct bt_declaration *ctf_declaration_integer_visit(FILE *fd, int depth,
		struct bt_list_head *expressions,
		struct ctf_trace *trace)
{
	struct ctf_node *expression;
	uint64_t alignment = 1, size = 0;
	int byte_order = trace->byte_order;
	int signedness = 0;
	int has_alignment = 0, has_size = 0;
	int base = 0;
	enum ctf_string_encoding encoding = CTF_STRING_NONE;
	struct ctf_clock *clock = NULL;
	struct declaration_integer *integer_declaration;

	bt_list_for_each_entry(expression, expressions, siblings) {
		struct ctf_node *left, *right;

		left = _bt_list_first_entry(&expression->u.ctf_expression.left, struct ctf_node, siblings);
		right = _bt_list_first_entry(&expression->u.ctf_expression.right, struct ctf_node, siblings);
		if (left->u.unary_expression.type != UNARY_STRING)
			return NULL;
		if (!strcmp(left->u.unary_expression.u.string, "signed")) {
			signedness = get_boolean(fd, depth, right);
			if (signedness < 0)
				return NULL;
		} else if (!strcmp(left->u.unary_expression.u.string, "byte_order")) {
			byte_order = get_byte_order(fd, depth, right, trace);
			if (byte_order < 0)
				return NULL;
		} else if (!strcmp(left->u.unary_expression.u.string, "size")) {
			if (right->u.unary_expression.type != UNARY_UNSIGNED_CONSTANT) {
				fprintf(fd, "[error] %s: size: expecting unsigned constant\n",
					__func__);
				return NULL;
			}
			size = right->u.unary_expression.u.unsigned_constant;
			if (!size) {
				fprintf(fd, "[error] %s: integer size: expecting non-zero constant\n",
					__func__);
				return NULL;
			}
			has_size = 1;
		} else if (!strcmp(left->u.unary_expression.u.string, "align")) {
			if (right->u.unary_expression.type != UNARY_UNSIGNED_CONSTANT) {
				fprintf(fd, "[error] %s: align: expecting unsigned constant\n",
					__func__);
				return NULL;
			}
			alignment = right->u.unary_expression.u.unsigned_constant;
			/* Make sure alignment is a power of two */
			if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
				fprintf(fd, "[error] %s: align: expecting power of two\n",
					__func__);
				return NULL;
			}
			has_alignment = 1;
		} else if (!strcmp(left->u.unary_expression.u.string, "base")) {
			switch (right->u.unary_expression.type) {
			case UNARY_UNSIGNED_CONSTANT:
				switch (right->u.unary_expression.u.unsigned_constant) {
				case 2: 
				case 8:
				case 10:
				case 16:
					base = right->u.unary_expression.u.unsigned_constant;
					break;
				default:
					fprintf(fd, "[error] %s: base not supported (%" PRIu64 ")\n",
						__func__, right->u.unary_expression.u.unsigned_constant);
				return NULL;
				}
				break;
			case UNARY_STRING:
			{
				char *s_right = concatenate_unary_strings(&expression->u.ctf_expression.right);
				if (!s_right) {
					fprintf(fd, "[error] %s: unexpected unary expression for integer base\n", __func__);
					g_free(s_right);
					return NULL;
				}
				if (!strcmp(s_right, "decimal") || !strcmp(s_right, "dec") || !strcmp(s_right, "d")
				    || !strcmp(s_right, "i") || !strcmp(s_right, "u")) {
					base = 10;
				} else if (!strcmp(s_right, "hexadecimal") || !strcmp(s_right, "hex")
				    || !strcmp(s_right, "x") || !strcmp(s_right, "X")
				    || !strcmp(s_right, "p")) {
					base = 16;
				} else if (!strcmp(s_right, "octal") || !strcmp(s_right, "oct")
				    || !strcmp(s_right, "o")) {
					base = 8;
				} else if (!strcmp(s_right, "binary") || !strcmp(s_right, "b")) {
					base = 2;
				} else {
					fprintf(fd, "[error] %s: unexpected expression for integer base (%s)\n", __func__, s_right);
					g_free(s_right);
					return NULL;
				}

				g_free(s_right);
				break;
			}
			default:
				fprintf(fd, "[error] %s: base: expecting unsigned constant or unary string\n",
					__func__);
				return NULL;
			}
		} else if (!strcmp(left->u.unary_expression.u.string, "encoding")) {
			char *s_right;

			if (right->u.unary_expression.type != UNARY_STRING) {
				fprintf(fd, "[error] %s: encoding: expecting unary string\n",
					__func__);
				return NULL;
			}
			s_right = concatenate_unary_strings(&expression->u.ctf_expression.right);
			if (!s_right) {
				fprintf(fd, "[error] %s: unexpected unary expression for integer base\n", __func__);
				g_free(s_right);
				return NULL;
			}
			if (!strcmp(s_right, "UTF8")
			    || !strcmp(s_right, "utf8")
			    || !strcmp(s_right, "utf-8")
			    || !strcmp(s_right, "UTF-8"))
				encoding = CTF_STRING_UTF8;
			else if (!strcmp(s_right, "ASCII")
			    || !strcmp(s_right, "ascii"))
				encoding = CTF_STRING_ASCII;
			else if (!strcmp(s_right, "none"))
				encoding = CTF_STRING_NONE;
			else {
				fprintf(fd, "[error] %s: unknown string encoding \"%s\"\n", __func__, s_right);
				g_free(s_right);
				return NULL;
			}
			g_free(s_right);
		} else if (!strcmp(left->u.unary_expression.u.string, "map")) {
			GQuark clock_name;

			if (right->u.unary_expression.type != UNARY_STRING) {
				fprintf(fd, "[error] %s: map: expecting identifier\n",
					__func__);
				return NULL;
			}
			/* currently only support clock.name.value */
			clock_name = get_map_clock_name_value(&expression->u.ctf_expression.right);
			if (!clock_name) {
				char *s_right;

				s_right = concatenate_unary_strings(&expression->u.ctf_expression.right);
				if (!s_right) {
					fprintf(fd, "[error] %s: unexpected unary expression for integer map\n", __func__);
					g_free(s_right);
					return NULL;
				}
				fprintf(fd, "[warning] %s: unknown map %s in integer declaration\n", __func__,
					s_right);
				g_free(s_right);
				continue;
			}
			clock = trace_clock_lookup(trace, clock_name);
			if (!clock) {
				fprintf(fd, "[error] %s: map: unable to find clock %s declaration\n",
					__func__, g_quark_to_string(clock_name));
				return NULL;
			}
		} else {
			fprintf(fd, "[warning] %s: unknown attribute name %s\n",
				__func__, left->u.unary_expression.u.string);
			/* Fall-through after warning */
		}
	}
	if (!has_size) {
		fprintf(fd, "[error] %s: missing size attribute\n", __func__);
		return NULL;
	}
	if (!has_alignment) {
		if (size % CHAR_BIT) {
			/* bit-packed alignment */
			alignment = 1;
		} else {
			/* byte-packed alignment */
			alignment = CHAR_BIT;
		}
	}
	integer_declaration = bt_integer_declaration_new(size,
				byte_order, signedness, alignment,
				base, encoding, clock);
	return &integer_declaration->p;
}

static
struct bt_declaration *ctf_declaration_floating_point_visit(FILE *fd, int depth,
		struct bt_list_head *expressions,
		struct ctf_trace *trace)
{
	struct ctf_node *expression;
	uint64_t alignment = 1, exp_dig = 0, mant_dig = 0;
	int byte_order = trace->byte_order, has_alignment = 0,
		has_exp_dig = 0, has_mant_dig = 0;
	struct declaration_float *float_declaration;

	bt_list_for_each_entry(expression, expressions, siblings) {
		struct ctf_node *left, *right;

		left = _bt_list_first_entry(&expression->u.ctf_expression.left, struct ctf_node, siblings);
		right = _bt_list_first_entry(&expression->u.ctf_expression.right, struct ctf_node, siblings);
		if (left->u.unary_expression.type != UNARY_STRING)
			return NULL;
		if (!strcmp(left->u.unary_expression.u.string, "byte_order")) {
			byte_order = get_byte_order(fd, depth, right, trace);
			if (byte_order < 0)
				return NULL;
		} else if (!strcmp(left->u.unary_expression.u.string, "exp_dig")) {
			if (right->u.unary_expression.type != UNARY_UNSIGNED_CONSTANT) {
				fprintf(fd, "[error] %s: exp_dig: expecting unsigned constant\n",
					__func__);
				return NULL;
			}
			exp_dig = right->u.unary_expression.u.unsigned_constant;
			has_exp_dig = 1;
		} else if (!strcmp(left->u.unary_expression.u.string, "mant_dig")) {
			if (right->u.unary_expression.type != UNARY_UNSIGNED_CONSTANT) {
				fprintf(fd, "[error] %s: mant_dig: expecting unsigned constant\n",
					__func__);
				return NULL;
			}
			mant_dig = right->u.unary_expression.u.unsigned_constant;
			has_mant_dig = 1;
		} else if (!strcmp(left->u.unary_expression.u.string, "align")) {
			if (right->u.unary_expression.type != UNARY_UNSIGNED_CONSTANT) {
				fprintf(fd, "[error] %s: align: expecting unsigned constant\n",
					__func__);
				return NULL;
			}
			alignment = right->u.unary_expression.u.unsigned_constant;
			/* Make sure alignment is a power of two */
			if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
				fprintf(fd, "[error] %s: align: expecting power of two\n",
					__func__);
				return NULL;
			}
			has_alignment = 1;
		} else {
			fprintf(fd, "[warning] %s: unknown attribute name %s\n",
				__func__, left->u.unary_expression.u.string);
			/* Fall-through after warning */
		}
	}
	if (!has_mant_dig) {
		fprintf(fd, "[error] %s: missing mant_dig attribute\n", __func__);
		return NULL;
	}
	if (!has_exp_dig) {
		fprintf(fd, "[error] %s: missing exp_dig attribute\n", __func__);
		return NULL;
	}
	if (!has_alignment) {
		if ((mant_dig + exp_dig) % CHAR_BIT) {
			/* bit-packed alignment */
			alignment = 1;
		} else {
			/* byte-packed alignment */
			alignment = CHAR_BIT;
		}
	}
	float_declaration = bt_float_declaration_new(mant_dig, exp_dig,
				byte_order, alignment);
	return &float_declaration->p;
}

static
struct bt_declaration *ctf_declaration_string_visit(FILE *fd, int depth,
		struct bt_list_head *expressions,
		struct ctf_trace *trace)
{
	struct ctf_node *expression;
	const char *encoding_c = NULL;
	enum ctf_string_encoding encoding = CTF_STRING_UTF8;
	struct declaration_string *string_declaration;

	bt_list_for_each_entry(expression, expressions, siblings) {
		struct ctf_node *left, *right;

		left = _bt_list_first_entry(&expression->u.ctf_expression.left, struct ctf_node, siblings);
		right = _bt_list_first_entry(&expression->u.ctf_expression.right, struct ctf_node, siblings);
		if (left->u.unary_expression.type != UNARY_STRING)
			return NULL;
		if (!strcmp(left->u.unary_expression.u.string, "encoding")) {
			if (right->u.unary_expression.type != UNARY_STRING) {
				fprintf(fd, "[error] %s: encoding: expecting string\n",
					__func__);
				return NULL;
			}
			encoding_c = right->u.unary_expression.u.string;
		} else {
			fprintf(fd, "[warning] %s: unknown attribute name %s\n",
				__func__, left->u.unary_expression.u.string);
			/* Fall-through after warning */
		}
	}
	if (encoding_c && !strcmp(encoding_c, "ASCII"))
		encoding = CTF_STRING_ASCII;
	string_declaration = bt_string_declaration_new(encoding);
	return &string_declaration->p;
}


static
struct bt_declaration *ctf_type_specifier_list_visit(FILE *fd,
		int depth, struct ctf_node *type_specifier_list,
		struct declaration_scope *declaration_scope,
		struct ctf_trace *trace)
{
	struct ctf_node *first;
	struct ctf_node *node;

	if (type_specifier_list->type != NODE_TYPE_SPECIFIER_LIST)
		return NULL;

	first = _bt_list_first_entry(&type_specifier_list->u.type_specifier_list.head, struct ctf_node, siblings);

	if (first->type != NODE_TYPE_SPECIFIER)
		return NULL;

	node = first->u.type_specifier.node;

	switch (first->u.type_specifier.type) {
	case TYPESPEC_FLOATING_POINT:
		return ctf_declaration_floating_point_visit(fd, depth,
			&node->u.floating_point.expressions, trace);
	case TYPESPEC_INTEGER:
		return ctf_declaration_integer_visit(fd, depth,
			&node->u.integer.expressions, trace);
	case TYPESPEC_STRING:
		return ctf_declaration_string_visit(fd, depth,
			&node->u.string.expressions, trace);
	case TYPESPEC_STRUCT:
		return ctf_declaration_struct_visit(fd, depth,
			node->u._struct.name,
			&node->u._struct.declaration_list,
			node->u._struct.has_body,
			&node->u._struct.min_align,
			declaration_scope,
			trace);
	case TYPESPEC_VARIANT:
		return ctf_declaration_variant_visit(fd, depth,
			node->u.variant.name,
			node->u.variant.choice,
			&node->u.variant.declaration_list,
			node->u.variant.has_body,
			declaration_scope,
			trace);
	case TYPESPEC_ENUM:
		return ctf_declaration_enum_visit(fd, depth,
			node->u._enum.enum_id,
			node->u._enum.container_type,
			&node->u._enum.enumerator_list,
			node->u._enum.has_body,
			declaration_scope,
			trace);

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
		return ctf_declaration_type_specifier_visit(fd, depth,
			type_specifier_list, declaration_scope);
	default:
		fprintf(fd, "[error] %s: unexpected node type %d\n", __func__, (int) first->u.type_specifier.type);
		return NULL;
	}
}

static
int ctf_event_declaration_visit(FILE *fd, int depth, struct ctf_node *node, struct ctf_event_declaration *event, struct ctf_trace *trace)
{
	int ret = 0;

	switch (node->type) {
	case NODE_TYPEDEF:
		ret = ctf_typedef_visit(fd, depth + 1,
					event->declaration_scope,
					node->u._typedef.type_specifier_list,
					&node->u._typedef.type_declarators,
					trace);
		if (ret)
			return ret;
		break;
	case NODE_TYPEALIAS:
		ret = ctf_typealias_visit(fd, depth + 1,
				event->declaration_scope,
				node->u.typealias.target, node->u.typealias.alias,
				trace);
		if (ret)
			return ret;
		break;
	case NODE_CTF_EXPRESSION:
	{
		char *left;

		left = concatenate_unary_strings(&node->u.ctf_expression.left);
		if (!left)
			return -EINVAL;
		if (!strcmp(left, "name")) {
			char *right;

			if (CTF_EVENT_FIELD_IS_SET(event, name)) {
				fprintf(fd, "[error] %s: name already declared in event declaration\n", __func__);
				ret = -EPERM;
				goto error;
			}
			right = concatenate_unary_strings(&node->u.ctf_expression.right);
			if (!right) {
				fprintf(fd, "[error] %s: unexpected unary expression for event name\n", __func__);
				ret = -EINVAL;
				goto error;
			}
			event->name = g_quark_from_string(right);
			g_free(right);
			CTF_EVENT_SET_FIELD(event, name);
		} else if (!strcmp(left, "id")) {
			if (CTF_EVENT_FIELD_IS_SET(event, id)) {
				fprintf(fd, "[error] %s: id already declared in event declaration\n", __func__);
				ret = -EPERM;
				goto error;
			}
			ret = get_unary_unsigned(&node->u.ctf_expression.right, &event->id);
			if (ret) {
				fprintf(fd, "[error] %s: unexpected unary expression for event id\n", __func__);
				ret = -EINVAL;
				goto error;
			}
			CTF_EVENT_SET_FIELD(event, id);
		} else if (!strcmp(left, "stream_id")) {
			if (CTF_EVENT_FIELD_IS_SET(event, stream_id)) {
				fprintf(fd, "[error] %s: stream_id already declared in event declaration\n", __func__);
				ret = -EPERM;
				goto error;
			}
			ret = get_unary_unsigned(&node->u.ctf_expression.right, &event->stream_id);
			if (ret) {
				fprintf(fd, "[error] %s: unexpected unary expression for event stream_id\n", __func__);
				ret = -EINVAL;
				goto error;
			}
			event->stream = trace_stream_lookup(trace, event->stream_id);
			if (!event->stream) {
				fprintf(fd, "[error] %s: stream id %" PRIu64 " cannot be found\n", __func__, event->stream_id);
				ret = -EINVAL;
				goto error;
			}
			CTF_EVENT_SET_FIELD(event, stream_id);
		} else if (!strcmp(left, "context")) {
			struct bt_declaration *declaration;

			if (event->context_decl) {
				fprintf(fd, "[error] %s: context already declared in event declaration\n", __func__);
				ret = -EINVAL;
				goto error;
			}
			declaration = ctf_type_specifier_list_visit(fd, depth,
					_bt_list_first_entry(&node->u.ctf_expression.right,
						struct ctf_node, siblings),
					event->declaration_scope, trace);
			if (!declaration) {
				ret = -EPERM;
				goto error;
			}
			if (declaration->id != CTF_TYPE_STRUCT) {
				ret = -EPERM;
				goto error;
			}
			event->context_decl = container_of(declaration, struct declaration_struct, p);
		} else if (!strcmp(left, "fields")) {
			struct bt_declaration *declaration;

			if (event->fields_decl) {
				fprintf(fd, "[error] %s: fields already declared in event declaration\n", __func__);
				ret = -EINVAL;
				goto error;
			}
			declaration = ctf_type_specifier_list_visit(fd, depth,
					_bt_list_first_entry(&node->u.ctf_expression.right,
						struct ctf_node, siblings),
					event->declaration_scope, trace);
			if (!declaration) {
				ret = -EPERM;
				goto error;
			}
			if (declaration->id != CTF_TYPE_STRUCT) {
				ret = -EPERM;
				goto error;
			}
			event->fields_decl = container_of(declaration, struct declaration_struct, p);
		} else if (!strcmp(left, "loglevel")) {
			int64_t loglevel = -1;

			if (CTF_EVENT_FIELD_IS_SET(event, loglevel)) {
				fprintf(fd, "[error] %s: loglevel already declared in event declaration\n", __func__);
				ret = -EPERM;
				goto error;
			}
			ret = get_unary_signed(&node->u.ctf_expression.right, &loglevel);
			if (ret) {
				fprintf(fd, "[error] %s: unexpected unary expression for event loglevel\n", __func__);
				ret = -EINVAL;
				goto error;
			}
			event->loglevel = (int) loglevel;
			CTF_EVENT_SET_FIELD(event, loglevel);
		} else if (!strcmp(left, "model.emf.uri")) {
			char *right;

			if (CTF_EVENT_FIELD_IS_SET(event, model_emf_uri)) {
				fprintf(fd, "[error] %s: model.emf.uri already declared in event declaration\n", __func__);
				ret = -EPERM;
				goto error;
			}
			right = concatenate_unary_strings(&node->u.ctf_expression.right);
			if (!right) {
				fprintf(fd, "[error] %s: unexpected unary expression for event model.emf.uri\n", __func__);
				ret = -EINVAL;
				goto error;
			}
			event->model_emf_uri = g_quark_from_string(right);
			g_free(right);
			CTF_EVENT_SET_FIELD(event, model_emf_uri);
		} else {
			fprintf(fd, "[warning] %s: attribute \"%s\" is unknown in event declaration.\n", __func__, left);
			/* Fall-through after warning */
		}
error:
		g_free(left);
		break;
	}
	default:
		return -EPERM;
	/* TODO: declaration specifier should be added. */
	}

	return ret;
}

static
int ctf_event_visit(FILE *fd, int depth, struct ctf_node *node,
		    struct declaration_scope *parent_declaration_scope, struct ctf_trace *trace)
{
	int ret = 0;
	struct ctf_node *iter;
	struct ctf_event_declaration *event;
	struct bt_ctf_event_decl *event_decl;

	if (node->visited)
		return 0;
	node->visited = 1;

	event_decl = g_new0(struct bt_ctf_event_decl, 1);
	event = &event_decl->parent;
	event->declaration_scope = bt_new_declaration_scope(parent_declaration_scope);
	event->loglevel = -1;
	bt_list_for_each_entry(iter, &node->u.event.declaration_list, siblings) {
		ret = ctf_event_declaration_visit(fd, depth + 1, iter, event, trace);
		if (ret)
			goto error;
	}
	if (!CTF_EVENT_FIELD_IS_SET(event, name)) {
		ret = -EPERM;
		fprintf(fd, "[error] %s: missing name field in event declaration\n", __func__);
		goto error;
	}
	if (!CTF_EVENT_FIELD_IS_SET(event, stream_id)) {
		/* Allow missing stream_id if there is only a single stream */
		switch (trace->streams->len) {
		case 0:	/* Create stream if there was none. */
			ret = ctf_stream_visit(fd, depth, NULL, trace->root_declaration_scope, trace);
			if (ret)
				goto error;
			/* Fall-through */
		case 1:
			event->stream_id = 0;
			event->stream = trace_stream_lookup(trace, event->stream_id);
			break;
		default:
			ret = -EPERM;
			fprintf(fd, "[error] %s: missing stream_id field in event declaration\n", __func__);
			goto error;
		}
	}
	/* Allow only one event without id per stream */
	if (!CTF_EVENT_FIELD_IS_SET(event, id)
	    && event->stream->events_by_id->len != 0) {
		ret = -EPERM;
		fprintf(fd, "[error] %s: missing id field in event declaration\n", __func__);
		goto error;
	}
	/* Disallow re-using the same event ID in the same stream */
	if (stream_event_lookup(event->stream, event->id)) {
		ret = -EPERM;
		fprintf(fd, "[error] %s: event ID %" PRIu64 " used more than once in stream %" PRIu64 "\n",
			__func__, event->id, event->stream_id);
		goto error;
	}
	if (event->stream->events_by_id->len <= event->id)
		g_ptr_array_set_size(event->stream->events_by_id, event->id + 1);
	g_ptr_array_index(event->stream->events_by_id, event->id) = event;
	g_hash_table_insert(event->stream->event_quark_to_id,
			    (gpointer) (unsigned long) event->name,
			    &event->id);
	g_ptr_array_add(trace->event_declarations, event_decl);
	return 0;

error:
	if (event->fields_decl)
		bt_declaration_unref(&event->fields_decl->p);
	if (event->context_decl)
		bt_declaration_unref(&event->context_decl->p);
	bt_free_declaration_scope(event->declaration_scope);
	g_free(event_decl);
	return ret;
}

 
static
int ctf_stream_declaration_visit(FILE *fd, int depth, struct ctf_node *node, struct ctf_stream_declaration *stream, struct ctf_trace *trace)
{
	int ret = 0;

	switch (node->type) {
	case NODE_TYPEDEF:
		ret = ctf_typedef_visit(fd, depth + 1,
					stream->declaration_scope,
					node->u._typedef.type_specifier_list,
					&node->u._typedef.type_declarators,
					trace);
		if (ret)
			return ret;
		break;
	case NODE_TYPEALIAS:
		ret = ctf_typealias_visit(fd, depth + 1,
				stream->declaration_scope,
				node->u.typealias.target, node->u.typealias.alias,
				trace);
		if (ret)
			return ret;
		break;
	case NODE_CTF_EXPRESSION:
	{
		char *left;

		left = concatenate_unary_strings(&node->u.ctf_expression.left);
		if (!left)
			return -EINVAL;
		if (!strcmp(left, "id")) {
			if (CTF_STREAM_FIELD_IS_SET(stream, stream_id)) {
				fprintf(fd, "[error] %s: id already declared in stream declaration\n", __func__);
				ret = -EPERM;
				goto error;
			}
			ret = get_unary_unsigned(&node->u.ctf_expression.right, &stream->stream_id);
			if (ret) {
				fprintf(fd, "[error] %s: unexpected unary expression for stream id\n", __func__);
				ret = -EINVAL;
				goto error;
			}
			CTF_STREAM_SET_FIELD(stream, stream_id);
		} else if (!strcmp(left, "event.header")) {
			struct bt_declaration *declaration;

			if (stream->event_header_decl) {
				fprintf(fd, "[error] %s: event.header already declared in stream declaration\n", __func__);
				ret = -EINVAL;
				goto error;
			}
			declaration = ctf_type_specifier_list_visit(fd, depth,
					_bt_list_first_entry(&node->u.ctf_expression.right,
						struct ctf_node, siblings),
					stream->declaration_scope, trace);
			if (!declaration) {
				ret = -EPERM;
				goto error;
			}
			if (declaration->id != CTF_TYPE_STRUCT) {
				ret = -EPERM;
				goto error;
			}
			stream->event_header_decl = container_of(declaration, struct declaration_struct, p);
		} else if (!strcmp(left, "event.context")) {
			struct bt_declaration *declaration;

			if (stream->event_context_decl) {
				fprintf(fd, "[error] %s: event.context already declared in stream declaration\n", __func__);
				ret = -EINVAL;
				goto error;
			}
			declaration = ctf_type_specifier_list_visit(fd, depth,
					_bt_list_first_entry(&node->u.ctf_expression.right,
						struct ctf_node, siblings),
					stream->declaration_scope, trace);
			if (!declaration) {
				ret = -EPERM;
				goto error;
			}
			if (declaration->id != CTF_TYPE_STRUCT) {
				ret = -EPERM;
				goto error;
			}
			stream->event_context_decl = container_of(declaration, struct declaration_struct, p);
		} else if (!strcmp(left, "packet.context")) {
			struct bt_declaration *declaration;

			if (stream->packet_context_decl) {
				fprintf(fd, "[error] %s: packet.context already declared in stream declaration\n", __func__);
				ret = -EINVAL;
				goto error;
			}
			declaration = ctf_type_specifier_list_visit(fd, depth,
					_bt_list_first_entry(&node->u.ctf_expression.right,
						struct ctf_node, siblings),
					stream->declaration_scope, trace);
			if (!declaration) {
				ret = -EPERM;
				goto error;
			}
			if (declaration->id != CTF_TYPE_STRUCT) {
				ret = -EPERM;
				goto error;
			}
			stream->packet_context_decl = container_of(declaration, struct declaration_struct, p);
		} else {
			fprintf(fd, "[warning] %s: attribute \"%s\" is unknown in stream declaration.\n", __func__, left);
			/* Fall-through after warning */
		}

error:
		g_free(left);
		break;
	}
	default:
		return -EPERM;
	/* TODO: declaration specifier should be added. */
	}

	return ret;
}

static
int ctf_stream_visit(FILE *fd, int depth, struct ctf_node *node,
		     struct declaration_scope *parent_declaration_scope, struct ctf_trace *trace)
{
	int ret = 0;
	struct ctf_node *iter;
	struct ctf_stream_declaration *stream;

	if (node) {
		if (node->visited)
			return 0;
		node->visited = 1;
	}

	stream = g_new0(struct ctf_stream_declaration, 1);
	stream->declaration_scope = bt_new_declaration_scope(parent_declaration_scope);
	stream->events_by_id = g_ptr_array_new();
	stream->event_quark_to_id = g_hash_table_new(g_direct_hash, g_direct_equal);
	stream->streams = g_ptr_array_new();
	if (node) {
		bt_list_for_each_entry(iter, &node->u.stream.declaration_list, siblings) {
			ret = ctf_stream_declaration_visit(fd, depth + 1, iter, stream, trace);
			if (ret)
				goto error;
		}
	}
	if (CTF_STREAM_FIELD_IS_SET(stream, stream_id)) {
		/* check that packet header has stream_id field. */
		if (!trace->packet_header_decl
		    || bt_struct_declaration_lookup_field_index(trace->packet_header_decl, g_quark_from_static_string("stream_id")) < 0) {
			ret = -EPERM;
			fprintf(fd, "[error] %s: missing stream_id field in packet header declaration, but stream_id attribute is declared for stream.\n", __func__);
			goto error;
		}
	} else {
		/* Allow only one id-less stream */
		if (trace->streams->len != 0) {
			ret = -EPERM;
			fprintf(fd, "[error] %s: missing id field in stream declaration\n", __func__);
			goto error;
		}
		stream->stream_id = 0;
	}
	if (trace->streams->len <= stream->stream_id)
		g_ptr_array_set_size(trace->streams, stream->stream_id + 1);
	g_ptr_array_index(trace->streams, stream->stream_id) = stream;
	stream->trace = trace;

	return 0;

error:
	if (stream->event_header_decl)
		bt_declaration_unref(&stream->event_header_decl->p);
	if (stream->event_context_decl)
		bt_declaration_unref(&stream->event_context_decl->p);
	if (stream->packet_context_decl)
		bt_declaration_unref(&stream->packet_context_decl->p);
	g_ptr_array_free(stream->streams, TRUE);
	g_ptr_array_free(stream->events_by_id, TRUE);
	g_hash_table_destroy(stream->event_quark_to_id);
	bt_free_declaration_scope(stream->declaration_scope);
	g_free(stream);
	return ret;
}

static
int ctf_trace_declaration_visit(FILE *fd, int depth, struct ctf_node *node, struct ctf_trace *trace)
{
	int ret = 0;

	switch (node->type) {
	case NODE_TYPEDEF:
		ret = ctf_typedef_visit(fd, depth + 1,
					trace->declaration_scope,
					node->u._typedef.type_specifier_list,
					&node->u._typedef.type_declarators,
					trace);
		if (ret)
			return ret;
		break;
	case NODE_TYPEALIAS:
		ret = ctf_typealias_visit(fd, depth + 1,
				trace->declaration_scope,
				node->u.typealias.target, node->u.typealias.alias,
				trace);
		if (ret)
			return ret;
		break;
	case NODE_CTF_EXPRESSION:
	{
		char *left;

		left = concatenate_unary_strings(&node->u.ctf_expression.left);
		if (!left)
			return -EINVAL;
		if (!strcmp(left, "major")) {
			if (CTF_TRACE_FIELD_IS_SET(trace, major)) {
				fprintf(fd, "[error] %s: major already declared in trace declaration\n", __func__);
				ret = -EPERM;
				goto error;
			}
			ret = get_unary_unsigned(&node->u.ctf_expression.right, &trace->major);
			if (ret) {
				fprintf(fd, "[error] %s: unexpected unary expression for trace major number\n", __func__);
				ret = -EINVAL;
				goto error;
			}
			CTF_TRACE_SET_FIELD(trace, major);
		} else if (!strcmp(left, "minor")) {
			if (CTF_TRACE_FIELD_IS_SET(trace, minor)) {
				fprintf(fd, "[error] %s: minor already declared in trace declaration\n", __func__);
				ret = -EPERM;
				goto error;
			}
			ret = get_unary_unsigned(&node->u.ctf_expression.right, &trace->minor);
			if (ret) {
				fprintf(fd, "[error] %s: unexpected unary expression for trace minor number\n", __func__);
				ret = -EINVAL;
				goto error;
			}
			CTF_TRACE_SET_FIELD(trace, minor);
		} else if (!strcmp(left, "uuid")) {
			unsigned char uuid[BABELTRACE_UUID_LEN];

			ret = get_unary_uuid(&node->u.ctf_expression.right, uuid);
			if (ret) {
				fprintf(fd, "[error] %s: unexpected unary expression for trace uuid\n", __func__);
				ret = -EINVAL;
				goto error;
			}
			if (CTF_TRACE_FIELD_IS_SET(trace, uuid)
				&& bt_uuid_compare(uuid, trace->uuid)) {
				fprintf(fd, "[error] %s: uuid mismatch\n", __func__);
				ret = -EPERM;
				goto error;
			} else {
				memcpy(trace->uuid, uuid, sizeof(uuid));
			}
			CTF_TRACE_SET_FIELD(trace, uuid);
		} else if (!strcmp(left, "byte_order")) {
			struct ctf_node *right;
			int byte_order;

			right = _bt_list_first_entry(&node->u.ctf_expression.right, struct ctf_node, siblings);
			byte_order = get_trace_byte_order(fd, depth, right);
			if (byte_order < 0) {
				ret = -EINVAL;
				goto error;
			}

			if (CTF_TRACE_FIELD_IS_SET(trace, byte_order)
				&& byte_order != trace->byte_order) {
				fprintf(fd, "[error] %s: endianness mismatch\n", __func__);
				ret = -EPERM;
				goto error;
			} else {
				if (byte_order != trace->byte_order) {
					trace->byte_order = byte_order;
					/*
					 * We need to restart
					 * construction of the
					 * intermediate representation.
					 */
					trace->field_mask = 0;
					CTF_TRACE_SET_FIELD(trace, byte_order);
					ret = -EINTR;
					goto error;
				}
			}
			CTF_TRACE_SET_FIELD(trace, byte_order);
		} else if (!strcmp(left, "packet.header")) {
			struct bt_declaration *declaration;

			if (trace->packet_header_decl) {
				fprintf(fd, "[error] %s: packet.header already declared in trace declaration\n", __func__);
				ret = -EINVAL;
				goto error;
			}
			declaration = ctf_type_specifier_list_visit(fd, depth,
					_bt_list_first_entry(&node->u.ctf_expression.right,
						struct ctf_node, siblings),
					trace->declaration_scope, trace);
			if (!declaration) {
				ret = -EPERM;
				goto error;
			}
			if (declaration->id != CTF_TYPE_STRUCT) {
				ret = -EPERM;
				goto error;
			}
			trace->packet_header_decl = container_of(declaration, struct declaration_struct, p);
		} else {
			fprintf(fd, "[warning] %s: attribute \"%s\" is unknown in trace declaration.\n", __func__, left);
		}

error:
		g_free(left);
		break;
	}
	default:
		return -EPERM;
	/* TODO: declaration specifier should be added. */
	}

	return ret;
}

static
int ctf_trace_visit(FILE *fd, int depth, struct ctf_node *node, struct ctf_trace *trace)
{
	int ret = 0;
	struct ctf_node *iter;

	if (!trace->restart_root_decl && node->visited)
		return 0;
	node->visited = 1;

	if (trace->declaration_scope)
		return -EEXIST;

	trace->declaration_scope = bt_new_declaration_scope(trace->root_declaration_scope);
	trace->streams = g_ptr_array_new();
	trace->event_declarations = g_ptr_array_new();
	bt_list_for_each_entry(iter, &node->u.trace.declaration_list, siblings) {
		ret = ctf_trace_declaration_visit(fd, depth + 1, iter, trace);
		if (ret)
			goto error;
	}
	if (!CTF_TRACE_FIELD_IS_SET(trace, major)) {
		ret = -EPERM;
		fprintf(fd, "[error] %s: missing major field in trace declaration\n", __func__);
		goto error;
	}
	if (!CTF_TRACE_FIELD_IS_SET(trace, minor)) {
		ret = -EPERM;
		fprintf(fd, "[error] %s: missing minor field in trace declaration\n", __func__);
		goto error;
	}
	if (!CTF_TRACE_FIELD_IS_SET(trace, byte_order)) {
		ret = -EPERM;
		fprintf(fd, "[error] %s: missing byte_order field in trace declaration\n", __func__);
		goto error;
	}

	if (!CTF_TRACE_FIELD_IS_SET(trace, byte_order)) {
		/* check that the packet header contains a "magic" field */
		if (!trace->packet_header_decl
		    || bt_struct_declaration_lookup_field_index(trace->packet_header_decl, g_quark_from_static_string("magic")) < 0) {
			ret = -EPERM;
			fprintf(fd, "[error] %s: missing both byte_order and packet header magic number in trace declaration\n", __func__);
			goto error;
		}
	}
	return 0;

error:
	if (trace->packet_header_decl) {
		bt_declaration_unref(&trace->packet_header_decl->p);
		trace->packet_header_decl = NULL;
	}
	g_ptr_array_free(trace->streams, TRUE);
	g_ptr_array_free(trace->event_declarations, TRUE);
	bt_free_declaration_scope(trace->declaration_scope);
	trace->declaration_scope = NULL;
	return ret;
}

static
int ctf_clock_declaration_visit(FILE *fd, int depth, struct ctf_node *node,
		struct ctf_clock *clock, struct ctf_trace *trace)
{
	int ret = 0;

	switch (node->type) {
	case NODE_CTF_EXPRESSION:
	{
		char *left;

		left = concatenate_unary_strings(&node->u.ctf_expression.left);
		if (!left)
			return -EINVAL;
		if (!strcmp(left, "name")) {
			char *right;

			if (CTF_CLOCK_FIELD_IS_SET(clock, name)) {
				fprintf(fd, "[error] %s: name already declared in clock declaration\n", __func__);
				ret = -EPERM;
				goto error;
			}
			right = concatenate_unary_strings(&node->u.ctf_expression.right);
			if (!right) {
				fprintf(fd, "[error] %s: unexpected unary expression for clock name\n", __func__);
				ret = -EINVAL;
				goto error;
			}
			clock->name = g_quark_from_string(right);
			g_free(right);
			CTF_CLOCK_SET_FIELD(clock, name);
		} else if (!strcmp(left, "uuid")) {
			char *right;

			if (clock->uuid) {
				fprintf(fd, "[error] %s: uuid already declared in clock declaration\n", __func__);
				ret = -EPERM;
				goto error;
			}
			right = concatenate_unary_strings(&node->u.ctf_expression.right);
			if (!right) {
				fprintf(fd, "[error] %s: unexpected unary expression for clock uuid\n", __func__);
				ret = -EINVAL;
				goto error;
			}
			clock->uuid = g_quark_from_string(right);
			g_free(right);
		} else if (!strcmp(left, "description")) {
			char *right;

			if (clock->description) {
				fprintf(fd, "[warning] %s: duplicated clock description\n", __func__);
				goto error;	/* ret is 0, so not an actual error, just warn. */
			}
			right = concatenate_unary_strings(&node->u.ctf_expression.right);
			if (!right) {
				fprintf(fd, "[warning] %s: unexpected unary expression for clock description\n", __func__);
				goto error;	/* ret is 0, so not an actual error, just warn. */
			}
			clock->description = right;
		} else if (!strcmp(left, "freq")) {
			if (CTF_CLOCK_FIELD_IS_SET(clock, freq)) {
				fprintf(fd, "[error] %s: freq already declared in clock declaration\n", __func__);
				ret = -EPERM;
				goto error;
			}
			ret = get_unary_unsigned(&node->u.ctf_expression.right, &clock->freq);
			if (ret) {
				fprintf(fd, "[error] %s: unexpected unary expression for clock freq\n", __func__);
				ret = -EINVAL;
				goto error;
			}
			CTF_CLOCK_SET_FIELD(clock, freq);
		} else if (!strcmp(left, "precision")) {
			if (clock->precision) {
				fprintf(fd, "[error] %s: precision already declared in clock declaration\n", __func__);
				ret = -EPERM;
				goto error;
			}
			ret = get_unary_unsigned(&node->u.ctf_expression.right, &clock->precision);
			if (ret) {
				fprintf(fd, "[error] %s: unexpected unary expression for clock precision\n", __func__);
				ret = -EINVAL;
				goto error;
			}
		} else if (!strcmp(left, "offset_s")) {
			if (clock->offset_s) {
				fprintf(fd, "[error] %s: offset_s already declared in clock declaration\n", __func__);
				ret = -EPERM;
				goto error;
			}
			ret = get_unary_signed(&node->u.ctf_expression.right, &clock->offset_s);
			if (ret) {
				fprintf(fd, "[error] %s: unexpected unary expression for clock offset_s\n", __func__);
				ret = -EINVAL;
				goto error;
			}
		} else if (!strcmp(left, "offset")) {
			if (clock->offset) {
				fprintf(fd, "[error] %s: offset already declared in clock declaration\n", __func__);
				ret = -EPERM;
				goto error;
			}
			ret = get_unary_signed(&node->u.ctf_expression.right, &clock->offset);
			if (ret) {
				fprintf(fd, "[error] %s: unexpected unary expression for clock offset\n", __func__);
				ret = -EINVAL;
				goto error;
			}
		} else if (!strcmp(left, "absolute")) {
			struct ctf_node *right;

			right = _bt_list_first_entry(&node->u.ctf_expression.right, struct ctf_node, siblings);
			ret = get_boolean(fd, depth, right);
			if (ret < 0) {
				fprintf(fd, "[error] %s: unexpected \"absolute\" right member\n", __func__);
				ret = -EINVAL;
				goto error;
			}
			clock->absolute = ret;
			ret = 0;
		} else {
			fprintf(fd, "[warning] %s: attribute \"%s\" is unknown in clock declaration.\n", __func__, left);
		}

error:
		g_free(left);
		break;
	}
	default:
		return -EPERM;
	/* TODO: declaration specifier should be added. */
	}

	return ret;
}

static
int ctf_clock_visit(FILE *fd, int depth, struct ctf_node *node, struct ctf_trace *trace)
{
	int ret = 0;
	struct ctf_node *iter;
	struct ctf_clock *clock;

	if (node->visited)
		return 0;
	node->visited = 1;

	clock = g_new0(struct ctf_clock, 1);
	/* Default clock frequency is set to 1000000000 */
	clock->freq = 1000000000ULL;
	bt_list_for_each_entry(iter, &node->u.clock.declaration_list, siblings) {
		ret = ctf_clock_declaration_visit(fd, depth + 1, iter, clock, trace);
		if (ret)
			goto error;
	}
	if (opt_clock_force_correlate) {
		/*
		 * User requested to forcibly correlate the clock
		 * sources, even if we have no correlation
		 * information.
		 */
		if (!clock->absolute) {
			fprintf(fd, "[warning] Forcibly correlating trace clock sources (--clock-force-correlate).\n");
		}
		clock->absolute = 1;
	}
	if (!CTF_CLOCK_FIELD_IS_SET(clock, name)) {
		ret = -EPERM;
		fprintf(fd, "[error] %s: missing name field in clock declaration\n", __func__);
		goto error;
	}
	if (g_hash_table_size(trace->parent.clocks) > 0) {
		fprintf(fd, "[error] Only CTF traces with a single clock description are supported by this babeltrace version.\n");
		ret = -EINVAL;
		goto error;
	}
	trace->parent.single_clock = clock;
	g_hash_table_insert(trace->parent.clocks, (gpointer) (unsigned long) clock->name, clock);
	return 0;

error:
	g_free(clock->description);
	g_free(clock);
	return ret;
}

static
void ctf_clock_default(FILE *fd, int depth, struct ctf_trace *trace)
{
	struct ctf_clock *clock;

	clock = g_new0(struct ctf_clock, 1);
	clock->name = g_quark_from_string("monotonic");
	clock->uuid = 0;
	clock->description = g_strdup("Default clock");
	/* Default clock frequency is set to 1000000000 */
	clock->freq = 1000000000ULL;
	if (opt_clock_force_correlate) {
		/*
		 * User requested to forcibly correlate the clock
		 * sources, even if we have no correlatation
		 * information.
		 */
		if (!clock->absolute) {
			fprintf(fd, "[warning] Forcibly correlating trace clock sources (--clock-force-correlate).\n");
		}
		clock->absolute = 1;
	} else {
		clock->absolute = 0;	/* Not an absolute reference across traces */
	}

	trace->parent.single_clock = clock;
	g_hash_table_insert(trace->parent.clocks, (gpointer) (unsigned long) clock->name, clock);
}

static
void clock_free(gpointer data)
{
	struct ctf_clock *clock = data;

	g_free(clock->description);
	g_free(clock);
}

static
int ctf_callsite_declaration_visit(FILE *fd, int depth, struct ctf_node *node,
		struct ctf_callsite *callsite, struct ctf_trace *trace)
{
	int ret = 0;

	switch (node->type) {
	case NODE_CTF_EXPRESSION:
	{
		char *left;

		left = concatenate_unary_strings(&node->u.ctf_expression.left);
		if (!left)
			return -EINVAL;
		if (!strcmp(left, "name")) {
			char *right;

			if (CTF_CALLSITE_FIELD_IS_SET(callsite, name)) {
				fprintf(fd, "[error] %s: name already declared in callsite declaration\n", __func__);
				ret = -EPERM;
				goto error;
			}
			right = concatenate_unary_strings(&node->u.ctf_expression.right);
			if (!right) {
				fprintf(fd, "[error] %s: unexpected unary expression for callsite name\n", __func__);
				ret = -EINVAL;
				goto error;
			}
			callsite->name = g_quark_from_string(right);
			g_free(right);
			CTF_CALLSITE_SET_FIELD(callsite, name);
		} else if (!strcmp(left, "func")) {
			char *right;

			if (CTF_CALLSITE_FIELD_IS_SET(callsite, func)) {
				fprintf(fd, "[error] %s: func already declared in callsite declaration\n", __func__);
				ret = -EPERM;
				goto error;
			}
			right = concatenate_unary_strings(&node->u.ctf_expression.right);
			if (!right) {
				fprintf(fd, "[error] %s: unexpected unary expression for callsite func\n", __func__);
				ret = -EINVAL;
				goto error;
			}
			callsite->func = right;
			CTF_CALLSITE_SET_FIELD(callsite, func);
		} else if (!strcmp(left, "file")) {
			char *right;

			if (CTF_CALLSITE_FIELD_IS_SET(callsite, file)) {
				fprintf(fd, "[error] %s: file already declared in callsite declaration\n", __func__);
				ret = -EPERM;
				goto error;
			}
			right = concatenate_unary_strings(&node->u.ctf_expression.right);
			if (!right) {
				fprintf(fd, "[error] %s: unexpected unary expression for callsite file\n", __func__);
				ret = -EINVAL;
				goto error;
			}
			callsite->file = right;
			CTF_CALLSITE_SET_FIELD(callsite, file);
		} else if (!strcmp(left, "line")) {
			if (CTF_CALLSITE_FIELD_IS_SET(callsite, line)) {
				fprintf(fd, "[error] %s: line already declared in callsite declaration\n", __func__);
				ret = -EPERM;
				goto error;
			}
			ret = get_unary_unsigned(&node->u.ctf_expression.right, &callsite->line);
			if (ret) {
				fprintf(fd, "[error] %s: unexpected unary expression for callsite line\n", __func__);
				ret = -EINVAL;
				goto error;
			}
			CTF_CALLSITE_SET_FIELD(callsite, line);
		} else if (!strcmp(left, "ip")) {
			if (CTF_CALLSITE_FIELD_IS_SET(callsite, ip)) {
				fprintf(fd, "[error] %s: ip already declared in callsite declaration\n", __func__);
				ret = -EPERM;
				goto error;
			}
			ret = get_unary_unsigned(&node->u.ctf_expression.right, &callsite->ip);
			if (ret) {
				fprintf(fd, "[error] %s: unexpected unary expression for callsite ip\n", __func__);
				ret = -EINVAL;
				goto error;
			}
			CTF_CALLSITE_SET_FIELD(callsite, ip);
		} else {
			fprintf(fd, "[warning] %s: attribute \"%s\" is unknown in callsite declaration.\n", __func__, left);
		}

error:
		g_free(left);
		break;
	}
	default:
		return -EPERM;
	/* TODO: declaration specifier should be added. */
	}

	return ret;
}

static
int ctf_callsite_visit(FILE *fd, int depth, struct ctf_node *node, struct ctf_trace *trace)
{
	int ret = 0;
	struct ctf_node *iter;
	struct ctf_callsite *callsite;
	struct ctf_callsite_dups *cs_dups;

	if (node->visited)
		return 0;
	node->visited = 1;

	callsite = g_new0(struct ctf_callsite, 1);
	bt_list_for_each_entry(iter, &node->u.callsite.declaration_list, siblings) {
		ret = ctf_callsite_declaration_visit(fd, depth + 1, iter, callsite, trace);
		if (ret)
			goto error;
	}
	if (!CTF_CALLSITE_FIELD_IS_SET(callsite, name)) {
		ret = -EPERM;
		fprintf(fd, "[error] %s: missing name field in callsite declaration\n", __func__);
		goto error;
	}
	if (!CTF_CALLSITE_FIELD_IS_SET(callsite, func)) {
		ret = -EPERM;
		fprintf(fd, "[error] %s: missing func field in callsite declaration\n", __func__);
		goto error;
	}
	if (!CTF_CALLSITE_FIELD_IS_SET(callsite, file)) {
		ret = -EPERM;
		fprintf(fd, "[error] %s: missing file field in callsite declaration\n", __func__);
		goto error;
	}
	if (!CTF_CALLSITE_FIELD_IS_SET(callsite, line)) {
		ret = -EPERM;
		fprintf(fd, "[error] %s: missing line field in callsite declaration\n", __func__);
		goto error;
	}

	cs_dups = g_hash_table_lookup(trace->callsites,
		(gpointer) (unsigned long) callsite->name);
	if (!cs_dups) {
		cs_dups = g_new0(struct ctf_callsite_dups, 1);
		BT_INIT_LIST_HEAD(&cs_dups->head);
		g_hash_table_insert(trace->callsites,
			(gpointer) (unsigned long) callsite->name, cs_dups);
	}
	bt_list_add_tail(&callsite->node, &cs_dups->head);
	return 0;

error:
	g_free(callsite->func);
	g_free(callsite->file);
	g_free(callsite);
	return ret;
}

static
void callsite_free(gpointer data)
{
	struct ctf_callsite_dups *cs_dups = data;
	struct ctf_callsite *callsite, *cs_n;

	bt_list_for_each_entry_safe(callsite, cs_n, &cs_dups->head, node) {
		g_free(callsite->func);
		g_free(callsite->file);
		g_free(callsite);
	}
	g_free(cs_dups);
}

static
int ctf_env_declaration_visit(FILE *fd, int depth, struct ctf_node *node,
		struct ctf_trace *trace)
{
	int ret = 0;
	struct ctf_tracer_env *env = &trace->env;

	switch (node->type) {
	case NODE_CTF_EXPRESSION:
	{
		char *left;

		left = concatenate_unary_strings(&node->u.ctf_expression.left);
		if (!left)
			return -EINVAL;
		if (!strcmp(left, "vpid")) {
			uint64_t v;

			if (env->vpid != -1) {
				fprintf(fd, "[error] %s: vpid already declared in env declaration\n", __func__);
				goto error;	/* ret is 0, so not an actual error, just warn. */
			}
			ret = get_unary_unsigned(&node->u.ctf_expression.right, &v);
			if (ret) {
				fprintf(fd, "[error] %s: unexpected unary expression for env vpid\n", __func__);
				goto error;	/* ret is 0, so not an actual error, just warn. */
			}
			env->vpid = (int) v;
			printf_verbose("env.vpid = %d\n", env->vpid);
		} else if (!strcmp(left, "procname")) {
			char *right;

			if (env->procname[0]) {
				fprintf(fd, "[warning] %s: duplicated env procname\n", __func__);
				goto error;	/* ret is 0, so not an actual error, just warn. */
			}
			right = concatenate_unary_strings(&node->u.ctf_expression.right);
			if (!right) {
				fprintf(fd, "[warning] %s: unexpected unary expression for env procname\n", __func__);
				goto error;	/* ret is 0, so not an actual error, just warn. */
			}
			strncpy(env->procname, right, TRACER_ENV_LEN);
			env->procname[TRACER_ENV_LEN - 1] = '\0';
			printf_verbose("env.procname = \"%s\"\n", env->procname);
			g_free(right);
		} else if (!strcmp(left, "hostname")) {
			char *right;

			if (env->hostname[0]) {
				fprintf(fd, "[warning] %s: duplicated env hostname\n", __func__);
				goto error;	/* ret is 0, so not an actual error, just warn. */
			}
			right = concatenate_unary_strings(&node->u.ctf_expression.right);
			if (!right) {
				fprintf(fd, "[warning] %s: unexpected unary expression for env hostname\n", __func__);
				goto error;	/* ret is 0, so not an actual error, just warn. */
			}
			strncpy(env->hostname, right, TRACER_ENV_LEN);
			env->hostname[TRACER_ENV_LEN - 1] = '\0';
			printf_verbose("env.hostname = \"%s\"\n", env->hostname);
			g_free(right);
		} else if (!strcmp(left, "domain")) {
			char *right;

			if (env->domain[0]) {
				fprintf(fd, "[warning] %s: duplicated env domain\n", __func__);
				goto error;	/* ret is 0, so not an actual error, just warn. */
			}
			right = concatenate_unary_strings(&node->u.ctf_expression.right);
			if (!right) {
				fprintf(fd, "[warning] %s: unexpected unary expression for env domain\n", __func__);
				goto error;	/* ret is 0, so not an actual error, just warn. */
			}
			strncpy(env->domain, right, TRACER_ENV_LEN);
			env->domain[TRACER_ENV_LEN - 1] = '\0';
			printf_verbose("env.domain = \"%s\"\n", env->domain);
			g_free(right);
		} else if (!strcmp(left, "tracer_name")) {
			char *right;

			if (env->tracer_name[0]) {
				fprintf(fd, "[warning] %s: duplicated env tracer_name\n", __func__);
				goto error;	/* ret is 0, so not an actual error, just warn. */
			}
			right = concatenate_unary_strings(&node->u.ctf_expression.right);
			if (!right) {
				fprintf(fd, "[warning] %s: unexpected unary expression for env tracer_name\n", __func__);
				goto error;	/* ret is 0, so not an actual error, just warn. */
			}
			strncpy(env->tracer_name, right, TRACER_ENV_LEN);
			env->tracer_name[TRACER_ENV_LEN - 1] = '\0';
			printf_verbose("env.tracer_name = \"%s\"\n", env->tracer_name);
			g_free(right);
		} else if (!strcmp(left, "sysname")) {
			char *right;

			if (env->sysname[0]) {
				fprintf(fd, "[warning] %s: duplicated env sysname\n", __func__);
				goto error;	/* ret is 0, so not an actual error, just warn. */
			}
			right = concatenate_unary_strings(&node->u.ctf_expression.right);
			if (!right) {
				fprintf(fd, "[warning] %s: unexpected unary expression for env sysname\n", __func__);
				goto error;	/* ret is 0, so not an actual error, just warn. */
			}
			strncpy(env->sysname, right, TRACER_ENV_LEN);
			env->sysname[TRACER_ENV_LEN - 1] = '\0';
			printf_verbose("env.sysname = \"%s\"\n", env->sysname);
			g_free(right);
		} else if (!strcmp(left, "kernel_release")) {
			char *right;

			if (env->release[0]) {
				fprintf(fd, "[warning] %s: duplicated env release\n", __func__);
				goto error;	/* ret is 0, so not an actual error, just warn. */
			}
			right = concatenate_unary_strings(&node->u.ctf_expression.right);
			if (!right) {
				fprintf(fd, "[warning] %s: unexpected unary expression for env release\n", __func__);
				goto error;	/* ret is 0, so not an actual error, just warn. */
			}
			strncpy(env->release, right, TRACER_ENV_LEN);
			env->release[TRACER_ENV_LEN - 1] = '\0';
			printf_verbose("env.release = \"%s\"\n", env->release);
			g_free(right);
		} else if (!strcmp(left, "kernel_version")) {
			char *right;

			if (env->version[0]) {
				fprintf(fd, "[warning] %s: duplicated env version\n", __func__);
				goto error;	/* ret is 0, so not an actual error, just warn. */
			}
			right = concatenate_unary_strings(&node->u.ctf_expression.right);
			if (!right) {
				fprintf(fd, "[warning] %s: unexpected unary expression for env version\n", __func__);
				goto error;	/* ret is 0, so not an actual error, just warn. */
			}
			strncpy(env->version, right, TRACER_ENV_LEN);
			env->version[TRACER_ENV_LEN - 1] = '\0';
			printf_verbose("env.version = \"%s\"\n", env->version);
			g_free(right);
		} else {
			if (is_unary_string(&node->u.ctf_expression.right)) {
				char *right;

				right = concatenate_unary_strings(&node->u.ctf_expression.right);
				if (!right) {
					fprintf(fd, "[warning] %s: unexpected unary expression for env\n", __func__);
					ret = -EINVAL;
					goto error;
				}
				printf_verbose("env.%s = \"%s\"\n", left, right);
				g_free(right);
			} else if (is_unary_unsigned(&node->u.ctf_expression.right)) {
				uint64_t v;
				int ret;

				ret = get_unary_unsigned(&node->u.ctf_expression.right, &v);
				if (ret)
					goto error;
				printf_verbose("env.%s = %" PRIu64 "\n", left, v);
			} else if (is_unary_signed(&node->u.ctf_expression.right)) {
				int64_t v;
				int ret;

				ret = get_unary_signed(&node->u.ctf_expression.right, &v);
				if (ret)
					goto error;
				printf_verbose("env.%s = %" PRId64 "\n", left, v);
			} else {
				printf_verbose("%s: attribute \"%s\" has unknown type.\n", __func__, left);
			}
		}

error:
		g_free(left);
		break;
	}
	default:
		return -EPERM;
	}

	return ret;
}

static
int ctf_env_visit(FILE *fd, int depth, struct ctf_node *node, struct ctf_trace *trace)
{
	int ret = 0;
	struct ctf_node *iter;

	if (node->visited)
		return 0;
	node->visited = 1;

	trace->env.vpid = -1;
	trace->env.procname[0] = '\0';
	trace->env.hostname[0] = '\0';
	trace->env.domain[0] = '\0';
	trace->env.sysname[0] = '\0';
	trace->env.release[0] = '\0';
	trace->env.version[0] = '\0';
	bt_list_for_each_entry(iter, &node->u.env.declaration_list, siblings) {
		ret = ctf_env_declaration_visit(fd, depth + 1, iter, trace);
		if (ret)
			goto error;
	}
error:
	return 0;
}

static
int ctf_root_declaration_visit(FILE *fd, int depth, struct ctf_node *node, struct ctf_trace *trace)
{
	int ret = 0;

	if (!trace->restart_root_decl && node->visited)
		return 0;
	node->visited = 1;

	switch (node->type) {
	case NODE_TYPEDEF:
		ret = ctf_typedef_visit(fd, depth + 1,
					trace->root_declaration_scope,
					node->u._typedef.type_specifier_list,
					&node->u._typedef.type_declarators,
					trace);
		if (ret)
			return ret;
		break;
	case NODE_TYPEALIAS:
		ret = ctf_typealias_visit(fd, depth + 1,
				trace->root_declaration_scope,
				node->u.typealias.target, node->u.typealias.alias,
				trace);
		if (ret)
			return ret;
		break;
	case NODE_TYPE_SPECIFIER_LIST:
	{
		struct bt_declaration *declaration;

		/*
		 * Just add the type specifier to the root scope
		 * declaration scope. Release local reference.
		 */
		declaration = ctf_type_specifier_list_visit(fd, depth + 1,
			node, trace->root_declaration_scope, trace);
		if (!declaration)
			return -ENOMEM;
		bt_declaration_unref(declaration);
		break;
	}
	default:
		return -EPERM;
	}

	return 0;
}

int ctf_visitor_construct_metadata(FILE *fd, int depth, struct ctf_node *node,
		struct ctf_trace *trace, int byte_order)
{
	int ret = 0;
	struct ctf_node *iter;

	printf_verbose("CTF visitor: metadata construction...\n");
	trace->byte_order = byte_order;
	trace->parent.clocks = g_hash_table_new_full(g_direct_hash,
				g_direct_equal, NULL, clock_free);
	trace->callsites = g_hash_table_new_full(g_direct_hash, g_direct_equal,
				NULL, callsite_free);

retry:
	trace->root_declaration_scope = bt_new_declaration_scope(NULL);

	switch (node->type) {
	case NODE_ROOT:
		/*
		 * declarations need to query clock hash table,
		 * so clock need to be treated first.
		 */
		if (bt_list_empty(&node->u.root.clock)) {
			ctf_clock_default(fd, depth + 1, trace);
		} else {
			bt_list_for_each_entry(iter, &node->u.root.clock, siblings) {
				ret = ctf_clock_visit(fd, depth + 1, iter,
						      trace);
				if (ret) {
					fprintf(fd, "[error] %s: clock declaration error\n", __func__);
					goto error;
				}
			}
		}
		bt_list_for_each_entry(iter, &node->u.root.declaration_list,
					siblings) {
			ret = ctf_root_declaration_visit(fd, depth + 1, iter, trace);
			if (ret) {
				fprintf(fd, "[error] %s: root declaration error\n", __func__);
				goto error;
			}
		}
		bt_list_for_each_entry(iter, &node->u.root.trace, siblings) {
			ret = ctf_trace_visit(fd, depth + 1, iter, trace);
			if (ret == -EINTR) {
				trace->restart_root_decl = 1;
				bt_free_declaration_scope(trace->root_declaration_scope);
				/*
				 * Need to restart creation of type
				 * definitions, aliases and
				 * trace header declarations.
				 */
				goto retry;
			}
			if (ret) {
				fprintf(fd, "[error] %s: trace declaration error\n", __func__);
				goto error;
			}
		}
		trace->restart_root_decl = 0;
		bt_list_for_each_entry(iter, &node->u.root.callsite, siblings) {
			ret = ctf_callsite_visit(fd, depth + 1, iter,
					      trace);
			if (ret) {
				fprintf(fd, "[error] %s: callsite declaration error\n", __func__);
				goto error;
			}
		}
		if (!trace->streams) {
			fprintf(fd, "[error] %s: missing trace declaration\n", __func__);
			ret = -EINVAL;
			goto error;
		}
		bt_list_for_each_entry(iter, &node->u.root.env, siblings) {
			ret = ctf_env_visit(fd, depth + 1, iter, trace);
			if (ret) {
				fprintf(fd, "[error] %s: env declaration error\n", __func__);
				goto error;
			}
		}
		bt_list_for_each_entry(iter, &node->u.root.stream, siblings) {
			ret = ctf_stream_visit(fd, depth + 1, iter,
		    			       trace->root_declaration_scope, trace);
			if (ret) {
				fprintf(fd, "[error] %s: stream declaration error\n", __func__);
				goto error;
			}
		}
		bt_list_for_each_entry(iter, &node->u.root.event, siblings) {
			ret = ctf_event_visit(fd, depth + 1, iter,
		    			      trace->root_declaration_scope, trace);
			if (ret) {
				fprintf(fd, "[error] %s: event declaration error\n", __func__);
				goto error;
			}
		}
		break;
	case NODE_UNKNOWN:
	default:
		fprintf(fd, "[error] %s: unknown node type %d\n", __func__,
			(int) node->type);
		ret = -EINVAL;
		goto error;
	}
	printf_verbose("done.\n");
	return ret;

error:
	bt_free_declaration_scope(trace->root_declaration_scope);
	g_hash_table_destroy(trace->callsites);
	g_hash_table_destroy(trace->parent.clocks);
	return ret;
}

int ctf_destroy_metadata(struct ctf_trace *trace)
{
	int i;
	struct ctf_file_stream *metadata_stream;

	if (trace->streams) {
		for (i = 0; i < trace->streams->len; i++) {
			struct ctf_stream_declaration *stream;
			int j;

			stream = g_ptr_array_index(trace->streams, i);
			if (!stream)
				continue;
			for (j = 0; j < stream->streams->len; j++) {
				struct ctf_stream_definition *stream_def;
				int k;

				stream_def = g_ptr_array_index(stream->streams, j);
				if (!stream_def)
					continue;
				for (k = 0; k < stream_def->events_by_id->len; k++) {
					struct ctf_event_definition *event;

					event = g_ptr_array_index(stream_def->events_by_id, k);
					if (!event)
						continue;
					if (&event->event_fields->p)
						bt_definition_unref(&event->event_fields->p);
					if (&event->event_context->p)
						bt_definition_unref(&event->event_context->p);
					g_free(event);
				}
				if (&stream_def->trace_packet_header->p)
					bt_definition_unref(&stream_def->trace_packet_header->p);
				if (&stream_def->stream_event_header->p)
					bt_definition_unref(&stream_def->stream_event_header->p);
				if (&stream_def->stream_packet_context->p)
					bt_definition_unref(&stream_def->stream_packet_context->p);
				if (&stream_def->stream_event_context->p)
					bt_definition_unref(&stream_def->stream_event_context->p);
				g_ptr_array_free(stream_def->events_by_id, TRUE);
				g_free(stream_def);
			}
			if (stream->event_header_decl)
				bt_declaration_unref(&stream->event_header_decl->p);
			if (stream->event_context_decl)
				bt_declaration_unref(&stream->event_context_decl->p);
			if (stream->packet_context_decl)
				bt_declaration_unref(&stream->packet_context_decl->p);
			g_ptr_array_free(stream->streams, TRUE);
			g_ptr_array_free(stream->events_by_id, TRUE);
			g_hash_table_destroy(stream->event_quark_to_id);
			bt_free_declaration_scope(stream->declaration_scope);
			g_free(stream);
		}
		g_ptr_array_free(trace->streams, TRUE);
	}

	if (trace->event_declarations) {
		for (i = 0; i < trace->event_declarations->len; i++) {
			struct bt_ctf_event_decl *event_decl;
			struct ctf_event_declaration *event;

			event_decl = g_ptr_array_index(trace->event_declarations, i);
			if (event_decl->context_decl)
				g_ptr_array_free(event_decl->context_decl, TRUE);
			if (event_decl->fields_decl)
				g_ptr_array_free(event_decl->fields_decl, TRUE);
			if (event_decl->packet_header_decl)
				g_ptr_array_free(event_decl->packet_header_decl, TRUE);
			if (event_decl->event_context_decl)
				g_ptr_array_free(event_decl->event_context_decl, TRUE);
			if (event_decl->event_header_decl)
				g_ptr_array_free(event_decl->event_header_decl, TRUE);
			if (event_decl->packet_context_decl)
				g_ptr_array_free(event_decl->packet_context_decl, TRUE);

			event = &event_decl->parent;
			if (event->fields_decl)
				bt_declaration_unref(&event->fields_decl->p);
			if (event->context_decl)
				bt_declaration_unref(&event->context_decl->p);
			bt_free_declaration_scope(event->declaration_scope);

			g_free(event);
		}
		g_ptr_array_free(trace->event_declarations, TRUE);
	}
	if (trace->packet_header_decl)
		bt_declaration_unref(&trace->packet_header_decl->p);

	bt_free_declaration_scope(trace->root_declaration_scope);
	bt_free_declaration_scope(trace->declaration_scope);

	g_hash_table_destroy(trace->callsites);
	g_hash_table_destroy(trace->parent.clocks);

	metadata_stream = container_of(trace->metadata, struct ctf_file_stream, parent);
	g_free(metadata_stream);

	return 0;
}
