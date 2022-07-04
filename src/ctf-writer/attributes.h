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

struct bt_ctf_private_value *bt_ctf_attributes_create(void);

void bt_ctf_attributes_destroy(struct bt_ctf_private_value *attr_obj);

int64_t bt_ctf_attributes_get_count(struct bt_ctf_private_value *attr_obj);

const char *bt_ctf_attributes_get_field_name(struct bt_ctf_private_value *attr_obj,
		uint64_t index);

struct bt_ctf_private_value *bt_ctf_attributes_borrow_field_value(struct bt_ctf_private_value *attr_obj,
		uint64_t index);

int bt_ctf_attributes_set_field_value(struct bt_ctf_private_value *attr_obj,
		const char *name, struct bt_ctf_private_value *value_obj);

struct bt_ctf_private_value *bt_ctf_attributes_borrow_field_value_by_name(
		struct bt_ctf_private_value *attr_obj, const char *name);

int bt_ctf_attributes_freeze(struct bt_ctf_private_value *attr_obj);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_WRITER_ATTRIBUTES_H */
