/*
 * Copyright (c) 2022 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_PLUGINS_CTF_COMMON_SRC_ITEM_SEQ_ITEM_SEQ_ITER_HPP
#define BABELTRACE_PLUGINS_CTF_COMMON_SRC_ITEM_SEQ_ITEM_SEQ_ITER_HPP

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <type_traits>
#include <vector>

#include "common/assert.h"
#include "common/common.h"
#include "compat/bitfield.h"
#include "cpp-common/bt2c/align.hpp"
#include "cpp-common/bt2c/logging.hpp"
#include "cpp-common/bt2c/read-fixed-len-int.hpp"
#include "cpp-common/bt2c/reverse-fixed-len-int-bits.hpp"
#include "cpp-common/bt2c/std-int.hpp"
#include "cpp-common/vendor/wise-enum/wise_enum.h"

#include "../null-cp-finder.hpp"
#include "item.hpp"
#include "medium.hpp"

/*
 * Like BT_CPPLOGE_APPEND_CAUSE_AND_THROW(), but the message starts with
 * the current item sequence offset and it always throws an instance
 * of `bt2c::Error`.
 *
 * ┌──────────────────────────────────────────────────────────────┐
 * │ IMPORTANT: Do NOT use an argument ID such as `{1}` in `_fmt` │
 * │ because this macro prepends a format string to `_fmt`.       │
 * └──────────────────────────────────────────────────────────────┘
 */
