/*
 * Common Trace Format
 *
 * Enumeration mapping strings (quarks) from/to integers.
 *
 * Copyright 2010, 2011 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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
#include <stdint.h>

int ctf_text_enum_write(struct stream_pos *ppos, struct definition *definition)
{
	struct definition_enum *enum_definition =
		container_of(definition, struct definition_enum, p);
	struct definition_integer *integer_definition =
		enum_definition->integer;
	struct ctf_text_stream_pos *pos = ctf_text_pos(ppos);
	GArray *qs;
	int i, ret;

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
	fprintf(pos->fp, "(");
	pos->depth++;
	ret = generic_rw(ppos, &integer_definition->p);
	//print_pos_tabs(pos);
	fprintf(pos->fp, " :");

	qs = enum_definition->value;
	assert(qs);

	for (i = 0; i < qs->len; i++) {
		GQuark q = g_array_index(qs, GQuark, i);
		const char *str = g_quark_to_string(q);

		if (i != 0)
			fprintf(pos->fp, ",");
		fprintf(pos->fp, " ");
		fprintf(pos->fp, "%s\n", str);
	}
	pos->depth--;
	//print_pos_tabs(pos);
	fprintf(pos->fp, " )");
	return ret;
}
