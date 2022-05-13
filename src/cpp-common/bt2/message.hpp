/*
 * Copyright (c) 2021 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_MESSAGE_HPP
#define BABELTRACE_CPP_COMMON_BT2_MESSAGE_HPP

#include <type_traits>
#include <cstdint>
#include <functional>
#include <babeltrace2/babeltrace.h>

#include "common/assert.h"
#include "common/common.h"
#include "internal/borrowed-obj.hpp"
#include "internal/shared-obj.hpp"
#include "internal/utils.hpp"
#include "cpp-common/optional.hpp"
#include "cpp-common/string_view.hpp"
#include "lib-error.hpp"

namespace bt2 {
namespace internal {

struct MessageRefFuncs final
{
    static void get(const bt_message * const libObjPtr)
    {
        bt_message_get_ref(libObjPtr);
    }

    static void put(const bt_message * const libObjPtr)
    {
        bt_message_put_ref(libObjPtr);
    }
};

template <typename ObjT, typename LibObjT>
using SharedMessage = internal::SharedObj<ObjT, LibObjT, internal::MessageRefFuncs>;

} /* namespace internal */

template <typename LibObjT>
class CommonStreamBeginningMessage;

template <typename LibObjT>
class CommonStreamEndMessage;

template <typename LibObjT>
class CommonEventMessage;

template <typename LibObjT>
class CommonPacketBeginningMessage;

template <typename LibObjT>
class CommonPacketEndMessage;

template <typename LibObjT>
class CommonDiscardedEventsMessage;

template <typename LibObjT>
class CommonDiscardedPacketsMessage;

template <typename LibObjT>
class CommonMessageIteratorInactivityMessage;

enum class MessageType
{
    STREAM_BEGINNING = BT_MESSAGE_TYPE_STREAM_BEGINNING,
    STREAM_END = BT_MESSAGE_TYPE_STREAM_END,
    EVENT = BT_MESSAGE_TYPE_EVENT,
    PACKET_BEGINNING = BT_MESSAGE_TYPE_PACKET_BEGINNING,
    PACKET_END = BT_MESSAGE_TYPE_PACKET_END,
    DISCARDED_EVENTS = BT_MESSAGE_TYPE_DISCARDED_EVENTS,
    DISCARDED_PACKETS = BT_MESSAGE_TYPE_DISCARDED_PACKETS,
    MESSAGE_ITERATOR_INACTIVITY = BT_MESSAGE_TYPE_MESSAGE_ITERATOR_INACTIVITY,
};

template <typename LibObjT>
class CommonMessage : public internal::BorrowedObj<LibObjT>
{
private:
    using typename internal::BorrowedObj<LibObjT>::_ThisBorrowedObj;

protected:
    using typename internal::BorrowedObj<LibObjT>::_LibObjPtr;
    using _ThisCommonMessage = CommonMessage<LibObjT>;

public:
    using Shared = internal::SharedMessage<CommonMessage<LibObjT>, LibObjT>;

    explicit CommonMessage(const _LibObjPtr libObjPtr) noexcept : _ThisBorrowedObj {libObjPtr}
    {
    }

    template <typename OtherLibObjT>
    CommonMessage(const CommonMessage<OtherLibObjT>& val) noexcept : _ThisBorrowedObj {val}
    {
    }

    template <typename OtherLibObjT>
    _ThisCommonMessage& operator=(const CommonMessage<OtherLibObjT>& val) noexcept
    {
        _ThisBorrowedObj::operator=(val);
        return *this;
    }

    MessageType type() const noexcept
    {
        return static_cast<MessageType>(bt_message_get_type(this->libObjPtr()));
    }

    bool isStreamBeginning() const noexcept
    {
        return this->type() == MessageType::STREAM_BEGINNING;
    }

    bool isStreamEnd() const noexcept
    {
        return this->type() == MessageType::STREAM_END;
    }

    bool isEvent() const noexcept
    {
        return this->type() == MessageType::EVENT;
    }

    bool isPacketBeginning() const noexcept
    {
        return this->type() == MessageType::PACKET_BEGINNING;
    }

    bool isPacketEnd() const noexcept
    {
        return this->type() == MessageType::PACKET_END;
    }

    bool isDiscardedEvents() const noexcept
    {
        return this->type() == MessageType::DISCARDED_EVENTS;
    }

    bool isDiscardedPackets() const noexcept
    {
        return this->type() == MessageType::DISCARDED_PACKETS;
    }

