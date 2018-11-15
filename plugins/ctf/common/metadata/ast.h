#ifndef _CTF_AST_H
#define _CTF_AST_H

/*
 * ctf-ast.h
 *
 * Copyright 2011-2012 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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

#include <stdint.h>
#include <stdio.h>
#include <glib.h>
#include <babeltrace/list-internal.h>
#include <babeltrace/babeltrace.h>
#include <babeltrace/babeltrace-internal.h>

#include "decoder.h"
#include "ctf-meta.h"

// the parameter name (of the reentrant 'yyparse' function)
// data is a pointer to a 'SParserParam' structure
//#define YYPARSE_PARAM	scanner

struct ctf_node;
struct ctf_parser;
struct ctf_visitor_generate_ir;

#define EINCOMPLETE	1000

#define FOREACH_CTF_NODES(F) \
	F(NODE_UNKNOWN) \
	F(NODE_ROOT) \
	F(NODE_ERROR) \
	F(NODE_EVENT) \
	F(NODE_STREAM) \
	F(NODE_ENV) \
	F(NODE_TRACE) \
	F(NODE_CLOCK) \
	F(NODE_CALLSITE) \
	F(NODE_CTF_EXPRESSION) \
	F(NODE_UNARY_EXPRESSION) \
	F(NODE_TYPEDEF) \
	F(NODE_TYPEALIAS_TARGET) \
	F(NODE_TYPEALIAS_ALIAS) \
	F(NODE_TYPEALIAS) \
	F(NODE_TYPE_SPECIFIER) \
	F(NODE_TYPE_SPECIFIER_LIST) \
	F(NODE_POINTER) \
	F(NODE_TYPE_DECLARATOR) \
	F(NODE_FLOATING_POINT) \
	F(NODE_INTEGER) \
	F(NODE_STRING) \
	F(NODE_ENUMERATOR) \
	F(NODE_ENUM) \
	F(NODE_STRUCT_OR_VARIANT_DECLARATION) \
	F(NODE_VARIANT) \
	F(NODE_STRUCT)

enum node_type {
#define ENTRY(S)	S,
	FOREACH_CTF_NODES(ENTRY)
#undef ENTRY
	NR_NODE_TYPES,
};

struct ctf_node {
	/*
	 * Parent node is only set on demand by specific visitor.
	 */
	struct ctf_node *parent;
	struct bt_list_head siblings;
	struct bt_list_head tmp_head;
	unsigned int lineno;
	/*
	 * We mark nodes visited in the generate-ir phase (last
	 * phase). We only mark the 1-depth level nodes as visited
	 * (never the root node, and not their sub-nodes). This allows
	 * skipping already visited nodes when doing incremental
	 * metadata append.
	 */
	int visited;

	enum node_type type;
	union {
		struct {
		} unknown;
		struct {
			/*
			 * Children nodes are ctf_expression, field_class_def,
			 * field_class_alias and field_class_specifier_list.
			 */
			struct bt_list_head declaration_list;
			struct bt_list_head trace;
			struct bt_list_head env;
			struct bt_list_head stream;
			struct bt_list_head event;
			struct bt_list_head clock;
			struct bt_list_head callsite;
		} root;
		struct {
			/*
			 * Children nodes are ctf_expression, field_class_def,
			 * field_class_alias and field_class_specifier_list.
			 */
			struct bt_list_head declaration_list;
		} event;
		struct {
			/*
			 * Children nodes are ctf_expression, field_class_def,
			 * field_class_alias and field_class_specifier_list.
			 */
			struct bt_list_head declaration_list;
		} stream;
		struct {
			/*
			 * Children nodes are ctf_expression, field_class_def,
			 * field_class_alias and field_class_specifier_list.
			 */
			struct bt_list_head declaration_list;
		} env;
		struct {
			/*
			 * Children nodes are ctf_expression, field_class_def,
			 * field_class_alias and field_class_specifier_list.
			 */
			struct bt_list_head declaration_list;
		} trace;
		struct {
			/*
			 * Children nodes are ctf_expression, field_class_def,
			 * field_class_alias and field_class_specifier_list.
			 */
			struct bt_list_head declaration_list;
		} clock;
		struct {
			/*
			 * Children nodes are ctf_expression, field_class_def,
			 * field_class_alias and field_class_specifier_list.
			 */
			struct bt_list_head declaration_list;
		} callsite;
		struct {
			struct bt_list_head left;	/* Should be string */
			struct bt_list_head right;	/* Unary exp. or type */
		} ctf_expression;
		struct {
			enum {
				UNARY_UNKNOWN = 0,
				UNARY_STRING,
				UNARY_SIGNED_CONSTANT,
				UNARY_UNSIGNED_CONSTANT,
				UNARY_SBRAC,
			} type;
			union {
				/*
				 * string for identifier, id_type, keywords,
				 * string literals and character constants.
				 */
				char *string;
				int64_t signed_constant;
				uint64_t unsigned_constant;
				struct ctf_node *sbrac_exp;
			} u;
			enum {
				UNARY_LINK_UNKNOWN = 0,
				UNARY_DOTLINK,
				UNARY_ARROWLINK,
				UNARY_DOTDOTDOT,
			} link;
		} unary_expression;
		struct {
			struct ctf_node *field_class_specifier_list;
			struct bt_list_head field_class_declarators;
		} field_class_def;
		/* new type is "alias", existing type "target" */
		struct {
			struct ctf_node *field_class_specifier_list;
			struct bt_list_head field_class_declarators;
		} field_class_alias_target;
		struct {
			struct ctf_node *field_class_specifier_list;
			struct bt_list_head field_class_declarators;
		} field_class_alias_name;
		struct {
			struct ctf_node *target;
			struct ctf_node *alias;
		} field_class_alias;
		struct {
			enum {
				TYPESPEC_UNKNOWN = 0,
				TYPESPEC_VOID,
				TYPESPEC_CHAR,
				TYPESPEC_SHORT,
				TYPESPEC_INT,
				TYPESPEC_LONG,
				TYPESPEC_FLOAT,
				TYPESPEC_DOUBLE,
				TYPESPEC_SIGNED,
				TYPESPEC_UNSIGNED,
				TYPESPEC_BOOL,
				TYPESPEC_COMPLEX,
				TYPESPEC_IMAGINARY,
				TYPESPEC_CONST,
				TYPESPEC_ID_TYPE,
				TYPESPEC_FLOATING_POINT,
				TYPESPEC_INTEGER,
				TYPESPEC_STRING,
				TYPESPEC_STRUCT,
				TYPESPEC_VARIANT,
				TYPESPEC_ENUM,
			} type;
			/* For struct, variant and enum */
			struct ctf_node *node;
			const char *id_type;
		} field_class_specifier;
		struct {
			/* list of field_class_specifier */
			struct bt_list_head head;
		} field_class_specifier_list;
		struct {
			unsigned int const_qualifier;
		} pointer;
		struct {
			struct bt_list_head pointers;
			enum {
				TYPEDEC_UNKNOWN = 0,
				TYPEDEC_ID,	/* identifier */
				TYPEDEC_NESTED,	/* (), array or sequence */
			} type;
			union {
				char *id;
				struct {
					/* typedec has no pointer list */
					struct ctf_node *field_class_declarator;
					/*
					 * unary expression (value) or
					 * field_class_specifier_list.
					 */
					struct bt_list_head length;
					/* for abstract type declarator */
					unsigned int abstract_array;
				} nested;
			} u;
			struct ctf_node *bitfield_len;
		} field_class_declarator;
		struct {
			/* Children nodes are ctf_expression. */
			struct bt_list_head expressions;
		} floating_point;
		struct {
			/* Children nodes are ctf_expression. */
			struct bt_list_head expressions;
		} integer;
		struct {
			/* Children nodes are ctf_expression. */
			struct bt_list_head expressions;
		} string;
		struct {
			char *id;
			/*
			 * Range list or single value node. Contains unary
			 * expressions.
			 */
			struct bt_list_head values;
		} enumerator;
		struct {
			char *enum_id;
			/*
			 * Either NULL, or points to unary expression or
			 * field_class_specifier_list.
			 */
			struct ctf_node *container_field_class;
			struct bt_list_head enumerator_list;
			int has_body;
		} _enum;
		struct {
			struct ctf_node *field_class_specifier_list;
			struct bt_list_head field_class_declarators;
		} struct_or_variant_declaration;
		struct {
			char *name;
			char *choice;
			/*
			 * list of field_class_def, field_class_alias and
			 * declarations
			 */
			struct bt_list_head declaration_list;
			int has_body;
		} variant;
		struct {
			char *name;
			/*
			 * list of field_class_def, field_class_alias and
			 * declarations
			 */
			struct bt_list_head declaration_list;
			int has_body;
			struct bt_list_head min_align;	/* align() attribute */
		} _struct;
	} u;
};

struct ctf_ast {
	struct ctf_node root;
};

const char *node_type(struct ctf_node *node);

BT_HIDDEN
struct ctf_visitor_generate_ir *ctf_visitor_generate_ir_create(
		const struct ctf_metadata_decoder_config *config,
		const char *name);

void ctf_visitor_generate_ir_destroy(struct ctf_visitor_generate_ir *visitor);

BT_HIDDEN
struct bt_trace *ctf_visitor_generate_ir_get_ir_trace(
		struct ctf_visitor_generate_ir *visitor);

BT_HIDDEN
struct ctf_trace_class *ctf_visitor_generate_ir_borrow_ctf_trace_class(
		struct ctf_visitor_generate_ir *visitor);

BT_HIDDEN
int ctf_visitor_generate_ir_visit_node(struct ctf_visitor_generate_ir *visitor,
		struct ctf_node *node);

BT_HIDDEN
int ctf_visitor_semantic_check(int depth, struct ctf_node *node);

BT_HIDDEN
int ctf_visitor_parent_links(int depth, struct ctf_node *node);

#endif /* _CTF_AST_H */
