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
#include <babeltrace/list.h>

// the parameter name (of the reentrant 'yyparse' function)
// data is a pointer to a 'SParserParam' structure
//#define YYPARSE_PARAM	scanner

// the argument for the 'yylex' function
#define YYLEX_PARAM	((struct ctf_scanner *) scanner)->scanner

struct ctf_node;
struct ctf_parser;

enum node_type {
	NODE_UNKNOWN = 0,
	NODE_ROOT,

	NODE_EVENT,
	NODE_STREAM,
	NODE_ENV,
	NODE_TRACE,
	NODE_CLOCK,

	NODE_CTF_EXPRESSION,
	NODE_UNARY_EXPRESSION,

	NODE_TYPEDEF,
	NODE_TYPEALIAS_TARGET,
	NODE_TYPEALIAS_ALIAS,
	NODE_TYPEALIAS,

	NODE_TYPE_SPECIFIER,
	NODE_TYPE_SPECIFIER_LIST,
	NODE_POINTER,
	NODE_TYPE_DECLARATOR,

	NODE_FLOATING_POINT,
	NODE_INTEGER,
	NODE_STRING,
	NODE_ENUMERATOR,
	NODE_ENUM,
	NODE_STRUCT_OR_VARIANT_DECLARATION,
	NODE_VARIANT,
	NODE_STRUCT,

	NR_NODE_TYPES,
};

struct ctf_node {
	/*
	 * Parent node is only set on demand by specific visitor.
	 */
	struct ctf_node *parent;
	struct bt_list_head siblings;
	struct bt_list_head tmp_head;
	struct bt_list_head gc;

	enum node_type type;
	union {
		struct {
		} unknown;
		struct {
			/*
			 * Children nodes are ctf_expression, typedef,
			 * typealias and type_specifier_list.
			 */
			struct bt_list_head declaration_list;
			struct bt_list_head trace;
			struct bt_list_head env;
			struct bt_list_head stream;
			struct bt_list_head event;
			struct bt_list_head clock;
		} root;
		struct {
			/*
			 * Children nodes are ctf_expression, typedef,
			 * typealias and type_specifier_list.
			 */
			struct bt_list_head declaration_list;
		} event;
		struct {
			/*
			 * Children nodes are ctf_expression, typedef,
			 * typealias and type_specifier_list.
			 */
			struct bt_list_head declaration_list;
		} stream;
		struct {
			/*
			 * Children nodes are ctf_expression, typedef,
			 * typealias and type_specifier_list.
			 */
			struct bt_list_head declaration_list;
		} env;
		struct {
			/*
			 * Children nodes are ctf_expression, typedef,
			 * typealias and type_specifier_list.
			 */
			struct bt_list_head declaration_list;
		} trace;
		struct {
			/*
			 * Children nodes are ctf_expression, typedef,
			 * typealias and type_specifier_list.
			 */
			struct bt_list_head declaration_list;
		} clock;
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
				UNARY_NESTED,
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
				struct ctf_node *nested_exp;
			} u;
			enum {
				UNARY_LINK_UNKNOWN = 0,
				UNARY_DOTLINK,
				UNARY_ARROWLINK,
				UNARY_DOTDOTDOT,
			} link;
		} unary_expression;
		struct {
			struct ctf_node *type_specifier_list;
			struct bt_list_head type_declarators;
		} _typedef;
		/* new type is "alias", existing type "target" */
		struct {
			struct ctf_node *type_specifier_list;
			struct bt_list_head type_declarators;
		} typealias_target;
		struct {
			struct ctf_node *type_specifier_list;
			struct bt_list_head type_declarators;
		} typealias_alias;
		struct {
			struct ctf_node *target;
			struct ctf_node *alias;
		} typealias;
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
		} type_specifier;
		struct {
			/* list of type_specifier */
			struct bt_list_head head;
		} type_specifier_list;
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
					struct ctf_node *type_declarator;
					/*
					 * unary expression (value) or
					 * type_specifier_list.
					 */
					struct bt_list_head length;
					/* for abstract type declarator */
					unsigned int abstract_array;
				} nested;
			} u;
			struct ctf_node *bitfield_len;
		} type_declarator;
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
			 * type_specifier_list.
			 */
			struct ctf_node *container_type;
			struct bt_list_head enumerator_list;
			int has_body;
		} _enum;
		struct {
			struct ctf_node *type_specifier_list;
			struct bt_list_head type_declarators;
		} struct_or_variant_declaration;
		struct {
			char *name;
			char *choice;
			/* list of typedef, typealias and declarations */
			struct bt_list_head declaration_list;
			int has_body;
		} variant;
		struct {
			char *name;
			/* list of typedef, typealias and declarations */
			struct bt_list_head declaration_list;
			int has_body;
			struct bt_list_head min_align;	/* align() attribute */
		} _struct;
	} u;
};

struct ctf_ast {
	struct ctf_node root;
	struct bt_list_head allocated_nodes;
};

const char *node_type(struct ctf_node *node);

struct ctf_trace;

int ctf_visitor_print_xml(FILE *fd, int depth, struct ctf_node *node);
int ctf_visitor_semantic_check(FILE *fd, int depth, struct ctf_node *node);
int ctf_visitor_parent_links(FILE *fd, int depth, struct ctf_node *node);
int ctf_visitor_construct_metadata(FILE *fd, int depth, struct ctf_node *node,
			struct ctf_trace *trace, int byte_order);

#endif /* _CTF_AST_H */
