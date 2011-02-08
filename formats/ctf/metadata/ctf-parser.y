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
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <helpers/list.h>
#include <glib.h>
#include "ctf-scanner.h"
#include "ctf-parser.h"
#include "ctf-ast.h"

#define printf_dbg(fmt, args...)	fprintf(stderr, "%s: " fmt, __func__, args)
#define printf_dbg_noarg(fmt)	fprintf(stderr, "%s: " fmt, __func__)

int yyparse(struct ctf_scanner *scanner);
int yylex(union YYSTYPE *yyval, struct ctf_scanner *scanner);
int yylex_init_extra(struct ctf_scanner *scanner, yyscan_t * ptr_yy_globals);
int yylex_destroy(yyscan_t yyscanner) ;
void yyset_in(FILE * in_str, yyscan_t scanner);

int yydebug;

struct gc_string {
	struct cds_list_head gc;
	char s[];
};

char *strredup(char **dest, const char *src)
{
	size_t len = strlen(src) + 1;

	*dest = realloc(*dest, len);
	if (!*dest)
		return NULL;
	strcpy(*dest, src);
	return *dest;
}

static struct gc_string *gc_string_alloc(struct ctf_scanner *scanner, size_t len)
{
	struct gc_string *gstr;

	gstr = malloc(sizeof(*gstr) + len);
	cds_list_add(&gstr->gc, &scanner->allocated_strings);
	return gstr;
}

void setstring(struct ctf_scanner *scanner, YYSTYPE *lvalp, const char *src)
{
	lvalp->gs = gc_string_alloc(scanner, strlen(src) + 1);
	strcpy(lvalp->gs->s, src);
}

static void init_scope(struct scope *scope, struct scope *parent)
{
	scope->parent = parent;
	scope->types = g_hash_table_new_full(g_str_hash, g_str_equal,
					     (GDestroyNotify) free, NULL);
}

static void finalize_scope(struct scope *scope)
{
	g_hash_table_destroy(scope->types);
}

static void push_scope(struct ctf_scanner *scanner)
{
	struct scope *ns;

	printf_dbg_noarg("push scope\n");
	ns = malloc(sizeof(struct scope));
	init_scope(ns, scanner->cs);
	scanner->cs = ns;
}

static void pop_scope(struct ctf_scanner *scanner)
{
	struct scope *os;

	printf_dbg_noarg("pop scope\n");
	os = scanner->cs;
	scanner->cs = os->parent;
	finalize_scope(os);
	free(os);
}

static int lookup_type(struct scope *s, const char *id)
{
	int ret;

	ret = (int) g_hash_table_lookup(s->types, id);
	printf_dbg("lookup %p %s %d\n", s, id, ret);
	return ret;
}

int is_type(struct ctf_scanner *scanner, const char *id)
{
	struct scope *it;
	int ret = 0;

	for (it = scanner->cs; it != NULL; it = it->parent) {
		if (lookup_type(it, id)) {
			ret = 1;
			break;
		}
	}
	printf_dbg("is type %s %d\n", id, ret);
	return ret;
}

static void add_type(struct ctf_scanner *scanner, const char *id)
{
	char *type_id = NULL;

	printf_dbg("add type %s\n", id);
	if (lookup_type(scanner->cs, id))
		return;
	strredup(&type_id, id);
	g_hash_table_insert(scanner->cs->types, type_id, type_id);
}

void yyerror(struct ctf_scanner *scanner, const char *str)
{
	fprintf(stderr, "error %s\n", str);
}
 
int yywrap(void)
{
	return 1;
} 

static void free_strings(struct cds_list_head *list)
{
	struct gc_string *gstr, *tmp;

	cds_list_for_each_entry_safe(gstr, tmp, list, gc)
		free(gstr);
}

static struct ctf_ast *ctf_ast_alloc(void)
{
	struct ctf_ast *ast;

	ast = malloc(sizeof(*ast));
	if (!ast)
		return NULL;
	memset(ast, 0, sizeof(*ast));
	return ast;
}

