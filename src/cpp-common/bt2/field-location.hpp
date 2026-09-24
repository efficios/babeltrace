/*
 * Copyright (c) 2022 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_FIELD_LOCATION_HPP
#define BABELTRACE_CPP_COMMON_BT2_FIELD_LOCATION_HPP

#include <cstdint>

#include <babeltrace2/babeltrace.h>

#include "cpp-common/bt2c/c-string-view.hpp"
#include "cpp-common/vendor/wise-enum/wise_enum.h"

#include "borrowed-object.hpp"
#include "shared-object.hpp"

namespace bt2 {
namespace internal {

struct FieldLocationRefFuncs final
{
    static void get(const bt_field_location * const libObjPtr)
    {
        bt_field_location_get_ref(libObjPtr);
    }

    static void put(const bt_field_location * const libObjPtr)
    {
        bt_field_location_put_ref(libObjPtr);
    }
};

} /* namespace internal */

class ConstFieldLocation : public BorrowedObject<const bt_field_location>
{
public:
    using Shared =
        SharedObject<ConstFieldLocation, const bt_field_location, internal::FieldLocationRefFuncs>;

    /* clang-format off */

    WISE_ENUM_CLASS_MEMBER(Scope,
        (PacketContext,         BT_FIELD_LOCATION_SCOPE_PACKET_CONTEXT),
        (EventCommonContext,    BT_FIELD_LOCATION_SCOPE_EVENT_COMMON_CONTEXT),
        (EventSpecificContext,  BT_FIELD_LOCATION_SCOPE_EVENT_SPECIFIC_CONTEXT),
        (EventPayload,          BT_FIELD_LOCATION_SCOPE_EVENT_PAYLOAD))

    /* clang-format on */

    explicit ConstFieldLocation(const LibObjPtr libObjPtr) noexcept
        : _ThisBorrowedObject {libObjPtr}
    {
    }

    Scope rootScope() const noexcept
    {
        return static_cast<Scope>(bt_field_location_get_root_scope(this->libObjPtr()));
    }

    std::uint64_t length() const noexcept
    {
        return bt_field_location_get_item_count(this->libObjPtr());
    }

    bt2c::CStringView operator[](const std::uint64_t index) const noexcept
    {
        return bt_field_location_get_item_by_index(this->libObjPtr(), index);
    }

    Shared shared() const noexcept
    {
        return Shared::createWithRef(*this);
    }
};

} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_FIELD_LOCATION_HPP */
