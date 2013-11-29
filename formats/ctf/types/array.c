/*
 * Common Trace Format
 *
 * Array format access functions.
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

int ctf_array_read(struct bt_stream_pos *ppos, struct bt_definition *definition)
{
	struct definition_array *array_definition =
		container_of(definition, struct definition_array, p);
	struct declaration_array *array_declaration =
		array_definition->declaration;
	struct bt_declaration *elem = array_declaration->elem;
	struct ctf_stream_pos *pos =
		container_of(ppos, struct ctf_stream_pos, parent);

	if (elem->id == CTF_TYPE_INTEGER) {
		struct declaration_integer *integer_declaration =
			container_of(elem, struct declaration_integer, p);

		if (integer_declaration->encoding == CTF_STRING_UTF8
		      || integer_declaration->encoding == CTF_STRING_ASCII) {

			if (integer_declaration->len == CHAR_BIT
			    && integer_declaration->p.alignment == CHAR_BIT) {

				if (!ctf_align_pos(pos, integer_declaration->p.alignment))
					return -EFAULT;
				if (!ctf_pos_access_ok(pos, array_declaration->len * CHAR_BIT))
					return -EFAULT;

				g_string_assign(array_definition->string, "");
				g_string_insert_len(array_definition->string,
					0, (char *) ctf_get_pos_addr(pos),
					array_declaration->len);
				/*
				 * We want to populate both the string
				 * and the underlying values, so carry
				 * on calling bt_array_rw().
				 */
			}
		}
	}
	return bt_array_rw(ppos, definition);
}

int ctf_array_write(struct bt_stream_pos *ppos, struct bt_definition *definition)
{
	struct definition_array *array_definition =
		container_of(definition, struct definition_array, p);
	struct declaration_array *array_declaration =
		array_definition->declaration;
	struct bt_declaration *elem = array_declaration->elem;
	struct ctf_stream_pos *pos =
		container_of(ppos, struct ctf_stream_pos, parent);

	if (elem->id == CTF_TYPE_INTEGER) {
		struct declaration_integer *integer_declaration =
			container_of(elem, struct declaration_integer, p);

		if (integer_declaration->encoding == CTF_STRING_UTF8
		      || integer_declaration->encoding == CTF_STRING_ASCII) {

			if (integer_declaration->len == CHAR_BIT
			    && integer_declaration->p.alignment == CHAR_BIT) {

				if (!ctf_align_pos(pos, integer_declaration->p.alignment))
					return -EFAULT;
				if (!ctf_pos_access_ok(pos, array_declaration->len * CHAR_BIT))
					return -EFAULT;

				memcpy((char *) ctf_get_pos_addr(pos),
					array_definition->string->str,
					array_declaration->len);
				if (!ctf_move_pos(pos, array_declaration->len * CHAR_BIT))
					return -EFAULT;
				return 0;
			}
		}
	}
	return bt_array_rw(ppos, definition);
}
