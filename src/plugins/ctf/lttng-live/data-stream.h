/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2016 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef LTTNG_LIVE_DATA_STREAM_H
#define LTTNG_LIVE_DATA_STREAM_H

#include <stdio.h>
#include <stdint.h>

#include <glib.h>

#include "../common/msg-iter/msg-iter.h"
#include "lttng-live.h"

enum lttng_live_iterator_status lttng_live_lazy_msg_init(
		struct lttng_live_session *session,
		bt_self_message_iterator *self_msg_iter);

struct lttng_live_stream_iterator *lttng_live_stream_iterator_create(
		struct lttng_live_session *session,
		uint64_t ctf_trace_id,
		uint64_t stream_id,
		bt_self_message_iterator *self_msg_iter);

void lttng_live_stream_iterator_destroy(
		struct lttng_live_stream_iterator *stream);

#endif /* LTTNG_LIVE_DATA_STREAM_H */