    bool isMessageIteratorInactivity() const noexcept
    {
        return this->type() == MessageType::MESSAGE_ITERATOR_INACTIVITY;
    }

    Shared shared() const noexcept
    {
        return Shared::createWithRef(*this);
    }

    CommonStreamBeginningMessage<LibObjT> asStreamBeginning() const noexcept;
    CommonStreamEndMessage<LibObjT> asStreamEnd() const noexcept;
    CommonEventMessage<LibObjT> asEvent() const noexcept;
    CommonPacketBeginningMessage<LibObjT> asPacketBeginning() const noexcept;
    CommonPacketEndMessage<LibObjT> asPacketEnd() const noexcept;
    CommonDiscardedEventsMessage<LibObjT> asDiscardedEvents() const noexcept;
    CommonDiscardedPacketsMessage<LibObjT> asDiscardedPackets() const noexcept;
    CommonMessageIteratorInactivityMessage<LibObjT> asMessageIteratorInactivity() const noexcept;
};

using Message = CommonMessage<bt_message>;
using ConstMessage = CommonMessage<const bt_message>;

namespace internal {

struct MessageTypeDescr
{
    using Const = ConstMessage;
    using NonConst = Message;
};

template <>
struct TypeDescr<Message> : public MessageTypeDescr
{
};

template <>
struct TypeDescr<ConstMessage> : public MessageTypeDescr
{
};

template <typename LibObjT>
struct CommonStreamBeginningMessageSpec;

/* Functions specific to mutable stream beginning messages */
template <>
struct CommonStreamBeginningMessageSpec<bt_message> final
{
    static bt_stream *stream(bt_message * const libObjPtr) noexcept
    {
        return bt_message_stream_beginning_borrow_stream(libObjPtr);
    }
};

/* Functions specific to constant stream beginning messages */
template <>
struct CommonStreamBeginningMessageSpec<const bt_message> final
{
    static const bt_stream *stream(const bt_message * const libObjPtr) noexcept
    {
        return bt_message_stream_beginning_borrow_stream_const(libObjPtr);
    }
};

} /* namespace internal */

template <typename LibObjT>
class CommonStreamBeginningMessage final : public CommonMessage<LibObjT>
{
private:
    using typename CommonMessage<LibObjT>::_LibObjPtr;
    using typename CommonMessage<LibObjT>::_ThisCommonMessage;

    using _Stream =
        typename std::conditional<std::is_const<LibObjT>::value, CommonStream<const bt_stream>,
                                  CommonStream<bt_stream>>::type;

public:
    using Shared = internal::SharedMessage<CommonStreamBeginningMessage<LibObjT>, LibObjT>;

    explicit CommonStreamBeginningMessage(const _LibObjPtr libObjPtr) noexcept :
        _ThisCommonMessage {libObjPtr}
    {
        BT_ASSERT_DBG(this->isStreamBeginning());
    }

    template <typename OtherLibObjT>
    CommonStreamBeginningMessage(const CommonStreamBeginningMessage<OtherLibObjT>& val) noexcept :
        _ThisCommonMessage {val}
    {
    }

    template <typename OtherLibObjT>
    CommonStreamBeginningMessage<LibObjT>&
    operator=(const CommonStreamBeginningMessage<OtherLibObjT>& val) noexcept
    {
        _ThisCommonMessage::operator=(val);
        return *this;
    }

    ConstStream stream() const noexcept
    {
        return ConstStream {internal::CommonStreamBeginningMessageSpec<const bt_message>::stream(
            this->libObjPtr())};
    }

    _Stream stream() noexcept
    {
        return _Stream {
            internal::CommonStreamBeginningMessageSpec<LibObjT>::stream(this->libObjPtr())};
    }

    void defaultClockSnapshot(const std::uint64_t val) noexcept
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        bt_message_stream_beginning_set_default_clock_snapshot(this->libObjPtr(), val);
    }

    nonstd::optional<ConstClockSnapshot> defaultClockSnapshot() const noexcept
    {
        const bt_clock_snapshot *libObjPtr;
        const auto state = bt_message_stream_beginning_borrow_default_clock_snapshot_const(
            this->libObjPtr(), &libObjPtr);

        if (state == BT_MESSAGE_STREAM_CLOCK_SNAPSHOT_STATE_KNOWN) {
            return ConstClockSnapshot {libObjPtr};
        }

        return nonstd::nullopt;
    }

    Shared shared() const noexcept
    {
        return Shared::createWithRef(*this);
    }
};

