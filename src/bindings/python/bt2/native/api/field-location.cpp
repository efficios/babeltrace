/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#include <Python.h>

#include <vector>

#include <babeltrace2/babeltrace.h>

#include "common/assert.h"
#include "cpp-common/bt2/field-location.hpp"
#include "cpp-common/bt2/trace-ir.hpp"

#include "../api.hpp"
#include "../runtime/py-conv.hpp"
#include "../runtime/py-module-builder.hpp"

namespace bt2py {
namespace {

/*
 * Creates a field location within `tc` having the root scope
 * `rootScope` and the items of the Python list of strings `pyItems`.
 *
 */
bt2::ConstFieldLocation::Shared createFieldLoc(const bt2::TraceClass tc,
                                               const bt2::ConstFieldLocation::Scope rootScope,
                                               PyObject * const pyItems)
{
    BT_ASSERT_DBG(PyList_Check(pyItems));

    const auto itemCount = PyList_GET_SIZE(pyItems);
    std::vector<const char *> items;

    items.reserve(static_cast<std::size_t>(itemCount));

    /*
     * The UTF-8 buffers remain valid for the duration of the call since
     * `pyItems` holds a reference to each string.
     */
    for (Py_ssize_t i = 0; i < itemCount; ++i) {
        items.push_back(internal::pyStrUtf8(*PyList_GET_ITEM(pyItems, i)));
    }

    return tc.createFieldLocation(rootScope, items);
}

} /* namespace */

void registerFieldLocationApi(PyModuleBuilder& mb)
{
    mb.addWiseEnumConstants<bt2::ConstFieldLocation::Scope>("FIELD_LOCATION_SCOPE_");

    mb.addFunc<createFieldLoc>("field_location_create");
    mb.addLibFunc<bt_field_location_get_ref>();
    mb.addLibFunc<bt_field_location_put_ref>();
    mb.addLibFunc<bt_field_location_get_root_scope>();
    mb.addLibFunc<bt_field_location_get_item_count>();
    mb.addLibFunc<bt_field_location_get_item_by_index>();
}

} /* namespace bt2py */
