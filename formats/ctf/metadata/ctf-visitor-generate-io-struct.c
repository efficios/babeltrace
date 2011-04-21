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
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <glib.h>
#include <inttypes.h>
#include <endian.h>
#include <errno.h>
#include <babeltrace/list.h>
#include <uuid/uuid.h>
#include "ctf-scanner.h"
#include "ctf-parser.h"
#include "ctf-ast.h"

#define fprintf_dbg(fd, fmt, args...)	fprintf(fd, "%s: " fmt, __func__, ## args)

#define _cds_list_first_entry(ptr, type, member)	\
	cds_list_entry((ptr)->next, type, member)

static
struct declaration *ctf_declaration_specifier_visit(FILE *fd,
		int depth, struct cds_list_head *head,
		struct declaration_scope *declaration_scope);

/*
 * String returned must be freed by the caller using g_free.
 */
static
char *concatenate_unary_strings(struct cds_list_head *head)
{
	struct ctf_node *node;
	GString *str;
	int i = 0;

	str = g_string_new();
	cds_list_for_each_entry(node, head, siblings) {
		char *src_string;

		assert(node->type == NODE_UNARY_EXPRESSION);
		assert(node->u.unary_expression.type == UNARY_STRING);
		assert((node->u.unary_expression.link == UNARY_LINK_UNKNOWN)
			^ (i != 0))
		switch (node->u.unary_expression.link) {
		case UNARY_DOTLINK:
			g_string_append(str, ".")
			break;
		case UNARY_ARROWLINK:
			g_string_append(str, "->")
			break;
		case UNARY_DOTDOTDOT:
			g_string_append(str, "...")
			break;
		}
		src_string = u.unary_expression.u.string;
		g_string_append(str, src_string);
		i++;
	}
	return g_string_free(str, FALSE);
}

static
int get_unary_unsigned(struct cds_list_head *head, uint64_t *value)
{
	struct ctf_node *node;
	int i = 0;

	cds_list_for_each_entry(node, head, siblings) {
		assert(node->type == NODE_UNARY_EXPRESSION);
		assert(node->u.unary_expression.type == UNARY_UNSIGNED_CONSTANT);
		assert(node->u.unary_expression.link == UNARY_LINK_UNKNOWN);
		assert(i == 0);
		*value = node->u.unary_expression.unsigned_constant
		i++;
	}
	return 0;
}

static
int get_unary_uuid(struct cds_list_head *head, uuid_t *uuid)
{
	struct ctf_node *node;
	int i = 0;
	int ret = -1;

	cds_list_for_each_entry(node, head, siblings) {
		const char *src_string;

		assert(node->type == NODE_UNARY_EXPRESSION);
		assert(node->u.unary_expression.type == UNARY_STRING);
		assert(node->u.unary_expression.link == UNARY_LINK_UNKNOWN);
		assert(i == 0);
		src_string = u.unary_expression.u.string;
		ret = uuid_parse(u.unary_expression.u.string, *uuid);
	}
	return ret;
}

static
struct ctf_stream *trace_stream_lookup(struct ctf_trace *trace, uint64_t stream_id)
{
	if (trace->streams->len <= stream_id)
		return NULL;
	return g_ptr_array_index(trace->streams, stream_id);
}

static
void visit_declaration_specifier(struct cds_list_head *declaration_specifier, GString *str)
{
	struct ctf_node *iter;
	int alias_item_nr = 0;
	int err;

	cds_list_for_each_entry(iter, declaration_specifier, siblings) {
		if (alias_item_nr != 0)
			g_string_append(str, " ");
		alias_item_nr++;

		switch (iter->type) {
		case NODE_TYPE_SPECIFIER:
			switch (iter->u.type_specifier.type) {
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
				if (iter->u.type_specifier.id_type)
					g_string_append(str, iter->u.type_specifier.id_type);
				break;
			default:
				fprintf(stderr, "[error] %s: unknown specifier\n", __func__);
				err = -EINVAL;
				goto error;
			}
			break;
		case NODE_ENUM:
			if (!iter->u._enum.enum_id) {
				fprintf(stderr, "[error] %s: unexpected empty enum ID\n", __func__);
				err = -EINVAL;
				goto error;
			}
			g_string_append(str, "enum ");
			g_string_append(str, iter->u._enum.enum_id);
			break;
		case NODE_VARIANT:
			if (!iter->u.variant.name) {
				fprintf(stderr, "[error] %s: unexpected empty variant name\n", __func__);
				err = -EINVAL;
				goto error;
			}
			g_string_append(str, "variant ");
			g_string_append(str, iter->u.variant.name);
			break;
		case NODE_STRUCT:
			if (!iter->u._struct.name) {
				fprintf(stderr, "[error] %s: unexpected empty variant name\n", __func__);
				err = -EINVAL;
				goto error;
			}
			g_string_append(str, "struct ");
			g_string_append(str, iter->u._struct.name);
			break;
		default:
			fprintf(stderr, "[error] %s: unexpected node type %d\n", __func__, (int) iter->type);
			err = -EINVAL;
			goto error;
		}
	}
	return 0;
error:
	return err;
}