using StreamBeginningMessage = CommonStreamBeginningMessage<bt_message>;
using ConstStreamBeginningMessage = CommonStreamBeginningMessage<const bt_message>;

namespace internal {

struct StreamBeginningMessageTypeDescr
{
    using Const = ConstStreamBeginningMessage;
    using NonConst = StreamBeginningMessage;
};

template <>
struct TypeDescr<StreamBeginningMessage> : public StreamBeginningMessageTypeDescr
{
};

template <>
struct TypeDescr<ConstStreamBeginningMessage> : public StreamBeginningMessageTypeDescr
{
};

template <typename LibObjT>
struct CommonStreamEndMessageSpec;

/* Functions specific to mutable stream end messages */
template <>
struct CommonStreamEndMessageSpec<bt_message> final
{
    static bt_stream *stream(bt_message * const libObjPtr) noexcept
    {
        return bt_message_stream_end_borrow_stream(libObjPtr);
    }
};

/* Functions specific to constant stream end messages */
template <>
struct CommonStreamEndMessageSpec<const bt_message> final
{
    static const bt_stream *stream(const bt_message * const libObjPtr) noexcept
    {
        return bt_message_stream_end_borrow_stream_const(libObjPtr);
    }
};

} /* namespace internal */

template <typename LibObjT>
class CommonStreamEndMessage final : public CommonMessage<LibObjT>
{
private:
    using typename CommonMessage<LibObjT>::_LibObjPtr;
    using typename CommonMessage<LibObjT>::_ThisCommonMessage;

    using _Stream =
        typename std::conditional<std::is_const<LibObjT>::value, CommonStream<const bt_stream>,
                                  CommonStream<bt_stream>>::type;

public:
    using Shared = internal::SharedMessage<CommonStreamEndMessage<LibObjT>, LibObjT>;

    explicit CommonStreamEndMessage(const _LibObjPtr libObjPtr) noexcept :
        _ThisCommonMessage {libObjPtr}
    {
        BT_ASSERT_DBG(this->isStreamEnd());
    }

    template <typename OtherLibObjT>
    CommonStreamEndMessage(const CommonStreamEndMessage<OtherLibObjT>& val) noexcept :
        _ThisCommonMessage {val}
    {
    }

    template <typename OtherLibObjT>
    CommonStreamEndMessage<LibObjT>&
    operator=(const CommonStreamEndMessage<OtherLibObjT>& val) noexcept
    {
        _ThisCommonMessage::operator=(val);
        return *this;
    }

    ConstStream stream() const noexcept
    {
        return ConstStream {
            internal::CommonStreamEndMessageSpec<const bt_message>::stream(this->libObjPtr())};
    }

    _Stream stream() noexcept
    {
        return _Stream {internal::CommonStreamEndMessageSpec<LibObjT>::stream(this->libObjPtr())};
    }

    void defaultClockSnapshot(const std::uint64_t val) noexcept
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        bt_message_stream_end_set_default_clock_snapshot(this->libObjPtr(), val);
    }

    nonstd::optional<ConstClockSnapshot> defaultClockSnapshot() const noexcept
    {
        const bt_clock_snapshot *libObjPtr;
        const auto state = bt_message_stream_end_borrow_default_clock_snapshot_const(
            this->libObjPtr(), &libObjPtr);

        if (state == BT_MESSAGE_STREAM_CLOCK_SNAPSHOT_STATE_KNOWN) {
            return ConstClockSnapshot {libObjPtr};
        }

        return nonstd::nullopt;
    }

    Shared shared() const noexcept
    {
        return Shared::createWithRef(*this);
    }
};

using StreamEndMessage = CommonStreamEndMessage<bt_message>;
using ConstStreamEndMessage = CommonStreamEndMessage<const bt_message>;

namespace internal {

struct StreamEndMessageTypeDescr
{
    using Const = ConstStreamEndMessage;
    using NonConst = StreamEndMessage;
};

template <>
struct TypeDescr<StreamEndMessage> : public StreamEndMessageTypeDescr
{
};

template <>
struct TypeDescr<ConstStreamEndMessage> : public StreamEndMessageTypeDescr
{
};

template <typename LibObjT>
struct CommonPacketBeginningMessageSpec;

/* Functions specific to mutable packet beginning messages */
template <>
struct CommonPacketBeginningMessageSpec<bt_message> final
{
    static bt_packet *packet(bt_message * const libObjPtr) noexcept
    {
        return bt_message_packet_beginning_borrow_packet(libObjPtr);
    }
};

/* Functions specific to constant packet beginning messages */
template <>
struct CommonPacketBeginningMessageSpec<const bt_message> final
{
    static const bt_packet *packet(const bt_message * const libObjPtr) noexcept
    {
        return bt_message_packet_beginning_borrow_packet_const(libObjPtr);
    }
};

} /* namespace internal */

