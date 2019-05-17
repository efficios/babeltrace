/*
 * Common Trace Format
 *
 * Integers read/write functions.
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
#include <inttypes.h>
#include <stdint.h>
#include <babeltrace/bitfield.h>
#include <babeltrace/trace-debug-info.h>

int ctf_text_integer_write(struct bt_stream_pos *ppos, struct bt_definition *definition)
{
	struct definition_integer *integer_definition =
		container_of(definition, struct definition_integer, p);
	const struct declaration_integer *integer_declaration =
		integer_definition->declaration;
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

	if (pos->string
	    && (integer_declaration->encoding == CTF_STRING_ASCII
	      || integer_declaration->encoding == CTF_STRING_UTF8)) {

		if (!integer_declaration->signedness) {
			g_string_append_c(pos->string,
				(int) integer_definition->value._unsigned);
		} else {
			g_string_append_c(pos->string,
				(int) integer_definition->value._signed);
		}
		return 0;
	}

	switch (integer_declaration->base) {
	case 0:	/* default */
	case 10:
		if (!integer_declaration->signedness) {
			fprintf(pos->fp, "%" PRIu64,
				integer_definition->value._unsigned);
		} else {
			fprintf(pos->fp, "%" PRId64,
				integer_definition->value._signed);
		}
		break;
	case 2:
	{
		int bitnr;
		uint64_t v;

		if (!integer_declaration->signedness)
			v = integer_definition->value._unsigned;
		else
			v = (uint64_t) integer_definition->value._signed;

		fprintf(pos->fp, "0b");
		_bt_safe_lshift(v, 64 - integer_declaration->len);
		for (bitnr = 0; bitnr < integer_declaration->len; bitnr++) {
			fprintf(pos->fp, "%u", (v & (1ULL << 63)) ? 1 : 0);
			_bt_safe_lshift(v, 1);
		}
		break;
	}
	case 8:
	{
		uint64_t v;

		if (!integer_declaration->signedness) {
			v = integer_definition->value._unsigned;
		} else {
			v = (uint64_t) integer_definition->value._signed;
			if (integer_declaration->len < 64) {
				size_t len = integer_declaration->len;
			        size_t rounded_len;

				assert(len != 0);
				/* Round length to the nearest 3-bit */
				rounded_len = (((len - 1) / 3) + 1) * 3;
				v &= ((uint64_t) 1 << rounded_len) - 1;
			}
		}

		fprintf(pos->fp, "0%" PRIo64, v);
		break;
	}
	case 16:
	{
		uint64_t v;

		if (!integer_declaration->signedness) {
			v = integer_definition->value._unsigned;
		} else {
			v = (uint64_t) integer_definition->value._signed;
			if (integer_declaration->len < 64) {
				/* Round length to the nearest nibble */
				uint8_t rounded_len =
					((integer_declaration->len + 3) & ~0x3);

				v &= ((uint64_t) 1 << rounded_len) - 1;
			}
		}

		fprintf(pos->fp, "0x%" PRIX64, v);
		break;
	}
	default:
		return -EINVAL;
	}

	ctf_text_integer_write_debug_info(ppos, definition);

	return 0;
}
