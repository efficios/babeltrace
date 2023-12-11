/*
 * Copyright (c) 2016-2022 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "uuid-view.hpp"
#include "uuid.hpp"

namespace bt2c {

UuidView::UuidView(const Uuid& uuid) noexcept : _mUuid {uuid.data()}
{
}

UuidView::operator Uuid() const noexcept
{
    return Uuid {*this};
}

} /* namespace bt2c */
