/*
 * Copyright (c) 2020 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_CLOCK_SNAPSHOT_HPP
#define BABELTRACE_CPP_COMMON_BT2_CLOCK_SNAPSHOT_HPP

#include <cstdint>

#include <babeltrace2/babeltrace.h>

#include "borrowed-object.hpp"
#include "exc.hpp"

namespace bt2 {

class ConstClockSnapshot final : public BorrowedObject<const bt_clock_snapshot>
{
public:
    explicit ConstClockSnapshot(const _LibObjPtr libObjPtr) noexcept :
        _ThisBorrowedObject {libObjPtr}
    {
    }

    ConstClockSnapshot(const ConstClockSnapshot& clkSnapshot) noexcept :
        _ThisBorrowedObject {clkSnapshot}
    {
    }

    ConstClockSnapshot& operator=(const ConstClockSnapshot& clkSnapshot) noexcept
    {
        _ThisBorrowedObject::operator=(clkSnapshot);
        return *this;
    }

    std::uint64_t value() const noexcept
    {
        return bt_clock_snapshot_get_value(this->libObjPtr());
    }

    operator std::uint64_t() const noexcept
    {
        return this->value();
    }

    std::int64_t nsFromOrigin() const
    {
        std::int64_t nsFromOrigin;
        const auto status = bt_clock_snapshot_get_ns_from_origin(this->libObjPtr(), &nsFromOrigin);

        if (status == BT_CLOCK_SNAPSHOT_GET_NS_FROM_ORIGIN_STATUS_OVERFLOW_ERROR) {
            throw OverflowError {};
        }

        return nsFromOrigin;
    }
};

} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_CLOCK_SNAPSHOT_HPP */
