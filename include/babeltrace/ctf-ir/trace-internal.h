#ifndef BABELTRACE_CTF_IR_TRACE_INTERNAL_H
#define BABELTRACE_CTF_IR_TRACE_INTERNAL_H

/*
 * BabelTrace - CTF IR: Trace internal
 *
 * Copyright 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/ctf-ir/trace.h>
#include <babeltrace/ctf-ir/event-types.h>
#include <babeltrace/ctf-ir/event-fields.h>
#include <babeltrace/ctf-ir/common-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/values.h>
#include <glib.h>
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

struct bt_ctf_trace {
	struct bt_ctf_base base;
	int frozen;
	uuid_t uuid;
	int byte_order; /* A value defined in Babeltrace's "endian.h" */
	struct bt_value *environment;
	GPtrArray *clocks; /* Array of pointers to bt_ctf_clock */
	GPtrArray *stream_classes; /* Array of ptrs to bt_ctf_stream_class */
	GPtrArray *streams; /* Array of ptrs to bt_ctf_stream */
	struct bt_ctf_field_type *packet_header_type;
	uint64_t next_stream_id;
};

struct metadata_context {
	GString *string;
	GString *field_name;
	unsigned int current_indentation_level;
};

BT_HIDDEN
const char *get_byte_order_string(int byte_order);

BT_HIDDEN
struct bt_ctf_field_type *get_field_type(enum field_type_alias alias);

#endif /* BABELTRACE_CTF_IR_TRACE_INTERNAL_H */
