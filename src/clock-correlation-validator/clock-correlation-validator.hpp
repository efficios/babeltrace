/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2024 EfficiOS, Inc.
 */

#ifndef CLOCK_CORRELATION_VALIDATOR_CLOCK_CORRELATION_VALIDATOR_HPP
#define CLOCK_CORRELATION_VALIDATOR_CLOCK_CORRELATION_VALIDATOR_HPP

#include "cpp-common/bt2/message.hpp"

#include "clock-correlation-validator/clock-correlation-validator.h"

namespace bt2ccv {

class ClockCorrelationError final : public std::runtime_error
{
public:
    enum class Type
    {
        EXPECTING_NO_CLOCK_CLASS_GOT_ONE =
            BT_CLOCK_CORRELATION_VALIDATOR_ERROR_TYPE_EXPECTING_NO_CLOCK_CLASS_GOT_ONE,
        EXPECTING_ORIGIN_UNIX_GOT_NONE =
            BT_CLOCK_CORRELATION_VALIDATOR_ERROR_TYPE_EXPECTING_ORIGIN_UNIX_GOT_NONE,
        EXPECTING_ORIGIN_UNIX_GOT_OTHER =
            BT_CLOCK_CORRELATION_VALIDATOR_ERROR_TYPE_EXPECTING_ORIGIN_UNIX_GOT_OTHER,
        EXPECTING_ORIGIN_UUID_GOT_NONE =
            BT_CLOCK_CORRELATION_VALIDATOR_ERROR_TYPE_EXPECTING_ORIGIN_UUID_GOT_NONE,
        EXPECTING_ORIGIN_UUID_GOT_UNIX =
            BT_CLOCK_CORRELATION_VALIDATOR_ERROR_TYPE_EXPECTING_ORIGIN_UUID_GOT_UNIX,
        EXPECTING_ORIGIN_UUID_GOT_NO_UUID =
            BT_CLOCK_CORRELATION_VALIDATOR_ERROR_TYPE_EXPECTING_ORIGIN_UUID_GOT_NO_UUID,
        EXPECTING_ORIGIN_UUID_GOT_OTHER_UUID =
            BT_CLOCK_CORRELATION_VALIDATOR_ERROR_TYPE_EXPECTING_ORIGIN_UUID_GOT_OTHER_UUID,
        EXPECTING_ORIGIN_NO_UUID_GOT_NONE =
            BT_CLOCK_CORRELATION_VALIDATOR_ERROR_TYPE_EXPECTING_ORIGIN_NO_UUID_GOT_NONE,
        EXPECTING_ORIGIN_NO_UUID_GOT_OTHER =
            BT_CLOCK_CORRELATION_VALIDATOR_ERROR_TYPE_EXPECTING_ORIGIN_NO_UUID_GOT_OTHER,
    };

    explicit ClockCorrelationError(
        Type type, const bt2s::optional<bt2c::UuidView> expectedUuid,
        const bt2::OptionalBorrowedObject<bt2::ConstClockClass> actualClockCls,
        const bt2::OptionalBorrowedObject<bt2::ConstClockClass> expectedClockCls,
        const bt2::OptionalBorrowedObject<bt2::ConstStreamClass> streamCls) noexcept :
        std::runtime_error {"Clock classes are not correlatable"},
        _mType {type}, _mExpectedUuid {expectedUuid}, _mActualClockCls {actualClockCls},
        _mExpectedClockCls {expectedClockCls}, _mStreamCls {streamCls}

    {
    }

    Type type() const noexcept
    {
        return _mType;
    }

    bt2s::optional<bt2c::UuidView> expectedUuid() const noexcept
    {
        return _mExpectedUuid;
    }

    bt2::OptionalBorrowedObject<bt2::ConstClockClass> actualClockCls() const noexcept
    {
        return _mActualClockCls;
    }

    bt2::OptionalBorrowedObject<bt2::ConstClockClass> expectedClockCls() const noexcept
    {
        return _mExpectedClockCls;
    }

    bt2::OptionalBorrowedObject<bt2::ConstStreamClass> streamCls() const noexcept
    {
        return _mStreamCls;
    }

private:
    Type _mType;
    bt2s::optional<bt2c::UuidView> _mExpectedUuid;
    bt2::OptionalBorrowedObject<bt2::ConstClockClass> _mActualClockCls;
    bt2::OptionalBorrowedObject<bt2::ConstClockClass> _mExpectedClockCls;
    bt2::OptionalBorrowedObject<bt2::ConstStreamClass> _mStreamCls;
};

class ClockCorrelationValidator final
{
private:
    enum class PropsExpectation
    {
        /* We haven't recorded clock properties yet. */
        UNSET,

        /* Expect to have no clock. */
        NONE,

        /* Expect a clock with a Unix epoch origin. */
        ORIGIN_UNIX,

        /* Expect a clock without a Unix epoch origin, but with a UUID. */
        ORIGIN_OTHER_UUID,

        /* Expect a clock without a Unix epoch origin and without a UUID. */
        ORIGIN_OTHER_NO_UUID,
    };

public:
    void validate(const bt2::ConstMessage msg)
    {
        if (!msg.isStreamBeginning() && !msg.isMessageIteratorInactivity()) {
            return;
        }

        this->_validate(msg);
    }

private:
    void _validate(const bt2::ConstMessage msg);

    PropsExpectation _mExpectation = PropsExpectation::UNSET;

    /*
     * Expected UUID of the clock, if `_mExpectation` is
     * `PropsExpectation::ORIGIN_OTHER_UUID`.
     *
     * If the origin of the clock is the Unix epoch, then the UUID is
     * irrelevant because the clock will have a correlation with other
     * clocks having the same origin.
     */
    bt2c::Uuid _mUuid;

    /*
     * Expected clock class, if `_mExpectation` is
     * `ClockExpectation::ORIGIN_OTHER_NO_UUID`.
     *
     * If the first analyzed clock class has an unknown origin and no
     * UUID, then all subsequent analyzed clock classes must be the same
     * instance.
     *
     * To make sure that the clock class pointed to by this member
     * doesn't get freed and another one reallocated at the same
     * address, which could potentially bypass the clock expectation
     * check, we keep a strong reference, ensuring that the clock class
     * lives at least as long as the owner of this validator.
     */
    bt2::ConstClockClass::Shared _mClockClass;
};

} /* namespace bt2ccv */

#endif /* CLOCK_CORRELATION_VALIDATOR_CLOCK_CORRELATION_VALIDATOR_HPP */
