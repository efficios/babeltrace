%{
/*
 * lexer.l
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
#include <babeltrace/babeltrace-internal.h>
#include "scanner.h"
#include "parser.h"
#include "ast.h"

#define PARSE_INTEGER_LITERAL(base)					\
	do {								\
		errno = 0;						\
		yylval->ull = strtoull(yytext, NULL, base);		\
		if (errno) {						\
			printfl_perror(yylineno, "Integer literal");	\
			return CTF_ERROR;					\
		}							\
	} while (0)

BT_HIDDEN
void setstring(struct ctf_scanner *scanner, YYSTYPE *lvalp, const char *src);

static void yyunput (int c, register char * yy_bp , yyscan_t yyscanner)
	__attribute__((unused));
static int input (yyscan_t yyscanner) __attribute__((unused));

BT_HIDDEN
int import_string(struct ctf_scanner *scanner, YYSTYPE *lvalp, const char *src, char delim);

%}

%x comment_ml comment_sl string_lit char_const
%option reentrant yylineno noyywrap bison-bridge
%option extra-type="struct ctf_scanner *"
	/* bison-locations */
INTEGER_SUFFIX			(U|UL|ULL|LU|LLU|Ul|Ull|lU|llU|u|uL|uLL|Lu|LLu|ul|ull|lu|llu)
DIGIT				[0-9]
NONDIGIT			[a-zA-Z_]
HEXDIGIT			[0-9A-Fa-f]
OCTALDIGIT			[0-7]
UCHARLOWERCASE			\\u{HEXDIGIT}{4}
UCHARUPPERCASE			\\U{HEXDIGIT}{8}
ID_NONDIGIT			{NONDIGIT}|{UCHARLOWERCASE}|{UCHARUPPERCASE}
IDENTIFIER			{ID_NONDIGIT}({ID_NONDIGIT}|{DIGIT})*
%%

				/*
				 * Using start conditions to deal with comments
				 * and strings.
				 */ 

"/*"				BEGIN(comment_ml);
<comment_ml>[^*\n]*		/* eat anything that's not a '*' */
<comment_ml>"*"+[^*/\n]*	/* eat up '*'s not followed by '/'s */
<comment_ml>\n
<comment_ml>"*"+"/"		BEGIN(INITIAL);

"//"[^\n]*\n			/* skip comment */

L?\"(\\.|[^\\"])*\"		{ if (import_string(yyextra, yylval, yytext, '\"') < 0) return CTF_ERROR; else return CTF_STRING_LITERAL; }
L?\'(\\.|[^\\'])*\'		{ if (import_string(yyextra, yylval, yytext, '\'') < 0) return CTF_ERROR; else return CTF_CHARACTER_LITERAL; }

"["				return CTF_LSBRAC;
"]"				return CTF_RSBRAC;
"("				return CTF_LPAREN;
")"				return CTF_RPAREN;
"{"				return CTF_LBRAC;
"}"				return CTF_RBRAC;
"->"				return CTF_RARROW;
"*"				return CTF_STAR;
"+"				return CTF_PLUS;
"-"				return CTF_MINUS;
"<"				return CTF_LT;
">"				return CTF_GT;
:=				return CTF_TYPEASSIGN;
:				return CTF_COLON;
;				return CTF_SEMICOLON;
"..."				return CTF_DOTDOTDOT;
"."				return CTF_DOT;
=				return CTF_EQUAL;
","				return CTF_COMMA;
align				setstring(yyextra, yylval, yytext); return CTF_TOK_ALIGN;
const				setstring(yyextra, yylval, yytext); return CTF_CONST;
char				setstring(yyextra, yylval, yytext); return CTF_CHAR;
clock				setstring(yyextra, yylval, yytext); return CTF_CLOCK;
double				setstring(yyextra, yylval, yytext); return CTF_DOUBLE;
enum				setstring(yyextra, yylval, yytext); return CTF_ENUM;
env				setstring(yyextra, yylval, yytext); return CTF_ENV;
event				setstring(yyextra, yylval, yytext); return CTF_EVENT;
floating_point			setstring(yyextra, yylval, yytext); return CTF_FLOATING_POINT;
float				setstring(yyextra, yylval, yytext); return CTF_FLOAT;
integer				setstring(yyextra, yylval, yytext); return CTF_INTEGER;
int				setstring(yyextra, yylval, yytext); return CTF_INT;
long				setstring(yyextra, yylval, yytext); return CTF_LONG;
short				setstring(yyextra, yylval, yytext); return CTF_SHORT;
signed				setstring(yyextra, yylval, yytext); return CTF_SIGNED;
stream				setstring(yyextra, yylval, yytext); return CTF_STREAM;
string				setstring(yyextra, yylval, yytext); return CTF_STRING;
struct				setstring(yyextra, yylval, yytext); return CTF_STRUCT;
trace				setstring(yyextra, yylval, yytext); return CTF_TRACE;
callsite			setstring(yyextra, yylval, yytext); return CTF_CALLSITE;
typealias			setstring(yyextra, yylval, yytext); return CTF_TYPEALIAS;
typedef				setstring(yyextra, yylval, yytext); return CTF_TYPEDEF;
unsigned			setstring(yyextra, yylval, yytext); return CTF_UNSIGNED;
variant				setstring(yyextra, yylval, yytext); return CTF_VARIANT;
void				setstring(yyextra, yylval, yytext); return CTF_VOID;
_Bool				setstring(yyextra, yylval, yytext); return CTF_BOOL;
_Complex			setstring(yyextra, yylval, yytext); return CTF_COMPLEX;
_Imaginary			setstring(yyextra, yylval, yytext); return CTF_IMAGINARY;
[1-9]{DIGIT}*{INTEGER_SUFFIX}?	PARSE_INTEGER_LITERAL(10); return CTF_INTEGER_LITERAL;
0{OCTALDIGIT}*{INTEGER_SUFFIX}?	PARSE_INTEGER_LITERAL(8); return CTF_INTEGER_LITERAL;
0[xX]{HEXDIGIT}+{INTEGER_SUFFIX}?	PARSE_INTEGER_LITERAL(16); return CTF_INTEGER_LITERAL;

{IDENTIFIER}			printf_debug("<IDENTIFIER %s>\n", yytext); setstring(yyextra, yylval, yytext); if (is_type(yyextra, yytext)) return ID_TYPE; else return IDENTIFIER;
[ \t\r\n]			; /* ignore */
.				printfl_error(yylineno, "invalid character '0x%02X'", yytext[0]);  return CTF_ERROR;
%%
