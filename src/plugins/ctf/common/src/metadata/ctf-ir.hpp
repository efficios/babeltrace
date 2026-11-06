/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2022-2024 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_PLUGINS_CTF_COMMON_SRC_METADATA_CTF_IR_HPP
#define BABELTRACE_PLUGINS_CTF_COMMON_SRC_METADATA_CTF_IR_HPP

#include <cstdlib>
#include <functional>
#include <set>
#include <vector>

#include "cpp-common/bt2/trace-ir.hpp"
#include "cpp-common/bt2c/observable.hpp"
#include "cpp-common/bt2c/text-loc.hpp"
#include "cpp-common/vendor/wise-enum/wise_enum.h"

#include "../../metadata/ctf-ir.hpp"

namespace ctf::src {

/*
 * This is the CTF IR API specific to source component classes.
 *
 * This API defines a few internal user mixins for our needs to reuse
 * the common `ctf::ir` API.
 *
 * In general, if a C++ class C has a libCls() method, then if libCls()
 * returns a value for a given instance X, then consider that all the
 * dependent field classes of X have a saved key value index (more about
 * this below).
 */

/*
 * Maximum alignment (bits) of a field class.
 *
 * Neither CTF 1.8 nor CTF 2 has a maximum alignment, but the metadata
 * stream parsers reject anything greater than this because:
 *
 * • The `ctf` plugin stores alignments as `unsigned int` (for example,
 *   the `align` parameters of the create*Fc() functions below), 2^31
 *   being the greatest power of two which fits.
 *
 * • `ctf::src::ItemSeqIter` keeps that many bits of headroom at the end
 *   of the item sequence to align any decoding head offset without
 *   overflow.
 */
constexpr auto maxAlign = 1ULL << 31;

/* clang-format off */

/*
 * The deep type of a field class is a superset of `ctf::ir::FcType`
 * which contains additional, static information about the field class.
 *
 * For fixed-length and variable-length field classes, this deep type
 * incorporates important decoding information, for example:
 *
 * • Byte order.
 *
 * • Whether or not the bit order is reversed (unnatural).
 *
 * • Whether or not it's a "standard" fixed-length bit array.
 *
 * • Length, if it's a "standard" fixed-length bit array.
 *
 * • Integer signedness.
 *
 * • Whether or not the field has a role.
 *
 * • Whether or not the value of the field has to be saved as a
 *   key value.
 *
 * The purpose of the deep type of a field class is to have a single
 * `switch` statement in the field reading method of some data stream
 * decoder to select its next state and make it easier/possible for the
 * compiler to optimize as most common decisions are encoded in there.
 *
 * For example, if it's known that the next field to read is a
 * little-endian, byte-aligned 32-bit unsigned integer field of which
 * the data stream decoder needs to save the value, then its deep type
 * is `FcDeepType::FixedLenUIntBa32LeSaveVal`, which means the decoder
 * will jump to a specific, corresponding reading method directly. The
 * latter method can do exactly what's needed to perform such a field
 * reading operation efficiently (using bt2c::readFixedLenIntLe()) and
 * save its value unconditionally.
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
 * `WithMetadataStreamUuidRole`:
 *     Static-length array/BLOB field with the "metadata stream
 *     UUID" role.
 *
 * `Utf*`:
 *     String field with a specific UTF string encoding.
 */
WISE_ENUM_CLASS(FcDeepType,
    /* Fixed-length bit array */
    FixedLenBitArrayBa8,
    FixedLenBitArrayBe,
    FixedLenBitArrayBa16Be,
    FixedLenBitArrayBa32Be,
    FixedLenBitArrayBa64Be,
    FixedLenBitArrayLe,
    FixedLenBitArrayBa16Le,
    FixedLenBitArrayBa32Le,
    FixedLenBitArrayBa64Le,
    FixedLenBitArrayBa8Rev,
    FixedLenBitArrayBeRev,
    FixedLenBitArrayBa16BeRev,
    FixedLenBitArrayBa32BeRev,
    FixedLenBitArrayBa64BeRev,
    FixedLenBitArrayLeRev,
    FixedLenBitArrayBa16LeRev,
    FixedLenBitArrayBa32LeRev,
    FixedLenBitArrayBa64LeRev,

    /* Fixed-length bit map */
    FixedLenBitMapBa8,
    FixedLenBitMapBe,
    FixedLenBitMapBa16Be,
    FixedLenBitMapBa32Be,
    FixedLenBitMapBa64Be,
    FixedLenBitMapLe,
    FixedLenBitMapBa16Le,
    FixedLenBitMapBa32Le,
    FixedLenBitMapBa64Le,
    FixedLenBitMapBa8Rev,
    FixedLenBitMapBeRev,
    FixedLenBitMapBa16BeRev,
    FixedLenBitMapBa32BeRev,
    FixedLenBitMapBa64BeRev,
    FixedLenBitMapLeRev,
    FixedLenBitMapBa16LeRev,
    FixedLenBitMapBa32LeRev,
    FixedLenBitMapBa64LeRev,

    /* Fixed-length boolean */
    FixedLenBoolBa8,
    FixedLenBoolBa8SaveVal,
    FixedLenBoolBe,
    FixedLenBoolBeSaveVal,
    FixedLenBoolBa16Be,
    FixedLenBoolBa16BeSaveVal,
    FixedLenBoolBa32Be,
    FixedLenBoolBa32BeSaveVal,
    FixedLenBoolBa64Be,
    FixedLenBoolBa64BeSaveVal,
    FixedLenBoolLe,
    FixedLenBoolLeSaveVal,
    FixedLenBoolBa16Le,
    FixedLenBoolBa16LeSaveVal,
    FixedLenBoolBa32Le,
    FixedLenBoolBa32LeSaveVal,
    FixedLenBoolBa64Le,
    FixedLenBoolBa64LeSaveVal,
    FixedLenBoolBa8Rev,
    FixedLenBoolBa8RevSaveVal,
    FixedLenBoolBeRev,
    FixedLenBoolBeRevSaveVal,
    FixedLenBoolBa16BeRev,
    FixedLenBoolBa16BeRevSaveVal,
    FixedLenBoolBa32BeRev,
    FixedLenBoolBa32BeRevSaveVal,
    FixedLenBoolBa64BeRev,
    FixedLenBoolBa64BeRevSaveVal,
    FixedLenBoolLeRev,
    FixedLenBoolLeRevSaveVal,
    FixedLenBoolBa16LeRev,
    FixedLenBoolBa16LeRevSaveVal,
    FixedLenBoolBa32LeRev,
    FixedLenBoolBa32LeRevSaveVal,
    FixedLenBoolBa64LeRev,
    FixedLenBoolBa64LeRevSaveVal,

    /* Fixed-length floating-point number */
    FixedLenFloat32Be,
    FixedLenFloatBa32Be,
    FixedLenFloat64Be,
    FixedLenFloatBa64Be,
    FixedLenFloat32Le,
    FixedLenFloatBa32Le,
    FixedLenFloat64Le,
    FixedLenFloatBa64Le,
    FixedLenFloat32BeRev,
    FixedLenFloatBa32BeRev,
    FixedLenFloat64BeRev,
    FixedLenFloatBa64BeRev,
    FixedLenFloat32LeRev,
    FixedLenFloatBa32LeRev,
    FixedLenFloat64LeRev,
    FixedLenFloatBa64LeRev,

    /* Fixed-length unsigned integer */
    FixedLenUIntBa8,
    FixedLenUIntBa8SaveVal,
    FixedLenUIntBa8WithRole,
    FixedLenUIntBa8WithRoleSaveVal,
    FixedLenUIntBe,
    FixedLenUIntBeSaveVal,
    FixedLenUIntBeWithRole,
    FixedLenUIntBeWithRoleSaveVal,
    FixedLenUIntBa16Be,
    FixedLenUIntBa16BeSaveVal,
    FixedLenUIntBa16BeWithRole,
    FixedLenUIntBa16BeWithRoleSaveVal,
    FixedLenUIntBa32Be,
    FixedLenUIntBa32BeSaveVal,
    FixedLenUIntBa32BeWithRole,
    FixedLenUIntBa32BeWithRoleSaveVal,
    FixedLenUIntBa64Be,
    FixedLenUIntBa64BeSaveVal,
    FixedLenUIntBa64BeWithRole,
    FixedLenUIntBa64BeWithRoleSaveVal,
    FixedLenUIntLe,
    FixedLenUIntLeSaveVal,
    FixedLenUIntLeWithRole,
    FixedLenUIntLeWithRoleSaveVal,
    FixedLenUIntBa16Le,
    FixedLenUIntBa16LeSaveVal,
    FixedLenUIntBa16LeWithRole,
    FixedLenUIntBa16LeWithRoleSaveVal,
    FixedLenUIntBa32Le,
    FixedLenUIntBa32LeSaveVal,
    FixedLenUIntBa32LeWithRole,
    FixedLenUIntBa32LeWithRoleSaveVal,
    FixedLenUIntBa64Le,
    FixedLenUIntBa64LeSaveVal,
    FixedLenUIntBa64LeWithRole,
    FixedLenUIntBa64LeWithRoleSaveVal,
    FixedLenUIntBa8Rev,
    FixedLenUIntBa8RevSaveVal,
    FixedLenUIntBa8RevWithRole,
    FixedLenUIntBa8RevWithRoleSaveVal,
    FixedLenUIntBeRev,
    FixedLenUIntBeRevSaveVal,
    FixedLenUIntBeRevWithRole,
    FixedLenUIntBeRevWithRoleSaveVal,
    FixedLenUIntBa16BeRev,
    FixedLenUIntBa16BeRevSaveVal,
    FixedLenUIntBa16BeRevWithRole,
    FixedLenUIntBa16BeRevWithRoleSaveVal,
    FixedLenUIntBa32BeRev,
    FixedLenUIntBa32BeRevSaveVal,
    FixedLenUIntBa32BeRevWithRole,
    FixedLenUIntBa32BeRevWithRoleSaveVal,
    FixedLenUIntBa64BeRev,
    FixedLenUIntBa64BeRevSaveVal,
    FixedLenUIntBa64BeRevWithRole,
    FixedLenUIntBa64BeRevWithRoleSaveVal,
    FixedLenUIntLeRev,
    FixedLenUIntLeRevSaveVal,
    FixedLenUIntLeRevWithRole,
    FixedLenUIntLeRevWithRoleSaveVal,
    FixedLenUIntBa16LeRev,
    FixedLenUIntBa16LeRevSaveVal,
    FixedLenUIntBa16LeRevWithRole,
    FixedLenUIntBa16LeRevWithRoleSaveVal,
    FixedLenUIntBa32LeRev,
    FixedLenUIntBa32LeRevSaveVal,
    FixedLenUIntBa32LeRevWithRole,
    FixedLenUIntBa32LeRevWithRoleSaveVal,
    FixedLenUIntBa64LeRev,
    FixedLenUIntBa64LeRevSaveVal,
    FixedLenUIntBa64LeRevWithRole,
    FixedLenUIntBa64LeRevWithRoleSaveVal,

    /* Fixed-length signed integer */
    FixedLenSIntBa8,
    FixedLenSIntBa8SaveVal,
    FixedLenSIntBe,
    FixedLenSIntBeSaveVal,
    FixedLenSIntBa16Be,
    FixedLenSIntBa16BeSaveVal,
    FixedLenSIntBa32Be,
    FixedLenSIntBa32BeSaveVal,
    FixedLenSIntBa64Be,
    FixedLenSIntBa64BeSaveVal,
    FixedLenSIntLe,
    FixedLenSIntLeSaveVal,
    FixedLenSIntBa16Le,
    FixedLenSIntBa16LeSaveVal,
    FixedLenSIntBa32Le,
    FixedLenSIntBa32LeSaveVal,
    FixedLenSIntBa64Le,
    FixedLenSIntBa64LeSaveVal,
    FixedLenSIntBa8Rev,
    FixedLenSIntBa8RevSaveVal,
    FixedLenSIntBeRev,
    FixedLenSIntBeRevSaveVal,
    FixedLenSIntBa16BeRev,
    FixedLenSIntBa16BeRevSaveVal,
    FixedLenSIntBa32BeRev,
    FixedLenSIntBa32BeRevSaveVal,
    FixedLenSIntBa64BeRev,
    FixedLenSIntBa64BeRevSaveVal,
    FixedLenSIntLeRev,
    FixedLenSIntLeRevSaveVal,
    FixedLenSIntBa16LeRev,
    FixedLenSIntBa16LeRevSaveVal,
    FixedLenSIntBa32LeRev,
    FixedLenSIntBa32LeRevSaveVal,
    FixedLenSIntBa64LeRev,
    FixedLenSIntBa64LeRevSaveVal,

    /* Variable-length unsigned integer */
    VarLenUInt,
    VarLenUIntSaveVal,
    VarLenUIntWithRole,
    VarLenUIntWithRoleSaveVal,

    /* Variable-length signed integer */
    VarLenSInt,
    VarLenSIntSaveVal,

    /* String */
    NullTerminatedStrUtf8,
    NullTerminatedStrUtf16,
    NullTerminatedStrUtf32,
    StaticLenStr,
    DynLenStr,

    /* BLOB */
    StaticLenBlob,
    StaticLenBlobWithMetadataStreamUuidRole,
    DynLenBlob,

    /* Array */
    StaticLenArray,
    StaticLenArrayWithMetadataStreamUuidRole,
    DynLenArray,

    /* Structure */
    Struct,

    /* Optional */
    OptionalWithBoolSel,
    OptionalWithUIntSel,
    OptionalWithSIntSel,

    /* Variant */
    VariantWithUIntSel,
    VariantWithSIntSel
)

/* clang-format on */

/*
 * Vector of key value saving indexes.
 *
 * The strategy to decode dynamic-length, optional, and variant fields
 * (called _dependend_ fields) is for the data stream decoder to save
 * the values of _key_ fields (boolean or integer fields) so that it can
 * use them afterwards to decode dependent fields.
 *
 * A field class FC of which the deep type contains `SaveVal` contains
 * key value saving indexes IX (keyValSavingIndexes() method).
 * keyValSavingIndexes() possibly returns more than one index because
 * many dependent fields may depend on the same key field.
 *
 * When decoding an instance of FC (a key field), the decoder saves its
 * value to some vector V at the indexes IX. When decoding a dependent
 * field, its class contains an index to retrieve a saved key value (its
 * length or selector) from V.
 */
using KeyValSavingIndexes = std::vector<std::size_t>;

namespace internal {

struct CtfIrMixins;

} /* namespace internal */

/*
 * Set of field class pointers.
 */
using FcSet = std::set<ir::Fc<internal::CtfIrMixins> *>;

namespace internal {

/*
 * Field location user mixin.
 */
class FieldLocMixin
{
public:
    explicit FieldLocMixin(const bt2c::TextLoc& loc) noexcept;

