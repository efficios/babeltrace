/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2011-2012 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 */

#ifndef _CTF_SCANNER_H
#define _CTF_SCANNER_H

#include <stdio.h>
#include "common/macros.h"
#include "ast.hpp"

#ifndef YY_TYPEDEF_YY_SCANNER_T
#define YY_TYPEDEF_YY_SCANNER_T
typedef void* yyscan_t;
#endif

struct ctf_scanner_scope;
struct ctf_scanner_scope {
	struct ctf_scanner_scope *parent;
	GHashTable *classes;
};

struct ctf_scanner {
	yyscan_t scanner;
	struct ctf_ast *ast;
	struct ctf_scanner_scope root_scope;
	struct ctf_scanner_scope *cs;
	struct objstack *objstack;
};

BT_HIDDEN
struct ctf_scanner *ctf_scanner_alloc(void);

BT_HIDDEN
void ctf_scanner_free(struct ctf_scanner *scanner);

BT_HIDDEN
int ctf_scanner_append_ast(struct ctf_scanner *scanner, FILE *input);

static inline
struct ctf_ast *ctf_scanner_get_ast(struct ctf_scanner *scanner)
{
	return scanner->ast;
}

BT_HIDDEN
int is_type(struct ctf_scanner *scanner, const char *id);

#endif /* _CTF_SCANNER_H */
