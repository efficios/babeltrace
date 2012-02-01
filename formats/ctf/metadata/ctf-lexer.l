%{
/*
 * ctf-lexer.l
 *
 * Common Trace Formal Lexer
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
#include <babeltrace/babeltrace-internal.h>
#include "ctf-scanner.h"
#include "ctf-parser.h"
#include "ctf-ast.h"

extern
void setstring(struct ctf_scanner *scanner, YYSTYPE *lvalp, const char *src);

static void yyunput (int c, register char * yy_bp , yyscan_t yyscanner)
	__attribute__((unused));
static int input (yyscan_t yyscanner) __attribute__((unused));

%}

%x comment_ml comment_sl string_lit char_const
%option reentrant yylineno noyywrap bison-bridge
%option extra-type="struct ctf_scanner *"
	/* bison-locations */
INTEGER_SUFFIX			[ \n\t]*(U|UL|ULL|LU|LLU|Ul|Ull|lU|llU|u|uL|uLL|Lu|LLu|ul|ull|lu|llu)
DIGIT				[0-9]
NONDIGIT			[a-zA-Z_]
HEXDIGIT			[0-9A-Fa-f]
OCTALDIGIT			[0-7]
UCHARLOWERCASE			\\u{HEXDIGIT}{4}
UCHARUPPERCASE			\\U{HEXDIGIT}{8}
ID_NONDIGIT			{NONDIGIT}|{UCHARLOWERCASE}|{UCHARUPPERCASE}
IDENTIFIER			{ID_NONDIGIT}({ID_NONDIGIT}|{DIGIT})*
ESCSEQ				\\(\'|\"|\?|\\|a|b|f|n|r|t|v|{OCTALDIGIT}{1,3}|u{HEXDIGIT}{4}|U{HEXDIGIT}{8}|x{HEXDIGIT}+)
%%

				/*
				 * Using start conditions to deal with comments
				 * and strings.
				 */ 

"/*"				BEGIN(comment_ml);
<comment_ml>[^*\n]*		/* eat anything that's not a '*' */
<comment_ml>"*"+[^*/\n]*	/* eat up '*'s not followed by '/'s */
<comment_ml>\n			++yylineno;
<comment_ml>"*"+"/"		BEGIN(INITIAL);

"//"				BEGIN(comment_sl);
<comment_sl>[^\n]*\n		++yylineno; BEGIN(INITIAL);

L\'				BEGIN(char_const); return CHARACTER_CONSTANT_START;
\'				BEGIN(char_const); return CHARACTER_CONSTANT_START;
<char_const>\'			BEGIN(INITIAL); return SQUOTE;

L\"				BEGIN(string_lit); return STRING_LITERAL_START;
\"				BEGIN(string_lit); return STRING_LITERAL_START;
<string_lit>\"			BEGIN(INITIAL); return DQUOTE;

<char_const,string_lit>ESCSEQ	return ESCSEQ;
<char_const,string_lit>\n	; /* ignore */
<char_const,string_lit>.	setstring(yyextra, yylval, yytext); return CHAR_STRING_TOKEN;

"["				return LSBRAC;
"]"				return RSBRAC;
"("				return LPAREN;
")"				return RPAREN;
"{"				return LBRAC;
"}"				return RBRAC;
"->"				return RARROW;
"*"				return STAR;
"+"				return PLUS;
"-"				return MINUS;
"<"				return LT;
">"				return GT;
:=				return TYPEASSIGN;
:				return COLON;
;				return SEMICOLON;
"..."				return DOTDOTDOT;
"."				return DOT;
=				return EQUAL;
","				return COMMA;
align				setstring(yyextra, yylval, yytext); return TOK_ALIGN;
const				setstring(yyextra, yylval, yytext); return CONST;
char				setstring(yyextra, yylval, yytext); return CHAR;
clock				setstring(yyextra, yylval, yytext); return CLOCK;
double				setstring(yyextra, yylval, yytext); return DOUBLE;
enum				setstring(yyextra, yylval, yytext); return ENUM;
env				setstring(yyextra, yylval, yytext); return ENV;
event				setstring(yyextra, yylval, yytext); return EVENT;
floating_point			setstring(yyextra, yylval, yytext); return FLOATING_POINT;
float				setstring(yyextra, yylval, yytext); return FLOAT;
integer				setstring(yyextra, yylval, yytext); return INTEGER;
int				setstring(yyextra, yylval, yytext); return INT;
long				setstring(yyextra, yylval, yytext); return LONG;
short				setstring(yyextra, yylval, yytext); return SHORT;
signed				setstring(yyextra, yylval, yytext); return SIGNED;
stream				setstring(yyextra, yylval, yytext); return STREAM;
string				setstring(yyextra, yylval, yytext); return STRING;
struct				setstring(yyextra, yylval, yytext); return STRUCT;
trace				setstring(yyextra, yylval, yytext); return TRACE;
typealias			setstring(yyextra, yylval, yytext); return TYPEALIAS;
typedef				setstring(yyextra, yylval, yytext); return TYPEDEF;
unsigned			setstring(yyextra, yylval, yytext); return UNSIGNED;
variant				setstring(yyextra, yylval, yytext); return VARIANT;
void				setstring(yyextra, yylval, yytext); return VOID;
_Bool				setstring(yyextra, yylval, yytext); return _BOOL;
_Complex			setstring(yyextra, yylval, yytext); return _COMPLEX;
_Imaginary			setstring(yyextra, yylval, yytext); return _IMAGINARY;
[1-9]{DIGIT}*{INTEGER_SUFFIX}?	setstring(yyextra, yylval, yytext); return DECIMAL_CONSTANT;
0{OCTALDIGIT}*{INTEGER_SUFFIX}?	setstring(yyextra, yylval, yytext); return OCTAL_CONSTANT;
0[xX]{HEXDIGIT}+{INTEGER_SUFFIX}?	setstring(yyextra, yylval, yytext); return HEXADECIMAL_CONSTANT;
{IDENTIFIER}			printf_debug("<IDENTIFIER %s>\n", yytext); setstring(yyextra, yylval, yytext); if (is_type(yyextra, yytext)) return ID_TYPE; else return IDENTIFIER;
[ \t\n]+			; /* ignore */
.				return ERROR;
%%
