/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2023 Philippe Proulx <pproulx@efficios.com>
 */

#include <algorithm>

#include <glib.h>

#include <babeltrace2/babeltrace.h>

#include "cpp-common/bt2c/fmt.hpp"
#include "cpp-common/bt2s/make-unique.hpp"
#include "cpp-common/vendor/fmt/format.h"

#include "plugins/common/muxing/muxing.h"

#include "comp.hpp"
#include "msg-iter.hpp"

namespace bt2mux {

MsgIter::MsgIter(const bt2::SelfMessageIterator selfMsgIter,
                 const bt2::SelfMessageIteratorConfiguration cfg, bt2::SelfComponentOutputPort) :
    bt2::UserMessageIterator<MsgIter, Comp> {selfMsgIter, "MSG-ITER"},
    _mHeap {_HeapComparator {_mLogger}}
{
    /*
     * Create one upstream message iterator for each connected
     * input port.
     */
    auto canSeekForward = true;

    for (const auto inputPort : this->_component()._inputPorts()) {
        if (!inputPort.isConnected()) {
            BT_CPPLOGI("Ignoring disconnected port: name={}", inputPort.name());
            continue;
        }

        /*
         * Create new upstream message iterator and immediately make it
         * part of `_mUpstreamMsgItersToReload` (_ensureFullHeap() will
         * deal with it when downstream calls next()).
         */
        auto upstreamMsgIter = bt2s::make_unique<UpstreamMsgIter>(
            this->_createMessageIterator(inputPort), inputPort.name(), _mLogger);

        canSeekForward = canSeekForward && upstreamMsgIter->canSeekForward();
        _mUpstreamMsgItersToReload.emplace_back(upstreamMsgIter.get());
        _mUpstreamMsgIters.push_back(std::move(upstreamMsgIter));
    }

    /* Set the "can seek forward" configuration */
    cfg.canSeekForward(canSeekForward);
}

namespace {

std::string optMsgTsStr(const bt2s::optional<std::int64_t>& ts)
{
    if (ts) {
        return fmt::to_string(*ts);
    }

    return "none";
}

} /* namespace */

void MsgIter::_next(bt2::ConstMessageArray& msgs)
{
    /* Make sure all upstream message iterators are part of the heap */
    this->_ensureFullHeap();

    while (msgs.length() < msgs.capacity()) {
        /* Empty heap? */
        if (G_UNLIKELY(_mHeap.isEmpty())) {
            /* No more upstream messages! */
            return;
        }

        /*
         * Retrieve the upstream message iterator having the oldest message.
         */
        auto& oldestUpstreamMsgIter = *_mHeap.top();

        /* Validate the clock class of the oldest message */
        this->_validateMsgClkCls(oldestUpstreamMsgIter.msg());

        /* Append the oldest message and discard it */
        msgs.append(oldestUpstreamMsgIter.msg().shared());

        if (_mLogger.wouldLogD()) {
            BT_CPPLOGD("Appended message to array: port-name={}, ts={}",
                       oldestUpstreamMsgIter.portName(),
                       optMsgTsStr(oldestUpstreamMsgIter.msgTs()));
        }

        oldestUpstreamMsgIter.discard();

        /*
         * Immediately try to reload `oldestUpstreamMsgIter`.
         *
         * The possible outcomes are:
         *
         * There's an available message:
         *     Call `_mHeap.replaceTop()` to bring
         *     `oldestUpstreamMsgIter` back to the heap, performing a
         *     single heap rebalance.
         *
         * There isn't an available message (ended):
         *     Remove `oldestUpstreamMsgIter` from the heap.
         *
         * `bt2::TryAgain` is thrown:
         *     Remove `oldestUpstreamMsgIter` from the heap.
         *
         *     Add `oldestUpstreamMsgIter` to the set of upstream
         *     message iterators to reload. The next call to _next()
         *     will move it to the heap again (if not ended) after
         *     having successfully called reload().
         */
        BT_CPPLOGD(
            "Trying to reload upstream message iterator having the oldest message: port-name={}",
            oldestUpstreamMsgIter.portName());

        try {
            if (G_LIKELY(oldestUpstreamMsgIter.reload() == UpstreamMsgIter::ReloadStatus::MORE)) {
                /* New current message: update heap */
                _mHeap.replaceTop(&oldestUpstreamMsgIter);
                BT_CPPLOGD("More messages available; updated heap: port-name={}, heap-len={}",
                           oldestUpstreamMsgIter.portName(), _mHeap.len());
            } else {
                _mHeap.removeTop();
                BT_CPPLOGD("Upstream message iterator has no more messages; removed from heap: "
                           "port-name{}, heap-len={}",
                           oldestUpstreamMsgIter.portName(), _mHeap.len());
            }
        } catch (const bt2::TryAgain&) {
            _mHeap.removeTop();
            _mUpstreamMsgItersToReload.push_back(&oldestUpstreamMsgIter);
            BT_CPPLOGD("Moved upstream message iterator from heap to \"to reload\" set: "
                       "port-name={}, heap-len={}, to-reload-len={}",
                       oldestUpstreamMsgIter.portName(), _mHeap.len(),
                       _mUpstreamMsgItersToReload.size());
            throw;
        }
    }
}

void MsgIter::_ensureFullHeap()
{
    /*
     * Always remove from `_mUpstreamMsgItersToReload` when reload()
     * doesn't throw.
     *
     * If reload() returns `UpstreamMsgIter::ReloadStatus::NO_MORE`,
     * then we don't need it anymore (remains alive in
     * `_mUpstreamMsgIters`).
     */
    for (auto it = _mUpstreamMsgItersToReload.begin(); it != _mUpstreamMsgItersToReload.end();
         it = _mUpstreamMsgItersToReload.erase(it)) {
        auto& upstreamMsgIter = **it;

        BT_CPPLOGD("Handling upstream message iterator to reload: "
                   "port-name={}, heap-len={}, to-reload-len={}",
                   upstreamMsgIter.portName(), _mHeap.len(), _mUpstreamMsgItersToReload.size());

        if (G_LIKELY(upstreamMsgIter.reload() == UpstreamMsgIter::ReloadStatus::MORE)) {
            /* New current message: move to heap */
            _mHeap.insert(&upstreamMsgIter);
            BT_CPPLOGD("More messages available; "
                       "inserted upstream message iterator into heap from \"to reload\" set: "
                       "port-name={}, heap-len={}",
                       upstreamMsgIter.portName(), _mHeap.len());
        } else {
            BT_CPPLOGD("Not inserting upstream message iterator into heap (no more messages): "
                       "port-name={}",
                       upstreamMsgIter.portName());
        }
    }
}

bool MsgIter::_canSeekBeginning()
{
    /*
     * We can only seek our beginning if all our upstream message
     * iterators also can.
     */
    return std::all_of(_mUpstreamMsgIters.begin(), _mUpstreamMsgIters.end(),
                       [](UpstreamMsgIter::UP& upstreamMsgIter) {
                           return upstreamMsgIter->canSeekBeginning();
                       });
}

void MsgIter::_seekBeginning()
{
    /*
     * The current approach is that this operation is either successful
     * (all upstream message iterators seek) or not. If it's not, then
     * we don't keep any state that some sought and some didn't: we'll
     * restart the whole process when the user tries to seek again.
     *
     * The first step is to clear all the containers of upstream message
     * iterator pointers so that we can process what's in
     * `_mUpstreamMsgIters` only. This is irreversible, but it's okay:
     * if any seeking fails below, the downstream user is required to
     * try the "seek beginning" operation again and only call
     * bt_message_iterator_next() if it was successful.
     *
     * This means if the first four upstream message iterators seek, and
     * then the fifth one throws `bt2::TryAgain`, then the next time
     * this method executes, the first four upstream message iterators
     * will seek again. That being said, it's such an unlikely scenario
     * that the simplicity outweighs performance concerns here.
     */
    _mHeap.clear();
    _mUpstreamMsgItersToReload.clear();

    /* Make each upstream message iterator seek */
    for (auto& upstreamMsgIter : _mUpstreamMsgIters) {
        /* This may throw! */
        upstreamMsgIter->seekBeginning();
    }

    /*
     * All sought successfully: fill `_mUpstreamMsgItersToReload`; the
     * next call to _next() will deal with those.
     */
    for (auto& upstreamMsgIter : _mUpstreamMsgIters) {
        _mUpstreamMsgItersToReload.push_back(upstreamMsgIter.get());
    }
}

void MsgIter::_validateMsgClkCls(const bt2::ConstMessage msg)
{
    if (G_LIKELY(!msg.isStreamBeginning() && !msg.isMessageIteratorInactivity())) {
        /*
         * We don't care about the other types: all the messages related
         * to a given stream shared the same default clock class, if
         * any.
         */
        return;
    }

    BT_CPPLOGD("Validating the clock class of a message: msg-type={}", msg.type());

    try {
        _mClkCorrValidator.validate(msg);
    } catch (const bt2ccv::ClockCorrelationError& error) {
        using Type = bt2ccv::ClockCorrelationError::Type;

        const auto actualClockCls = error.actualClockCls();

        switch (error.type()) {
        case Type::EXPECTING_NO_CLOCK_CLASS_GOT_ONE:
            BT_CPPLOGE_APPEND_CAUSE_AND_THROW(bt2::Error,
                                              "Expecting no clock class, but got one: "
                                              "clock-class-addr={}, clock-class-name={}",
                                              fmt::ptr(actualClockCls->libObjPtr()),
                                              actualClockCls->name());

        case Type::EXPECTING_ORIGIN_UNIX_GOT_NONE:
        case Type::EXPECTING_ORIGIN_UUID_GOT_NONE:
        case Type::EXPECTING_ORIGIN_NO_UUID_GOT_NONE:
        {
            const auto streamCls = *error.streamCls();

            BT_CPPLOGE_APPEND_CAUSE_AND_THROW(bt2::Error,
                                              "Expecting a clock class, but got none: "
                                              "stream-class-addr={}, stream-class-name=\"{}\", "
                                              "stream-class-id={}",
                                              fmt::ptr(streamCls.libObjPtr()), streamCls.name(),
                                              streamCls.id());
        }

        case Type::EXPECTING_ORIGIN_UNIX_GOT_OTHER:
            BT_CPPLOGE_APPEND_CAUSE_AND_THROW(bt2::Error,
                                              "Expecting a clock class having a Unix epoch origin, "
                                              "but got one not having a Unix epoch origin: "
                                              "clock-class-addr={}, clock-class-name={}",
                                              fmt::ptr(actualClockCls->libObjPtr()),
                                              actualClockCls->name());

        case Type::EXPECTING_ORIGIN_UUID_GOT_UNIX:
            BT_CPPLOGE_APPEND_CAUSE_AND_THROW(
                bt2::Error,
                "Expecting a clock class not having a Unix epoch origin, "
                "but got one having a Unix epoch origin: "
                "clock-class-addr={}, clock-class-name={}",
                fmt::ptr(actualClockCls->libObjPtr()), actualClockCls->name());

        case Type::EXPECTING_ORIGIN_UUID_GOT_NO_UUID:
            BT_CPPLOGE_APPEND_CAUSE_AND_THROW(
                bt2::Error,
                "Expecting a clock class with a UUID, but got one without a UUID: "
                "clock-class-addr={}, clock-class-name={}",
                fmt::ptr(actualClockCls->libObjPtr()), actualClockCls->name());

        case Type::EXPECTING_ORIGIN_UUID_GOT_OTHER_UUID:
            BT_CPPLOGE_APPEND_CAUSE_AND_THROW(bt2::Error,
                                              "Expecting a clock class with a specific UUID, "
                                              "but got one with a different UUID: "
                                              "clock-class-addr={}, clock-class-name={}, "
                                              "expected-uuid=\"{}\", uuid=\"{}\"",
                                              fmt::ptr(actualClockCls->libObjPtr()),
                                              actualClockCls->name(), *error.expectedUuid(),
                                              *actualClockCls->uuid());

        case Type::EXPECTING_ORIGIN_NO_UUID_GOT_OTHER:
        {
            const auto expectedClockCls = error.expectedClockCls();

            BT_CPPLOGE_APPEND_CAUSE_AND_THROW(
                bt2::Error,
                "Unexpected clock class: "
                "expected-clock-class-addr={}, expected-clock-class-name={}, "
                "actual-clock-class-addr={}, actual-clock-class-name={}",
                fmt::ptr(expectedClockCls->libObjPtr()), expectedClockCls->name(),
                fmt::ptr(actualClockCls->libObjPtr()), actualClockCls->name());
        }
        }
    }
}

MsgIter::_HeapComparator::_HeapComparator(const bt2c::Logger& logger) : _mLogger {logger}
{
}

bool MsgIter::_HeapComparator::operator()(
    const UpstreamMsgIter * const upstreamMsgIterA,
    const UpstreamMsgIter * const upstreamMsgIterB) const noexcept
{
    /* The two messages to compare */
    const auto msgA = upstreamMsgIterA->msg();
    const auto msgB = upstreamMsgIterB->msg();
    auto& msgTsA = upstreamMsgIterA->msgTs();
    auto& msgTsB = upstreamMsgIterB->msgTs();

    if (_mLogger.wouldLogT()) {
        BT_CPPLOGT("Comparing two messages: "
                   "port-name-a={}, msg-a-type={}, msg-a-ts={}, "
                   "port-name-b={}, msg-b-type={}, msg-b-ts={}",
                   upstreamMsgIterA->portName(), msgA.type(), optMsgTsStr(msgTsA),
                   upstreamMsgIterB->portName(), msgB.type(), optMsgTsStr(msgTsB));
    }

    /*
     * Try to compare using timestamps.
     *
     * If both timestamps are set and their values are different, then
     * use this to establish the ordering of the two messages.
     *
     * If one timestamp is set, but not the other, the latter always
     * wins. This is because, for a given upstream message iterator, we
     * need to consume all the messages having no timestamp so that we
     * can reach a message with a timestamp to compare it.
     *
     * Otherwise, we'll fall back to using
     * common_muxing_compare_messages().
     */
    if (G_LIKELY(msgTsA && msgTsB)) {
        if (*msgTsA < *msgTsB) {
            /*
             * Return `true` because `_mHeap.top()` provides the
             * "greatest" element. For us, the "greatest" message is
             * the oldest one, that is, the one having the smallest
             * timestamp.
             */
            BT_CPPLOGT_STR("Timestamp of message A is less than timestamp of message B: oldest=A");
            return true;
        } else if (*msgTsA > *msgTsB) {
            BT_CPPLOGT_STR(
                "Timestamp of message A is greater than timestamp of message B: oldest=B");
            return false;
        }
    } else if (msgTsA && !msgTsB) {
        BT_CPPLOGT_STR("Message A has a timestamp, but message B has none: oldest=B");
        return false;
    } else if (!msgTsA && msgTsB) {
        BT_CPPLOGT_STR("Message B has a timestamp, but message A has none: oldest=A");
        return true;
    }

    /*
     * Comparison failed using timestamps: determine an ordering using
     * arbitrary properties, but in a deterministic way.
     *
     * common_muxing_compare_messages() returns less than 0 if the first
     * message is considered older than the second, which corresponds to
     * this comparator returning `true`.
     */
    const auto res = common_muxing_compare_messages(msgA.libObjPtr(), msgB.libObjPtr()) < 0;

    BT_CPPLOGT("Timestamps are considered equal; comparing other properties: oldest={}",
               res ? "A" : "B");
    return res;
}

} /* namespace bt2mux */
