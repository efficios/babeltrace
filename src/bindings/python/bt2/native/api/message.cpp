/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#include <Python.h>

#include <cstdint>
#include <optional>

#include <babeltrace2/babeltrace.h>

#include "cpp-common/bt2/message.hpp"

#include "../api.hpp"
#include "../runtime/py-module-builder.hpp"

namespace bt2py {
namespace {

bt2::OptionalBorrowedObject<bt2::ConstClockSnapshot>
borrowStreamBeginningMsgDefClkSnapshot(const bt2::ConstMessage msg) noexcept
{
    return msg.asStreamBeginning().defaultClockSnapshot();
}

bt2::OptionalBorrowedObject<bt2::ConstClockSnapshot>
borrowStreamEndMsgDefClkSnapshot(const bt2::ConstMessage msg) noexcept
{
    return msg.asStreamEnd().defaultClockSnapshot();
}

std::optional<std::uint64_t> getDiscardedEventsMsgCount(const bt2::ConstMessage msg) noexcept
{
    return msg.asDiscardedEvents().count();
}

std::optional<std::uint64_t> getDiscardedPacketsMsgCount(const bt2::ConstMessage msg) noexcept
{
    return msg.asDiscardedPackets().count();
}

} /* namespace */

void registerMessageApi(PyModuleBuilder& mb)
{
    mb.addWiseEnumConstants<bt2::MessageType>("MESSAGE_TYPE_");

    mb.addLibFunc<bt_message_get_type>();
    mb.addLibFunc<bt_message_get_ref>();
    mb.addLibFunc<bt_message_put_ref>();

    mb.addLibFunc<bt_message_stream_beginning_create>();
    mb.addLibFunc<bt_message_stream_beginning_borrow_stream>();
    mb.addLibFunc<bt_message_stream_beginning_borrow_stream_const>();
    mb.addLibFunc<bt_message_stream_beginning_set_default_clock_snapshot>();
    mb.addFunc<borrowStreamBeginningMsgDefClkSnapshot>(
        "message_stream_beginning_borrow_default_clock_snapshot_const");

    mb.addLibFunc<bt_message_stream_end_create>();
    mb.addLibFunc<bt_message_stream_end_borrow_stream>();
    mb.addLibFunc<bt_message_stream_end_borrow_stream_const>();
    mb.addLibFunc<bt_message_stream_end_set_default_clock_snapshot>();
    mb.addFunc<borrowStreamEndMsgDefClkSnapshot>(
        "message_stream_end_borrow_default_clock_snapshot_const");

    mb.addLibFunc<bt_message_event_create>();
    mb.addLibFunc<bt_message_event_create_with_default_clock_snapshot>();
    mb.addLibFunc<bt_message_event_create_with_packet>();
    mb.addLibFunc<bt_message_event_create_with_packet_and_default_clock_snapshot>();
    mb.addLibFunc<bt_message_event_borrow_event>();
    mb.addLibFunc<bt_message_event_borrow_event_const>();
    mb.addLibFunc<bt_message_event_borrow_default_clock_snapshot_const>();

    mb.addLibFunc<bt_message_packet_beginning_create>();
    mb.addLibFunc<bt_message_packet_beginning_create_with_default_clock_snapshot>();
    mb.addLibFunc<bt_message_packet_beginning_borrow_packet>();
    mb.addLibFunc<bt_message_packet_beginning_borrow_packet_const>();
    mb.addLibFunc<bt_message_packet_beginning_borrow_default_clock_snapshot_const>();

    mb.addLibFunc<bt_message_packet_end_create>();
    mb.addLibFunc<bt_message_packet_end_create_with_default_clock_snapshot>();
    mb.addLibFunc<bt_message_packet_end_borrow_packet>();
    mb.addLibFunc<bt_message_packet_end_borrow_packet_const>();
    mb.addLibFunc<bt_message_packet_end_borrow_default_clock_snapshot_const>();

    mb.addLibFunc<bt_message_discarded_events_create>();
    mb.addLibFunc<bt_message_discarded_events_create_with_default_clock_snapshots>();
    mb.addLibFunc<bt_message_discarded_events_borrow_stream>();
    mb.addLibFunc<bt_message_discarded_events_borrow_stream_const>();
    mb.addLibFunc<bt_message_discarded_events_borrow_beginning_default_clock_snapshot_const>();
    mb.addLibFunc<bt_message_discarded_events_borrow_end_default_clock_snapshot_const>();
    mb.addLibFunc<bt_message_discarded_events_set_count>();
    mb.addFunc<getDiscardedEventsMsgCount>("message_discarded_events_get_count");

    mb.addLibFunc<bt_message_discarded_packets_create>();
    mb.addLibFunc<bt_message_discarded_packets_create_with_default_clock_snapshots>();
    mb.addLibFunc<bt_message_discarded_packets_borrow_stream>();
    mb.addLibFunc<bt_message_discarded_packets_borrow_stream_const>();
    mb.addLibFunc<bt_message_discarded_packets_borrow_beginning_default_clock_snapshot_const>();
    mb.addLibFunc<bt_message_discarded_packets_borrow_end_default_clock_snapshot_const>();
    mb.addLibFunc<bt_message_discarded_packets_set_count>();
    mb.addFunc<getDiscardedPacketsMsgCount>("message_discarded_packets_get_count");

    mb.addLibFunc<bt_message_message_iterator_inactivity_create>();
    mb.addLibFunc<bt_message_message_iterator_inactivity_borrow_clock_snapshot_const>();
}

} /* namespace bt2py */
