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

int ctf_text_integer_write(struct stream_pos *ppos, struct definition *definition)
{
	struct definition_integer *integer_definition =
		container_of(definition, struct definition_integer, p);
	const struct declaration_integer *integer_declaration =
		integer_definition->declaration;
	struct ctf_text_stream_pos *pos = ctf_text_pos(ppos);

	if (pos->dummy)
		return 0;

	if (definition->index != 0 && definition->index != INT_MAX)
		fprintf(pos->fp, ",");
	if (definition->index != INT_MAX)
		fprintf(pos->fp, " ");
	if (pos->print_names)
		fprintf(pos->fp, "%s = ",
			g_quark_to_string(definition->name));

	//print_pos_tabs(pos);

	if (!compare_definition_path(definition, g_quark_from_static_string("stream.event.header.timestamp"))) {
		if (!pos->print_names)
			fprintf(pos->fp, "[%" PRIu64 "]",
				integer_definition->value._unsigned);
		else
			fprintf(pos->fp, "%" PRIu64,
				integer_definition->value._unsigned);
		return 0;
	}

	if (!integer_declaration->signedness) {
		fprintf(pos->fp, "%" PRIu64" (0x%" PRIX64 ")",
			integer_definition->value._unsigned,
			integer_definition->value._unsigned);
	} else {
		fprintf(pos->fp, "%" PRId64" (0x%" PRIX64 ")",
			integer_definition->value._signed,
			integer_definition->value._signed);
	}
	return 0;
}