static void ctf_ast_free(struct ctf_ast *ast)
{
}

int ctf_scanner_append_ast(struct ctf_scanner *scanner)
{
	return yyparse(scanner);
}

struct ctf_scanner *ctf_scanner_alloc(FILE *input)
{
	struct ctf_scanner *scanner;
	int ret;

	scanner = malloc(sizeof(*scanner));
	if (!scanner)
		return NULL;
	memset(scanner, 0, sizeof(*scanner));

	ret = yylex_init_extra(scanner, &scanner->scanner);
	if (ret) {
		fprintf(stderr, "yylex_init error\n");
		goto cleanup_scanner;
	}
	yyset_in(input, scanner);

	scanner->ast = ctf_ast_alloc();
	if (!scanner->ast)
		goto cleanup_lexer;
	init_scope(&scanner->root_scope, NULL);
	CDS_INIT_LIST_HEAD(&scanner->allocated_strings);

	return scanner;

cleanup_lexer:
	ret = yylex_destroy(scanner->scanner);
	if (!ret)
		fprintf(stderr, "yylex_destroy error\n");
cleanup_scanner:
	free(scanner);
	return NULL;
}

void ctf_scanner_free(struct ctf_scanner *scanner)
{
	int ret;

	finalize_scope(&scanner->root_scope);
	free_strings(&scanner->allocated_strings);
	ctf_ast_free(scanner->ast);
	ret = yylex_destroy(scanner->scanner);
	if (ret)
		fprintf(stderr, "yylex_destroy error\n");
	free(scanner);
}

%}

%define api.pure
	/* %locations */
%parse-param {struct ctf_scanner *scanner}
%lex-param {struct ctf_scanner *scanner}
%start file
%token CHARACTER_CONSTANT_START SQUOTE STRING_LITERAL_START DQUOTE ESCSEQ CHAR_STRING_TOKEN LSBRAC RSBRAC LPAREN RPAREN LBRAC RBRAC RARROW STAR PLUS MINUS LT GT TYPEASSIGN COLON SEMICOLON DOTDOTDOT DOT EQUAL COMMA CONST CHAR DOUBLE ENUM EVENT FLOATING_POINT FLOAT INTEGER INT LONG SHORT SIGNED STREAM STRING STRUCT TRACE TYPEALIAS TYPEDEF UNSIGNED VARIANT VOID _BOOL _COMPLEX _IMAGINARY DECIMAL_CONSTANT OCTAL_CONSTANT HEXADECIMAL_CONSTANT
%token <gs> IDENTIFIER ID_TYPE
%token ERROR
%union
{
	long long ll;
	char c;
	struct gc_string *gs;
	struct ctf_node *n;
}

%%

file:
		declaration
	|	file declaration
	;

keywords:
		VOID
	|	CHAR
	|	SHORT
	|	INT
	|	LONG
	|	FLOAT
	|	DOUBLE
	|	SIGNED
	|	UNSIGNED
	|	_BOOL
	|	_COMPLEX
	|	FLOATING_POINT
	|	INTEGER
	|	STRING
	|	ENUM
	|	VARIANT
	|	STRUCT
	|	CONST
	|	TYPEDEF
	|	EVENT
	|	STREAM
	|	TRACE
	;

/* 1.5 Constants */

c_char_sequence:
		c_char
	|	c_char_sequence c_char
	;

c_char:
		CHAR_STRING_TOKEN
	|	ESCSEQ
	;

/* 1.6 String literals */

s_char_sequence:
		s_char
	|	s_char_sequence s_char
	;

s_char:
		CHAR_STRING_TOKEN
	|	ESCSEQ
	;

/* 2: Phrase structure grammar */