    /*
     * Text location of this field location within the metadata stream.
     */
    const bt2c::TextLoc& loc() const noexcept
    {
        return _mLoc;
    }

private:
    /* Text location of this field location within the metadata stream */
    bt2c::TextLoc _mLoc;
};

/*
 * Clock class user mixin.
 */
class ClkClsMixin
{
public:
    explicit ClkClsMixin() noexcept = default;

    /*
     * Sets the equivalent libbabeltrace2 class to `cls` (shared).
     */
    void sharedLibCls(bt2::ClockClass::Shared cls) noexcept;

private:
    /* Equivalent libbabeltrace2 class (shared) */
    bt2::ClockClass::Shared _mSharedLibCls;
};

template <typename FcT>
class KeyFcMixin;

/*
 * Field class user mixin.
 */
class FcMixin
{
    template <typename>
    friend class KeyFcMixin;

public:
    explicit FcMixin(const bt2c::TextLoc& loc = bt2c::TextLoc {}) noexcept;

    explicit FcMixin(FcDeepType deepType, const bt2c::TextLoc& loc = bt2c::TextLoc {}) noexcept;

    /*
     * Text location of this field class within the metadata stream.
     */
    const bt2c::TextLoc& loc() const noexcept
    {
        return _mLoc;
    }

    /*
     * Deep type of this field class.
     */
    FcDeepType deepType() const noexcept
    {
        return _mDeepType;
    }

private:
    /* Deep type of this field class */
    FcDeepType _mDeepType;

