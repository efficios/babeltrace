#ifndef _CTF_SCANNER_H
#define _CTF_SCANNER_H

/*
 * ctf-scanner.h
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

#include <stdio.h>
#include "ctf-ast.h"

#ifndef YY_TYPEDEF_YY_SCANNER_T
#define YY_TYPEDEF_YY_SCANNER_T
typedef void* yyscan_t;
#endif

struct ctf_scanner_scope;
struct ctf_scanner_scope {
	struct ctf_scanner_scope *parent;
	GHashTable *types;
};

struct ctf_scanner {
	yyscan_t scanner;
	struct ctf_ast *ast;
	struct ctf_scanner_scope root_scope;
	struct ctf_scanner_scope *cs;
	struct bt_list_head allocated_strings;
};

struct ctf_scanner *ctf_scanner_alloc(FILE *input);
void ctf_scanner_free(struct ctf_scanner *scanner);
int ctf_scanner_append_ast(struct ctf_scanner *scanner);

static inline
struct ctf_ast *ctf_scanner_get_ast(struct ctf_scanner *scanner)
{
	return scanner->ast;
}

BT_HIDDEN
int is_type(struct ctf_scanner *scanner, const char *id);

#endif /* _CTF_SCANNER_H */