postfix_expression:
		IDENTIFIER
	|	ID_TYPE
	|	keywords
	|	DECIMAL_CONSTANT
	|	OCTAL_CONSTANT
	|	HEXADECIMAL_CONSTANT
	|	STRING_LITERAL_START DQUOTE
	|	STRING_LITERAL_START s_char_sequence DQUOTE
	|	CHARACTER_CONSTANT_START c_char_sequence SQUOTE
	|	LPAREN unary_expression RPAREN
	|	postfix_expression LSBRAC unary_expression RSBRAC
	|	postfix_expression DOT IDENTIFIER
	|	postfix_expression DOT ID_TYPE
	|	postfix_expression RARROW IDENTIFIER
	|	postfix_expression RARROW ID_TYPE
	;

unary_expression:
		postfix_expression
	|	PLUS postfix_expression
	|	MINUS postfix_expression
	;

unary_expression_or_range:
		unary_expression DOTDOTDOT unary_expression
	|	unary_expression
	;

/* 2.2: Declarations */

declaration:
		declaration_specifiers SEMICOLON
	|	event_declaration
	|	stream_declaration
	|	trace_declaration
	|	declaration_specifiers TYPEDEF declaration_specifiers type_declarator_list SEMICOLON
	|	TYPEDEF declaration_specifiers type_declarator_list SEMICOLON
	|	declaration_specifiers TYPEDEF type_declarator_list SEMICOLON
	|	TYPEALIAS declaration_specifiers abstract_declarator_list COLON declaration_specifiers abstract_type_declarator_list SEMICOLON
	|	TYPEALIAS declaration_specifiers abstract_declarator_list COLON type_declarator_list SEMICOLON
	;

event_declaration:
		event_declaration_begin event_declaration_end
	|	event_declaration_begin ctf_assignment_expression_list event_declaration_end
	;

event_declaration_begin:
		EVENT LBRAC
		{
			push_scope(scanner);
		}
	;

event_declaration_end:
		RBRAC SEMICOLON
		{
			pop_scope(scanner);
		}
	;


stream_declaration:
		stream_declaration_begin stream_declaration_end
	|	stream_declaration_begin ctf_assignment_expression_list stream_declaration_end
	;

stream_declaration_begin:
		STREAM LBRAC
		{
			push_scope(scanner);
		}
	;

stream_declaration_end:
		RBRAC SEMICOLON
		{
			pop_scope(scanner);
		}
	;


trace_declaration:
		trace_declaration_begin trace_declaration_end
	|	trace_declaration_begin ctf_assignment_expression_list trace_declaration_end
	;

trace_declaration_begin:
		TRACE LBRAC
		{
			push_scope(scanner);
		}
	;

trace_declaration_end:
		RBRAC SEMICOLON
		{
			pop_scope(scanner);
		}
	;

declaration_specifiers:
		CONST
	|	type_specifier
	|	declaration_specifiers CONST
	|	declaration_specifiers type_specifier
	;

type_declarator_list:
		type_declarator
	|	type_declarator_list COMMA type_declarator
	;

abstract_type_declarator_list:
		abstract_type_declarator
	|	abstract_type_declarator_list COMMA abstract_type_declarator
	;

type_specifier:
		VOID
	|	CHAR
	|	SHORT
	|	INT
	|	LONG
	|	FLOAT
	|	DOUBLE
	|	SIGNED
	|	UNSIGNED
	|	_BOOL
	|	_COMPLEX
	|	ID_TYPE
	|	FLOATING_POINT LBRAC RBRAC
	|	FLOATING_POINT LBRAC ctf_assignment_expression_list RBRAC
	|	INTEGER LBRAC RBRAC
	|	INTEGER LBRAC ctf_assignment_expression_list RBRAC
	|	STRING LBRAC RBRAC
	|	STRING LBRAC ctf_assignment_expression_list RBRAC
	|	ENUM enum_type_specifier
	|	VARIANT variant_type_specifier
	|	STRUCT struct_type_specifier
	;

struct_type_specifier:
		struct_declaration_begin struct_or_variant_declaration_list struct_declaration_end
	|	IDENTIFIER struct_declaration_begin struct_or_variant_declaration_list struct_declaration_end
	|	ID_TYPE struct_declaration_begin struct_or_variant_declaration_list struct_declaration_end
	|	IDENTIFIER
	|	ID_TYPE
	;

