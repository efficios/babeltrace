/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2024 EfficiOS, Inc.
 */

#include "cpp-common/bt2/clock-class.hpp"
#include "cpp-common/bt2/message.hpp"
#include "cpp-common/bt2/wrap.hpp"

#include "clock-correlation-validator.h"
#include "clock-correlation-validator.hpp"

namespace bt2ccv {

void ClockCorrelationValidator::_validate(const bt2::ConstMessage msg)
{
    bt2::OptionalBorrowedObject<bt2::ConstClockClass> clockCls;
    bt2::OptionalBorrowedObject<bt2::ConstStreamClass> streamCls;

    switch (msg.type()) {
    case bt2::MessageType::STREAM_BEGINNING:
        streamCls = msg.asStreamBeginning().stream().cls();
        clockCls = streamCls->defaultClockClass();
        break;

    case bt2::MessageType::MESSAGE_ITERATOR_INACTIVITY:
        clockCls = msg.asMessageIteratorInactivity().clockSnapshot().clockClass();
        break;

    default:
        bt_common_abort();
    }

    switch (_mExpectation) {
    case PropsExpectation::UNSET:
        /*
         * This is the first analysis of a message with a clock
         * snapshot: record the properties of that clock, against which
         * we'll compare the clock properties of the following messages.
         */
        if (!clockCls) {
            _mExpectation = PropsExpectation::NONE;
        } else if (clockCls->originIsUnixEpoch()) {
            _mExpectation = PropsExpectation::ORIGIN_UNIX;
        } else if (const auto uuid = clockCls->uuid()) {
            _mExpectation = PropsExpectation::ORIGIN_OTHER_UUID;
            _mUuid = *uuid;
        } else {
            _mExpectation = PropsExpectation::ORIGIN_OTHER_NO_UUID;
            _mClockClass = clockCls->shared();
        }

        break;

    case PropsExpectation::NONE:
        if (clockCls) {
            throw ClockCorrelationError {
                ClockCorrelationError::Type::EXPECTING_NO_CLOCK_CLASS_GOT_ONE,
                bt2s::nullopt,
                *clockCls,
                {},
                streamCls};
        }

        break;

    case PropsExpectation::ORIGIN_UNIX:
        if (!clockCls) {
            throw ClockCorrelationError {
                ClockCorrelationError::Type::EXPECTING_ORIGIN_UNIX_GOT_NONE,
                bt2s::nullopt,
                {},
                {},
                streamCls};
        }

        if (!clockCls->originIsUnixEpoch()) {
            throw ClockCorrelationError {
                ClockCorrelationError::Type::EXPECTING_ORIGIN_UNIX_GOT_OTHER,
                bt2s::nullopt,
                *clockCls,
                {},
                streamCls};
        }

        break;

    case PropsExpectation::ORIGIN_OTHER_UUID:
    {
        if (!clockCls) {
            throw ClockCorrelationError {
                ClockCorrelationError::Type::EXPECTING_ORIGIN_UUID_GOT_NONE,
                bt2s::nullopt,
                {},
                {},
                streamCls};
        }

        if (clockCls->originIsUnixEpoch()) {
            throw ClockCorrelationError {
                ClockCorrelationError::Type::EXPECTING_ORIGIN_UUID_GOT_UNIX,
                bt2s::nullopt,
                *clockCls,
                {},
                streamCls};
        }

        const auto uuid = clockCls->uuid();

        if (!uuid) {
            throw ClockCorrelationError {
                ClockCorrelationError::Type::EXPECTING_ORIGIN_UUID_GOT_NO_UUID,
                bt2s::nullopt,
                *clockCls,
                {},
                streamCls};
        }

        if (*uuid != _mUuid) {
            throw ClockCorrelationError {
                ClockCorrelationError::Type::EXPECTING_ORIGIN_UUID_GOT_OTHER_UUID,
                _mUuid,
                *clockCls,
                {},
                streamCls};
        }

        break;
    }

    case PropsExpectation::ORIGIN_OTHER_NO_UUID:
        if (!clockCls) {
            throw ClockCorrelationError {
                ClockCorrelationError::Type::EXPECTING_ORIGIN_NO_UUID_GOT_NONE,
                bt2s::nullopt,
                {},
                {},
                streamCls};
        }

        if (clockCls->libObjPtr() != _mClockClass->libObjPtr()) {
            throw ClockCorrelationError {
                ClockCorrelationError::Type::EXPECTING_ORIGIN_NO_UUID_GOT_OTHER, bt2s::nullopt,
                *clockCls, *_mClockClass, streamCls};
        }

        break;

    default:
        bt_common_abort();
    }
}

} /* namespace bt2ccv */

bt_clock_correlation_validator *bt_clock_correlation_validator_create() noexcept
{
    try {
        return reinterpret_cast<bt_clock_correlation_validator *>(
            new bt2ccv::ClockCorrelationValidator);
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
}

bool bt_clock_correlation_validator_validate_message(
    bt_clock_correlation_validator * const validator, const bt_message * const msg,
    bt_clock_correlation_validator_error_type * const type, bt_uuid * const expectedUuidOut,
    const bt_clock_class ** const actualClockClsOut,
    const bt_clock_class ** const expectedClockClsOut) noexcept
{
    try {
        reinterpret_cast<bt2ccv::ClockCorrelationValidator *>(validator)->validate(bt2::wrap(msg));
        return true;
    } catch (const bt2ccv::ClockCorrelationError& error) {
        *type = static_cast<bt_clock_correlation_validator_error_type>(error.type());

        if (error.expectedUuid()) {
            *expectedUuidOut = error.expectedUuid()->data();
        } else {
            *expectedUuidOut = nullptr;
        }

        if (error.actualClockCls()) {
            *actualClockClsOut = error.actualClockCls()->libObjPtr();
        } else {
            *actualClockClsOut = nullptr;
        }

        if (error.expectedClockCls()) {
            *expectedClockClsOut = error.expectedClockCls()->libObjPtr();
        } else {
            *expectedClockClsOut = nullptr;
        }

        return false;
    }
}

void bt_clock_correlation_validator_destroy(
    bt_clock_correlation_validator * const validator) noexcept
{
    delete reinterpret_cast<bt2ccv::ClockCorrelationValidator *>(validator);
}
