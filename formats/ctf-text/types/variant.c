/*
 * Common Trace Format
 *
 * Variant format access functions.
 *
 * Copyright 2011 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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

int ctf_text_variant_write(struct stream_pos *ppos, struct definition *definition)
{
	struct ctf_text_stream_pos *pos = ctf_text_pos(ppos);
	int ret;

	if (!pos->dummy) {
		if (pos->depth >= 0) {
			//print_pos_tabs(pos);
			if (definition->index != 0 && definition->index != INT_MAX)
				fprintf(pos->fp, ",");
			if (definition->index != INT_MAX)
				fprintf(pos->fp, " ");
			if (pos->print_names)
				fprintf(pos->fp, "%s = ",
					g_quark_to_string(definition->name));
			fprintf(pos->fp, "{");
		}
		pos->depth++;
	}
	ret = variant_rw(ppos, definition);
	if (!pos->dummy) {
		pos->depth--;
		if (pos->depth >= 0) {
			//print_pos_tabs(pos);
			fprintf(pos->fp, " }");
		}
	}
	return ret;
}
