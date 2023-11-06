/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2017-2023 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_PLUGINS_UTILS_MUXER_MSG_ITER_HPP
#define BABELTRACE_PLUGINS_UTILS_MUXER_MSG_ITER_HPP

#include <vector>

#include "cpp-common/bt2/optional-borrowed-object.hpp"
#include "cpp-common/bt2/plugin-dev.hpp"
#include "cpp-common/bt2c/prio-heap.hpp"
#include "cpp-common/bt2c/uuid.hpp"
#include "cpp-common/bt2s/optional.hpp"

#include "upstream-msg-iter.hpp"

namespace bt2mux {

class Comp;

class MsgIter final : public bt2::UserMessageIterator<MsgIter, Comp>
{
    friend bt2::UserMessageIterator<MsgIter, Comp>;

private:
    /* Clock class nature expectation */
    enum class _ClkClsExpectation
    {
        ANY,
        NONE,
        ORIG_IS_UNIX_EPOCH,
        ORIG_ISNT_UNIX_EPOCH_AND_SPEC_UUID,
        ORIG_ISNT_UNIX_EPOCH_AND_NO_UUID,
    };

    /* Comparator for `_mHeap` with its own logger */
    class _HeapComparator final
    {
    public:
        explicit _HeapComparator(const bt2c::Logger& logger);

        bool operator()(const UpstreamMsgIter *upstreamMsgIterA,
                        const UpstreamMsgIter *upstreamMsgIterB) const noexcept;

    private:
        bt2c::Logger _mLogger;
    };

public:
    explicit MsgIter(bt2::SelfMessageIterator selfMsgIter,
                     bt2::SelfMessageIteratorConfiguration config,
                     bt2::SelfComponentOutputPort selfPort);

private:
    bool _canSeekBeginning();
    void _seekBeginning();
    void _next(bt2::ConstMessageArray& msgs);

    /*
     * Makes sure `_mUpstreamMsgItersToReload` is empty so that `_mHeap`
     * is ready for the next message selection.
     *
     * This may throw whatever UpstreamMsgIter::reload() may throw.
     */
    void _ensureFullHeap();

    /*
     * Validates the clock class of the received message `msg`, setting
     * the expectation if this is the first one.
     *
     * Throws `bt2::Error` on error.
     */
    void _validateMsgClkCls(bt2::ConstMessage msg);

    /*
     * Sets the clock class expectation (`_mClkClsExpectation` and
     * `_mExpectedClkClsUuid`) according to `clkCls`.
     */
    void _setClkClsExpectation(bt2::OptionalBorrowedObject<bt2::ConstClockClass> clkCls) noexcept;

    /*
     * Checks that `clkCls` meets the current clock class expectation,
     * throwing if it doesn't.
     */
    void _makeSureClkClsIsExpected(bt2::ConstMessage msg,
                                   bt2::OptionalBorrowedObject<bt2::ConstClockClass> clkCls) const;

    /*
     * Container of all the upstream message iterators.
     *
     * The only purpose of this is to own them; where they are below
     * indicates their state.
     */
    std::vector<UpstreamMsgIter::UP> _mUpstreamMsgIters;

    /*
     * Heap of ready-to-use upstream message iterators (pointers to
     * owned objects in `_mUpstreamMsgIters` above).
     */
    bt2c::PrioHeap<UpstreamMsgIter *, _HeapComparator> _mHeap;

    /*
     * Current upstream message iterators to reload, on which we must
     * call reload() before moving them to `_mHeap` or to
     * `_mEndedUpstreamMsgIters`.
     *
     * Using `std::vector` instead of some linked list because the
     * typical scenario is to add a single one and then remove it
     * shortly after.
     */
    std::vector<UpstreamMsgIter *> _mUpstreamMsgItersToReload;

    /*
     * Which kind of clock class to expect from any incoming message.
     *
     * The very first received message determines this for all the
     * following.
     *
     * For `ORIG_ISNT_UNIX_EPOCH_AND_SPEC_UUID`, `*_mExpectedClkClsUuid`
     * is the expected specific UUID.
     */
    _ClkClsExpectation _mClkClsExpectation = _ClkClsExpectation::ANY;
    bt2s::optional<bt2c::Uuid> _mExpectedClkClsUuid;
};

} /* namespace bt2mux */

#endif /* BABELTRACE_PLUGINS_UTILS_MUXER_MSG_ITER_HPP */
