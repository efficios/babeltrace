/*
 * Common Trace Format
 *
 * Floating point read/write functions.
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
 *
 * Reference: ISO C99 standard 5.2.4
 */

#include <babeltrace/ctf-text/types.h>
#include <stdio.h>

int ctf_text_float_write(struct stream_pos *ppos, struct definition *definition)
{
	struct definition_float *float_definition =
		container_of(definition, struct definition_float, p);
	struct ctf_text_stream_pos *pos = ctf_text_pos(ppos);

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

	fprintf(pos->fp, "%g", float_definition->value);
	return 0;
}