template <typename LibObjT>
class CommonPacketBeginningMessage final : public CommonMessage<LibObjT>
{
private:
    using typename CommonMessage<LibObjT>::_LibObjPtr;
    using typename CommonMessage<LibObjT>::_ThisCommonMessage;

    using _Packet =
        typename std::conditional<std::is_const<LibObjT>::value, CommonPacket<const bt_packet>,
                                  CommonPacket<bt_packet>>::type;

public:
    using Shared = internal::SharedMessage<CommonPacketBeginningMessage<LibObjT>, LibObjT>;

    explicit CommonPacketBeginningMessage(const _LibObjPtr libObjPtr) noexcept :
        _ThisCommonMessage {libObjPtr}
    {
        BT_ASSERT_DBG(this->isPacketBeginning());
    }

    template <typename OtherLibObjT>
    CommonPacketBeginningMessage(const CommonPacketBeginningMessage<OtherLibObjT>& val) noexcept :
        _ThisCommonMessage {val}
    {
    }

    template <typename OtherLibObjT>
    CommonPacketBeginningMessage<LibObjT>&
    operator=(const CommonPacketBeginningMessage<OtherLibObjT>& val) noexcept
    {
        _ThisCommonMessage::operator=(val);
        return *this;
    }

    ConstPacket packet() const noexcept
    {
        return ConstPacket {internal::CommonPacketBeginningMessageSpec<const bt_message>::packet(
            this->libObjPtr())};
    }

    _Packet packet() noexcept
    {
        return _Packet {
            internal::CommonPacketBeginningMessageSpec<LibObjT>::packet(this->libObjPtr())};
    }

    void defaultClockSnapshot(const std::uint64_t val) noexcept
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        bt_message_packet_beginning_set_default_clock_snapshot(this->libObjPtr(), val);
    }

    ConstClockSnapshot defaultClockSnapshot() const noexcept
    {
        const auto libObjPtr =
            bt_message_packet_beginning_borrow_default_clock_snapshot_const(this->libObjPtr());

        return ConstClockSnapshot {libObjPtr};
    }

    Shared shared() const noexcept
    {
        return Shared::createWithRef(*this);
    }
};

using PacketBeginningMessage = CommonPacketBeginningMessage<bt_message>;
using ConstPacketBeginningMessage = CommonPacketBeginningMessage<const bt_message>;

namespace internal {

struct PacketBeginningMessageTypeDescr
{
    using Const = ConstPacketBeginningMessage;
    using NonConst = PacketBeginningMessage;
};

template <>
struct TypeDescr<PacketBeginningMessage> : public PacketBeginningMessageTypeDescr
{
};

template <>
struct TypeDescr<ConstPacketBeginningMessage> : public PacketBeginningMessageTypeDescr
{
};

template <typename LibObjT>
struct CommonPacketEndMessageSpec;

/* Functions specific to mutable packet end messages */
template <>
struct CommonPacketEndMessageSpec<bt_message> final
{
    static bt_packet *packet(bt_message * const libObjPtr) noexcept
    {
        return bt_message_packet_end_borrow_packet(libObjPtr);
    }
};

/* Functions specific to constant packet end messages */
template <>
struct CommonPacketEndMessageSpec<const bt_message> final
{
    static const bt_packet *packet(const bt_message * const libObjPtr) noexcept
    {
        return bt_message_packet_end_borrow_packet_const(libObjPtr);
    }
};

} /* namespace internal */

template <typename LibObjT>
class CommonPacketEndMessage final : public CommonMessage<LibObjT>
{
private:
    using typename CommonMessage<LibObjT>::_LibObjPtr;
    using typename CommonMessage<LibObjT>::_ThisCommonMessage;

    using _Packet =
        typename std::conditional<std::is_const<LibObjT>::value, CommonPacket<const bt_packet>,
                                  CommonPacket<bt_packet>>::type;

public:
    using Shared = internal::SharedMessage<CommonPacketEndMessage<LibObjT>, LibObjT>;