struct_declaration_begin:
		LBRAC
		{
			push_scope(scanner);
		}
	;

struct_declaration_end:
		RBRAC
		{
			pop_scope(scanner);
		}
	;

variant_type_specifier:
		variant_declaration_begin struct_or_variant_declaration_list variant_declaration_end
	|	LT IDENTIFIER GT variant_declaration_begin struct_or_variant_declaration_list variant_declaration_end
	|	LT ID_TYPE GT variant_declaration_begin struct_or_variant_declaration_list variant_declaration_end
	|	IDENTIFIER variant_declaration_begin struct_or_variant_declaration_list variant_declaration_end
	|	IDENTIFIER LT IDENTIFIER GT variant_declaration_begin struct_or_variant_declaration_list variant_declaration_end
	|	IDENTIFIER LT IDENTIFIER GT
	|	IDENTIFIER LT ID_TYPE GT variant_declaration_begin struct_or_variant_declaration_list variant_declaration_end
	|	IDENTIFIER LT ID_TYPE GT
	|	ID_TYPE variant_declaration_begin struct_or_variant_declaration_list variant_declaration_end
	|	ID_TYPE LT IDENTIFIER GT variant_declaration_begin struct_or_variant_declaration_list variant_declaration_end
	|	ID_TYPE LT IDENTIFIER GT
	|	ID_TYPE LT ID_TYPE GT variant_declaration_begin struct_or_variant_declaration_list variant_declaration_end
	|	ID_TYPE LT ID_TYPE GT
	;

variant_declaration_begin:
		LBRAC
		{
			push_scope(scanner);
		}
	;

variant_declaration_end:
		RBRAC
		{
			pop_scope(scanner);
		}
	;

type_specifier_or_integer_constant:
		declaration_specifiers
	|	DECIMAL_CONSTANT
	|	OCTAL_CONSTANT
	|	HEXADECIMAL_CONSTANT
	;

enum_type_specifier:
		LBRAC enumerator_list RBRAC
	|	LT type_specifier_or_integer_constant GT LBRAC enumerator_list RBRAC
	|	IDENTIFIER LBRAC enumerator_list RBRAC
	|	IDENTIFIER LT type_specifier_or_integer_constant GT LBRAC enumerator_list RBRAC
	|	ID_TYPE LBRAC enumerator_list RBRAC
	|	ID_TYPE LT type_specifier_or_integer_constant GT LBRAC enumerator_list RBRAC
	|	LBRAC enumerator_list COMMA RBRAC
	|	LT type_specifier_or_integer_constant GT LBRAC enumerator_list COMMA RBRAC
	|	IDENTIFIER LBRAC enumerator_list COMMA RBRAC
	|	IDENTIFIER LT type_specifier_or_integer_constant GT LBRAC enumerator_list COMMA RBRAC
	|	IDENTIFIER
	|	IDENTIFIER LT type_specifier_or_integer_constant GT
	|	ID_TYPE LBRAC enumerator_list COMMA RBRAC
	|	ID_TYPE LT type_specifier_or_integer_constant GT LBRAC enumerator_list COMMA RBRAC
	|	ID_TYPE
	|	ID_TYPE LT type_specifier_or_integer_constant GT
	;

struct_or_variant_declaration_list:
		/* empty */
	|	struct_or_variant_declaration_list struct_or_variant_declaration
	;

struct_or_variant_declaration:
		specifier_qualifier_list struct_or_variant_declarator_list SEMICOLON
	|	specifier_qualifier_list TYPEDEF specifier_qualifier_list type_declarator_list SEMICOLON
	|	TYPEDEF specifier_qualifier_list type_declarator_list SEMICOLON
	|	specifier_qualifier_list TYPEDEF type_declarator_list SEMICOLON
	|	TYPEALIAS specifier_qualifier_list abstract_declarator_list COLON specifier_qualifier_list abstract_type_declarator_list SEMICOLON
	|	TYPEALIAS specifier_qualifier_list abstract_declarator_list COLON type_declarator_list SEMICOLON
	;

