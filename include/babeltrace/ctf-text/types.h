#ifndef _BABELTRACE_CTF_TEXT_TYPES_H
#define _BABELTRACE_CTF_TEXT_TYPES_H

/*
 * Common Trace Format (Text Output)
 *
 * Type header
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
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <sys/mman.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <glib.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/types.h>
#include <babeltrace/format.h>

/*
 * Inherit from both struct bt_stream_pos and struct bt_trace_descriptor.
 */
struct ctf_text_stream_pos {
	struct bt_stream_pos parent;
	struct bt_trace_descriptor trace_descriptor;
	FILE *fp;		/* File pointer. NULL if unset. */
	int depth;
	int dummy;		/* disable output */
	int print_names;	/* print field names */
	int field_nr;
	uint64_t last_real_timestamp;	/* to print delta */
	uint64_t last_cycles_timestamp;	/* to print delta */
	GString *string;	/* Current string */
};

static inline
struct ctf_text_stream_pos *ctf_text_pos(struct bt_stream_pos *pos)
{
	return container_of(pos, struct ctf_text_stream_pos, parent);
}

/*
 * Write only is supported for now.
 */
BT_HIDDEN
int ctf_text_integer_write(struct bt_stream_pos *pos, struct bt_definition *definition);
BT_HIDDEN
int ctf_text_float_write(struct bt_stream_pos *pos, struct bt_definition *definition);
BT_HIDDEN
int ctf_text_string_write(struct bt_stream_pos *pos, struct bt_definition *definition);
BT_HIDDEN
int ctf_text_enum_write(struct bt_stream_pos *pos, struct bt_definition *definition);
BT_HIDDEN
int ctf_text_struct_write(struct bt_stream_pos *pos, struct bt_definition *definition);
BT_HIDDEN
int ctf_text_variant_write(struct bt_stream_pos *pos, struct bt_definition *definition);
BT_HIDDEN
int ctf_text_array_write(struct bt_stream_pos *pos, struct bt_definition *definition);
BT_HIDDEN
int ctf_text_sequence_write(struct bt_stream_pos *pos, struct bt_definition *definition);

static inline
void print_pos_tabs(struct ctf_text_stream_pos *pos)
{
	int i;

	for (i = 0; i < pos->depth; i++)
		fprintf(pos->fp, "\t");
}

/*
 * Check if the field must be printed.
 */
BT_HIDDEN
int print_field(struct bt_definition *definition);

#endif /* _BABELTRACE_CTF_TEXT_TYPES_H */
