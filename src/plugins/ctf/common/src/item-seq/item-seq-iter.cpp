/*
 * Copyright (c) 2022 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <algorithm>

#include "common/assert.h"

#include "item-seq-iter.hpp"
#include "item.hpp"

namespace ctf::src {

using namespace bt2c::literals::datalen;

ItemSeqIter::ItemSeqIter(std::unique_ptr<Medium> medium, const TraceCls& traceCls,
                         const bt2c::Logger& parentLogger)
    : _mMedium {std::move(medium)},
      _mTraceCls {&traceCls},
      _mTraceClsSavedKeyValCountUpdatedObservableToken(
          _mTraceCls->savedKeyValCountUpdatedObservable().attach([this](const auto count) {
              this->_savedKeyValCountUpdated(count);
          })),
      _mLogger {parentLogger, "PLUGIN/CTF/ITEM-SEQ-ITER"}
{
    /* Allocate enough elements to save values for dependent fields */
    _mSavedKeyVals.resize(traceCls.savedKeyValCount());
}

ItemSeqIter::ItemSeqIter(std::unique_ptr<Medium> medium, const TraceCls& traceCls,
                         const bt2c::DataLen pktOffset, const bt2c::Logger& parentLogger)
    : ItemSeqIter {std::move(medium), traceCls, parentLogger}
{
    this->seekPkt(pktOffset);
}

ItemSeqIter::_StackFrame::_StackFrame(const _State restoringStateParam) noexcept
    : restoringState {restoringStateParam}
{
}

ItemSeqIter::_StackFrame::_StackFrame(const _State restoringStateParam, const Fc& fcParam) noexcept
    : restoringState {restoringStateParam},
      fc {&fcParam}
{
}

void ItemSeqIter::seekPkt(const bt2c::DataLen pktOffset)
{
    /* New packet offset within the whole item sequence */
    _mCurPktOffsetInItemSeq = pktOffset;

    /*
     * Reset the current buffer so as to make the next call to _tryHaveData()
     * request a new buffer at the beginning of this packet from the medium.
     */
    this->_mBuf = Buf {};

    /* Next: try to begin reading a packet */
    this->_state(_State::TryBeginReadPkt);
}

void ItemSeqIter::_updateDefClkVal(const unsigned long long val, const bt2c::DataLen len) noexcept
{
    /*
     * Special case for a 64-bit new value, which is the limit of a
     * clock value as of this version: overwrite the current value
     * directly.
     */
    if (len == 64_bits) {
        _mDefClkVal = val;
        return;
    }

    const auto newValMask = (1ULL << *len) - 1;

    {
        if (val < (_mDefClkVal & newValMask)) {
            /*
             * It looks like a wrap occurred on the number of bits of the
             * new value. Assume that the clock value wrapped only once.
             */
            _mDefClkVal += newValMask + 1;
        }
    }

    /* Clear the low bits of the current default clock value */
    _mDefClkVal &= ~newValMask;

    /* Set the low bits of the current default clock value */
    _mDefClkVal |= val;
}

void ItemSeqIter::_resetForNewPkt()
{
    _mCurClsId = std::nullopt;
    _mLastFixedLenBitArrayFieldByteOrder = std::nullopt;
    _mStack.clear();
    _mDefClkVal = 0;

    /* Reset decoding head to the beginning of the new packet */
    _mHeadOffsetInCurPkt = 0_bits;

    /*
     * Reset informative items as a given item sequence could contain
     * packets from data streams having different classes, therefore
     * having different packet context and event record header field
     * classes.
     */
    _mItems.dataStreamInfo._reset();
    _mItems.pktInfo._reset();
    _mItems.eventRecordInfo._reset();

    /*
     * Reset both expected total and content packet lengths to
     * "infinity" so that, if both are missing at the
     * `_State::SetPktInfoItem` state, then _remainingPktContentLen()
     * will always return a very large value so as to read the whole
     * medium data (the medium offers a single packet).
     */
    _mCurPktExpectedLens.total = this->_infDataLen();
    _mCurPktExpectedLens.content = this->_infDataLen();

    /*
     * Any state handler which calls _handleCommonVarLenIntFieldState()
     * may be reentered as is. This may happen if its
     * _requireContentData() call throws `bt2c::TryAgain`, for
     * example.
     *
     * This means there's no initial setup to read a variable-length
     * integer field: the state handlers just call
     * _handleCommonVarLenIntFieldState() to start _and_ to continue.
     *
     * Because of this, and because both `_mCurVarLenInt.val` and
     * `_mCurVarLenInt.len` must be zero before starting to read a
     * variable-length integer field, we reset them here for the first
     * variable-length integer field reading operation of this new
     * packet.
     *
     * _handleCommonVarLenIntFieldState() also resets both variables
     * when it finishes decoding a variable-length integer field.
     */
    _mCurVarLenInt.val = 0;
    _mCurVarLenInt.len = 0_bits;
}

