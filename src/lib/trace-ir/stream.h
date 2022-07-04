/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2013, 2014 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#ifndef BABELTRACE_TRACE_IR_STREAM_INTERNAL_H
#define BABELTRACE_TRACE_IR_STREAM_INTERNAL_H

#include <babeltrace2/trace-ir/stream.h>
#include "lib/object.h"
#include "lib/object-pool.h"
#include "common/macros.h"
#include <glib.h>
#include <stdbool.h>

#include "utils.h"

struct bt_stream_class;
struct bt_stream;

struct bt_stream {
	struct bt_object base;

	/* Owned by this */
	struct bt_value *user_attributes;

	/* Owned by this */
	struct bt_stream_class *class;

	struct {
		GString *str;

		/* NULL or `str->str` above */
		const char *value;
	} name;

	uint64_t id;

	/* Pool of `struct bt_packet *` */
	struct bt_object_pool packet_pool;

	bool frozen;
};

void _bt_stream_freeze(const struct bt_stream *stream);

#ifdef BT_DEV_MODE
# define bt_stream_freeze		_bt_stream_freeze
#else
# define bt_stream_freeze(_stream)
#endif

static inline
struct bt_trace *bt_stream_borrow_trace_inline(const struct bt_stream *stream)
{
	BT_ASSERT_DBG(stream);
	return (void *) bt_object_borrow_parent(&stream->base);
}

#endif /* BABELTRACE_TRACE_IR_STREAM_INTERNAL_H */
