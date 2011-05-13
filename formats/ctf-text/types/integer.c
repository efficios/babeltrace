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

	if (pos->field_nr++ != 0)
		fprintf(pos->fp, ",");
	fprintf(pos->fp, " ");
	if (pos->print_names)
		fprintf(pos->fp, "%s = ",
			g_quark_to_string(definition->name));

	switch (integer_declaration->base) {
	case 2:
	{
		int bitnr;
		uint64_t v = integer_definition->value._unsigned;

		fprintf(pos->fp, "b");
		for (bitnr = 0; bitnr < integer_declaration->len; bitnr++)
			v <<= 1;
		for (; bitnr < sizeof(v) * CHAR_BIT; bitnr++) {
			fprintf(pos->fp, "%u", ((v & 1ULL) << 63) ? 1 : 0);
			v <<= 1;
		}
		break;
	}
	case 8:
		fprintf(pos->fp, "0%" PRIo64,
			integer_definition->value._unsigned);
		break;
	case 10:
		if (!integer_declaration->signedness) {
			fprintf(pos->fp, "%" PRIu64,
				integer_definition->value._unsigned);
		} else {
			fprintf(pos->fp, "%" PRId64,
				integer_definition->value._signed);
		}
		break;
	case 16:
		fprintf(pos->fp, "0x%" PRIX64,
			integer_definition->value._unsigned);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
