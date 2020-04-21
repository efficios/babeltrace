/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2016-2018 Philippe Proulx <pproulx@efficios.com>
 *
 * The Common Trace Format (CTF) Specification is available at
 * http://www.efficios.com/ctf
 */

#ifndef BABELTRACE_TRACE_IR_RESOLVE_FIELD_PATH_INTERNAL
#define BABELTRACE_TRACE_IR_RESOLVE_FIELD_PATH_INTERNAL

#include "common/macros.h"
#include "lib/object.h"
#include <babeltrace2/trace-ir/field-class.h>
#include <babeltrace2/trace-ir/field-path.h>
#include <glib.h>

struct bt_resolve_field_path_context {
	struct bt_field_class *packet_context;
	struct bt_field_class *event_common_context;
	struct bt_field_class *event_specific_context;
	struct bt_field_class *event_payload;
};

BT_HIDDEN
int bt_resolve_field_paths(struct bt_field_class *field_class,
		struct bt_resolve_field_path_context *ctx,
		const char *api_func);

#endif /* BABELTRACE_TRACE_IR_RESOLVE_FIELD_PATH_INTERNAL */
