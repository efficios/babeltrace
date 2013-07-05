/*
 * Common Trace Format
 *
 * Enumeration mapping strings (quarks) from/to integers.
 *
 * Copyright 2010-2011 EfficiOS Inc. and Linux Foundation
 *
 * Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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

#include <babeltrace/ctf-text/types.h>
#include <stdio.h>
#include <stdint.h>

int ctf_text_enum_write(struct bt_stream_pos *ppos, struct bt_definition *definition)
{
	struct definition_enum *enum_definition =
		container_of(definition, struct definition_enum, p);
	struct definition_integer *integer_definition =
		enum_definition->integer;
	struct ctf_text_stream_pos *pos = ctf_text_pos(ppos);
	GArray *qs;
	int ret;
	int field_nr_saved;

	if (!print_field(definition))
		return 0;

	if (pos->dummy)
		return 0;

	if (pos->field_nr++ != 0)
		fprintf(pos->fp, ",");
	fprintf(pos->fp, " ");
	if (pos->print_names)
		fprintf(pos->fp, "%s = ",
			rem_(g_quark_to_string(definition->name)));

	field_nr_saved = pos->field_nr;
	pos->field_nr = 0;
	fprintf(pos->fp, "(");
	pos->depth++;
	qs = enum_definition->value;

	if (qs) {
		int i;

		for (i = 0; i < qs->len; i++) {
			GQuark q = g_array_index(qs, GQuark, i);
			const char *str = g_quark_to_string(q);

			assert(str);
			if (pos->field_nr++ != 0)
				fprintf(pos->fp, ",");
			fprintf(pos->fp, " ");
			fprintf(pos->fp, "%s", str);
		}
	} else {
		fprintf(pos->fp, " <unknown>");
	}

	pos->field_nr = 0;
	fprintf(pos->fp, " :");
	ret = generic_rw(ppos, &integer_definition->p);

	pos->depth--;
	fprintf(pos->fp, " )");
	pos->field_nr = field_nr_saved;
	return ret;
}
