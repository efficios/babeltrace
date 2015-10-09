/*
 * Common Trace Format
 *
 * Strings read/write functions.
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

#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/ctf/types.h>
#include <limits.h>		/* C99 limits */
#include <babeltrace/compat/string.h>

int ctf_string_read(struct bt_stream_pos *ppos, struct bt_definition *definition)
{
	struct definition_string *string_definition =
		container_of(definition, struct definition_string, p);
	const struct declaration_string *string_declaration =
		string_definition->declaration;
	struct ctf_stream_pos *pos = ctf_pos(ppos);
	size_t len;
	ssize_t max_len_bits;
	char *srcaddr;

	if (!ctf_align_pos(pos, string_declaration->p.alignment))
		return -EFAULT;

	srcaddr = ctf_get_pos_addr(pos);
	if (pos->offset == EOF)
		return -EFAULT;
	/* Not counting \0. Counting in bits. */
	max_len_bits = pos->packet_size - pos->offset - CHAR_BIT;
	if (max_len_bits < 0)
		return -EFAULT;
	/* Add \0, counting in bytes. */
	len = bt_strnlen(srcaddr, (size_t) max_len_bits / CHAR_BIT) + 1;
	/* Truncated string, unexpected. Trace probably corrupted. */
	if (srcaddr[len - 1] != '\0')
		return -EFAULT;

	if (string_definition->alloc_len < len) {
		string_definition->value =
			g_realloc(string_definition->value, len);
		string_definition->alloc_len = len;
	}
	printf_debug("CTF string read %s\n", srcaddr);
	memcpy(string_definition->value, srcaddr, len);
	string_definition->len = len;
	if (!ctf_move_pos(pos, len * CHAR_BIT))
		return -EFAULT;
	return 0;
}

int ctf_string_write(struct bt_stream_pos *ppos,
		      struct bt_definition *definition)
{
	struct definition_string *string_definition =
		container_of(definition, struct definition_string, p);
	const struct declaration_string *string_declaration =
		string_definition->declaration;
	struct ctf_stream_pos *pos = ctf_pos(ppos);
	size_t len;
	char *destaddr;

	if (!ctf_align_pos(pos, string_declaration->p.alignment))
		return -EFAULT;
	assert(string_definition->value != NULL);
	len = string_definition->len;

	if (!ctf_pos_access_ok(pos, len))
		return -EFAULT;

	if (pos->dummy)
		goto end;
	destaddr = ctf_get_pos_addr(pos);
	memcpy(destaddr, string_definition->value, len);
end:
	if (!ctf_move_pos(pos, len * CHAR_BIT))
		return -EFAULT;
	return 0;
}