    /* Text location of this field class within the metadata stream */
    bt2c::TextLoc _mLoc;
};

/*
 * Internal mixin for a key field class `FcT`.
 *
 * See the comment of `ValSavingIndexes` to learn more.
 */
template <typename FcT>
class KeyFcMixin
{
public:
    explicit KeyFcMixin() noexcept { };

    const KeyValSavingIndexes& keyValSavingIndexes() const noexcept
    {
        return _mKeyValSavingIndexes;
    }

    void addKeyValSavingIndex(const std::size_t index)
    {
        _mKeyValSavingIndexes.push_back(index);

        auto& deepType = static_cast<FcT&>(*this)._mDeepType;

        /*
         * At this point we know that the data stream decoder needs to
         * save the value of any instance of this field class (a
         * key field).
         *
         * Adjust the deep type of the field class so as to include that
         * it's a key one.
         */
        deepType = std::invoke([deepType] {
            switch (deepType) {
            case FcDeepType::FixedLenBoolBa8:
                return FcDeepType::FixedLenBoolBa8SaveVal;
            case FcDeepType::FixedLenBoolBe:
                return FcDeepType::FixedLenBoolBeSaveVal;
            case FcDeepType::FixedLenBoolBa16Be:
                return FcDeepType::FixedLenBoolBa16BeSaveVal;
            case FcDeepType::FixedLenBoolBa32Be:
                return FcDeepType::FixedLenBoolBa32BeSaveVal;
            case FcDeepType::FixedLenBoolBa64Be:
                return FcDeepType::FixedLenBoolBa64BeSaveVal;
            case FcDeepType::FixedLenBoolLe:
                return FcDeepType::FixedLenBoolLeSaveVal;
            case FcDeepType::FixedLenBoolBa16Le:
                return FcDeepType::FixedLenBoolBa16LeSaveVal;
            case FcDeepType::FixedLenBoolBa32Le:
                return FcDeepType::FixedLenBoolBa32LeSaveVal;
            case FcDeepType::FixedLenBoolBa64Le:
                return FcDeepType::FixedLenBoolBa64LeSaveVal;
            case FcDeepType::FixedLenBoolBa8Rev:
                return FcDeepType::FixedLenBoolBa8RevSaveVal;
            case FcDeepType::FixedLenBoolBeRev:
                return FcDeepType::FixedLenBoolBeRevSaveVal;
            case FcDeepType::FixedLenBoolBa16BeRev:
                return FcDeepType::FixedLenBoolBa16BeRevSaveVal;
            case FcDeepType::FixedLenBoolBa32BeRev:
                return FcDeepType::FixedLenBoolBa32BeRevSaveVal;
            case FcDeepType::FixedLenBoolBa64BeRev:
                return FcDeepType::FixedLenBoolBa64BeRevSaveVal;
            case FcDeepType::FixedLenBoolLeRev:
                return FcDeepType::FixedLenBoolLeRevSaveVal;
            case FcDeepType::FixedLenBoolBa16LeRev:
                return FcDeepType::FixedLenBoolBa16LeRevSaveVal;
            case FcDeepType::FixedLenBoolBa32LeRev:
                return FcDeepType::FixedLenBoolBa32LeRevSaveVal;
            case FcDeepType::FixedLenBoolBa64LeRev:
                return FcDeepType::FixedLenBoolBa64LeRevSaveVal;
            case FcDeepType::FixedLenUIntBa8:
                return FcDeepType::FixedLenUIntBa8SaveVal;
            case FcDeepType::FixedLenUIntBa8WithRole:
                return FcDeepType::FixedLenUIntBa8WithRoleSaveVal;
            case FcDeepType::FixedLenUIntBe:
                return FcDeepType::FixedLenUIntBeSaveVal;
            case FcDeepType::FixedLenUIntBeWithRole:
                return FcDeepType::FixedLenUIntBeWithRoleSaveVal;
            case FcDeepType::FixedLenUIntBa16Be:
                return FcDeepType::FixedLenUIntBa16BeSaveVal;
            case FcDeepType::FixedLenUIntBa16BeWithRole:
                return FcDeepType::FixedLenUIntBa16BeWithRoleSaveVal;
            case FcDeepType::FixedLenUIntBa32Be:
                return FcDeepType::FixedLenUIntBa32BeSaveVal;
            case FcDeepType::FixedLenUIntBa32BeWithRole:
                return FcDeepType::FixedLenUIntBa32BeWithRoleSaveVal;
            case FcDeepType::FixedLenUIntBa64Be:
                return FcDeepType::FixedLenUIntBa64BeSaveVal;
            case FcDeepType::FixedLenUIntBa64BeWithRole:
                return FcDeepType::FixedLenUIntBa64BeWithRoleSaveVal;
            case FcDeepType::FixedLenUIntLe:
                return FcDeepType::FixedLenUIntLeSaveVal;
            case FcDeepType::FixedLenUIntLeWithRole:
                return FcDeepType::FixedLenUIntLeWithRoleSaveVal;
            case FcDeepType::FixedLenUIntBa16Le:
                return FcDeepType::FixedLenUIntBa16LeSaveVal;
            case FcDeepType::FixedLenUIntBa16LeWithRole:
                return FcDeepType::FixedLenUIntBa16LeWithRoleSaveVal;
            case FcDeepType::FixedLenUIntBa32Le:
                return FcDeepType::FixedLenUIntBa32LeSaveVal;
            case FcDeepType::FixedLenUIntBa32LeWithRole:
                return FcDeepType::FixedLenUIntBa32LeWithRoleSaveVal;
            case FcDeepType::FixedLenUIntBa64Le:
                return FcDeepType::FixedLenUIntBa64LeSaveVal;
            case FcDeepType::FixedLenUIntBa64LeWithRole:
                return FcDeepType::FixedLenUIntBa64LeWithRoleSaveVal;
            case FcDeepType::FixedLenUIntBa8Rev:
                return FcDeepType::FixedLenUIntBa8RevSaveVal;
            case FcDeepType::FixedLenUIntBa8RevWithRole:
                return FcDeepType::FixedLenUIntBa8RevWithRoleSaveVal;
            case FcDeepType::FixedLenUIntBeRev:
                return FcDeepType::FixedLenUIntBeRevSaveVal;
            case FcDeepType::FixedLenUIntBeRevWithRole:
                return FcDeepType::FixedLenUIntBeRevWithRoleSaveVal;
            case FcDeepType::FixedLenUIntBa16BeRev:
                return FcDeepType::FixedLenUIntBa16BeRevSaveVal;
            case FcDeepType::FixedLenUIntBa16BeRevWithRole:
                return FcDeepType::FixedLenUIntBa16BeRevWithRoleSaveVal;
            case FcDeepType::FixedLenUIntBa32BeRev:
                return FcDeepType::FixedLenUIntBa32BeRevSaveVal;
            case FcDeepType::FixedLenUIntBa32BeRevWithRole:
                return FcDeepType::FixedLenUIntBa32BeRevWithRoleSaveVal;
            case FcDeepType::FixedLenUIntBa64BeRev:
                return FcDeepType::FixedLenUIntBa64BeRevSaveVal;
            case FcDeepType::FixedLenUIntBa64BeRevWithRole:
                return FcDeepType::FixedLenUIntBa64BeRevWithRoleSaveVal;
            case FcDeepType::FixedLenUIntLeRev:
                return FcDeepType::FixedLenUIntLeRevSaveVal;
            case FcDeepType::FixedLenUIntLeRevWithRole:
                return FcDeepType::FixedLenUIntLeRevWithRoleSaveVal;
            case FcDeepType::FixedLenUIntBa16LeRev:
                return FcDeepType::FixedLenUIntBa16LeRevSaveVal;
            case FcDeepType::FixedLenUIntBa16LeRevWithRole:
                return FcDeepType::FixedLenUIntBa16LeRevWithRoleSaveVal;
            case FcDeepType::FixedLenUIntBa32LeRev:
                return FcDeepType::FixedLenUIntBa32LeRevSaveVal;
            case FcDeepType::FixedLenUIntBa32LeRevWithRole:
                return FcDeepType::FixedLenUIntBa32LeRevWithRoleSaveVal;
            case FcDeepType::FixedLenUIntBa64LeRev:
                return FcDeepType::FixedLenUIntBa64LeRevSaveVal;
            case FcDeepType::FixedLenUIntBa64LeRevWithRole:
                return FcDeepType::FixedLenUIntBa64LeRevWithRoleSaveVal;
            case FcDeepType::FixedLenSIntBa8:
                return FcDeepType::FixedLenSIntBa8SaveVal;
            case FcDeepType::FixedLenSIntBe:
                return FcDeepType::FixedLenSIntBeSaveVal;
            case FcDeepType::FixedLenSIntBa16Be:
                return FcDeepType::FixedLenSIntBa16BeSaveVal;
            case FcDeepType::FixedLenSIntBa32Be:
                return FcDeepType::FixedLenSIntBa32BeSaveVal;
            case FcDeepType::FixedLenSIntBa64Be:
                return FcDeepType::FixedLenSIntBa64BeSaveVal;
            case FcDeepType::FixedLenSIntLe:
                return FcDeepType::FixedLenSIntLeSaveVal;
            case FcDeepType::FixedLenSIntBa16Le:
                return FcDeepType::FixedLenSIntBa16LeSaveVal;
            case FcDeepType::FixedLenSIntBa32Le:
                return FcDeepType::FixedLenSIntBa32LeSaveVal;
            case FcDeepType::FixedLenSIntBa64Le:
                return FcDeepType::FixedLenSIntBa64LeSaveVal;
            case FcDeepType::FixedLenSIntBa8Rev:
                return FcDeepType::FixedLenSIntBa8RevSaveVal;
            case FcDeepType::FixedLenSIntBeRev:
                return FcDeepType::FixedLenSIntBeRevSaveVal;
            case FcDeepType::FixedLenSIntBa16BeRev:
                return FcDeepType::FixedLenSIntBa16BeRevSaveVal;
            case FcDeepType::FixedLenSIntBa32BeRev:
                return FcDeepType::FixedLenSIntBa32BeRevSaveVal;
            case FcDeepType::FixedLenSIntBa64BeRev:
                return FcDeepType::FixedLenSIntBa64BeRevSaveVal;
            case FcDeepType::FixedLenSIntLeRev:
                return FcDeepType::FixedLenSIntLeRevSaveVal;
            case FcDeepType::FixedLenSIntBa16LeRev:
                return FcDeepType::FixedLenSIntBa16LeRevSaveVal;
            case FcDeepType::FixedLenSIntBa32LeRev:
                return FcDeepType::FixedLenSIntBa32LeRevSaveVal;
            case FcDeepType::FixedLenSIntBa64LeRev:
                return FcDeepType::FixedLenSIntBa64LeRevSaveVal;
            case FcDeepType::VarLenUInt:
                return FcDeepType::VarLenUIntSaveVal;
            case FcDeepType::VarLenUIntWithRole:
                return FcDeepType::VarLenUIntWithRoleSaveVal;
            case FcDeepType::VarLenSInt:
                return FcDeepType::VarLenSIntSaveVal;
            default:
                return deepType;
            }
        });
    }

private:
    /* Value saving indexes */
    KeyValSavingIndexes _mKeyValSavingIndexes;
};

/*
 * Internal mixin for a dependent (dynamic-length, optional, or variant)
 * field class.
 *
 * See the comment of `ValSavingIndexes` to learn more.
 */
class DependentFcMixin
{
public:
    explicit DependentFcMixin() noexcept { };

