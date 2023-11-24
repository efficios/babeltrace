/*
 * Copyright (c) 2023 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_SELF_MESSAGE_ITERATOR_HPP
#define BABELTRACE_CPP_COMMON_BT2_SELF_MESSAGE_ITERATOR_HPP

#include <babeltrace2/babeltrace.h>

#include "common/assert.h"
#include "common/common.h"

#include "borrowed-object.hpp"
#include "message-iterator.hpp"
#include "self-component-port.hpp"

namespace bt2 {

class SelfMessageIterator final : public BorrowedObject<bt_self_message_iterator>
{
public:
    explicit SelfMessageIterator(const LibObjPtr libObjPtr) noexcept :
        _ThisBorrowedObject {libObjPtr}
    {
    }

    MessageIterator::Shared createMessageIterator(const SelfComponentInputPort port) const
    {
        bt_message_iterator *libMsgIterPtr = nullptr;
        const auto status = bt_message_iterator_create_from_message_iterator(
            this->libObjPtr(), port.libObjPtr(), &libMsgIterPtr);

        switch (status) {
        case BT_MESSAGE_ITERATOR_CREATE_FROM_MESSAGE_ITERATOR_STATUS_OK:
            BT_ASSERT(libMsgIterPtr);
            return MessageIterator::Shared::createWithoutRef(libMsgIterPtr);
        case BT_MESSAGE_ITERATOR_CREATE_FROM_MESSAGE_ITERATOR_STATUS_MEMORY_ERROR:
            throw MemoryError {};
        case BT_MESSAGE_ITERATOR_CREATE_FROM_MESSAGE_ITERATOR_STATUS_ERROR:
            throw Error {};
        default:
            bt_common_abort();
        }
    }

    SelfComponent component() const noexcept
    {
        return SelfComponent {bt_self_message_iterator_borrow_component(this->libObjPtr())};
    }

    SelfComponentOutputPort port() const noexcept
    {
        return SelfComponentOutputPort {bt_self_message_iterator_borrow_port(this->libObjPtr())};
    }

    bool isInterrupted() const noexcept
    {
        return static_cast<bool>(bt_self_message_iterator_is_interrupted(this->libObjPtr()));
    }

    template <typename T>
    T& data() const noexcept
    {
        return *static_cast<T *>(bt_self_message_iterator_get_data(this->libObjPtr()));
    }

    template <typename T>
    void data(T& obj) const noexcept
    {
        bt_self_message_iterator_set_data(this->libObjPtr(), static_cast<void *>(&obj));
    }
};

} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_SELF_MESSAGE_ITERATOR_HPP */
