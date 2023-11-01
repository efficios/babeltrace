/*
 * Copyright (c) 2023 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_MESSAGE_ITERATOR_HPP
#define BABELTRACE_CPP_COMMON_BT2_MESSAGE_ITERATOR_HPP

#include <babeltrace2/babeltrace.h>

#include "common/common.h"
#include "cpp-common/optional.hpp"

#include "component-port.hpp"
#include "exc.hpp"
#include "message-array.hpp"
#include "shared-object.hpp"

namespace bt2 {
namespace internal {

struct MessageIteratorRefFuncs final
{
    static void get(const bt_message_iterator * const libObjPtr) noexcept
    {
        bt_message_iterator_get_ref(libObjPtr);
    }

    static void put(const bt_message_iterator * const libObjPtr) noexcept
    {
        bt_message_iterator_put_ref(libObjPtr);
    }
};

} /* namespace internal */

class MessageIterator final : public BorrowedObject<bt_message_iterator>
{
public:
    using Shared =
        SharedObject<MessageIterator, bt_message_iterator, internal::MessageIteratorRefFuncs>;

    explicit MessageIterator(const _LibObjPtr libObjPtr) noexcept : _ThisBorrowedObject {libObjPtr}
    {
    }

    ConstComponent component() const noexcept
    {
        return ConstComponent {bt_message_iterator_borrow_component(this->libObjPtr())};
    }

    nonstd::optional<ConstMessageArray> next() const
    {
        bt_message_array_const libMsgsPtr;
        std::uint64_t count;
        const auto status = bt_message_iterator_next(this->libObjPtr(), &libMsgsPtr, &count);

        switch (status) {
        case BT_MESSAGE_ITERATOR_NEXT_STATUS_OK:
            /* Caller becomes the owner of the contained messages */
            return ConstMessageArray::wrapExisting(libMsgsPtr, count);
        case BT_MESSAGE_ITERATOR_NEXT_STATUS_END:
            return nonstd::nullopt;
        case BT_MESSAGE_ITERATOR_NEXT_STATUS_AGAIN:
            throw TryAgain {};
        case BT_MESSAGE_ITERATOR_NEXT_STATUS_MEMORY_ERROR:
            throw MemoryError {};
        case BT_MESSAGE_ITERATOR_NEXT_STATUS_ERROR:
            throw Error {};
        default:
            bt_common_abort();
        }
    }

    bool canSeekBeginning() const
    {
        bt_bool canSeek;

        const auto status = bt_message_iterator_can_seek_beginning(this->libObjPtr(), &canSeek);

        switch (status) {
        case BT_MESSAGE_ITERATOR_CAN_SEEK_BEGINNING_STATUS_OK:
            return static_cast<bool>(canSeek);
        case BT_MESSAGE_ITERATOR_CAN_SEEK_BEGINNING_STATUS_AGAIN:
            throw TryAgain {};
        case BT_MESSAGE_ITERATOR_CAN_SEEK_BEGINNING_STATUS_MEMORY_ERROR:
            throw MemoryError {};
        case BT_MESSAGE_ITERATOR_CAN_SEEK_BEGINNING_STATUS_ERROR:
            throw Error {};
        default:
            bt_common_abort();
        }
    }

    void seekBeginning() const
    {
        const auto status = bt_message_iterator_seek_beginning(this->libObjPtr());

        switch (status) {
        case BT_MESSAGE_ITERATOR_SEEK_BEGINNING_STATUS_OK:
            return;
        case BT_MESSAGE_ITERATOR_SEEK_BEGINNING_STATUS_AGAIN:
            throw TryAgain {};
        case BT_MESSAGE_ITERATOR_SEEK_BEGINNING_STATUS_MEMORY_ERROR:
            throw MemoryError {};
        case BT_MESSAGE_ITERATOR_SEEK_BEGINNING_STATUS_ERROR:
            throw Error {};
        default:
            bt_common_abort();
        }
    }

    bool canSeekNsFromOrigin(const std::int64_t nsFromOrigin) const
    {
        bt_bool canSeek;

        const auto status =
            bt_message_iterator_can_seek_ns_from_origin(this->libObjPtr(), nsFromOrigin, &canSeek);

        switch (status) {
        case BT_MESSAGE_ITERATOR_CAN_SEEK_NS_FROM_ORIGIN_STATUS_OK:
            return static_cast<bool>(canSeek);
        case BT_MESSAGE_ITERATOR_CAN_SEEK_NS_FROM_ORIGIN_STATUS_AGAIN:
            throw TryAgain {};
        case BT_MESSAGE_ITERATOR_CAN_SEEK_NS_FROM_ORIGIN_STATUS_MEMORY_ERROR:
            throw MemoryError {};
        case BT_MESSAGE_ITERATOR_CAN_SEEK_NS_FROM_ORIGIN_STATUS_ERROR:
            throw Error {};
        default:
            bt_common_abort();
        }
    }

    void seekNsFromOrigin(const std::int64_t nsFromOrigin) const
    {
        const auto status =
            bt_message_iterator_seek_ns_from_origin(this->libObjPtr(), nsFromOrigin);

        switch (status) {
        case BT_MESSAGE_ITERATOR_SEEK_NS_FROM_ORIGIN_STATUS_OK:
            return;
        case BT_MESSAGE_ITERATOR_SEEK_NS_FROM_ORIGIN_STATUS_AGAIN:
            throw TryAgain {};
        case BT_MESSAGE_ITERATOR_SEEK_NS_FROM_ORIGIN_STATUS_MEMORY_ERROR:
            throw MemoryError {};
        case BT_MESSAGE_ITERATOR_SEEK_NS_FROM_ORIGIN_STATUS_ERROR:
            throw Error {};
        default:
            bt_common_abort();
        }
    }

    bool canSeekForward() const noexcept
    {
        return static_cast<bool>(bt_message_iterator_can_seek_forward(this->libObjPtr()));
    }

    Shared shared() const noexcept
    {
        return Shared::createWithRef(*this);
    }
};

} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_MESSAGE_ITERATOR_HPP */