    explicit CommonPacketEndMessage(const _LibObjPtr libObjPtr) noexcept :
        _ThisCommonMessage {libObjPtr}
    {
        BT_ASSERT_DBG(this->isPacketEnd());
    }

    template <typename OtherLibObjT>
    CommonPacketEndMessage(const CommonPacketEndMessage<OtherLibObjT>& val) noexcept :
        _ThisCommonMessage {val}
    {
    }

    template <typename OtherLibObjT>
    CommonPacketEndMessage<LibObjT>&
    operator=(const CommonPacketEndMessage<OtherLibObjT>& val) noexcept
    {
        _ThisCommonMessage::operator=(val);
        return *this;
    }

    ConstPacket packet() const noexcept
    {
        return ConstPacket {
            internal::CommonPacketEndMessageSpec<const bt_message>::packet(this->libObjPtr())};
    }

    _Packet packet() noexcept
    {
        return _Packet {internal::CommonPacketEndMessageSpec<LibObjT>::packet(this->libObjPtr())};
    }

    void defaultClockSnapshot(const std::uint64_t val) noexcept
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        bt_message_packet_end_set_default_clock_snapshot(this->libObjPtr(), val);
    }

    ConstClockSnapshot defaultClockSnapshot() const noexcept
    {
        const auto libObjPtr =
            bt_message_packet_end_borrow_default_clock_snapshot_const(this->libObjPtr());

        return ConstClockSnapshot {libObjPtr};
    }

    Shared shared() const noexcept
    {
        return Shared::createWithRef(*this);
    }
};

using PacketEndMessage = CommonPacketEndMessage<bt_message>;
using ConstPacketEndMessage = CommonPacketEndMessage<const bt_message>;

namespace internal {

struct PacketEndMessageTypeDescr
{
    using Const = ConstPacketEndMessage;
    using NonConst = PacketEndMessage;
};

template <>
struct TypeDescr<PacketEndMessage> : public PacketEndMessageTypeDescr
{
};

template <>
struct TypeDescr<ConstPacketEndMessage> : public PacketEndMessageTypeDescr
{
};

template <typename LibObjT>
struct CommonEventMessageSpec;

/* Functions specific to mutable event messages */
template <>
struct CommonEventMessageSpec<bt_message> final
{
    static bt_event *event(bt_message * const libObjPtr) noexcept
    {
        return bt_message_event_borrow_event(libObjPtr);
    }
};

/* Functions specific to constant event messages */
template <>
struct CommonEventMessageSpec<const bt_message> final
{
    static const bt_event *event(const bt_message * const libObjPtr) noexcept
    {
        return bt_message_event_borrow_event_const(libObjPtr);
    }
};

} /* namespace internal */

template <typename LibObjT>
class CommonEventMessage final : public CommonMessage<LibObjT>
{
private:
    using typename CommonMessage<LibObjT>::_LibObjPtr;
    using typename CommonMessage<LibObjT>::_ThisCommonMessage;

    using _Event =
        typename std::conditional<std::is_const<LibObjT>::value, CommonEvent<const bt_event>,
                                  CommonEvent<bt_event>>::type;

public:
    using Shared = internal::SharedMessage<CommonEventMessage<LibObjT>, LibObjT>;

    explicit CommonEventMessage(const _LibObjPtr libObjPtr) noexcept :
        _ThisCommonMessage {libObjPtr}
    {
        BT_ASSERT_DBG(this->isEvent());
    }

    template <typename OtherLibObjT>
    CommonEventMessage(const CommonEventMessage<OtherLibObjT>& val) noexcept :
        _ThisCommonMessage {val}
    {
    }

    template <typename OtherLibObjT>
    CommonEventMessage<LibObjT>& operator=(const CommonEventMessage<OtherLibObjT>& val) noexcept
    {
        _ThisCommonMessage::operator=(val);
        return *this;
    }

    ConstEvent event() const noexcept
    {
        return ConstEvent {
            internal::CommonEventMessageSpec<const bt_message>::event(this->libObjPtr())};
    }

    _Event event() noexcept
    {
        return _Event {internal::CommonEventMessageSpec<LibObjT>::event(this->libObjPtr())};
    }

    ConstClockSnapshot defaultClockSnapshot() const noexcept
    {
        const auto libObjPtr =
            bt_message_event_borrow_default_clock_snapshot_const(this->libObjPtr());

        return ConstClockSnapshot {libObjPtr};
    }

