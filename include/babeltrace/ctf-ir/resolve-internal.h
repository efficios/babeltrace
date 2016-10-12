#ifndef BABELTRACE_CTF_IR_RESOLVE_INTERNAL_H
#define BABELTRACE_CTF_IR_RESOLVE_INTERNAL_H

/*
 * Babeltrace - CTF IR: Type resolving internal
 *
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2016 Philippe Proulx <pproulx@efficios.com>
 *
 * Authors: Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *          Philippe Proulx <pproulx@efficios.com>
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

#include <babeltrace/ctf-ir/field-types.h>
#include <babeltrace/ctf-ir/field-types-internal.h>
#include <babeltrace/values.h>
#include <babeltrace/babeltrace-internal.h>
#include <glib.h>

enum bt_ctf_resolve_flag {
	BT_CTF_RESOLVE_FLAG_PACKET_HEADER	= 0x01,
	BT_CTF_RESOLVE_FLAG_PACKET_CONTEXT	= 0x02,
	BT_CTF_RESOLVE_FLAG_EVENT_HEADER	= 0x04,
	BT_CTF_RESOLVE_FLAG_STREAM_EVENT_CTX	= 0x08,
	BT_CTF_RESOLVE_FLAG_EVENT_CONTEXT	= 0x10,
	BT_CTF_RESOLVE_FLAG_EVENT_PAYLOAD	= 0x20,
};

/*
 * Resolves CTF IR field types: recursively locates the tag and length
 * field types of resp. variant and sequence field types.
 *
 * All `*_type` parameters may be resolved, and may as well serve as
 * resolving targets.
 *
 * Resolving is performed based on the flags in `flags`.
 *
 * It is expected that, amongst all the provided types, no common
 * references to sequence variant field types exist. In other words,
 * this function does not copy field types.
 *
 * All parameters are owned by the caller.
 */
BT_HIDDEN
int bt_ctf_resolve_types(struct bt_value *environment,
		struct bt_ctf_field_type *packet_header_type,
		struct bt_ctf_field_type *packet_context_type,
		struct bt_ctf_field_type *event_header_type,
		struct bt_ctf_field_type *stream_event_ctx_type,
		struct bt_ctf_field_type *event_context_type,
		struct bt_ctf_field_type *event_payload_type,
		enum bt_ctf_resolve_flag flags);

#endif /* BABELTRACE_CTF_IR_RESOLVE_INTERNAL_H */