#define CTF_SRC_ITEM_SEQ_ITER_CPPLOGE_APPEND_CAUSE_AND_THROW(_fmt, ...)                            \
    BT_CPPLOGE_APPEND_CAUSE_AND_THROW(bt2c::Error, ("At {} bits: " _fmt),                          \
                                      *this->_headOffsetInItemSeq(), ##__VA_ARGS__)

/*
 * Like BT_CPPLOGT(), but the message starts with the current item
 * sequence offset.
 *
 * ┌──────────────────────────────────────────────────────────────┐
 * │ IMPORTANT: Do NOT use an argument ID such as `{1}` in `_fmt` │
 * │ because this macro prepends a format string to `_fmt`.       │
 * └──────────────────────────────────────────────────────────────┘
 */
#define CTF_SRC_ITEM_SEQ_ITER_CPPLOGT(_fmt, ...)                                                   \
    BT_CPPLOGT(("At {} bits: " _fmt), *this->_headOffsetInItemSeq(), ##__VA_ARGS__)

namespace ctf::src {

class ItemSeqIter;

namespace internal {

/*
 * Return type of ReadFixedLenIntFunc::read() depending on
 * `SignednessV`.
 */
template <bt2c::Signedness SignednessV>
using ReadFixedLenIntFuncRet =
    std::conditional_t<SignednessV == bt2c::Signedness::Signed, long long, unsigned long long>;

/*
 * Whether or not the bits are reversed (unnatural).
 */
enum class BitOrder
{
    Natural,
    Reversed,
};

/*
 * Reverses `*len` bits of `val` if `BitOrderV` is `BitOrder::Natural`;
 * no op otherwise.
 */
template <bt2c::Signedness SignednessV, BitOrder BitOrderV>
auto reverseFixedLenIntBitsIfNeeded(const ReadFixedLenIntFuncRet<SignednessV> val,
                                    const bt2c::DataLen len) noexcept
{
    if constexpr (BitOrderV == BitOrder::Natural) {
        return val;
    } else {
        return bt2c::reverseFixedLenIntBits(val, len);
    }
}

/*
 * Reads a fixed-length integer having the signedness `SignednessV`, the
 * length `LenBitsV`, the byte order `ByteOrderV`, and the bit order
 * `BitOrderV` from some buffer.
 *
 * `LenBitsV` must be one of:
 *
 * 0:
 *     Uses bt_bitfield_read_be() and bt_bitfield_read_le().
 *
 * 8, 16, 32, or 64:
 *     Uses bt2c::readFixedLenIntBe() or bt2c::readFixedLenIntLe().
 *
 *     The alignment of the field must be a multiple of 8.
 *
 * Declared here; defined after the `ItemSeqIter` class definition
 * because it needs to know it.
 */
template <bt2c::Signedness SignednessV, std::size_t LenBitsV, ByteOrder ByteOrderV,
          BitOrder BitOrderV>
ReadFixedLenIntFuncRet<SignednessV> readFixedLenInt(const ItemSeqIter& iter,
                                                    const FixedLenBitArrayFc& fc);

/*
 * Returns the value (of which the signedness is `SignednessV`) of a
 * variable-length integer field from some LEB128-decoded unsigned value
 * of a given length.
 */
template <bt2c::Signedness SignednessV>
unsigned long long varLenIntFieldVal(const bt2c::DataLen len, unsigned long long v) noexcept
{
    if constexpr (SignednessV == bt2c::Signedness::Unsigned) {
        static_cast<void>(len);
        return v;
    } else {
        using namespace bt2c::literals::datalen;

        BT_ASSERT_DBG(len <= 64_bits);

        if (len == 64_bits) {
            /* `(1ULL << *len)` below is UB: no need to sign-extend */
            return v;
        }

        /* Sign-extend */
        const auto mask = 1ULL << (*len - 1);

        v = v & ((1ULL << *len) - 1);
        v = (v ^ mask) - mask;

        /* Return equivalent unsigned value */
        return v;
    }
}

} /* namespace internal */

/*
 * Item sequence iterator.
 *
 * An item sequence iterator can decode a sequence of packets using:
 *
 * • A medium, which provides binary stream data to the iterator.
 * • A trace class, which describes how to decode said data.
 *
 * The value of an item sequence iterator is an item. The item sequence
 * iterator doesn't actually create items as it advances: it has one
 * instance of each concrete item class and sets a pointer to one of
 * those as it advances (it's a single-pass input iterator).
 *
 * Seek a specific packet beginning with the seekPkt() method.
 *
 * Methods which make the iterator decode may append a cause to the
 * error of the current thread and throw `bt2c::Error`.
 *
 * The rationale for having a Python-style iterator, where a next()
 * method both advances to the next item and returns it once, instead of
 * an STL-style one (operator++() and operator*()) is that it's
 * guaranteed that the constructors don't throw `bt2c::TryAgain` or a
 * medium error because they don't perform any initial decoding.
 *
 * This API and its implementation are inspired by the yactfr
 * (<https://github.com/eepp/yactfr>) element sequence iterator API,
 * conveniently written by the same author.
 *
 * EXPECTED ITEM SEQUENCE
 * ━━━━━━━━━━━━━━━━━━━━━━
 * Here's what you can expect when you iterate an item sequence with
 * such an iterator.
 *
 * In the following descriptions, consider this language:
 *
 * `A B`:
 *     Item of type A followed by item of type B (two iterations).
 *
 * `A | B`:
 *     Item of type A _or_ item of type B (single iteration).
 *
 * `A*`:
 *     Item of type A occuring zero or more times (zero or
 *     more iterations).
 *
 * `A{N}`:
 *     Item of type A occuring N times (N iterations).
 *
 * `ScopeBeginItem<SCOPE>`:
 *     Item of type `ScopeBeginItem` with specific scope SCOPE.
 *
 * `( ... )`:
 *     Group of items of the given types or of other groups.
 *
 * `[ ... ]`:
 *     Group of optional items of the given types or of other groups.
 *
 * When a name is written in UPPERCASE, then it's a named group of items
 * having specific types. This is used to make the descriptions below
 * easier to read and to allow recursion.
 *
 * FIELD group
 * ───────────
 *     (
 *       (
 *         (FixedLenUIntFieldItem | VarLenUIntFieldItem)
 *         [DefClkValItem]
 *       ) |
 *       FixedLenBitArrayFieldItem |
 *       FixedLenBitMapFieldItem |
 *       FixedLenBoolFieldItem |
 *       FixedLenSIntFieldItem |
 *       FixedLenFloatFieldItem |
 *       VarLenSIntFieldItem |
 *       (
 *         NullTerminatedStrFieldBeginItem
 *         StrFieldSubstrItem StrFieldSubstrItem *
 *         NullTerminatedStrFieldEndItem
 *       ) |
 *       (
 *         StaticLenArrayFieldBeginItem
 *         FIELD*
 *         StaticLenArrayFieldEndItem
 *       ) |
 *       (
 *         StaticLenArrayFieldBeginItem
 *         FixedLenUIntFieldItem{16}
 *         MetadataStreamUuidItem
 *         StaticLenArrayFieldEndItem
 *       ) |
 *       (
 *         DynLenArrayFieldBeginItem
 *         FIELD*
 *         DynLenArrayFieldEndItem
 *       ) |
 *       (
 *         StaticLenStrFieldBeginItem
 *         RawDataItem*
 *         StaticLenStrFieldEndItem
 *       ) |
 *       (
 *         DynLenStrFieldBeginItem
 *         RawDataItem*
 *         DynLenStrFieldEndItem
 *       ) |
 *       (
 *         StaticLenBlobFieldBeginItem
 *         RawDataItem*
 *         [MetadataStreamUuidItem]
 *         StaticLenBlobFieldEndItem
 *       ) |
 *       (
 *         DynLenBlobFieldBeginItem
 *         RawDataItem*
 *         DynLenBlobFieldEndItem
 *       ) |
 *       (
 *         StructFieldBeginItem
 *         FIELD*
 *         StructFieldEndItem
 *       ) |
 *       (
 *         VariantFieldWithUIntSelBeginItem
 *         FIELD
 *         VariantFieldWithUIntSelEndItem
 *       ) |
 *       (
 *         VariantFieldWithSIntSelBeginItem
 *         FIELD
 *         VariantFieldWithSIntSelEndItem
 *       ) |
 *       (
 *         OptionalFieldWithBoolSelBeginItem
 *         [FIELD]
 *         OptionalFieldWithBoolSelEndItem
 *       ) |
 *       (
 *         OptionalFieldWithUIntSelBeginItem
 *         [FIELD]
 *         OptionalFieldWithUIntSelEndItem
 *       ) |
 *       (
 *         OptionalFieldWithSIntSelBeginItem
 *         [FIELD]
 *         OptionalFieldWithSIntSelEndItem
 *       )
 *     )
 *
 * Note that:
 *
 * • A `DefClkValItem` item may only follow an unsigned integer field
 *   item when it's within the `Scope::PktCtx` or
 *   `Scope::EventRecordHeader` scope.
 *
 * • A `MetadataStreamUuidItem` item may only precede a
 *   `StaticLenArrayFieldEndItem` or a `StaticLenBlobFieldEndItem` item
 *   when it's within the `Scope::PktHeader` scope.
 *
 * EVENT-RECORD group
 * ──────────────────
 *     (
 *       EventRecordBeginItem
 *       [
 *         ScopeBeginItem<Scope::EventRecordHeader>
 *         StructFieldBeginItem FIELD* StructFieldEndItem
 *         ScopeEndItem<Scope::EventRecordHeader>
 *       ]
 *       EventRecordInfoItem
 *       [
 *         ScopeBeginItem<Scope::CommonEventRecordCtx>
 *         StructFieldBeginItem FIELD* StructFieldEndItem
 *         ScopeEndItem<Scope::CommonEventRecordCtx>
 *       ]
 *       [
 *         ScopeBeginItem<Scope::SpecEventRecordCtx>
 *         StructFieldBeginItem FIELD* StructFieldEndItem
 *         ScopeEndItem<Scope::SpecEventRecordCtx>
 *       ]
 *       [
 *         ScopeBeginItem<Scope::EventRecordPayload>
 *         StructFieldBeginItem FIELD* StructFieldEndItem
 *         ScopeEndItem<Scope::EventRecordPayload>
 *       ]
 *       EventRecordEndItem
 *     )
 *
 * Note how an `EventRecordInfoItem` always exists, whether or not
 * there's an event record header field.
 *
 * PACKET group
 * ────────────
 *     (
 *       PktBeginItem PktContentBeginItem
 *       [
 *         ScopeBeginItem<Scope::PktHeader>
 *         StructFieldBeginItem
 *         (
 *           (
 *             FixedLenUIntFieldItem
 *             PktMagicNumberItem
 *           ) |
 *           StructFieldBeginItem FIELD* StructFieldEndItem
 *         )*
 *         StructFieldEndItem
 *         ScopeEndItem<Scope::PktHeader>
 *       ]
 *       DataStreamInfoItem
 *       [
 *         ScopeBeginItem<Scope::PktCtx>
 *         StructFieldBeginItem FIELD* StructFieldEndItem
 *         ScopeEndItem<Scope::PktCtx>
 *       ]
 *       PktInfoItem
 *       EVENT-RECORD*
 *       PktContentEndItem PktEndItem
 *     )
 *
 * Note how a `DataStreamInfoItem` always exists, whether or not there's
 * a packet header field, and how a `PktInfoItem` always exists, whether
 * or not there's a packet context field.
 *
 * Item sequence
 * ─────────────
 * The whole item sequence is just a sequence of zero or more packets:
 *
 *     PACKET*
 *
 * PADDING EXCLUSION
 * ━━━━━━━━━━━━━━━━━
 * The purpose of an item sequence iterator is to extract _data_ from
 * data streams. Considering this, an item sequence iterator doesn't
 * provide padding data. In other words, this API always _skips_ padding
 * bits so that the following field is aligned according to
 * its metadata.
 *
 * The guarantees of offset() are as follows, depending on the class of
 * the current item:
 *
 * `PktBeginItem`:
 *     The offset is the beginning of a packet, _excluding_ any prior
 *     padding following the previous packet content, if any.
 *
 * `PktEndItem`:
 *     The offset is the end of the packet, that is, following any
 *     padding following its content (the difference between the offset
 *     of this item and the offset at the prior `PktContentEndItem`,
 *     if any).
 *
 *     It's the same as it will be at the next `PktBeginItem`.
 *
 * `PktContentBeginItem`:
 *     The offset is the same as it was at the last `PktBeginItem`.
 *
 * `PktContentEndItem`:
 *     The offset is the same as it was at the last `ScopeEndItem`.
 *
 * `EventRecordBeginItem`:
 *     The offset is the beginning of an event record.
 *
 *     It's the same as it will be at the next `ScopeBeginItem`.
 *
 * `EventRecordEndItem`:
 *     The offset is the end of an event record.
 *
 *     It's the same as it was at the last `ScopeEndItem`.
 *
 * `ScopeBeginItem`:
 *     The offset is the same as it will be at the
 *     next `StructFieldBeginItem`.
 *
 * `ScopeEndItem`:
 *     The offset is the same as it was at the
 *     last `StructFieldEndItem`.
 *
 * `NullTerminatedStrFieldBeginItem`:
 * `StructFieldBeginItem`:
 * `NonNullTerminatedStrFieldBeginItem`:
 * `ArrayFieldBeginItem`:
 * `BlobFieldBeginItem`:
 * `VariantFieldBeginItem`:
 * `OptionalFieldBeginItem`:
 *     The offset is the beginning of the container field, that is, it's
 *     aligned according to the alignment of the container field class
 *     (with relation to the beginning of the packet).
 *
 * Other `EndItem`:
 *     Depending on the type of the previous item instance I:
 *
 *     `FixedLenBitArrayFieldItem`:
 *     `RawDataItem`:
 *     `VarLenIntFieldItem`:
 *         The offset of I plus its length.
 *
 *     `EndItem`:
 *         The offset of I.
 *
 * MEDIUM GUARANTEES
 * ━━━━━━━━━━━━━━━━━
 * When you iterate an item sequence with next(), it's _guaranteed_ that
 * the requested offsets, when calling Medium::buf(), increase
 * monotonically. However, it's possible that a message sequence
 * iterator requests consecutive buffers containing overlapping data,
 * for example:
 *
 *     ▓▓▓▓▓▓▓▓▓▓▓▓▓▓
 *                ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓
 *                              ▓▓▓▓▓▓▓
 *                                   ▓▓▓▓▓▓▓▓▓▓▓▓▓
 *                                             ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓
 *
 * The length of overlapping data is _always_ less than ten bytes.
 *
 * IMPLEMENTATION
 * ━━━━━━━━━━━━━━
 * An item sequence iterator is a state machine, its current state
 * being `_mState`.
 *
 * When you call next(), the iterator handles the current state until
 * the reaction is to stop (`_StateHandlingReaction::Stop`). Stopping
 * means that one of:
 *
 * • There's a current item (the method returns a valid item pointer).
 * • The iterator is ended (the method returns `nullptr`).
 *
 * The `_mItems` structure contains one instance of each concrete item
 * class. `_mCurItem` points to one of those instances. This is why it's
 * a single-pass input iterator: there's no dynamic item allocation
 * during the iteration process.
 *
 * _prepareToReadField() transforms a given field class into a field
 * reading state based on what's called its deep type. For fixed-length
 * and variable-length field classes, this deep type incorporates
 * important decoding information, for example:
 *
 * • Byte order.
 *
 * • Whether or not the bit order is reversed (unnatural), if it's a
 *   "standard" fixed-length bit array.
 *
 * • Length if it's a "standard" fixed-length bit array.
 *
 * • Integer signedness.
 *
 * • Whether or not the field has a role.
 *
 * • Whether or not the value of the field has to be saved as a
 *   key value.
 *
 * • Encoding of a string field.
 *
 * Having a single `switch` statement in _prepareToReadField() to select
 * the next state makes it easier/possible for the compiler to optimize
 * as most common decisions are encoded in there.
 *
 * For example, if it's known that the next field to read is a
 * little-endian, byte-aligned 32-bit unsigned integer field of which
 * the iterator needs to save the value, then its deep type is
 * `FcDeepType::FixedLenUIntBa32LeSaveVal` which means the state
 * `_State::ReadFixedLenUIntFieldBa32LeSaveVal`, so that _handleState()
 * will jump to _handleReadFixedLenUIntFieldBa32LeSaveValState()
 * directly. The latter method does exactly what's needed to perform
 * such a field reading operation efficiently (using
 * bt2c::readFixedLenIntLe()) and saves the key value without
 * superfluous branches.
 *
 * Speaking about key value saving, this is the strategy to decode
 * dynamic-length, optional, and variant fields (called dependend
 * fields) here. A field class FC of which the deep type contains
 * `SaveVal` contains key value saving indexes I (keyValSavingIndexes()
 * method). When decoding an instance of FC (a key field), the iterator
 * saves the value to `_mSavedKeyVals` at the indexes I through
 * _saveKeyVal(). When decoding a dependend field, its class contains an
 * index in `_mSavedKeyVals` to retrieve a saved key value (its length
 * or selector). The state handler retrieves the key value through
 * _savedKeyVal(). _savedKeyVal() casts the saved key value (of type
 * `unsigned long long` within `_mSavedKeyVals`) to a compatible type
 * (`bool` or another integral type).
 *
 * All the state handlers have the name _handle*State(), although there
 * are common state handling helpers which start with `_handleCommon`.
 *
 * State transitions
 * ─────────────────
 * Here's a Graphviz DOT source which shows the state transitions:
 *
 *     digraph {
 *       node [
 *         fontname = monospace
 *         fontsize = 12
 *         shape = box
 *         style = "rounded, filled"
 *         margin = "1, 0"
 *       ]
 *
 *       edge [
 *         fontname = "sans-serif"
 *         fontsize = 8
 *         fontcolor = "#16a085"
 *       ]
 *
 *       subgraph cluster_0 {
 *         label = "Read event record"
 *         color = "#cce5f6"
 *         style = "rounded, filled"
 *         fontname = "sans-serif bold"
 *         margin = 25
 *
 *         read_event_record_header_struct_field [
 *           label = "Read structure field"
 *           fontname = "sans-serif"
 *           fillcolor = "#8e44ad"
 *           fontcolor = "white"
 *           width = 3
 *         ]
 *
 *         read_common_event_record_ctx_struct_field [
 *           label = "Read structure field"
 *           fontname = "sans-serif"
 *           fillcolor = "#8e44ad"
 *           fontcolor = "white"
 *           width = 3
 *         ]
 *
 *         read_spec_event_record_ctx_struct_field [
 *           label = "Read structure field"
 *           fontname = "sans-serif"
 *           fillcolor = "#8e44ad"
 *           fontcolor = "white"
 *           width = 3
 *         ]
 *
 *         read_event_record_payload_struct_field [
 *           label = "Read structure field"
 *           fontname = "sans-serif"
 *           fillcolor = "#8e44ad"
 *           fontcolor = "white"
 *           width = 3
 *         ]
 *
 *         EndReadEventRecord [fillcolor = "#2c3e50", fontcolor = white]
 *         EndReadCommonEventRecordCtxScope [fillcolor = "#34495e", fontcolor = white]
 *         EndReadEventRecordHeaderScope [fillcolor = "#34495e", fontcolor = white]
 *         EndReadEventRecordPayloadScope [fillcolor = "#34495e", fontcolor = white]
 *         EndReadSpecEventRecordCtxScope [fillcolor = "#34495e", fontcolor = white]
 *         SetEventRecordInfoItem [fillcolor = "#2980b9", fontcolor = white]
 *         TryBeginReadEventRecord [fillcolor = "#d35400", fontcolor = white]
 *         TryBeginReadCommonEventRecordCtxScope [fillcolor = "#f39c12", fontcolor = white]
 *         TryBeginReadEventRecordHeaderScope [fillcolor = "#f39c12", fontcolor = white]
 *         TryBeginReadEventRecordPayloadScope [fillcolor = "#f39c12", fontcolor = white]
 *         TryBeginReadSpecEventRecordCtxScope [fillcolor = "#f39c12", fontcolor = white]
 *
 *         TryBeginReadEventRecord -> TryBeginReadEventRecordHeaderScope
 *         TryBeginReadEventRecordHeaderScope -> SetEventRecordInfoItem [label = "No field"]
 *         TryBeginReadEventRecordHeaderScope -> read_event_record_header_struct_field
 *         read_event_record_header_struct_field -> EndReadEventRecordHeaderScope
 *         EndReadEventRecordHeaderScope -> SetEventRecordInfoItem
 *         SetEventRecordInfoItem -> TryBeginReadCommonEventRecordCtxScope
 *         TryBeginReadCommonEventRecordCtxScope -> EndReadEventRecord [label = "No field,\nno event record class"]
 *         TryBeginReadCommonEventRecordCtxScope -> TryBeginReadSpecEventRecordCtxScope [label = "No field"]
 *         TryBeginReadCommonEventRecordCtxScope -> read_common_event_record_ctx_struct_field
 *         read_common_event_record_ctx_struct_field -> EndReadCommonEventRecordCtxScope
 *         EndReadCommonEventRecordCtxScope -> EndReadEventRecord [label = "No event record class"]
 *         EndReadCommonEventRecordCtxScope -> TryBeginReadSpecEventRecordCtxScope
 *         TryBeginReadSpecEventRecordCtxScope -> TryBeginReadEventRecordPayloadScope [label = "No field"]
 *         TryBeginReadSpecEventRecordCtxScope -> read_spec_event_record_ctx_struct_field
 *         read_spec_event_record_ctx_struct_field -> EndReadSpecEventRecordCtxScope
 *         EndReadSpecEventRecordCtxScope -> TryBeginReadEventRecordPayloadScope
 *         TryBeginReadEventRecordPayloadScope -> EndReadEventRecord [label = "No field"]
 *         TryBeginReadEventRecordPayloadScope -> read_event_record_payload_struct_field
 *         read_event_record_payload_struct_field -> EndReadEventRecordPayloadScope
 *         EndReadEventRecordPayloadScope -> EndReadEventRecord
 *         EndReadEventRecord -> TryBeginReadEventRecord
 *       }
 *
 *       read_pkt_header_struct_field [
 *         label = "Read structure field"
 *         fontname = "sans-serif"
 *         fillcolor = "#8e44ad"
 *         fontcolor = "white"
 *         width = 3
 *       ]
 *
 *       read_pkt_ctx_struct_field [
 *         label = "Read structure field"
 *         fontname = "sans-serif"
 *         fillcolor = "#8e44ad"
 *         fontcolor = "white"
 *         width = 3
 *       ]
 *
 *       Init [fillcolor = "#27ae60", fontcolor = white]
 *       Done [fillcolor = "#c0392b", fontcolor = white]
 *       TryBeginReadPkt [fillcolor = "#d35400", fontcolor = white]
 *       BeginReadPktContent [fillcolor = "#d35400", fontcolor = white]
 *       TryBeginReadPktHeaderScope [fillcolor = "#f39c12", fontcolor = white]
 *       TryBeginReadPktCtxScope [fillcolor = "#f39c12", fontcolor = white]
 *       EndReadPktHeaderScope [fillcolor = "#34495e", fontcolor = white]
 *       EndReadPktCtxScope [fillcolor = "#34495e", fontcolor = white]
 *       EndReadPktContent [fillcolor = "#2c3e50", fontcolor = white]
 *       EndReadPkt [fillcolor = "#2c3e50", fontcolor = white]
 *       SetDataStreamInfoItem [fillcolor = "#2980b9", fontcolor = white]
 *       SetPktInfoItem [fillcolor = "#2980b9", fontcolor = white]
 *       SkipPadding [fillcolor = "#ecf0f1", fontcolor = black]
 *
 *       Init -> TryBeginReadPkt
 *       TryBeginReadPkt -> Done [label = "No more data"]
 *       TryBeginReadPkt -> BeginReadPktContent
 *       BeginReadPktContent -> TryBeginReadPktHeaderScope
 *       TryBeginReadPktHeaderScope -> SetDataStreamInfoItem [label = "No field"]
 *       TryBeginReadPktHeaderScope -> read_pkt_header_struct_field
 *       read_pkt_header_struct_field -> EndReadPktHeaderScope
 *       EndReadPktHeaderScope -> SetDataStreamInfoItem
 *       SetDataStreamInfoItem -> TryBeginReadPktCtxScope
 *       SetDataStreamInfoItem -> SetPktInfoItem [label = "No data\nstream class"]
 *       TryBeginReadPktCtxScope -> SetPktInfoItem [label = "No field"]
 *       TryBeginReadPktCtxScope -> read_pkt_ctx_struct_field
 *       read_pkt_ctx_struct_field -> EndReadPktCtxScope
 *       EndReadPktCtxScope -> SetPktInfoItem
 *       SetPktInfoItem -> TryBeginReadEventRecord
 *       EndReadPktContent -> EndReadPkt [label="No padding"]
 *       EndReadPktContent -> SkipPadding
 *       SkipPadding -> SkipPadding
 *       SkipPadding -> EndReadPkt
 *       EndReadPkt -> TryBeginReadPkt
 *       TryBeginReadEventRecord -> EndReadPktContent [label = "No more data"]
 *     }
 *
 * Buffer and offsets
 * ──────────────────
 * The "decoding head" of a an item sequence iterator is a position
 * within some buffer to decode the next field.
 *
 * An item sequence iterator works with three offsets:
 *
 * `_mBufOffsetInCurPkt`:
 *     The offset of the beginning of the buffer (`_mBuf`) within the
 *     current packet.
 *
 * `_mCurPktOffsetInItemSeq`:
 *     The offset of the current packet within the whole item sequence.
 *
 * `_mHeadOffsetInCurPkt`:
 *     The offset of the decoding head within the current packet.
 *
 *     This is the only member which the iterator updates systematically
 *     when it reads data. It's relative to the beginning of the packet
 *     because CTF (all versions) says that the alignment requirement of
 *     any field is relative to the beginning of its containing packet.
 *
 * Note that `_mCurItemOffsetInItemSeq`, the offset of the current item
 * within the whole item sequence, is not strictly needed for the
 * decoding operation. This is what the user-visible offset() method
 * returns. We can't compute it on demand, within the offset() method,
 * because once an iterator finishes reading a variable-length integer
 * field VF, its decoding head (`_mHeadOffsetInCurPkt`) is _after_ VF,
 * but offset() returns the offset at the _beginning_ of an item.
 *
 * The following diagram shows the meaning of the significant decoding
 * members in relation to a packet and a current buffer:
 *
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                                          Decoding head                    ║
 * ║                                          ▼                                ║
 * ║ Packet: ▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒▒ ║
 * ║         ┊                                                               ┊ ║
 * ║ Buffer: ┊                         ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓          ┊ ║
 * ║         ┊                         ┊      ┊                   ┊          ┊ ║
 * ║         ┣┅┅ _mBufOffsetInCurPkt ┅┅┫      ┊                   ┊          ┊ ║
 * ║         ┊                         ┊      ┊                   ┊          ┊ ║
 * ║         ┣┅┅┅┅┅ _mHeadOffsetInCurPkt ┅┅┅┅┅┫                   ┊          ┊ ║
 * ║         ┊                         ┊                          ┊          ┊ ║
 * ║         ┊                         ┣┅┅┅┅┅┅ _mBuf.size() ┅┅┅┅┅┅┫          ┊ ║
 * ║         ┊                                                               ┊ ║
 * ║         ┣┅┅┅┅┅┅┅┅┅┅┅┅┅┅┅┅┅┅ _mCurPktExpectedLens.total ┅┅┅┅┅┅┅┅┅┅┅┅┅┅┅┅┅┫ ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 */
class ItemSeqIter final
{
    template <bt2c::Signedness SignednessV, std::size_t, ByteOrder, internal::BitOrder>
    friend internal::ReadFixedLenIntFuncRet<SignednessV>
    internal::readFixedLenInt(const ItemSeqIter&, const FixedLenBitArrayFc&);

public:
    /*
     * Creates an item sequence iterator using the medium `medium` and
     * the trace class `traceCls`.
     *
     * It's guaranteed that this constructor doesn't throw
     * `bt2c::TryAgain` or a medium error.
     */
    explicit ItemSeqIter(Medium::UP medium, const TraceCls& traceCls,
                         const bt2c::Logger& parentLogger);

    /*
     * Creates an item sequence iterator using the medium `medium` and
     * the trace class `traceCls`, initially seeking the medium to
     * `pktOffset`.
     *
     * It's guaranteed that this constructor doesn't throw
     * `bt2c::TryAgain` or a medium error.
     */
    explicit ItemSeqIter(Medium::UP medium, const TraceCls& traceCls, bt2c::DataLen pktOffset,
                         const bt2c::Logger& parentLogger);

    /*
     * Make the intention explicit here, but the medium unique pointer
     * isn't copyable anyway.
     */
    ItemSeqIter(const ItemSeqIter&) = delete;
    ItemSeqIter& operator=(const ItemSeqIter&) = delete;

    /*
     * Makes the underlying medium seek to `pktOffset` and resets the
     * state to decode a packet.
     *
     * It's guaranteed that this method doesn't throw `bt2c::TryAgain`
     * or a medium error.
     */
    void seekPkt(bt2c::DataLen pktOffset);

    /*
     * Advances the iterator to the next item, returning one of:
     *
     * A valid item pointer:
     *     The next item.
     *
     * `nullptr`:
     *     End of iterator (no more items).
     *
     * This iterator must not be ended (last call to next() didn't
     * return `nullptr`).
     *
     * May throw whatever Medium::buf() may throw as well as
     * `bt2c::Error`.
     */
    const Item *next()
    {
        BT_ASSERT_DBG(_mState != _State::Done);

        try {
            while (this->_handleState() == _StateHandlingReaction::Continue) {
                continue;
            }
        } catch (const bt2c::OverflowError& exc) {
            /*
             * Some data length or offset, computed from the data
             * stream, doesn't fit.
             *
             * Don't use CTF_SRC_ITEM_SEQ_ITER_CPPLOGE_APPEND_CAUSE_AND_THROW()
             * because the head offset itself may be what doesn't fit.
             */
            BT_CPPLOGE_APPEND_CAUSE_AND_THROW(
                bt2c::Error,
                "Invalid data length or offset: {}: last-item-offset-in-item-seq-bits={}",
                exc.what(), *_mCurItemOffsetInItemSeq);
        }

        return _mCurItem;
    }

    /*
     * Current offset of this iterator relative to the beginning of the
     * item sequence (_not_ to the beginning of some current packet).
     */
    bt2c::DataLen offset() const noexcept
    {
        return _mCurItemOffsetInItemSeq;
    }

private:
    /* clang-format off */

    /*
     * State.
     *
     * The enumerator names below use the following common parts:
     *
     * `Be`:
     *     Big-endian.
     *
     * `Le`:
     *     Little-endian.
     *
     * `Rev`:
     *     Reversed (unnatural) bit order.
     *
     * `Ba`:
     *     Byte-aligned (alignment of at least 8 bits).
     *
     * `8`, `16`, `32`, `64`:
     *     Fixed 8-bit, 16-bit, 32-bit, or 64-bit length.
     *
     * `SaveVal`:
     *     Save the value of the boolean/integer key field.
     *
     * `WithRole`:
     *     Unsigned integer field with at least one role.
     *
     * `MetadataStreamUuid`:
     *     Static-length array/BLOB field with the "metadata stream
     *     UUID" role.
     *
     * `Utf*`:
     *     String field with a specific UTF string encoding.
     */
    WISE_ENUM_CLASS_MEMBER(_State,
        BeginReadDynLenArrayField,
        BeginReadDynLenBlobField,
        BeginReadDynLenStrField,
        BeginReadNullTerminatedStrFieldUtf16,
        BeginReadNullTerminatedStrFieldUtf32,
        BeginReadNullTerminatedStrFieldUtf8,
        BeginReadOptionalFieldWithBoolSel,
        BeginReadOptionalFieldWithSIntSel,
        BeginReadOptionalFieldWithUIntSel,
        BeginReadPktContent,
        BeginReadStaticLenArrayField,
        BeginReadStaticLenArrayFieldMetadataStreamUuid,
        BeginReadStaticLenBlobField,
        BeginReadStaticLenBlobFieldMetadataStreamUuid,
        BeginReadStaticLenStrField,
        BeginReadStructField,
        BeginReadVariantFieldWithSIntSel,
        BeginReadVariantFieldWithUIntSel,
        Done,
        EndReadCommonEventRecordCtxScope,
        EndReadDynLenArrayField,
        EndReadDynLenBlobField,
        EndReadDynLenStrField,
        EndReadEventRecord,
        EndReadEventRecordHeaderScope,
        EndReadEventRecordPayloadScope,
        EndReadNullTerminatedStrField,
        EndReadOptionalFieldWithBoolSel,
        EndReadOptionalFieldWithSIntSel,
        EndReadOptionalFieldWithUIntSel,
        EndReadPkt,
        EndReadPktContent,
        EndReadPktCtxScope,
        EndReadPktHeaderScope,
        EndReadSpecEventRecordCtxScope,
        EndReadStaticLenArrayField,
        EndReadStaticLenBlobField,
        EndReadStaticLenStrField,
        EndReadStructField,
        EndReadVariantFieldWithSIntSel,
        EndReadVariantFieldWithUIntSel,
        Init,
        ReadFixedLenBitArrayFieldBa16Be,
        ReadFixedLenBitArrayFieldBa16BeRev,
        ReadFixedLenBitArrayFieldBa16Le,
        ReadFixedLenBitArrayFieldBa16LeRev,
        ReadFixedLenBitArrayFieldBa32Be,
        ReadFixedLenBitArrayFieldBa32BeRev,
        ReadFixedLenBitArrayFieldBa32Le,
        ReadFixedLenBitArrayFieldBa32LeRev,
        ReadFixedLenBitArrayFieldBa64Be,
        ReadFixedLenBitArrayFieldBa64BeRev,
        ReadFixedLenBitArrayFieldBa64Le,
        ReadFixedLenBitArrayFieldBa64LeRev,
        ReadFixedLenBitArrayFieldBa8,
        ReadFixedLenBitArrayFieldBa8Rev,
        ReadFixedLenBitArrayFieldBe,
        ReadFixedLenBitArrayFieldBeRev,
        ReadFixedLenBitArrayFieldLe,
        ReadFixedLenBitArrayFieldLeRev,
        ReadFixedLenBitMapFieldBa16Be,
        ReadFixedLenBitMapFieldBa16BeRev,
        ReadFixedLenBitMapFieldBa16Le,
        ReadFixedLenBitMapFieldBa16LeRev,
        ReadFixedLenBitMapFieldBa32Be,
        ReadFixedLenBitMapFieldBa32BeRev,
        ReadFixedLenBitMapFieldBa32Le,
        ReadFixedLenBitMapFieldBa32LeRev,
        ReadFixedLenBitMapFieldBa64Be,
        ReadFixedLenBitMapFieldBa64BeRev,
        ReadFixedLenBitMapFieldBa64Le,
        ReadFixedLenBitMapFieldBa64LeRev,
        ReadFixedLenBitMapFieldBa8,
        ReadFixedLenBitMapFieldBa8Rev,
        ReadFixedLenBitMapFieldBe,
        ReadFixedLenBitMapFieldBeRev,
        ReadFixedLenBitMapFieldLe,
        ReadFixedLenBitMapFieldLeRev,
        ReadFixedLenBoolFieldBa16Be,
        ReadFixedLenBoolFieldBa16BeRev,
        ReadFixedLenBoolFieldBa16BeRevSaveVal,
        ReadFixedLenBoolFieldBa16BeSaveVal,
        ReadFixedLenBoolFieldBa16Le,
        ReadFixedLenBoolFieldBa16LeRev,
        ReadFixedLenBoolFieldBa16LeRevSaveVal,
        ReadFixedLenBoolFieldBa16LeSaveVal,
        ReadFixedLenBoolFieldBa32Be,
        ReadFixedLenBoolFieldBa32BeRev,
        ReadFixedLenBoolFieldBa32BeRevSaveVal,
        ReadFixedLenBoolFieldBa32BeSaveVal,
        ReadFixedLenBoolFieldBa32Le,
        ReadFixedLenBoolFieldBa32LeRev,
        ReadFixedLenBoolFieldBa32LeRevSaveVal,
        ReadFixedLenBoolFieldBa32LeSaveVal,
        ReadFixedLenBoolFieldBa64Be,
        ReadFixedLenBoolFieldBa64BeRev,
        ReadFixedLenBoolFieldBa64BeRevSaveVal,
        ReadFixedLenBoolFieldBa64BeSaveVal,
        ReadFixedLenBoolFieldBa64Le,
        ReadFixedLenBoolFieldBa64LeRev,
        ReadFixedLenBoolFieldBa64LeRevSaveVal,
        ReadFixedLenBoolFieldBa64LeSaveVal,
        ReadFixedLenBoolFieldBa8,
        ReadFixedLenBoolFieldBa8Rev,
        ReadFixedLenBoolFieldBa8RevSaveVal,
        ReadFixedLenBoolFieldBa8SaveVal,
        ReadFixedLenBoolFieldBe,
        ReadFixedLenBoolFieldBeRev,
        ReadFixedLenBoolFieldBeRevSaveVal,
        ReadFixedLenBoolFieldBeSaveVal,
        ReadFixedLenBoolFieldLe,
        ReadFixedLenBoolFieldLeRev,
        ReadFixedLenBoolFieldLeRevSaveVal,
        ReadFixedLenBoolFieldLeSaveVal,
        ReadFixedLenFloatField32Be,
        ReadFixedLenFloatField32BeRev,
        ReadFixedLenFloatField32Le,
        ReadFixedLenFloatField32LeRev,
        ReadFixedLenFloatField64Be,
        ReadFixedLenFloatField64BeRev,
        ReadFixedLenFloatField64Le,
        ReadFixedLenFloatField64LeRev,
        ReadFixedLenFloatFieldBa32Be,
        ReadFixedLenFloatFieldBa32BeRev,
        ReadFixedLenFloatFieldBa32Le,
        ReadFixedLenFloatFieldBa32LeRev,
        ReadFixedLenFloatFieldBa64Be,
        ReadFixedLenFloatFieldBa64BeRev,
        ReadFixedLenFloatFieldBa64Le,
        ReadFixedLenFloatFieldBa64LeRev,
        ReadFixedLenMetadataStreamUuidByteUIntFieldBa8,
        ReadFixedLenSIntFieldBa16Be,
        ReadFixedLenSIntFieldBa16BeRev,
        ReadFixedLenSIntFieldBa16BeRevSaveVal,
        ReadFixedLenSIntFieldBa16BeSaveVal,
        ReadFixedLenSIntFieldBa16Le,
        ReadFixedLenSIntFieldBa16LeRev,
        ReadFixedLenSIntFieldBa16LeRevSaveVal,
        ReadFixedLenSIntFieldBa16LeSaveVal,
        ReadFixedLenSIntFieldBa32Be,
        ReadFixedLenSIntFieldBa32BeRev,
        ReadFixedLenSIntFieldBa32BeRevSaveVal,
        ReadFixedLenSIntFieldBa32BeSaveVal,
        ReadFixedLenSIntFieldBa32Le,
        ReadFixedLenSIntFieldBa32LeRev,
        ReadFixedLenSIntFieldBa32LeRevSaveVal,
        ReadFixedLenSIntFieldBa32LeSaveVal,
        ReadFixedLenSIntFieldBa64Be,
        ReadFixedLenSIntFieldBa64BeRev,
        ReadFixedLenSIntFieldBa64BeRevSaveVal,
        ReadFixedLenSIntFieldBa64BeSaveVal,
        ReadFixedLenSIntFieldBa64Le,
        ReadFixedLenSIntFieldBa64LeRev,
        ReadFixedLenSIntFieldBa64LeRevSaveVal,
        ReadFixedLenSIntFieldBa64LeSaveVal,
        ReadFixedLenSIntFieldBa8,
        ReadFixedLenSIntFieldBa8Rev,
        ReadFixedLenSIntFieldBa8RevSaveVal,
        ReadFixedLenSIntFieldBa8SaveVal,
        ReadFixedLenSIntFieldBe,
        ReadFixedLenSIntFieldBeRev,
        ReadFixedLenSIntFieldBeRevSaveVal,
        ReadFixedLenSIntFieldBeSaveVal,
        ReadFixedLenSIntFieldLe,
        ReadFixedLenSIntFieldLeRev,
        ReadFixedLenSIntFieldLeRevSaveVal,
        ReadFixedLenSIntFieldLeSaveVal,
        ReadFixedLenUIntFieldBa16Be,
        ReadFixedLenUIntFieldBa16BeRev,
        ReadFixedLenUIntFieldBa16BeRevSaveVal,
        ReadFixedLenUIntFieldBa16BeRevWithRole,
        ReadFixedLenUIntFieldBa16BeRevWithRoleSaveVal,
        ReadFixedLenUIntFieldBa16BeSaveVal,
        ReadFixedLenUIntFieldBa16BeWithRole,
        ReadFixedLenUIntFieldBa16BeWithRoleSaveVal,
        ReadFixedLenUIntFieldBa16Le,
        ReadFixedLenUIntFieldBa16LeRev,
        ReadFixedLenUIntFieldBa16LeRevSaveVal,
        ReadFixedLenUIntFieldBa16LeRevWithRole,
        ReadFixedLenUIntFieldBa16LeRevWithRoleSaveVal,
        ReadFixedLenUIntFieldBa16LeSaveVal,
        ReadFixedLenUIntFieldBa16LeWithRole,
        ReadFixedLenUIntFieldBa16LeWithRoleSaveVal,
        ReadFixedLenUIntFieldBa32Be,
        ReadFixedLenUIntFieldBa32BeRev,
        ReadFixedLenUIntFieldBa32BeRevSaveVal,
        ReadFixedLenUIntFieldBa32BeRevWithRole,
        ReadFixedLenUIntFieldBa32BeRevWithRoleSaveVal,
        ReadFixedLenUIntFieldBa32BeSaveVal,
        ReadFixedLenUIntFieldBa32BeWithRole,
        ReadFixedLenUIntFieldBa32BeWithRoleSaveVal,
        ReadFixedLenUIntFieldBa32Le,
        ReadFixedLenUIntFieldBa32LeRev,
        ReadFixedLenUIntFieldBa32LeRevSaveVal,
        ReadFixedLenUIntFieldBa32LeRevWithRole,
        ReadFixedLenUIntFieldBa32LeRevWithRoleSaveVal,
        ReadFixedLenUIntFieldBa32LeSaveVal,
        ReadFixedLenUIntFieldBa32LeWithRole,
        ReadFixedLenUIntFieldBa32LeWithRoleSaveVal,
        ReadFixedLenUIntFieldBa64Be,
        ReadFixedLenUIntFieldBa64BeRev,
        ReadFixedLenUIntFieldBa64BeRevSaveVal,
        ReadFixedLenUIntFieldBa64BeRevWithRole,
        ReadFixedLenUIntFieldBa64BeRevWithRoleSaveVal,
        ReadFixedLenUIntFieldBa64BeSaveVal,
        ReadFixedLenUIntFieldBa64BeWithRole,
        ReadFixedLenUIntFieldBa64BeWithRoleSaveVal,
        ReadFixedLenUIntFieldBa64Le,
        ReadFixedLenUIntFieldBa64LeRev,
        ReadFixedLenUIntFieldBa64LeRevSaveVal,
        ReadFixedLenUIntFieldBa64LeRevWithRole,
        ReadFixedLenUIntFieldBa64LeRevWithRoleSaveVal,
        ReadFixedLenUIntFieldBa64LeSaveVal,
        ReadFixedLenUIntFieldBa64LeWithRole,
        ReadFixedLenUIntFieldBa64LeWithRoleSaveVal,
        ReadFixedLenUIntFieldBa8,
        ReadFixedLenUIntFieldBa8Rev,
        ReadFixedLenUIntFieldBa8RevSaveVal,
        ReadFixedLenUIntFieldBa8RevWithRole,
        ReadFixedLenUIntFieldBa8RevWithRoleSaveVal,
        ReadFixedLenUIntFieldBa8SaveVal,
        ReadFixedLenUIntFieldBa8WithRole,
        ReadFixedLenUIntFieldBa8WithRoleSaveVal,
        ReadFixedLenUIntFieldBe,
        ReadFixedLenUIntFieldBeRev,
        ReadFixedLenUIntFieldBeRevSaveVal,
        ReadFixedLenUIntFieldBeRevWithRole,
        ReadFixedLenUIntFieldBeRevWithRoleSaveVal,
        ReadFixedLenUIntFieldBeSaveVal,
        ReadFixedLenUIntFieldBeWithRole,
        ReadFixedLenUIntFieldBeWithRoleSaveVal,
        ReadFixedLenUIntFieldLe,
        ReadFixedLenUIntFieldLeRev,
        ReadFixedLenUIntFieldLeRevSaveVal,
        ReadFixedLenUIntFieldLeRevWithRole,
        ReadFixedLenUIntFieldLeRevWithRoleSaveVal,
        ReadFixedLenUIntFieldLeSaveVal,
        ReadFixedLenUIntFieldLeWithRole,
        ReadFixedLenUIntFieldLeWithRoleSaveVal,
        ReadMetadataStreamUuidBlobFieldSection,
        ReadRawData,
        ReadSubstrUntilNullCodepointUtf16,
        ReadSubstrUntilNullCodepointUtf32,
        ReadSubstrUntilNullCodepointUtf8,
        ReadVarLenSIntField,
        ReadVarLenSIntFieldSaveVal,
        ReadVarLenUIntField,
        ReadVarLenUIntFieldSaveVal,
        ReadVarLenUIntFieldWithRole,
        ReadVarLenUIntFieldWithRoleSaveVal,
        SetDataStreamInfoItem,
        SetDefClkValItem,
        SetEventRecordInfoItem,
        SetMetadataStreamUuidItem,
        SetPktInfoItem,
        SetPktMagicNumberItem,
        SkipContentPadding,
        SkipPadding,
        TryBeginReadCommonEventRecordCtxScope,
        TryBeginReadEventRecord,
        TryBeginReadEventRecordHeaderScope,
        TryBeginReadEventRecordPayloadScope,
        TryBeginReadPkt,
        TryBeginReadPktCtxScope,
        TryBeginReadPktHeaderScope,
        TryBeginReadSpecEventRecordCtxScope
    )

    /* clang-format on */

    /*
     * Reaction of a state handling method.
     */
    enum class _StateHandlingReaction
    {
        /* Continue executing the state machine */
        Continue,

        /*
         * Stop the state machine, returning control to the user.
         *
         * This reaction means one of:
         *
         * • There's a current item (next() will return a valid item
         *   pointer).
         *
         * • The iterator is ended (next() will return `nullptr`).
         */
        Stop,
    };

    /*
     * Single frame of the stack.
     *
     * A stack frame holds:
     *
     * • The previous state (to be restored).
     * • A base field class (optional).
     * • A current length, if applicable.
     * • A number of remaining elements, if applicable.
     *
     * The iterator needs such a stack because fields may contain other
     * fields. The meaning of the current length and number of remaining
     * elements depend on the base field class.
     */
    struct _StackFrame final
    {
        explicit _StackFrame(_State restoringState) noexcept;
        explicit _StackFrame(_State restoringState, const Fc& fc) noexcept;

        /* State to restore when removing this frame */
        _State restoringState;

        /* Class of containing field */
        const Fc *fc = nullptr;

        /*
         * Index of the current "element" to decode.
         *
         * The meaning of this field depends on the type of `*fc` above:
         *
         * `Fc::Type::Struct`:
         *     Member index.
         *
         * `Fc::Type::StaticLenArray`:
         * `Fc::Type::DynLenArray`:
         *     Element index.
         *
         * `Fc::Type::OptionalWithBoolSel`:
         * `Fc::Type::OptionalWithUIntSel`:
         * `Fc::Type::OptionalWithSIntSel`:
         * `Fc::Type::VariantWithUIntSel`:
         * `Fc::Type::VariantWithSIntSel`:
         *     1 means we're done.
         *
         * `FcType::NullTerminatedStr`:
         * `FcType::StaticLenBlob`:
         * `FcType::DynLenBlob`:
         *     Byte index.
         *
         * Other:
         *     Meaningless.
         */
        std::size_t elemIndex = 0;

        /*
         * Length of containing field.
         *
         * The meaning of this field depends on the type of `*fc` above:
         *
         * `Fc::Type::Struct`:
         *     Member count.
         *
         * `Fc::Type::StaticLenArray`:
         * `Fc::Type::DynLenArray`:
         *     Element count.
         *
         * `FcType::NullTerminatedStr`:
         * `FcType::StaticLenBlob`:
         * `FcType::DynLenBlob`:
         *     Byte count.
         *
         * `Fc::Type::OptionalWithBoolSel`:
         * `Fc::Type::OptionalWithUIntSel`:
         * `Fc::Type::OptionalWithSIntSel`:
         * `Fc::Type::VariantWithUIntSel`:
         * `Fc::Type::VariantWithSIntSel`:
         *     Always 1.
         *
         * Other:
         *     Meaningless.
         */
        std::size_t len = 0;
    };

    /*
     * Updates the current default clock value (`_mDefClkVal`) from the
     * value `val` having the length `len`.
     */
    void _updateDefClkVal(const unsigned long long val, bt2c::DataLen len) noexcept;

    /*
     * Resets some members of this iterator to prepare to decode a new
     * packet.
     */
    void _resetForNewPkt();

    /*
     * Requests a new buffer from the medium, setting the corresponding
     * members accordingly on success.
     */
    void _newBuf(bt2c::DataLen offsetInItemSeq, bt2c::DataLen minSize);

    /*
     * Length of remaining content in the current packet.
     *
     * If `_mCurPktExpectedLens.content` is `this->_infDataLen()`,
     * then this method never returns zero (in practice).
     */
    bt2c::DataLen _remainingPktContentLen() const noexcept
    {
        if (G_UNLIKELY(_mHeadOffsetInCurPkt >= _mCurPktExpectedLens.content)) {
            return bt2c::DataLen::fromBits(0);
        }

        /* Can't underflow: checked above */
        return _mCurPktExpectedLens.content.subDbg(_mHeadOffsetInCurPkt);
    }

    /*
     * Offset of the decoding head within the whole item sequence.
     */
    bt2c::DataLen _headOffsetInItemSeq() const noexcept
    {
        /*
         * Can't overflow: the head is always within the current buffer,
         * of which _newBuf() checks that the end offset fits.
         */
        return _mCurPktOffsetInItemSeq.addDbg(_mHeadOffsetInCurPkt);
    }

    /*
     * Pushes a frame onto the stack, without a field class, so that
     * _restoreState() restores `restoringState`.
     */
    void _stackPush(const _State restoringState)
    {
        CTF_SRC_ITEM_SEQ_ITER_CPPLOGT("Pushing onto stack: restoring-state={}, new-stack-len={}",
                                      wise_enum::to_string(restoringState), _mStack.size() + 1);
        _mStack.emplace_back(restoringState);
    }

    /*
     * Pushes a frame onto the stack, without a field class, so that
     * _restoreState() restores the current state.
     */
    void _stackPush()
    {
        this->_stackPush(_mState);
    }

    /*
     * Pushes a frame onto the stack, `fc` being the class of some
     * containing field, so that _restoreState() restores
     * `restoringState`.
     */
    void _stackPush(const _State restoringState, const Fc& fc)
    {
        CTF_SRC_ITEM_SEQ_ITER_CPPLOGT(
            "Pushing onto stack: restoring-state={}, fc-deep-type={}, new-stack-len={}",
            wise_enum::to_string(restoringState), wise_enum::to_string(fc.deepType()),
            _mStack.size() + 1);
        _mStack.emplace_back(restoringState, fc);
    }

    /*
     * Pushes a frame onto the stack, `fc` being the class of some
     * containing field, so that _restoreState() restores the _current_
     * state.
     */
    void _stackPush(const Fc& fc)
    {
        this->_stackPush(_mState, fc);
    }

    /*
     * Pops a frame from the stack without changing the current state
     * (use _restoreState() before calling this method to do so).
     */
    void _stackPop()
    {
        BT_ASSERT_DBG(!_mStack.empty());
        CTF_SRC_ITEM_SEQ_ITER_CPPLOGT("Popping stack: new-stack-len={}", _mStack.size() - 1);
        _mStack.pop_back();
    }

    /*
     * Top frame of the stack.
     */
    _StackFrame& _stackTop() noexcept
    {
        BT_ASSERT_DBG(!_mStack.empty());
        return _mStack.back();
    }

    /*
     * Restores the current state from the top frame of the stack.
     */
    void _restoreState() noexcept
    {
        this->_state(this->_stackTop().restoringState);
    }

    /*
     * Saves the key value `val` to the saved key value vector at the
     * indexes `indexes`.
     */
    template <typename KeyValT>
    void _saveKeyVal(const KeyValSavingIndexes& indexes, const KeyValT val) noexcept
    {
        CTF_SRC_ITEM_SEQ_ITER_CPPLOGT("Saving key value: val={}", val);

        for (const auto index : indexes) {
            BT_ASSERT_DBG(index < _mSavedKeyVals.size());
            _mSavedKeyVals[index] = static_cast<unsigned long long>(val);
        }
    }

    /*
     * Returns the saved key value of type `KeyValT` from the saved key
     * value vector at index `index`.
     */
    template <typename KeyValT>
    KeyValT _savedKeyVal(const std::size_t index) const noexcept
    {
        BT_ASSERT_DBG(index < _mSavedKeyVals.size());
        return static_cast<KeyValT>(_mSavedKeyVals[index]);
    }

    /*
     * Returns the saved unsigned integer key value from the saved key
     * value vector at index `fc.savedKeyValIndex()`.
     */
    template <typename FcT>
    unsigned long long _savedUIntKeyVal(const FcT& fc) const noexcept
    {
        BT_ASSERT_DBG(fc.savedKeyValIndex());
        return this->_savedKeyVal<unsigned long long>(*fc.savedKeyValIndex());
    }

    /*
     * Updates the user-visible members: current item and offset
     * relative to the item sequence beginning.
     */
    void _updateForUser(const Item& item, const bt2c::DataLen offset) noexcept
    {
        _mCurItem = &item;
        _mCurItemOffsetInItemSeq = offset;
    }

    /*
     * Like the other _updateForUser(), but using _headOffsetInItemSeq()
     * for the offset.
     */
    void _updateForUser(const Item& item) noexcept
    {
        this->_updateForUser(item, this->_headOffsetInItemSeq());
    }

    /*
     * Aligns the decoding head to `align` bits.
     *
     * This method may throw whatever _tryHaveData() may throw.
     */
    void _alignHead(const unsigned long long align)
    {
        /*
         * Compute new decoding head offset and how many bits we need to
         * skip to align.
         *
         * Can't overflow: the decoding head offset is at most
         * `_maxEndOffsetInItemSeq` (see _newBuf() and
         * _handleSetPktInfoItemState()) and the metadata stream parsers
         * guarantee that `align` is at most `maxAlign`.
         */
        BT_ASSERT_DBG(align <= maxAlign);
        BT_ASSERT_DBG(*_mHeadOffsetInCurPkt <= _maxEndOffsetInItemSeq);

        const auto newHeadOffset =
            bt2c::DataLen::fromBits(bt2c::align(*_mHeadOffsetInCurPkt, align));
        const auto lenToSkip = newHeadOffset.subDbg(_mHeadOffsetInCurPkt);

        /* Already aligned? */
        if (lenToSkip == bt2c::DataLen::fromBits(0)) {
            /* Yes */
            return;
        }

        /* Do align */
        CTF_SRC_ITEM_SEQ_ITER_CPPLOGT(
            "Aligning decoding head: "
            "head-offset-in-cur-packet-bits={}, new-head-offset-in-cur-packet-bits={}, len-to-skip-bits={}",
            *_mHeadOffsetInCurPkt, *newHeadOffset, *lenToSkip);

        /*
         * Validate that we're not skipping more than the packet content
         * that's left.
         */
        if (lenToSkip > this->_remainingPktContentLen()) {
            CTF_SRC_ITEM_SEQ_ITER_CPPLOGE_APPEND_CAUSE_AND_THROW(
                "{} bits of packet content required at this point, "
                "but only {} bits of packet content remain.",
                *lenToSkip, *this->_remainingPktContentLen());
        }

        /*
         * Set the state so as to skip content padding, but also try to
         * skip all of it immediately.
         */
        _mRemainingLenToSkip = lenToSkip;
        _mPostSkipPaddingState = _mState;
        this->_state(_State::SkipContentPadding);
        this->_skipPadding<true>();
    }

    /*
     * Aligns the decoding head to `fc.align()` bits.
     *
     * This method may throw whatever _tryHaveData() may throw.
     */
    void _alignHead(const Fc& fc)
    {
        this->_alignHead(fc.align());
    }

    /*
     * Skips `_mRemainingLenToSkip` bits of padding data.
     *
     * `IsContentDataV` indicates whether or not the padding data to
     * skip is packet content data (as opposed to padding data _after_
     * the packet content).
     *
     * This method may throw whatever _tryHaveData() may throw.
     */
    template <bool IsContentDataV>
    void _skipPadding()
    {
        using namespace bt2c::literals::datalen;

        while (_mRemainingLenToSkip != 0_bytes) {
            /* Require at least one bit of data */
            if constexpr (IsContentDataV) {
                this->_requireContentData(1_bits);
            } else {
                this->_requireData(1_bits);
            }

            /*
             * How many bits to skip is the minimum between what's left
             * in the buffer and what remains to skip.
             */
            const auto lenToSkip = std::min(_mRemainingLenToSkip, this->_remainingBufLen());

            /* Skip, marking the padding bits as consumed */
            CTF_SRC_ITEM_SEQ_ITER_CPPLOGT("Skipping padding bits: "
                                          "len-bits={}",
                                          *lenToSkip);

            _mRemainingLenToSkip = _mRemainingLenToSkip.subDbg(lenToSkip);
            this->_consumeAvailData(lenToSkip);
        }

        /* Done! */
        this->_state(_mPostSkipPaddingState);
    }

    /*
     * Tries to have `len` bits (maximum: 64 bits) of data, returning
     * false if not possible.
     *
     * This method may still throw `bt2c::TryAgain`, but it won't
     * throw `NoData`.
     */
    [[gnu::always_inline]] bool _tryHaveData(const bt2c::DataLen len)
    {
        BT_ASSERT_DBG(len <= bt2c::DataLen::fromBits(64));

        if (len <= this->_remainingBufLen()) {
            /* We already have enough */
            return true;
        }

        return this->_tryHaveDataSlow(len);
    }

    /*
     * Slow path of _tryHaveData(): requests a new buffer from
     * the medium.
     *
     * Not inline to keep the fast path of _tryHaveData(), which
     * reading any field requires, small.
     */
    [[gnu::noinline]] bool _tryHaveDataSlow(const bt2c::DataLen len)
    {
        /*
         * In the initializations below:
         *
         * • `_mHeadOffsetInCurPkt.bytes()` is a flooring operation.
         *
         * • Adding seven bits to `len` and the extra bit count of
         *   `_mHeadOffsetInCurPkt`, and then using DataLen::fromBytes()
         *   is a way to perform a ceiling operation.
         *
         *   For example, if `*_mHeadOffsetInCurPkt` is 1963 and `*len`
         *   is 75, then `*reqSize` is 80 (10 bytes), that is,
         *   mathematically (result in bytes):
         *
         *       floor((75 + 7 + (1963 mod 8)) / 8)
         */
        const auto reqOffsetInElemSeq = bt2c::DataLen::fromBytes(_mCurPktOffsetInItemSeq.bytes() +
                                                                 _mHeadOffsetInCurPkt.bytes());
        const auto reqSize = bt2c::DataLen::fromBytes(
            bt2c::DataLen::fromBits(*len + 7 + _mHeadOffsetInCurPkt.extraBitCount()).bytes());

        try {
            this->_newBuf(reqOffsetInElemSeq, reqSize);
        } catch (const NoData&) {
            return false;
        }

        return true;
    }

    /*
     * Requires `len` bits of data, throwing if not possible.
     */
    void _requireData(const bt2c::DataLen len)
    {
        if (!this->_tryHaveData(len)) {
            CTF_SRC_ITEM_SEQ_ITER_CPPLOGE_APPEND_CAUSE_AND_THROW(
                "{} bits of data required at this point.", *len);
        }
    }

    /*
     * Requires `len` bits of content data, throwing if not possible.
     */
    void _requireContentData(const bt2c::DataLen len)
    {
        if (len > this->_remainingPktContentLen()) {
            /* Going past the packet content */
            CTF_SRC_ITEM_SEQ_ITER_CPPLOGE_APPEND_CAUSE_AND_THROW(
                "{} bits of packet content required at this point, "
                "but only {} bits of packet content remain.",
                *len, *this->_remainingPktContentLen());
        }

        this->_requireData(len);
    }

    /*
     * Buffer at the decoding head position.
     *
     * This returns a pointer to the "byte we're in", so that the return
     * value is always the same for a given value of
     * `_mHeadOffsetInCurPkt.bytes()`, that is, whatever the value of
     * `_mHeadOffsetInCurPkt.extraBitCount()`.
     */
    const std::uint8_t *_bufAtHead() const noexcept
    {
        /* The head is always within the current buffer */
        return _mBuf.addr() + _mHeadOffsetInCurPkt.subDbg(_mBufOffsetInCurPkt).bytes();
    }

    /*
     * Length of remaining data in the current buffer.
     */
    bt2c::DataLen _remainingBufLen() const noexcept
    {
        /* The head is always within the current buffer */
        return _mBufOffsetInCurPkt.addDbg(_mBuf.size()).subDbg(_mHeadOffsetInCurPkt);
    }

    /*
     * Marks `len` bits as consumed.
     */
    void _consumeAvailData(const bt2c::DataLen len) noexcept
    {
        BT_ASSERT_DBG(len <= this->_remainingBufLen());
        CTF_SRC_ITEM_SEQ_ITER_CPPLOGT("Marking data as consumed: "
                                      "len-bits={}",
                                      *len);

        /* Can't overflow: `len` fits the current buffer (checked above) */
        _mHeadOffsetInCurPkt = _mHeadOffsetInCurPkt.addDbg(len);
    }

    /*
     * Sets the field class pointer member of the item `item` to `fc`.
     */
    template <typename ItemT>
    static void _setFieldItemFc(ItemT& item, const Fc& fc) noexcept
    {
        item._mCls = &fc;
    }

    /*
     * Calls _setFieldItemFc() and then _updateForUser().
     *
     * Almost all "read field" state handlers eventually call this.
     */
    template <typename ItemT>
    void _setFieldItemFcAndUpdateForUser(ItemT& item, const Fc& fc) noexcept
    {
        this->_setFieldItemFc(item, fc);
        this->_updateForUser(item);
    }

    /*
     * Sets the state to `tryBeginReadState`.
     *
     * Afterwards, if `fc` is `nullptr`, this method returns
     * immediately.
     *
     * Otherwise, this method prepares to read the scope `scope` of
     * which the structure field class is `fc`, setting the state to
     * restore afterwards to `endReadState`.
     */
    void _prepareToTryReadScope(const _State tryBeginReadState, const _State endReadState,
                                const Scope scope, const StructFc * const fc)
    {
        CTF_SRC_ITEM_SEQ_ITER_CPPLOGT("Preparing to read scope: "
                                      "scope={}, try-begin-read-state={}, end-read-state={}",
                                      wise_enum::to_string(scope),
                                      wise_enum::to_string(tryBeginReadState),
                                      wise_enum::to_string(endReadState));

        /* Next: try beginning to read scope */
        this->_state(tryBeginReadState);

        /*
         * Set scope and scope field class, even if it's `nullptr`,
         * because the handler of `tryBeginReadState` checks
         * `_mCurScope.fc`.
         */
        _mCurScope.scope = scope;
        _mCurScope.fc = fc;

        /*
         * If field class doesn't exist, then return immediately, not
         * pushing anything onto the stack.
         */
        if (!fc) {
            CTF_SRC_ITEM_SEQ_ITER_CPPLOGT("Field class doesn't exist: scope={}",
                                          wise_enum::to_string(scope));
            return;
        }

        /*
         * At this point we know we need to read a scope structure
         * field. Add a stack frame so that the restoring state is
         * `endReadState`.
         */
        this->_stackPush(endReadState);

        /*
         * Setting this to one will make _prepareToReadNextField() call
         * _restoreState(), effectively restoring the state
         * `endReadState` without ever needing `this->_stackTop().fc`.
         *
         * This makes it possible for _handleEndReadStructFieldState()
         * to simply call _prepareToReadNextField() unconditionally.
         */
        this->_stackTop().len = 1;
    }

    /*
     * Sets the state to `state` and updates the stack to read an
     * instance of the container field class `fc`, restoring the
     * state `restoringState` afterwards.
     */
    void _prepareToReadContainerField(const _State state, const _State restoringState, const Fc& fc)
    {
        CTF_SRC_ITEM_SEQ_ITER_CPPLOGT(
            "Preparing to read container field: state={}, restoring-state={}, fc-deep-type={}",
            wise_enum::to_string(state), wise_enum::to_string(restoringState),
            wise_enum::to_string(fc.deepType()));
        this->_stackPush(restoringState, fc);
        this->_state(state);
    }

    /*
     * Sets the state and updates the stack to read an instance of the
     * structure field class `fc`.
     */
    void _prepareToReadStructField(const StructFc& fc)
    {
        this->_prepareToReadContainerField(_State::BeginReadStructField, _State::EndReadStructField,
                                           fc);
    }

    /*
     * Sets the state to `state` and updates `_mCurScalarFc` to read an
     * instance of the scalar field class `fc`.
     */
    void _prepareToReadScalarField(const _State state, const Fc& fc) noexcept
    {
        CTF_SRC_ITEM_SEQ_ITER_CPPLOGT("Preparing to read scalar field: state={}, fc-deep-type={}",
                                      wise_enum::to_string(state),
                                      wise_enum::to_string(fc.deepType()));
        _mCurScalarFc = &fc;
        this->_state(state);
    }

    /*
     * Sets the state, possibly updates the current scalar field class
     * (`_mCurScalarFc`), and possibly updates the stack to read an
     * instance of `fc`.
     */
    void _prepareToReadField(const Fc& fc)
    {
        switch (fc.deepType()) {
        case FcDeepType::FixedLenBitArrayBe:
            this->_prepareToReadScalarField(_State::ReadFixedLenBitArrayFieldBe, fc);
            break;
        case FcDeepType::FixedLenBitArrayLe:
            this->_prepareToReadScalarField(_State::ReadFixedLenBitArrayFieldLe, fc);
            break;
        case FcDeepType::FixedLenBitArrayBa8:
            this->_prepareToReadScalarField(_State::ReadFixedLenBitArrayFieldBa8, fc);
            break;
        case FcDeepType::FixedLenBitArrayBa16Le:
            this->_prepareToReadScalarField(_State::ReadFixedLenBitArrayFieldBa16Le, fc);
            break;
        case FcDeepType::FixedLenBitArrayBa16Be:
            this->_prepareToReadScalarField(_State::ReadFixedLenBitArrayFieldBa16Be, fc);
            break;
        case FcDeepType::FixedLenBitArrayBa32Le:
            this->_prepareToReadScalarField(_State::ReadFixedLenBitArrayFieldBa32Le, fc);
            break;
        case FcDeepType::FixedLenBitArrayBa32Be:
            this->_prepareToReadScalarField(_State::ReadFixedLenBitArrayFieldBa32Be, fc);
            break;
        case FcDeepType::FixedLenBitArrayBa64Le:
            this->_prepareToReadScalarField(_State::ReadFixedLenBitArrayFieldBa64Le, fc);
            break;
        case FcDeepType::FixedLenBitArrayBa64Be:
            this->_prepareToReadScalarField(_State::ReadFixedLenBitArrayFieldBa64Be, fc);
            break;
        case FcDeepType::FixedLenBitMapBe:
            this->_prepareToReadScalarField(_State::ReadFixedLenBitMapFieldBe, fc);
            break;
        case FcDeepType::FixedLenBitMapLe:
            this->_prepareToReadScalarField(_State::ReadFixedLenBitMapFieldLe, fc);
            break;
        case FcDeepType::FixedLenBitMapBa8:
            this->_prepareToReadScalarField(_State::ReadFixedLenBitMapFieldBa8, fc);
            break;
        case FcDeepType::FixedLenBitMapBa16Le:
            this->_prepareToReadScalarField(_State::ReadFixedLenBitMapFieldBa16Le, fc);
            break;
        case FcDeepType::FixedLenBitMapBa16Be:
            this->_prepareToReadScalarField(_State::ReadFixedLenBitMapFieldBa16Be, fc);
            break;
        case FcDeepType::FixedLenBitMapBa32Le:
            this->_prepareToReadScalarField(_State::ReadFixedLenBitMapFieldBa32Le, fc);
            break;
        case FcDeepType::FixedLenBitMapBa32Be:
            this->_prepareToReadScalarField(_State::ReadFixedLenBitMapFieldBa32Be, fc);
            break;
        case FcDeepType::FixedLenBitMapBa64Le:
            this->_prepareToReadScalarField(_State::ReadFixedLenBitMapFieldBa64Le, fc);
            break;
        case FcDeepType::FixedLenBitMapBa64Be:
            this->_prepareToReadScalarField(_State::ReadFixedLenBitMapFieldBa64Be, fc);
            break;
        case FcDeepType::FixedLenBoolBe:
            this->_prepareToReadScalarField(_State::ReadFixedLenBoolFieldBe, fc);
            break;
        case FcDeepType::FixedLenBoolLe:
            this->_prepareToReadScalarField(_State::ReadFixedLenBoolFieldLe, fc);
            break;
        case FcDeepType::FixedLenBoolBa8:
            this->_prepareToReadScalarField(_State::ReadFixedLenBoolFieldBa8, fc);
            break;
        case FcDeepType::FixedLenBoolBa16Le:
            this->_prepareToReadScalarField(_State::ReadFixedLenBoolFieldBa16Le, fc);
            break;
        case FcDeepType::FixedLenBoolBa16Be:
            this->_prepareToReadScalarField(_State::ReadFixedLenBoolFieldBa16Be, fc);
            break;
        case FcDeepType::FixedLenBoolBa32Le:
            this->_prepareToReadScalarField(_State::ReadFixedLenBoolFieldBa32Le, fc);
            break;
        case FcDeepType::FixedLenBoolBa32Be:
            this->_prepareToReadScalarField(_State::ReadFixedLenBoolFieldBa32Be, fc);
            break;
        case FcDeepType::FixedLenBoolBa64Le:
            this->_prepareToReadScalarField(_State::ReadFixedLenBoolFieldBa64Le, fc);
            break;
        case FcDeepType::FixedLenBoolBa64Be:
            this->_prepareToReadScalarField(_State::ReadFixedLenBoolFieldBa64Be, fc);
            break;
        case FcDeepType::FixedLenBoolBeSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenBoolFieldBeSaveVal, fc);
            break;
        case FcDeepType::FixedLenBoolLeSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenBoolFieldLeSaveVal, fc);
            break;
        case FcDeepType::FixedLenBoolBa8SaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenBoolFieldBa8SaveVal, fc);
            break;
        case FcDeepType::FixedLenBoolBa16LeSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenBoolFieldBa16LeSaveVal, fc);
            break;
        case FcDeepType::FixedLenBoolBa16BeSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenBoolFieldBa16BeSaveVal, fc);
            break;
        case FcDeepType::FixedLenBoolBa32LeSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenBoolFieldBa32LeSaveVal, fc);
            break;
        case FcDeepType::FixedLenBoolBa32BeSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenBoolFieldBa32BeSaveVal, fc);
            break;
        case FcDeepType::FixedLenBoolBa64LeSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenBoolFieldBa64LeSaveVal, fc);
            break;
        case FcDeepType::FixedLenBoolBa64BeSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenBoolFieldBa64BeSaveVal, fc);
            break;
        case FcDeepType::FixedLenFloat32Be:
            this->_prepareToReadScalarField(_State::ReadFixedLenFloatField32Be, fc);
            break;
        case FcDeepType::FixedLenFloat32Le:
            this->_prepareToReadScalarField(_State::ReadFixedLenFloatField32Le, fc);
            break;
        case FcDeepType::FixedLenFloat64Be:
            this->_prepareToReadScalarField(_State::ReadFixedLenFloatField64Be, fc);
            break;
        case FcDeepType::FixedLenFloat64Le:
            this->_prepareToReadScalarField(_State::ReadFixedLenFloatField64Le, fc);
            break;
        case FcDeepType::FixedLenFloatBa32Le:
            this->_prepareToReadScalarField(_State::ReadFixedLenFloatFieldBa32Le, fc);
            break;
        case FcDeepType::FixedLenFloatBa32Be:
            this->_prepareToReadScalarField(_State::ReadFixedLenFloatFieldBa32Be, fc);
            break;
        case FcDeepType::FixedLenFloatBa64Le:
            this->_prepareToReadScalarField(_State::ReadFixedLenFloatFieldBa64Le, fc);
            break;
        case FcDeepType::FixedLenFloatBa64Be:
            this->_prepareToReadScalarField(_State::ReadFixedLenFloatFieldBa64Be, fc);
            break;
        case FcDeepType::FixedLenUIntBe:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBe, fc);
            break;
        case FcDeepType::FixedLenUIntLe:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldLe, fc);
            break;
        case FcDeepType::FixedLenUIntBa8:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa8, fc);
            break;
        case FcDeepType::FixedLenUIntBa16Le:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa16Le, fc);
            break;
        case FcDeepType::FixedLenUIntBa16Be:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa16Be, fc);
            break;
        case FcDeepType::FixedLenUIntBa32Le:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa32Le, fc);
            break;
        case FcDeepType::FixedLenUIntBa32Be:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa32Be, fc);
            break;
        case FcDeepType::FixedLenUIntBa64Le:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa64Le, fc);
            break;
        case FcDeepType::FixedLenUIntBa64Be:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa64Be, fc);
            break;
        case FcDeepType::FixedLenUIntBeWithRole:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBeWithRole, fc);
            break;
        case FcDeepType::FixedLenUIntLeWithRole:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldLeWithRole, fc);
            break;
        case FcDeepType::FixedLenUIntBa8WithRole:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa8WithRole, fc);
            break;
        case FcDeepType::FixedLenUIntBa16LeWithRole:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa16LeWithRole, fc);
            break;
        case FcDeepType::FixedLenUIntBa16BeWithRole:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa16BeWithRole, fc);
            break;
        case FcDeepType::FixedLenUIntBa32LeWithRole:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa32LeWithRole, fc);
            break;
        case FcDeepType::FixedLenUIntBa32BeWithRole:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa32BeWithRole, fc);
            break;
        case FcDeepType::FixedLenUIntBa64LeWithRole:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa64LeWithRole, fc);
            break;
        case FcDeepType::FixedLenUIntBa64BeWithRole:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa64BeWithRole, fc);
            break;
        case FcDeepType::FixedLenUIntBeSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBeSaveVal, fc);
            break;
        case FcDeepType::FixedLenUIntLeSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldLeSaveVal, fc);
            break;
        case FcDeepType::FixedLenUIntBa8SaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa8SaveVal, fc);
            break;
        case FcDeepType::FixedLenUIntBa16LeSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa16LeSaveVal, fc);
            break;
        case FcDeepType::FixedLenUIntBa16BeSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa16BeSaveVal, fc);
            break;
        case FcDeepType::FixedLenUIntBa32LeSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa32LeSaveVal, fc);
            break;
        case FcDeepType::FixedLenUIntBa32BeSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa32BeSaveVal, fc);
            break;
        case FcDeepType::FixedLenUIntBa64LeSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa64LeSaveVal, fc);
            break;
        case FcDeepType::FixedLenUIntBa64BeSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa64BeSaveVal, fc);
            break;
        case FcDeepType::FixedLenUIntBeWithRoleSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBeWithRoleSaveVal, fc);
            break;
        case FcDeepType::FixedLenUIntLeWithRoleSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldLeWithRoleSaveVal, fc);
            break;
        case FcDeepType::FixedLenUIntBa8WithRoleSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa8WithRoleSaveVal, fc);
            break;
        case FcDeepType::FixedLenUIntBa16LeWithRoleSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa16LeWithRoleSaveVal, fc);
            break;
        case FcDeepType::FixedLenUIntBa16BeWithRoleSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa16BeWithRoleSaveVal, fc);
            break;
        case FcDeepType::FixedLenUIntBa32LeWithRoleSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa32LeWithRoleSaveVal, fc);
            break;
        case FcDeepType::FixedLenUIntBa32BeWithRoleSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa32BeWithRoleSaveVal, fc);
            break;
        case FcDeepType::FixedLenUIntBa64LeWithRoleSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa64LeWithRoleSaveVal, fc);
            break;
        case FcDeepType::FixedLenUIntBa64BeWithRoleSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa64BeWithRoleSaveVal, fc);
            break;
        case FcDeepType::FixedLenSIntBe:
            this->_prepareToReadScalarField(_State::ReadFixedLenSIntFieldBe, fc);
            break;
        case FcDeepType::FixedLenSIntLe:
            this->_prepareToReadScalarField(_State::ReadFixedLenSIntFieldLe, fc);
            break;
        case FcDeepType::FixedLenSIntBa8:
            this->_prepareToReadScalarField(_State::ReadFixedLenSIntFieldBa8, fc);
            break;
        case FcDeepType::FixedLenSIntBa16Le:
            this->_prepareToReadScalarField(_State::ReadFixedLenSIntFieldBa16Le, fc);
            break;
        case FcDeepType::FixedLenSIntBa16Be:
            this->_prepareToReadScalarField(_State::ReadFixedLenSIntFieldBa16Be, fc);
            break;
        case FcDeepType::FixedLenSIntBa32Le:
            this->_prepareToReadScalarField(_State::ReadFixedLenSIntFieldBa32Le, fc);
            break;
        case FcDeepType::FixedLenSIntBa32Be:
            this->_prepareToReadScalarField(_State::ReadFixedLenSIntFieldBa32Be, fc);
            break;
        case FcDeepType::FixedLenSIntBa64Le:
            this->_prepareToReadScalarField(_State::ReadFixedLenSIntFieldBa64Le, fc);
            break;
        case FcDeepType::FixedLenSIntBa64Be:
            this->_prepareToReadScalarField(_State::ReadFixedLenSIntFieldBa64Be, fc);
            break;
        case FcDeepType::FixedLenSIntBeSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenSIntFieldBeSaveVal, fc);
            break;
        case FcDeepType::FixedLenSIntLeSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenSIntFieldLeSaveVal, fc);
            break;
        case FcDeepType::FixedLenSIntBa8SaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenSIntFieldBa8SaveVal, fc);
            break;
        case FcDeepType::FixedLenSIntBa16LeSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenSIntFieldBa16LeSaveVal, fc);
            break;
        case FcDeepType::FixedLenSIntBa16BeSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenSIntFieldBa16BeSaveVal, fc);
            break;
        case FcDeepType::FixedLenSIntBa32LeSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenSIntFieldBa32LeSaveVal, fc);
            break;
        case FcDeepType::FixedLenSIntBa32BeSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenSIntFieldBa32BeSaveVal, fc);
            break;
        case FcDeepType::FixedLenSIntBa64LeSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenSIntFieldBa64LeSaveVal, fc);
            break;
        case FcDeepType::FixedLenSIntBa64BeSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenSIntFieldBa64BeSaveVal, fc);
            break;
        case FcDeepType::FixedLenBitArrayBeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenBitArrayFieldBeRev, fc);
            break;
        case FcDeepType::FixedLenBitArrayLeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenBitArrayFieldLeRev, fc);
            break;
        case FcDeepType::FixedLenBitArrayBa8Rev:
            this->_prepareToReadScalarField(_State::ReadFixedLenBitArrayFieldBa8Rev, fc);
            break;
        case FcDeepType::FixedLenBitArrayBa16LeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenBitArrayFieldBa16LeRev, fc);
            break;
        case FcDeepType::FixedLenBitArrayBa16BeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenBitArrayFieldBa16BeRev, fc);
            break;
        case FcDeepType::FixedLenBitArrayBa32LeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenBitArrayFieldBa32LeRev, fc);
            break;
        case FcDeepType::FixedLenBitArrayBa32BeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenBitArrayFieldBa32BeRev, fc);
            break;
        case FcDeepType::FixedLenBitArrayBa64LeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenBitArrayFieldBa64LeRev, fc);
            break;
        case FcDeepType::FixedLenBitArrayBa64BeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenBitArrayFieldBa64BeRev, fc);
            break;
        case FcDeepType::FixedLenBitMapBeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenBitMapFieldBeRev, fc);
            break;
        case FcDeepType::FixedLenBitMapLeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenBitMapFieldLeRev, fc);
            break;
        case FcDeepType::FixedLenBitMapBa8Rev:
            this->_prepareToReadScalarField(_State::ReadFixedLenBitMapFieldBa8Rev, fc);
            break;
        case FcDeepType::FixedLenBitMapBa16LeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenBitMapFieldBa16LeRev, fc);
            break;
        case FcDeepType::FixedLenBitMapBa16BeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenBitMapFieldBa16BeRev, fc);
            break;
        case FcDeepType::FixedLenBitMapBa32LeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenBitMapFieldBa32LeRev, fc);
            break;
        case FcDeepType::FixedLenBitMapBa32BeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenBitMapFieldBa32BeRev, fc);
            break;
        case FcDeepType::FixedLenBitMapBa64LeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenBitMapFieldBa64LeRev, fc);
            break;
        case FcDeepType::FixedLenBitMapBa64BeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenBitMapFieldBa64BeRev, fc);
            break;
        case FcDeepType::FixedLenBoolBeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenBoolFieldBeRev, fc);
            break;
        case FcDeepType::FixedLenBoolLeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenBoolFieldLeRev, fc);
            break;
        case FcDeepType::FixedLenBoolBa8Rev:
            this->_prepareToReadScalarField(_State::ReadFixedLenBoolFieldBa8Rev, fc);
            break;
        case FcDeepType::FixedLenBoolBa16LeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenBoolFieldBa16LeRev, fc);
            break;
        case FcDeepType::FixedLenBoolBa16BeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenBoolFieldBa16BeRev, fc);
            break;
        case FcDeepType::FixedLenBoolBa32LeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenBoolFieldBa32LeRev, fc);
            break;
        case FcDeepType::FixedLenBoolBa32BeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenBoolFieldBa32BeRev, fc);
            break;
        case FcDeepType::FixedLenBoolBa64LeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenBoolFieldBa64LeRev, fc);
            break;
        case FcDeepType::FixedLenBoolBa64BeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenBoolFieldBa64BeRev, fc);
            break;
        case FcDeepType::FixedLenBoolBeRevSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenBoolFieldBeRevSaveVal, fc);
            break;
        case FcDeepType::FixedLenBoolLeRevSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenBoolFieldLeRevSaveVal, fc);
            break;
        case FcDeepType::FixedLenBoolBa8RevSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenBoolFieldBa8RevSaveVal, fc);
            break;
        case FcDeepType::FixedLenBoolBa16LeRevSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenBoolFieldBa16LeRevSaveVal, fc);
            break;
        case FcDeepType::FixedLenBoolBa16BeRevSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenBoolFieldBa16BeRevSaveVal, fc);
            break;
        case FcDeepType::FixedLenBoolBa32LeRevSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenBoolFieldBa32LeRevSaveVal, fc);
            break;
        case FcDeepType::FixedLenBoolBa32BeRevSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenBoolFieldBa32BeRevSaveVal, fc);
            break;
        case FcDeepType::FixedLenBoolBa64LeRevSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenBoolFieldBa64LeRevSaveVal, fc);
            break;
        case FcDeepType::FixedLenBoolBa64BeRevSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenBoolFieldBa64BeRevSaveVal, fc);
            break;
        case FcDeepType::FixedLenFloat32BeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenFloatField32BeRev, fc);
            break;
        case FcDeepType::FixedLenFloat32LeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenFloatField32LeRev, fc);
            break;
        case FcDeepType::FixedLenFloat64BeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenFloatField64BeRev, fc);
            break;
        case FcDeepType::FixedLenFloat64LeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenFloatField64LeRev, fc);
            break;
        case FcDeepType::FixedLenFloatBa32LeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenFloatFieldBa32LeRev, fc);
            break;
        case FcDeepType::FixedLenFloatBa32BeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenFloatFieldBa32BeRev, fc);
            break;
        case FcDeepType::FixedLenFloatBa64LeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenFloatFieldBa64LeRev, fc);
            break;
        case FcDeepType::FixedLenFloatBa64BeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenFloatFieldBa64BeRev, fc);
            break;
        case FcDeepType::FixedLenUIntBeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBeRev, fc);
            break;
        case FcDeepType::FixedLenUIntLeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldLeRev, fc);
            break;
        case FcDeepType::FixedLenUIntBa8Rev:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa8Rev, fc);
            break;
        case FcDeepType::FixedLenUIntBa16LeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa16LeRev, fc);
            break;
        case FcDeepType::FixedLenUIntBa16BeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa16BeRev, fc);
            break;
        case FcDeepType::FixedLenUIntBa32LeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa32LeRev, fc);
            break;
        case FcDeepType::FixedLenUIntBa32BeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa32BeRev, fc);
            break;
        case FcDeepType::FixedLenUIntBa64LeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa64LeRev, fc);
            break;
        case FcDeepType::FixedLenUIntBa64BeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa64BeRev, fc);
            break;
        case FcDeepType::FixedLenUIntBeRevWithRole:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBeRevWithRole, fc);
            break;
        case FcDeepType::FixedLenUIntLeRevWithRole:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldLeRevWithRole, fc);
            break;
        case FcDeepType::FixedLenUIntBa8RevWithRole:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa8RevWithRole, fc);
            break;
        case FcDeepType::FixedLenUIntBa16LeRevWithRole:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa16LeRevWithRole, fc);
            break;
        case FcDeepType::FixedLenUIntBa16BeRevWithRole:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa16BeRevWithRole, fc);
            break;
        case FcDeepType::FixedLenUIntBa32LeRevWithRole:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa32LeRevWithRole, fc);
            break;
        case FcDeepType::FixedLenUIntBa32BeRevWithRole:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa32BeRevWithRole, fc);
            break;
        case FcDeepType::FixedLenUIntBa64LeRevWithRole:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa64LeRevWithRole, fc);
            break;
        case FcDeepType::FixedLenUIntBa64BeRevWithRole:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa64BeRevWithRole, fc);
            break;
        case FcDeepType::FixedLenUIntBeRevSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBeRevSaveVal, fc);
            break;
        case FcDeepType::FixedLenUIntLeRevSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldLeRevSaveVal, fc);
            break;
        case FcDeepType::FixedLenUIntBa8RevSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa8RevSaveVal, fc);
            break;
        case FcDeepType::FixedLenUIntBa16LeRevSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa16LeRevSaveVal, fc);
            break;
        case FcDeepType::FixedLenUIntBa16BeRevSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa16BeRevSaveVal, fc);
            break;
        case FcDeepType::FixedLenUIntBa32LeRevSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa32LeRevSaveVal, fc);
            break;
        case FcDeepType::FixedLenUIntBa32BeRevSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa32BeRevSaveVal, fc);
            break;
        case FcDeepType::FixedLenUIntBa64LeRevSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa64LeRevSaveVal, fc);
            break;
        case FcDeepType::FixedLenUIntBa64BeRevSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa64BeRevSaveVal, fc);
            break;
        case FcDeepType::FixedLenUIntBeRevWithRoleSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBeRevWithRoleSaveVal, fc);
            break;
        case FcDeepType::FixedLenUIntLeRevWithRoleSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldLeRevWithRoleSaveVal, fc);
            break;
        case FcDeepType::FixedLenUIntBa8RevWithRoleSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa8RevWithRoleSaveVal, fc);
            break;
        case FcDeepType::FixedLenUIntBa16LeRevWithRoleSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa16LeRevWithRoleSaveVal,
                                            fc);
            break;
        case FcDeepType::FixedLenUIntBa16BeRevWithRoleSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa16BeRevWithRoleSaveVal,
                                            fc);
            break;
        case FcDeepType::FixedLenUIntBa32LeRevWithRoleSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa32LeRevWithRoleSaveVal,
                                            fc);
            break;
        case FcDeepType::FixedLenUIntBa32BeRevWithRoleSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa32BeRevWithRoleSaveVal,
                                            fc);
            break;
        case FcDeepType::FixedLenUIntBa64LeRevWithRoleSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa64LeRevWithRoleSaveVal,
                                            fc);
            break;
        case FcDeepType::FixedLenUIntBa64BeRevWithRoleSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenUIntFieldBa64BeRevWithRoleSaveVal,
                                            fc);
            break;
        case FcDeepType::FixedLenSIntBeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenSIntFieldBeRev, fc);
            break;
        case FcDeepType::FixedLenSIntLeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenSIntFieldLeRev, fc);
            break;
        case FcDeepType::FixedLenSIntBa8Rev:
            this->_prepareToReadScalarField(_State::ReadFixedLenSIntFieldBa8Rev, fc);
            break;
        case FcDeepType::FixedLenSIntBa16LeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenSIntFieldBa16LeRev, fc);
            break;
        case FcDeepType::FixedLenSIntBa16BeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenSIntFieldBa16BeRev, fc);
            break;
        case FcDeepType::FixedLenSIntBa32LeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenSIntFieldBa32LeRev, fc);
            break;
        case FcDeepType::FixedLenSIntBa32BeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenSIntFieldBa32BeRev, fc);
            break;
        case FcDeepType::FixedLenSIntBa64LeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenSIntFieldBa64LeRev, fc);
            break;
        case FcDeepType::FixedLenSIntBa64BeRev:
            this->_prepareToReadScalarField(_State::ReadFixedLenSIntFieldBa64BeRev, fc);
            break;
        case FcDeepType::FixedLenSIntBeRevSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenSIntFieldBeRevSaveVal, fc);
            break;
        case FcDeepType::FixedLenSIntLeRevSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenSIntFieldLeRevSaveVal, fc);
            break;
        case FcDeepType::FixedLenSIntBa8RevSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenSIntFieldBa8RevSaveVal, fc);
            break;
        case FcDeepType::FixedLenSIntBa16LeRevSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenSIntFieldBa16LeRevSaveVal, fc);
            break;
        case FcDeepType::FixedLenSIntBa16BeRevSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenSIntFieldBa16BeRevSaveVal, fc);
            break;
        case FcDeepType::FixedLenSIntBa32LeRevSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenSIntFieldBa32LeRevSaveVal, fc);
            break;
        case FcDeepType::FixedLenSIntBa32BeRevSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenSIntFieldBa32BeRevSaveVal, fc);
            break;
        case FcDeepType::FixedLenSIntBa64LeRevSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenSIntFieldBa64LeRevSaveVal, fc);
            break;
        case FcDeepType::FixedLenSIntBa64BeRevSaveVal:
            this->_prepareToReadScalarField(_State::ReadFixedLenSIntFieldBa64BeRevSaveVal, fc);
            break;
        case FcDeepType::VarLenUInt:
            this->_prepareToReadScalarField(_State::ReadVarLenUIntField, fc);
            break;
        case FcDeepType::VarLenUIntWithRole:
            this->_prepareToReadScalarField(_State::ReadVarLenUIntFieldWithRole, fc);
            break;
        case FcDeepType::VarLenUIntSaveVal:
            this->_prepareToReadScalarField(_State::ReadVarLenUIntFieldSaveVal, fc);
            break;
        case FcDeepType::VarLenUIntWithRoleSaveVal:
            this->_prepareToReadScalarField(_State::ReadVarLenUIntFieldWithRoleSaveVal, fc);
            break;
        case FcDeepType::VarLenSInt:
            this->_prepareToReadScalarField(_State::ReadVarLenSIntField, fc);
            break;
        case FcDeepType::VarLenSIntSaveVal:
            this->_prepareToReadScalarField(_State::ReadVarLenSIntFieldSaveVal, fc);
            break;
        case FcDeepType::NullTerminatedStrUtf8:
            this->_prepareToReadContainerField(_State::BeginReadNullTerminatedStrFieldUtf8,
                                               _State::EndReadNullTerminatedStrField, fc);
            break;
        case FcDeepType::NullTerminatedStrUtf16:
            this->_prepareToReadContainerField(_State::BeginReadNullTerminatedStrFieldUtf16,
                                               _State::EndReadNullTerminatedStrField, fc);
            break;
        case FcDeepType::NullTerminatedStrUtf32:
            this->_prepareToReadContainerField(_State::BeginReadNullTerminatedStrFieldUtf32,
                                               _State::EndReadNullTerminatedStrField, fc);
            break;
        case FcDeepType::StaticLenStr:
            this->_prepareToReadContainerField(_State::BeginReadStaticLenStrField,
                                               _State::EndReadStaticLenStrField, fc);
            break;
        case FcDeepType::DynLenStr:
            this->_prepareToReadContainerField(_State::BeginReadDynLenStrField,
                                               _State::EndReadDynLenStrField, fc);
            break;
        case FcDeepType::StaticLenBlob:
            this->_prepareToReadContainerField(_State::BeginReadStaticLenBlobField,
                                               _State::EndReadStaticLenBlobField, fc);
            break;
        case FcDeepType::StaticLenBlobWithMetadataStreamUuidRole:
            this->_prepareToReadContainerField(
                _State::BeginReadStaticLenBlobFieldMetadataStreamUuid,
                _State::EndReadStaticLenBlobField, fc);
            break;
        case FcDeepType::DynLenBlob:
            this->_prepareToReadContainerField(_State::BeginReadDynLenBlobField,
                                               _State::EndReadDynLenBlobField, fc);
            break;
        case FcDeepType::Struct:
            this->_prepareToReadStructField(fc.asStruct());
            break;
        case FcDeepType::StaticLenArray:
            this->_prepareToReadContainerField(_State::BeginReadStaticLenArrayField,
                                               _State::EndReadStaticLenArrayField, fc);
            break;
        case FcDeepType::StaticLenArrayWithMetadataStreamUuidRole:
            this->_prepareToReadContainerField(
                _State::BeginReadStaticLenArrayFieldMetadataStreamUuid,
                _State::EndReadStaticLenArrayField, fc);
            break;
        case FcDeepType::DynLenArray:
            this->_prepareToReadContainerField(_State::BeginReadDynLenArrayField,
                                               _State::EndReadDynLenArrayField, fc);
            break;
        case FcDeepType::OptionalWithBoolSel:
            this->_prepareToReadContainerField(_State::BeginReadOptionalFieldWithBoolSel,
                                               _State::EndReadOptionalFieldWithBoolSel, fc);
            break;
        case FcDeepType::OptionalWithUIntSel:
            this->_prepareToReadContainerField(_State::BeginReadOptionalFieldWithUIntSel,
                                               _State::EndReadOptionalFieldWithUIntSel, fc);
            break;
        case FcDeepType::OptionalWithSIntSel:
            this->_prepareToReadContainerField(_State::BeginReadOptionalFieldWithSIntSel,
                                               _State::EndReadOptionalFieldWithSIntSel, fc);
            break;
        case FcDeepType::VariantWithUIntSel:
            this->_prepareToReadContainerField(_State::BeginReadVariantFieldWithUIntSel,
                                               _State::EndReadVariantFieldWithUIntSel, fc);
            break;
        case FcDeepType::VariantWithSIntSel:
            this->_prepareToReadContainerField(_State::BeginReadVariantFieldWithSIntSel,
                                               _State::EndReadVariantFieldWithSIntSel, fc);
            break;
        default:
            bt_common_abort();
        }
    }

    /*
     * Sets the state and possibly updates the stack to read the next
     * field.
     */
    void _prepareToReadNextField()
    {
        auto& top = this->_stackTop();

        /* Next */
        ++top.elemIndex;

        if (top.elemIndex == top.len) {
            /*
             * Restore previous state.
             *
             * We don't call _stackPop() at this point because the
             * handler of the restored state typically needs
             * `this->_stackTop().fc` to update its item for the user.
             */
            this->_restoreState();
        } else {
            /* Find the class of the field to read next */
            BT_ASSERT_DBG(top.fc);

            if (top.fc->isStruct()) {
                this->_prepareToReadField(top.fc->asStruct()[top.elemIndex].fc());
            } else if (top.fc->isArray()) {
                this->_prepareToReadField(top.fc->asArray().elemFc());
            } else {
                /*
                 * `top.elemIndex == top.len` is always true for other
                 * compound (optional and variant) field class types.
                 */
                bt_common_abort();
            }
        }
    }

    /*
     * Sets the current state to `state`.
     */
    void _state(const _State state) noexcept
    {
        CTF_SRC_ITEM_SEQ_ITER_CPPLOGT("Changing state `{}` → `{}`: cur-state={}, new-state={}",
                                      wise_enum::to_string(_mState), wise_enum::to_string(state),
                                      wise_enum::to_string(_mState), wise_enum::to_string(state));
        _mState = state;
    }

    /*
     * Handles the current state.
     */
    _StateHandlingReaction _handleState()
    {
        CTF_SRC_ITEM_SEQ_ITER_CPPLOGT("Handling state `{}`: state={}, stack-len={}",
                                      wise_enum::to_string(_mState), wise_enum::to_string(_mState),
                                      _mStack.size());

        switch (_mState) {
        case _State::Init:
            return this->_handleInitState();
        case _State::TryBeginReadPkt:
            return this->_handleTryBeginReadPktState();
        case _State::BeginReadPktContent:
            return this->_handleBeginReadPktContentState();
        case _State::TryBeginReadPktHeaderScope:
            return this->_handleTryBeginReadPktHeaderScopeState();
        case _State::TryBeginReadPktCtxScope:
            return this->_handleTryBeginReadPktCtxScopeState();
        case _State::TryBeginReadEventRecordHeaderScope:
            return this->_handleTryBeginReadEventRecordHeaderScopeState();
        case _State::TryBeginReadCommonEventRecordCtxScope:
            return this->_handleTryBeginReadCommonEventRecordCtxScopeState();
        case _State::TryBeginReadSpecEventRecordCtxScope:
            return this->_handleTryBeginReadSpecEventRecordCtxScopeState();
        case _State::TryBeginReadEventRecordPayloadScope:
            return this->_handleTryBeginReadEventRecordPayloadScopeState();
        case _State::EndReadPktHeaderScope:
            return this->_handleEndReadPktHeaderScopeState();
        case _State::EndReadPktCtxScope:
            return this->_handleEndReadPktCtxScopeState();
        case _State::EndReadEventRecordHeaderScope:
            return this->_handleEndReadEventRecordHeaderScopeState();
        case _State::EndReadCommonEventRecordCtxScope:
            return this->_handleEndReadCommonEventRecordCtxScopeState();
        case _State::EndReadSpecEventRecordCtxScope:
            return this->_handleEndReadSpecEventRecordCtxScopeState();
        case _State::EndReadEventRecordPayloadScope:
            return this->_handleEndReadEventRecordPayloadScopeState();
        case _State::TryBeginReadEventRecord:
            return this->_handleTryBeginReadEventRecordState();
        case _State::EndReadEventRecord:
            return this->_handleEndReadEventRecordState();
        case _State::BeginReadStructField:
            return this->_handleBeginReadStructFieldState();
        case _State::EndReadStructField:
            return this->_handleEndReadStructFieldState();
        case _State::BeginReadStaticLenArrayField:
            return this->_handleBeginReadStaticLenArrayFieldState();
        case _State::BeginReadStaticLenArrayFieldMetadataStreamUuid:
            return this->_handleBeginReadStaticLenArrayFieldMetadataStreamUuidState();
        case _State::SetMetadataStreamUuidItem:
            return this->_handleSetMetadataStreamUuidItemState();
        case _State::EndReadStaticLenArrayField:
            return this->_handleEndReadStaticLenArrayFieldState();
        case _State::BeginReadDynLenArrayField:
            return this->_handleBeginReadDynLenArrayFieldState();
        case _State::EndReadDynLenArrayField:
            return this->_handleEndReadDynLenArrayFieldState();
        case _State::BeginReadNullTerminatedStrFieldUtf8:
            return this->_handleBeginReadNullTerminatedStrFieldUtf8State();
        case _State::BeginReadNullTerminatedStrFieldUtf16:
            return this->_handleBeginReadNullTerminatedStrFieldUtf16State();
        case _State::BeginReadNullTerminatedStrFieldUtf32:
            return this->_handleBeginReadNullTerminatedStrFieldUtf32State();
        case _State::EndReadNullTerminatedStrField:
            return this->_handleEndReadNullTerminatedStrFieldState();
        case _State::ReadSubstrUntilNullCodepointUtf8:
            return this->_handleReadSubstrUntilNullCodepointUtf8State();
        case _State::ReadSubstrUntilNullCodepointUtf16:
            return this->_handleReadSubstrUntilNullCodepointUtf16State();
        case _State::ReadSubstrUntilNullCodepointUtf32:
            return this->_handleReadSubstrUntilNullCodepointUtf32State();
        case _State::BeginReadStaticLenStrField:
            return this->_handleBeginReadStaticLenStrFieldState();
        case _State::EndReadStaticLenStrField:
            return this->_handleEndReadStaticLenStrFieldState();
        case _State::BeginReadDynLenStrField:
            return this->_handleBeginReadDynLenStrFieldState();
        case _State::EndReadDynLenStrField:
            return this->_handleEndReadDynLenStrFieldState();
        case _State::ReadRawData:
            return this->_handleReadRawDataState();
        case _State::BeginReadStaticLenBlobField:
            return this->_handleBeginReadStaticLenBlobFieldState();
        case _State::BeginReadStaticLenBlobFieldMetadataStreamUuid:
            return this->_handleBeginReadStaticLenBlobFieldMetadataStreamUuidState();
        case _State::EndReadStaticLenBlobField:
            return this->_handleEndReadStaticLenBlobFieldState();
        case _State::BeginReadDynLenBlobField:
            return this->_handleBeginReadDynLenBlobFieldState();
        case _State::EndReadDynLenBlobField:
            return this->_handleEndReadDynLenBlobFieldState();
        case _State::ReadMetadataStreamUuidBlobFieldSection:
            return this->_handleReadMetadataStreamUuidBlobFieldSectionState();
        case _State::BeginReadVariantFieldWithUIntSel:
            return this->_handleBeginReadVariantFieldWithUIntSelState();
        case _State::EndReadVariantFieldWithUIntSel:
            return this->_handleEndReadVariantFieldWithUIntSelState();
        case _State::BeginReadVariantFieldWithSIntSel:
            return this->_handleBeginReadVariantFieldWithSIntSelState();
        case _State::EndReadVariantFieldWithSIntSel:
            return this->_handleEndReadVariantFieldWithSIntSelState();
        case _State::BeginReadOptionalFieldWithBoolSel:
            return this->_handleBeginReadOptionalFieldWithBoolSelState();
        case _State::EndReadOptionalFieldWithBoolSel:
            return this->_handleEndReadOptionalFieldWithBoolSelState();
        case _State::BeginReadOptionalFieldWithUIntSel:
            return this->_handleBeginReadOptionalFieldWithUIntSelState();
        case _State::EndReadOptionalFieldWithUIntSel:
            return this->_handleEndReadOptionalFieldWithUIntSelState();
        case _State::BeginReadOptionalFieldWithSIntSel:
            return this->_handleBeginReadOptionalFieldWithSIntSelState();
        case _State::EndReadOptionalFieldWithSIntSel:
            return this->_handleEndReadOptionalFieldWithSIntSelState();
        case _State::ReadFixedLenBitArrayFieldBe:
            return this->_handleReadFixedLenBitArrayFieldBeState();
        case _State::ReadFixedLenBitArrayFieldLe:
            return this->_handleReadFixedLenBitArrayFieldLeState();
        case _State::ReadFixedLenBitArrayFieldBa8:
            return this->_handleReadFixedLenBitArrayFieldBa8State();
        case _State::ReadFixedLenBitArrayFieldBa16Le:
            return this->_handleReadFixedLenBitArrayFieldBa16LeState();
        case _State::ReadFixedLenBitArrayFieldBa16Be:
            return this->_handleReadFixedLenBitArrayFieldBa16BeState();
        case _State::ReadFixedLenBitArrayFieldBa32Le:
            return this->_handleReadFixedLenBitArrayFieldBa32LeState();
        case _State::ReadFixedLenBitArrayFieldBa32Be:
            return this->_handleReadFixedLenBitArrayFieldBa32BeState();
        case _State::ReadFixedLenBitArrayFieldBa64Le:
            return this->_handleReadFixedLenBitArrayFieldBa64LeState();
        case _State::ReadFixedLenBitArrayFieldBa64Be:
            return this->_handleReadFixedLenBitArrayFieldBa64BeState();
        case _State::ReadFixedLenBitMapFieldBe:
            return this->_handleReadFixedLenBitMapFieldBeState();
        case _State::ReadFixedLenBitMapFieldLe:
            return this->_handleReadFixedLenBitMapFieldLeState();
        case _State::ReadFixedLenBitMapFieldBa8:
            return this->_handleReadFixedLenBitMapFieldBa8State();
        case _State::ReadFixedLenBitMapFieldBa16Le:
            return this->_handleReadFixedLenBitMapFieldBa16LeState();
        case _State::ReadFixedLenBitMapFieldBa16Be:
            return this->_handleReadFixedLenBitMapFieldBa16BeState();
        case _State::ReadFixedLenBitMapFieldBa32Le:
            return this->_handleReadFixedLenBitMapFieldBa32LeState();
        case _State::ReadFixedLenBitMapFieldBa32Be:
            return this->_handleReadFixedLenBitMapFieldBa32BeState();
        case _State::ReadFixedLenBitMapFieldBa64Le:
            return this->_handleReadFixedLenBitMapFieldBa64LeState();
        case _State::ReadFixedLenBitMapFieldBa64Be:
            return this->_handleReadFixedLenBitMapFieldBa64BeState();
        case _State::ReadFixedLenBoolFieldBe:
            return this->_handleReadFixedLenBoolFieldBeState();
        case _State::ReadFixedLenBoolFieldLe:
            return this->_handleReadFixedLenBoolFieldLeState();
        case _State::ReadFixedLenBoolFieldBa8:
            return this->_handleReadFixedLenBoolFieldBa8State();
        case _State::ReadFixedLenBoolFieldBa16Le:
            return this->_handleReadFixedLenBoolFieldBa16LeState();
        case _State::ReadFixedLenBoolFieldBa16Be:
            return this->_handleReadFixedLenBoolFieldBa16BeState();
        case _State::ReadFixedLenBoolFieldBa32Le:
            return this->_handleReadFixedLenBoolFieldBa32LeState();
        case _State::ReadFixedLenBoolFieldBa32Be:
            return this->_handleReadFixedLenBoolFieldBa32BeState();
        case _State::ReadFixedLenBoolFieldBa64Le:
            return this->_handleReadFixedLenBoolFieldBa64LeState();
        case _State::ReadFixedLenBoolFieldBa64Be:
            return this->_handleReadFixedLenBoolFieldBa64BeState();
        case _State::ReadFixedLenBoolFieldBeSaveVal:
            return this->_handleReadFixedLenBoolFieldBeSaveValState();
        case _State::ReadFixedLenBoolFieldLeSaveVal:
            return this->_handleReadFixedLenBoolFieldLeSaveValState();
        case _State::ReadFixedLenBoolFieldBa8SaveVal:
            return this->_handleReadFixedLenBoolFieldBa8SaveValState();
        case _State::ReadFixedLenBoolFieldBa16LeSaveVal:
            return this->_handleReadFixedLenBoolFieldBa16LeSaveValState();
        case _State::ReadFixedLenBoolFieldBa16BeSaveVal:
            return this->_handleReadFixedLenBoolFieldBa16BeSaveValState();
        case _State::ReadFixedLenBoolFieldBa32LeSaveVal:
            return this->_handleReadFixedLenBoolFieldBa32LeSaveValState();
        case _State::ReadFixedLenBoolFieldBa32BeSaveVal:
            return this->_handleReadFixedLenBoolFieldBa32BeSaveValState();
        case _State::ReadFixedLenBoolFieldBa64LeSaveVal:
            return this->_handleReadFixedLenBoolFieldBa64LeSaveValState();
        case _State::ReadFixedLenBoolFieldBa64BeSaveVal:
            return this->_handleReadFixedLenBoolFieldBa64BeSaveValState();
        case _State::ReadFixedLenFloatField32Be:
            return this->_handleReadFixedLenFloatField32BeState();
        case _State::ReadFixedLenFloatField32Le:
            return this->_handleReadFixedLenFloatField32LeState();
        case _State::ReadFixedLenFloatField64Be:
            return this->_handleReadFixedLenFloatField64BeState();
        case _State::ReadFixedLenFloatField64Le:
            return this->_handleReadFixedLenFloatField64LeState();
        case _State::ReadFixedLenFloatFieldBa32Le:
            return this->_handleReadFixedLenFloatFieldBa32LeState();
        case _State::ReadFixedLenFloatFieldBa32Be:
            return this->_handleReadFixedLenFloatFieldBa32BeState();
        case _State::ReadFixedLenFloatFieldBa64Le:
            return this->_handleReadFixedLenFloatFieldBa64LeState();
        case _State::ReadFixedLenFloatFieldBa64Be:
            return this->_handleReadFixedLenFloatFieldBa64BeState();
        case _State::ReadFixedLenUIntFieldBe:
            return this->_handleReadFixedLenUIntFieldBeState();
        case _State::ReadFixedLenUIntFieldLe:
            return this->_handleReadFixedLenUIntFieldLeState();
        case _State::ReadFixedLenUIntFieldBa8:
            return this->_handleReadFixedLenUIntFieldBa8State();
        case _State::ReadFixedLenUIntFieldBa16Le:
            return this->_handleReadFixedLenUIntFieldBa16LeState();
        case _State::ReadFixedLenUIntFieldBa16Be:
            return this->_handleReadFixedLenUIntFieldBa16BeState();
        case _State::ReadFixedLenUIntFieldBa32Le:
            return this->_handleReadFixedLenUIntFieldBa32LeState();
        case _State::ReadFixedLenUIntFieldBa32Be:
            return this->_handleReadFixedLenUIntFieldBa32BeState();
        case _State::ReadFixedLenUIntFieldBa64Le:
            return this->_handleReadFixedLenUIntFieldBa64LeState();
        case _State::ReadFixedLenUIntFieldBa64Be:
            return this->_handleReadFixedLenUIntFieldBa64BeState();
        case _State::ReadFixedLenUIntFieldBeWithRole:
            return this->_handleReadFixedLenUIntFieldBeWithRoleState();
        case _State::ReadFixedLenUIntFieldLeWithRole:
            return this->_handleReadFixedLenUIntFieldLeWithRoleState();
        case _State::ReadFixedLenUIntFieldBa8WithRole:
            return this->_handleReadFixedLenUIntFieldBa8WithRoleState();
        case _State::ReadFixedLenUIntFieldBa16LeWithRole:
            return this->_handleReadFixedLenUIntFieldBa16LeWithRoleState();
        case _State::ReadFixedLenUIntFieldBa16BeWithRole:
            return this->_handleReadFixedLenUIntFieldBa16BeWithRoleState();
        case _State::ReadFixedLenUIntFieldBa32LeWithRole:
            return this->_handleReadFixedLenUIntFieldBa32LeWithRoleState();
        case _State::ReadFixedLenUIntFieldBa32BeWithRole:
            return this->_handleReadFixedLenUIntFieldBa32BeWithRoleState();
        case _State::ReadFixedLenUIntFieldBa64LeWithRole:
            return this->_handleReadFixedLenUIntFieldBa64LeWithRoleState();
        case _State::ReadFixedLenUIntFieldBa64BeWithRole:
            return this->_handleReadFixedLenUIntFieldBa64BeWithRoleState();
        case _State::ReadFixedLenUIntFieldBeSaveVal:
            return this->_handleReadFixedLenUIntFieldBeSaveValState();
        case _State::ReadFixedLenUIntFieldLeSaveVal:
            return this->_handleReadFixedLenUIntFieldLeSaveValState();
        case _State::ReadFixedLenUIntFieldBa8SaveVal:
            return this->_handleReadFixedLenUIntFieldBa8SaveValState();
        case _State::ReadFixedLenUIntFieldBa16LeSaveVal:
            return this->_handleReadFixedLenUIntFieldBa16LeSaveValState();
        case _State::ReadFixedLenUIntFieldBa16BeSaveVal:
            return this->_handleReadFixedLenUIntFieldBa16BeSaveValState();
        case _State::ReadFixedLenUIntFieldBa32LeSaveVal:
            return this->_handleReadFixedLenUIntFieldBa32LeSaveValState();
        case _State::ReadFixedLenUIntFieldBa32BeSaveVal:
            return this->_handleReadFixedLenUIntFieldBa32BeSaveValState();
        case _State::ReadFixedLenUIntFieldBa64LeSaveVal:
            return this->_handleReadFixedLenUIntFieldBa64LeSaveValState();
        case _State::ReadFixedLenUIntFieldBa64BeSaveVal:
            return this->_handleReadFixedLenUIntFieldBa64BeSaveValState();
        case _State::ReadFixedLenUIntFieldBeWithRoleSaveVal:
            return this->_handleReadFixedLenUIntFieldBeWithRoleSaveValState();
        case _State::ReadFixedLenUIntFieldLeWithRoleSaveVal:
            return this->_handleReadFixedLenUIntFieldLeWithRoleSaveValState();
        case _State::ReadFixedLenUIntFieldBa8WithRoleSaveVal:
            return this->_handleReadFixedLenUIntFieldBa8WithRoleSaveValState();
        case _State::ReadFixedLenUIntFieldBa16LeWithRoleSaveVal:
            return this->_handleReadFixedLenUIntFieldBa16LeWithRoleSaveValState();
        case _State::ReadFixedLenUIntFieldBa16BeWithRoleSaveVal:
            return this->_handleReadFixedLenUIntFieldBa16BeWithRoleSaveValState();
        case _State::ReadFixedLenUIntFieldBa32LeWithRoleSaveVal:
            return this->_handleReadFixedLenUIntFieldBa32LeWithRoleSaveValState();
        case _State::ReadFixedLenUIntFieldBa32BeWithRoleSaveVal:
            return this->_handleReadFixedLenUIntFieldBa32BeWithRoleSaveValState();
        case _State::ReadFixedLenUIntFieldBa64LeWithRoleSaveVal:
            return this->_handleReadFixedLenUIntFieldBa64LeWithRoleSaveValState();
        case _State::ReadFixedLenUIntFieldBa64BeWithRoleSaveVal:
            return this->_handleReadFixedLenUIntFieldBa64BeWithRoleSaveValState();
        case _State::ReadFixedLenSIntFieldBe:
            return this->_handleReadFixedLenSIntFieldBeState();
        case _State::ReadFixedLenSIntFieldLe:
            return this->_handleReadFixedLenSIntFieldLeState();
        case _State::ReadFixedLenSIntFieldBa8:
            return this->_handleReadFixedLenSIntFieldBa8State();
        case _State::ReadFixedLenSIntFieldBa16Le:
            return this->_handleReadFixedLenSIntFieldBa16LeState();
        case _State::ReadFixedLenSIntFieldBa16Be:
            return this->_handleReadFixedLenSIntFieldBa16BeState();
        case _State::ReadFixedLenSIntFieldBa32Le:
            return this->_handleReadFixedLenSIntFieldBa32LeState();
        case _State::ReadFixedLenSIntFieldBa32Be:
            return this->_handleReadFixedLenSIntFieldBa32BeState();
        case _State::ReadFixedLenSIntFieldBa64Le:
            return this->_handleReadFixedLenSIntFieldBa64LeState();
        case _State::ReadFixedLenSIntFieldBa64Be:
            return this->_handleReadFixedLenSIntFieldBa64BeState();
        case _State::ReadFixedLenSIntFieldBeSaveVal:
            return this->_handleReadFixedLenSIntFieldBeSaveValState();
        case _State::ReadFixedLenSIntFieldLeSaveVal:
            return this->_handleReadFixedLenSIntFieldLeSaveValState();
        case _State::ReadFixedLenSIntFieldBa8SaveVal:
            return this->_handleReadFixedLenSIntFieldBa8SaveValState();
        case _State::ReadFixedLenSIntFieldBa16LeSaveVal:
            return this->_handleReadFixedLenSIntFieldBa16LeSaveValState();
        case _State::ReadFixedLenSIntFieldBa16BeSaveVal:
            return this->_handleReadFixedLenSIntFieldBa16BeSaveValState();
        case _State::ReadFixedLenSIntFieldBa32LeSaveVal:
            return this->_handleReadFixedLenSIntFieldBa32LeSaveValState();
        case _State::ReadFixedLenSIntFieldBa32BeSaveVal:
            return this->_handleReadFixedLenSIntFieldBa32BeSaveValState();
        case _State::ReadFixedLenSIntFieldBa64LeSaveVal:
            return this->_handleReadFixedLenSIntFieldBa64LeSaveValState();
        case _State::ReadFixedLenSIntFieldBa64BeSaveVal:
            return this->_handleReadFixedLenSIntFieldBa64BeSaveValState();
        case _State::ReadFixedLenBitArrayFieldBeRev:
            return this->_handleReadFixedLenBitArrayFieldBeRevState();
        case _State::ReadFixedLenBitArrayFieldLeRev:
            return this->_handleReadFixedLenBitArrayFieldLeRevState();
        case _State::ReadFixedLenBitArrayFieldBa8Rev:
            return this->_handleReadFixedLenBitArrayFieldBa8RevState();
        case _State::ReadFixedLenBitArrayFieldBa16LeRev:
            return this->_handleReadFixedLenBitArrayFieldBa16LeRevState();
        case _State::ReadFixedLenBitArrayFieldBa16BeRev:
            return this->_handleReadFixedLenBitArrayFieldBa16BeRevState();
        case _State::ReadFixedLenBitArrayFieldBa32LeRev:
            return this->_handleReadFixedLenBitArrayFieldBa32LeRevState();
        case _State::ReadFixedLenBitArrayFieldBa32BeRev:
            return this->_handleReadFixedLenBitArrayFieldBa32BeRevState();
        case _State::ReadFixedLenBitArrayFieldBa64LeRev:
            return this->_handleReadFixedLenBitArrayFieldBa64LeRevState();
        case _State::ReadFixedLenBitArrayFieldBa64BeRev:
            return this->_handleReadFixedLenBitArrayFieldBa64BeRevState();
        case _State::ReadFixedLenBitMapFieldBeRev:
            return this->_handleReadFixedLenBitMapFieldBeRevState();
        case _State::ReadFixedLenBitMapFieldLeRev:
            return this->_handleReadFixedLenBitMapFieldLeRevState();
        case _State::ReadFixedLenBitMapFieldBa8Rev:
            return this->_handleReadFixedLenBitMapFieldBa8RevState();
        case _State::ReadFixedLenBitMapFieldBa16LeRev:
            return this->_handleReadFixedLenBitMapFieldBa16LeRevState();
        case _State::ReadFixedLenBitMapFieldBa16BeRev:
            return this->_handleReadFixedLenBitMapFieldBa16BeRevState();
        case _State::ReadFixedLenBitMapFieldBa32LeRev:
            return this->_handleReadFixedLenBitMapFieldBa32LeRevState();
        case _State::ReadFixedLenBitMapFieldBa32BeRev:
            return this->_handleReadFixedLenBitMapFieldBa32BeRevState();
        case _State::ReadFixedLenBitMapFieldBa64LeRev:
            return this->_handleReadFixedLenBitMapFieldBa64LeRevState();
        case _State::ReadFixedLenBitMapFieldBa64BeRev:
            return this->_handleReadFixedLenBitMapFieldBa64BeRevState();
        case _State::ReadFixedLenBoolFieldBeRev:
            return this->_handleReadFixedLenBoolFieldBeRevState();
        case _State::ReadFixedLenBoolFieldLeRev:
            return this->_handleReadFixedLenBoolFieldLeRevState();
        case _State::ReadFixedLenBoolFieldBa8Rev:
            return this->_handleReadFixedLenBoolFieldBa8RevState();
        case _State::ReadFixedLenBoolFieldBa16LeRev:
            return this->_handleReadFixedLenBoolFieldBa16LeRevState();
        case _State::ReadFixedLenBoolFieldBa16BeRev:
            return this->_handleReadFixedLenBoolFieldBa16BeRevState();
        case _State::ReadFixedLenBoolFieldBa32LeRev:
            return this->_handleReadFixedLenBoolFieldBa32LeRevState();
        case _State::ReadFixedLenBoolFieldBa32BeRev:
            return this->_handleReadFixedLenBoolFieldBa32BeRevState();
        case _State::ReadFixedLenBoolFieldBa64LeRev:
            return this->_handleReadFixedLenBoolFieldBa64LeRevState();
        case _State::ReadFixedLenBoolFieldBa64BeRev:
            return this->_handleReadFixedLenBoolFieldBa64BeRevState();
        case _State::ReadFixedLenBoolFieldBeRevSaveVal:
            return this->_handleReadFixedLenBoolFieldBeRevSaveValState();
        case _State::ReadFixedLenBoolFieldLeRevSaveVal:
            return this->_handleReadFixedLenBoolFieldLeRevSaveValState();
        case _State::ReadFixedLenBoolFieldBa8RevSaveVal:
            return this->_handleReadFixedLenBoolFieldBa8RevSaveValState();
        case _State::ReadFixedLenBoolFieldBa16LeRevSaveVal:
            return this->_handleReadFixedLenBoolFieldBa16LeRevSaveValState();
        case _State::ReadFixedLenBoolFieldBa16BeRevSaveVal:
            return this->_handleReadFixedLenBoolFieldBa16BeRevSaveValState();
        case _State::ReadFixedLenBoolFieldBa32LeRevSaveVal:
            return this->_handleReadFixedLenBoolFieldBa32LeRevSaveValState();
        case _State::ReadFixedLenBoolFieldBa32BeRevSaveVal:
            return this->_handleReadFixedLenBoolFieldBa32BeRevSaveValState();
        case _State::ReadFixedLenBoolFieldBa64LeRevSaveVal:
            return this->_handleReadFixedLenBoolFieldBa64LeRevSaveValState();
        case _State::ReadFixedLenBoolFieldBa64BeRevSaveVal:
            return this->_handleReadFixedLenBoolFieldBa64BeRevSaveValState();
        case _State::ReadFixedLenFloatField32BeRev:
            return this->_handleReadFixedLenFloatField32BeRevState();
        case _State::ReadFixedLenFloatField32LeRev:
            return this->_handleReadFixedLenFloatField32LeRevState();
        case _State::ReadFixedLenFloatField64BeRev:
            return this->_handleReadFixedLenFloatField64BeRevState();
        case _State::ReadFixedLenFloatField64LeRev:
            return this->_handleReadFixedLenFloatField64LeRevState();
        case _State::ReadFixedLenFloatFieldBa32LeRev:
            return this->_handleReadFixedLenFloatFieldBa32LeRevState();
        case _State::ReadFixedLenFloatFieldBa32BeRev:
            return this->_handleReadFixedLenFloatFieldBa32BeRevState();
        case _State::ReadFixedLenFloatFieldBa64LeRev:
            return this->_handleReadFixedLenFloatFieldBa64LeRevState();
        case _State::ReadFixedLenFloatFieldBa64BeRev:
            return this->_handleReadFixedLenFloatFieldBa64BeRevState();
        case _State::ReadFixedLenUIntFieldBeRev:
            return this->_handleReadFixedLenUIntFieldBeRevState();
        case _State::ReadFixedLenUIntFieldLeRev:
            return this->_handleReadFixedLenUIntFieldLeRevState();
        case _State::ReadFixedLenUIntFieldBa8Rev:
            return this->_handleReadFixedLenUIntFieldBa8RevState();
        case _State::ReadFixedLenUIntFieldBa16LeRev:
            return this->_handleReadFixedLenUIntFieldBa16LeRevState();
        case _State::ReadFixedLenUIntFieldBa16BeRev:
            return this->_handleReadFixedLenUIntFieldBa16BeRevState();
        case _State::ReadFixedLenUIntFieldBa32LeRev:
            return this->_handleReadFixedLenUIntFieldBa32LeRevState();
        case _State::ReadFixedLenUIntFieldBa32BeRev:
            return this->_handleReadFixedLenUIntFieldBa32BeRevState();
        case _State::ReadFixedLenUIntFieldBa64LeRev:
            return this->_handleReadFixedLenUIntFieldBa64LeRevState();
        case _State::ReadFixedLenUIntFieldBa64BeRev:
            return this->_handleReadFixedLenUIntFieldBa64BeRevState();
        case _State::ReadFixedLenUIntFieldBeRevWithRole:
            return this->_handleReadFixedLenUIntFieldBeRevWithRoleState();
        case _State::ReadFixedLenUIntFieldLeRevWithRole:
            return this->_handleReadFixedLenUIntFieldLeRevWithRoleState();
        case _State::ReadFixedLenUIntFieldBa8RevWithRole:
            return this->_handleReadFixedLenUIntFieldBa8RevWithRoleState();
        case _State::ReadFixedLenUIntFieldBa16LeRevWithRole:
            return this->_handleReadFixedLenUIntFieldBa16LeRevWithRoleState();
        case _State::ReadFixedLenUIntFieldBa16BeRevWithRole:
            return this->_handleReadFixedLenUIntFieldBa16BeRevWithRoleState();
        case _State::ReadFixedLenUIntFieldBa32LeRevWithRole:
            return this->_handleReadFixedLenUIntFieldBa32LeRevWithRoleState();
        case _State::ReadFixedLenUIntFieldBa32BeRevWithRole:
            return this->_handleReadFixedLenUIntFieldBa32BeRevWithRoleState();
        case _State::ReadFixedLenUIntFieldBa64LeRevWithRole:
            return this->_handleReadFixedLenUIntFieldBa64LeRevWithRoleState();
        case _State::ReadFixedLenUIntFieldBa64BeRevWithRole:
            return this->_handleReadFixedLenUIntFieldBa64BeRevWithRoleState();
        case _State::ReadFixedLenUIntFieldBeRevSaveVal:
            return this->_handleReadFixedLenUIntFieldBeRevSaveValState();
        case _State::ReadFixedLenUIntFieldLeRevSaveVal:
            return this->_handleReadFixedLenUIntFieldLeRevSaveValState();
        case _State::ReadFixedLenUIntFieldBa8RevSaveVal:
            return this->_handleReadFixedLenUIntFieldBa8RevSaveValState();
        case _State::ReadFixedLenUIntFieldBa16LeRevSaveVal:
            return this->_handleReadFixedLenUIntFieldBa16LeRevSaveValState();
        case _State::ReadFixedLenUIntFieldBa16BeRevSaveVal:
            return this->_handleReadFixedLenUIntFieldBa16BeRevSaveValState();
        case _State::ReadFixedLenUIntFieldBa32LeRevSaveVal:
            return this->_handleReadFixedLenUIntFieldBa32LeRevSaveValState();
        case _State::ReadFixedLenUIntFieldBa32BeRevSaveVal:
            return this->_handleReadFixedLenUIntFieldBa32BeRevSaveValState();
        case _State::ReadFixedLenUIntFieldBa64LeRevSaveVal:
            return this->_handleReadFixedLenUIntFieldBa64LeRevSaveValState();
        case _State::ReadFixedLenUIntFieldBa64BeRevSaveVal:
            return this->_handleReadFixedLenUIntFieldBa64BeRevSaveValState();
        case _State::ReadFixedLenUIntFieldBeRevWithRoleSaveVal:
            return this->_handleReadFixedLenUIntFieldBeRevWithRoleSaveValState();
        case _State::ReadFixedLenUIntFieldLeRevWithRoleSaveVal:
            return this->_handleReadFixedLenUIntFieldLeRevWithRoleSaveValState();
        case _State::ReadFixedLenUIntFieldBa8RevWithRoleSaveVal:
            return this->_handleReadFixedLenUIntFieldBa8RevWithRoleSaveValState();
        case _State::ReadFixedLenUIntFieldBa16LeRevWithRoleSaveVal:
            return this->_handleReadFixedLenUIntFieldBa16LeRevWithRoleSaveValState();
        case _State::ReadFixedLenUIntFieldBa16BeRevWithRoleSaveVal:
            return this->_handleReadFixedLenUIntFieldBa16BeRevWithRoleSaveValState();
        case _State::ReadFixedLenUIntFieldBa32LeRevWithRoleSaveVal:
            return this->_handleReadFixedLenUIntFieldBa32LeRevWithRoleSaveValState();
        case _State::ReadFixedLenUIntFieldBa32BeRevWithRoleSaveVal:
            return this->_handleReadFixedLenUIntFieldBa32BeRevWithRoleSaveValState();
        case _State::ReadFixedLenUIntFieldBa64LeRevWithRoleSaveVal:
            return this->_handleReadFixedLenUIntFieldBa64LeRevWithRoleSaveValState();
        case _State::ReadFixedLenUIntFieldBa64BeRevWithRoleSaveVal:
            return this->_handleReadFixedLenUIntFieldBa64BeRevWithRoleSaveValState();
        case _State::ReadFixedLenSIntFieldBeRev:
            return this->_handleReadFixedLenSIntFieldBeRevState();
        case _State::ReadFixedLenSIntFieldLeRev:
            return this->_handleReadFixedLenSIntFieldLeRevState();
        case _State::ReadFixedLenSIntFieldBa8Rev:
            return this->_handleReadFixedLenSIntFieldBa8RevState();
        case _State::ReadFixedLenSIntFieldBa16LeRev:
            return this->_handleReadFixedLenSIntFieldBa16LeRevState();
        case _State::ReadFixedLenSIntFieldBa16BeRev:
            return this->_handleReadFixedLenSIntFieldBa16BeRevState();
        case _State::ReadFixedLenSIntFieldBa32LeRev:
            return this->_handleReadFixedLenSIntFieldBa32LeRevState();
        case _State::ReadFixedLenSIntFieldBa32BeRev:
            return this->_handleReadFixedLenSIntFieldBa32BeRevState();
        case _State::ReadFixedLenSIntFieldBa64LeRev:
            return this->_handleReadFixedLenSIntFieldBa64LeRevState();
        case _State::ReadFixedLenSIntFieldBa64BeRev:
            return this->_handleReadFixedLenSIntFieldBa64BeRevState();
        case _State::ReadFixedLenSIntFieldBeRevSaveVal:
            return this->_handleReadFixedLenSIntFieldBeRevSaveValState();
        case _State::ReadFixedLenSIntFieldLeRevSaveVal:
            return this->_handleReadFixedLenSIntFieldLeRevSaveValState();
        case _State::ReadFixedLenSIntFieldBa8RevSaveVal:
            return this->_handleReadFixedLenSIntFieldBa8RevSaveValState();
        case _State::ReadFixedLenSIntFieldBa16LeRevSaveVal:
            return this->_handleReadFixedLenSIntFieldBa16LeRevSaveValState();
        case _State::ReadFixedLenSIntFieldBa16BeRevSaveVal:
            return this->_handleReadFixedLenSIntFieldBa16BeRevSaveValState();
        case _State::ReadFixedLenSIntFieldBa32LeRevSaveVal:
            return this->_handleReadFixedLenSIntFieldBa32LeRevSaveValState();
        case _State::ReadFixedLenSIntFieldBa32BeRevSaveVal:
            return this->_handleReadFixedLenSIntFieldBa32BeRevSaveValState();
        case _State::ReadFixedLenSIntFieldBa64LeRevSaveVal:
            return this->_handleReadFixedLenSIntFieldBa64LeRevSaveValState();
        case _State::ReadFixedLenSIntFieldBa64BeRevSaveVal:
            return this->_handleReadFixedLenSIntFieldBa64BeRevSaveValState();
        case _State::ReadVarLenUIntField:
            return this->_handleReadVarLenUIntFieldState();
        case _State::ReadVarLenUIntFieldWithRole:
            return this->_handleReadVarLenUIntFieldWithRoleState();
        case _State::ReadVarLenUIntFieldSaveVal:
            return this->_handleReadVarLenUIntFieldSaveValState();
        case _State::ReadVarLenUIntFieldWithRoleSaveVal:
            return this->_handleReadVarLenUIntFieldWithRoleSaveValState();
        case _State::ReadVarLenSIntField:
            return this->_handleReadVarLenSIntFieldState();
        case _State::ReadVarLenSIntFieldSaveVal:
            return this->_handleReadVarLenSIntFieldSaveValState();
        case _State::ReadFixedLenMetadataStreamUuidByteUIntFieldBa8:
            return this->_handleReadFixedLenMetadataStreamUuidByteUIntFieldBa8State();
        case _State::SetDataStreamInfoItem:
            return this->_handleSetDataStreamInfoItemState();
        case _State::SetPktInfoItem:
            return this->_handleSetPktInfoItemState();
        case _State::SetEventRecordInfoItem:
            return this->_handleSetEventRecordInfoItemState();
        case _State::SetPktMagicNumberItem:
            return this->_handleSetPktMagicNumberItem();
        case _State::SetDefClkValItem:
            return this->_handleSetDefClkValItem();
        case _State::EndReadPktContent:
            return this->_handleEndReadPktContentState();
        case _State::EndReadPkt:
            return this->_handleEndReadPktState();
        case _State::SkipPadding:
            return this->_handleSkipPaddingState();
        case _State::SkipContentPadding:
            return this->_handleSkipContentPaddingState();
        default:
            bt_common_abort();
        };
    };

    /* State handlers */
    _StateHandlingReaction _handleInitState();
    _StateHandlingReaction _handleSkipPaddingState();
    _StateHandlingReaction _handleSkipContentPaddingState();
    _StateHandlingReaction _handleTryBeginReadPktState();
    _StateHandlingReaction _handleEndReadPktState();
    _StateHandlingReaction _handleBeginReadPktContentState();
    _StateHandlingReaction _handleEndReadPktContentState();
    _StateHandlingReaction _handleSetPktMagicNumberItem();
    _StateHandlingReaction _handleSetDefClkValItem();
    _StateHandlingReaction _handleTryBeginReadPktHeaderScopeState();
    _StateHandlingReaction _handleTryBeginReadPktCtxScopeState();
    _StateHandlingReaction _handleTryBeginReadEventRecordHeaderScopeState();
    _StateHandlingReaction _handleTryBeginReadCommonEventRecordCtxScopeState();
    _StateHandlingReaction _handleTryBeginReadSpecEventRecordCtxScopeState();
    _StateHandlingReaction _handleTryBeginReadEventRecordPayloadScopeState();
    _StateHandlingReaction _handleEndReadPktHeaderScopeState();
    _StateHandlingReaction _handleEndReadPktCtxScopeState();
    _StateHandlingReaction _handleEndReadEventRecordHeaderScopeState();
    _StateHandlingReaction _handleEndReadCommonEventRecordCtxScopeState();
    _StateHandlingReaction _handleEndReadSpecEventRecordCtxScopeState();
    _StateHandlingReaction _handleEndReadEventRecordPayloadScopeState();
    _StateHandlingReaction _handleTryBeginReadEventRecordState();
    _StateHandlingReaction _handleEndReadEventRecordState();
    _StateHandlingReaction _handleSetDataStreamInfoItemState();
    _StateHandlingReaction _handleSetPktInfoItemState();
    _StateHandlingReaction _handleSetEventRecordInfoItemState();
    _StateHandlingReaction _handleBeginReadStructFieldState();
    _StateHandlingReaction _handleEndReadStructFieldState();
    _StateHandlingReaction _handleBeginReadStaticLenArrayFieldState();
    _StateHandlingReaction _handleBeginReadStaticLenArrayFieldMetadataStreamUuidState();
    _StateHandlingReaction _handleSetMetadataStreamUuidItemState();
    _StateHandlingReaction _handleEndReadStaticLenArrayFieldState();
    _StateHandlingReaction _handleBeginReadDynLenArrayFieldState();
    _StateHandlingReaction _handleEndReadDynLenArrayFieldState();
    _StateHandlingReaction _handleBeginReadNullTerminatedStrFieldUtf8State();
    _StateHandlingReaction _handleBeginReadNullTerminatedStrFieldUtf16State();
    _StateHandlingReaction _handleBeginReadNullTerminatedStrFieldUtf32State();
    _StateHandlingReaction _handleEndReadNullTerminatedStrFieldState();
    _StateHandlingReaction _handleReadSubstrUntilNullCodepointUtf8State();
    _StateHandlingReaction _handleReadSubstrUntilNullCodepointUtf16State();
    _StateHandlingReaction _handleReadSubstrUntilNullCodepointUtf32State();
    _StateHandlingReaction _handleBeginReadStaticLenStrFieldState();
    _StateHandlingReaction _handleEndReadStaticLenStrFieldState();
    _StateHandlingReaction _handleBeginReadDynLenStrFieldState();
    _StateHandlingReaction _handleEndReadDynLenStrFieldState();
    _StateHandlingReaction _handleReadRawDataState();
    _StateHandlingReaction _handleBeginReadStaticLenBlobFieldState();
    _StateHandlingReaction _handleBeginReadStaticLenBlobFieldMetadataStreamUuidState();
    _StateHandlingReaction _handleEndReadStaticLenBlobFieldState();
    _StateHandlingReaction _handleBeginReadDynLenBlobFieldState();
    _StateHandlingReaction _handleEndReadDynLenBlobFieldState();
    _StateHandlingReaction _handleReadMetadataStreamUuidBlobFieldSectionState();
    _StateHandlingReaction _handleBeginReadVariantFieldWithUIntSelState();
    _StateHandlingReaction _handleEndReadVariantFieldWithUIntSelState();
    _StateHandlingReaction _handleBeginReadVariantFieldWithSIntSelState();
    _StateHandlingReaction _handleEndReadVariantFieldWithSIntSelState();
    _StateHandlingReaction _handleBeginReadOptionalFieldWithBoolSelState();
    _StateHandlingReaction _handleEndReadOptionalFieldWithBoolSelState();
    _StateHandlingReaction _handleBeginReadOptionalFieldWithUIntSelState();
    _StateHandlingReaction _handleEndReadOptionalFieldWithUIntSelState();
    _StateHandlingReaction _handleBeginReadOptionalFieldWithSIntSelState();
    _StateHandlingReaction _handleEndReadOptionalFieldWithSIntSelState();
    _StateHandlingReaction _handleReadFixedLenBitArrayFieldBeState();
    _StateHandlingReaction _handleReadFixedLenBitArrayFieldLeState();
    _StateHandlingReaction _handleReadFixedLenBitArrayFieldBa8State();
    _StateHandlingReaction _handleReadFixedLenBitArrayFieldBa16LeState();
    _StateHandlingReaction _handleReadFixedLenBitArrayFieldBa16BeState();
    _StateHandlingReaction _handleReadFixedLenBitArrayFieldBa32LeState();
    _StateHandlingReaction _handleReadFixedLenBitArrayFieldBa32BeState();
    _StateHandlingReaction _handleReadFixedLenBitArrayFieldBa64LeState();
    _StateHandlingReaction _handleReadFixedLenBitArrayFieldBa64BeState();
    _StateHandlingReaction _handleReadFixedLenBitMapFieldBeState();
    _StateHandlingReaction _handleReadFixedLenBitMapFieldLeState();
    _StateHandlingReaction _handleReadFixedLenBitMapFieldBa8State();
    _StateHandlingReaction _handleReadFixedLenBitMapFieldBa16LeState();
    _StateHandlingReaction _handleReadFixedLenBitMapFieldBa16BeState();
    _StateHandlingReaction _handleReadFixedLenBitMapFieldBa32LeState();
    _StateHandlingReaction _handleReadFixedLenBitMapFieldBa32BeState();
    _StateHandlingReaction _handleReadFixedLenBitMapFieldBa64LeState();
    _StateHandlingReaction _handleReadFixedLenBitMapFieldBa64BeState();
    _StateHandlingReaction _handleReadFixedLenBoolFieldBeState();
    _StateHandlingReaction _handleReadFixedLenBoolFieldLeState();
    _StateHandlingReaction _handleReadFixedLenBoolFieldBa8State();
    _StateHandlingReaction _handleReadFixedLenBoolFieldBa16LeState();
    _StateHandlingReaction _handleReadFixedLenBoolFieldBa16BeState();
    _StateHandlingReaction _handleReadFixedLenBoolFieldBa32LeState();
    _StateHandlingReaction _handleReadFixedLenBoolFieldBa32BeState();
    _StateHandlingReaction _handleReadFixedLenBoolFieldBa64LeState();
    _StateHandlingReaction _handleReadFixedLenBoolFieldBa64BeState();
    _StateHandlingReaction _handleReadFixedLenBoolFieldBeSaveValState();
    _StateHandlingReaction _handleReadFixedLenBoolFieldLeSaveValState();
    _StateHandlingReaction _handleReadFixedLenBoolFieldBa8SaveValState();
    _StateHandlingReaction _handleReadFixedLenBoolFieldBa16LeSaveValState();
    _StateHandlingReaction _handleReadFixedLenBoolFieldBa16BeSaveValState();
    _StateHandlingReaction _handleReadFixedLenBoolFieldBa32LeSaveValState();
    _StateHandlingReaction _handleReadFixedLenBoolFieldBa32BeSaveValState();
    _StateHandlingReaction _handleReadFixedLenBoolFieldBa64LeSaveValState();
    _StateHandlingReaction _handleReadFixedLenBoolFieldBa64BeSaveValState();
    _StateHandlingReaction _handleReadFixedLenFloatField32BeState();
    _StateHandlingReaction _handleReadFixedLenFloatField32LeState();
    _StateHandlingReaction _handleReadFixedLenFloatField64BeState();
    _StateHandlingReaction _handleReadFixedLenFloatField64LeState();
    _StateHandlingReaction _handleReadFixedLenFloatFieldBa32LeState();
    _StateHandlingReaction _handleReadFixedLenFloatFieldBa32BeState();
    _StateHandlingReaction _handleReadFixedLenFloatFieldBa64LeState();
    _StateHandlingReaction _handleReadFixedLenFloatFieldBa64BeState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBeState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldLeState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa8State();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa16LeState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa16BeState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa32LeState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa32BeState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa64LeState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa64BeState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBeWithRoleState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldLeWithRoleState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa8WithRoleState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa16LeWithRoleState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa16BeWithRoleState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa32LeWithRoleState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa32BeWithRoleState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa64LeWithRoleState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa64BeWithRoleState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBeSaveValState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldLeSaveValState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa8SaveValState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa16LeSaveValState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa16BeSaveValState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa32LeSaveValState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa32BeSaveValState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa64LeSaveValState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa64BeSaveValState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBeWithRoleSaveValState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldLeWithRoleSaveValState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa8WithRoleSaveValState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa16LeWithRoleSaveValState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa16BeWithRoleSaveValState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa32LeWithRoleSaveValState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa32BeWithRoleSaveValState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa64LeWithRoleSaveValState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa64BeWithRoleSaveValState();
    _StateHandlingReaction _handleReadFixedLenSIntFieldBeState();
    _StateHandlingReaction _handleReadFixedLenSIntFieldLeState();
    _StateHandlingReaction _handleReadFixedLenSIntFieldBa8State();
    _StateHandlingReaction _handleReadFixedLenSIntFieldBa16LeState();
    _StateHandlingReaction _handleReadFixedLenSIntFieldBa16BeState();
    _StateHandlingReaction _handleReadFixedLenSIntFieldBa32LeState();
    _StateHandlingReaction _handleReadFixedLenSIntFieldBa32BeState();
    _StateHandlingReaction _handleReadFixedLenSIntFieldBa64LeState();
    _StateHandlingReaction _handleReadFixedLenSIntFieldBa64BeState();
    _StateHandlingReaction _handleReadFixedLenSIntFieldBeSaveValState();
    _StateHandlingReaction _handleReadFixedLenSIntFieldLeSaveValState();
    _StateHandlingReaction _handleReadFixedLenSIntFieldBa8SaveValState();
    _StateHandlingReaction _handleReadFixedLenSIntFieldBa16LeSaveValState();
    _StateHandlingReaction _handleReadFixedLenSIntFieldBa16BeSaveValState();
    _StateHandlingReaction _handleReadFixedLenSIntFieldBa32LeSaveValState();
    _StateHandlingReaction _handleReadFixedLenSIntFieldBa32BeSaveValState();
    _StateHandlingReaction _handleReadFixedLenSIntFieldBa64LeSaveValState();
    _StateHandlingReaction _handleReadFixedLenSIntFieldBa64BeSaveValState();
    _StateHandlingReaction _handleReadFixedLenBitArrayFieldBeRevState();
    _StateHandlingReaction _handleReadFixedLenBitArrayFieldLeRevState();
    _StateHandlingReaction _handleReadFixedLenBitArrayFieldBa8RevState();
    _StateHandlingReaction _handleReadFixedLenBitArrayFieldBa16LeRevState();
    _StateHandlingReaction _handleReadFixedLenBitArrayFieldBa16BeRevState();
    _StateHandlingReaction _handleReadFixedLenBitArrayFieldBa32LeRevState();
    _StateHandlingReaction _handleReadFixedLenBitArrayFieldBa32BeRevState();
    _StateHandlingReaction _handleReadFixedLenBitArrayFieldBa64LeRevState();
    _StateHandlingReaction _handleReadFixedLenBitArrayFieldBa64BeRevState();
    _StateHandlingReaction _handleReadFixedLenBitMapFieldBeRevState();
    _StateHandlingReaction _handleReadFixedLenBitMapFieldLeRevState();
    _StateHandlingReaction _handleReadFixedLenBitMapFieldBa8RevState();
    _StateHandlingReaction _handleReadFixedLenBitMapFieldBa16LeRevState();
    _StateHandlingReaction _handleReadFixedLenBitMapFieldBa16BeRevState();
    _StateHandlingReaction _handleReadFixedLenBitMapFieldBa32LeRevState();
    _StateHandlingReaction _handleReadFixedLenBitMapFieldBa32BeRevState();
    _StateHandlingReaction _handleReadFixedLenBitMapFieldBa64LeRevState();
    _StateHandlingReaction _handleReadFixedLenBitMapFieldBa64BeRevState();
    _StateHandlingReaction _handleReadFixedLenBoolFieldBeRevState();
    _StateHandlingReaction _handleReadFixedLenBoolFieldLeRevState();
    _StateHandlingReaction _handleReadFixedLenBoolFieldBa8RevState();
    _StateHandlingReaction _handleReadFixedLenBoolFieldBa16LeRevState();
    _StateHandlingReaction _handleReadFixedLenBoolFieldBa16BeRevState();
    _StateHandlingReaction _handleReadFixedLenBoolFieldBa32LeRevState();
    _StateHandlingReaction _handleReadFixedLenBoolFieldBa32BeRevState();
    _StateHandlingReaction _handleReadFixedLenBoolFieldBa64LeRevState();
    _StateHandlingReaction _handleReadFixedLenBoolFieldBa64BeRevState();
    _StateHandlingReaction _handleReadFixedLenBoolFieldBeRevSaveValState();
    _StateHandlingReaction _handleReadFixedLenBoolFieldLeRevSaveValState();
    _StateHandlingReaction _handleReadFixedLenBoolFieldBa8RevSaveValState();
    _StateHandlingReaction _handleReadFixedLenBoolFieldBa16LeRevSaveValState();
    _StateHandlingReaction _handleReadFixedLenBoolFieldBa16BeRevSaveValState();
    _StateHandlingReaction _handleReadFixedLenBoolFieldBa32LeRevSaveValState();
    _StateHandlingReaction _handleReadFixedLenBoolFieldBa32BeRevSaveValState();
    _StateHandlingReaction _handleReadFixedLenBoolFieldBa64LeRevSaveValState();
    _StateHandlingReaction _handleReadFixedLenBoolFieldBa64BeRevSaveValState();
    _StateHandlingReaction _handleReadFixedLenFloatField32BeRevState();
    _StateHandlingReaction _handleReadFixedLenFloatField32LeRevState();
    _StateHandlingReaction _handleReadFixedLenFloatField64BeRevState();
    _StateHandlingReaction _handleReadFixedLenFloatField64LeRevState();
    _StateHandlingReaction _handleReadFixedLenFloatFieldBa32LeRevState();
    _StateHandlingReaction _handleReadFixedLenFloatFieldBa32BeRevState();
    _StateHandlingReaction _handleReadFixedLenFloatFieldBa64LeRevState();
    _StateHandlingReaction _handleReadFixedLenFloatFieldBa64BeRevState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBeRevState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldLeRevState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa8RevState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa16LeRevState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa16BeRevState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa32LeRevState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa32BeRevState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa64LeRevState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa64BeRevState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBeRevWithRoleState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldLeRevWithRoleState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa8RevWithRoleState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa16LeRevWithRoleState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa16BeRevWithRoleState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa32LeRevWithRoleState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa32BeRevWithRoleState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa64LeRevWithRoleState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa64BeRevWithRoleState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBeRevSaveValState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldLeRevSaveValState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa8RevSaveValState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa16LeRevSaveValState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa16BeRevSaveValState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa32LeRevSaveValState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa32BeRevSaveValState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa64LeRevSaveValState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa64BeRevSaveValState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBeRevWithRoleSaveValState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldLeRevWithRoleSaveValState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa8RevWithRoleSaveValState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa16LeRevWithRoleSaveValState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa16BeRevWithRoleSaveValState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa32LeRevWithRoleSaveValState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa32BeRevWithRoleSaveValState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa64LeRevWithRoleSaveValState();
    _StateHandlingReaction _handleReadFixedLenUIntFieldBa64BeRevWithRoleSaveValState();
    _StateHandlingReaction _handleReadFixedLenSIntFieldBeRevState();
    _StateHandlingReaction _handleReadFixedLenSIntFieldLeRevState();
    _StateHandlingReaction _handleReadFixedLenSIntFieldBa8RevState();
    _StateHandlingReaction _handleReadFixedLenSIntFieldBa16LeRevState();
    _StateHandlingReaction _handleReadFixedLenSIntFieldBa16BeRevState();
    _StateHandlingReaction _handleReadFixedLenSIntFieldBa32LeRevState();
    _StateHandlingReaction _handleReadFixedLenSIntFieldBa32BeRevState();
    _StateHandlingReaction _handleReadFixedLenSIntFieldBa64LeRevState();
    _StateHandlingReaction _handleReadFixedLenSIntFieldBa64BeRevState();
    _StateHandlingReaction _handleReadFixedLenSIntFieldBeRevSaveValState();
    _StateHandlingReaction _handleReadFixedLenSIntFieldLeRevSaveValState();
    _StateHandlingReaction _handleReadFixedLenSIntFieldBa8RevSaveValState();
    _StateHandlingReaction _handleReadFixedLenSIntFieldBa16LeRevSaveValState();
    _StateHandlingReaction _handleReadFixedLenSIntFieldBa16BeRevSaveValState();
    _StateHandlingReaction _handleReadFixedLenSIntFieldBa32LeRevSaveValState();
    _StateHandlingReaction _handleReadFixedLenSIntFieldBa32BeRevSaveValState();
    _StateHandlingReaction _handleReadFixedLenSIntFieldBa64LeRevSaveValState();
    _StateHandlingReaction _handleReadFixedLenSIntFieldBa64BeRevSaveValState();
    _StateHandlingReaction _handleReadVarLenUIntFieldState();
    _StateHandlingReaction _handleReadVarLenUIntFieldWithRoleState();
    _StateHandlingReaction _handleReadVarLenUIntFieldSaveValState();
    _StateHandlingReaction _handleReadVarLenUIntFieldWithRoleSaveValState();
    _StateHandlingReaction _handleReadVarLenSIntFieldState();
    _StateHandlingReaction _handleReadVarLenSIntFieldSaveValState();
    _StateHandlingReaction _handleReadFixedLenMetadataStreamUuidByteUIntFieldBa8State();

    /* Helpers for state handlers */
    _StateHandlingReaction _handleCommonBeginReadScopeState(Scope scope);
    _StateHandlingReaction _handleCommonEndReadScopeState(Scope scope);
    void _handleCommonBeginReadStructFieldState();
    _StateHandlingReaction _handleCommonBeginReadArrayFieldState(unsigned long long len,
                                                                 const ArrayFc& arrayFc);
    /*
     * Validates the length `len` (bytes) of the current dynamic-length
     * string/BLOB field, returning it as a data length.
     */
    bt2c::DataLen _validatedDynLenStrBlobFieldLen(unsigned long long len);

    _StateHandlingReaction _handleCommonBeginReadStrBlobFieldState(unsigned long long len,
                                                                   _State contentState,
                                                                   const Fc& fc);
    _StateHandlingReaction _handleCommonBeginReadStaticLenBlobFieldState(_State contentState);
    void _handleCommonReadRawDataNoNextState();
    void _handleCommonBeginReadNullTerminatedStrFieldState(_State dataState);
    void _handleCommonAfterCommonEventRecordCtxScopeState();
    void _handleCommonAfterSpecEventRecordCtxScopeState();

    /*
     * Common compound field reading end state handler using `item`.
     */
    template <typename ItemT>
    _StateHandlingReaction _handleCommonEndReadCompoundFieldState(ItemT& item)
    {
        /* Update for user */
        BT_ASSERT_DBG(this->_stackTop().fc);
        this->_setFieldItemFcAndUpdateForUser(item, *this->_stackTop().fc);

        /* Current stack frame isn't needed anymore */
        this->_stackPop();

        /* Next: read next field */
        this->_prepareToReadNextField();
        return _StateHandlingReaction::Stop;
    }

    template <typename NullCpFinderT>
    void _handleCommonBeginReadNullTerminatedStrFieldState(NullCpFinderT& nullCpFinder,
                                                           const _State dataState)
    {
        /* Update for user */
        this->_setFieldItemFcAndUpdateForUser(_mItems.nullTerminatedStrFieldBegin,
                                              *this->_stackTop().fc);

        /* Align head for string field */
        this->_alignHead(*this->_stackTop().fc);

        /* Reset null-terminated string code unit buffer */
        nullCpFinder = NullCpFinderT {};

        /* Next: read substring until (and including) a null character */
        this->_state(dataState);
    }

    /*
     * Reads the next buffer bytes until it finds the U+0000 codepoint
     * using the null codepoint finder `nullCpFinder` (one of the
     * `_mUtf*NullCpFinder` members).
     */
    template <typename NullCpFinderT>
    _StateHandlingReaction
    _handleCommonReadSubstrUntilNullCodepointState(NullCpFinderT& nullCpFinder)
    {
        using namespace bt2c::literals::datalen;

        BT_ASSERT_DBG(!_mHeadOffsetInCurPkt.hasExtraBits());

        /* Require at least one byte of packet content */
        this->_requireContentData(1_bytes);

        const auto bufLen = this->_remainingBufLen();

        BT_ASSERT_DBG(bufLen >= 1_bytes);

        /* Find any null character within the current buffer */
        const auto begin = this->_bufAtHead();
        auto end = begin + bufLen.bytes();
        auto foundNullCodepoint = false;

        /* Try to find a first U+0000 codepoint */
        if (const auto afterNullCpIt = nullCpFinder.findNullCp(bt2c::ConstBytes {begin, end})) {
            foundNullCodepoint = true;
            end = &(**afterNullCpIt);
        }

        /*
         * Make sure the string data is completely part of the
         * packet content.
         */
        {
            /* Can't overflow: the string data is within the current buffer */
            if (const auto strDataLen = bt2c::DataLen::fromBytesDbg(end - begin);
                strDataLen > this->_remainingPktContentLen()) {
                CTF_SRC_ITEM_SEQ_ITER_CPPLOGE_APPEND_CAUSE_AND_THROW(
                    "{} null-terminated string field bytes required at this point, "
                    "but only {} bits of packet content remain.",
                    strDataLen.bytes(), *this->_remainingPktContentLen());
            }
        }

        /* Update for user */
        _mItems.rawData._assign(begin, end);
        BT_ASSERT_DBG(_mItems.rawData.len() >= 1_bytes);
        this->_updateForUser(_mItems.rawData);

        /* Mark the string data as consumed */
        this->_consumeAvailData(_mItems.rawData.len());

        /* End found yet? */
        if (foundNullCodepoint) {
            /* Next: end reading null-terminated string field */
            this->_restoreState();
        }

        return _StateHandlingReaction::Stop;
    }

    /*
     * Common variant field (of concrete class `VarFcT`) reading
     * beginning state handler using `item`.
     */
    template <typename VarFcT, typename ItemT>
    _StateHandlingReaction _handleCommonBeginReadVariantField(ItemT& item)
    {
        auto& varFc = static_cast<const VarFcT&>(*this->_stackTop().fc);

        /* A variant field always contains a single field */
        this->_stackTop().len = 1;

        /* Update for user */
        this->_setFieldItemFcAndUpdateForUser(item, varFc);

        /* Find selected option */
        BT_ASSERT_DBG(varFc.savedKeyValIndex());
        item._mSelVal = this->_savedKeyVal<typename VarFcT::SelVal>(*varFc.savedKeyValIndex());

        const auto optIt = varFc.findOptBySelVal(item._mSelVal);

        if (optIt == varFc.end()) {
            CTF_SRC_ITEM_SEQ_ITER_CPPLOGE_APPEND_CAUSE_AND_THROW(
                "no variant field option selected by the selector value {}.", item._mSelVal);
        }

        item._mSelectedOptIndex = optIt - varFc.begin();

        /* Next: read the selected field */
        this->_prepareToReadField(optIt->fc());
        return _StateHandlingReaction::Stop;
    }

    /*
     * Common optional field (of concrete class `OptFcT`) reading
     * beginning state handler using `item`.
     */
    template <typename OptFcT, typename ItemT>
    _StateHandlingReaction _handleCommonBeginReadOptionalField(ItemT& item)
    {
        auto& optFc = static_cast<const OptFcT&>(*this->_stackTop().fc);

        /* Update for user */
        this->_setFieldItemFcAndUpdateForUser(item, optFc);

        /* Check whether or not the optional field is enabled */
        BT_ASSERT_DBG(optFc.savedKeyValIndex());
        item._mSelVal = this->_savedKeyVal<typename OptFcT::SelVal>(*optFc.savedKeyValIndex());
        item._mIsEnabled = optFc.isEnabledBySelVal(item._mSelVal);

        if (item._mIsEnabled) {
            /* Next: read the real field */
            this->_stackTop().len = 1;
            this->_prepareToReadField(optFc.fc());
        } else {
            /*
             * Next: end reading optional field.
             *
             * The handler of the restored state needs
             * `this->_stackTop().fc` and therefore calls _stackPop()
             * itself.
             */
            this->_restoreState();
        }

        return _StateHandlingReaction::Stop;
    }

    static constexpr const char *_byteOrderStr(const ByteOrder byteOrder) noexcept
    {
        return byteOrder == ByteOrder::Big ? "big-endian" : "little-endian";
    }

    /*
     * If `*iter._mHeadOffsetInCurPkt` is not a multiple of 8 and
     * `fc.byteOrder` and `iter._mLastFixedLenBitArrayFieldByteOrder`
     * aren't compatible, this method throws `DecodingError`.
     */
    void _checkLastFixedLenBitArrayFieldByteOrder(const FixedLenBitArrayFc& fc) const
    {
        if (_mHeadOffsetInCurPkt.hasExtraBits() && _mLastFixedLenBitArrayFieldByteOrder &&
            fc.byteOrder() != *_mLastFixedLenBitArrayFieldByteOrder) {
            /*
             * A fixed-length bit array field which doesn't start on a
             * byte boundary must have the same byte order as the
             * previous fixed-length bit array field.
             */
            CTF_SRC_ITEM_SEQ_ITER_CPPLOGE_APPEND_CAUSE_AND_THROW(
                "two contiguous fixed-length bit array fields which aren't "
                "byte-aligned don't share the same byte order: {} followed with {}.",
                ItemSeqIter::_byteOrderStr(*_mLastFixedLenBitArrayFieldByteOrder),
                ItemSeqIter::_byteOrderStr(fc.byteOrder()));
        }
    }

    /*
     * Reads a fixed-length integer field.
     *
     * `LenBitsV` is either:
     *
     * 8, 16, 32, or 64:
     *     Length, in bits, of the integer field to field to read.
     *
     *     `fc.align()` must be greater than or equal to 8.
     *
     *     `fc.len()` must be equal to `LenBitsV`.
     *
     * 0:
     *     Use `fc.len()`.
     *
     * `ByteOrderV` is the byte order of the integer field to read. It
     * must be equal to `fc.byteOrder()`.
     *
     * `BitOrder` is whether or not the bits of the field are reversed
     * (unnatural).
     *
     * Checks and updates `_mLastFixedLenBitArrayFieldByteOrder` if
     * needed.
     *
     * Marks the fixed-length integer field bits as consumed.
     *
     * Returns the decoded value, of type `unsigned long long` or `long
     * long`.
     */
    template <bt2c::Signedness SignednessV, std::size_t LenBitsV, ByteOrder ByteOrderV,
              internal::BitOrder BitOrderV>
    auto _readFixedLenIntField(const FixedLenBitArrayFc& fc)
    {
        static_assert(LenBitsV == 0 || LenBitsV == 8 || LenBitsV == 16 || LenBitsV == 32 ||
                          LenBitsV == 64,
                      "`LenBitsV` is 0, 8, 16, 32, or 64.");

        BT_ASSERT_DBG(LenBitsV == 0 || *fc.len() == LenBitsV);
        BT_ASSERT_DBG(LenBitsV == 0 || fc.align() >= 8);
        BT_ASSERT_DBG((*fc.len() == 8 && fc.align() >= 8) || fc.byteOrder() == ByteOrderV);

        /* Align head for fixed-length bit array field */
        this->_alignHead(fc);

        /*
         * Require enough packet content to read the whole fixed-length
         * bit array field.
         */
        this->_requireContentData(fc.len());

        /* Read the field */
        const auto val =
            internal::readFixedLenInt<SignednessV, LenBitsV, ByteOrderV, BitOrderV>(*this, fc);

        /* Set last fixed-length bit array field byte order */
        _mLastFixedLenBitArrayFieldByteOrder = fc.byteOrder();

        /* Mark the fixed-length bit array field as consumed */
        this->_consumeAvailData(fc.len());

        /* Return decoded value */
        return val;
    }

    /*
     * Common fixed-length integer field state handler using `item`.
     *
     * This method doesn't update the _value_ of `item`.
     *
     * The `SignednessV`, `LenBitsV`, `ByteOrderV`, and `BitOrderV`
     * template parameters are the same as for the
     * _readFixedLenIntField() method template.
     *
     * Returns the decoded value, of type `unsigned long long` or `long
     * long`.
     */
    template <bt2c::Signedness SignednessV, std::size_t LenBitsV, ByteOrder ByteOrderV,
              internal::BitOrder BitOrderV, typename ItemT>
    auto _handleCommonReadFixedLenIntFieldState(ItemT& item)
    {
        /* Read the fixed-length integer field */
        const auto val = this->_readFixedLenIntField<SignednessV, LenBitsV, ByteOrderV, BitOrderV>(
            _mCurScalarFc->asFixedLenBitArray());

        /* Update for user */
        this->_setFieldItemFcAndUpdateForUser(item, *_mCurScalarFc);

        /* Return decoded value */
        return val;
    }

    /*
     * Same as the one above, but also prepares to read the next field.
     */
    template <bt2c::Signedness SignednessV, std::size_t LenBitsV, ByteOrder ByteOrderV,
              internal::BitOrder BitOrderV, typename ItemT>
    auto _handleCommonReadFixedLenIntFieldStateAndPrepareToReadNextField(ItemT& item)
    {
        const auto val = this->_handleCommonReadFixedLenIntFieldState<SignednessV, LenBitsV,
                                                                      ByteOrderV, BitOrderV>(item);

        /* Next: read next field */
        this->_prepareToReadNextField();

        /* Return decoded value */
        return val;
    }

    /*
     * Handles a single unsigned integer field role `role` using the
     * decoded unsigned integer value `val` having the length `len`.
     *
     * This method may change the current state to
     * `_State::SetPktMagicNumberItem` or
     * `_State::SetDefClkValItem`.
     */
    void _handleUIntFieldRole(const UIntFieldRole role, const bt2c::DataLen len,
                              const unsigned long long val)
    {
        switch (role) {
        case UIntFieldRole::PktMagicNumber:
            /* Update for user */
            _mItems.pktMagicNumber._mVal = val;

            /* Next: set packet magic number item */
            this->_state(_State::SetPktMagicNumberItem);
            break;
        case UIntFieldRole::DataStreamClsId:
        case UIntFieldRole::EventRecordClsId:
            _mCurClsId = val;
            break;
        case UIntFieldRole::DataStreamId:
            _mItems.dataStreamInfo._mId = val;
            break;
        case UIntFieldRole::PktTotalLen:
        {
            const auto totalLen = bt2c::DataLen::fromBits(val);

            if (totalLen.hasExtraBits()) {
                CTF_SRC_ITEM_SEQ_ITER_CPPLOGE_APPEND_CAUSE_AND_THROW(
                    "expected total length of current packet ({} bits) isn't a multiple of 8 bits.",
                    *totalLen);
            }

            _mCurPktExpectedLens.total = totalLen;
            _mItems.pktInfo._mExpectedTotalLen = totalLen;
            break;
        }
        case UIntFieldRole::PktContentLen:
            _mCurPktExpectedLens.content = bt2c::DataLen::fromBits(val);
            _mItems.pktInfo._mExpectedContentLen = _mCurPktExpectedLens.content;
            break;
        case UIntFieldRole::DefClkTs:
            /* Update clock value */
            this->_updateDefClkVal(val, len);

            /* Update for user */
            _mItems.defClkVal._mCycles = _mDefClkVal;

            /* Next: set default clock value item */
            this->_state(_State::SetDefClkValItem);
            break;
        case UIntFieldRole::PktEndDefClkTs:
            _mItems.pktInfo._mEndDefClkVal = val;
            break;
        case UIntFieldRole::DiscEventRecordCounterSnap:
            _mItems.pktInfo._mDiscErCounterSnap = val;
            break;
        case UIntFieldRole::PktSeqNum:
            _mItems.pktInfo._mSeqNum = val;
            break;
        default:
            bt_common_abort();
        }
    }

    /*
     * Returns the length of a current instance of `fc`.
     */
    bt2c::DataLen _uIntFieldLen(const FixedLenBitArrayFc& fc) const noexcept
    {
        return fc.len();
    }

    /*
     * Returns the length of the current variable-length unsigned
     * integer instance.
     */
    bt2c::DataLen _uIntFieldLen(const VarLenUIntFc&) const noexcept
    {
        /* Variable-length integer field length is dynamic */
        return _mCurVarLenInt.len;
    }

    /*
     * Whether or not to save a decoded field value.
     */
    enum class _SaveVal
    {
        Yes,
        No,
    };

    /*
     * Whether or not a decoded field value has at least one role.
     */
    enum class _WithRole
    {
        Yes,
        No,
    };

    /*
     * Common unsigned integer field state handler
     *
     * `WithRoleV` indicates whether or not the unsigned integer field
     * has at least one role.
     *
     * `SaveValV` indicates whether or not to save the unsigned integer
     * field value.
     */
    template <typename FcT, _WithRole WithRoleV, _SaveVal SaveValV>
    _StateHandlingReaction _handleCommonUIntFieldState(const unsigned long long val)
    {
        BT_ASSERT_DBG(_mCurScalarFc);

        auto& intFc = static_cast<const FcT&>(*_mCurScalarFc);

        /*
         * May be a length/selector of some upcoming dynamic length,
         * optional, or variant field.
         */
        if constexpr (SaveValV == _SaveVal::Yes) {
            this->_saveKeyVal(intFc.keyValSavingIndexes(), val);
        }

        /* Role? */
        if constexpr (WithRoleV == _WithRole::Yes) {
            /* Keep the current state to detect a change */
            const auto prevState = _mState;

            /* Handle each role */
            for (auto& role : intFc.roles()) {
                this->_handleUIntFieldRole(role, this->_uIntFieldLen(intFc), val);
            }

            /*
             * _handleUIntFieldRole() may change the state to
             * `_State::SetPktMagicNumberItem` or
             * `_State::SetDefClkValItem`.
             */
            if (_mState == prevState) {
                /* State didn't change; next: read next field */
                this->_prepareToReadNextField();
            }
        } else {
            /* Next: read next field */
            this->_prepareToReadNextField();
        }

        return _StateHandlingReaction::Stop;
    }

    /*
     * Common fixed-length unsigned integer field state handler.
     *
     * The `LenBitsV`, `ByteOrderV`, and `BitOrderV` template parameters
     * are the same as for the _handleCommonReadFixedLenIntFieldState()
     * method template.
     *
     * `WithRoleV` indicates whether or not the unsigned integer field
     * has at least one role.
     *
     * `SaveValV` indicates whether or not to save the unsigned integer
     * field value.
     */
    template <typename FcT, std::size_t LenBitsV, ByteOrder ByteOrderV,
              internal::BitOrder BitOrderV, _WithRole WithRoleV, _SaveVal SaveValV>
    _StateHandlingReaction _handleCommonReadFixedLenUIntFieldState()
    {
        /* Decode the unsigned integer value */
        const auto val = this->_handleCommonReadFixedLenIntFieldState<
            bt2c::Signedness::Unsigned, LenBitsV, ByteOrderV, BitOrderV>(_mItems.fixedLenUIntField);

        /* Update for user */
        _mItems.fixedLenUIntField._val(val);

        /*
         * Handle role and value saving.
         *
         * This call prepares to read the next field if needed.
         */
        return this->_handleCommonUIntFieldState<FcT, WithRoleV, SaveValV>(val);
    }

    /*
     * Common fixed-length signed integer field state handler.
     *
     * The `LenBitsV`, `ByteOrderV`, and `BitOrderV` template parameters
     * are the same as for the _handleCommonReadFixedLenIntFieldState()
     * method template.
     *
     * `SaveValV` indicates whether or not to save the signed integer
     * field value.
     */
    template <typename FcT, std::size_t LenBitsV, ByteOrder ByteOrderV,
              internal::BitOrder BitOrderV, _SaveVal SaveValV>
    _StateHandlingReaction _handleCommonReadFixedLenSIntFieldState()
    {
        /*
         * Capture the current field class before
         * _handleCommonReadFixedLenIntFieldStateAndPrepareToReadNextField()
         * possibly overwrites `_mCurScalarFc` while preparing the
         * next state.
         */
        const auto& sIntFc = static_cast<const FcT&>(*_mCurScalarFc);

        /* Decode the signed integer value */
        const auto val = this->_handleCommonReadFixedLenIntFieldStateAndPrepareToReadNextField<
            bt2c::Signedness::Signed, LenBitsV, ByteOrderV, BitOrderV>(_mItems.fixedLenSIntField);

        /* Update for user */
        _mItems.fixedLenSIntField._val(val);

        /* May be a selector of some upcoming optional/variant field */
        if constexpr (SaveValV == _SaveVal::Yes) {
            this->_saveKeyVal(sIntFc.keyValSavingIndexes(), val);
        }

        return _StateHandlingReaction::Stop;
    }

    /*
     * Common fixed-length boolean field state handler.
     *
     * The `LenBitsV`, `ByteOrderV`, `BitOrderV`, and `ItemT` template
     * parameters are the same as for the
     * _handleCommonReadFixedLenIntFieldState() method template.
     *
     * `SaveValV` indicates whether or not to save the boolean field
     * value.
     */
    template <std::size_t LenBitsV, ByteOrder ByteOrderV, internal::BitOrder BitOrderV,
              _SaveVal SaveValV, typename ItemT>
    _StateHandlingReaction _handleCommonReadFixedLenBoolFieldState(ItemT& item)
    {
        /*
         * Capture the current field class before
         * _handleCommonReadFixedLenIntFieldStateAndPrepareToReadNextField()
         * possibly overwrites `_mCurScalarFc` while preparing the
         * next state.
         */
        const auto& boolFc = _mCurScalarFc->asFixedLenBool();

        /* Decode the boolean value as an unsigned integer */
        const auto val = this->_handleCommonReadFixedLenIntFieldStateAndPrepareToReadNextField<
            bt2c::Signedness::Unsigned, LenBitsV, ByteOrderV, BitOrderV>(item);

        /* Update for user */
        item._val(val);

        /* May be a selector of some upcoming optional field */
        if constexpr (SaveValV == _SaveVal::Yes) {
            this->_saveKeyVal(boolFc.keyValSavingIndexes(), val);
        }

        return _StateHandlingReaction::Stop;
    }

    /*
     * Common fixed-length floating-point number field state handler.
     *
     * The `LenBitsV`, `ByteOrderV`, and `BitOrderV` template parameters
     * are the same as for the _handleCommonReadFixedLenIntFieldState()
     * method template.
     *
     * `FloatT` must be either `float` or `double`.
     */
    template <std::size_t LenBitsV, ByteOrder ByteOrderV, internal::BitOrder BitOrderV,
              typename FloatT>
    _StateHandlingReaction _handleCommonReadFixedLenFloatFieldState()
    {
        static_assert(std::is_same_v<FloatT, float> || std::is_same_v<FloatT, double>,
                      "`FloatT` is `float` or `double`.");

        /* Decode the floating-point number value as an unsigned integer */
        const auto val = this->_handleCommonReadFixedLenIntFieldStateAndPrepareToReadNextField<
            bt2c::Signedness::Unsigned, LenBitsV, ByteOrderV, BitOrderV>(
            _mItems.fixedLenFloatField);

        /* Update for user */
        {
            /* Check IEEE-754 binary compatibility */
            using UIntT =
                std::conditional_t<std::is_same_v<FloatT, float>, std::uint32_t, std::uint64_t>;

            static_assert(std::numeric_limits<FloatT>::is_iec559,
                          "`FloatT` fulfills the requirements of IEC 559.");
            static_assert(sizeof(FloatT) == sizeof(UIntT),
                          "Size of `FloatT` and of its equivalent integral type match in union.");
            static_assert(
                std::alignment_of_v<FloatT> == std::alignment_of_v<UIntT>,
                "Alignment of `FloatT` and of its equivalent integral type match in union.");

            /* Convert unsigned integer value to floating-point number value */
            union
            {
                FloatT f;
                UIntT u;
            } u;

            u.u = static_cast<UIntT>(val);

            /* Update for user */
            _mItems.fixedLenFloatField._val(static_cast<double>(u.f));
        }

        return _StateHandlingReaction::Stop;
    }

    /*
     * Common fixed-length bit array field state handler.
     *
     * The `LenBitsV`, `ByteOrderV`, `BitOrderV`, and `ItemT` template
     * parameters are the same as for the
     * _handleCommonReadFixedLenIntFieldState() method template.
     */
    template <std::size_t LenBitsV, ByteOrder ByteOrderV, internal::BitOrder BitOrderV,
              typename ItemT>
    _StateHandlingReaction _handleCommonReadFixedLenBitArrayFieldState(ItemT& item)
    {
        /* Read bit array value as an unsigned integer */
        const auto val = this->_handleCommonReadFixedLenIntFieldStateAndPrepareToReadNextField<
            bt2c::Signedness::Unsigned, LenBitsV, ByteOrderV, BitOrderV>(item);

        /* Update for user */
        item._val(val);

        return _StateHandlingReaction::Stop;
    }

    /*
     * Appends the single LEB128 byte `byte` to `_mCurVarLenInt.val` and
     * updates `_mCurVarLenInt.len` accordingly.
     */
    template <bt2c::Signedness SignednessV>
    void _appendVarLenIntByte(const std::uint8_t byte)
    {
        using namespace bt2c::literals::datalen;

        /* Can't overflow: `_mCurVarLenInt.len` is at most 64 bits */
        auto newVarLenIntLen = _mCurVarLenInt.len.addDbg(7_bits);
        const auto byteVal = byte & 0x7f;

        /* Validate upcoming variable-length integer length */
        if (newVarLenIntLen > 63_bits) {
            /*
             * We make an exception for some 10th byte which can contain
             * the MSB of a 64-bit integer (as 9 × 7 is 63).
             *
             * The condition to accept it is:
             *
             * * It's the last byte of the variable-length integer.
             *
             * * If `SignednessV` is `bt2c::Signedness::Unsigned`:
             *       Its 7-bit value (`byteVal`) must be 1.
             *
             *   If `SignednessV` is `bt2c::Signedness::Signed`:
             *       Its 7-bit value must be 0 (positive) or 127
             *       (negative).
             */
            const auto isLastByte = (byte & 0x80) == 0;
            const auto hasValidVal = std::invoke([byteVal] {
                if constexpr (SignednessV == bt2c::Signedness::Signed) {
                    return byteVal == 0 || byteVal == 0x7f;
                } else {
                    return byteVal == 1;
                }
            });

            if (!isLastByte || !hasValidVal) {
                CTF_SRC_ITEM_SEQ_ITER_CPPLOGE_APPEND_CAUSE_AND_THROW(
                    "unsupported oversized (more than 64 bits of data) variable-length integer field.");
            }

            newVarLenIntLen = 64_bits;
        }

        /* Mark this byte as consumed immediately */
        this->_consumeAvailData(8_bits);

        /* Update unsigned integer value, clearing the continuation bit */
        _mCurVarLenInt.val |= (static_cast<unsigned long long>(byteVal) << *_mCurVarLenInt.len);

        /* Update current variable-length integer length */
        _mCurVarLenInt.len = newVarLenIntLen;
    }

    /*
     * Common variable-length unsigned integer field state handler
     * updating `item`.
     *
     * Does NOT prepare to read the next field.
     */
    template <bt2c::Signedness SignednessV, typename ItemT>
    void _handleCommonVarLenIntFieldState(ItemT& item)
    {
        BT_ASSERT_DBG(_mCurScalarFc);
        this->_alignHead(*_mCurScalarFc);

        while (true) {
            /*
             * Read a single byte, and then:
             *
             * If the variable-length integer is not ended:
             *     Continue.
             *
             * Otherwise:
             *     Set the variable-length integer element.
             *
             *     The caller is responsible for preparing or not to
             *     read the next field.
             *
             * See <https://en.wikipedia.org/wiki/LEB128>.
             */
            BT_ASSERT_DBG(!_mHeadOffsetInCurPkt.hasExtraBits());

            /* Require at least one byte */
            this->_requireContentData(bt2c::DataLen::fromBits(8));

            /* Read current byte */
            const auto byte = *this->_bufAtHead();

            if ((byte & 0x80) == 0) {
                /* This is the last byte */
                this->_appendVarLenIntByte<SignednessV>(byte);

                /*
                 * Update for user.
                 *
                 * `_headOffsetInItemSeq()` now returns the offset at
                 * the _end_ of the variable-length integer; the
                 * iterator user expects its beginning offset.
                 */
                item._val(internal::varLenIntFieldVal<SignednessV>(_mCurVarLenInt.len,
                                                                   _mCurVarLenInt.val));
                item._mLen = _mCurVarLenInt.len;
                this->_setFieldItemFc(item, *_mCurScalarFc);
                this->_updateForUser(item, this->_headOffsetInItemSeq().subDbg(item.fieldLen()));

                /*
                 * Any state handler which calls this method template
                 * may be reentered as is. This may happen if the
                 * _requireContentData() call above throws
                 * `bt2c::TryAgain`, for example.
                 *
                 * This means there's no initial setup to read a
                 * variable-length integer field: the state handlers
                 * just call this method template to start _and_ to
                 * continue.
                 *
                 * Because of this, and because both
                 * `_mCurVarLenInt.len` and `_mCurVarLenInt.val` must be
                 * zero before starting to read a variable-length
                 * integer field, we reset them here for the next
                 * variable-length integer field reading operation.
                 *
                 * _resetForNewPkt() also resets both variables before
                 * starting to read a packet.
                 */
                _mCurVarLenInt.val = 0;
                _mCurVarLenInt.len = bt2c::DataLen::fromBits(0);

                /* Done */
                return;
            }

            /* Not the last byte */
            this->_appendVarLenIntByte<SignednessV>(byte);
        }
    }

    /*
     * Common variable-length unsigned integer field state handler.
     *
     * `WithRoleV` indicates whether or not the unsigned integer
     * field has at least one role.
     *
     * `SaveValV` indicates whether or not to save the unsigned integer
     * field value.
     */
    template <typename FcT, _WithRole WithRoleV, _SaveVal SaveValV, typename ItemT>
    _StateHandlingReaction _handleCommonReadVarLenUIntFieldState(ItemT& item)
    {
        /* This call sets the value of `item` */
        this->_handleCommonVarLenIntFieldState<bt2c::Signedness::Unsigned>(item);

        /*
         * Handle role and value saving.
         *
         * We can't use `_mCurVarLenInt.val` here because the
         * successful _handleCommonVarLenIntFieldState() call above
         * reset it.
         *
         * This call prepares to read the next field if needed.
         */
        return this->_handleCommonUIntFieldState<FcT, WithRoleV, SaveValV>(item.val());
    }

    /*
     * Common variable-length signed integer field state handler.
     *
     * `SaveValV` indicates whether or not to save the signed integer
     * field value.
     */
    template <typename VarLenFcT, _SaveVal SaveValV, typename ItemT>
    _StateHandlingReaction _handleCommonReadVarLenSIntFieldState(ItemT& item)
    {
        /* This call sets the value of `item` */
        this->_handleCommonVarLenIntFieldState<bt2c::Signedness::Signed>(item);

        /* May be a selector of some upcoming optional/variant field */
        if constexpr (SaveValV == _SaveVal::Yes) {
            /*
             * We can't use `_mCurVarLenInt.val` here because the
             * successful _handleCommonVarLenIntFieldState() call above
             * reset it.
             */
            this->_saveKeyVal(static_cast<const VarLenFcT&>(*_mCurScalarFc).keyValSavingIndexes(),
                              item.val());
        }

        /* Next: read next field */
        this->_prepareToReadNextField();

        /* Done */
        return _StateHandlingReaction::Stop;
    }

    /*
     * This acts as an "infinite" data length value.
     *
     * We make it a multiple of eight bits so that, when there's no
     * expected total packet length, the check in
     * _handleSetPktInfoItemState() passes.
     */
    static constexpr bt2c::DataLen _infDataLen() noexcept
    {
        return bt2c::DataLen::fromBits(std::numeric_limits<unsigned long long>::max() & ~7ULL);
    }

    /*
     * Maximum end offset (bits) of a packet or of a medium buffer
     * within the item sequence.
     *
     * This leaves `maxAlign` bits of headroom so that aligning any
     * decoding head offset (at most this value) never overflows.
     */
    static constexpr unsigned long long _maxEndOffsetInItemSeq =
        std::numeric_limits<unsigned long long>::max() - maxAlign + 1;

    /*
     * Observer notified when the number of saved key values required by
     * `*_mTraceCls` changes.
     */
    void _savedKeyValCountUpdated(const size_t savedKeyValCount)
    {
        _mSavedKeyVals.resize(savedKeyValCount);
    }

    /* Underlying medium to request data from */
    Medium::UP _mMedium;

    /* Trace class */
    const TraceCls *_mTraceCls;

    /*
     * Token of the _savedKeyValCountUpdated() observer.
     */
    TraceCls::SavedKeyValCountUpdatedObservable::Token
        _mTraceClsSavedKeyValCountUpdatedObservableToken;

    /* Current state */
    _State _mState = _State::Init;

    /* State to restore after having skipped padding bits */
    _State _mPostSkipPaddingState;

    /* Current data buffer */
    Buf _mBuf;

    /* Offset of current data buffer within current packet */
    bt2c::DataLen _mBufOffsetInCurPkt = bt2c::DataLen::fromBits(0);

    /* Offset of current packet beginning within its item sequence */
    bt2c::DataLen _mCurPktOffsetInItemSeq = bt2c::DataLen::fromBits(0);

    /* Head offset within current packet */
    bt2c::DataLen _mHeadOffsetInCurPkt = bt2c::DataLen::fromBits(0);

    /* Current user-visible item offset within the item sequence */
    bt2c::DataLen _mCurItemOffsetInItemSeq = bt2c::DataLen::fromBits(0);

    /* Current item */
    const Item *_mCurItem = nullptr;

    /* Item instances */
    struct
    {
        PktBeginItem pktBegin;
        PktEndItem pktEnd;
        ScopeBeginItem scopeBegin;
        ScopeEndItem scopeEnd;
        PktContentBeginItem pktContentBegin;
        PktContentEndItem pktContentEnd;
        EventRecordBeginItem eventRecordBegin;
        EventRecordEndItem eventRecordEnd;
        PktMagicNumberItem pktMagicNumber;
        MetadataStreamUuidItem metadataStreamUuid;
        DataStreamInfoItem dataStreamInfo;
        PktInfoItem pktInfo;
        EventRecordInfoItem eventRecordInfo;
        DefClkValItem defClkVal;
        FixedLenBitArrayFieldItem fixedLenBitArrayField;
        FixedLenBitMapFieldItem fixedLenBitMapField;
        FixedLenBoolFieldItem fixedLenBoolField;
        FixedLenSIntFieldItem fixedLenSIntField;
        FixedLenUIntFieldItem fixedLenUIntField;
        FixedLenFloatFieldItem fixedLenFloatField;
        VarLenSIntFieldItem varLenSIntField;
        VarLenUIntFieldItem varLenUIntField;
        NullTerminatedStrFieldBeginItem nullTerminatedStrFieldBegin;
        NullTerminatedStrFieldEndItem nullTerminatedStrFieldEnd;
        RawDataItem rawData;
        StaticLenArrayFieldBeginItem staticLenArrayFieldBegin;
        StaticLenArrayFieldEndItem staticLenArrayFieldEnd;
        DynLenArrayFieldBeginItem dynLenArrayFieldBegin;
        DynLenArrayFieldEndItem dynLenArrayFieldEnd;
        StaticLenStrFieldBeginItem staticLenStrFieldBegin;
        StaticLenStrFieldEndItem staticLenStrFieldEnd;
        DynLenStrFieldBeginItem dynLenStrFieldBegin;
        DynLenStrFieldEndItem dynLenStrFieldEnd;
        StaticLenBlobFieldBeginItem staticLenBlobFieldBegin;
        StaticLenBlobFieldEndItem staticLenBlobFieldEnd;
        DynLenBlobFieldBeginItem dynLenBlobFieldBegin;
        DynLenBlobFieldEndItem dynLenBlobFieldEnd;
        StructFieldBeginItem structFieldBegin;
        StructFieldEndItem structFieldEnd;
        VariantFieldWithSIntSelBeginItem variantFieldWithSIntSelBegin;
        VariantFieldWithSIntSelEndItem variantFieldWithSIntSelEnd;
        VariantFieldWithUIntSelBeginItem variantFieldWithUIntSelBegin;
        VariantFieldWithUIntSelEndItem variantFieldWithUIntSelEnd;
        OptionalFieldWithBoolSelBeginItem optionalFieldWithBoolSelBegin;
        OptionalFieldWithBoolSelEndItem optionalFieldWithBoolSelEnd;
        OptionalFieldWithSIntSelBeginItem optionalFieldWithSIntSelBegin;
        OptionalFieldWithSIntSelEndItem optionalFieldWithSIntSelEnd;
        OptionalFieldWithUIntSelBeginItem optionalFieldWithUIntSelBegin;
        OptionalFieldWithUIntSelEndItem optionalFieldWithUIntSelEnd;
    } _mItems;

    /* Last fixed-length bit array field byte order */
    std::optional<ByteOrder> _mLastFixedLenBitArrayFieldByteOrder;

    /*
     * Head offset within current packet at the beginning of the current
     * event record.
     *
     * This is used to detect an empty event record which is invalid
     * because it inhibits decoding progress.
     */
    bt2c::DataLen _mCurEventRecordHeadOffsetInCurPkt = bt2c::DataLen::fromBits(0);

    /* Remaining padding bits to skip for alignment */
    bt2c::DataLen _mRemainingLenToSkip = bt2c::DataLen::fromBits(0);

    /* Current data stream class or event record class ID */
    std::optional<unsigned long long> _mCurClsId;

    /* Current metadata stream UUID */
    std::array<bt2c::Uuid::Val, bt2c::Uuid::size()> _mCurMetadataStreamUuid;

    /* Current variable-length integer data */
    struct
    {
        /* Current value */
        unsigned long long val = 0;

        /* Current length */
        bt2c::DataLen len = bt2c::DataLen::fromBits(0);
    } _mCurVarLenInt;

    /*
     * Null codepoint finders.
     */
    NullCpFinder<1> _mUtf8NullCpFinder;
    NullCpFinder<2> _mUtf16NullCpFinder;
    NullCpFinder<4> _mUtf32NullCpFinder;

    /* Current scalar field class */
    const Fc *_mCurScalarFc = nullptr;

    /* Current scope */
    struct
    {
        Scope scope;
        const StructFc *fc = nullptr;
    } _mCurScope;

    struct
    {
        /* Expected total length of current packet */
        bt2c::DataLen total = bt2c::DataLen::fromBits(0);

        /* Expected content length of current packet */
        bt2c::DataLen content = bt2c::DataLen::fromBits(0);
    } _mCurPktExpectedLens;

    /* Stack */
    std::vector<_StackFrame> _mStack;

    /*
     * Saved key values (dynamic-length field lengths and
     * variant/optional field selectors).
     */
    std::vector<unsigned long long> _mSavedKeyVals;

    /* Current default clock value, if any */
    unsigned long long _mDefClkVal = 0;

    /* Logging configuration */
    bt2c::Logger _mLogger;
};

