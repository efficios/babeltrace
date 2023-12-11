/*
 * Copyright (c) 2022 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_ENDIAN_HPP
#define BABELTRACE_CPP_COMMON_ENDIAN_HPP

#include <cstdint>

#include "compat/endian.h" /* IWYU pragma: keep  */

namespace bt2_common {

static inline std::uint8_t littleEndianToNative(const std::uint8_t val) noexcept
{
    return val;
}

static inline std::uint8_t bigEndianToNative(const std::uint8_t val) noexcept
{
    return val;
}

static inline std::int8_t littleEndianToNative(const std::int8_t val) noexcept
{
    return val;
}

static inline std::int8_t bigEndianToNative(const std::int8_t val) noexcept
{
    return val;
}

static inline std::uint16_t littleEndianToNative(const std::uint16_t val) noexcept
{
    return static_cast<std::uint16_t>(le16toh(val));
}

static inline std::uint16_t bigEndianToNative(const std::uint16_t val) noexcept
{
    return static_cast<std::uint16_t>(be16toh(val));
}

static inline std::int16_t littleEndianToNative(const std::int16_t val) noexcept
{
    return static_cast<std::int16_t>(littleEndianToNative(static_cast<std::uint16_t>(val)));
}

static inline std::int16_t bigEndianToNative(const std::int16_t val) noexcept
{
    return static_cast<std::int16_t>(bigEndianToNative(static_cast<std::uint16_t>(val)));
}

static inline std::uint32_t littleEndianToNative(const std::uint32_t val) noexcept
{
    return static_cast<std::uint32_t>(le32toh(val));
}

static inline std::uint32_t bigEndianToNative(const std::uint32_t val) noexcept
{
    return static_cast<std::uint32_t>(be32toh(val));
}

static inline std::int32_t littleEndianToNative(const std::int32_t val) noexcept
{
    return static_cast<std::int32_t>(littleEndianToNative(static_cast<std::uint32_t>(val)));
}

static inline std::int32_t bigEndianToNative(const std::int32_t val) noexcept
{
    return static_cast<std::int32_t>(bigEndianToNative(static_cast<std::uint32_t>(val)));
}

static inline std::uint64_t littleEndianToNative(const std::uint64_t val) noexcept
{
    return static_cast<std::uint64_t>(le64toh(val));
}

static inline std::uint64_t bigEndianToNative(const std::uint64_t val) noexcept
{
    return static_cast<std::uint64_t>(be64toh(val));
}

static inline std::int64_t littleEndianToNative(const std::int64_t val) noexcept
{
    return static_cast<std::int64_t>(littleEndianToNative(static_cast<std::uint64_t>(val)));
}

static inline std::int64_t bigEndianToNative(const std::int64_t val) noexcept
{
    return static_cast<std::int64_t>(bigEndianToNative(static_cast<std::uint64_t>(val)));
}

} /* namespace bt2_common */

#endif /* BABELTRACE_CPP_COMMON_ENDIAN_HPP */
