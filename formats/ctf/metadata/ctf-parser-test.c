#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include "ctf-scanner.h"
#include "ctf-parser.h"
#include "ctf-ast.h"

extern int yydebug;

int main(int argc, char **argv)
{
	struct ctf_scanner *scanner;

	yydebug = 1;
	scanner = ctf_scanner_alloc(stdin);
	if (!scanner) {
		fprintf(stderr, "Error allocating scanner\n");
		return -1;
	}
	ctf_scanner_append_ast(scanner);
	ctf_scanner_free(scanner);
	return 0;
} 


