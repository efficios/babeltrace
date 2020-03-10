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
#include "common/macros.h"
#include <babeltrace2/value.h>

BT_HIDDEN
struct bt_value *bt_attributes_create(void);

BT_HIDDEN
void bt_attributes_destroy(struct bt_value *attr_obj);

BT_HIDDEN
uint64_t bt_attributes_get_count(const struct bt_value *attr_obj);

BT_HIDDEN
const char *bt_attributes_get_field_name(const struct bt_value *attr_obj,
		uint64_t index);

BT_HIDDEN
struct bt_value *bt_attributes_borrow_field_value(
		struct bt_value *attr_obj,
		uint64_t index);

BT_HIDDEN
int bt_attributes_set_field_value(struct bt_value *attr_obj,
		const char *name, struct bt_value *value_obj);

BT_HIDDEN
struct bt_value *bt_attributes_borrow_field_value_by_name(
		struct bt_value *attr_obj, const char *name);

BT_HIDDEN
int bt_attributes_freeze(const struct bt_value *attr_obj);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_TRACE_IR_ATTRIBUTES_H */
