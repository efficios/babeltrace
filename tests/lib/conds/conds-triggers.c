/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2020 Philippe Proulx <pproulx@efficios.com>
 */

#include <babeltrace2/babeltrace.h>

#include "common/assert.h"
#include "utils.h"

static
bt_field_class *get_uint_fc(bt_self_component *self_comp)
{
	bt_trace_class *tc = bt_trace_class_create(self_comp);
	bt_field_class *fc;

	BT_ASSERT(tc);
	fc = bt_field_class_integer_unsigned_create(tc);
	BT_ASSERT(fc);
	return fc;
}

static
void trigger_fc_int_set_field_value_range_n_0(bt_self_component *self_comp)
{
	bt_field_class_integer_set_field_value_range(get_uint_fc(self_comp), 0);
}

static
void trigger_fc_int_set_field_value_range_n_gt_64(bt_self_component *self_comp)
{
	bt_field_class_integer_set_field_value_range(get_uint_fc(self_comp),
		65);
}

static
void trigger_fc_int_set_field_value_range_null(bt_self_component *self_comp)
{
	bt_field_class_integer_set_field_value_range(NULL, 23);
}

static
const struct cond_trigger triggers[] = {
	COND_TRIGGER_PRE_RUN_IN_COMP_CLS_INIT(
		"pre:field-class-integer-set-field-value-range:valid-n",
		"0",
		trigger_fc_int_set_field_value_range_n_0
	),
	COND_TRIGGER_PRE_RUN_IN_COMP_CLS_INIT(
		"pre:field-class-integer-set-field-value-range:valid-n",
		"gt-64",
		trigger_fc_int_set_field_value_range_n_gt_64
	),
	COND_TRIGGER_PRE_RUN_IN_COMP_CLS_INIT(
		"pre:field-class-integer-set-field-value-range:not-null:field-class",
		NULL,
		trigger_fc_int_set_field_value_range_null
	),
};

int main(int argc, const char *argv[])
{
	cond_main(argc, argv, triggers, sizeof(triggers) / sizeof(*triggers));
	return 0;
}
