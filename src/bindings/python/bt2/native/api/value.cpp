/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#include <Python.h>

#include <babeltrace2/babeltrace.h>

#include "common/assert.h"
#include "cpp-common/bt2/value.hpp"
#include "cpp-common/bt2c/c-string-view.hpp"

#include "../api.hpp"
#include "../runtime/exc.hpp"
#include "../runtime/ptr-obj.hpp"
#include "../runtime/py-module-builder.hpp"
#include "../runtime/utils.hpp"

namespace bt2py {
namespace {

/*
 * Returns a new Python list of strings containing the keys of `val`.
 */
PyObjUP getMapValueKeys(const bt2::ConstMapValue val)
{
    auto pyKeys = ownPyObjOrThrow(PyList_New(static_cast<Py_ssize_t>(val.length())));
    Py_ssize_t index = 0;
    auto failed = false;

    /*
     * Don't throw from the user function:
     * bt2::CommonMapValue::forEach() would translate any exception to
     * `bt2::Error`, overwriting the Python error indicator.
     *
     * On failure, the remaining items of `pyKeys` are `NULL`, which the
     * list deallocator handles.
     */
    val.forEach([&pyKeys, &index, &failed](const bt2c::CStringView key, bt2::ConstValue) {
        if (failed) {
            return;
        }

        const auto pyKey = PyUnicode_FromString(key.data());

        if (!pyKey) {
            failed = true;
            return;
        }

        /* Steals the reference to `pyKey` */
        PyList_SET_ITEM(pyKeys.get(), index, pyKey);
        ++index;
    });

    if (failed) {
        throw PyErrSet {};
    }

    BT_ASSERT_DBG(index == PyList_GET_SIZE(pyKeys.get()));
    return pyKeys;
}

} /* namespace */

void registerValueApi(PyModuleBuilder& mb)
{
    mb.addWiseEnumConstants<bt2::ValueType>("VALUE_TYPE_");

    mb.addObject("value_null", []() -> PyObject& {
        return createPtrObj(bt_value_null);
    });

    mb.addLibFunc<bt_value_get_type>();
    mb.addLibFunc<bt_value_get_ref>();
    mb.addLibFunc<bt_value_put_ref>();

    mb.addLibFunc<bt_value_bool_create>();
    mb.addLibFunc<bt_value_bool_create_init>();
    mb.addLibBoolRetFunc<bt_value_bool_get>();
    mb.addLibFunc<bt_value_bool_set>();

    mb.addLibFunc<bt_value_integer_unsigned_create>();
    mb.addLibFunc<bt_value_integer_unsigned_create_init>();
    mb.addLibFunc<bt_value_integer_unsigned_get>();
    mb.addLibFunc<bt_value_integer_unsigned_set>();

    mb.addLibFunc<bt_value_integer_signed_create>();
    mb.addLibFunc<bt_value_integer_signed_create_init>();
    mb.addLibFunc<bt_value_integer_signed_get>();
    mb.addLibFunc<bt_value_integer_signed_set>();

    mb.addLibFunc<bt_value_real_create>();
    mb.addLibFunc<bt_value_real_create_init>();
    mb.addLibFunc<bt_value_real_get>();
    mb.addLibFunc<bt_value_real_set>();

    mb.addLibFunc<bt_value_string_create>();
    mb.addLibFunc<bt_value_string_create_init>();
    mb.addLibFunc<bt_value_string_get>();
    mb.addLibFunc<bt_value_string_set>();

    mb.addLibFunc<bt_value_array_create>();
    mb.addLibFunc<bt_value_array_get_length>();
    mb.addLibFunc<bt_value_array_borrow_element_by_index>();
    mb.addLibFunc<bt_value_array_borrow_element_by_index_const>();
    mb.addLibFunc<bt_value_array_append_element>();
    mb.addLibFunc<bt_value_array_set_element_by_index>();

    mb.addLibFunc<bt_value_map_create>();
    mb.addLibFunc<bt_value_map_get_size>();
    mb.addLibBoolRetFunc<bt_value_map_has_entry>();
    mb.addLibFunc<bt_value_map_borrow_entry_value>();
    mb.addLibFunc<bt_value_map_borrow_entry_value_const>();
    mb.addLibFunc<bt_value_map_insert_entry>();
    mb.addFunc<getMapValueKeys>("value_map_get_keys");
}

} /* namespace bt2py */
