/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2020 Philippe Proulx <pproulx@efficios.com>
 */

#include <babeltrace2/babeltrace.h>

#include "utils.hpp"

namespace {

void triggerGraphMipVersion() noexcept
{
    bt_graph_create(292);
}

bt2::IntegerFieldClass::Shared getUIntFc(const bt2::SelfComponent self) noexcept
{
    return self.createTraceClass()->createUnsignedIntegerFieldClass();
}

void triggerFcIntSetFieldValueRangeN0(const bt2::SelfComponent self) noexcept
{
    getUIntFc(self)->fieldValueRange(0);
}

void triggerFcIntSetFieldValueRangeNGt64(const bt2::SelfComponent self) noexcept
{
    getUIntFc(self)->fieldValueRange(65);
}

void triggerFcIntSetFieldValueRangeNull(bt2::SelfComponent) noexcept
{
    bt_field_class_integer_set_field_value_range(NULL, 23);
}

const cond_trigger triggers[] = {
    COND_TRIGGER_PRE_BASIC("pre:graph-create:valid-mip-version", NULL, triggerGraphMipVersion),
    COND_TRIGGER_PRE_RUN_IN_COMP_CLS_INIT("pre:field-class-integer-set-field-value-range:valid-n",
                                          "0", triggerFcIntSetFieldValueRangeN0),
    COND_TRIGGER_PRE_RUN_IN_COMP_CLS_INIT("pre:field-class-integer-set-field-value-range:valid-n",
                                          "gt-64", triggerFcIntSetFieldValueRangeNGt64),
    COND_TRIGGER_PRE_RUN_IN_COMP_CLS_INIT(
        "pre:field-class-integer-set-field-value-range:not-null:field-class", NULL,
        triggerFcIntSetFieldValueRangeNull),
};

} /* namespace */

int main(int argc, const char *argv[])
{
    cond_main(argc, argv, triggers, sizeof(triggers) / sizeof(*triggers));
    return 0;
}
