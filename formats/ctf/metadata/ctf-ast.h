#ifndef _CTF_PARSER_H
#define _CTF_PARSER_H

#include <stdint.h>
#include <helpers/list.h>
#include <stdio.h>
#include <glib.h>

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
	NODE_TRACE,

	NODE_CTF_EXPRESSION,

	NODE_TYPEDEF,
	NODE_TYPEALIAS_TARGET,
	NODE_TYPEALIAS_ALIAS,
	NODE_TYPEALIAS,

	NODE_TYPE_SPECIFIER,
	NODE_DECLARATION_SPECIFIER,
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
	struct ctf_node *parent;
	struct cds_list_head siblings;
	struct cds_list_head gc;

	enum node_type type;
	union {
		struct {
		} unknown;
		struct {
			struct cds_list_head _typedef;
			struct cds_list_head typealias;
			struct cds_list_head declaration_specifier;
			struct cds_list_head trace;
			struct cds_list_head stream;
			struct cds_list_head event;
		} root;
		struct {
			/*
			 * Children nodes are ctf_expression, typedef or
			 * typealias.
			 */
			struct cds_list_head _typedef;
			struct cds_list_head typealias;
			struct cds_list_head ctf_expression;
			struct cds_list_head declaration_specifier;
		} event;
		struct {
			/*
			 * Children nodes are ctf_expression, typedef or
			 * typealias.
			 */
			struct cds_list_head _typedef;
			struct cds_list_head typealias;
			struct cds_list_head ctf_expression;
			struct cds_list_head declaration_specifier;
		} stream;
		struct {
			/*
			 * Children nodes are ctf_expression, typedef or
			 * typealias.
			 */
			struct cds_list_head _typedef;
			struct cds_list_head typealias;
			struct cds_list_head ctf_expression;
			struct cds_list_head declaration_specifier;
		} trace;
		struct {
			char *left_id;
			enum {
				EXP_UNKNOWN = 0,
				EXP_ID,
				EXP_TYPE,
			} type;
			union {
				char *id;
				struct ctf_node *type;
			} right;
		} ctf_expression;
		struct {
			struct ctf_node *declaration_specifier;
			struct cds_list_head type_declarators;
		} _typedef;
		/* new type is "alias", existing type "target" */
		struct {
			struct ctf_node *declaration_specifier;
			struct cds_list_head type_declarators;
		} typealias_target;
		struct {
			struct ctf_node *declaration_specifier;
			struct cds_list_head type_declarators;
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
				TYPESPEC_ID_TYPE,

				TYPESPEC_FLOATING_POINT,
				TYPESPEC_INTEGER,
				TYPESPEC_STRING,
				TYPESPEC_ENUM,
				TYPESPEC_VARIANT,
				TYPESPEC_STRUCT,
			} type;
			union {
				struct ctf_node *floating_point;
				struct ctf_node *integer;
				struct ctf_node *string;
				struct ctf_node *_enum;
				struct ctf_node *variant;
				struct ctf_node *_struct;
			} u;
		} type_specifier;
		struct {
			/* drop "const" specifier */
			/* Children nodes are type_specifiers */
			struct cds_list_head type_specifiers;
		} declaration_specifier;
		struct {
		} pointer;
		struct {
			struct cds_list_head pointers;
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
					struct {
						enum {
							TYPEDEC_TYPE_UNKNOWN = 0,
							TYPEDEC_TYPE_VALUE, /* must be > 0 */
							TYPEDEC_TYPE_TYPE,
						} type;
						union {
							uint64_t value;
							struct ctf_node *declaration_specifier;
						} u;
					} length;
				} nested;
			} u;
		} type_declarator;
		struct {
			/* Children nodes are ctf_expression. */
			struct cds_list_head expressions;
		} floating_point;
		struct {
			/* Children nodes are ctf_expression. */
			struct cds_list_head expressions;
		} integer;
		struct {
			/* Children nodes are ctf_expression. */
			struct cds_list_head expressions;
		} string;
		struct {
			char *id;
			union {		/* inclusive start/end of range */
				struct {
					int64_t start, end;
				} _signed;
				struct {
					uint64_t start, end;
				} _unsigned;
			} u;
		} enumerator;
		struct {
			char *enum_id;
			struct {
				enum {
					ENUM_TYPE_UNKNOWN = 0,
					ENUM_TYPE_VALUE, /* must be > 0 */
					ENUM_TYPE_TYPE,
				} type;
				union {
					uint64_t value;
					struct ctf_node *declaration_specifier;
				} u;
			} container_type;
			struct cds_list_head enumerator_list;
		} _enum;
		struct {
			struct ctf_node *declaration_specifier;
			struct cds_list_head type_declarators;
		} struct_or_variant_declaration;
		struct {
			struct cds_list_head _typedef;
			struct cds_list_head typealias;
			struct cds_list_head declaration_list;
		} variant;
		struct {
			struct cds_list_head _typedef;
			struct cds_list_head typealias;
			struct cds_list_head declaration_list;
		} _struct;
	} u;
};

struct ctf_ast {
	struct ctf_node root;
	struct cds_list_head allocated_nodes;
};

#endif /* _CTF_PARSER_H */
