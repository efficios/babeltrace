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
#include "optional-borrowed-object.hpp"
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

inline OptionalBorrowedObject<ClockClass> wrapOptional(bt_clock_class * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<ConstClockClass>
wrapOptional(const bt_clock_class * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<ConstClockSnapshot>
wrapOptional(const bt_clock_snapshot * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<ConstComponent>
wrapOptional(const bt_component * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<ConstSourceComponent>
wrapOptional(const bt_component_source * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<ConstFilterComponent>
wrapOptional(const bt_component_filter * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<ConstSinkComponent>
wrapOptional(const bt_component_sink * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<ConstInputPort>
wrapOptional(const bt_port_input * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<ConstOutputPort>
wrapOptional(const bt_port_output * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<FieldClass> wrapOptional(bt_field_class * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<ConstFieldClass>
wrapOptional(const bt_field_class * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<ConstFieldPathItem>
wrapOptional(const bt_field_path_item * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<ConstFieldPath>
wrapOptional(const bt_field_path * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<Field> wrapOptional(bt_field * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<ConstField> wrapOptional(const bt_field * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<UnsignedIntegerRangeSet>
wrapOptional(bt_integer_range_set_unsigned * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<ConstUnsignedIntegerRangeSet>
wrapOptional(const bt_integer_range_set_unsigned * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<SignedIntegerRangeSet>
wrapOptional(bt_integer_range_set_signed * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<ConstSignedIntegerRangeSet>
wrapOptional(const bt_integer_range_set_signed * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<ConstUnsignedIntegerRange>
wrapOptional(const bt_integer_range_unsigned * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<ConstSignedIntegerRange>
wrapOptional(const bt_integer_range_signed * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<MessageIterator>
wrapOptional(bt_message_iterator * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<Message> wrapOptional(bt_message * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<ConstMessage>
wrapOptional(const bt_message * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<PrivateQueryExecutor>
wrapOptional(bt_private_query_executor * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<SelfComponentClass>
wrapOptional(bt_self_component_class * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<SelfComponentClass>
wrapOptional(bt_self_component_class_source * const libObjPtr) noexcept
{
    return bt_self_component_class_source_as_self_component_class(libObjPtr);
}

inline OptionalBorrowedObject<SelfComponentClass>
wrapOptional(bt_self_component_class_filter * const libObjPtr) noexcept
{
    return bt_self_component_class_filter_as_self_component_class(libObjPtr);
}

inline OptionalBorrowedObject<SelfComponentClass>
wrapOptional(bt_self_component_class_sink * const libObjPtr) noexcept
{
    return bt_self_component_class_sink_as_self_component_class(libObjPtr);
}

inline OptionalBorrowedObject<SelfComponent>
wrapOptional(bt_self_component * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<SelfSourceComponent>
wrapOptional(bt_self_component_source * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<SelfFilterComponent>
wrapOptional(bt_self_component_filter * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<SelfSinkComponent>
wrapOptional(bt_self_component_sink * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<SelfComponentInputPort>
wrapOptional(bt_self_component_port_input * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<SelfComponentOutputPort>
wrapOptional(bt_self_component_port_output * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<SelfMessageIterator>
wrapOptional(bt_self_message_iterator * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<SelfMessageIteratorConfiguration>
wrapOptional(bt_self_message_iterator_configuration * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<Event> wrapOptional(bt_event * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<ConstEvent> wrapOptional(const bt_event * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<Packet> wrapOptional(bt_packet * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<ConstPacket> wrapOptional(const bt_packet * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<Stream> wrapOptional(bt_stream * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<ConstStream> wrapOptional(const bt_stream * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<Trace> wrapOptional(bt_trace * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<ConstTrace> wrapOptional(const bt_trace * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<EventClass> wrapOptional(bt_event_class * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<ConstEventClass>
wrapOptional(const bt_event_class * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<StreamClass> wrapOptional(bt_stream_class * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<ConstStreamClass>
wrapOptional(const bt_stream_class * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<TraceClass> wrapOptional(bt_trace_class * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<ConstTraceClass>
wrapOptional(const bt_trace_class * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<Value> wrapOptional(bt_value * const libObjPtr) noexcept
{
    return libObjPtr;
}

inline OptionalBorrowedObject<ConstValue> wrapOptional(const bt_value * const libObjPtr) noexcept
{
    return libObjPtr;
}

} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_WRAP_HPP */
