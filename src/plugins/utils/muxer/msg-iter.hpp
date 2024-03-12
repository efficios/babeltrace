/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2017-2023 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_PLUGINS_UTILS_MUXER_MSG_ITER_HPP
#define BABELTRACE_PLUGINS_UTILS_MUXER_MSG_ITER_HPP

#include <vector>

#include "cpp-common/bt2/component-class-dev.hpp"
#include "cpp-common/bt2/self-message-iterator-configuration.hpp"
#include "cpp-common/bt2c/prio-heap.hpp"

#include "clock-correlation-validator/clock-correlation-validator.hpp"
#include "upstream-msg-iter.hpp"

namespace bt2mux {

class Comp;

class MsgIter final : public bt2::UserMessageIterator<MsgIter, Comp>
{
    friend bt2::UserMessageIterator<MsgIter, Comp>;

private:
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

    /* Clock class correlation validator */
    bt2ccv::ClockCorrelationValidator _mClkCorrValidator;
};

} /* namespace bt2mux */

#endif /* BABELTRACE_PLUGINS_UTILS_MUXER_MSG_ITER_HPP */
