/*
 * Copyright (c) 2016-2022 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "uuid.hpp"
#include "uuid-view.hpp"

namespace bt2_common {

UuidView::UuidView(const Uuid& uuid) noexcept : _mUuid {uuid.data()}
{
}

UuidView::operator Uuid() const noexcept
{
    return Uuid {*this};
}

} /* namespace bt2_common */
