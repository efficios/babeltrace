/*
 * Copyright (c) 2020 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_UUID_VIEW_HPP
#define BABELTRACE_CPP_COMMON_UUID_VIEW_HPP

#include <cstdint>

#include "common/assert.h"
#include "common/uuid.h"

namespace bt2_common {

class UuidView
{
public:
    explicit UuidView(const std::uint8_t * const uuid) noexcept : _mUuid {uuid}
    {
        BT_ASSERT_DBG(uuid);
    }

    UuidView(const UuidView&) noexcept = default;
    UuidView& operator=(const UuidView&) noexcept = default;

    bool operator==(const UuidView& other) const noexcept
    {
        return bt_uuid_compare(_mUuid, other._mUuid) == 0;
    }

    bool operator!=(const UuidView& other) const noexcept
    {
        return !(*this == other);
    }

    std::string string() const
    {
        std::array<char, BT_UUID_STR_LEN> buf;

        bt_uuid_to_str(_mUuid, buf.data());
        return {buf.data(), buf.size()};
    }

    static std::size_t size() noexcept
    {
        return BT_UUID_LEN;
    }

    const std::uint8_t *data() const noexcept
    {
        return _mUuid;
    }

private:
    const std::uint8_t *_mUuid;
};

} // namespace bt2_common

#endif // BABELTRACE_CPP_COMMON_UUID_VIEW_HPP