    Shared shared() const noexcept
    {
        return Shared::createWithRef(*this);
    }
};

using EventMessage = CommonEventMessage<bt_message>;
using ConstEventMessage = CommonEventMessage<const bt_message>;

namespace internal {

struct EventMessageTypeDescr
{
    using Const = ConstEventMessage;
    using NonConst = EventMessage;
};

template <>
struct TypeDescr<EventMessage> : public EventMessageTypeDescr
{
};

template <>
struct TypeDescr<ConstEventMessage> : public EventMessageTypeDescr
{
};

template <typename LibObjT>
struct CommonDiscardedEventsMessageSpec;

/* Functions specific to mutable discarded events messages */
template <>
struct CommonDiscardedEventsMessageSpec<bt_message> final
{
    static bt_stream *stream(bt_message * const libObjPtr) noexcept
    {
        return bt_message_discarded_events_borrow_stream(libObjPtr);
    }
};

/* Functions specific to constant discarded events messages */
template <>
struct CommonDiscardedEventsMessageSpec<const bt_message> final
{
    static const bt_stream *stream(const bt_message * const libObjPtr) noexcept
    {
        return bt_message_discarded_events_borrow_stream_const(libObjPtr);
    }
};

} /* namespace internal */

template <typename LibObjT>
class CommonDiscardedEventsMessage final : public CommonMessage<LibObjT>
{
private:
    using typename CommonMessage<LibObjT>::_LibObjPtr;
    using typename CommonMessage<LibObjT>::_ThisCommonMessage;

    using _Stream =
        typename std::conditional<std::is_const<LibObjT>::value, CommonStream<const bt_stream>,
                                  CommonStream<bt_stream>>::type;

public:
    using Shared = internal::SharedMessage<CommonDiscardedEventsMessage<LibObjT>, LibObjT>;

    explicit CommonDiscardedEventsMessage(const _LibObjPtr libObjPtr) noexcept :
        _ThisCommonMessage {libObjPtr}
    {
        BT_ASSERT_DBG(this->isDiscardedEvents());
    }

    template <typename OtherLibObjT>
    CommonDiscardedEventsMessage(const CommonDiscardedEventsMessage<OtherLibObjT>& val) noexcept :
        _ThisCommonMessage {val}
    {
    }

    template <typename OtherLibObjT>
    CommonDiscardedEventsMessage<LibObjT>&
    operator=(const CommonDiscardedEventsMessage<OtherLibObjT>& val) noexcept
    {
        _ThisCommonMessage::operator=(val);
        return *this;
    }

    ConstStream stream() const noexcept
    {
        return ConstStream {internal::CommonDiscardedEventsMessageSpec<const bt_message>::stream(
            this->libObjPtr())};
    }

    _Stream stream() noexcept
    {
        return _Stream {
            internal::CommonDiscardedEventsMessageSpec<LibObjT>::stream(this->libObjPtr())};
    }

    ConstClockSnapshot beginningDefaultClockSnapshot() const noexcept
    {
        const auto libObjPtr =
            bt_message_discarded_events_borrow_beginning_default_clock_snapshot_const(
                this->libObjPtr());

        return ConstClockSnapshot {libObjPtr};
    }

    ConstClockSnapshot endDefaultClockSnapshot() const noexcept
    {
        const auto libObjPtr =
            bt_message_discarded_events_borrow_end_default_clock_snapshot_const(this->libObjPtr());

        return ConstClockSnapshot {libObjPtr};
    }

    void count(const std::uint64_t count) noexcept
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        bt_message_discarded_events_set_count(this->libObjPtr(), count);
    }

    nonstd::optional<std::uint64_t> count() const noexcept
    {
        std::uint64_t count;
        const auto avail = bt_message_discarded_events_get_count(this->libObjPtr(), &count);

        if (avail) {
            return count;
        }

        return nonstd::nullopt;
    }

    Shared shared() const noexcept
    {
        return Shared::createWithRef(*this);
    }
};

using DiscardedEventsMessage = CommonDiscardedEventsMessage<bt_message>;
using ConstDiscardedEventsMessage = CommonDiscardedEventsMessage<const bt_message>;

