#ifndef BABELTRACE_CTF_WRITER_FIELDS_INTERNAL_H
#define BABELTRACE_CTF_WRITER_FIELDS_INTERNAL_H

/*
 * Babeltrace - CTF writer: Event Fields
 *
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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
 *
 * The Common Trace Format (CTF) Specification is available at
 * http://www.efficios.com/ctf
 */

#include <stdint.h>
#include <stddef.h>

#include <babeltrace/babeltrace.h>
#include <babeltrace/ctf-ir/fields-internal.h>
#include <babeltrace/ctf-writer/fields.h>
#include <babeltrace/ctf-writer/serialize-internal.h>

struct bt_ctf_field_enumeration {
	struct bt_field_common common;
	struct bt_field_common_integer *container;
};

struct bt_ctf_field_variant {
	struct bt_field_common_variant common;
	struct bt_ctf_field_enumeration *tag;
};

BT_HIDDEN
int bt_ctf_field_serialize_recursive(struct bt_ctf_field *field,
		struct bt_ctf_stream_pos *pos,
		enum bt_ctf_byte_order native_byte_order);

BT_HIDDEN
int bt_ctf_field_structure_set_field_by_name(struct bt_ctf_field *field,
		const char *name, struct bt_ctf_field *value);

BT_HIDDEN
struct bt_ctf_field *bt_ctf_field_enumeration_borrow_container(
		struct bt_ctf_field *field);

static inline
bt_bool bt_ctf_field_is_set_recursive(struct bt_ctf_field *field)
{
	return bt_field_common_is_set_recursive((void *) field);
}

#endif /* BABELTRACE_CTF_WRITER_FIELDS_INTERNAL_H */