namespace internal {

template <bt2c::Signedness SignednessV, std::size_t LenBitsV, ByteOrder ByteOrderV,
          BitOrder BitOrderV>
ReadFixedLenIntFuncRet<SignednessV> readFixedLenInt(const ItemSeqIter& iter,
                                                    const FixedLenBitArrayFc& fc)
{
    if constexpr (LenBitsV != 0) {
        /* Byte-aligned, byte-sized: use bt2c::readFixedLenInt{Be,Le}() */
        static_cast<void>(fc);

        if constexpr (ByteOrderV == ByteOrder::Big) {
            return reverseFixedLenIntBitsIfNeeded<SignednessV, BitOrderV>(
                static_cast<ReadFixedLenIntFuncRet<SignednessV>>(
                    bt2c::readFixedLenIntBe<bt2c::StdIntT<LenBitsV, SignednessV>>(
                        iter._bufAtHead())),
                bt2c::DataLen::fromBits(LenBitsV));
        } else {
            return reverseFixedLenIntBitsIfNeeded<SignednessV, BitOrderV>(
                static_cast<ReadFixedLenIntFuncRet<SignednessV>>(
                    bt2c::readFixedLenIntLe<bt2c::StdIntT<LenBitsV, SignednessV>>(
                        iter._bufAtHead())),
                bt2c::DataLen::fromBits(LenBitsV));
        }
    } else {
        /* Any alignment, any length: use bt_bitfield_read_{be,le}() */
        iter._checkLastFixedLenBitArrayFieldByteOrder(fc);

        ReadFixedLenIntFuncRet<SignednessV> val;

        if constexpr (ByteOrderV == ByteOrder::Big) {
            bt_bitfield_read_be(iter._bufAtHead(), std::uint8_t,
                                iter._mHeadOffsetInCurPkt.extraBitCount(), *fc.len(), &val);
        } else {
            bt_bitfield_read_le(iter._bufAtHead(), std::uint8_t,
                                iter._mHeadOffsetInCurPkt.extraBitCount(), *fc.len(), &val);
        }

        return reverseFixedLenIntBitsIfNeeded<SignednessV, BitOrderV>(val, fc.len());
    }
}

} /* namespace internal */
} /* namespace ctf::src */

#endif /* BABELTRACE_PLUGINS_CTF_COMMON_SRC_ITEM_SEQ_ITEM_SEQ_ITER_HPP */