namespace internal {

struct DiscardedEventsMessageTypeDescr
{
    using Const = ConstDiscardedEventsMessage;
    using NonConst = DiscardedEventsMessage;
};

template <>
struct TypeDescr<DiscardedEventsMessage> : public DiscardedEventsMessageTypeDescr
{
};

template <>
struct TypeDescr<ConstDiscardedEventsMessage> : public DiscardedEventsMessageTypeDescr
{
};

template <typename LibObjT>
struct CommonDiscardedPacketsMessageSpec;

/* Functions specific to mutable discarded packets messages */
template <>
struct CommonDiscardedPacketsMessageSpec<bt_message> final
{
    static bt_stream *stream(bt_message * const libObjPtr) noexcept
    {
        return bt_message_discarded_packets_borrow_stream(libObjPtr);
    }
};

/* Functions specific to constant discarded packets messages */
template <>
struct CommonDiscardedPacketsMessageSpec<const bt_message> final
{
    static const bt_stream *stream(const bt_message * const libObjPtr) noexcept
    {
        return bt_message_discarded_packets_borrow_stream_const(libObjPtr);
    }
};

} /* namespace internal */

template <typename LibObjT>
class CommonDiscardedPacketsMessage final : public CommonMessage<LibObjT>
{
private:
    using typename CommonMessage<LibObjT>::_LibObjPtr;
    using typename CommonMessage<LibObjT>::_ThisCommonMessage;

    using _Stream =
        typename std::conditional<std::is_const<LibObjT>::value, CommonStream<const bt_stream>,
                                  CommonStream<bt_stream>>::type;

public:
    using Shared = internal::SharedMessage<CommonDiscardedPacketsMessage<LibObjT>, LibObjT>;

    explicit CommonDiscardedPacketsMessage(const _LibObjPtr libObjPtr) noexcept :
        _ThisCommonMessage {libObjPtr}
    {
        BT_ASSERT_DBG(this->isDiscardedPackets());
    }

    template <typename OtherLibObjT>
    CommonDiscardedPacketsMessage(const CommonDiscardedPacketsMessage<OtherLibObjT>& val) noexcept :
        _ThisCommonMessage {val}
    {
    }

    template <typename OtherLibObjT>
    CommonDiscardedPacketsMessage<LibObjT>&
    operator=(const CommonDiscardedPacketsMessage<OtherLibObjT>& val) noexcept
    {
        _ThisCommonMessage::operator=(val);
        return *this;
    }

    ConstStream stream() const noexcept
    {
        return ConstStream {internal::CommonDiscardedPacketsMessageSpec<const bt_message>::stream(
            this->libObjPtr())};
    }

    _Stream stream() noexcept
    {
        return _Stream {
            internal::CommonDiscardedPacketsMessageSpec<LibObjT>::stream(this->libObjPtr())};
    }

    ConstClockSnapshot beginningDefaultClockSnapshot() const noexcept
    {
        const auto libObjPtr =
            bt_message_discarded_packets_borrow_beginning_default_clock_snapshot_const(
                this->libObjPtr());

        return ConstClockSnapshot {libObjPtr};
    }

    ConstClockSnapshot endDefaultClockSnapshot() const noexcept
    {
        const auto libObjPtr =
            bt_message_discarded_packets_borrow_end_default_clock_snapshot_const(this->libObjPtr());

        return ConstClockSnapshot {libObjPtr};
    }

    void count(const std::uint64_t count) noexcept
    {
        static_assert(!std::is_const<LibObjT>::value, "`LibObjT` must NOT be `const`.");

        bt_message_discarded_packets_set_count(this->libObjPtr(), count);
    }

    nonstd::optional<std::uint64_t> count() const noexcept
    {
        std::uint64_t count;
        const auto avail = bt_message_discarded_packets_get_count(this->libObjPtr(), &count);

        if (avail) {
            return count;
        }

        return nonstd::nullopt;
    }

    Shared shared() const noexcept
    {
        return Shared::createWithRef(*this);
    }
};

using DiscardedPacketsMessage = CommonDiscardedPacketsMessage<bt_message>;
using ConstDiscardedPacketsMessage = CommonDiscardedPacketsMessage<const bt_message>;

namespace internal {

struct DiscardedPacketsMessageTypeDescr
{
    using Const = ConstDiscardedPacketsMessage;
    using NonConst = DiscardedPacketsMessage;
};

template <>
struct TypeDescr<DiscardedPacketsMessage> : public DiscardedPacketsMessageTypeDescr
{
};

template <>
struct TypeDescr<ConstDiscardedPacketsMessage> : public DiscardedPacketsMessageTypeDescr
{
};

} /* namespace internal */