static
GQuark create_typealias_identifier(int fd, int depth,
	struct cds_list_head *declaration_specifier,
	struct ctf_node *node_type_declarator)
{
	GString *str;
	const char *str_c;
	GQuark alias_q;
	int ret;

	str = g_string_new();
	ret = visit_declaration_specifier(declaration_specifier, str);
	if (ret) {
		g_string_free(str, TRUE);
		return 0;
	}
	cds_list_for_each_entry(iter, &node_type_declarator->u.type_declarator.pointers, siblings) {
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
struct declaration *ctf_type_declarator_visit(int fd, int depth,
	struct cds_list_head *declaration_specifier,
	GQuark *field_name,
	struct ctf_node *node_type_declarator,
	struct declaration_scope *declaration_scope,
	struct declaration *nested_declaration)
{
	/*
	 * Visit type declarator by first taking care of sequence/array
	 * (recursively). Then, when we get to the identifier, take care
	 * of pointers.
	 */

	assert(node_type_declarator->u.type_declarator.type != TYPEDEC_UNKNOWN);

	/* TODO: gcc bitfields not supported yet. */
	if (node_type_declarator->u.type_declarator.bitfield_len != NULL) {
		fprintf(stderr, "[error] %s: gcc bitfields are not supported yet.\n", __func__);
		return NULL;
	}

	if (!nested_declaration) {
		if (!cds_list_empty(&node_type_declarator->u.type_declarator.pointers)) {
			GQuark alias_q;

			/*
			 * If we have a pointer declarator, it _has_ to be present in
			 * the typealiases (else fail).
			 */
			alias_q = create_typealias_identifier(fd, depth,
				declaration_specifier, node_type_declarator);
			nested_declaration = lookup_declaration(alias_q, declaration_scope);
			if (!nested_declaration) {
				fprintf(stderr, "[error] %s: cannot find typealias \"%s\".\n", __func__, g_quark_to_string(alias_q));
				return NULL;
			}
		} else {
			nested_declaration = /* parse declaration_specifier */;
		}
	}

	if (node_type_declarator->u.type_declarator.type == TYPEDEC_ID) {
		if (node_type_declarator->u.type_declarator.u.id)
			*field_name = g_quark_from_string(node_type_declarator->u.type_declarator.u.id);
		else
			*field_name = 0;
		return nested_declaration;
	} else {
		struct declaration *declaration;
		struct node *length;

		/* TYPEDEC_NESTED */

		/* create array/sequence, pass nested_declaration as child. */
		length = node_type_declarator->u.type_declarator.u.nested.length;
		if (length) {
			switch (length->type) {
			case NODE_UNARY_EXPRESSION:
				/* Array */
				/* TODO */
		.............
				declaration = /* create array */;
				break;
			case NODE_TYPE_SPECIFIER:
				/* Sequence */
				declaration = /* create sequence */;
				break;
			default:
				assert(0);
			}
		}

		/* Pass it as content of outer container */
		declaration = ctf_type_declarator_visit(fd, depth,
				declaration_specifier, field_name,
				node_type_declarator->u.type_declarator.u.nested.type_declarator,
				declaration_scope, declaration);
		return declaration;
	}
}

static
int ctf_struct_type_declarators_visit(int fd, int depth,
	struct declaration_struct *struct_declaration,
	struct cds_list_head *declaration_specifier,
	struct cds_list_head *type_declarators,
	struct declaration_scope *declaration_scope)
{
	struct ctf_node *iter;
	GQuark field_name;

	cds_list_for_each_entry(iter, type_declarators, siblings) {
		struct declaration *field_declaration;

		field_declaration = ctf_type_declarator_visit(fd, depth,
						declaration_specifier,
						&field_name, iter,
						struct_declaration->scope,
						NULL);
		struct_declaration_add_field(struct_declaration,
					     g_quark_to_string(field_name),
					     field_declaration);
	}
	return 0;
}

static
int ctf_variant_type_declarators_visit(int fd, int depth,
	struct declaration_variant *variant_declaration,
	struct cds_list_head *declaration_specifier,
	struct cds_list_head *type_declarators,
	struct declaration_scope *declaration_scope)
{
	struct ctf_node *iter;
	GQuark field_name;

	cds_list_for_each_entry(iter, type_declarators, siblings) {
		struct declaration *field_declaration;

		field_declaration = ctf_type_declarator_visit(fd, depth,
						declaration_specifier,
						&field_name, iter,
						variant_declaration->scope,
						NULL);
		variant_declaration_add_field(variant_declaration,
					      g_quark_to_string(field_name),
					      field_declaration);
	}
	return 0;
}

static
int ctf_typedef_visit(int fd, int depth, struct declaration_scope *scope,
		struct cds_list_head *declaration_specifier,
		struct cds_list_head *type_declarators)
{
	struct ctf_node *iter;
	GQuark identifier;

	cds_list_for_each_entry(iter, type_declarators, siblings) {
		struct declaration *type_declaration;
		int ret;
	
		type_declaration = ctf_type_declarator_visit(fd, depth,
					declaration_specifier,
					&identifier, iter,
					scope, NULL);
		ret = register_declaration(identifier, type_declaration, scope);
		if (ret) {
			type_declaration->declaration_free(type_declaration);
			return ret;
		}
	}
	return 0;
}

static
int ctf_typealias_visit(int fd, int depth, struct declaration_scope *scope,
		struct ctf_node *target, struct ctf_node *alias)
{
	struct declaration *type_declaration;
	struct ctf_node *iter, *node;
	GQuark dummy_id;
	GQuark alias_q;

	/* See ctf_visitor_type_declarator() in the semantic validator. */

	/*
	 * Create target type declaration.
	 */

	type_declaration = ctf_type_declarator_visit(fd, depth,
		&target->u.typealias_target.declaration_specifier,
		&dummy_id, &target->u.typealias_target.type_declarators,
		scope, NULL);
	if (!type_declaration) {
		fprintf(stderr, "[error] %s: problem creating type declaration\n", __func__);
		err = -EINVAL;
		goto error;
	}
	/*
	 * The semantic validator does not check whether the target is
	 * abstract or not (if it has an identifier). Check it here.
	 */
	if (dummy_id != 0) {
		fprintf(stderr, "[error] %s: expecting empty identifier\n", __func__);
		err = -EINVAL;
		goto error;
	}
	/*
	 * Create alias identifier.
	 */

	node = _cds_list_first_entry(&alias->u.typealias_alias.type_declarators,
				struct node, siblings);
	alias_q = create_typealias_identifier(fd, depth,
			&alias->u.typealias_alias.declaration_specifier, node);
	ret = register_declaration(alias_q, type_declaration, scope);
	if (ret)
		goto error;
	return 0;

error:
	type_declaration->declaration_free(type_declaration);
	return ret;
}

static
int ctf_struct_declaration_list_visit(int fd, int depth,
	struct ctf_node *iter, struct declaration_struct *struct_declaration)
{
	struct declaration *declaration;
	int ret;

	switch (iter->type) {
	case NODE_TYPEDEF:
		/* For each declarator, declare type and add type to struct declaration scope */
		ret = ctf_typedef_visit(fd, depth,
			struct_declaration->scope,
			&iter->u._typedef.declaration_specifier,
			&iter->u._typedef.type_declarators);
		if (ret)
			return ret;
		break;
	case NODE_TYPEALIAS:
		/* Declare type with declarator and add type to struct declaration scope */
		ret = ctf_typealias_visit(fd, depth,
			struct_declaration->scope,
			iter->u.typealias.target,
			iter->u.typealias.alias);
		if (ret)
			return ret;
		break;
	case NODE_STRUCT_OR_VARIANT_DECLARATION:
		/* Add field to structure declaration */
		ret = ctf_struct_type_declarators_visit(fd, depth,
				struct_declaration,
				&iter->u.struct_or_variant_declaration.declaration_specifier,
				&iter->u.struct_or_variant_declaration.type_declarators);
		if (ret)
			return ret;
		break;
	default:
		fprintf(stderr, "[error] %s: unexpected node type %d\n", __func__, (int) iter->type);
		assert(0);
	}
	return 0;
}

static
int ctf_variant_declaration_list_visit(int fd, int depth,
	struct ctf_node *iter, struct declaration_variant *variant_declaration)
{
	struct declaration *declaration;
	int ret;

	switch (iter->type) {
	case NODE_TYPEDEF:
		/* For each declarator, declare type and add type to variant declaration scope */
		ret = ctf_typedef_visit(fd, depth,
			variant_declaration->scope,
			&iter->u._typedef.declaration_specifier,
			&iter->u._typedef.type_declarators);
		if (ret)
			return ret;
		break;
	case NODE_TYPEALIAS:
		/* Declare type with declarator and add type to variant declaration scope */
		ret = ctf_typealias_visit(fd, depth,
			variant_declaration->scope,
			iter->u.typealias.target,
			iter->u.typealias.alias);
		if (ret)
			return ret;
		break;
	case NODE_STRUCT_OR_VARIANT_DECLARATION:
		/* Add field to structure declaration */
		ret = ctf_variant_type_declarators_visit(fd, depth,
				variant_declaration,
				&iter->u.struct_or_variant_declaration.declaration_specifier,
				&iter->u.struct_or_variant_declaration.type_declarators);
		if (ret)
			return ret;
		break;
	default:
		fprintf(stderr, "[error] %s: unexpected node type %d\n", __func__, (int) iter->type);
		assert(0);
	}
	return 0;
}

static
struct declaration_struct *ctf_declaration_struct_visit(FILE *fd,
	int depth, const char *name, struct cds_list_head *declaration_list,
	int has_body, struct declaration_scope *declaration_scope)
{
	struct declaration *declaration;
	struct declaration_struct *struct_declaration;
	struct ctf_node *iter;

	/*
	 * For named struct (without body), lookup in
	 * declaration scope. Don't take reference on struct
	 * declaration: ref is only taken upon definition.
	 */
	if (!has_body) {
		assert(name);
		struct_declaration =
			lookup_struct_declaration(g_quark_from_string(name),
						  declaration_scope);
		return struct_declaration;
	} else {
		/* For unnamed struct, create type */
		/* For named struct (with body), create type and add to declaration scope */
		if (name) {
			if (lookup_struct_declaration(g_quark_from_string(name),
						      declaration_scope)) {
				
				fprintf(stderr, "[error] %s: struct %s already declared in scope\n", __func__, name);
				return NULL;
			}
		}
		struct_declaration = struct_declaration_new(name, declaration_scope);
		cds_list_for_each_entry(iter, declaration_list, siblings) {
			ret = ctf_struct_declaration_list_visit(fd, depth + 1, iter, struct_declaration);
			if (ret)
				goto error;
		}
		if (name) {
			ret = register_struct_declaration(g_quark_from_string(name),
					struct_declaration,
					declaration_scope);
			assert(!ret);
		}
		return struct_declaration;
	}
error:
	struct_declaration->p.declaration_free(&struct_declaration->p);
	return NULL;
}

static
struct declaration_variant *ctf_declaration_variant_visit(FILE *fd,
	int depth, const char *name, struct cds_list_head *declaration_list,
	int has_body, struct declaration_scope *declaration_scope)
{
	struct declaration *declaration;
	struct declaration_variant *variant_declaration;
	struct ctf_node *iter;

	/*
	 * For named variant (without body), lookup in
	 * declaration scope. Don't take reference on variant
	 * declaration: ref is only taken upon definition.
	 */
	if (!has_body) {
		assert(name);
		variant_declaration =
			lookup_variant_declaration(g_quark_from_string(name),
						   declaration_scope);
		return variant_declaration;
	} else {
		/* For unnamed variant, create type */
		/* For named variant (with body), create type and add to declaration scope */
		if (name) {
			if (lookup_variant_declaration(g_quark_from_string(name),
						       declaration_scope)) {
				
				fprintf(stderr, "[error] %s: variant %s already declared in scope\n", __func__, name);
				return NULL;
			}
		}
		variant_declaration = variant_declaration_new(name, declaration_scope);
		cds_list_for_each_entry(iter, declaration_list, siblings) {
			ret = ctf_variant_declaration_list_visit(fd, depth + 1, iter, variant_declaration);
			if (ret)
				goto error;
		}
		if (name) {
			ret = register_variant_declaration(g_quark_from_string(name),
					variant_declaration,
					declaration_scope);
			assert(!ret);
		}
		return variant_declaration;
	}
error:
	variant_declaration->p.declaration_free(&variant_declaration->p);
	return NULL;
}

static
int ctf_enumerator_list_visit(int fd, int depth,
		struct ctf_node *enumerator,
		struct declaration_enum *enum_declaration)
{
	/* TODO */
........
	return 0;
}

static
struct declaration *ctf_declaration_enum_visit(int fd, int depth,
			const char *name,
			struct ctf_node *container_type,
			struct cds_list_head *enumerator_list,
			int has_body)
{
	struct declaration *declaration;
	struct declaration_enum *enum_declaration;
	struct declaration_integer *integer_declaration;
	struct ctf_node *iter;

	/*
	 * For named enum (without body), lookup in
	 * declaration scope. Don't take reference on enum
	 * declaration: ref is only taken upon definition.
	 */
	if (!has_body) {
		assert(name);
		enum_declaration =
			lookup_enum_declaration(g_quark_from_string(name),
						declaration_scope);
		return enum_declaration;
	} else {
		/* For unnamed enum, create type */
		/* For named enum (with body), create type and add to declaration scope */
		if (name) {
			if (lookup_enum_declaration(g_quark_from_string(name),
						    declaration_scope)) {
				
				fprintf(stderr, "[error] %s: enum %s already declared in scope\n", __func__, name);
				return NULL;
			}
		}

		/* TODO CHECK Enumerations need to have their size/type specifier (< >). */
		integer_declaration = integer_declaration_new(); /* TODO ... */
		.....
		enum_declaration = enum_declaration_new(name, integer_declaration);
		declaration_unref(&integer_declaration->p);	/* leave ref to enum */
		cds_list_for_each_entry(iter, enumerator_list, siblings) {
			ret = ctf_enumerator_list_visit(fd, depth + 1, iter, enum_declaration);
			if (ret)
				goto error;
		}
		if (name) {
			ret = register_enum_declaration(g_quark_from_string(name),
					enum_declaration,
					declaration_scope);
			assert(!ret);
		}
		return enum_declaration;
	}
error:
	enum_declaration->p.declaration_free(&enum_declaration->p);
	return NULL;
}

static
struct declaration *ctf_declaration_type_specifier_visit(int fd, int depth,
		struct cds_list_head *declaration_specifier,
		struct declaration_scope *declaration_scope)
{
	GString *str;
	struct declaration *declaration;
	const char *str_c;

	str = g_string_new();
	ret = visit_declaration_specifier(declaration_specifier, str);
	if (ret)
		return NULL;
	str_c = g_string_free(str, FALSE);
	id_q = g_quark_from_string(str_c);
	g_free(str_c);
	declaration = lookup_declaration(id_q, declaration_scope);
	return declaration;
}

static
struct declaration *ctf_declaration_integer_visit(int fd, int depth,
		struct cds_list_head *expressions)
{
	struct node *expression;
	uint64_t alignment, size, byte_order = trace->native_bo, signedness = 0;
	int has_alignment = 0, has_size = 0;
	struct declaration_integer *integer_declaration;

	cds_list_for_each_entry(expression, expressions, siblings) {
		struct node *left, *right;

		left = expression->u.ctf_expression.left;
		right = expression->u.ctf_expression.right;
		assert(left->u.unary_expression.type == UNARY_STRING);
		if (!strcmp(left->u.unary_expression.u.string, "signed")) {
			/* TODO */
	...........
		} else if (!strcmp(left->u.unary_expression.u.string, "byte_order")) {

		} else if (!strcmp(left->u.unary_expression.u.string, "size")) {
			has_size = 1;
		} else if (!strcmp(left->u.unary_expression.u.string, "align")) {
			has_alignment = 1;
		} else {
			fprintf(stderr, "[error] %s: unknown attribute name %s\n",
				__func__, left->u.unary_expression.u.string);
			return NULL;
		}
	}
	if (!has_alignment) {
		fprintf(stderr, "[error] %s: missing alignment attribute\n", __func__);
		return NULL;
	}
	if (!has_size) {
		fprintf(stderr, "[error] %s: missing size attribute\n", __func__);
		return NULL;
	}
	integer_declaration = integer_declaration_new(size,
				byte_order, signedness, alignment);
	return &integer_declaration->p;
}

/*
 * Also add named variant, struct or enum to the current declaration scope.
 */
static
struct declaration *ctf_declaration_specifier_visit(FILE *fd,
		int depth, struct cds_list_head *head,
		struct declaration_scope *declaration_scope)
{
	struct declaration *declaration;
	struct node *first;

	first = _cds_list_first_entry(head, struct node, siblings);

	switch (first->type) {
	case NODE_STRUCT:
		return ctf_declaration_struct_visit(fd, depth,
			first->u._struct.name,
			&first->u._struct.declaration_list,
			first->u._struct.has_body,
			declaration_scope);
	case NODE_VARIANT:
		return ctf_declaration_variant_visit(fd, depth,
			first->u.variant.name,
			&first->u.variant.declaration_list,
			first->u.variant.has_body,
			declaration_scope);
	case NODE_ENUM:
		return ctf_declaration_enum_visit(fd, depth,
			first->u._enum.enum_id,
			first->u._enum.container_type,
			&first->u._enum.enumerator_list,
			first->u._enum.has_body);
	case NODE_INTEGER:
		return ctf_declaration_integer_visit(fd, depth,
			&first->u.integer.expressions);
	case NODE_FLOATING_POINT:	/* TODO  */
		return ctf_declaration_floating_point_visit(fd, depth,
			&first->u.floating_point.expressions);
	case NODE_STRING:		/* TODO */
		return ctf_declaration_string_visit(fd, depth,
			&first->u.string.expressions);
	case NODE_TYPE_SPECIFIER:
		return ctf_declaration_type_specifier_visit(fd, depth,
				head, declaration_scope);
	}
}

static
int ctf_event_declaration_visit(FILE *fd, int depth, struct ctf_node *node, struct ctf_event *event, struct ctf_trace *trace)
{
	int ret = 0;

	switch (node->type) {
	case NODE_TYPEDEF:
		ret = ctf_typedef_visit(fd, depth + 1,
					&node->u._typedef.declaration_specifier,
					&node->u._typedef.type_declarators,
					event->declaration_scope);
		if (ret)
			return ret;
		break;
	case NODE_TYPEALIAS:
		ret = ctf_typealias_visit(fd, depth + 1,
				&node->u.typealias.target, &node->u.typealias.alias
				event->declaration_scope);
		if (ret)
			return ret;
		break;
	case NODE_CTF_EXPRESSION:
	{
		char *left;

		left = concatenate_unary_strings(&node->u.ctf_expression.left);
		if (!strcmp(left, "name")) {
			char *right;

			if (CTF_EVENT_FIELD_IS_SET(event, name))
				return -EPERM;
			right = concatenate_unary_strings(&node->u.ctf_expression.right);
			if (!right) {
				fprintf(stderr, "[error] %s: unexpected unary expression for event name\n", __func__);
				return -EINVAL;
			}
			event->name = g_quark_from_string(right);
			g_free(right);
			CTF_EVENT_SET_FIELD(event, name);
		} else if (!strcmp(left, "id")) {
			if (CTF_EVENT_FIELD_IS_SET(event, id))
				return -EPERM;
			ret = get_unary_unsigned(&node->u.ctf_expression.right, &event->id);
			if (ret) {
				fprintf(stderr, "[error] %s: unexpected unary expression for event id\n", __func__);
				return -EINVAL;
			}
			CTF_EVENT_SET_FIELD(event, id);
		} else if (!strcmp(left, "stream_id")) {
			if (CTF_EVENT_FIELD_IS_SET(event, stream_id))
				return -EPERM;
			ret = get_unary_unsigned(&node->u.ctf_expression.right, &event->stream_id);
			if (ret) {
				fprintf(stderr, "[error] %s: unexpected unary expression for event stream_id\n", __func__);
				return -EINVAL;
			}
			event->stream = trace_stream_lookup(trace, event->stream_id);
			if (!event->stream) {
				fprintf(stderr, "[error] %s: stream id %" PRIu64 " cannot be found\n", __func__, event->stream_id);
				return -EINVAL;
			}
			CTF_EVENT_SET_FIELD(event, stream_id);
		} else if (!strcmp(left, "context")) {
			struct declaration *declaration;

			if (!event->definition_scope)
				return -EPERM;
			declaration = ctf_declaration_specifier_visit(fd, depth,
					&node->u.ctf_expression.right,
					event->declaration_scope);
			if (!declaration)
				return -EPERM;
			if (declaration->type->id != CTF_TYPE_STRUCT)
				return -EPERM;
			event->context_decl = container_of(declaration, struct declaration_struct, p);
		} else if (!strcmp(left, "fields")) {
			struct declaration *declaration;

			if (!event->definition_scope)
				return -EPERM;
			declaration = ctf_declaration_specifier_visit(fd, depth,
					&node->u.ctf_expression.right,
					event->declaration_scope);
			if (!declaration)
				return -EPERM;
			if (declaration->type->id != CTF_TYPE_STRUCT)
				return -EPERM;
			event->fields_decl = container_of(declaration, struct declaration_struct, p);
		}
		g_free(left);
		break;
	}
	default:
		return -EPERM;
	/* TODO: declaration specifier should be added. */
	}

	return 0;
}

static
int ctf_event_visit(FILE *fd, int depth, struct ctf_node *node,
		    struct declaration_scope *parent_declaration_scope, struct ctf_trace *trace)
{
	int ret = 0;
	struct ctf_node *iter;
	struct ctf_event *event;
	struct definition_scope *parent_def_scope;

	event = g_new0(struct ctf_event, 1);
	event->declaration_scope = new_declaration_scope(parent_declaration_scope);
	cds_list_for_each_entry(iter, &node->u.event.declaration_list, siblings) {
		ret = ctf_event_declaration_visit(fd, depth + 1, iter, event, trace);
		if (ret)
			goto error;
	}
	if (!CTF_EVENT_FIELD_IS_SET(event, name)) {
		ret = -EPERM;
		goto error;
	}
	if (!CTF_EVENT_FIELD_IS_SET(event, id)) {
		ret = -EPERM;
		goto error;
	}
	if (!CTF_EVENT_FIELD_IS_SET(event, stream_id)) {
		ret = -EPERM;
		goto error;
	}
	if (event->stream->events_by_id->len <= event->id)
		g_ptr_array_set_size(event->stream->events_by_id, event->id + 1);
	g_ptr_array_index(event->stream->events_by_id, event->id) = event;
	g_hash_table_insert(event->stream->event_quark_to_id,
			    (gpointer)(unsigned long) event->name,
			    &event->id);
	parent_def_scope = stream->definition_scope;
	if (event->context_decl) {
		event->context =
			event->context_decl->definition_new(event->context_decl,
				parent_def_scope, 0, 0);
		set_dynamic_definition_scope(&event->context->p,
					     event->context->scope,
					     "event.context");
		parent_def_scope = event->context->scope;
		declaration_unref(event->context_decl);
	}
	if (event->fields_decl) {
		event->fields =
			event->fields_decl->definition_new(event->fields_decl,
				parent_def_scope, 0, 0);
		set_dynamic_definition_scope(&event->fields->p,
					     event->fields->scope,
					     "event.fields");
		parent_def_scope = event->fields->scope;
		declaration_unref(event->fields_decl);
	}
	return 0;

error:
	declaration_unref(event->fields_decl);
	declaration_unref(event->context_decl);
	free_definition_scope(event->definition_scope);
	free_declaration_scope(event->declaration_scope);
	g_free(event);
	return ret;
}

 
static
int ctf_stream_declaration_visit(FILE *fd, int depth, struct ctf_node *node, struct ctf_stream *stream, struct ctf_trace *trace)
{
	int ret = 0;

	switch (node->type) {
	case NODE_TYPEDEF:
		ret = ctf_typedef_visit(fd, depth + 1,
					&node->u._typedef.declaration_specifier,
					&node->u._typedef.type_declarators,
					stream->declaration_scope);
		if (ret)
			return ret;
		break;
	case NODE_TYPEALIAS:
		ret = ctf_typealias_visit(fd, depth + 1,
				&node->u.typealias.target, &node->u.typealias.alias
				stream->declaration_scope);
		if (ret)
			return ret;
		break;
	case NODE_CTF_EXPRESSION:
	{
		char *left;

		left = concatenate_unary_strings(&node->u.ctf_expression.left);
		if (!strcmp(left, "stream_id")) {
			if (CTF_EVENT_FIELD_IS_SET(event, stream_id))
				return -EPERM;
			ret = get_unary_unsigned(&node->u.ctf_expression.right, &event->stream_id);
			if (ret) {
				fprintf(stderr, "[error] %s: unexpected unary expression for event stream_id\n", __func__);
				return -EINVAL;
			}
			CTF_EVENT_SET_FIELD(event, stream_id);
		} else if (!strcmp(left, "event.header")) {
			struct declaration *declaration;

			declaration = ctf_declaration_specifier_visit(fd, depth,
					&node->u.ctf_expression.right,
					stream->declaration_scope, stream->definition_scope);
			if (!declaration)
				return -EPERM;
			if (declaration->type->id != CTF_TYPE_STRUCT)
				return -EPERM;
			stream->event_header_decl = container_of(declaration, struct declaration_struct, p);
		} else if (!strcmp(left, "event.context")) {
			struct declaration *declaration;

			declaration = ctf_declaration_specifier_visit(fd, depth,
					&node->u.ctf_expression.right,
					stream->declaration_scope);
			if (!declaration)
				return -EPERM;
			if (declaration->type->id != CTF_TYPE_STRUCT)
				return -EPERM;
			stream->event_context_decl = container_of(declaration, struct declaration_struct, p);
		} else if (!strcmp(left, "packet.context")) {
			struct declaration *declaration;

			declaration = ctf_declaration_specifier_visit(fd, depth,
					&node->u.ctf_expression.right,
					stream->declaration_scope);
			if (!declaration)
				return -EPERM;
			if (declaration->type->id != CTF_TYPE_STRUCT)
				return -EPERM;
			stream->packet_context_decl = container_of(declaration, struct declaration_struct, p);
		}
		g_free(left);
		break;
	}
	default:
		return -EPERM;
	/* TODO: declaration specifier should be added. */
	}

	return 0;
}

static
int ctf_stream_visit(FILE *fd, int depth, struct ctf_node *node,
		     struct declaration_scope *parent_declaration_scope, struct ctf_trace *trace)
{
	int ret = 0;
	struct ctf_node *iter;
	struct ctf_stream *stream;
	struct definition_scope *parent_def_scope;

	stream = g_new0(struct ctf_stream, 1);
	stream->declaration_scope = new_declaration_scope(parent_declaration_scope);
	stream->events_by_id = g_ptr_array_new();
	stream->event_quark_to_id = g_hash_table_new(g_int_hash, g_int_equal);
	cds_list_for_each_entry(iter, &node->u.stream.declaration_list, siblings) {
		ret = ctf_stream_declaration_visit(fd, depth + 1, iter, stream, trace);
		if (ret)
			goto error;
	}
	if (!CTF_EVENT_FIELD_IS_SET(stream, stream_id)) {
		ret = -EPERM;
		goto error;
	}
	if (trace->streams->len <= stream->stream_id)
		g_ptr_array_set_size(trace->streams, stream->stream_id + 1);
	g_ptr_array_index(trace->streams, stream->stream_id) = stream;

	parent_def_scope = NULL;
	if (stream->packet_context_decl) {
		stream->packet_context =
			stream->packet_context_decl->definition_new(stream->packet_context_decl,
				parent_def_scope, 0, 0);
		set_dynamic_definition_scope(&stream->packet_context->p,
					     stream->packet_context->scope,
					     "stream.packet.context");
		parent_def_scope = stream->packet_context->scope;
		declaration_unref(stream->packet_context_decl);
	}
	if (stream->event_header_decl) {
		stream->event_header =
			stream->event_header_decl->definition_new(stream->event_header_decl,
				parent_def_scope, 0, 0);
		set_dynamic_definition_scope(&stream->event_header->p,
					     stream->event_header->scope,
					     "stream.event.header");
		parent_def_scope = stream->event_header->scope;
		declaration_unref(stream->event_header_decl);
	}
	if (stream->event_context_decl) {
		stream->event_context =
			stream->event_context_decl->definition_new(stream->event_context_decl,
				parent_def_scope, 0, 0);
		set_dynamic_definition_scope(&stream->event_context->p,
					     stream->event_context->scope,
					     "stream.event.context");
		parent_def_scope = stream->event_context->scope;
		declaration_unref(stream->event_context_decl);
	}
	stream->definition_scope = parent_def_scope;

	return 0;

error:
	declaration_unref(stream->event_header);
	declaration_unref(stream->event_context);
	declaration_unref(stream->packet_context);
	g_ptr_array_free(stream->events_by_id, TRUE);
	g_hash_table_free(stream->event_quark_to_id);
	free_definition_scope(stream->definition_scope);
	free_declaration_scope(stream->declaration_scope);
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
					&node->u._typedef.declaration_specifier,
					&node->u._typedef.type_declarators,
					trace->declaration_scope);
		if (ret)
			return ret;
		break;
	case NODE_TYPEALIAS:
		ret = ctf_typealias_visit(fd, depth + 1,
				&node->u.typealias.target, &node->u.typealias.alias
				trace->declaration_scope);
		if (ret)
			return ret;
		break;
	case NODE_CTF_EXPRESSION:
	{
		char *left;

		left = concatenate_unary_strings(&node->u.ctf_expression.left);
		if (!strcmp(left, "major")) {
			if (CTF_EVENT_FIELD_IS_SET(trace, major))
				return -EPERM;
			ret = get_unary_unsigned(&node->u.ctf_expression.right, &trace->major);
			if (ret) {
				fprintf(stderr, "[error] %s: unexpected unary expression for trace major number\n", __func__);
				return -EINVAL;
			}
			CTF_EVENT_SET_FIELD(trace, major);
		} else if (!strcmp(left, "minor")) {
			if (CTF_EVENT_FIELD_IS_SET(trace, minor))
				return -EPERM;
			ret = get_unary_unsigned(&node->u.ctf_expression.right, &trace->minor);
			if (ret) {
				fprintf(stderr, "[error] %s: unexpected unary expression for trace minor number\n", __func__);
				return -EINVAL;
			}
			CTF_EVENT_SET_FIELD(trace, minor);
		} else if (!strcmp(left, "word_size")) {
			if (CTF_EVENT_FIELD_IS_SET(trace, word_size))
				return -EPERM;
			ret = get_unary_unsigned(&node->u.ctf_expression.right, &trace->word_size);
			if (ret) {
				fprintf(stderr, "[error] %s: unexpected unary expression for trace word_size\n", __func__);
				return -EINVAL;
			}
			CTF_EVENT_SET_FIELD(trace, word_size);
		} else if (!strcmp(left, "uuid")) {
			if (CTF_EVENT_FIELD_IS_SET(trace, uuid))
				return -EPERM;
			ret = get_unary_uuid(&node->u.ctf_expression.right, &trace->uuid);
			if (ret) {
				fprintf(stderr, "[error] %s: unexpected unary expression for trace uuid\n", __func__);
				return -EINVAL;
			}
			CTF_EVENT_SET_FIELD(trace, uuid);
		}
		g_free(left);
		break;
	}
	default:
		return -EPERM;
	/* TODO: declaration specifier should be added. */
	}

