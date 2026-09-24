/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#include <Python.h>

#include <babeltrace2/babeltrace.h>

#include "../api.hpp"
#include "../runtime/py-module-builder.hpp"

namespace bt2py {

void registerIntegerRangeSetApi(PyModuleBuilder& mb)
{
    mb.addLibFunc<bt_integer_range_unsigned_get_lower>();
    mb.addLibFunc<bt_integer_range_unsigned_get_upper>();
    mb.addLibFunc<bt_integer_range_signed_get_lower>();
    mb.addLibFunc<bt_integer_range_signed_get_upper>();

    mb.addLibFunc<bt_integer_range_set_get_range_count>();

    mb.addLibFunc<bt_integer_range_set_unsigned_create>();
    mb.addLibFunc<bt_integer_range_set_unsigned_get_ref>();
    mb.addLibFunc<bt_integer_range_set_unsigned_put_ref>();
    mb.addLibFunc<bt_integer_range_set_unsigned_add_range>();
    mb.addLibFunc<bt_integer_range_set_unsigned_borrow_range_by_index_const>();
    mb.addLibBoolRetFunc<bt_integer_range_set_unsigned_is_equal>();
    mb.addLibFunc<bt_integer_range_set_unsigned_as_range_set_const>();

    mb.addLibFunc<bt_integer_range_set_signed_create>();
    mb.addLibFunc<bt_integer_range_set_signed_get_ref>();
    mb.addLibFunc<bt_integer_range_set_signed_put_ref>();
    mb.addLibFunc<bt_integer_range_set_signed_add_range>();
    mb.addLibFunc<bt_integer_range_set_signed_borrow_range_by_index_const>();
    mb.addLibBoolRetFunc<bt_integer_range_set_signed_is_equal>();
    mb.addLibFunc<bt_integer_range_set_signed_as_range_set_const>();
}

} /* namespace bt2py */
