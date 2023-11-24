/*
 * Copyright (c) 2023 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_SELF_MESSAGE_ITERATOR_CONFIGURATION_HPP
#define BABELTRACE_CPP_COMMON_BT2_SELF_MESSAGE_ITERATOR_CONFIGURATION_HPP

#include <babeltrace2/babeltrace.h>

#include "borrowed-object.hpp"

namespace bt2 {

class SelfMessageIteratorConfiguration final :
    public BorrowedObject<bt_self_message_iterator_configuration>
{
public:
    explicit SelfMessageIteratorConfiguration(const LibObjPtr libObjPtr) noexcept :
        _ThisBorrowedObject {libObjPtr}
    {
    }

    void canSeekForward(const bool canSeekForward) const noexcept
    {
        bt_self_message_iterator_configuration_set_can_seek_forward(
            this->libObjPtr(), static_cast<bt_bool>(canSeekForward));
    }
};

} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_SELF_MESSAGE_ITERATOR_CONFIGURATION_HPP */
