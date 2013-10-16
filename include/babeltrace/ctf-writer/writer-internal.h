#ifndef BABELTRACE_CTF_WRITER_WRITER_INTERNAL_H
#define BABELTRACE_CTF_WRITER_WRITER_INTERNAL_H

/*
 * BabelTrace - CTF Writer: Writer internal
 *
 * Copyright 2013 EfficiOS Inc.
 *
 * Author: Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/ctf-writer/ref-internal.h>
#include <babeltrace/ctf-writer/writer.h>
#include <babeltrace/ctf-writer/event-types.h>
#include <babeltrace/ctf-writer/event-fields.h>
#include <babeltrace/babeltrace-internal.h>
#include <glib.h>
#include <dirent.h>
#include <sys/types.h>
#include <uuid/uuid.h>

enum field_type_alias {
	FIELD_TYPE_ALIAS_UINT5_T = 0,
	FIELD_TYPE_ALIAS_UINT8_T,
	FIELD_TYPE_ALIAS_UINT16_T,
	FIELD_TYPE_ALIAS_UINT27_T,
	FIELD_TYPE_ALIAS_UINT32_T,
	FIELD_TYPE_ALIAS_UINT64_T,
	NR_FIELD_TYPE_ALIAS,
};

struct bt_ctf_writer {
	struct bt_ctf_ref ref_count;
	int frozen; /* Protects attributes that can't be changed mid-trace */
	GString *path;
	uuid_t uuid;
	int byte_order;
	int trace_dir_fd;
	int metadata_fd;
	GPtrArray *environment; /* Array of pointers to environment_variable */
	GPtrArray *clocks; /* Array of pointers to bt_ctf_clock */
	GPtrArray *stream_classes; /* Array of pointers to bt_ctf_stream_class */
	GPtrArray *streams; /* Array of pointers to bt_ctf_stream */
	struct bt_ctf_field_type *trace_packet_header_type;
	struct bt_ctf_field *trace_packet_header;
	uint32_t next_stream_id;
};

struct environment_variable {
	GString *name, *value;
};

struct metadata_context {
	GString *string;
	GString *field_name;
	unsigned int current_indentation_level;
};

/* Checks that the string does not contain a reserved keyword */
BT_HIDDEN
int validate_identifier(const char *string);

BT_HIDDEN
const char *get_byte_order_string(int byte_order);

BT_HIDDEN
struct bt_ctf_field_type *get_field_type(enum field_type_alias alias);

#endif /* BABELTRACE_CTF_WRITER_WRITER_INTERNAL_H */