	return 0;
}

static
int ctf_trace_visit(FILE *fd, int depth, struct ctf_node *node, struct ctf_trace *trace)
{
	int ret = 0;
	struct ctf_node *iter;

	if (trace->declaration_scope)
		return -EEXIST;
	trace->declaration_scope = new_declaration_scope(trace->root_declaration_scope);
	trace->definition_scope = new_dynamic_definition_scope(trace->root_definition_scope);
	trace->streams = g_ptr_array_new();
	cds_list_for_each_entry(iter, &node->u.trace.declaration_list, siblings) {
		ret = ctf_trace_declaration_visit(fd, depth + 1, iter, trace);
		if (ret)
			goto error;
	}
	if (!CTF_EVENT_FIELD_IS_SET(trace, major)) {
		ret = -EPERM;
		goto error;
	}
	if (!CTF_EVENT_FIELD_IS_SET(trace, minor)) {
		ret = -EPERM;
		goto error;
	}
	if (!CTF_EVENT_FIELD_IS_SET(trace, uuid)) {
		ret = -EPERM;
		goto error;
	}
	if (!CTF_EVENT_FIELD_IS_SET(trace, word_size)) {
		ret = -EPERM;
		goto error;
	}
	return 0;

error:
	g_ptr_array_free(trace->streams, TRUE);
	free_definition_scope(stream->definition_scope);
	free_declaration_scope(stream->declaration_scope);
	return ret;
}

