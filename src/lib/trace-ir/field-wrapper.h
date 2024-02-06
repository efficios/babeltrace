/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2018 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_TRACE_IR_FIELD_WRAPPER_INTERNAL_H
#define BABELTRACE_TRACE_IR_FIELD_WRAPPER_INTERNAL_H

#include "lib/object-pool.h"
#include "lib/object.h"

#include "field.h"

struct bt_field_wrapper {
	struct bt_object base;

	/* Owned by this */
	struct bt_field *field;
};

struct bt_field_wrapper *bt_field_wrapper_new(void *data);

void bt_field_wrapper_destroy(struct bt_field_wrapper *field);

struct bt_field_wrapper *bt_field_wrapper_create(
		struct bt_object_pool *pool, struct bt_field_class *fc);

#endif /* BABELTRACE_TRACE_IR_FIELD_WRAPPER_INTERNAL_H */
