/*
 * Copyright (c) 2023 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_SELF_COMPONENT_CLASS_HPP
#define BABELTRACE_CPP_COMMON_BT2_SELF_COMPONENT_CLASS_HPP

#include <babeltrace2/babeltrace.h>

#include "cpp-common/bt2c/c-string-view.hpp"

#include "borrowed-object.hpp"

namespace bt2 {

class SelfComponentClass final : public BorrowedObject<bt_self_component_class>
{
public:
    explicit SelfComponentClass(const LibObjPtr libObjPtr) noexcept :
        _ThisBorrowedObject {libObjPtr}
    {
    }

    explicit SelfComponentClass(bt_self_component_class_source * const libObjPtr) noexcept :
        _ThisBorrowedObject {bt_self_component_class_source_as_self_component_class(libObjPtr)}
    {
    }

    explicit SelfComponentClass(bt_self_component_class_filter * const libObjPtr) noexcept :
        _ThisBorrowedObject {bt_self_component_class_filter_as_self_component_class(libObjPtr)}
    {
    }

    explicit SelfComponentClass(bt_self_component_class_sink * const libObjPtr) noexcept :
        _ThisBorrowedObject {bt_self_component_class_sink_as_self_component_class(libObjPtr)}
    {
    }

    bt2c::CStringView name() const noexcept
    {
        return bt_component_class_get_name(this->_libCompClsPtr());
    }

    bt2c::CStringView description() const noexcept
    {
        return bt_component_class_get_description(this->_libCompClsPtr());
    }

    bt2c::CStringView help() const noexcept
    {
        return bt_component_class_get_help(this->_libCompClsPtr());
    }

private:
    const bt_component_class *_libCompClsPtr() const noexcept
    {
        return bt_self_component_class_as_component_class(this->libObjPtr());
    }
};

} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_SELF_COMPONENT_CLASS_HPP */
