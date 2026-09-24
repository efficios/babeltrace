/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#include <Python.h>

#include <babeltrace2/babeltrace.h>

#include "cpp-common/bt2/field-path.hpp"

#include "../api.hpp"
#include "../runtime/py-module-builder.hpp"

namespace bt2py {

void registerFieldPathApi(PyModuleBuilder& mb)
{
    mb.addWiseEnumConstants<bt2::FieldPathScope>("FIELD_PATH_SCOPE_");
    mb.addWiseEnumConstants<bt2::FieldPathItemType>("FIELD_PATH_ITEM_TYPE_");

    mb.addLibFunc<bt_field_path_get_ref>();
    mb.addLibFunc<bt_field_path_put_ref>();
    mb.addLibFunc<bt_field_path_get_root_scope>();
    mb.addLibFunc<bt_field_path_get_item_count>();
    mb.addLibFunc<bt_field_path_borrow_item_by_index_const>();

    mb.addLibFunc<bt_field_path_item_get_type>();
    mb.addLibFunc<bt_field_path_item_index_get_index>();
}

} /* namespace bt2py */
