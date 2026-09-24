/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2026 Philippe Proulx <pproulx@efficios.com>
 */

#include <fmt/core.h>

#include "common/common.h"
#include "cpp-common/bt2c/glib-up.hpp"

#include "plugins/common/param-validation/param-validation.h"

#include "counter.hpp"

namespace bt2counter {
namespace {

bt_param_validation_map_value_entry_descr counterParams[] = {
    {"step", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeUnsignedInteger()},
    {"hide-zero", BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_OPTIONAL,
     bt_param_validation_value_descr::makeBool()},
    BT_PARAM_VALIDATION_MAP_VALUE_ENTRY_END};

} /* namespace */

Counter::Counter(const bt2::SelfSinkComponent selfComp, const bt2::ConstMapValue params, void *)
    : bt2::UserSinkComponent<Counter> {selfComp, "PLUGIN/SINK.UTILS.COUNTER"}
{
    BT_CPPLOGI("Initializing component.");

    try {
        this->_addInputPort("in");
    } catch (const bt2c::Error&) {
        BT_CPPLOGE_APPEND_CAUSE_AND_RETHROW("Failed to add a single input port.");
    }

    {
        gchar *validateError = nullptr;

        if (const auto validationStatus =
                bt_param_validation_validate(params.libObjPtr(), counterParams, &validateError);
            validationStatus == BT_PARAM_VALIDATION_STATUS_MEMORY_ERROR) {
            throw bt2c::MemoryError {};
        } else if (validationStatus == BT_PARAM_VALIDATION_STATUS_VALIDATION_ERROR) {
            bt2c::GCharUP errorFreer {validateError};

            BT_CPPLOGE_APPEND_CAUSE_AND_THROW(bt2c::Error, "{}", validateError);
        }
    }

    if (const auto step = params["step"]) {
        _mStep = step->asUnsignedInteger().value();
    }

    if (const auto hideZero = params["hide-zero"]) {
        _mHideZero = hideZero->asBool().value();
    }

    BT_CPPLOGI("Initialized component.");
}

Counter::~Counter()
{
    this->_tryPrintLast();
}

void Counter::_getSupportedMipVersions(bt2::SelfComponentClass, bt2::ConstValue, void *,
                                       bt2::LoggingLevel, const bt2::UnsignedIntegerRangeSet ranges)
{
    ranges.addRange(0, 1);
}

void Counter::_graphIsConfigured()
{
    _mMsgIter = this->_createMessageIterator(this->_inputPorts()["in"]);
}

bool Counter::_consume()
{
    try {
        const auto msgs = _mMsgIter->next();

        if (!msgs) {
            this->_tryPrintLast();
            return false;
        }

        for (const auto msg : *msgs) {
            switch (msg.type()) {
            case bt2::MessageType::Event:
                ++_mCount.event;
                break;

            case bt2::MessageType::PacketBeginning:
                ++_mCount.pktBegin;
                break;

            case bt2::MessageType::PacketEnd:
                ++_mCount.pktEnd;
                break;

            case bt2::MessageType::MessageIteratorInactivity:
                ++_mCount.msgIterInactivity;
                break;

            case bt2::MessageType::StreamBeginning:
                ++_mCount.streamBegin;
                break;

            case bt2::MessageType::StreamEnd:
                ++_mCount.streamEnd;
                break;

            case bt2::MessageType::DiscardedEvents:
                ++_mCount.discEvents;
                break;

            case bt2::MessageType::DiscardedPackets:
                ++_mCount.discPackets;
                break;

            default:
                ++_mCount.other;
            }
        }

        this->_tryPrintCount(msgs->length());
        return true;
    } catch (const bt2c::Error&) {
        BT_CPPLOGE_APPEND_CAUSE_AND_RETHROW("Failed to get messages from upstream component.");
    }
}

void Counter::_printOneCount(const std::string_view what, const std::uint64_t count) const
{
    if (count != 0 || !_mHideZero) {
        fmt::print("{:15} {} message{}\n", count, what, count == 1 ? "" : "s");
    }
}

std::uint64_t Counter::_totalCount() const noexcept
{
    return _mCount.event + _mCount.streamBegin + _mCount.streamEnd + _mCount.pktBegin +
           _mCount.pktEnd + _mCount.discEvents + _mCount.discPackets + _mCount.msgIterInactivity +
           _mCount.other;
}

void Counter::_printCount()
{
    this->_printOneCount("Event", _mCount.event);
    this->_printOneCount("Stream beginning", _mCount.streamBegin);
    this->_printOneCount("Stream end", _mCount.streamEnd);
    this->_printOneCount("Packet beginning", _mCount.pktBegin);
    this->_printOneCount("Packet end", _mCount.pktEnd);
    this->_printOneCount("Discarded event", _mCount.discEvents);
    this->_printOneCount("Discarded packet", _mCount.discPackets);
    this->_printOneCount("Message iterator inactivity", _mCount.msgIterInactivity);

    if (_mCount.other > 0) {
        this->_printOneCount("Other (unknown)", _mCount.other);
    }

    const auto total = this->_totalCount();

    fmt::print("{}{:15} message{} (TOTAL){}\n", bt_common_color_bold(), total,
               total == 1 ? "" : "s", bt_common_color_reset());
    _mLastPrintedTotal = total;
}

void Counter::_tryPrintCount(const std::uint64_t msgCount)
{
    if (_mStep == 0) {
        /* No update */
        return;
    }

    _mAt += msgCount;

    if (_mAt >= _mStep) {
        _mAt = 0;
        this->_printCount();
        fmt::print("\n");
    }
}

void Counter::_tryPrintLast()
{
    if (this->_totalCount() != _mLastPrintedTotal) {
        this->_printCount();
    }
}

} /* namespace bt2counter */