specifier_qualifier_list:
		CONST
	|	type_specifier
	|	specifier_qualifier_list CONST
	|	specifier_qualifier_list type_specifier
	;

struct_or_variant_declarator_list:
		struct_or_variant_declarator
	|	struct_or_variant_declarator_list COMMA struct_or_variant_declarator
	;

struct_or_variant_declarator:
		declarator
	|	COLON unary_expression
	|	declarator COLON unary_expression
	;

enumerator_list:
		enumerator
	|	enumerator_list COMMA enumerator
	;

enumerator:
		IDENTIFIER
	|	ID_TYPE
	|	keywords
	|	STRING_LITERAL_START DQUOTE
	|	STRING_LITERAL_START s_char_sequence DQUOTE
	|	IDENTIFIER EQUAL unary_expression_or_range
	|	ID_TYPE EQUAL unary_expression_or_range
	|	keywords EQUAL unary_expression_or_range
	|	STRING_LITERAL_START DQUOTE EQUAL unary_expression_or_range
	|	STRING_LITERAL_START s_char_sequence DQUOTE EQUAL unary_expression_or_range
	;

abstract_declarator_list:
		abstract_declarator
	|	abstract_declarator_list COMMA abstract_declarator
	;

abstract_declarator:
		direct_abstract_declarator
	|	pointer direct_abstract_declarator
	;

direct_abstract_declarator:
		/* empty */
	|	IDENTIFIER
	|	LPAREN abstract_declarator RPAREN
	|	direct_abstract_declarator LSBRAC type_specifier_or_integer_constant RSBRAC
	|	direct_abstract_declarator LSBRAC RSBRAC
	;

declarator:
		direct_declarator
	|	pointer direct_declarator
	;

direct_declarator:
		IDENTIFIER
	|	LPAREN declarator RPAREN
	|	direct_declarator LSBRAC type_specifier_or_integer_constant RSBRAC
	;

type_declarator:
		direct_type_declarator
	|	pointer direct_type_declarator
	;

direct_type_declarator:
		IDENTIFIER
		{
			add_type(scanner, $1->s);
		}
	|	LPAREN type_declarator RPAREN
	|	direct_type_declarator LSBRAC type_specifier_or_integer_constant RSBRAC
	;

abstract_type_declarator:
		direct_abstract_type_declarator
	|	pointer direct_abstract_type_declarator
	;

direct_abstract_type_declarator:
		/* empty */
	|	IDENTIFIER
		{
			add_type(scanner, $1->s);
		}
	|	LPAREN abstract_type_declarator RPAREN
	|	direct_abstract_type_declarator LSBRAC type_specifier_or_integer_constant RSBRAC
	|	direct_abstract_type_declarator LSBRAC RSBRAC
	;

pointer:	
		STAR
	|	STAR pointer
	|	STAR type_qualifier_list pointer
	;

type_qualifier_list:
		CONST
	|	type_qualifier_list CONST
	;

/* 2.3: CTF-specific declarations */

ctf_assignment_expression_list:
		ctf_assignment_expression SEMICOLON
	|	ctf_assignment_expression_list ctf_assignment_expression SEMICOLON
	;

ctf_assignment_expression:
		unary_expression EQUAL unary_expression
	|	unary_expression TYPEASSIGN type_specifier
	|	declaration_specifiers TYPEDEF declaration_specifiers type_declarator_list
	|	TYPEDEF declaration_specifiers type_declarator_list
	|	declaration_specifiers TYPEDEF type_declarator_list
	|	TYPEALIAS declaration_specifiers abstract_declarator_list COLON declaration_specifiers abstract_type_declarator_list
	|	TYPEALIAS declaration_specifiers abstract_declarator_list COLON type_declarator_list
	;
