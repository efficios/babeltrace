/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2016 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_CTF_WRITER_RESOLVE_INTERNAL_H
#define BABELTRACE_CTF_WRITER_RESOLVE_INTERNAL_H

#include <babeltrace2-ctf-writer/field-types.h>
#include "common/macros.h"
#include <glib.h>

#include "field-types.h"
#include "values.h"

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
int bt_ctf_resolve_types(struct bt_ctf_private_value *environment,
		struct bt_ctf_field_type_common *packet_header_type,
		struct bt_ctf_field_type_common *packet_context_type,
		struct bt_ctf_field_type_common *event_header_type,
		struct bt_ctf_field_type_common *stream_event_ctx_type,
		struct bt_ctf_field_type_common *event_context_type,
		struct bt_ctf_field_type_common *event_payload_type,
		enum bt_ctf_resolve_flag flags);

#endif /* BABELTRACE_CTF_WRITER_RESOLVE_INTERNAL_H */
