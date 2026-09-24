/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2026 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_PLUGINS_UTILS_COUNTER_COUNTER_HPP
#define BABELTRACE_PLUGINS_UTILS_COUNTER_COUNTER_HPP

#include <cstdint>
#include <string_view>

#include "cpp-common/bt2/component-class-dev.hpp"
#include "cpp-common/bt2/message-iterator.hpp"

namespace bt2counter {

class Counter final : public bt2::UserSinkComponent<Counter>
{
    friend bt2::UserSinkComponent<Counter>;

public:
    explicit Counter(bt2::SelfSinkComponent selfComp, bt2::ConstMapValue params, void *);
    ~Counter();

private:
    static void _getSupportedMipVersions(bt2::SelfComponentClass, bt2::ConstValue, void *,
                                         bt2::LoggingLevel, bt2::UnsignedIntegerRangeSet ranges);
    void _graphIsConfigured();
    bool _consume();

    /*
     * Sum of all the message counts in `_mCount`.
     */
    std::uint64_t _totalCount() const noexcept;

    /*
     * Prints a single counter line showing `count` messages of kind
     * `what` to standard output.
     *
     * If `_mHideZero` is true and `count` is zero, then this function
     * prints nothing.
     */
    void _printOneCount(std::string_view what, std::uint64_t count) const;

    /*
     * Prints one counter line per message kind, followed by a total
     * line, to standard output.
     *
     * Updates `_mLastPrintedTotal` to the value of _totalCount() at the
     * time of printing.
     */
    void _printCount();

    /*
     * Adds `msgCount` to `_mAt` and, if the running total reaches
     * `_mStep`, calls _printCount() and resets `_mAt`.
     *
     * Does nothing if `_mStep` is zero (periodic updates disabled).
     */
    void _tryPrintCount(std::uint64_t msgCount);

    /*
     * Calls _printCount() unless the current total already matches
     * `_mLastPrintedTotal` (avoiding a duplicate final report).
     */
    void _tryPrintLast();

    struct
    {
        std::uint64_t event = 0;
        std::uint64_t streamBegin = 0;
        std::uint64_t streamEnd = 0;
        std::uint64_t pktBegin = 0;
        std::uint64_t pktEnd = 0;
        std::uint64_t discEvents = 0;
        std::uint64_t discPackets = 0;
        std::uint64_t msgIterInactivity = 0;
        std::uint64_t other = 0;
    } _mCount;

    std::uint64_t _mLastPrintedTotal = -1ULL;
    std::uint64_t _mAt = 0;
    std::uint64_t _mStep = 10000;
    bool _mHideZero = false;
    bt2::MessageIterator::Shared _mMsgIter;
};

} /* namespace bt2counter */

#endif /* BABELTRACE_PLUGINS_UTILS_COUNTER_COUNTER_HPP */
