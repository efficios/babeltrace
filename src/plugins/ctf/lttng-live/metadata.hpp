/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2016 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef LTTNG_LIVE_METADATA_H
#define LTTNG_LIVE_METADATA_H

#include <stdint.h>
#include <stdio.h>

#include "lttng-live.hpp"

int lttng_live_metadata_create_stream(struct lttng_live_session *session, uint64_t ctf_trace_id,
                                      uint64_t stream_id);

enum lttng_live_iterator_status lttng_live_metadata_update(struct lttng_live_trace *trace);

void lttng_live_metadata_fini(struct lttng_live_trace *trace);

#endif /* LTTNG_LIVE_METADATA_H */
