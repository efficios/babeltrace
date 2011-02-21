/*
 * ctf-parser-test.c
 *
 * Common Trace Format Parser Test
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
	int ret = 0;

	yydebug = 1;
	scanner = ctf_scanner_alloc(stdin);
	if (!scanner) {
		fprintf(stderr, "Error allocating scanner\n");
		return -1;
	}
	ctf_scanner_append_ast(scanner);

	if (ctf_visitor_print_xml(stdout, 0, &scanner->ast->root)) {
		fprintf(stderr, "error visiting AST for XML output\n");
		ret = -1;
	}
	ctf_scanner_free(scanner);

	return ret;
} 


