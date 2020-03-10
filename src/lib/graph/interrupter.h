/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2019 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_GRAPH_INTERRUPTER_INTERNAL_H
#define BABELTRACE_GRAPH_INTERRUPTER_INTERNAL_H

#include <stdbool.h>

#include <glib.h>
#include <babeltrace2/babeltrace.h>

#include "lib/object.h"

struct bt_interrupter {
	struct bt_object base;
	bool is_set;
};

static inline
bool bt_interrupter_array_any_is_set(const GPtrArray *interrupters)
{
	bool is_set = false;
	uint64_t i;

	BT_ASSERT_DBG(interrupters);

	for (i = 0; i < interrupters->len; i++) {
		const struct bt_interrupter *intr = interrupters->pdata[i];

		if (intr->is_set) {
			is_set = true;
			goto end;
		}
	}

end:
	return is_set;
}

#endif /* BABELTRACE_GRAPH_INTERRUPTER_INTERNAL_H */
