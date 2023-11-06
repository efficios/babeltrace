
/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2017-2023 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_PLUGINS_UTILS_MUXER_UPSTREAM_MSG_ITER_HPP
#define BABELTRACE_PLUGINS_UTILS_MUXER_UPSTREAM_MSG_ITER_HPP

#include <memory>

#include "common/assert.h"
#include "cpp-common/bt2/message-array.hpp"
#include "cpp-common/bt2/message-iterator.hpp"
#include "cpp-common/bt2c/logging.hpp"
#include "cpp-common/bt2s/optional.hpp"

namespace bt2mux {

/*
 * An instance of this wraps an upstream libbabeltrace2 message
 * iterator, keeping an internal array of receives messages, and making
 * the oldest one available (msg() method).
 */
class UpstreamMsgIter final
{
public:
    /* Unique pointer to upstream message iterator */
    using UP = std::unique_ptr<UpstreamMsgIter>;

    /* Return type of reload() */
    enum class ReloadStatus
    {
        MORE,
        NO_MORE,
    };

    /*
     * Builds an upstream message iterator wrapper using the
     * libbabeltrace2 message iterator `msgIter`.
     *
     * This constructor doesn't immediately gets the next messages from
     * `*msgIter` (you always need to call reload() before you call
     * msg()), therefore it won't throw `bt2::Error` or `bt2::TryAgain`.
     */
    explicit UpstreamMsgIter(bt2::MessageIterator::Shared msgIter, std::string portName,
                             const bt2c::Logger& parentLogger);

    /* Some protection */
    UpstreamMsgIter(const UpstreamMsgIter&) = delete;
    UpstreamMsgIter& operator=(const UpstreamMsgIter&) = delete;

    /*
     * Current message.
     *
     * Before you call this method:
     *
     * 1. If needed, you must call discard().
     *
     *    This is not the case immediately after construction and
     *    immediately after seeking.
     *
     * 2. You must call reload() successfully (not ended).
     *
     *    This is always the case.
     *
     *    This makes it possible to build an `UpstreamMsgIter` instance
     *    without libbabeltrace2 message iterator exceptions.
     */
    bt2::ConstMessage msg() const noexcept
    {
        BT_ASSERT_DBG(_mMsgs.msgs && _mMsgs.index < _mMsgs.msgs->length());
        return (*_mMsgs.msgs)[_mMsgs.index];
    }

    /*
     * Timestamp, if any, of the current message.
     *
     * It must be valid to call msg() when you call this method.
     */
    const bt2s::optional<std::int64_t> msgTs() const noexcept
    {
        return _mMsgTs;
    }

    /*
     * Discards the current message, making this upstream message
     * iterator ready for a reload (reload()).
     *
     * You may only call reload() or seekBeginning() after having called
     * this.
     */
    void discard() noexcept
    {
        BT_ASSERT_DBG(_mMsgs.msgs && _mMsgs.index < _mMsgs.msgs->length());
        BT_ASSERT_DBG(_mDiscardRequired);
        _mDiscardRequired = false;
        ++_mMsgs.index;

        if (_mMsgs.index == _mMsgs.msgs->length()) {
            _mMsgs.msgs.reset();
        }
    }

    /*
     * Retrieves the next message, making it available afterwards
     * through the msg() method.
     *
     * You must have called discard() to discard the current message, if
     * any, before you call this method.
     *
     * This method may throw anything bt2::MessageIterator::next() may
     * throw.
     *
     * If this method returns `ReloadStatus::NO_MORE`, then the
     * underlying libbabeltrace2 message iterator is ended, meaning you
     * may not call msg(), msgTs(), or reload() again for this message
     * iterator until you successfully call seekBeginning().
     */
    ReloadStatus reload();

    /*
     * Forwards to bt2::MessageIterator::canSeekBeginning().
     */
    bool canSeekBeginning();

    /*
     * Forwards to bt2::MessageIterator::seekBeginning().
     *
     * On success, you may call reload() afterwards. With any exception,
     * you must call this method again, successfully, before you may
     * call reload().
     */
    void seekBeginning();

    /*
     * Forwards to bt2::MessageIterator::canSeekForward().
     */
    bool canSeekForward() const noexcept;

    /*
     * Name of the input port on which the libbabeltrace2 message
     * iterator was created.
     */
    const std::string& portName() const noexcept
    {
        return _mPortName;
    }

private:
    /*
     * Tries to get new messages into `_mMsgs.msgs`.
     */
    void _tryGetNewMsgs();

    /* Actual upstream message iterator */
    bt2::MessageIterator::Shared _mMsgIter;

    /*
     * Currently contained messages.
     *
     * `index` is the index of the current message (msg()/msgTs())
     * within `msgs`.
     */
    struct
    {
        bt2s::optional<bt2::ConstMessageArray> msgs;
        std::size_t index;
    } _mMsgs;

    /* Timestamp of the current message, if any */
    bt2s::optional<std::int64_t> _mMsgTs;

    /*
     * Only relevant in debug mode: true if a call to discard() is
     * required before calling reload().
     */
    bool _mDiscardRequired = false;

    bt2c::Logger _mLogger;
    std::string _mPortName;
};

} /* namespace bt2mux */

#endif /* BABELTRACE_PLUGINS_UTILS_MUXER_UPSTREAM_MSG_ITER_HPP */
