#ifndef _CTF_SCANNER_H
#define _CTF_SCANNER_H

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

int is_type(struct ctf_scanner *scanner, const char *id);

#endif /* _CTF_SCANNER_H */
