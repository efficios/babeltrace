/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2015-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright (c) 2015 EfficiOS Inc. and Linux Foundation
 */

#ifndef BABELTRACE_TRACE_IR_ATTRIBUTES_H
#define BABELTRACE_TRACE_IR_ATTRIBUTES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <babeltrace2/value.h>

struct bt_value *bt_attributes_create(void);

void bt_attributes_destroy(struct bt_value *attr_obj);

uint64_t bt_attributes_get_count(const struct bt_value *attr_obj);

const char *bt_attributes_get_field_name(const struct bt_value *attr_obj,
		uint64_t index);

struct bt_value *bt_attributes_borrow_field_value(
		struct bt_value *attr_obj,
		uint64_t index);

int bt_attributes_set_field_value(struct bt_value *attr_obj,
		const char *name, struct bt_value *value_obj);

struct bt_value *bt_attributes_borrow_field_value_by_name(
		struct bt_value *attr_obj, const char *name);

int bt_attributes_freeze(const struct bt_value *attr_obj);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_TRACE_IR_ATTRIBUTES_H */
