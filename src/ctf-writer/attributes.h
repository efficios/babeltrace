/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2015 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2015 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_CTF_WRITER_ATTRIBUTES_H
#define BABELTRACE_CTF_WRITER_ATTRIBUTES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "common/macros.h"

#include "values.h"

BT_HIDDEN
struct bt_ctf_private_value *bt_ctf_attributes_create(void);

BT_HIDDEN
void bt_ctf_attributes_destroy(struct bt_ctf_private_value *attr_obj);

BT_HIDDEN
int64_t bt_ctf_attributes_get_count(struct bt_ctf_private_value *attr_obj);

BT_HIDDEN
const char *bt_ctf_attributes_get_field_name(struct bt_ctf_private_value *attr_obj,
		uint64_t index);

BT_HIDDEN
struct bt_ctf_private_value *bt_ctf_attributes_borrow_field_value(struct bt_ctf_private_value *attr_obj,
		uint64_t index);

BT_HIDDEN
int bt_ctf_attributes_set_field_value(struct bt_ctf_private_value *attr_obj,
		const char *name, struct bt_ctf_private_value *value_obj);

BT_HIDDEN
struct bt_ctf_private_value *bt_ctf_attributes_borrow_field_value_by_name(
		struct bt_ctf_private_value *attr_obj, const char *name);

BT_HIDDEN
int bt_ctf_attributes_freeze(struct bt_ctf_private_value *attr_obj);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_WRITER_ATTRIBUTES_H */
