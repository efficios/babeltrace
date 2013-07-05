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

#include <babeltrace/ctf/types.h>
#include <inttypes.h>
#include <stdint.h>
#include <glib.h>

int ctf_enum_read(struct bt_stream_pos *ppos, struct bt_definition *definition)
{
	struct definition_enum *enum_definition =
		container_of(definition, struct definition_enum, p);
	const struct declaration_enum *enum_declaration =
		enum_definition->declaration;
	struct definition_integer *integer_definition =
		enum_definition->integer;
	const struct declaration_integer *integer_declaration =
		integer_definition->declaration;
	GArray *qs;
	int ret;

	ret = ctf_integer_read(ppos, &integer_definition->p);
	if (ret)
		return ret;
	if (!integer_declaration->signedness) {
		qs = bt_enum_uint_to_quark_set(enum_declaration,
			integer_definition->value._unsigned);
		if (!qs) {
			fprintf(stderr, "[warning] Unknown value %" PRIu64 " in enum.\n",
				integer_definition->value._unsigned);
		}
	} else {
		qs = bt_enum_int_to_quark_set(enum_declaration,
			integer_definition->value._signed);
		if (!qs) {
			fprintf(stderr, "[warning] Unknown value %" PRId64 " in enum.\n",
				integer_definition->value._signed);
		}
	}
	/* unref previous quark set */
	if (enum_definition->value)
		g_array_unref(enum_definition->value);
	enum_definition->value = qs;
	return 0;
}

int ctf_enum_write(struct bt_stream_pos *pos, struct bt_definition *definition)
{
	struct definition_enum *enum_definition =
		container_of(definition, struct definition_enum, p);
	struct definition_integer *integer_definition =
		enum_definition->integer;

	return ctf_integer_write(pos, &integer_definition->p);
}
