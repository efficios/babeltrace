/*
 * Copyright (c) 2022 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "common/assert.h"

#include "medium.hpp"

namespace ctf::src {

Buf::Buf(const std::uint8_t * const addr, const bt2c::DataLen size) noexcept
    : _mAddr {addr},
      _mSize {size}
{
    BT_ASSERT_DBG(!size.hasExtraBits());
}

Buf Buf::slice(const bt2c::DataLen offset) const noexcept
{
    BT_ASSERT_DBG(offset <= _mSize);
    BT_ASSERT_DBG(!offset.hasExtraBits());
    return Buf {_mAddr + offset.bytes(), _mSize.subDbg(offset)};
}

} /* namespace ctf::src */