    /*
     * Index of the saved key value of this field class.
     */
    const std::optional<std::size_t>& savedKeyValIndex() const noexcept
    {
        return _mSavedKeyValIndex;
    }

    /*
     * Sets the index of the saved key value of this field class
     * to `savedKeyValIndex`.
     *
     * ┌────────────────────────────────────────────────────────────┐
     * │ IMPORTANT: Only call this method if you can guarantee that │
     * │ you won't clone `*this` (through Fc::clone()).             │
     * └────────────────────────────────────────────────────────────┘
     */
    void savedKeyValIndex(const std::size_t savedKeyValIndex) noexcept
    {
        _mSavedKeyValIndex = savedKeyValIndex;
    }

    /*
     * Key field classes of this field class.
     */
    const FcSet& keyFcs() noexcept
    {
        return _mKeyFcs;
    }

    /*
     * Sets the key field classes of this field class to `keyFcs`.
     *
     * ┌────────────────────────────────────────────────────────────┐
     * │ IMPORTANT: Only call this method if you can guarantee that │
     * │ you won't clone `*this` (through Fc::clone()).             │
     * └────────────────────────────────────────────────────────────┘
     */
    void keyFcs(FcSet keyFcs);

private:
    /* Index of the saved key value of this field class */
    std::optional<std::size_t> _mSavedKeyValIndex;

