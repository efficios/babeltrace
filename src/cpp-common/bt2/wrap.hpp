/*
 * Copyright (c) 2023 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_WRAP_HPP
#define BABELTRACE_CPP_COMMON_BT2_WRAP_HPP

#include <babeltrace2/babeltrace.h>

#include "clock-class.hpp"
#include "clock-snapshot.hpp"
#include "component-port.hpp"
#include "field-class.hpp"
#include "field.hpp"
#include "integer-range-set.hpp"
#include "integer-range.hpp"
#include "message-iterator.hpp"
#include "message.hpp"
#include "private-query-executor.hpp"
#include "self-component-class.hpp"
#include "self-component-port.hpp"
#include "self-message-iterator-configuration.hpp"
#include "self-message-iterator.hpp"
#include "trace-ir.hpp"
#include "value.hpp"

namespace bt2 {

inline ClockClass wrap(bt_clock_class * const libObjPtr) noexcept
{
    return ClockClass {libObjPtr};
}

inline ConstClockClass wrap(const bt_clock_class * const libObjPtr) noexcept
{
    return ConstClockClass {libObjPtr};
}

inline ConstClockSnapshot wrap(const bt_clock_snapshot * const libObjPtr) noexcept
{
    return ConstClockSnapshot {libObjPtr};
}

inline ConstComponent wrap(const bt_component * const libObjPtr) noexcept
{
    return ConstComponent {libObjPtr};
}

inline ConstSourceComponent wrap(const bt_component_source * const libObjPtr) noexcept
{
    return ConstSourceComponent {libObjPtr};
}

inline ConstFilterComponent wrap(const bt_component_filter * const libObjPtr) noexcept
{
    return ConstFilterComponent {libObjPtr};
}

inline ConstSinkComponent wrap(const bt_component_sink * const libObjPtr) noexcept
{
    return ConstSinkComponent {libObjPtr};
}

inline ConstInputPort wrap(const bt_port_input * const libObjPtr) noexcept
{
    return ConstInputPort {libObjPtr};
}

inline ConstOutputPort wrap(const bt_port_output * const libObjPtr) noexcept
{
    return ConstOutputPort {libObjPtr};
}

inline FieldClass wrap(bt_field_class * const libObjPtr) noexcept
{
    return FieldClass {libObjPtr};
}

inline ConstFieldClass wrap(const bt_field_class * const libObjPtr) noexcept
{
    return ConstFieldClass {libObjPtr};
}

inline ConstFieldPathItem wrap(const bt_field_path_item * const libObjPtr) noexcept
{
    return ConstFieldPathItem {libObjPtr};
}

inline ConstFieldPath wrap(const bt_field_path * const libObjPtr) noexcept
{
    return ConstFieldPath {libObjPtr};
}

inline Field wrap(bt_field * const libObjPtr) noexcept
{
    return Field {libObjPtr};
}

inline ConstField wrap(const bt_field * const libObjPtr) noexcept
{
    return ConstField {libObjPtr};
}

inline UnsignedIntegerRangeSet wrap(bt_integer_range_set_unsigned * const libObjPtr) noexcept
{
    return UnsignedIntegerRangeSet {libObjPtr};
}

inline ConstUnsignedIntegerRangeSet
wrap(const bt_integer_range_set_unsigned * const libObjPtr) noexcept
{
    return ConstUnsignedIntegerRangeSet {libObjPtr};
}

inline SignedIntegerRangeSet wrap(bt_integer_range_set_signed * const libObjPtr) noexcept
{
    return SignedIntegerRangeSet {libObjPtr};
}

inline ConstSignedIntegerRangeSet wrap(const bt_integer_range_set_signed * const libObjPtr) noexcept
{
    return ConstSignedIntegerRangeSet {libObjPtr};
}

inline ConstUnsignedIntegerRange wrap(const bt_integer_range_unsigned * const libObjPtr) noexcept
{
    return ConstUnsignedIntegerRange {libObjPtr};
}

inline ConstSignedIntegerRange wrap(const bt_integer_range_signed * const libObjPtr) noexcept
{
    return ConstSignedIntegerRange {libObjPtr};
}

inline MessageIterator wrap(bt_message_iterator * const libObjPtr) noexcept
{
    return MessageIterator {libObjPtr};
}

inline Message wrap(bt_message * const libObjPtr) noexcept
{
    return Message {libObjPtr};
}

inline ConstMessage wrap(const bt_message * const libObjPtr) noexcept
{
    return ConstMessage {libObjPtr};
}

inline PrivateQueryExecutor wrap(bt_private_query_executor * const libObjPtr) noexcept
{
    return PrivateQueryExecutor {libObjPtr};
}

inline SelfComponentClass wrap(bt_self_component_class * const libObjPtr) noexcept
{
    return SelfComponentClass {libObjPtr};
}

inline SelfComponentClass wrap(bt_self_component_class_source * const libObjPtr) noexcept
{
    return SelfComponentClass {libObjPtr};
}

inline SelfComponentClass wrap(bt_self_component_class_filter * const libObjPtr) noexcept
{
    return SelfComponentClass {libObjPtr};
}

inline SelfComponentClass wrap(bt_self_component_class_sink * const libObjPtr) noexcept
{
    return SelfComponentClass {libObjPtr};
}

inline SelfComponent wrap(bt_self_component * const libObjPtr) noexcept
{
    return SelfComponent {libObjPtr};
}

inline SelfSourceComponent wrap(bt_self_component_source * const libObjPtr) noexcept
{
    return SelfSourceComponent {libObjPtr};
}

inline SelfFilterComponent wrap(bt_self_component_filter * const libObjPtr) noexcept
{
    return SelfFilterComponent {libObjPtr};
}

inline SelfSinkComponent wrap(bt_self_component_sink * const libObjPtr) noexcept
{
    return SelfSinkComponent {libObjPtr};
}

inline SelfComponentInputPort wrap(bt_self_component_port_input * const libObjPtr) noexcept
{
    return SelfComponentInputPort {libObjPtr};
}

inline SelfComponentOutputPort wrap(bt_self_component_port_output * const libObjPtr) noexcept
{
    return SelfComponentOutputPort {libObjPtr};
}

inline SelfMessageIterator wrap(bt_self_message_iterator * const libObjPtr) noexcept
{
    return SelfMessageIterator {libObjPtr};
}

inline SelfMessageIteratorConfiguration
wrap(bt_self_message_iterator_configuration * const libObjPtr) noexcept
{
    return SelfMessageIteratorConfiguration {libObjPtr};
}

inline Event wrap(bt_event * const libObjPtr) noexcept
{
    return Event {libObjPtr};
}

inline ConstEvent wrap(const bt_event * const libObjPtr) noexcept
{
    return ConstEvent {libObjPtr};
}

inline Packet wrap(bt_packet * const libObjPtr) noexcept
{
    return Packet {libObjPtr};
}

inline ConstPacket wrap(const bt_packet * const libObjPtr) noexcept
{
    return ConstPacket {libObjPtr};
}

inline Stream wrap(bt_stream * const libObjPtr) noexcept
{
    return Stream {libObjPtr};
}

inline ConstStream wrap(const bt_stream * const libObjPtr) noexcept
{
    return ConstStream {libObjPtr};
}

inline Trace wrap(bt_trace * const libObjPtr) noexcept
{
    return Trace {libObjPtr};
}

inline ConstTrace wrap(const bt_trace * const libObjPtr) noexcept
{
    return ConstTrace {libObjPtr};
}

inline EventClass wrap(bt_event_class * const libObjPtr) noexcept
{
    return EventClass {libObjPtr};
}

inline ConstEventClass wrap(const bt_event_class * const libObjPtr) noexcept
{
    return ConstEventClass {libObjPtr};
}

inline StreamClass wrap(bt_stream_class * const libObjPtr) noexcept
{
    return StreamClass {libObjPtr};
}

inline ConstStreamClass wrap(const bt_stream_class * const libObjPtr) noexcept
{
    return ConstStreamClass {libObjPtr};
}

inline TraceClass wrap(bt_trace_class * const libObjPtr) noexcept
{
    return TraceClass {libObjPtr};
}

inline ConstTraceClass wrap(const bt_trace_class * const libObjPtr) noexcept
{
    return ConstTraceClass {libObjPtr};
}

inline Value wrap(bt_value * const libObjPtr) noexcept
{
    return Value {libObjPtr};
}

inline ConstValue wrap(const bt_value * const libObjPtr) noexcept
{
    return ConstValue {libObjPtr};
}

} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_WRAP_HPP */