void ItemSeqIter::_newBuf(const bt2c::DataLen offsetInItemSeq, const bt2c::DataLen minSize)
{
    BT_ASSERT_DBG(minSize <= 9_bytes);
    _mBuf = _mMedium->buf(offsetInItemSeq, minSize);
    _mBufOffsetInCurPkt = offsetInItemSeq - _mCurPktOffsetInItemSeq;

    /*
     * Make sure that the end offset of this buffer within the item
     * sequence is at most `_maxEndOffsetInItemSeq`: this is what makes
     * the `Dbg` operations of the offset helpers (for example,
     * _headOffsetInItemSeq()), as well as aligning the decoding head
     * (see _alignHead()), safe because the head is always within the
     * current buffer.
     */
    if (*offsetInItemSeq > _maxEndOffsetInItemSeq ||
        *_mBuf.size() > _maxEndOffsetInItemSeq - *offsetInItemSeq) {
        CTF_SRC_ITEM_SEQ_ITER_CPPLOGE_APPEND_CAUSE_AND_THROW(
            "end offset of medium buffer is greater than the maximum supported offset "
            "({} bits): buf-offset-in-item-seq-bits={}, buf-size-bits={}",
            _maxEndOffsetInItemSeq, *offsetInItemSeq, *_mBuf.size());
    }
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleInitState()
{
    /* Next: try to begin reading a packet */
    this->_state(_State::TryBeginReadPkt);
    return _StateHandlingReaction::Continue;
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleTryBeginReadPktState()
{
    /* This is a new packet */
    this->_resetForNewPkt();

    if (this->_remainingBufLen() == 0_bits) {
        /*
         * Try getting a single bit to see if we're at the end of the
         * item sequence.
         */
        if (!this->_tryHaveData(1_bits)) {
            /* No more data: no more packets */
            _mCurItem = nullptr;
            _mState = _State::Done;
            return _StateHandlingReaction::Stop;
        }
    }

    /* Update for user */
    this->_updateForUser(_mItems.pktBegin);

    /* Next: begin reading packet content */
    this->_state(_State::BeginReadPktContent);
    return _StateHandlingReaction::Stop;
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleEndReadPktState()
{
    /* Update for user */
    this->_updateForUser(_mItems.pktEnd);

    /* Adjust offsets */
    _mCurPktOffsetInItemSeq = this->_headOffsetInItemSeq();
    BT_ASSERT_DBG(!_mCurPktOffsetInItemSeq.hasExtraBits());
    _mHeadOffsetInCurPkt = 0_bits;

    /* Adjust current buffer for the next packet, if any */
    if (_mCurPktExpectedLens.total == this->_infDataLen()) {
        /* Item sequence contains a single packet: reset the buffer */
        _mBuf = Buf {};
    } else {
        /*
         * Make it so that the beginning of the buffer is the beginning
         * of the next packet to read.
         *
         * For example, before:
         *
         * ╔═══════════════════════════════════════════════════════════════════╗
         * ║ Packet: ▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒                    ║
         * ║         ┆                Buffer: ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓ ║
         * ║         ┆                        ┆           ┆                  ┆ ║
         * ║         ┣┅ _mBufOffsetInCurPkt ┅┅┫           ┆                  ┆ ║
         * ║         ┆                        ┣┅┅┅┅┅┅┅┅ _mBuf.size() ┅┅┅┅┅┅┅┅┫ ║
         * ║         ┣┅┅┅┅ _mCurPktExpectedLens.total ┅┅┅┅┫                    ║
         * ╚═══════════════════════════════════════════════════════════════════╝
         *
         * After (`_mBufOffsetInCurPkt` will be reset to zero):
         *
         * ╔═══════════════════════════════════════════════════════════════════╗
         * ║ Packet: ▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒                    ║
         * ║         ┆                            Buffer: ┆▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓ ║
         * ║         ┆                                    ┆┆                 ┆ ║
         * ║         ┆                                    ┆┣┅┅ _mBuf.size() ┅┫ ║
         * ║         ┣┅┅┅┅ _mCurPktExpectedLens.total ┅┅┅┅┫                    ║
         * ╚═══════════════════════════════════════════════════════════════════╝
         */
        _mBuf = _mBuf.slice(_mCurPktExpectedLens.total - _mBufOffsetInCurPkt);
    }

    _mBufOffsetInCurPkt = 0_bits;

    /* Next: try reading a packet */
    this->_state(_State::TryBeginReadPkt);
    return _StateHandlingReaction::Stop;
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleBeginReadPktContentState()
{
    /* Update for user */
    this->_updateForUser(_mItems.pktContentBegin);

    /* Next: try reading packet header field */
    this->_prepareToTryReadScope(_State::TryBeginReadPktHeaderScope, _State::EndReadPktHeaderScope,
                                 Scope::PktHeader, _mTraceCls->pktHeaderFc());
    return _StateHandlingReaction::Stop;
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleEndReadPktContentState()
{
    /* Update for user */
    this->_updateForUser(_mItems.pktContentEnd);

    /* Next step depends on whether or not there's a single packet */
    if (_mCurPktExpectedLens.total == this->_infDataLen()) {
        /* Single packet: next, end reading the packet */
        this->_state(_State::EndReadPkt);
    } else {
        /*
         * Compute the non-content padding data to skip to reach the end
         * of the packet.
         */
        BT_ASSERT_DBG(_mCurPktExpectedLens.content != this->_infDataLen());

        if (const auto lenToSkip = _mCurPktExpectedLens.total - _mHeadOffsetInCurPkt;
            lenToSkip > 0_bits) {
            /*
             * Set the state so as to skip padding, but also try to skip
             * all of it immediately.
             */
            _mRemainingLenToSkip = lenToSkip;
            _mPostSkipPaddingState = _State::EndReadPkt;
            this->_state(_State::SkipPadding);
            this->_skipPadding<false>();
        } else {
            /* No padding: next, end reading the packet*/
            this->_state(_State::EndReadPkt);
        }
    }

    return _StateHandlingReaction::Stop;
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleSkipPaddingState()
{
    this->_skipPadding<false>();

    /* Continue to `_mPostSkipPaddingState` */
    return _StateHandlingReaction::Continue;
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleSkipContentPaddingState()
{
    this->_skipPadding<true>();

    /* Continue to `_mPostSkipPaddingState` */
    return _StateHandlingReaction::Continue;
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleSetPktMagicNumberItem()
{
    /*
     * Update for user (the previous _handleUIntFieldRole() call already
     * set the value).
     */
    this->_updateForUser(_mItems.pktMagicNumber);

    /* Next: read next field */
    this->_prepareToReadNextField();
    return _StateHandlingReaction::Stop;
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleSetDefClkValItem()
{
    /*
     * Update for user (the previous _handleUIntFieldRole() call already
     * set the value).
     */
    this->_updateForUser(_mItems.defClkVal);

    /* Next: read next field */
    this->_prepareToReadNextField();
    return _StateHandlingReaction::Stop;
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleCommonBeginReadScopeState(const Scope scope)
{
    /* Update for user */
    _mItems.scopeBegin._mScope = scope;
    this->_updateForUser(_mItems.scopeBegin);

    /* Next: read the scope structure field */
    BT_ASSERT_DBG(_mCurScope.fc);
    this->_prepareToReadStructField(*_mCurScope.fc);
    return _StateHandlingReaction::Stop;
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleCommonEndReadScopeState(const Scope scope)
{
    /* Update for user */
    {
        _mItems.scopeEnd._mScope = scope;
        this->_updateForUser(_mItems.scopeEnd);
    }

    return _StateHandlingReaction::Stop;
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleTryBeginReadPktHeaderScopeState()
{
    if (!_mCurScope.fc) {
        /* No packet header field: set data stream info item immediately */
        this->_state(_State::SetDataStreamInfoItem);
        return _StateHandlingReaction::Continue;
    }

    return this->_handleCommonBeginReadScopeState(Scope::PktHeader);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleEndReadPktHeaderScopeState()
{
    /* Next: set data stream info item */
    this->_state(_State::SetDataStreamInfoItem);

    /* End reading packet header scope */
    return this->_handleCommonEndReadScopeState(Scope::PktHeader);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleTryBeginReadPktCtxScopeState()
{
    if (!_mCurScope.fc) {
        /* No packet context field: set packet info item immediately */
        this->_state(_State::SetPktInfoItem);
        return _StateHandlingReaction::Continue;
    }

    return this->_handleCommonBeginReadScopeState(Scope::PktCtx);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleEndReadPktCtxScopeState()
{
    /* Next: set packet info item */
    this->_state(_State::SetPktInfoItem);

    /* End reading packet context scope */
    return this->_handleCommonEndReadScopeState(Scope::PktCtx);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleTryBeginReadEventRecordHeaderScopeState()
{
    if (!_mCurScope.fc) {
        /*
         * No event record header field: set event record info item
         * immediately.
         */
        this->_state(_State::SetEventRecordInfoItem);
        return _StateHandlingReaction::Continue;
    }

    return this->_handleCommonBeginReadScopeState(Scope::EventRecordHeader);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleEndReadEventRecordHeaderScopeState()
{
    /* Next: set event record info item */
    this->_state(_State::SetEventRecordInfoItem);

    /* End reading event record header scope */
    return this->_handleCommonEndReadScopeState(Scope::EventRecordHeader);
}

void ItemSeqIter::_handleCommonAfterCommonEventRecordCtxScopeState()
{
    if (_mItems.eventRecordInfo._mCls) {
        /* Next: try reading specific context field */
        this->_prepareToTryReadScope(
            _State::TryBeginReadSpecEventRecordCtxScope, _State::EndReadSpecEventRecordCtxScope,
            Scope::SpecEventRecordCtx, _mItems.eventRecordInfo._mCls->specCtxFc());
    } else {
        /* Next: end event record */
        this->_state(_State::EndReadEventRecord);
    }
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleTryBeginReadCommonEventRecordCtxScopeState()
{
    if (!_mCurScope.fc) {
        /* No common event record context field */
        this->_handleCommonAfterCommonEventRecordCtxScopeState();
        return _StateHandlingReaction::Continue;
    }

    return this->_handleCommonBeginReadScopeState(Scope::CommonEventRecordCtx);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleEndReadCommonEventRecordCtxScopeState()
{
    this->_handleCommonAfterCommonEventRecordCtxScopeState();
    return this->_handleCommonEndReadScopeState(Scope::CommonEventRecordCtx);
}

void ItemSeqIter::_handleCommonAfterSpecEventRecordCtxScopeState()
{
    BT_ASSERT_DBG(_mItems.eventRecordInfo._mCls);

    /* Next: try reading payload field */
    this->_prepareToTryReadScope(_State::TryBeginReadEventRecordPayloadScope,
                                 _State::EndReadEventRecordPayloadScope, Scope::EventRecordPayload,
                                 _mItems.eventRecordInfo._mCls->payloadFc());
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleTryBeginReadSpecEventRecordCtxScopeState()
{
    if (!_mCurScope.fc) {
        /* No specific event record context field */
        this->_handleCommonAfterSpecEventRecordCtxScopeState();
        return _StateHandlingReaction::Continue;
    }

    return this->_handleCommonBeginReadScopeState(Scope::SpecEventRecordCtx);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleEndReadSpecEventRecordCtxScopeState()
{
    this->_handleCommonAfterSpecEventRecordCtxScopeState();
    return this->_handleCommonEndReadScopeState(Scope::SpecEventRecordCtx);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleTryBeginReadEventRecordPayloadScopeState()
{
    if (!_mCurScope.fc) {
        /* No event record payload field: end event record immediately */
        this->_state(_State::EndReadEventRecord);
        return _StateHandlingReaction::Continue;
    }

    return this->_handleCommonBeginReadScopeState(Scope::EventRecordPayload);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleEndReadEventRecordPayloadScopeState()
{
    /* Next: end reading event record */
    this->_state(_State::EndReadEventRecord);

    /* End reading event record payload scope */
    return this->_handleCommonEndReadScopeState(Scope::EventRecordPayload);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleSetDataStreamInfoItemState()
{
    /* Set data stream class, if any */
    if (_mCurClsId) {
        _mItems.dataStreamInfo._mCls = (*_mTraceCls)[*_mCurClsId];

        if (!_mItems.dataStreamInfo._mCls) {
            CTF_SRC_ITEM_SEQ_ITER_CPPLOGE_APPEND_CAUSE_AND_THROW(
                "no data stream class exists with ID {}", *_mCurClsId);
        }

        /*
         * Reset `_mCurClsId` because we also use this member for the
         * current event record class ID.
         */
        _mCurClsId = std::nullopt;
    } else {
        /*
         * If there's no current data stream class ID and our trace
         * class has a single data stream class, then use this one.
         */
        BT_ASSERT_DBG(_mTraceCls->size() <= 1);

        if (_mTraceCls->size() == 1) {
            _mItems.dataStreamInfo._mCls = _mTraceCls->begin()->get();
        }
    }

    /* Update for user */
    this->_updateForUser(_mItems.dataStreamInfo);

    /*
     * Next step depends on whether or not there's a current data stream
     * class.
     */
    if (_mItems.dataStreamInfo._mCls) {
        /* Next: try reading its packet context field */
        this->_prepareToTryReadScope(_State::TryBeginReadPktCtxScope, _State::EndReadPktCtxScope,
                                     Scope::PktCtx, _mItems.dataStreamInfo._mCls->pktCtxFc());
    } else {
        /* Next: end of packet content: set packet info item */
        this->_state(_State::SetPktInfoItem);
    }

    return _StateHandlingReaction::Stop;
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleSetPktInfoItemState()
{
    /* Automatically set expected packet lengths from each other */
    {
        if (_mCurPktExpectedLens.total == this->_infDataLen()) {
            _mCurPktExpectedLens.total = _mCurPktExpectedLens.content;
        }

        if (_mCurPktExpectedLens.content == this->_infDataLen()) {
            _mCurPktExpectedLens.content = _mCurPktExpectedLens.total;
        }
    }

    /* Validate expected packet lengths */
    {
        if (_mCurPktExpectedLens.total.hasExtraBits()) {
            CTF_SRC_ITEM_SEQ_ITER_CPPLOGE_APPEND_CAUSE_AND_THROW(
                "expected total length of current packet ({} bits) isn't a multiple of 8 bits.",
                *_mCurPktExpectedLens.total);
        }

        if (_mCurPktExpectedLens.content > _mCurPktExpectedLens.total) {
            CTF_SRC_ITEM_SEQ_ITER_CPPLOGE_APPEND_CAUSE_AND_THROW(
                "expected content length of current packet ({} bits) "
                "is greater than its expected total length ({} bits).",
                *_mCurPktExpectedLens.content, *_mCurPktExpectedLens.total);
        }

        /*
         * Make sure that the end offset of the current packet within
         * the item sequence is at most `_maxEndOffsetInItemSeq`.
         *
         * Because the decoding head is always within the current packet
         * from this point, this is what makes the `Dbg` operations
         * involving packet offsets and lengths, as well as aligning the
         * decoding head (see _alignHead()), safe.
         */
        if (_mCurPktExpectedLens.total != this->_infDataLen() &&
            (*_mCurPktOffsetInItemSeq > _maxEndOffsetInItemSeq ||
             *_mCurPktExpectedLens.total > _maxEndOffsetInItemSeq - *_mCurPktOffsetInItemSeq)) {
            CTF_SRC_ITEM_SEQ_ITER_CPPLOGE_APPEND_CAUSE_AND_THROW(
                "end offset of current packet is greater than the maximum supported offset "
                "({} bits): pkt-offset-in-item-seq-bits={}, expected-total-len-bits={}",
                _maxEndOffsetInItemSeq, *_mCurPktOffsetInItemSeq, *_mCurPktExpectedLens.total);
        }

        /*
         * Make sure that the packet header and context fields are
         * within the packet content.
         */
        if (_mHeadOffsetInCurPkt > _mCurPktExpectedLens.content) {
            CTF_SRC_ITEM_SEQ_ITER_CPPLOGE_APPEND_CAUSE_AND_THROW(
                "expected content length of current packet ({} bits) "
                "is less than the length of its header and context fields ({} bits).",
                *_mCurPktExpectedLens.content, *_mHeadOffsetInCurPkt);
        }
    }

    /* Update for user */
    if (_mItems.dataStreamInfo._mCls && _mItems.dataStreamInfo._mCls->pktCtxFcHasDefClkTsRole()) {
        _mItems.pktInfo._mBeginDefClkVal = _mDefClkVal;
    }

    this->_updateForUser(_mItems.pktInfo);

    /* Next: try reading an event record */
    this->_state(_State::TryBeginReadEventRecord);
    return _StateHandlingReaction::Stop;
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleSetEventRecordInfoItemState()
{
    BT_ASSERT_DBG(_mItems.dataStreamInfo._mCls);

    auto& dataStreamCls = *_mItems.dataStreamInfo._mCls;

    /* Set event record class, if any */
    if (_mCurClsId) {
        _mItems.eventRecordInfo._mCls = (*_mItems.dataStreamInfo._mCls)[*_mCurClsId];

        if (!_mItems.eventRecordInfo._mCls) {
            CTF_SRC_ITEM_SEQ_ITER_CPPLOGE_APPEND_CAUSE_AND_THROW(
                "no event record class exists with ID {} within the "
                "data stream class with ID {}.",
                *_mCurClsId, _mItems.dataStreamInfo._mCls->id());
        }
    } else {
        /*
         * If there's no current event record class ID and our current
         * data stream class has a single event record class, then use
         * this one.
         */
        BT_ASSERT_DBG(dataStreamCls.size() <= 1);

        if (dataStreamCls.size() == 1) {
            _mItems.eventRecordInfo._mCls = dataStreamCls.begin()->get();
        }
    }

    /* Update for user */
    if (dataStreamCls.defClkCls()) {
        _mItems.eventRecordInfo._mDefClkVal = _mDefClkVal;
    }

    this->_updateForUser(_mItems.eventRecordInfo);

    /* Next: try reading common event record context field */
    this->_prepareToTryReadScope(
        _State::TryBeginReadCommonEventRecordCtxScope, _State::EndReadCommonEventRecordCtxScope,
        Scope::CommonEventRecordCtx, dataStreamCls.commonEventRecordCtxFc());
    return _StateHandlingReaction::Stop;
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleTryBeginReadEventRecordState()
{
    /*
     * Next step depends on whether or not there's remaining packet
     * content data and whether or not it's a single-packet item
     * sequence.
     */
    BT_ASSERT_DBG(_mItems.dataStreamInfo._mCls);

    if (_mCurPktExpectedLens.content == this->_infDataLen()) {
        /* Single packet */
        if (this->_remainingBufLen() == 0_bits) {
            /*
             * Try having 1 bit to see if we're at the end of the
             * packet.
             */
            if (!this->_tryHaveData(1_bits)) {
                /* No more data: no more event records */
                this->_state(_State::EndReadPktContent);
                return _StateHandlingReaction::Continue;
            }
        }
    } else if (this->_remainingPktContentLen() == 0_bits) {
        /* End of packet content: no more event records */
        this->_state(_State::EndReadPktContent);
        return _StateHandlingReaction::Continue;
    }

    /* Update for user */
    this->_updateForUser(_mItems.eventRecordBegin);

    /* Next: try reading the event record header field */
    BT_ASSERT_DBG(this->_remainingPktContentLen() > 0_bits);
    _mCurEventRecordHeadOffsetInCurPkt = _mHeadOffsetInCurPkt;
    this->_prepareToTryReadScope(_State::TryBeginReadEventRecordHeaderScope,
                                 _State::EndReadEventRecordHeaderScope, Scope::EventRecordHeader,
                                 _mItems.dataStreamInfo._mCls->eventRecordHeaderFc());
    return _StateHandlingReaction::Stop;
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleEndReadEventRecordState()
{
    /* Validate that current event record has data */
    BT_ASSERT_DBG(_mHeadOffsetInCurPkt >= _mCurEventRecordHeadOffsetInCurPkt);

    if (_mHeadOffsetInCurPkt == _mCurEventRecordHeadOffsetInCurPkt) {
        CTF_SRC_ITEM_SEQ_ITER_CPPLOGE_APPEND_CAUSE_AND_THROW("Invalid empty event record.");
    }

    /* Update for user */
    this->_updateForUser(_mItems.eventRecordEnd);

    /* Next: try reading the next event record */
    this->_state(_State::TryBeginReadEventRecord);
    return _StateHandlingReaction::Stop;
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleBeginReadStructFieldState()
{
    /* Update for user */
    this->_setFieldItemFcAndUpdateForUser(_mItems.structFieldBegin, *this->_stackTop().fc);

    /* Structure field class */
    auto& structFc = this->_stackTop().fc->asStruct();

    /* Align head for structure field */
    this->_alignHead(structFc);

    /* Next step depends on whether or not the structure field is empty */
    if (structFc.isEmpty()) {
        /* Next: end reading the structure field */
        this->_restoreState();
    } else {
        /* Set length (member count) */
        this->_stackTop().len = structFc.size();

        /* Next: read the first struct field */
        this->_prepareToReadField(structFc.begin()->fc());
    }

    return _StateHandlingReaction::Stop;
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleEndReadStructFieldState()
{
    return this->_handleCommonEndReadCompoundFieldState(_mItems.structFieldEnd);
}

ItemSeqIter::_StateHandlingReaction
ItemSeqIter::_handleCommonBeginReadArrayFieldState(const unsigned long long len,
                                                   const ArrayFc& arrayFc)
{
    /* Align head for array field */
    this->_alignHead(arrayFc);

    /* Next step depends on whether or not the array field is empty */
    if (len == 0) {
        /* Next: end reading the array field */
        this->_restoreState();
    } else {
        /* Set length (element count) */
        this->_stackTop().len = len;

        /* Next: read the first element field */
        this->_prepareToReadField(arrayFc.elemFc());
    }

    return _StateHandlingReaction::Stop;
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleBeginReadStaticLenArrayFieldState()
{
    /* Update for user */
    this->_setFieldItemFcAndUpdateForUser(_mItems.staticLenArrayFieldBegin, *this->_stackTop().fc);

    /* Static-length array field class */
    auto& arrayFc = this->_stackTop().fc->asStaticLenArray();

    /* Begin reading static-length array field */
    return this->_handleCommonBeginReadArrayFieldState(arrayFc.len(), arrayFc);
}

ItemSeqIter::_StateHandlingReaction
ItemSeqIter::_handleBeginReadStaticLenArrayFieldMetadataStreamUuidState()
{
    /* Update for user */
    this->_setFieldItemFcAndUpdateForUser(_mItems.staticLenArrayFieldBegin, *this->_stackTop().fc);

    /* Static-length array field class */
    auto& arrayFc = this->_stackTop().fc->asStaticLenArray();

    BT_ASSERT_DBG(arrayFc.len() == _mItems.metadataStreamUuid._mUuid.size());

    /* Align head for array field */
    this->_alignHead(arrayFc);

    /* Next step: read the first byte field */
    _mCurScalarFc = &arrayFc.elemFc();
    this->_state(_State::ReadFixedLenMetadataStreamUuidByteUIntFieldBa8);

    return _StateHandlingReaction::Stop;
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleSetMetadataStreamUuidItemState()
{
    /* Update for user */
    this->_updateForUser(_mItems.metadataStreamUuid);

    /*
     * Next: end reading static-length metadata stream UUID array/BLOB
     * field.
     */
    this->_restoreState();
    return _StateHandlingReaction::Stop;
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleEndReadStaticLenArrayFieldState()
{
    return this->_handleCommonEndReadCompoundFieldState(_mItems.staticLenArrayFieldEnd);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleBeginReadDynLenArrayFieldState()
{
    /* Dynamic-length array field class */
    auto& arrayFc = this->_stackTop().fc->asDynLenArray();

    /* Get length of array field */
    const auto len = this->_savedUIntKeyVal(arrayFc);

    /* Update for user */
    this->_setFieldItemFcAndUpdateForUser(_mItems.dynLenArrayFieldBegin, *this->_stackTop().fc);
    _mItems.dynLenArrayFieldBegin._mLen = len;

    /* Begin reading dynamic-length array field */
    return this->_handleCommonBeginReadArrayFieldState(len, arrayFc);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleEndReadDynLenArrayFieldState()
{
    return this->_handleCommonEndReadCompoundFieldState(_mItems.dynLenArrayFieldEnd);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleBeginReadNullTerminatedStrFieldUtf8State()
{
    this->_handleCommonBeginReadNullTerminatedStrFieldState(
        _mUtf8NullCpFinder, _State::ReadSubstrUntilNullCodepointUtf8);
    return _StateHandlingReaction::Stop;
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleBeginReadNullTerminatedStrFieldUtf16State()
{
    this->_handleCommonBeginReadNullTerminatedStrFieldState(
        _mUtf16NullCpFinder, _State::ReadSubstrUntilNullCodepointUtf16);
    return _StateHandlingReaction::Stop;
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleBeginReadNullTerminatedStrFieldUtf32State()
{
    this->_handleCommonBeginReadNullTerminatedStrFieldState(
        _mUtf32NullCpFinder, _State::ReadSubstrUntilNullCodepointUtf32);
    return _StateHandlingReaction::Stop;
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleEndReadNullTerminatedStrFieldState()
{
    return this->_handleCommonEndReadCompoundFieldState(_mItems.nullTerminatedStrFieldEnd);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadSubstrUntilNullCodepointUtf8State()
{
    return this->_handleCommonReadSubstrUntilNullCodepointState(_mUtf8NullCpFinder);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadSubstrUntilNullCodepointUtf16State()
{
    return this->_handleCommonReadSubstrUntilNullCodepointState(_mUtf16NullCpFinder);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadSubstrUntilNullCodepointUtf32State()
{
    return this->_handleCommonReadSubstrUntilNullCodepointState(_mUtf32NullCpFinder);
}

ItemSeqIter::_StateHandlingReaction
ItemSeqIter::_handleCommonBeginReadStrBlobFieldState(const unsigned long long len,
                                                     const _State contentState, const Fc& fc)
{
    /* Align head for string/BLOB field */
    this->_alignHead(fc);

    /* Next step depends on whether or not the string/BLOB field is empty */
    if (len == 0) {
        /* Next: end reading the string/BLOB field */
        this->_restoreState();
    } else {
        /* Set length (byte count) */
        this->_stackTop().len = len;

        /* Next: new state */
        this->_state(contentState);
    }

    return _StateHandlingReaction::Stop;
}

bt2c::DataLen ItemSeqIter::_validatedDynLenStrBlobFieldLen(const unsigned long long len)
{
    /*
     * The string/BLOB field data must be within the remaining
     * packet content.
     *
     * Even when the expected content length is unknown (_infDataLen()),
     * `_remainingPktContentLen().bytes()` is less than 2^61, therefore
     * this check also makes DataLen::fromBytesDbg() below safe.
     */
    if (len > this->_remainingPktContentLen().bytes()) {
        CTF_SRC_ITEM_SEQ_ITER_CPPLOGE_APPEND_CAUSE_AND_THROW(
            "{} string/BLOB field bytes required at this point, "
            "but only {} bits of packet content remain.",
            len, *this->_remainingPktContentLen());
    }

    return bt2c::DataLen::fromBytesDbg(len);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleBeginReadStaticLenStrFieldState()
{
    /* Update for user */
    this->_setFieldItemFcAndUpdateForUser(_mItems.staticLenStrFieldBegin, *this->_stackTop().fc);

    /* Static-length string field class */
    auto& strFc = this->_stackTop().fc->asStaticLenStr();

    /* Begin reading static-length string field */
    return this->_handleCommonBeginReadStrBlobFieldState(strFc.len(), _State::ReadRawData, strFc);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleEndReadStaticLenStrFieldState()
{
    return this->_handleCommonEndReadCompoundFieldState(_mItems.staticLenStrFieldEnd);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleBeginReadDynLenStrFieldState()
{
    /* Dynamic-length string field class */
    auto& strFc = this->_stackTop().fc->asDynLenStr();

    /* Get length of string field */
    const auto len = this->_savedUIntKeyVal(strFc);

    /* Update for user */
    this->_setFieldItemFcAndUpdateForUser(_mItems.dynLenStrFieldBegin, *this->_stackTop().fc);
    _mItems.dynLenStrFieldBegin._mLen = this->_validatedDynLenStrBlobFieldLen(len);

    /* Begin reading dynamic-length string field */
    return this->_handleCommonBeginReadStrBlobFieldState(len, _State::ReadRawData, strFc);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleEndReadDynLenStrFieldState()
{
    return this->_handleCommonEndReadCompoundFieldState(_mItems.dynLenStrFieldEnd);
}

void ItemSeqIter::_handleCommonReadRawDataNoNextState()
{
    BT_ASSERT_DBG(!_mHeadOffsetInCurPkt.hasExtraBits());

    auto& top = this->_stackTop();

    BT_ASSERT_DBG(top.elemIndex < top.len);

    /* Require at least one byte of packet content */
    this->_requireContentData(1_bytes);
    BT_ASSERT_DBG(this->_remainingBufLen() >= 1_bytes);

    /* Set beginning and end pointers */
    const auto begin = this->_bufAtHead();
    const auto end = begin + std::min(this->_remainingBufLen().bytes(),
                                      static_cast<unsigned long long>(top.len - top.elemIndex));

    /*
     * Make sure the section is completely part of the packet content.
     *
     * Can't overflow: the section is within the current buffer.
     */
    const auto len = bt2c::DataLen::fromBytesDbg(end - begin);

    if (len > this->_remainingPktContentLen()) {
        CTF_SRC_ITEM_SEQ_ITER_CPPLOGE_APPEND_CAUSE_AND_THROW(
            "{} string/BLOB field bytes required at this point, "
            "but only {} bits of packet content remain.",
            len.bytes(), *this->_remainingPktContentLen());
    }

    /* Update for user */
    _mItems.rawData._assign(begin, end);
    BT_ASSERT_DBG(len >= 1_bytes);
    this->_updateForUser(_mItems.rawData);

    /* Mark the section as consumed */
    this->_consumeAvailData(len);

    /* Update `top.elemIndex` */
    top.elemIndex += len.bytes();
    BT_ASSERT_DBG(top.elemIndex <= top.len);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadRawDataState()
{
    this->_handleCommonReadRawDataNoNextState();

    if (this->_stackTop().elemIndex == this->_stackTop().len) {
        /* Next: end reading string/BLOB field */
        this->_restoreState();
    }

    return _StateHandlingReaction::Stop;
}

ItemSeqIter::_StateHandlingReaction
ItemSeqIter::_handleCommonBeginReadStaticLenBlobFieldState(const _State contentState)
{
    /* Update for user */
    this->_setFieldItemFcAndUpdateForUser(_mItems.staticLenBlobFieldBegin, *this->_stackTop().fc);

    /* Static-length BLOB field class */
    auto& blobFc = this->_stackTop().fc->asStaticLenBlob();

    /* Begin reading static-length BLOB field */
    return this->_handleCommonBeginReadStrBlobFieldState(blobFc.len(), contentState, blobFc);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleBeginReadStaticLenBlobFieldState()
{
    return this->_handleCommonBeginReadStaticLenBlobFieldState(_State::ReadRawData);
}

ItemSeqIter::_StateHandlingReaction
ItemSeqIter::_handleBeginReadStaticLenBlobFieldMetadataStreamUuidState()
{
    return this->_handleCommonBeginReadStaticLenBlobFieldState(
        _State::ReadMetadataStreamUuidBlobFieldSection);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleEndReadStaticLenBlobFieldState()
{
    return this->_handleCommonEndReadCompoundFieldState(_mItems.staticLenBlobFieldEnd);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleBeginReadDynLenBlobFieldState()
{
    /* Dynamic-length BLOB field class */
    auto& blobFc = this->_stackTop().fc->asDynLenBlob();

    /* Get length of BLOB field */
    const auto len = this->_savedUIntKeyVal(blobFc);

    /* Update for user */
    this->_setFieldItemFcAndUpdateForUser(_mItems.dynLenBlobFieldBegin, *this->_stackTop().fc);
    _mItems.dynLenBlobFieldBegin._mLen = this->_validatedDynLenStrBlobFieldLen(len);

    /* Begin reading dynamic-length BLOB field */
    return this->_handleCommonBeginReadStrBlobFieldState(len, _State::ReadRawData, blobFc);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleEndReadDynLenBlobFieldState()
{
    return this->_handleCommonEndReadCompoundFieldState(_mItems.dynLenBlobFieldEnd);
}

ItemSeqIter::_StateHandlingReaction
ItemSeqIter::_handleReadMetadataStreamUuidBlobFieldSectionState()
{
    const auto uuidByteIndex = this->_stackTop().elemIndex;

    this->_handleCommonReadRawDataNoNextState();

    /*
     * Set current metadata stream UUID bytes from current BLOB section.
     */
    BT_ASSERT_DBG(uuidByteIndex + _mItems.rawData.data().size() <=
                  _mItems.metadataStreamUuid._mUuid.size());
    std::copy(_mItems.rawData.data().begin(), _mItems.rawData.data().end(),
              _mCurMetadataStreamUuid.data() + uuidByteIndex);

    /*
     * Next step depends on whether or not we're done reading all the
     * metadata stream UUID bytes.
     */
    if (this->_stackTop().elemIndex == _mItems.metadataStreamUuid._mUuid.size()) {
        /* Update for user */
        _mItems.metadataStreamUuid._mUuid = bt2c::Uuid {_mCurMetadataStreamUuid.data()};

        /* Next: set metadata stream UUID item */
        this->_state(_State::SetMetadataStreamUuidItem);
    }

    return _StateHandlingReaction::Stop;
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleBeginReadVariantFieldWithUIntSelState()
{
    return this->_handleCommonBeginReadVariantField<VariantWithUIntSelFc>(
        _mItems.variantFieldWithUIntSelBegin);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleEndReadVariantFieldWithUIntSelState()
{
    return this->_handleCommonEndReadCompoundFieldState(_mItems.variantFieldWithUIntSelEnd);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleBeginReadVariantFieldWithSIntSelState()
{
    return this->_handleCommonBeginReadVariantField<VariantWithSIntSelFc>(
        _mItems.variantFieldWithSIntSelBegin);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleEndReadVariantFieldWithSIntSelState()
{
    return this->_handleCommonEndReadCompoundFieldState(_mItems.variantFieldWithSIntSelEnd);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleBeginReadOptionalFieldWithBoolSelState()
{
    return this->_handleCommonBeginReadOptionalField<OptionalWithBoolSelFc>(
        _mItems.optionalFieldWithBoolSelBegin);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleEndReadOptionalFieldWithBoolSelState()
{
    return this->_handleCommonEndReadCompoundFieldState(_mItems.optionalFieldWithBoolSelEnd);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleBeginReadOptionalFieldWithUIntSelState()
{
    return this->_handleCommonBeginReadOptionalField<OptionalWithUIntSelFc>(
        _mItems.optionalFieldWithUIntSelBegin);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleEndReadOptionalFieldWithUIntSelState()
{
    return this->_handleCommonEndReadCompoundFieldState(_mItems.optionalFieldWithUIntSelEnd);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleBeginReadOptionalFieldWithSIntSelState()
{
    return this->_handleCommonBeginReadOptionalField<OptionalWithSIntSelFc>(
        _mItems.optionalFieldWithSIntSelBegin);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleEndReadOptionalFieldWithSIntSelState()
{
    return this->_handleCommonEndReadCompoundFieldState(_mItems.optionalFieldWithSIntSelEnd);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBitArrayFieldBeState()
{
    return this->_handleCommonReadFixedLenBitArrayFieldState<0, ByteOrder::Big,
                                                             internal::BitOrder::Natural>(
        _mItems.fixedLenBitArrayField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBitArrayFieldLeState()
{
    return this->_handleCommonReadFixedLenBitArrayFieldState<0, ByteOrder::Little,
                                                             internal::BitOrder::Natural>(
        _mItems.fixedLenBitArrayField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBitArrayFieldBa8State()
{
    return this->_handleCommonReadFixedLenBitArrayFieldState<8, ByteOrder::Big,
                                                             internal::BitOrder::Natural>(
        _mItems.fixedLenBitArrayField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBitArrayFieldBa16LeState()
{
    return this->_handleCommonReadFixedLenBitArrayFieldState<16, ByteOrder::Little,
                                                             internal::BitOrder::Natural>(
        _mItems.fixedLenBitArrayField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBitArrayFieldBa16BeState()
{
    return this->_handleCommonReadFixedLenBitArrayFieldState<16, ByteOrder::Big,
                                                             internal::BitOrder::Natural>(
        _mItems.fixedLenBitArrayField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBitArrayFieldBa32LeState()
{
    return this->_handleCommonReadFixedLenBitArrayFieldState<32, ByteOrder::Little,
                                                             internal::BitOrder::Natural>(
        _mItems.fixedLenBitArrayField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBitArrayFieldBa32BeState()
{
    return this->_handleCommonReadFixedLenBitArrayFieldState<32, ByteOrder::Big,
                                                             internal::BitOrder::Natural>(
        _mItems.fixedLenBitArrayField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBitArrayFieldBa64LeState()
{
    return this->_handleCommonReadFixedLenBitArrayFieldState<64, ByteOrder::Little,
                                                             internal::BitOrder::Natural>(
        _mItems.fixedLenBitArrayField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBitArrayFieldBa64BeState()
{
    return this->_handleCommonReadFixedLenBitArrayFieldState<64, ByteOrder::Big,
                                                             internal::BitOrder::Natural>(
        _mItems.fixedLenBitArrayField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBitMapFieldBeState()
{
    return this->_handleCommonReadFixedLenBitArrayFieldState<0, ByteOrder::Big,
                                                             internal::BitOrder::Natural>(
        _mItems.fixedLenBitMapField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBitMapFieldLeState()
{
    return this->_handleCommonReadFixedLenBitArrayFieldState<0, ByteOrder::Little,
                                                             internal::BitOrder::Natural>(
        _mItems.fixedLenBitMapField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBitMapFieldBa8State()
{
    return this->_handleCommonReadFixedLenBitArrayFieldState<8, ByteOrder::Big,
                                                             internal::BitOrder::Natural>(
        _mItems.fixedLenBitMapField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBitMapFieldBa16LeState()
{
    return this->_handleCommonReadFixedLenBitArrayFieldState<16, ByteOrder::Little,
                                                             internal::BitOrder::Natural>(
        _mItems.fixedLenBitMapField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBitMapFieldBa16BeState()
{
    return this->_handleCommonReadFixedLenBitArrayFieldState<16, ByteOrder::Big,
                                                             internal::BitOrder::Natural>(
        _mItems.fixedLenBitMapField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBitMapFieldBa32LeState()
{
    return this->_handleCommonReadFixedLenBitArrayFieldState<32, ByteOrder::Little,
                                                             internal::BitOrder::Natural>(
        _mItems.fixedLenBitMapField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBitMapFieldBa32BeState()
{
    return this->_handleCommonReadFixedLenBitArrayFieldState<32, ByteOrder::Big,
                                                             internal::BitOrder::Natural>(
        _mItems.fixedLenBitMapField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBitMapFieldBa64LeState()
{
    return this->_handleCommonReadFixedLenBitArrayFieldState<64, ByteOrder::Little,
                                                             internal::BitOrder::Natural>(
        _mItems.fixedLenBitMapField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBitMapFieldBa64BeState()
{
    return this->_handleCommonReadFixedLenBitArrayFieldState<64, ByteOrder::Big,
                                                             internal::BitOrder::Natural>(
        _mItems.fixedLenBitMapField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBoolFieldBeState()
{
    return this->_handleCommonReadFixedLenBoolFieldState<0, ByteOrder::Big,
                                                         internal::BitOrder::Natural, _SaveVal::No>(
        _mItems.fixedLenBoolField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBoolFieldLeState()
{
    return this->_handleCommonReadFixedLenBoolFieldState<0, ByteOrder::Little,
                                                         internal::BitOrder::Natural, _SaveVal::No>(
        _mItems.fixedLenBoolField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBoolFieldBa8State()
{
    return this->_handleCommonReadFixedLenBoolFieldState<8, ByteOrder::Big,
                                                         internal::BitOrder::Natural, _SaveVal::No>(
        _mItems.fixedLenBoolField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBoolFieldBa16LeState()
{
    return this->_handleCommonReadFixedLenBoolFieldState<16, ByteOrder::Little,
                                                         internal::BitOrder::Natural, _SaveVal::No>(
        _mItems.fixedLenBoolField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBoolFieldBa16BeState()
{
    return this->_handleCommonReadFixedLenBoolFieldState<16, ByteOrder::Big,
                                                         internal::BitOrder::Natural, _SaveVal::No>(
        _mItems.fixedLenBoolField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBoolFieldBa32LeState()
{
    return this->_handleCommonReadFixedLenBoolFieldState<32, ByteOrder::Little,
                                                         internal::BitOrder::Natural, _SaveVal::No>(
        _mItems.fixedLenBoolField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBoolFieldBa32BeState()
{
    return this->_handleCommonReadFixedLenBoolFieldState<32, ByteOrder::Big,
                                                         internal::BitOrder::Natural, _SaveVal::No>(
        _mItems.fixedLenBoolField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBoolFieldBa64LeState()
{
    return this->_handleCommonReadFixedLenBoolFieldState<64, ByteOrder::Little,
                                                         internal::BitOrder::Natural, _SaveVal::No>(
        _mItems.fixedLenBoolField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBoolFieldBa64BeState()
{
    return this->_handleCommonReadFixedLenBoolFieldState<64, ByteOrder::Big,
                                                         internal::BitOrder::Natural, _SaveVal::No>(
        _mItems.fixedLenBoolField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBoolFieldBeSaveValState()
{
    return this->_handleCommonReadFixedLenBoolFieldState<
        0, ByteOrder::Big, internal::BitOrder::Natural, _SaveVal::Yes>(_mItems.fixedLenBoolField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBoolFieldLeSaveValState()
{
    return this->_handleCommonReadFixedLenBoolFieldState<
        0, ByteOrder::Little, internal::BitOrder::Natural, _SaveVal::Yes>(
        _mItems.fixedLenBoolField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBoolFieldBa8SaveValState()
{
    return this->_handleCommonReadFixedLenBoolFieldState<
        8, ByteOrder::Big, internal::BitOrder::Natural, _SaveVal::Yes>(_mItems.fixedLenBoolField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBoolFieldBa16LeSaveValState()
{
    return this->_handleCommonReadFixedLenBoolFieldState<
        16, ByteOrder::Little, internal::BitOrder::Natural, _SaveVal::Yes>(
        _mItems.fixedLenBoolField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBoolFieldBa16BeSaveValState()
{
    return this->_handleCommonReadFixedLenBoolFieldState<
        16, ByteOrder::Big, internal::BitOrder::Natural, _SaveVal::Yes>(_mItems.fixedLenBoolField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBoolFieldBa32LeSaveValState()
{
    return this->_handleCommonReadFixedLenBoolFieldState<
        32, ByteOrder::Little, internal::BitOrder::Natural, _SaveVal::Yes>(
        _mItems.fixedLenBoolField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBoolFieldBa32BeSaveValState()
{
    return this->_handleCommonReadFixedLenBoolFieldState<
        32, ByteOrder::Big, internal::BitOrder::Natural, _SaveVal::Yes>(_mItems.fixedLenBoolField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBoolFieldBa64LeSaveValState()
{
    return this->_handleCommonReadFixedLenBoolFieldState<
        64, ByteOrder::Little, internal::BitOrder::Natural, _SaveVal::Yes>(
        _mItems.fixedLenBoolField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBoolFieldBa64BeSaveValState()
{
    return this->_handleCommonReadFixedLenBoolFieldState<
        64, ByteOrder::Big, internal::BitOrder::Natural, _SaveVal::Yes>(_mItems.fixedLenBoolField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenFloatField32BeState()
{
    return this->_handleCommonReadFixedLenFloatFieldState<0, ByteOrder::Big,
                                                          internal::BitOrder::Natural, float>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenFloatField32LeState()
{
    return this->_handleCommonReadFixedLenFloatFieldState<0, ByteOrder::Little,
                                                          internal::BitOrder::Natural, float>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenFloatField64BeState()
{
    return this->_handleCommonReadFixedLenFloatFieldState<0, ByteOrder::Big,
                                                          internal::BitOrder::Natural, double>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenFloatField64LeState()
{
    return this->_handleCommonReadFixedLenFloatFieldState<0, ByteOrder::Little,
                                                          internal::BitOrder::Natural, double>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenFloatFieldBa32LeState()
{
    return this->_handleCommonReadFixedLenFloatFieldState<32, ByteOrder::Little,
                                                          internal::BitOrder::Natural, float>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenFloatFieldBa32BeState()
{
    return this->_handleCommonReadFixedLenFloatFieldState<32, ByteOrder::Big,
                                                          internal::BitOrder::Natural, float>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenFloatFieldBa64LeState()
{
    return this->_handleCommonReadFixedLenFloatFieldState<64, ByteOrder::Little,
                                                          internal::BitOrder::Natural, double>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenFloatFieldBa64BeState()
{
    return this->_handleCommonReadFixedLenFloatFieldState<64, ByteOrder::Big,
                                                          internal::BitOrder::Natural, double>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBeState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 0, ByteOrder::Big,
                                                         internal::BitOrder::Natural, _WithRole::No,
                                                         _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldLeState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 0, ByteOrder::Little,
                                                         internal::BitOrder::Natural, _WithRole::No,
                                                         _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBa8State()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 8, ByteOrder::Big,
                                                         internal::BitOrder::Natural, _WithRole::No,
                                                         _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBa16LeState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 16, ByteOrder::Little,
                                                         internal::BitOrder::Natural, _WithRole::No,
                                                         _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBa16BeState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 16, ByteOrder::Big,
                                                         internal::BitOrder::Natural, _WithRole::No,
                                                         _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBa32LeState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 32, ByteOrder::Little,
                                                         internal::BitOrder::Natural, _WithRole::No,
                                                         _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBa32BeState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 32, ByteOrder::Big,
                                                         internal::BitOrder::Natural, _WithRole::No,
                                                         _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBa64LeState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 64, ByteOrder::Little,
                                                         internal::BitOrder::Natural, _WithRole::No,
                                                         _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBa64BeState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 64, ByteOrder::Big,
                                                         internal::BitOrder::Natural, _WithRole::No,
                                                         _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBeWithRoleState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 0, ByteOrder::Big,
                                                         internal::BitOrder::Natural,
                                                         _WithRole::Yes, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldLeWithRoleState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 0, ByteOrder::Little,
                                                         internal::BitOrder::Natural,
                                                         _WithRole::Yes, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBa8WithRoleState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 8, ByteOrder::Big,
                                                         internal::BitOrder::Natural,
                                                         _WithRole::Yes, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBa16LeWithRoleState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 16, ByteOrder::Little,
                                                         internal::BitOrder::Natural,
                                                         _WithRole::Yes, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBa16BeWithRoleState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 16, ByteOrder::Big,
                                                         internal::BitOrder::Natural,
                                                         _WithRole::Yes, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBa32LeWithRoleState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 32, ByteOrder::Little,
                                                         internal::BitOrder::Natural,
                                                         _WithRole::Yes, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBa32BeWithRoleState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 32, ByteOrder::Big,
                                                         internal::BitOrder::Natural,
                                                         _WithRole::Yes, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBa64LeWithRoleState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 64, ByteOrder::Little,
                                                         internal::BitOrder::Natural,
                                                         _WithRole::Yes, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBa64BeWithRoleState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 64, ByteOrder::Big,
                                                         internal::BitOrder::Natural,
                                                         _WithRole::Yes, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBeSaveValState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 0, ByteOrder::Big,
                                                         internal::BitOrder::Natural, _WithRole::No,
                                                         _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldLeSaveValState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 0, ByteOrder::Little,
                                                         internal::BitOrder::Natural, _WithRole::No,
                                                         _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBa8SaveValState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 8, ByteOrder::Big,
                                                         internal::BitOrder::Natural, _WithRole::No,
                                                         _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBa16LeSaveValState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 16, ByteOrder::Little,
                                                         internal::BitOrder::Natural, _WithRole::No,
                                                         _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBa16BeSaveValState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 16, ByteOrder::Big,
                                                         internal::BitOrder::Natural, _WithRole::No,
                                                         _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBa32LeSaveValState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 32, ByteOrder::Little,
                                                         internal::BitOrder::Natural, _WithRole::No,
                                                         _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBa32BeSaveValState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 32, ByteOrder::Big,
                                                         internal::BitOrder::Natural, _WithRole::No,
                                                         _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBa64LeSaveValState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 64, ByteOrder::Little,
                                                         internal::BitOrder::Natural, _WithRole::No,
                                                         _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBa64BeSaveValState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 64, ByteOrder::Big,
                                                         internal::BitOrder::Natural, _WithRole::No,
                                                         _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction
ItemSeqIter::_handleReadFixedLenUIntFieldBeWithRoleSaveValState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 0, ByteOrder::Big,
                                                         internal::BitOrder::Natural,
                                                         _WithRole::Yes, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction
ItemSeqIter::_handleReadFixedLenUIntFieldLeWithRoleSaveValState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 0, ByteOrder::Little,
                                                         internal::BitOrder::Natural,
                                                         _WithRole::Yes, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction
ItemSeqIter::_handleReadFixedLenUIntFieldBa8WithRoleSaveValState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 8, ByteOrder::Big,
                                                         internal::BitOrder::Natural,
                                                         _WithRole::Yes, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction
ItemSeqIter::_handleReadFixedLenUIntFieldBa16LeWithRoleSaveValState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 16, ByteOrder::Little,
                                                         internal::BitOrder::Natural,
                                                         _WithRole::Yes, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction
ItemSeqIter::_handleReadFixedLenUIntFieldBa16BeWithRoleSaveValState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 16, ByteOrder::Big,
                                                         internal::BitOrder::Natural,
                                                         _WithRole::Yes, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction
ItemSeqIter::_handleReadFixedLenUIntFieldBa32LeWithRoleSaveValState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 32, ByteOrder::Little,
                                                         internal::BitOrder::Natural,
                                                         _WithRole::Yes, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction
ItemSeqIter::_handleReadFixedLenUIntFieldBa32BeWithRoleSaveValState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 32, ByteOrder::Big,
                                                         internal::BitOrder::Natural,
                                                         _WithRole::Yes, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction
ItemSeqIter::_handleReadFixedLenUIntFieldBa64LeWithRoleSaveValState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 64, ByteOrder::Little,
                                                         internal::BitOrder::Natural,
                                                         _WithRole::Yes, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction
ItemSeqIter::_handleReadFixedLenUIntFieldBa64BeWithRoleSaveValState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 64, ByteOrder::Big,
                                                         internal::BitOrder::Natural,
                                                         _WithRole::Yes, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenSIntFieldBeState()
{
    return this->_handleCommonReadFixedLenSIntFieldState<
        FixedLenSIntFc, 0, ByteOrder::Big, internal::BitOrder::Natural, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenSIntFieldLeState()
{
    return this->_handleCommonReadFixedLenSIntFieldState<
        FixedLenSIntFc, 0, ByteOrder::Little, internal::BitOrder::Natural, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenSIntFieldBa8State()
{
    return this->_handleCommonReadFixedLenSIntFieldState<
        FixedLenSIntFc, 8, ByteOrder::Big, internal::BitOrder::Natural, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenSIntFieldBa16LeState()
{
    return this->_handleCommonReadFixedLenSIntFieldState<
        FixedLenSIntFc, 16, ByteOrder::Little, internal::BitOrder::Natural, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenSIntFieldBa16BeState()
{
    return this->_handleCommonReadFixedLenSIntFieldState<
        FixedLenSIntFc, 16, ByteOrder::Big, internal::BitOrder::Natural, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenSIntFieldBa32LeState()
{
    return this->_handleCommonReadFixedLenSIntFieldState<
        FixedLenSIntFc, 32, ByteOrder::Little, internal::BitOrder::Natural, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenSIntFieldBa32BeState()
{
    return this->_handleCommonReadFixedLenSIntFieldState<
        FixedLenSIntFc, 32, ByteOrder::Big, internal::BitOrder::Natural, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenSIntFieldBa64LeState()
{
    return this->_handleCommonReadFixedLenSIntFieldState<
        FixedLenSIntFc, 64, ByteOrder::Little, internal::BitOrder::Natural, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenSIntFieldBa64BeState()
{
    return this->_handleCommonReadFixedLenSIntFieldState<
        FixedLenSIntFc, 64, ByteOrder::Big, internal::BitOrder::Natural, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenSIntFieldBeSaveValState()
{
    return this->_handleCommonReadFixedLenSIntFieldState<
        FixedLenSIntFc, 0, ByteOrder::Big, internal::BitOrder::Natural, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenSIntFieldLeSaveValState()
{
    return this->_handleCommonReadFixedLenSIntFieldState<
        FixedLenSIntFc, 0, ByteOrder::Little, internal::BitOrder::Natural, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenSIntFieldBa8SaveValState()
{
    return this->_handleCommonReadFixedLenSIntFieldState<
        FixedLenSIntFc, 8, ByteOrder::Big, internal::BitOrder::Natural, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenSIntFieldBa16LeSaveValState()
{
    return this->_handleCommonReadFixedLenSIntFieldState<
        FixedLenSIntFc, 16, ByteOrder::Little, internal::BitOrder::Natural, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenSIntFieldBa16BeSaveValState()
{
    return this->_handleCommonReadFixedLenSIntFieldState<
        FixedLenSIntFc, 16, ByteOrder::Big, internal::BitOrder::Natural, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenSIntFieldBa32LeSaveValState()
{
    return this->_handleCommonReadFixedLenSIntFieldState<
        FixedLenSIntFc, 32, ByteOrder::Little, internal::BitOrder::Natural, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenSIntFieldBa32BeSaveValState()
{
    return this->_handleCommonReadFixedLenSIntFieldState<
        FixedLenSIntFc, 32, ByteOrder::Big, internal::BitOrder::Natural, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenSIntFieldBa64LeSaveValState()
{
    return this->_handleCommonReadFixedLenSIntFieldState<
        FixedLenSIntFc, 64, ByteOrder::Little, internal::BitOrder::Natural, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenSIntFieldBa64BeSaveValState()
{
    return this->_handleCommonReadFixedLenSIntFieldState<
        FixedLenSIntFc, 64, ByteOrder::Big, internal::BitOrder::Natural, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBitArrayFieldBeRevState()
{
    return this->_handleCommonReadFixedLenBitArrayFieldState<0, ByteOrder::Big,
                                                             internal::BitOrder::Reversed>(
        _mItems.fixedLenBitArrayField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBitArrayFieldLeRevState()
{
    return this->_handleCommonReadFixedLenBitArrayFieldState<0, ByteOrder::Little,
                                                             internal::BitOrder::Reversed>(
        _mItems.fixedLenBitArrayField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBitArrayFieldBa8RevState()
{
    return this->_handleCommonReadFixedLenBitArrayFieldState<8, ByteOrder::Big,
                                                             internal::BitOrder::Reversed>(
        _mItems.fixedLenBitArrayField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBitArrayFieldBa16LeRevState()
{
    return this->_handleCommonReadFixedLenBitArrayFieldState<16, ByteOrder::Little,
                                                             internal::BitOrder::Reversed>(
        _mItems.fixedLenBitArrayField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBitArrayFieldBa16BeRevState()
{
    return this->_handleCommonReadFixedLenBitArrayFieldState<16, ByteOrder::Big,
                                                             internal::BitOrder::Reversed>(
        _mItems.fixedLenBitArrayField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBitArrayFieldBa32LeRevState()
{
    return this->_handleCommonReadFixedLenBitArrayFieldState<32, ByteOrder::Little,
                                                             internal::BitOrder::Reversed>(
        _mItems.fixedLenBitArrayField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBitArrayFieldBa32BeRevState()
{
    return this->_handleCommonReadFixedLenBitArrayFieldState<32, ByteOrder::Big,
                                                             internal::BitOrder::Reversed>(
        _mItems.fixedLenBitArrayField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBitArrayFieldBa64LeRevState()
{
    return this->_handleCommonReadFixedLenBitArrayFieldState<64, ByteOrder::Little,
                                                             internal::BitOrder::Reversed>(
        _mItems.fixedLenBitArrayField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBitArrayFieldBa64BeRevState()
{
    return this->_handleCommonReadFixedLenBitArrayFieldState<64, ByteOrder::Big,
                                                             internal::BitOrder::Reversed>(
        _mItems.fixedLenBitArrayField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBitMapFieldBeRevState()
{
    return this->_handleCommonReadFixedLenBitArrayFieldState<0, ByteOrder::Big,
                                                             internal::BitOrder::Reversed>(
        _mItems.fixedLenBitMapField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBitMapFieldLeRevState()
{
    return this->_handleCommonReadFixedLenBitArrayFieldState<0, ByteOrder::Little,
                                                             internal::BitOrder::Reversed>(
        _mItems.fixedLenBitMapField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBitMapFieldBa8RevState()
{
    return this->_handleCommonReadFixedLenBitArrayFieldState<8, ByteOrder::Big,
                                                             internal::BitOrder::Reversed>(
        _mItems.fixedLenBitMapField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBitMapFieldBa16LeRevState()
{
    return this->_handleCommonReadFixedLenBitArrayFieldState<16, ByteOrder::Little,
                                                             internal::BitOrder::Reversed>(
        _mItems.fixedLenBitMapField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBitMapFieldBa16BeRevState()
{
    return this->_handleCommonReadFixedLenBitArrayFieldState<16, ByteOrder::Big,
                                                             internal::BitOrder::Reversed>(
        _mItems.fixedLenBitMapField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBitMapFieldBa32LeRevState()
{
    return this->_handleCommonReadFixedLenBitArrayFieldState<32, ByteOrder::Little,
                                                             internal::BitOrder::Reversed>(
        _mItems.fixedLenBitMapField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBitMapFieldBa32BeRevState()
{
    return this->_handleCommonReadFixedLenBitArrayFieldState<32, ByteOrder::Big,
                                                             internal::BitOrder::Reversed>(
        _mItems.fixedLenBitMapField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBitMapFieldBa64LeRevState()
{
    return this->_handleCommonReadFixedLenBitArrayFieldState<64, ByteOrder::Little,
                                                             internal::BitOrder::Reversed>(
        _mItems.fixedLenBitMapField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBitMapFieldBa64BeRevState()
{
    return this->_handleCommonReadFixedLenBitArrayFieldState<64, ByteOrder::Big,
                                                             internal::BitOrder::Reversed>(
        _mItems.fixedLenBitMapField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBoolFieldBeRevState()
{
    return this->_handleCommonReadFixedLenBoolFieldState<
        0, ByteOrder::Big, internal::BitOrder::Reversed, _SaveVal::No>(_mItems.fixedLenBoolField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBoolFieldLeRevState()
{
    return this->_handleCommonReadFixedLenBoolFieldState<
        0, ByteOrder::Little, internal::BitOrder::Reversed, _SaveVal::No>(
        _mItems.fixedLenBoolField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBoolFieldBa8RevState()
{
    return this->_handleCommonReadFixedLenBoolFieldState<
        8, ByteOrder::Big, internal::BitOrder::Reversed, _SaveVal::No>(_mItems.fixedLenBoolField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBoolFieldBa16LeRevState()
{
    return this->_handleCommonReadFixedLenBoolFieldState<
        16, ByteOrder::Little, internal::BitOrder::Reversed, _SaveVal::No>(
        _mItems.fixedLenBoolField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBoolFieldBa16BeRevState()
{
    return this->_handleCommonReadFixedLenBoolFieldState<
        16, ByteOrder::Big, internal::BitOrder::Reversed, _SaveVal::No>(_mItems.fixedLenBoolField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBoolFieldBa32LeRevState()
{
    return this->_handleCommonReadFixedLenBoolFieldState<
        32, ByteOrder::Little, internal::BitOrder::Reversed, _SaveVal::No>(
        _mItems.fixedLenBoolField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBoolFieldBa32BeRevState()
{
    return this->_handleCommonReadFixedLenBoolFieldState<
        32, ByteOrder::Big, internal::BitOrder::Reversed, _SaveVal::No>(_mItems.fixedLenBoolField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBoolFieldBa64LeRevState()
{
    return this->_handleCommonReadFixedLenBoolFieldState<
        64, ByteOrder::Little, internal::BitOrder::Reversed, _SaveVal::No>(
        _mItems.fixedLenBoolField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBoolFieldBa64BeRevState()
{
    return this->_handleCommonReadFixedLenBoolFieldState<
        64, ByteOrder::Big, internal::BitOrder::Reversed, _SaveVal::No>(_mItems.fixedLenBoolField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBoolFieldBeRevSaveValState()
{
    return this->_handleCommonReadFixedLenBoolFieldState<
        0, ByteOrder::Big, internal::BitOrder::Reversed, _SaveVal::Yes>(_mItems.fixedLenBoolField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBoolFieldLeRevSaveValState()
{
    return this->_handleCommonReadFixedLenBoolFieldState<
        0, ByteOrder::Little, internal::BitOrder::Reversed, _SaveVal::Yes>(
        _mItems.fixedLenBoolField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBoolFieldBa8RevSaveValState()
{
    return this->_handleCommonReadFixedLenBoolFieldState<
        8, ByteOrder::Big, internal::BitOrder::Reversed, _SaveVal::Yes>(_mItems.fixedLenBoolField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBoolFieldBa16LeRevSaveValState()
{
    return this->_handleCommonReadFixedLenBoolFieldState<
        16, ByteOrder::Little, internal::BitOrder::Reversed, _SaveVal::Yes>(
        _mItems.fixedLenBoolField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBoolFieldBa16BeRevSaveValState()
{
    return this->_handleCommonReadFixedLenBoolFieldState<
        16, ByteOrder::Big, internal::BitOrder::Reversed, _SaveVal::Yes>(_mItems.fixedLenBoolField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBoolFieldBa32LeRevSaveValState()
{
    return this->_handleCommonReadFixedLenBoolFieldState<
        32, ByteOrder::Little, internal::BitOrder::Reversed, _SaveVal::Yes>(
        _mItems.fixedLenBoolField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBoolFieldBa32BeRevSaveValState()
{
    return this->_handleCommonReadFixedLenBoolFieldState<
        32, ByteOrder::Big, internal::BitOrder::Reversed, _SaveVal::Yes>(_mItems.fixedLenBoolField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBoolFieldBa64LeRevSaveValState()
{
    return this->_handleCommonReadFixedLenBoolFieldState<
        64, ByteOrder::Little, internal::BitOrder::Reversed, _SaveVal::Yes>(
        _mItems.fixedLenBoolField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenBoolFieldBa64BeRevSaveValState()
{
    return this->_handleCommonReadFixedLenBoolFieldState<
        64, ByteOrder::Big, internal::BitOrder::Reversed, _SaveVal::Yes>(_mItems.fixedLenBoolField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenFloatField32BeRevState()
{
    return this->_handleCommonReadFixedLenFloatFieldState<0, ByteOrder::Big,
                                                          internal::BitOrder::Reversed, float>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenFloatField32LeRevState()
{
    return this->_handleCommonReadFixedLenFloatFieldState<0, ByteOrder::Little,
                                                          internal::BitOrder::Reversed, float>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenFloatField64BeRevState()
{
    return this->_handleCommonReadFixedLenFloatFieldState<0, ByteOrder::Big,
                                                          internal::BitOrder::Reversed, double>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenFloatField64LeRevState()
{
    return this->_handleCommonReadFixedLenFloatFieldState<0, ByteOrder::Little,
                                                          internal::BitOrder::Reversed, double>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenFloatFieldBa32LeRevState()
{
    return this->_handleCommonReadFixedLenFloatFieldState<32, ByteOrder::Little,
                                                          internal::BitOrder::Reversed, float>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenFloatFieldBa32BeRevState()
{
    return this->_handleCommonReadFixedLenFloatFieldState<32, ByteOrder::Big,
                                                          internal::BitOrder::Reversed, float>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenFloatFieldBa64LeRevState()
{
    return this->_handleCommonReadFixedLenFloatFieldState<64, ByteOrder::Little,
                                                          internal::BitOrder::Reversed, double>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenFloatFieldBa64BeRevState()
{
    return this->_handleCommonReadFixedLenFloatFieldState<64, ByteOrder::Big,
                                                          internal::BitOrder::Reversed, double>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBeRevState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 0, ByteOrder::Big,
                                                         internal::BitOrder::Reversed,
                                                         _WithRole::No, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldLeRevState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 0, ByteOrder::Little,
                                                         internal::BitOrder::Reversed,
                                                         _WithRole::No, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBa8RevState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 8, ByteOrder::Big,
                                                         internal::BitOrder::Reversed,
                                                         _WithRole::No, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBa16LeRevState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 16, ByteOrder::Little,
                                                         internal::BitOrder::Reversed,
                                                         _WithRole::No, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBa16BeRevState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 16, ByteOrder::Big,
                                                         internal::BitOrder::Reversed,
                                                         _WithRole::No, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBa32LeRevState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 32, ByteOrder::Little,
                                                         internal::BitOrder::Reversed,
                                                         _WithRole::No, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBa32BeRevState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 32, ByteOrder::Big,
                                                         internal::BitOrder::Reversed,
                                                         _WithRole::No, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBa64LeRevState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 64, ByteOrder::Little,
                                                         internal::BitOrder::Reversed,
                                                         _WithRole::No, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBa64BeRevState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 64, ByteOrder::Big,
                                                         internal::BitOrder::Reversed,
                                                         _WithRole::No, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBeRevWithRoleState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 0, ByteOrder::Big,
                                                         internal::BitOrder::Reversed,
                                                         _WithRole::Yes, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldLeRevWithRoleState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 0, ByteOrder::Little,
                                                         internal::BitOrder::Reversed,
                                                         _WithRole::Yes, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBa8RevWithRoleState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 8, ByteOrder::Big,
                                                         internal::BitOrder::Reversed,
                                                         _WithRole::Yes, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction
ItemSeqIter::_handleReadFixedLenUIntFieldBa16LeRevWithRoleState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 16, ByteOrder::Little,
                                                         internal::BitOrder::Reversed,
                                                         _WithRole::Yes, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction
ItemSeqIter::_handleReadFixedLenUIntFieldBa16BeRevWithRoleState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 16, ByteOrder::Big,
                                                         internal::BitOrder::Reversed,
                                                         _WithRole::Yes, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction
ItemSeqIter::_handleReadFixedLenUIntFieldBa32LeRevWithRoleState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 32, ByteOrder::Little,
                                                         internal::BitOrder::Reversed,
                                                         _WithRole::Yes, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction
ItemSeqIter::_handleReadFixedLenUIntFieldBa32BeRevWithRoleState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 32, ByteOrder::Big,
                                                         internal::BitOrder::Reversed,
                                                         _WithRole::Yes, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction
ItemSeqIter::_handleReadFixedLenUIntFieldBa64LeRevWithRoleState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 64, ByteOrder::Little,
                                                         internal::BitOrder::Reversed,
                                                         _WithRole::Yes, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction
ItemSeqIter::_handleReadFixedLenUIntFieldBa64BeRevWithRoleState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 64, ByteOrder::Big,
                                                         internal::BitOrder::Reversed,
                                                         _WithRole::Yes, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBeRevSaveValState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 0, ByteOrder::Big,
                                                         internal::BitOrder::Reversed,
                                                         _WithRole::No, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldLeRevSaveValState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 0, ByteOrder::Little,
                                                         internal::BitOrder::Reversed,
                                                         _WithRole::No, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBa8RevSaveValState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 8, ByteOrder::Big,
                                                         internal::BitOrder::Reversed,
                                                         _WithRole::No, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBa16LeRevSaveValState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 16, ByteOrder::Little,
                                                         internal::BitOrder::Reversed,
                                                         _WithRole::No, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBa16BeRevSaveValState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 16, ByteOrder::Big,
                                                         internal::BitOrder::Reversed,
                                                         _WithRole::No, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBa32LeRevSaveValState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 32, ByteOrder::Little,
                                                         internal::BitOrder::Reversed,
                                                         _WithRole::No, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBa32BeRevSaveValState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 32, ByteOrder::Big,
                                                         internal::BitOrder::Reversed,
                                                         _WithRole::No, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBa64LeRevSaveValState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 64, ByteOrder::Little,
                                                         internal::BitOrder::Reversed,
                                                         _WithRole::No, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenUIntFieldBa64BeRevSaveValState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 64, ByteOrder::Big,
                                                         internal::BitOrder::Reversed,
                                                         _WithRole::No, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction
ItemSeqIter::_handleReadFixedLenUIntFieldBeRevWithRoleSaveValState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 0, ByteOrder::Big,
                                                         internal::BitOrder::Reversed,
                                                         _WithRole::Yes, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction
ItemSeqIter::_handleReadFixedLenUIntFieldLeRevWithRoleSaveValState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 0, ByteOrder::Little,
                                                         internal::BitOrder::Reversed,
                                                         _WithRole::Yes, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction
ItemSeqIter::_handleReadFixedLenUIntFieldBa8RevWithRoleSaveValState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 8, ByteOrder::Big,
                                                         internal::BitOrder::Reversed,
                                                         _WithRole::Yes, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction
ItemSeqIter::_handleReadFixedLenUIntFieldBa16LeRevWithRoleSaveValState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 16, ByteOrder::Little,
                                                         internal::BitOrder::Reversed,
                                                         _WithRole::Yes, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction
ItemSeqIter::_handleReadFixedLenUIntFieldBa16BeRevWithRoleSaveValState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 16, ByteOrder::Big,
                                                         internal::BitOrder::Reversed,
                                                         _WithRole::Yes, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction
ItemSeqIter::_handleReadFixedLenUIntFieldBa32LeRevWithRoleSaveValState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 32, ByteOrder::Little,
                                                         internal::BitOrder::Reversed,
                                                         _WithRole::Yes, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction
ItemSeqIter::_handleReadFixedLenUIntFieldBa32BeRevWithRoleSaveValState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 32, ByteOrder::Big,
                                                         internal::BitOrder::Reversed,
                                                         _WithRole::Yes, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction
ItemSeqIter::_handleReadFixedLenUIntFieldBa64LeRevWithRoleSaveValState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 64, ByteOrder::Little,
                                                         internal::BitOrder::Reversed,
                                                         _WithRole::Yes, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction
ItemSeqIter::_handleReadFixedLenUIntFieldBa64BeRevWithRoleSaveValState()
{
    return this->_handleCommonReadFixedLenUIntFieldState<FixedLenUIntFc, 64, ByteOrder::Big,
                                                         internal::BitOrder::Reversed,
                                                         _WithRole::Yes, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenSIntFieldBeRevState()
{
    return this->_handleCommonReadFixedLenSIntFieldState<
        FixedLenSIntFc, 0, ByteOrder::Big, internal::BitOrder::Reversed, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenSIntFieldLeRevState()
{
    return this->_handleCommonReadFixedLenSIntFieldState<
        FixedLenSIntFc, 0, ByteOrder::Little, internal::BitOrder::Reversed, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenSIntFieldBa8RevState()
{
    return this->_handleCommonReadFixedLenSIntFieldState<
        FixedLenSIntFc, 8, ByteOrder::Big, internal::BitOrder::Reversed, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenSIntFieldBa16LeRevState()
{
    return this->_handleCommonReadFixedLenSIntFieldState<
        FixedLenSIntFc, 16, ByteOrder::Little, internal::BitOrder::Reversed, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenSIntFieldBa16BeRevState()
{
    return this->_handleCommonReadFixedLenSIntFieldState<
        FixedLenSIntFc, 16, ByteOrder::Big, internal::BitOrder::Reversed, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenSIntFieldBa32LeRevState()
{
    return this->_handleCommonReadFixedLenSIntFieldState<
        FixedLenSIntFc, 32, ByteOrder::Little, internal::BitOrder::Reversed, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenSIntFieldBa32BeRevState()
{
    return this->_handleCommonReadFixedLenSIntFieldState<
        FixedLenSIntFc, 32, ByteOrder::Big, internal::BitOrder::Reversed, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenSIntFieldBa64LeRevState()
{
    return this->_handleCommonReadFixedLenSIntFieldState<
        FixedLenSIntFc, 64, ByteOrder::Little, internal::BitOrder::Reversed, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenSIntFieldBa64BeRevState()
{
    return this->_handleCommonReadFixedLenSIntFieldState<
        FixedLenSIntFc, 64, ByteOrder::Big, internal::BitOrder::Reversed, _SaveVal::No>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenSIntFieldBeRevSaveValState()
{
    return this->_handleCommonReadFixedLenSIntFieldState<
        FixedLenSIntFc, 0, ByteOrder::Big, internal::BitOrder::Reversed, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenSIntFieldLeRevSaveValState()
{
    return this->_handleCommonReadFixedLenSIntFieldState<
        FixedLenSIntFc, 0, ByteOrder::Little, internal::BitOrder::Reversed, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenSIntFieldBa8RevSaveValState()
{
    return this->_handleCommonReadFixedLenSIntFieldState<
        FixedLenSIntFc, 8, ByteOrder::Big, internal::BitOrder::Reversed, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenSIntFieldBa16LeRevSaveValState()
{
    return this->_handleCommonReadFixedLenSIntFieldState<
        FixedLenSIntFc, 16, ByteOrder::Little, internal::BitOrder::Reversed, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenSIntFieldBa16BeRevSaveValState()
{
    return this->_handleCommonReadFixedLenSIntFieldState<
        FixedLenSIntFc, 16, ByteOrder::Big, internal::BitOrder::Reversed, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenSIntFieldBa32LeRevSaveValState()
{
    return this->_handleCommonReadFixedLenSIntFieldState<
        FixedLenSIntFc, 32, ByteOrder::Little, internal::BitOrder::Reversed, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenSIntFieldBa32BeRevSaveValState()
{
    return this->_handleCommonReadFixedLenSIntFieldState<
        FixedLenSIntFc, 32, ByteOrder::Big, internal::BitOrder::Reversed, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenSIntFieldBa64LeRevSaveValState()
{
    return this->_handleCommonReadFixedLenSIntFieldState<
        FixedLenSIntFc, 64, ByteOrder::Little, internal::BitOrder::Reversed, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadFixedLenSIntFieldBa64BeRevSaveValState()
{
    return this->_handleCommonReadFixedLenSIntFieldState<
        FixedLenSIntFc, 64, ByteOrder::Big, internal::BitOrder::Reversed, _SaveVal::Yes>();
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadVarLenUIntFieldState()
{
    return this->_handleCommonReadVarLenUIntFieldState<VarLenUIntFc, _WithRole::No, _SaveVal::No>(
        _mItems.varLenUIntField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadVarLenUIntFieldWithRoleState()
{
    return this->_handleCommonReadVarLenUIntFieldState<VarLenUIntFc, _WithRole::Yes, _SaveVal::No>(
        _mItems.varLenUIntField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadVarLenUIntFieldSaveValState()
{
    return this->_handleCommonReadVarLenUIntFieldState<VarLenUIntFc, _WithRole::No, _SaveVal::Yes>(
        _mItems.varLenUIntField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadVarLenUIntFieldWithRoleSaveValState()
{
    return this->_handleCommonReadVarLenUIntFieldState<VarLenUIntFc, _WithRole::Yes, _SaveVal::Yes>(
        _mItems.varLenUIntField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadVarLenSIntFieldState()
{
    return this->_handleCommonReadVarLenSIntFieldState<VarLenSIntFc, _SaveVal::No>(
        _mItems.varLenSIntField);
}

ItemSeqIter::_StateHandlingReaction ItemSeqIter::_handleReadVarLenSIntFieldSaveValState()
{
    return this->_handleCommonReadVarLenSIntFieldState<VarLenSIntFc, _SaveVal::Yes>(
        _mItems.varLenSIntField);
}

ItemSeqIter::_StateHandlingReaction
ItemSeqIter::_handleReadFixedLenMetadataStreamUuidByteUIntFieldBa8State()
{
    /* Read byte as an unsigned integer */
    const auto val =
        this->_handleCommonReadFixedLenIntFieldState<bt2c::Signedness::Unsigned, 8, ByteOrder::Big,
                                                     internal::BitOrder::Natural>(
            _mItems.fixedLenUIntField);

    /* Update for user */
    _mItems.fixedLenUIntField._val(val);

    /* Set metadata stream UUID byte */
    auto& top = this->_stackTop();

    _mCurMetadataStreamUuid[top.elemIndex] = static_cast<std::uint8_t>(val);

    /* Next byte */
    ++top.elemIndex;

    /*
     * Next step depends on whether or not we're done reading all
     * the metadata stream UUID bytes.
     */
    if (top.elemIndex == _mItems.metadataStreamUuid._mUuid.size()) {
        /* Update for user */
        _mItems.metadataStreamUuid._mUuid = bt2c::Uuid {_mCurMetadataStreamUuid.data()};

        /* Next: set metadata stream UUID item */
        this->_state(_State::SetMetadataStreamUuidItem);
    }

    return _StateHandlingReaction::Stop;
}

} /* namespace ctf::src */
