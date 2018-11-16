#ifndef BABELTRACE_TRACE_IR_ATTRIBUTES_H
#define BABELTRACE_TRACE_IR_ATTRIBUTES_H

/*
 * attributes.c
 *
 * Babeltrace - Trace IR: Attributes internal
 *
 * Copyright (c) 2015 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2015 Philippe Proulx <pproulx@efficios.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/values.h>

BT_HIDDEN
struct bt_private_value *bt_attributes_create(void);

BT_HIDDEN
void bt_attributes_destroy(struct bt_private_value *attr_obj);

BT_HIDDEN
int64_t bt_attributes_get_count(struct bt_private_value *attr_obj);

BT_HIDDEN
const char *bt_attributes_get_field_name(struct bt_private_value *attr_obj,
		uint64_t index);

BT_HIDDEN
struct bt_private_value *bt_attributes_borrow_field_value(
		struct bt_private_value *attr_obj,
		uint64_t index);

BT_HIDDEN
int bt_attributes_set_field_value(struct bt_private_value *attr_obj,
		const char *name, struct bt_private_value *value_obj);

BT_HIDDEN
struct bt_private_value *bt_attributes_borrow_field_value_by_name(
		struct bt_private_value *attr_obj, const char *name);

BT_HIDDEN
int bt_attributes_freeze(struct bt_private_value *attr_obj);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_TRACE_IR_ATTRIBUTES_H */
