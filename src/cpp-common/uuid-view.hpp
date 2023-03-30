/*
 * Copyright (c) 2020 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_UUID_VIEW_HPP
#define BABELTRACE_CPP_COMMON_UUID_VIEW_HPP

#include <array>
#include <cstdint>
#include <string>
#include <algorithm>

#include "common/assert.h"
#include "common/uuid.h"

namespace bt2_common {

class Uuid;

/*
 * A view on existing UUID data.
 *
 * A `UuidView` object doesn't contain its UUID data: see `Uuid` for a
 * UUID data container.
 */
class UuidView final
{
public:
    using Val = std::uint8_t;
    using ConstIter = const Val *;

public:
    explicit UuidView(const Val * const uuid) noexcept : _mUuid {uuid}
    {
        BT_ASSERT_DBG(uuid);
    }

    explicit UuidView(const Uuid& uuid) noexcept;
    UuidView(const UuidView&) noexcept = default;
    UuidView& operator=(const UuidView&) noexcept = default;

    UuidView& operator=(const Val * const uuid) noexcept
    {
        _mUuid = uuid;
        return *this;
    }

    operator Uuid() const noexcept;

    std::string str() const
    {
        std::string s;

        s.resize(BT_UUID_STR_LEN);
        bt_uuid_to_str(_mUuid, &s[0]);

        return s;
    }

    bool operator==(const UuidView& other) const noexcept
    {
        return bt_uuid_compare(_mUuid, other._mUuid) == 0;
    }

    bool operator!=(const UuidView& other) const noexcept
    {
        return !(*this == other);
    }

    bool operator<(const UuidView& other) const noexcept
    {
        return bt_uuid_compare(_mUuid, other._mUuid) < 0;
    }

    static constexpr std::size_t size() noexcept
    {
        return BT_UUID_LEN;
    }

    const Val *data() const noexcept
    {
        return _mUuid;
    }

    Val operator[](const std::size_t index) const noexcept
    {
        return _mUuid[index];
    }

    ConstIter begin() const noexcept
    {
        return _mUuid;
    }

    ConstIter end() const noexcept
    {
        return _mUuid + this->size();
    }

    bool isNil() const noexcept
    {
        return std::all_of(this->begin(), this->end(), [](const std::uint8_t byte) {
            return byte == 0;
        });
    }

private:
    const Val *_mUuid;
};

} /* namespace bt2_common */

#endif /* BABELTRACE_CPP_COMMON_UUID_VIEW_HPP */