template <typename LibObjT>
class CommonMessageIteratorInactivityMessage final : public CommonMessage<LibObjT>
{
private:
    using typename CommonMessage<LibObjT>::_LibObjPtr;
    using typename CommonMessage<LibObjT>::_ThisCommonMessage;

public:
    using Shared =
        internal::SharedMessage<CommonMessageIteratorInactivityMessage<LibObjT>, LibObjT>;

    explicit CommonMessageIteratorInactivityMessage(const _LibObjPtr libObjPtr) noexcept :
        _ThisCommonMessage {libObjPtr}
    {
        BT_ASSERT_DBG(this->isMessageIteratorInactivity());
    }

    template <typename OtherLibObjT>
    CommonMessageIteratorInactivityMessage(
        const CommonMessageIteratorInactivityMessage<OtherLibObjT>& val) noexcept :
        _ThisCommonMessage {val}
    {
    }

    template <typename OtherLibObjT>
    CommonMessageIteratorInactivityMessage<LibObjT>&
    operator=(const CommonMessageIteratorInactivityMessage<OtherLibObjT>& val) noexcept
    {
        _ThisCommonMessage::operator=(val);
        return *this;
    }

    ConstClockSnapshot clockSnapshot() const noexcept
    {
        const auto libObjPtr =
            bt_message_message_iterator_inactivity_borrow_clock_snapshot_const(this->libObjPtr());

        return ConstClockSnapshot {libObjPtr};
    }

    Shared shared() const noexcept
    {
        return Shared::createWithRef(*this);
    }
};

using MessageIteratorInactivityMessage = CommonMessageIteratorInactivityMessage<bt_message>;
using ConstMessageIteratorInactivityMessage =
    CommonMessageIteratorInactivityMessage<const bt_message>;

namespace internal {

struct MessageIteratorInactivityMessageTypeDescr
{
    using Const = ConstMessageIteratorInactivityMessage;
    using NonConst = MessageIteratorInactivityMessage;
};

template <>
struct TypeDescr<MessageIteratorInactivityMessage> :
    public MessageIteratorInactivityMessageTypeDescr
{
};

template <>
struct TypeDescr<ConstMessageIteratorInactivityMessage> :
    public MessageIteratorInactivityMessageTypeDescr
{
};

} /* namespace internal */

template <typename LibObjT>
CommonStreamBeginningMessage<LibObjT> CommonMessage<LibObjT>::asStreamBeginning() const noexcept
{
    BT_ASSERT_DBG(this->isStreamBeginning());
    return CommonStreamBeginningMessage<LibObjT> {this->libObjPtr()};
}

template <typename LibObjT>
CommonStreamEndMessage<LibObjT> CommonMessage<LibObjT>::asStreamEnd() const noexcept
{
    BT_ASSERT_DBG(this->isStreamEnd());
    return CommonStreamEndMessage<LibObjT> {this->libObjPtr()};
}

template <typename LibObjT>
CommonPacketBeginningMessage<LibObjT> CommonMessage<LibObjT>::asPacketBeginning() const noexcept
{
    BT_ASSERT_DBG(this->isPacketBeginning());
    return CommonPacketBeginningMessage<LibObjT> {this->libObjPtr()};
}

template <typename LibObjT>
CommonPacketEndMessage<LibObjT> CommonMessage<LibObjT>::asPacketEnd() const noexcept
{
    BT_ASSERT_DBG(this->isPacketEnd());
    return CommonPacketEndMessage<LibObjT> {this->libObjPtr()};
}

template <typename LibObjT>
CommonEventMessage<LibObjT> CommonMessage<LibObjT>::asEvent() const noexcept
{
    BT_ASSERT_DBG(this->isEvent());
    return CommonEventMessage<LibObjT> {this->libObjPtr()};
}

template <typename LibObjT>
CommonDiscardedEventsMessage<LibObjT> CommonMessage<LibObjT>::asDiscardedEvents() const noexcept
{
    BT_ASSERT_DBG(this->isDiscardedEvents());
    return CommonDiscardedEventsMessage<LibObjT> {this->libObjPtr()};
}

template <typename LibObjT>
CommonMessageIteratorInactivityMessage<LibObjT>
CommonMessage<LibObjT>::asMessageIteratorInactivity() const noexcept
{
    BT_ASSERT_DBG(this->isMessageIteratorInactivity());
    return CommonMessageIteratorInactivityMessage<LibObjT> {this->libObjPtr()};
}

} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_MESSAGE_HPP */
