/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * The Common Trace Format (CTF) Specification is available at
 * http://www.efficios.com/ctf
 */

#ifndef BABELTRACE_CTF_WRITER_STREAM_INTERNAL_H
#define BABELTRACE_CTF_WRITER_STREAM_INTERNAL_H

#include "common/assert.h"
#include "common/macros.h"
#include <babeltrace2-ctf-writer/stream.h>
#include "ctfser/ctfser.h"
#include <stdint.h>

#include "assert-pre.h"
#include "object.h"
#include "stream.h"
#include "utils.h"

struct bt_ctf_stream_common;

struct bt_ctf_stream_common {
	struct bt_ctf_object base;
	int64_t id;
	struct bt_ctf_stream_class_common *stream_class;
	GString *name;
};

int bt_ctf_stream_common_initialize(
		struct bt_ctf_stream_common *stream,
		struct bt_ctf_stream_class_common *stream_class, const char *name,
		uint64_t id, bt_ctf_object_release_func release_func);

void bt_ctf_stream_common_finalize(struct bt_ctf_stream_common *stream);

static inline
struct bt_ctf_stream_class_common *bt_ctf_stream_common_borrow_class(
		struct bt_ctf_stream_common *stream)
{
	BT_ASSERT_DBG(stream);
	return stream->stream_class;
}

static inline
const char *bt_ctf_stream_common_get_name(struct bt_ctf_stream_common *stream)
{
	BT_CTF_ASSERT_PRE_NON_NULL(stream, "Stream");
	return stream->name ? stream->name->str : NULL;
}

static inline
int64_t bt_ctf_stream_common_get_id(struct bt_ctf_stream_common *stream)
{
	int64_t ret;

	BT_CTF_ASSERT_PRE_NON_NULL(stream, "Stream");
	ret = stream->id;
	if (ret < 0) {
		BT_LOGT("Stream's ID is not set: addr=%p, name=\"%s\"",
			stream, bt_ctf_stream_common_get_name(stream));
	}

	return ret;
}

struct bt_ctf_stream {
	struct bt_ctf_stream_common common;
	struct bt_ctf_field *packet_header;
	struct bt_ctf_field *packet_context;

	/* Array of pointers to bt_ctf_event for the current packet */
	GPtrArray *events;
	struct bt_ctfser ctfser;
	unsigned int flushed_packet_count;
	uint64_t discarded_events;
	uint64_t last_ts_end;
};

struct bt_ctf_stream *bt_ctf_stream_create_with_id(
		struct bt_ctf_stream_class *stream_class,
		const char *name, uint64_t id);

#endif /* BABELTRACE_CTF_WRITER_STREAM_INTERNAL_H */