int _ctf_visitor(FILE *fd, int depth, struct ctf_node *node, struct ctf_trace *trace)
{
	int ret = 0;
	struct ctf_node *iter;

	switch (node->type) {
	case NODE_ROOT:
		cds_list_for_each_entry(iter, &node->u.root._typedef,
					siblings) {
			ret = ctf_typedef_visit(fd, depth + 1,
						&iter->u._typedef.declaration_specifier,
						&iter->u._typedef.type_declarators,
						trace->root_declaration_scope);
			if (ret)
				return ret;
		}
		cds_list_for_each_entry(iter, &node->u.root.typealias,
					siblings) {
			ret = ctf_typealias_visit(fd, depth + 1,
					&iter->u.typealias.target, &iter->u.typealias.alias
					trace->root_declaration_scope);
			if (ret)
				return ret;
		}
		cds_list_for_each_entry(iter, &node->u.root.declaration_specifier, siblings) {
			ret = ctf_declaration_specifier_visit(fd, depth, iter,
					trace->root_declaration_scope);
			if (ret)
				return ret;
		}
		cds_list_for_each_entry(iter, &node->u.root.trace, siblings) {
			ret = ctf_trace_visit(fd, depth + 1, iter, trace);
			if (ret)
				return ret;
		}
		cds_list_for_each_entry(iter, &node->u.root.stream, siblings) {
			ret = ctf_stream_visit(fd, depth + 1, iter,
		    			       trace->root_declaration_scope, trace);
			if (ret)
				return ret;
		}
		cds_list_for_each_entry(iter, &node->u.root.event, siblings) {
			ret = ctf_event_visit(fd, depth + 1, iter,
		    			      trace->root_declaration_scope, trace);
			if (ret)
				return ret;
		}
		break;
	case NODE_UNKNOWN:
	default:
		fprintf(stderr, "[error] %s: unknown node type %d\n", __func__,
			(int) node->type);
		return -EINVAL;
	}
	return ret;
}
