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
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <glib.h>
#include <errno.h>
#include <babeltrace/endian.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/ctf/metadata.h>
#include "ctf-scanner.h"
#include "ctf-parser.h"
#include "ctf-ast.h"

int babeltrace_verbose, babeltrace_debug;

int main(int argc, char **argv)
{
	struct ctf_scanner *scanner;
	struct ctf_trace *trace;
	int ret = 0;

	babeltrace_debug = 1;
	babeltrace_verbose = 1;
	scanner = ctf_scanner_alloc(stdin);
	if (!scanner) {
		fprintf(stderr, "Error allocating scanner\n");
		return -ENOMEM;
	}
	ret = ctf_scanner_append_ast(scanner);
	if (ret) {
		fprintf(stderr, "Error creating AST\n");
		goto end;
	}

	ret = ctf_visitor_print_xml(stderr, 0, &scanner->ast->root);
	if (ret) {
		fprintf(stderr, "Error visiting AST for XML output\n");
		goto end;
	}

	ret = ctf_visitor_semantic_check(stderr, 0, &scanner->ast->root);
	if (ret) {
		fprintf(stderr, "Error in CTF semantic validation %d\n", ret);
		goto end;
	}
	trace = malloc(sizeof(*trace));
	memset(trace, 0, sizeof(*trace));
	ret = ctf_visitor_construct_metadata(stderr, 0, &scanner->ast->root,
			trace, BYTE_ORDER);
	if (ret) {
		fprintf(stderr, "Error in CTF metadata constructor %d\n", ret);
		goto free_trace;
	}
free_trace:
	free(trace);
end:
	ctf_scanner_free(scanner);
	return ret;
} 