    /* Dependencies of this field class */
    FcSet _mKeyFcs;
};

/*
 * Trace class user mixin.
 */
class TraceClsMixin
{
public:
    using SavedKeyValCountUpdatedObservable = bt2c::Observable<std::size_t>;

public:
    explicit TraceClsMixin() noexcept { };

    /*
     * Sets the equivalent libbabeltrace2 class to `cls` (shared).
     */
    void sharedLibCls(bt2::TraceClass::Shared cls) noexcept;

    /*
     * Maximum number of saved key values for an instance of this
     * trace class.
     *
     * See the comment of `ValSavingIndexes` to learn more.
     */
    std::size_t savedKeyValCount() const noexcept
    {
        return _mSavedKeyValCount;
    }

    /*
     * Sets the maximum number of saved key values for an instance of
     * this trace class to `savedKeyValCount`.
     *
     * See the comment of `ValSavingIndexes` to learn more.
     */
    void savedKeyValCount(const std::size_t savedKeyValCount) noexcept
    {
        _mSavedKeyValCount = savedKeyValCount;
        _mSavedKeyValCountUpdatedObservable.notify(_mSavedKeyValCount);
    }

    /*
     * Observable notified when the number of saved keys values required
     * by this trace class changes.
     */
    SavedKeyValCountUpdatedObservable& savedKeyValCountUpdatedObservable() const
    {
        return _mSavedKeyValCountUpdatedObservable;
    }

private:
    /* Equivalent libbabeltrace2 class (shared) */
    bt2::TraceClass::Shared _mSharedLibCls;

    /*
     * Maximum number of saved key values for an instance of this
     * trace class.
     *
     * See the comment of `ValSavingIndexes` to learn more.
     */
    std::size_t _mSavedKeyValCount = 0;

