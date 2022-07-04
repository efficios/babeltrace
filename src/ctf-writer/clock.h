/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_CTF_WRITER_CLOCK_INTERNAL_H
#define BABELTRACE_CTF_WRITER_CLOCK_INTERNAL_H

#include <babeltrace2-ctf-writer/clock.h>
#include "common/macros.h"
#include <glib.h>

#include "clock-class.h"
#include "object.h"
#include "trace.h"

struct bt_ctf_clock {
	struct bt_ctf_object base;
	struct bt_ctf_clock_class *clock_class;
	uint64_t value;		/* Current clock value */
};

struct metadata_context;

int bt_ctf_clock_get_value(struct bt_ctf_clock *clock, uint64_t *value);

void bt_ctf_clock_class_serialize(struct bt_ctf_clock_class *clock_class,
		struct metadata_context *context);

#endif /* BABELTRACE_CTF_WRITER_CLOCK_INTERNAL_H */
