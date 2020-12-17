/*
 * Copyright (c) 2020 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_CLOCK_SNAPSHOT_HPP
#define BABELTRACE_CPP_COMMON_BT2_CLOCK_SNAPSHOT_HPP

#include <cstdint>
#include <babeltrace2/babeltrace.h>

#include "internal/borrowed-obj.hpp"
#include "lib-error.hpp"

namespace bt2 {

class ConstClockSnapshot final : public internal::BorrowedObj<const bt_clock_snapshot>
{
public:
    explicit ConstClockSnapshot(const _LibObjPtr libObjPtr) noexcept : _ThisBorrowedObj {libObjPtr}
    {
    }

    ConstClockSnapshot(const ConstClockSnapshot& clkSnapshot) noexcept :
        _ThisBorrowedObj {clkSnapshot}
    {
    }

    ConstClockSnapshot& operator=(const ConstClockSnapshot& clkSnapshot) noexcept
    {
        _ThisBorrowedObj::operator=(clkSnapshot);
        return *this;
    }

    std::uint64_t value() const noexcept
    {
        return bt_clock_snapshot_get_value(this->_libObjPtr());
    }

    operator std::uint64_t() const noexcept
    {
        return this->value();
    }

    std::int64_t nsFromOrigin() const
    {
        std::int64_t nsFromOrigin;
        const auto status = bt_clock_snapshot_get_ns_from_origin(this->_libObjPtr(), &nsFromOrigin);

        if (status == BT_CLOCK_SNAPSHOT_GET_NS_FROM_ORIGIN_STATUS_OVERFLOW_ERROR) {
            throw LibOverflowError {};
        }

        return nsFromOrigin;
    }
};

} // namespace bt2

#endif // BABELTRACE_CPP_COMMON_BT2_CLOCK_SNAPSHOT_HPP
