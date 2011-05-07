/*
 * Common Trace Format
 *
 * Integers read/write functions.
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

#include <babeltrace/ctf-text/types.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdint.h>

void ctf_text_integer_write(struct stream_pos *ppos, struct definition *definition)
{
	struct definition_integer *integer_definition =
		container_of(definition, struct definition_integer, p);
	const struct declaration_integer *integer_declaration =
		integer_definition->declaration;
	struct ctf_text_stream_pos *pos = ctf_text_pos(ppos);

	if (pos->dummy)
		return;
	print_pos_tabs(pos);
	if (!integer_declaration->signedness) {
		fprintf(pos->fp, "%" PRIu64" (%" PRIX64 ")\n",
			integer_definition->value._unsigned,
			integer_definition->value._unsigned);
	} else {
		fprintf(pos->fp, "%" PRId64" (%" PRIX64 ")\n",
			integer_definition->value._signed,
			integer_definition->value._signed);
	}
}