    /*
     * Observable notified when the number of saved key values required
     * by this trace class changes.
     */
    mutable SavedKeyValCountUpdatedObservable _mSavedKeyValCountUpdatedObservable;
};

/*
 * User mixin container.
 */
struct CtfIrMixins final : public ir::DefUserMixins
{
    using FieldLoc = FieldLocMixin;
    using ClkCls = ClkClsMixin;
    using Fc = FcMixin;
    using FixedLenBoolFc = KeyFcMixin<ir::FixedLenBoolFc<internal::CtfIrMixins>>;
    using FixedLenIntFc = KeyFcMixin<ir::FixedLenIntFc<internal::CtfIrMixins>>;
    using VarLenIntFc = KeyFcMixin<ir::VarLenIntFc<internal::CtfIrMixins>>;
    using DynLenStrFc = DependentFcMixin;
    using DynLenBlobFc = DependentFcMixin;
    using DynLenArrayFc = DependentFcMixin;
    using VariantFc = DependentFcMixin;
    using OptionalFc = DependentFcMixin;
    using TraceCls = TraceClsMixin;
};

} /* namespace internal */

/* Aliases of the `ctf::src` CTF IR API */
using ArrayFc = ir::ArrayFc<internal::CtfIrMixins>;
using BitOrder = ir::BitOrder;
using BlobFc = ir::BlobFc<internal::CtfIrMixins>;
using ByteOrder = ir::ByteOrder;
using ClkCls = ir::ClkCls<internal::CtfIrMixins>;
using ClkOffset = ir::ClkOffset;
using ClkOrigin = ir::ClkOrigin;
using ConstFcVisitor = ir::ConstFcVisitor<internal::CtfIrMixins>;
using DataStreamCls = ir::DataStreamCls<internal::CtfIrMixins>;
using DispBase = ir::DispBase;
using DynLenArrayFc = ir::DynLenArrayFc<internal::CtfIrMixins>;
using DynLenBlobFc = ir::DynLenBlobFc<internal::CtfIrMixins>;
using DynLenStrFc = ir::DynLenStrFc<internal::CtfIrMixins>;
using EventRecordCls = ir::EventRecordCls<internal::CtfIrMixins>;
using Fc = ir::Fc<internal::CtfIrMixins>;
using FcType = ir::FcType;
using FcVisitor = ir::FcVisitor<internal::CtfIrMixins>;
using FieldLoc = ir::FieldLoc<internal::CtfIrMixins>;
using FixedLenBitArrayFc = ir::FixedLenBitArrayFc<internal::CtfIrMixins>;
using FixedLenBitMapFc = ir::FixedLenBitMapFc<internal::CtfIrMixins>;
using FixedLenBoolFc = ir::FixedLenBoolFc<internal::CtfIrMixins>;
using FixedLenFloatFc = ir::FixedLenFloatFc<internal::CtfIrMixins>;
using FixedLenIntFc = ir::FixedLenIntFc<internal::CtfIrMixins>;
using FixedLenSIntFc = ir::FixedLenSIntFc<internal::CtfIrMixins>;
using FixedLenUIntFc = ir::FixedLenUIntFc<internal::CtfIrMixins>;
using NonNullTerminatedStrFc = ir::NonNullTerminatedStrFc<internal::CtfIrMixins>;
using NullTerminatedStrFc = ir::NullTerminatedStrFc<internal::CtfIrMixins>;
using OptionalFc = ir::OptionalFc<internal::CtfIrMixins>;
using OptionalWithBoolSelFc = ir::OptionalWithBoolSelFc<internal::CtfIrMixins>;
using OptionalWithSIntSelFc = ir::OptionalWithSIntSelFc<internal::CtfIrMixins>;
using OptionalWithUIntSelFc = ir::OptionalWithUIntSelFc<internal::CtfIrMixins>;
using OptAttrs = ir::OptAttrs;
using Scope = ir::Scope;
using StaticLenArrayFc = ir::StaticLenArrayFc<internal::CtfIrMixins>;
using StaticLenBlobFc = ir::StaticLenBlobFc<internal::CtfIrMixins>;
using StaticLenStrFc = ir::StaticLenStrFc<internal::CtfIrMixins>;
using StrEncoding = ir::StrEncoding;
using StrFc = ir::StrFc<internal::CtfIrMixins>;
using StructFc = ir::StructFc<internal::CtfIrMixins>;
using StructFieldMemberCls = ir::StructFieldMemberCls<internal::CtfIrMixins>;
using TraceCls = ir::TraceCls<internal::CtfIrMixins>;
using UIntFieldRole = ir::UIntFieldRole;
using UIntFieldRoles = ir::UIntFieldRoles;
using VariantWithSIntSelFc = ir::VariantWithSIntSelFc<internal::CtfIrMixins>;
using VariantWithUIntSelFc = ir::VariantWithUIntSelFc<internal::CtfIrMixins>;
using VarLenIntFc = ir::VarLenIntFc<internal::CtfIrMixins>;
using VarLenSIntFc = ir::VarLenSIntFc<internal::CtfIrMixins>;
using VarLenUIntFc = ir::VarLenUIntFc<internal::CtfIrMixins>;

/*
 * Creates and returns a field location having the origin `origin` and
 * the path items `items`.
 */
FieldLoc createFieldLoc(const bt2c::TextLoc& loc, std::optional<Scope> origin,
                        FieldLoc::Items items);

/*
 * Overload without a text location.
 */
inline FieldLoc createFieldLoc(std::optional<Scope> origin, FieldLoc::Items items)
{
    return createFieldLoc(bt2c::TextLoc {}, std::move(origin), std::move(items));
}

/*
 * Creates and returns a class of fixed-length bit array fields having
 * the alignment `align` bits, the length `len`, the byte order
 * `byteOrder`, the bit order `bitOrder`, and the attributes `attrs`.
 */
std::unique_ptr<FixedLenBitArrayFc> createFixedLenBitArrayFc(
    const bt2c::TextLoc& loc, unsigned int align, bt2c::DataLen len, ByteOrder byteOrder,
    const std::optional<BitOrder>& bitOrder = std::nullopt, OptAttrs attrs = OptAttrs {});

/*
 * Creates and returns a class of fixed-length bit map fields having the
 * alignment `align` bits, the length `len`, the byte order `byteOrder`,
 * the bit order `bitOrder`, the flags `flags`, and the
 * attributes `attrs`.
 */
std::unique_ptr<FixedLenBitMapFc>
createFixedLenBitMapFc(const bt2c::TextLoc& loc, unsigned int align, bt2c::DataLen len,
                       ByteOrder byteOrder, FixedLenBitMapFc::Flags flags,
                       const std::optional<BitOrder>& bitOrder = std::nullopt,
                       OptAttrs attrs = OptAttrs {});

/*
 * Creates and returns a class of fixed-length boolean fields having the
 * alignment `align` bits, the length `len`, the byte order `byteOrder`,
 * the bit order `bitOrder`, and the attributes `attrs`.
 */
std::unique_ptr<FixedLenBoolFc>
createFixedLenBoolFc(const bt2c::TextLoc& loc, unsigned int align, bt2c::DataLen len,
                     ByteOrder byteOrder, const std::optional<BitOrder>& bitOrder = std::nullopt,
                     OptAttrs attrs = OptAttrs {});

/*
 * Creates and returns a class of fixed-length floating point number
 * fields having the alignment `align` bits, the length `len`, the byte
 * order `byteOrder`, the bit order `bitOrder`, and the
 * attributes `attrs`.
 */
std::unique_ptr<FixedLenFloatFc>
createFixedLenFloatFc(const bt2c::TextLoc& loc, unsigned int align, bt2c::DataLen len,
                      ByteOrder byteOrder, const std::optional<BitOrder>& bitOrder = std::nullopt,
                      OptAttrs attrs = OptAttrs {});

/*
 * Overload without a text location.
 */
inline std::unique_ptr<FixedLenFloatFc>
createFixedLenFloatFc(const unsigned int align, const bt2c::DataLen len, const ByteOrder byteOrder,
                      const std::optional<BitOrder>& bitOrder = std::nullopt,
                      OptAttrs attrs = OptAttrs {})
{
    return createFixedLenFloatFc(bt2c::TextLoc {}, align, len, byteOrder, bitOrder,
                                 std::move(attrs));
}

/*
 * Creates and returns a class of fixed-length unsigned integer fields
 * having the alignment `align` bits, the length `len`, the byte order
 * `byteOrder`, the bit order `bitOrder`, the preferred display base
 * `prefDispBase`, the mappings `mappings`, the roles `roles`, and the
 * attributes `attrs`.
 */
std::unique_ptr<FixedLenUIntFc>
createFixedLenUIntFc(const bt2c::TextLoc& loc, unsigned int align, bt2c::DataLen len,
                     ByteOrder byteOrder, const std::optional<BitOrder>& bitOrder = std::nullopt,
                     DispBase prefDispBase = DispBase::Dec, FixedLenUIntFc::Mappings mappings = {},
                     UIntFieldRoles roles = {}, OptAttrs attrs = OptAttrs {});

/*
 * Overload without a text location.
 */
inline std::unique_ptr<FixedLenUIntFc>
createFixedLenUIntFc(const unsigned int align, const bt2c::DataLen len, const ByteOrder byteOrder,
                     const std::optional<BitOrder>& bitOrder = std::nullopt,
                     const DispBase prefDispBase = DispBase::Dec,
                     FixedLenUIntFc::Mappings mappings = {}, UIntFieldRoles roles = {},
                     OptAttrs attrs = OptAttrs {})
{
    return createFixedLenUIntFc(bt2c::TextLoc {}, align, len, byteOrder, bitOrder, prefDispBase,
                                std::move(mappings), std::move(roles), std::move(attrs));
}

/*
 * Creates and returns a class of fixed-length signed integer fields
 * having the alignment `align` bits, the length `len`, the byte order
 * `byteOrder`, the bit order `bitOrder`, the preferred display base
 * `prefDispBase`, the mappings `mappings`, and the attributes `attrs`.
 */
std::unique_ptr<FixedLenSIntFc>
createFixedLenSIntFc(const bt2c::TextLoc& loc, unsigned int align, bt2c::DataLen len,
                     ByteOrder byteOrder, const std::optional<BitOrder>& bitOrder = std::nullopt,
                     DispBase prefDispBase = DispBase::Dec, FixedLenSIntFc::Mappings mappings = {},
                     OptAttrs attrs = OptAttrs {});

/*
 * Overload without a text location.
 */
inline std::unique_ptr<FixedLenSIntFc>
createFixedLenSIntFc(const unsigned int align, const bt2c::DataLen len, const ByteOrder byteOrder,
                     const std::optional<BitOrder>& bitOrder = std::nullopt,
                     const DispBase prefDispBase = DispBase::Dec,
                     FixedLenSIntFc::Mappings mappings = {}, OptAttrs attrs = OptAttrs {})
{
    return createFixedLenSIntFc(bt2c::TextLoc {}, align, len, byteOrder, bitOrder, prefDispBase,
                                std::move(mappings), std::move(attrs));
}

/*
 * Creates and returns a class of variable-length unsigned integer
 * fields having the preferred display base `prefDispBase`, the mappings
 * `mappings`, the roles `roles`, and the attributes `attrs`.
 */
std::unique_ptr<VarLenUIntFc> createVarLenUIntFc(const bt2c::TextLoc& loc,
                                                 DispBase prefDispBase = DispBase::Dec,
                                                 VarLenUIntFc::Mappings mappings = {},
                                                 UIntFieldRoles roles = {},
                                                 OptAttrs attrs = OptAttrs {});

/*
 * Overload without a text location.
 */
inline std::unique_ptr<VarLenUIntFc> createVarLenUIntFc(const DispBase prefDispBase = DispBase::Dec,
                                                        VarLenUIntFc::Mappings mappings = {},
                                                        UIntFieldRoles roles = {},
                                                        OptAttrs attrs = OptAttrs {})
{
    return createVarLenUIntFc(bt2c::TextLoc {}, prefDispBase, std::move(mappings), std::move(roles),
                              std::move(attrs));
}

/*
 * Creates and returns a class of variable-length signed integer fields
 * having, the preferred display base `prefDispBase`, the mappings
 * `mappings`, and the attributes `attrs`.
 */
std::unique_ptr<VarLenSIntFc> createVarLenSIntFc(const bt2c::TextLoc& loc,
                                                 DispBase prefDispBase = DispBase::Dec,
                                                 VarLenSIntFc::Mappings mappings = {},
                                                 OptAttrs attrs = OptAttrs {});

/*
 * Overload without a text location.
 */
inline std::unique_ptr<VarLenSIntFc> createVarLenSIntFc(const DispBase prefDispBase = DispBase::Dec,
                                                        VarLenSIntFc::Mappings mappings = {},
                                                        OptAttrs attrs = OptAttrs {})
{
    return createVarLenSIntFc(bt2c::TextLoc {}, prefDispBase, std::move(mappings),
                              std::move(attrs));
}

/*
 * Creates and returns a class of null-terminated fields having the
 * encoding `encoding` and the attributes `attrs`.
 */
std::unique_ptr<NullTerminatedStrFc>
createNullTerminatedStrFc(const bt2c::TextLoc& loc, StrEncoding encoding = StrEncoding::Utf8,
                          OptAttrs attrs = OptAttrs {});

/*
 * Overload without a text location.
 */
std::unique_ptr<NullTerminatedStrFc> inline createNullTerminatedStrFc(
    const StrEncoding encoding = StrEncoding::Utf8, OptAttrs attrs = OptAttrs {})
{
    return createNullTerminatedStrFc(bt2c::TextLoc {}, encoding, std::move(attrs));
}

/*
 * Creates and returns a class of static-length string fields having the
 * length `len` bytes, the encoding `encoding`, and the
 * attributes `attrs`.
 */
std::unique_ptr<StaticLenStrFc> createStaticLenStrFc(const bt2c::TextLoc& loc, std::size_t len,
                                                     StrEncoding encoding = StrEncoding::Utf8,
                                                     OptAttrs attrs = OptAttrs {});

/*
 * Overload without a text location.
 */
inline std::unique_ptr<StaticLenStrFc>
createStaticLenStrFc(const std::size_t len, const StrEncoding encoding = StrEncoding::Utf8,
                     OptAttrs attrs = OptAttrs {})
{
    return createStaticLenStrFc(bt2c::TextLoc {}, len, encoding, std::move(attrs));
}

/*
 * Creates and returns a class of dynamic-length string fields having
 * the length field location `lenFieldLoc`, the encoding `encoding`, and
 * the attributes `attrs`.
 */
std::unique_ptr<DynLenStrFc> createDynLenStrFc(const bt2c::TextLoc& loc, FieldLoc lenFieldLoc,
                                               StrEncoding encoding = StrEncoding::Utf8,
                                               OptAttrs attrs = OptAttrs {});

/*
 * Overload without a text location.
 */
inline std::unique_ptr<DynLenStrFc>
createDynLenStrFc(FieldLoc lenFieldLoc, const StrEncoding encoding = StrEncoding::Utf8,
                  OptAttrs attrs = OptAttrs {})
{
    return createDynLenStrFc(bt2c::TextLoc {}, std::move(lenFieldLoc), encoding, std::move(attrs));
}

/*
 * Creates and returns a class of static-length BLOB fields having the
 * length `len` bytes, the media type `mediaType`, the "metadata stream
 * Uuid" role if `hasMetadataStreamUuidRole` is true, and the user
 * attributes `attrs`.
 */
std::unique_ptr<StaticLenBlobFc>
createStaticLenBlobFc(const bt2c::TextLoc& loc, std::size_t len,
                      std::string mediaType = ir::defaultBlobMediaType,
                      bool hasMetadataStreamUuidRole = false, OptAttrs attrs = OptAttrs {});

/*
 * Creates and returns a class of dynamic-length BLOB fields having the
 * length field location `lenFieldLoc`, the media type `mediaType`, and
 * the attributes `attrs`.
 */
std::unique_ptr<DynLenBlobFc> createDynLenBlobFc(const bt2c::TextLoc& loc, FieldLoc lenFieldLoc,
                                                 std::string mediaType = ir::defaultBlobMediaType,
                                                 OptAttrs attrs = OptAttrs {});

/*
 * Creates and returns a class of static-length array fields having the
 * length `len`, the element field class `elemFc`, the minimum alignment
 * `minAlign` bits, the attributes `attrs`, and the "metadata stream
 * Uuid" role if `hasMetadataStreamUuidRole` is true.
 */
std::unique_ptr<StaticLenArrayFc> createStaticLenArrayFc(const bt2c::TextLoc& loc, std::size_t len,
                                                         Fc::UP elemFc, unsigned int minAlign = 1,
                                                         bool hasMetadataStreamUuidRole = false,
                                                         OptAttrs attrs = OptAttrs {});

/*
 * Overload without a text location.
 */
inline std::unique_ptr<StaticLenArrayFc>
createStaticLenArrayFc(const std::size_t len, Fc::UP elemFc, const unsigned int minAlign = 1,
                       const bool hasMetadataStreamUuidRole = false, OptAttrs attrs = OptAttrs {})
{
    return createStaticLenArrayFc(bt2c::TextLoc {}, len, std::move(elemFc), minAlign,
                                  hasMetadataStreamUuidRole, std::move(attrs));
}

/*
 * Creates and returns a class of dynamic-length array fields having the
 * length field location `lenFieldLoc`, the element field class
 * `elemFc`, the minimum alignment `minAlign` bits, and the user
 * attributes `attrs`.
 */
std::unique_ptr<DynLenArrayFc> createDynLenArrayFc(const bt2c::TextLoc& loc, FieldLoc lenFieldLoc,
                                                   Fc::UP elemFc, unsigned int minAlign = 1,
                                                   OptAttrs attrs = OptAttrs {});

/*
 * Overload without a text location.
 */
inline std::unique_ptr<DynLenArrayFc> createDynLenArrayFc(FieldLoc lenFieldLoc, Fc::UP elemFc,
                                                          const unsigned int minAlign = 1,
                                                          OptAttrs attrs = OptAttrs {})
{
    return createDynLenArrayFc(bt2c::TextLoc {}, std::move(lenFieldLoc), std::move(elemFc),
                               minAlign, std::move(attrs));
}

/*
 * Creates and returns a class of structure field members having the
 * name `name`, the field class `fc`, and the attributes `attrs`.
 */
StructFieldMemberCls createStructFieldMemberCls(std::string name, Fc::UP fc,
                                                OptAttrs attrs = OptAttrs {});

/*
 * Creates and returns a class of structure fields having members of
 * classes `memberClasses`, the minimum alignment `minAlign` bits, and
 * the attributes `attrs`.
 */
std::unique_ptr<StructFc> createStructFc(const bt2c::TextLoc& loc,
                                         StructFc::MemberClasses&& memberClasses = {},
                                         unsigned int minAlign = 1, OptAttrs attrs = OptAttrs {});

/*
 * Overload without a text location.
 */
inline std::unique_ptr<StructFc> createStructFc(StructFc::MemberClasses&& memberClasses = {},
                                                const unsigned int minAlign = 1,
                                                OptAttrs attrs = OptAttrs {})
{
    return createStructFc(bt2c::TextLoc {}, std::move(memberClasses), minAlign, std::move(attrs));
}

/*
 * Creates and returns a class of optional fields having a boolean
 * selector, the optional field class `fc`, the selector field location
 * `selFieldLoc`, and the attributes `attrs`.
 */
std::unique_ptr<OptionalWithBoolSelFc> createOptionalFc(const bt2c::TextLoc& loc, Fc::UP fc,
                                                        FieldLoc selFieldLoc,
                                                        OptAttrs attrs = OptAttrs {});

/*
 * Creates and returns a class of optional fields having an unsigned
 * integer selector, the optional field class `fc`, the selector field
 * location `selFieldLoc`, the integer selector field ranges
 * `selFieldRanges`, and the attributes `attrs`.
 */
std::unique_ptr<OptionalWithUIntSelFc> createOptionalFc(const bt2c::TextLoc& loc, Fc::UP fc,
                                                        FieldLoc selFieldLoc,
                                                        UIntRangeSet selFieldRanges,
                                                        OptAttrs attrs = OptAttrs {});

/*
 * Creates and returns a class of optional fields having a signed
 * integer selector, the optional field class `fc`, the selector field
 * location `selFieldLoc`, the integer selector field ranges
 * `selFieldRanges`, and the attributes `attrs`.
 */
std::unique_ptr<OptionalWithSIntSelFc> createOptionalFc(const bt2c::TextLoc& loc, Fc::UP fc,
                                                        FieldLoc selFieldLoc,
                                                        SIntRangeSet selFieldRanges,
                                                        OptAttrs attrs = OptAttrs {});

/*
 * Creates and returns an option, for the class of a variant field
 * having an unsigned integer selector, having the field class `fc`, the
 * selector field ranges `selFieldRanges`, the name `name`, and the user
 * attributes `attrs`.
 */
VariantWithUIntSelFc::Opt createVariantFcOpt(Fc::UP fc, UIntRangeSet selFieldRanges,
                                             std::optional<std::string> name,
                                             OptAttrs attrs = OptAttrs {});

/*
 * Creates and returns an option, for the class of a variant field
 * having a signed integer selector, having the field class `fc`, the
 * selector field ranges `selFieldRanges`, the name `name`, and the user
 * attributes `attrs`.
 */
VariantWithSIntSelFc::Opt createVariantFcOpt(Fc::UP fc, SIntRangeSet selFieldRanges,
                                             std::optional<std::string> name,
                                             OptAttrs attrs = OptAttrs {});

/*
 * Creates and returns a class of variant fields having an unsigned
 * integer selector, the options `opts`, the selector field location
 * `selFieldLoc`, and the attributes `attrs`.
 */
std::unique_ptr<VariantWithUIntSelFc> createVariantFc(const bt2c::TextLoc& loc,
                                                      VariantWithUIntSelFc::Opts&& opts,
                                                      FieldLoc selFieldLoc,
                                                      OptAttrs attrs = OptAttrs {});

/*
 * Overload without a text location.
 */
inline std::unique_ptr<VariantWithUIntSelFc> createVariantFc(VariantWithUIntSelFc::Opts&& opts,
                                                             FieldLoc selFieldLoc,
                                                             OptAttrs attrs = OptAttrs {})
{
    return createVariantFc(bt2c::TextLoc {}, std::move(opts), std::move(selFieldLoc),
                           std::move(attrs));
}

/*
 * Creates and returns a class of variant fields having a signed integer
 * selector, the options `opts`, the selector field location
 * `selFieldLoc`, and the attributes `attrs`.
 */
std::unique_ptr<VariantWithSIntSelFc> createVariantFc(const bt2c::TextLoc& loc,
                                                      VariantWithSIntSelFc::Opts&& opts,
                                                      FieldLoc selFieldLoc,
                                                      OptAttrs attrs = OptAttrs {});

/*
 * Overload without a text location.
 */
inline std::unique_ptr<VariantWithSIntSelFc> createVariantFc(VariantWithSIntSelFc::Opts&& opts,
                                                             FieldLoc selFieldLoc,
                                                             OptAttrs attrs = OptAttrs {})
{
    return createVariantFc(bt2c::TextLoc {}, std::move(opts), std::move(selFieldLoc),
                           std::move(attrs));
}

/*
 * Creates and returns a class, having the ID `id` (unique within its
 * trace class), of a clocks having the frequency `freq` Hz, the
 * namespace `ns`, the name `name`, the UID `uid`, the offset from
 * origin `offset`, the origin `origin`, the description `descr`, the
 * precision `precision` cycles, the accuracy `accuracy` cycles, and the
 * attributes `attrs`.
 */
ClkCls::SP createClkCls(
    std::string id, unsigned long long freq, std::optional<std::string> ns = std::nullopt,
    std::optional<std::string> name = std::nullopt, std::optional<std::string> uid = std::nullopt,
    const ClkOffset& offset = ClkOffset {}, std::optional<ClkOrigin> origin = ClkOrigin {},
    std::optional<std::string> descr = std::nullopt,
    std::optional<unsigned long long> precision = std::nullopt,
    std::optional<unsigned long long> accuracy = std::nullopt, OptAttrs attrs = OptAttrs {});

/*
 * Creates and returns a class, having the ID `id`, of event records
 * having the namespace `ns`, the name `name`, the UID `uid`, the
 * specific context field class `specCtxFc`, the payload field class
 * `payloadFc`, and the attributes `attrs`.
 */
std::unique_ptr<EventRecordCls>
createEventRecordCls(unsigned long long id, std::optional<std::string> ns = std::nullopt,
                     std::optional<std::string> name = std::nullopt,
                     std::optional<std::string> uid = std::nullopt, Fc::UP specCtxFc = nullptr,
                     Fc::UP payloadFc = nullptr, OptAttrs attrs = OptAttrs {});

/*
 * Creates and returns a class, having the ID `id`, of data streams
 * having the the namespace `ns`, the name `name`, the UID `uid`, the
 * packet context field class `pktCtxFc`, the event record header field
 * class `eventRecordHeaderFc`, the common event record context field
 * class `commonEventRecordCtxFc`, the default clock class `defClkCls`,
 * and the attributes `attrs`.
 */
std::unique_ptr<DataStreamCls>
createDataStreamCls(unsigned long long id, std::optional<std::string> ns = std::nullopt,
                    std::optional<std::string> name = std::nullopt,
                    std::optional<std::string> uid = std::nullopt, Fc::UP pktCtxFc = nullptr,
                    Fc::UP eventRecordHeaderFc = nullptr, Fc::UP commonEventRecordCtxFc = nullptr,
                    ClkCls::SP defClkCls = nullptr, OptAttrs attrs = OptAttrs {});

/*
 * Creates and returns a class of traces having the the namespace `ns`,
 * the name `name`, the UID `uid`, the environment `env`, the packet
 * header field class `pktHeaderFc`, and the attributes `attrs`.
 */
std::unique_ptr<TraceCls>
createTraceCls(std::optional<std::string> ns = std::nullopt,
               std::optional<std::string> name = std::nullopt,
               std::optional<std::string> uid = std::nullopt,
               bt2::ConstMapValue::Shared env = bt2::ConstMapValue::Shared {},
               Fc::UP pktHeaderFc = nullptr, OptAttrs attrs = OptAttrs {});

} /* namespace ctf::src */

#endif /* BABELTRACE_PLUGINS_CTF_COMMON_SRC_METADATA_CTF_IR_HPP */
