/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2023 Philippe Proulx <pproulx@efficios.com>
 */

#include <glib.h>

#include "cpp-common/bt2/optional-borrowed-object.hpp"
#include "cpp-common/bt2c/logging.hpp"
#include "cpp-common/vendor/fmt/core.h"
#include "cpp-common/vendor/fmt/format.h"

#include "upstream-msg-iter.hpp"

namespace bt2mux {

UpstreamMsgIter::UpstreamMsgIter(bt2::MessageIterator::Shared msgIter, std::string portName,
                                 const bt2c::Logger& parentLogger) :
    _mMsgIter {std::move(msgIter)},
    _mLogger {parentLogger, fmt::format("{}/[{}]", parentLogger.tag(), portName)},
    _mPortName {std::move(portName)}
{
    BT_CPPLOGI("Created an upstream message iterator: this={}, port-name={}", fmt::ptr(this),
               _mPortName);
}

namespace {

/*
 * Returns the clock snapshot of `msg`, possibly missing.
 */
bt2::OptionalBorrowedObject<bt2::ConstClockSnapshot> msgCs(const bt2::ConstMessage msg) noexcept
{
    switch (msg.type()) {
    case bt2::MessageType::EVENT:
        if (msg.asEvent().streamClassDefaultClockClass()) {
            return msg.asEvent().defaultClockSnapshot();
        }

        break;
    case bt2::MessageType::PACKET_BEGINNING:
        if (msg.asPacketBeginning().packet().stream().cls().packetsHaveBeginningClockSnapshot()) {
            return msg.asPacketBeginning().defaultClockSnapshot();
        }

        break;
    case bt2::MessageType::PACKET_END:
        if (msg.asPacketEnd().packet().stream().cls().packetsHaveEndClockSnapshot()) {
            return msg.asPacketEnd().defaultClockSnapshot();
        }

        break;
    case bt2::MessageType::DISCARDED_EVENTS:
        if (msg.asDiscardedEvents().stream().cls().discardedEventsHaveDefaultClockSnapshots()) {
            return msg.asDiscardedEvents().beginningDefaultClockSnapshot();
        }

        break;
    case bt2::MessageType::DISCARDED_PACKETS:
        if (msg.asDiscardedPackets().stream().cls().discardedPacketsHaveDefaultClockSnapshots()) {
            return msg.asDiscardedPackets().beginningDefaultClockSnapshot();
        }

        break;
    case bt2::MessageType::MESSAGE_ITERATOR_INACTIVITY:
        return msg.asMessageIteratorInactivity().clockSnapshot();
    case bt2::MessageType::STREAM_BEGINNING:
        if (msg.asStreamBeginning().stream().cls().defaultClockClass()) {
            return msg.asStreamBeginning().defaultClockSnapshot();
        }

        break;
    case bt2::MessageType::STREAM_END:
        if (msg.asStreamEnd().stream().cls().defaultClockClass()) {
            return msg.asStreamEnd().defaultClockSnapshot();
        }

        break;
    default:
        bt_common_abort();
    }

    return {};
}

} /* namespace */

UpstreamMsgIter::ReloadStatus UpstreamMsgIter::reload()
{
    BT_ASSERT_DBG(!_mDiscardRequired);

    if (G_UNLIKELY(!_mMsgs.msgs)) {
        /*
         * This will either:
         *
         * 1. Set `_mMsgs.msgs` to new messages (we'll return
         *    `ReloadStatus::MORE`).
         *
         * 2. Not set `_mMsgs.msgs` (ended, we'll return
         *    `ReloadStatus::NO_MORE`).
         *
         * 3. Throw.
         */
        this->_tryGetNewMsgs();
    }

    if (G_UNLIKELY(!_mMsgs.msgs)) {
        /* Still none: no more */
        _mMsgTs.reset();
        return ReloadStatus::NO_MORE;
    } else {
        if (const auto cs = msgCs(this->msg())) {
            _mMsgTs = cs->nsFromOrigin();
            BT_CPPLOGD("Cached the timestamp of the current message: this={}, ts={}",
                       fmt::ptr(this), *_mMsgTs);
        } else {
            _mMsgTs.reset();
            BT_CPPLOGD("Reset the timestamp of the current message: this={}", fmt::ptr(this));
        }

        _mDiscardRequired = true;
        return ReloadStatus::MORE;
    }
}

void UpstreamMsgIter::_tryGetNewMsgs()
{
    BT_ASSERT_DBG(_mMsgIter);
    BT_CPPLOGD("Calling the \"next\" method of the upstream message iterator: this={}",
               fmt::ptr(this));

    /*
     * Replace with next batch!
     *
     * This may throw, in which case we'll keep our current
     * `_mMsgs.msgs` (set), still requiring to get new messages the next
     * time the user calls reload().
     */
    _mMsgs.msgs = _mMsgIter->next();

    if (!_mMsgs.msgs) {
        /*
         * Don't destroy `*_mMsgIter` here because the user may still
         * call seekBeginning() afterwards.
         */
        BT_CPPLOGD("End of upstream message iterator: this={}", fmt::ptr(this));
        return;
    }

    _mMsgs.index = 0;
    BT_CPPLOGD("Got {1} messages from upstream: this={0}, count={1}", fmt::ptr(this),
               _mMsgs.msgs->length());
}

bool UpstreamMsgIter::canSeekBeginning()
{
    return _mMsgIter->canSeekBeginning();
}

void UpstreamMsgIter::seekBeginning()
{
    _mMsgIter->seekBeginning();
    _mMsgs.msgs.reset();
    _mMsgTs.reset();
    _mDiscardRequired = false;
}

bool UpstreamMsgIter::canSeekForward() const noexcept
{
    return _mMsgIter->canSeekForward();
}

} /* namespace bt2mux */
