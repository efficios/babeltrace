/*
 * Copyright (c) 2022 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_PLUGINS_CTF_COMMON_SRC_ITEM_SEQ_ITEM_HPP
#define BABELTRACE_PLUGINS_CTF_COMMON_SRC_ITEM_SEQ_ITEM_HPP

#include <cstdint>

#include <fmt/format.h> /* IWYU pragma: keep */

#include "common/assert.h"
#include "cpp-common/bt2c/aliases.hpp"
#include "cpp-common/bt2c/data-len.hpp"
#include "cpp-common/vendor/wise-enum/wise_enum.h"

#include "../metadata/ctf-ir.hpp"
#include "item-visitor.hpp"

namespace ctf::src {

class ItemSeqIter;

/*
 * Abstract item base class.
 *
 * An item is the value of an item sequence iterator.
 */
class Item
{
public:
    /* clang-format off */

    /*
     * Item type.
     */
    WISE_ENUM_CLASS_MEMBER((Type, unsigned long long),
        /* `PktBeginItem` */
        PktBegin,

        /* `PktEndItem` */
        PktEnd,

        /* `ScopeBeginItem` */
        ScopeBegin,

        /* `ScopeEndItem` */
        ScopeEnd,

        /* `PktContentBeginItem` */
        PktContentBegin,

        /* `PktContentEndItem` */
        PktContentEnd,

        /* `EventRecordBeginItem` */
        EventRecordBegin,

        /* `EventRecordEndItem` */
        EventRecordEnd,

        /* `PktMagicNumberItem` */
        PktMagicNumber,

        /* `MetadataStreamUuidItem` */
        MetadataStreamUuid,

        /* `DataStreamInfoItem` */
        DataStreamInfo,

        /* `DefClkValItem` */
        DefClkValue,

        /* `PktInfoItem` */
        PktInfo,

        /* `EventRecordInfoItem` */
        EventRecordInfo,

        /* `FixedLenBitArrayFieldItem` */
        FixedLenBitArrayField,

        /* `FixedLenBitMapFieldItem` */
        FixedLenBitMapField,

        /* `FixedLenBoolFieldItem` */
        FixedLenBoolField,

        /* `FixedLenSIntFieldItem` */
        FixedLenSIntField,

        /* `FixedLenUIntFieldItem` */
        FixedLenUIntField,

        /* `FixedLenFloatFieldItem` */
        FixedLenFloatField,

        /* `VarLenSIntFieldItem` */
        VarLenSIntField,

        /* `VarLenUIntFieldItem` */
        VarLenUIntField,

        /* `NullTerminatedStrFieldBeginItem` */
        NullTerminatedStrFieldBegin,

        /* `NullTerminatedStrFieldEndItem` */
        NullTerminatedStrFieldEnd,

        /* `RawDataItem` */
        RawData,

        /* `StructFieldBeginItem` */
        StructFieldBegin,

        /* `StructFieldEndItem` */
        StructFieldEnd,

        /* `StaticLenArrayFieldBeginItem` */
        StaticLenArrayFieldBegin,

        /* `StaticLenArrayFieldEndItem` */
        StaticLenArrayFieldEnd,

        /* `DynLenArrayFieldBeginItem` */
        DynLenArrayFieldBegin,

        /* `DynLenArrayFieldEndItem` */
        DynLenArrayFieldEnd,

        /* `StaticLenBlobFieldBeginItem` */
        StaticLenBlobFieldBegin,

        /* `StaticLenBlobFieldEndItem` */
        StaticLenBlobFieldEnd,

        /* `DynLenBlobFieldBeginItem` */
        DynLenBlobFieldBegin,

        /* `DynLenBlobFieldEndItem` */
        DynLenBlobFieldEnd,

        /* `StaticLenStrFieldBeginItem` */
        StaticLenStrFieldBegin,

        /* `StaticLenStrFieldEndItem` */
        StaticLenStrFieldEnd,

        /* `DynLenStrFieldBeginItem` */
        DynLenStrFieldBegin,

        /* `DynLenStrFieldEndItem` */
        DynLenStrFieldEnd,

        /* `VariantFieldWithSIntSelBeginItem` */
        VariantFieldWithSIntSelBegin,

        /* `VariantFieldWithSIntSelEndItem` */
        VariantFieldWithSIntSelEnd,

        /* `VariantFieldWithUIntSelBeginItem` */
        VariantFieldWithUIntSelBegin,

        /* `VariantFieldWithUIntSelEndItem` */
        VariantFieldWithUIntSelEnd,

        /* `OptionalFieldWithBoolSelBeginItem` */
        OptionalFieldWithBoolSelBegin,

        /* `OptionalFieldWithBoolSelEndItem` */
        OptionalFieldWithBoolSelEnd,

        /* `OptionalFieldWithSIntSelBeginItem` */
        OptionalFieldWithSIntSelBegin,

        /* `OptionalFieldWithSIntSelEndItem` */
        OptionalFieldWithSIntSelEnd,

        /* `OptionalFieldWithUIntSelBeginItem` */
        OptionalFieldWithUIntSelBegin,

        /* `OptionalFieldWithUIntSelEndItem` */
        OptionalFieldWithUIntSelEnd
    )

    /* clang-format on */

protected:
    explicit Item(Type type) noexcept;

public:
    virtual ~Item() = default;

    /*
     * Type of this item.
     *
     * You can also use accept() with an `ItemVisitor` instance to get
     * access to the concrete item.
     */
    Type type() const noexcept
    {
        return _mType;
    }

    virtual void accept(ItemVisitor& visitor) const = 0;

    /*
     * True if this item is a packet beginning item.
     */
    bool isPktBegin() const noexcept
    {
        return _mType == Type::PktBegin;
    }

    /*
     * True if this item is a packet end item.
     */
    bool isPktEnd() const noexcept
    {
        return _mType == Type::PktEnd;
    }

    /*
     * True if this item is a scope beginning item.
     */
    bool isScopeBegin() const noexcept
    {
        return _mType == Type::ScopeBegin;
    }

    /*
     * True if this item is a scope end item.
     */
    bool isScopeEnd() const noexcept
    {
        return _mType == Type::ScopeEnd;
    }

    /*
     * True if this item is a packet content beginning item.
     */
    bool isPktContentBegin() const noexcept
    {
        return _mType == Type::PktContentBegin;
    }

    /*
     * True if this item is a packet content end item.
     */
    bool isPktContentEnd() const noexcept
    {
        return _mType == Type::PktContentEnd;
    }

    /*
     * True if this item is an event record beginning item.
     */
    bool isEventRecordBegin() const noexcept
    {
        return _mType == Type::EventRecordBegin;
    }

    /*
     * True if this item is an event record end item.
     */
    bool isEventRecordEnd() const noexcept
    {
        return _mType == Type::EventRecordEnd;
    }

    /*
     * True if this item is a packet magic number item.
     */
    bool isPktMagicNumber() const noexcept
    {
        return _mType == Type::PktMagicNumber;
    }

    /*
     * True if this item is a metadata stream Uuid item.
     */
    bool isMetadataStreamUuid() const noexcept
    {
        return _mType == Type::MetadataStreamUuid;
    }

    /*
     * True if this item is a data stream info item.
     */
    bool isDataStreamInfo() const noexcept
    {
        return _mType == Type::DataStreamInfo;
    }

    /*
     * True if this item is a default clock value item.
     */
    bool isDefClkVal() const noexcept
    {
        return _mType == Type::DefClkValue;
    }

    /*
     * True if this item is a packet info item.
     */
    bool isPktInfo() const noexcept
    {
        return _mType == Type::PktInfo;
    }

    /*
     * True if this item is an event record info item.
     */
    bool isEventRecordInfo() const noexcept
    {
        return _mType == Type::EventRecordInfo;
    }

    /*
     * True if this item is a fixed-length bit map field item.
     */
    bool isFixedLenBitMapField() const noexcept
    {
        return _mType == Type::FixedLenBitMapField;
    }

    /*
     * True if this item is a fixed-length boolean field item.
     */
    bool isFixedLenBoolField() const noexcept
    {
        return _mType == Type::FixedLenBoolField;
    }

    /*
     * True if this item is a fixed-length signed integer field item.
     */
    bool isFixedLenSIntField() const noexcept
    {
        return _mType == Type::FixedLenSIntField;
    }

    /*
     * True if this item is a fixed-length unsigned integer field item.
     */
    bool isFixedLenUIntField() const noexcept
    {
        return _mType == Type::FixedLenUIntField;
    }

    /*
     * True if this item is a fixed-length floating-point number field
     * item.
     */
    bool isFixedLenFloatField() const noexcept
    {
        return _mType == Type::FixedLenFloatField;
    }

    /*
     * True if this item is a variable-length signed integer field item.
     */
    bool isVarLenSIntField() const noexcept
    {
        return _mType == Type::VarLenSIntField;
    }

    /*
     * True if this item is a variable-length unsigned integer field
     * item.
     */
    bool isVarLenUIntField() const noexcept
    {
        return _mType == Type::VarLenUIntField;
    }

    /*
     * True if this item is a null-terminated string field beginning
     * item.
     */
    bool isNullTerminatedStrFieldBegin() const noexcept
    {
        return _mType == Type::NullTerminatedStrFieldBegin;
    }

    /*
     * True if this item is a null-terminated string field end item.
     */
    bool isNullTerminatedStrFieldEnd() const noexcept
    {
        return _mType == Type::NullTerminatedStrFieldEnd;
    }

    /*
     * True if this item is a raw data item.
     */
    bool isRawData() const noexcept
    {
        return _mType == Type::RawData;
    }

    /*
     * True if this item is a structure field beginning item.
     */
    bool isStructFieldBegin() const noexcept
    {
        return _mType == Type::StructFieldBegin;
    }

    /*
     * True if this item is a structure field end item.
     */
    bool isStructFieldEnd() const noexcept
    {
        return _mType == Type::StructFieldEnd;
    }

    /*
     * True if this item is a static-length array field beginning item.
     */
    bool isStaticLenArrayFieldBegin() const noexcept
    {
        return _mType == Type::StaticLenArrayFieldBegin;
    }

    /*
     * True if this item is a static-length array field end item.
     */
    bool isStaticLenArrayFieldEnd() const noexcept
    {
        return _mType == Type::StaticLenArrayFieldEnd;
    }

    /*
     * True if this item is a dynamic-length array field beginning item.
     */
    bool isDynLenArrayFieldBegin() const noexcept
    {
        return _mType == Type::DynLenArrayFieldBegin;
    }

    /*
     * True if this item is a dynamic-length array field end item.
     */
    bool isDynLenArrayFieldEnd() const noexcept
    {
        return _mType == Type::DynLenArrayFieldEnd;
    }

    /*
     * True if this item is a static-length string field beginning item.
     */
    bool isStaticLenStrFieldBegin() const noexcept
    {
        return _mType == Type::StaticLenStrFieldBegin;
    }

    /*
     * True if this item is a static-length string field end item.
     */
    bool isStaticLenStrFieldEnd() const noexcept
    {
        return _mType == Type::StaticLenStrFieldEnd;
    }

    /*
     * True if this item is a dynamic-length string field beginning
     * item.
     */
    bool isDynLenStrFieldBegin() const noexcept
    {
        return _mType == Type::DynLenStrFieldBegin;
    }

    /*
     * True if this item is a dynamic-length string field end item.
     */
    bool isDynLenStrFieldEnd() const noexcept
    {
        return _mType == Type::DynLenStrFieldEnd;
    }

    /*
     * True if this item is a static-length BLOB field beginning item.
     */
    bool isStaticLenBlobFieldBegin() const noexcept
    {
        return _mType == Type::StaticLenBlobFieldBegin;
    }

    /*
     * True if this item is a static-length BLOB field end item.
     */
    bool isStaticLenBlobFieldEnd() const noexcept
    {
        return _mType == Type::StaticLenBlobFieldEnd;
    }

    /*
     * True if this item is a dynamic-length BLOB field beginning item.
     */
    bool isDynLenBlobFieldBegin() const noexcept
    {
        return _mType == Type::DynLenBlobFieldBegin;
    }

    /*
     * True if this item is a dynamic-length BLOB field end item.
     */
    bool isDynLenBlobFieldEnd() const noexcept
    {
        return _mType == Type::DynLenBlobFieldEnd;
    }

    /*
     * True if this item is a variant field with a signed integer
     * selector beginning item.
     */
    bool isVariantFieldWithSIntSelBegin() const noexcept
    {
        return _mType == Type::VariantFieldWithSIntSelBegin;
    }

    /*
     * True if this item is a variant field with a signed integer
     * selector end item.
     */
    bool isVariantFieldWithSIntSelEnd() const noexcept
    {
        return _mType == Type::VariantFieldWithSIntSelEnd;
    }

    /*
     * True if this item is a variant field with an unsigned integer
     * selector beginning item.
     */
    bool isVariantFieldWithUIntSelBegin() const noexcept
    {
        return _mType == Type::VariantFieldWithUIntSelBegin;
    }

    /*
     * True if this item is a variant field with an unsigned integer
     * selector end item.
     */
    bool isVariantFieldWithUIntSelEnd() const noexcept
    {
        return _mType == Type::VariantFieldWithUIntSelEnd;
    }

    /*
     * True if this item is an optional field with a boolean selector
     * beginning item.
     */
    bool isOptionalFieldWithBoolSelBegin() const noexcept
    {
        return _mType == Type::OptionalFieldWithBoolSelBegin;
    }

    /*
     * True if this item is an optional field with a boolean selector
     * end item.
     */
    bool isOptionalFieldWithBoolSelEnd() const noexcept
    {
        return _mType == Type::OptionalFieldWithBoolSelEnd;
    }

    /*
     * True if this item is an optional field with a signed integer
     * selector beginning item.
     */
    bool isOptionalFieldWithSIntSelBegin() const noexcept
    {
        return _mType == Type::OptionalFieldWithSIntSelBegin;
    }

    /*
     * True if this item is an optional field with a signed integer
     * selector end item.
     */
    bool isOptionalFieldWithSIntSelEnd() const noexcept
    {
        return _mType == Type::OptionalFieldWithSIntSelEnd;
    }

    /*
     * True if this item is an optional field with an unsigned integer
     * selector beginning item.
     */
    bool isOptionalFieldWithUIntSelBegin() const noexcept
    {
        return _mType == Type::OptionalFieldWithUIntSelBegin;
    }

    /*
     * True if this item is an optional field with an unsigned integer
     * selector end item.
     */
    bool isOptionalFieldWithUIntSelEnd() const noexcept
    {
        return _mType == Type::OptionalFieldWithUIntSelEnd;
    }

    /*
     * Returns this item as a data stream info item.
     */
    const DataStreamInfoItem& asDataStreamInfo() const noexcept;

    /*
     * Returns this item as a default clock value item.
     */
    const DefClkValItem& asDefClkVal() const noexcept;

    /*
     * Returns this item as a dynamic-length array field beginning item.
     */
    const DynLenArrayFieldBeginItem& asDynLenArrayFieldBegin() const noexcept;

    /*
     * Returns this item as a dynamic-length array field end item.
     */
    const DynLenArrayFieldEndItem& asDynLenArrayFieldEnd() const noexcept;

    /*
     * Returns this item as a dynamic-length string field beginning item.
     */
    const DynLenStrFieldBeginItem& asDynLenStrFieldBegin() const noexcept;

    /*
     * Returns this item as a dynamic-length string field end item.
     */
    const DynLenStrFieldEndItem& asDynLenStrFieldEnd() const noexcept;

    /*
     * Returns this item as a dynamic-length BLOB field beginning item.
     */
    const DynLenBlobFieldBeginItem& asDynLenBlobFieldBegin() const noexcept;

    /*
     * Returns this item as a dynamic-length BLOB field end item.
     */
    const DynLenBlobFieldEndItem& asDynLenBlobFieldEnd() const noexcept;

    /*
     * Returns this item as an event record beginning item.
     */
    const EventRecordBeginItem& asEventRecordBegin() const noexcept;

    /*
     * Returns this item as an event record end item.
     */
    const EventRecordEndItem& asEventRecordEnd() const noexcept;

    /*
     * Returns this item as an event record info item.
     */
    const EventRecordInfoItem& asEventRecordInfo() const noexcept;

    /*
     * Returns this item as a fixed-length bit array field item.
     */
    const FixedLenBitArrayFieldItem& asFixedLenBitArrayField() const noexcept;

    /*
     * Returns this item as a fixed-length bit map field item.
     */
    const FixedLenBitMapFieldItem& asFixedLenBitMapField() const noexcept;

    /*
     * Returns this item as a fixed-length boolean field item.
     */
    const FixedLenBoolFieldItem& asFixedLenBoolField() const noexcept;

    /*
     * Returns this item as a fixed-length floating-point number field
     * item.
     */
    const FixedLenFloatFieldItem& asFixedLenFloatField() const noexcept;

    /*
     * Returns this item as a fixed-length signed integer field item.
     */
    const FixedLenSIntFieldItem& asFixedLenSIntField() const noexcept;

    /*
     * Returns this item as a fixed-length unsigned integer field item.
     */
    const FixedLenUIntFieldItem& asFixedLenUIntField() const noexcept;

    /*
     * Returns this item as a string field beginning item.
     */
    const NullTerminatedStrFieldBeginItem& asNullTerminatedStrFieldBegin() const noexcept;

    /*
     * Returns this item as a string field end item.
     */
    const NullTerminatedStrFieldEndItem& asNullTerminatedStrFieldEnd() const noexcept;

    /*
     * Returns this item as an optional field beginning item.
     */
    const OptionalFieldBeginItem& asOptionalFieldBegin() const noexcept;

    /*
     * Returns this item as an optional field end item.
     */
    const OptionalFieldEndItem& asOptionalFieldEnd() const noexcept;

    /*
     * Returns this item as an optional field with a boolean selector
     * beginning item.
     */
    const OptionalFieldWithBoolSelBeginItem& asOptionalFieldWithBoolSelBegin() const noexcept;

    /*
     * Returns this item as an optional field with a boolean selector
     * end item.
     */
    const OptionalFieldWithBoolSelEndItem& asOptionalFieldWithBoolSelEnd() const noexcept;

    /*
     * Returns this item as an optional field with a signed integer
     * selector beginning item.
     */
    const OptionalFieldWithSIntSelBeginItem& asOptionalFieldWithSIntSelBegin() const noexcept;

    /*
     * Returns this item as an optional field with a signed integer
     * selector end item.
     */
    const OptionalFieldWithSIntSelEndItem& asOptionalFieldWithSIntSelEnd() const noexcept;

    /*
     * Returns this item as an optional field with an unsigned integer
     * selector beginning item.
     */
    const OptionalFieldWithUIntSelBeginItem& asOptionalFieldWithUIntSelBegin() const noexcept;

    /*
     * Returns this item as an optional field with an unsigned integer
     * selector end item.
     */
    const OptionalFieldWithUIntSelEndItem& asOptionalFieldWithUIntSelEnd() const noexcept;

    /*
     * Returns this item as a packet beginning item.
     */
    const PktBeginItem& asPktBegin() const noexcept;

    /*
     * Returns this item as a packet content beginning item.
     */
    const PktContentBeginItem& asPktContentBegin() const noexcept;

    /*
     * Returns this item as a packet content end item.
     */
    const PktContentEndItem& asPktContentEnd() const noexcept;

    /*
     * Returns this item as a packet end item.
     */
    const PktEndItem& asPktEnd() const noexcept;

    /*
     * Returns this item as a packet info item.
     */
    const PktInfoItem& asPktInfo() const noexcept;

    /*
     * Returns this item as a packet magic number item.
     */
    const PktMagicNumberItem& asPktMagicNumber() const noexcept;

    /*
     * Returns this item as a raw data item.
     */
    const RawDataItem& asRawData() const noexcept;

    /*
     * Returns this item as a scope beginning item.
     */
    const ScopeBeginItem& asScopeBegin() const noexcept;

    /*
     * Returns this item as a scope end item.
     */
    const ScopeEndItem& asScopeEnd() const noexcept;

    /*
     * Returns this item as an array field beginning item.
     */
    const ArrayFieldBeginItem& asArrayFieldBegin() const noexcept;

    /*
     * Returns this item as an array field end item.
     */
    const ArrayFieldEndItem& asArrayFieldEnd() const noexcept;

    /*
     * Returns this item as a static-length array field beginning item.
     */
    const StaticLenArrayFieldBeginItem& asStaticLenArrayFieldBegin() const noexcept;

    /*
     * Returns this item as a static-length array field end item.
     */
    const StaticLenArrayFieldEndItem& asStaticLenArrayFieldEnd() const noexcept;

    /*
     * Returns this item as a non null-terminated string field
     * beginning item.
     */
    const NonNullTerminatedStrFieldBeginItem& asNonNullTerminatedStrFieldBegin() const noexcept;

    /*
     * Returns this item as a non null-terminated string field end item.
     */
    const NonNullTerminatedStrFieldEndItem& asNonNullTerminatedStrFieldEnd() const noexcept;

    /*
     * Returns this item as a static-length string field beginning item.
     */
    const StaticLenStrFieldBeginItem& asStaticLenStrFieldBegin() const noexcept;

    /*
     * Returns this item as a static-length string field end item.
     */
    const StaticLenStrFieldEndItem& asStaticLenStrFieldEnd() const noexcept;

    /*
     * Returns this item as a BLOB field beginning item.
     */
    const BlobFieldBeginItem& asBlobFieldBegin() const noexcept;

    /*
     * Returns this item as a BLOB field end item.
     */
    const BlobFieldEndItem& asBlobFieldEnd() const noexcept;

    /*
     * Returns this item as a static-length BLOB field beginning item.
     */
    const StaticLenBlobFieldBeginItem& asStaticLenBlobFieldBegin() const noexcept;

    /*
     * Returns this item as a static-length BLOB field end item.
     */
    const StaticLenBlobFieldEndItem& asStaticLenBlobFieldEnd() const noexcept;

    /*
     * Returns this item as a structure field beginning item.
     */
    const StructFieldBeginItem& asStructFieldBegin() const noexcept;

    /*
     * Returns this item as a structure field end item.
     */
    const StructFieldEndItem& asStructFieldEnd() const noexcept;

    /*
     * Returns this item as a metadata stream Uuid item.
     */
    const MetadataStreamUuidItem& asMetadataStreamUuid() const noexcept;

    /*
     * Returns this item as a variable-length signed integer field item.
     */
    const VarLenSIntFieldItem& asVarLenSIntField() const noexcept;

    /*
     * Returns this item as a variable-length unsigned integer field
     * item.
     */
    const VarLenUIntFieldItem& asVarLenUIntField() const noexcept;

    /*
     * Returns this item as a variant field beginning item.
     */
    const VariantFieldBeginItem& asVariantFieldBegin() const noexcept;

    /*
     * Returns this item as a variant field end item.
     */
    const VariantFieldEndItem& asVariantFieldEnd() const noexcept;

    /*
     * Returns this item as a variant field with a signed integer
     * selector beginning item.
     */
    const VariantFieldWithSIntSelBeginItem& asVariantFieldWithSIntSelBegin() const noexcept;

    /*
     * Returns this item as a variant field with a signed integer
     * selector end item.
     */
    const VariantFieldWithSIntSelEndItem& asVariantFieldWithSIntSelEnd() const noexcept;

    /*
     * Returns this item as a variant field with an unsigned integer
     * selector beginning item.
     */
    const VariantFieldWithUIntSelBeginItem& asVariantFieldWithUIntSelBegin() const noexcept;

    /*
     * Returns this item as a variant field with an unsigned integer
     * selector end item.
     */
    const VariantFieldWithUIntSelEndItem& asVariantFieldWithUIntSelEnd() const noexcept;

private:
    Type _mType;
};

/*
 * Abstract beginning item base class.
 */
class BeginItem : public Item
{
protected:
    explicit BeginItem(Type type) noexcept;
};

/*
 * Abstract end item base class.
 */
class EndItem : public Item
{
protected:
    explicit EndItem(Type type) noexcept;
};

/*
 * Packet beginning item.
 */
class PktBeginItem final : public BeginItem
{
    friend class ItemSeqIter;

private:
    explicit PktBeginItem() noexcept;

public:
    void accept(ItemVisitor& visitor) const override;
};

/*
 * Packet end item.
 */
class PktEndItem final : public EndItem
{
    friend class ItemSeqIter;

private:
    explicit PktEndItem() noexcept;

public:
    void accept(ItemVisitor& visitor) const override;
};

/*
 * Abstract scope item base class.
 */
class ScopeItem
{
    friend class ItemSeqIter;

protected:
    explicit ScopeItem() noexcept = default;

public:
    Scope scope() const noexcept
    {
        return _mScope;
    }

private:
    Scope _mScope;
};

/*
 * Scope beginning item.
 */
class ScopeBeginItem final : public BeginItem, public ScopeItem
{
    friend class ItemSeqIter;

private:
    explicit ScopeBeginItem() noexcept;

public:
    void accept(ItemVisitor& visitor) const override;
};

/*
 * Scope end item.
 */
class ScopeEndItem final : public EndItem, public ScopeItem
{
    friend class ItemSeqIter;

private:
    explicit ScopeEndItem() noexcept;

public:
    void accept(ItemVisitor& visitor) const override;
};

/*
 * Event record beginning item.
 */
class EventRecordBeginItem final : public BeginItem
{
    friend class ItemSeqIter;

private:
    explicit EventRecordBeginItem() noexcept;

public:
    void accept(ItemVisitor& visitor) const override;
};

/*
 * Event record end item.
 */
class EventRecordEndItem final : public EndItem
{
    friend class ItemSeqIter;

private:
    explicit EventRecordEndItem() noexcept;

public:
    void accept(ItemVisitor& visitor) const override;
};

/*
 * Packet content beginning item.
 *
 * Such an item indicates the beginning of the _content of the current
 * packet.
 *
 * All the items between this one and the following `PktContentEndItem`
 * within a given item sequence are part of the packet content, which
 * does _not include the optional padding bits before the end of the
 * packet (indicated by the next `PktEndItem`).
 */
class PktContentBeginItem final : public BeginItem
{
    friend class ItemSeqIter;

private:
    explicit PktContentBeginItem() noexcept;

public:
    void accept(ItemVisitor& visitor) const override;
};

/*
 * Packet content end item.
 *
 * This item indicates the end of the _content of the current packet.
 */
class PktContentEndItem final : public EndItem
{
    friend class ItemSeqIter;

private:
    explicit PktContentEndItem() noexcept;

public:
    void accept(ItemVisitor& visitor) const override;
};

/*
 * Packet magic number item.
 *
 * This item contains the magic number of the decoded packet, as well as
 * the expected magic number (always 0xc1fc1fc1).
 *
 * Call isValid() to get whether or not the decoded magic number is
 * valid as per the Ctf specification.
 */
class PktMagicNumberItem final : public Item
{
    friend class ItemSeqIter;

private:
    explicit PktMagicNumberItem() noexcept;

public:
    void accept(ItemVisitor& visitor) const override;

    /*
     * True if the decoded magic number is valid.
     */
    bool isValid() const noexcept
    {
        return _mVal == this->expectedVal();
    }

    /*
     * Decoded magic number value.
     */
    unsigned long long val() const noexcept
    {
        return _mVal;
    }

    /*
     * Expected magic number value.
     */
    static constexpr unsigned long long expectedVal() noexcept
    {
        return 0xc1fc1fc1;
    }

private:
    unsigned long long _mVal = 0;
};

/*
 * Metadata stream Uuid item.
 *
 * This item contains the decoded metadata stream Uuid.
 */
class MetadataStreamUuidItem final : public Item
{
    friend class ItemSeqIter;

private:
    explicit MetadataStreamUuidItem() noexcept;

public:
    void accept(ItemVisitor& visitor) const override;

    /*
     * Decoded Uuid.
     */
    const bt2c::Uuid& uuid() const noexcept
    {
        return _mUuid;
    }

private:
    bt2c::Uuid _mUuid;
};

/*
 * Data stream information item.
 *
 * This item contains information about the current data stream, as
 * found in the header (if any) of the current packet.
 */
class DataStreamInfoItem final : public Item
{
    friend class ItemSeqIter;

private:
    explicit DataStreamInfoItem() noexcept;

public:
    void accept(ItemVisitor& visitor) const override;

    /*
     * Class of the data stream of the current packet, or `nullptr` if
     * the trace class has no data stream classes.
     */
    const DataStreamCls *cls() const noexcept
    {
        return _mCls;
    }

    /*
     * Id of the data stream of the current packet.
     *
     * Note: Not to be confused with the _class Id of the data stream
     * of the current packet: use `cls().id()`.
     */
    const std::optional<unsigned long long>& id() const noexcept
    {
        return _mId;
    }

private:
    void _reset() noexcept
    {
        _mCls = nullptr;
        _mId = std::nullopt;
    }

    const DataStreamCls *_mCls = nullptr;
    std::optional<unsigned long long> _mId;
};

/*
 * Default clock value item.
 *
 * This item provides the value of the default clock of the data stream
 * of the current packet.
 */
class DefClkValItem final : public Item
{
    friend class ItemSeqIter;

private:
    explicit DefClkValItem() noexcept;

public:
    /*
     * Value of the clock (cycles).
     */
    unsigned long long cycles() const noexcept
    {
        return _mCycles;
    }

    void accept(ItemVisitor& visitor) const override;

private:
    unsigned long long _mCycles = 0;
};

/*
 * Packet information item.
 *
 * This item contains information about the current packet, as found in
 * its context (if any).
 *
 * As per the Ctf rules:
 *
 * If expectedTotalLen() and expectedContentLen() are both not set:
 *     The total and content lengths of this packet are the length of
 *     the current data stream (this packet is the only one within its
 *     data stream).
 *
 * If expectedTotalLen() is set, but expectedContentLen() isn't:
 *     The expected content length of this packet is its expected total
 *     length (value of expectedTotalLen()).
 *
 * If expectedContentLen() is set, but expectedTotalLen() isn't:
 *     The expected total length of this packet is its expected content
 *     length (value of expectedContentLen()).
*/
class PktInfoItem final : public Item
{
    friend class ItemSeqIter;

private:
    explicit PktInfoItem() noexcept;

public:
    void accept(ItemVisitor& visitor) const override;

    /*
     * Numeric sequence number of the current packet within its _data
     * stream (_not within its item sequence).
     */
    const std::optional<unsigned long long>& seqNum() const noexcept
    {
        return _mSeqNum;
    }

    /*
     * Count of total discarded event records at the end of the current
     * packet since the beginning of its _data stream (_not its item
     * sequence).
     */
    const std::optional<unsigned long long>& discEventRecordCounterSnap() const noexcept
    {
        return _mDiscErCounterSnap;
    }

    /*
     * Expected total length of the current packet.
     *
     * This length _includes the packet padding bits before the end of
     * the packet, if any.
    */
    const std::optional<bt2c::DataLen>& expectedTotalLen() const noexcept
    {
        return _mExpectedTotalLen;
    }

    /*
     * Expected content length of the current packet.
     *
     * This length _excludes the packet padding bits before the end of
     * the packet, if any: the total length of the packet minus its
     * content length provides its padding length.
     */
    const std::optional<bt2c::DataLen>& expectedContentLen() const noexcept
    {
        return _mExpectedContentLen;
    }

    /*
     * Value (cycles) of the default clock of the data stream of the
     * current packet at its beginning.
     */
    const std::optional<unsigned long long>& beginDefClkVal() const noexcept
    {
        return _mBeginDefClkVal;
    }

    /*
     * Value (cycles) of the default clock of the data stream of the
     * current packet at its end.
     *
     * During the packet decoding process, this default clock value is
     * known _before decoding the event records.
     */
    const std::optional<unsigned long long>& endDefClkVal() const noexcept
    {
        return _mEndDefClkVal;
    }

private:
    void _reset() noexcept
    {
        _mSeqNum = std::nullopt;
        _mDiscErCounterSnap = std::nullopt;
        _mExpectedTotalLen = std::nullopt;
        _mExpectedContentLen = std::nullopt;
        _mBeginDefClkVal = std::nullopt;
        _mEndDefClkVal = std::nullopt;
    }

    std::optional<unsigned long long> _mSeqNum;
    std::optional<unsigned long long> _mDiscErCounterSnap;
    std::optional<bt2c::DataLen> _mExpectedTotalLen;
    std::optional<bt2c::DataLen> _mExpectedContentLen;
    std::optional<unsigned long long> _mBeginDefClkVal;
    std::optional<unsigned long long> _mEndDefClkVal;
};

/*
 * Event record information item.
 *
 * This item contains information about the current event record, as
 * found in its header (if any).
 */
class EventRecordInfoItem final : public Item
{
    friend class ItemSeqIter;

private:
    explicit EventRecordInfoItem() noexcept;

public:
    void accept(ItemVisitor& visitor) const override;

    /*
     * Value (cycles) of the default clock of the data stream of the
     * current event record when it occurred.
     */
    const std::optional<unsigned long long>& defClkVal() const noexcept
    {
        return _mDefClkVal;
    }

    /*
     * Class of the current event record, or `nullptr` if the data
     * stream class has no event record classes.
     */
    const EventRecordCls *cls() const noexcept
    {
        return _mCls;
    }

private:
    void _reset() noexcept
    {
        _mCls = nullptr;
        _mDefClkVal = std::nullopt;
    }

    const EventRecordCls *_mCls = nullptr;
    std::optional<unsigned long long> _mDefClkVal;
};

/*
 * Abstract field item base class.
 */
class FieldItem
{
    friend class ItemSeqIter;

protected:
    explicit FieldItem() noexcept = default;

public:
    /*
     * Class of this field.
     */
    const Fc& cls() const noexcept
    {
        return *_mCls;
    }

private:
    const Fc *_mCls;
};

/*
 * Abstract number field item base class.
 */
class NumberFieldItem : public FieldItem
{
    friend class ItemSeqIter;

protected:
    explicit NumberFieldItem() noexcept = default;

private:
    void _val(const unsigned long long val) noexcept
    {
        _mVal.u = val;
    }

    void _val(const long long val) noexcept
    {
        _mVal.i = val;
    }

    void _val(const double val) noexcept
    {
        _mVal.d = val;
    }

protected:
    union
    {
        unsigned long long u;
        long long i;
        double d;
    } _mVal;
};

/*
 * Fixed-length bit array field item.
 */
class FixedLenBitArrayFieldItem : public Item, public NumberFieldItem
{
    friend class ItemSeqIter;

protected:
    explicit FixedLenBitArrayFieldItem(Type type) noexcept;

private:
    explicit FixedLenBitArrayFieldItem() noexcept;

public:
    const FixedLenBitArrayFc& cls() const noexcept
    {
        return FieldItem::cls().asFixedLenBitArray();
    }

    /*
     * Value as an unsigned integer.
     */
    unsigned long long uIntVal() const noexcept
    {
        return _mVal.u;
    }

    /*
     * Returns the value of the bit at the index `index`, where 0 is the
     * index of the least significant bit.
     */
    bool operator[](const unsigned long long index) const noexcept
    {
        BT_ASSERT_DBG(index < *this->cls().len());
        return static_cast<bool>((_mVal.u >> index) & 1);
    }

    void accept(ItemVisitor& visitor) const override;
};

/*
 * Fixed-length bit map field item.
 */
class FixedLenBitMapFieldItem final : public FixedLenBitArrayFieldItem
{
    friend class ItemSeqIter;

private:
    explicit FixedLenBitMapFieldItem() noexcept;

public:
    const FixedLenBitMapFc& cls() const noexcept
    {
        return FieldItem::cls().asFixedLenBitMap();
    }

    void accept(ItemVisitor& visitor) const override;
};

/*
 * Fixed-length boolean field item.
 */
class FixedLenBoolFieldItem final : public FixedLenBitArrayFieldItem
{
    friend class ItemSeqIter;

private:
    explicit FixedLenBoolFieldItem() noexcept;

public:
    const FixedLenBoolFc& cls() const noexcept
    {
        return FieldItem::cls().asFixedLenBool();
    }

    bool val() const noexcept
    {
        return static_cast<bool>(_mVal.u);
    }

    void accept(ItemVisitor& visitor) const override;
};

/*
 * Fixed-length signed integer field item.
 */
class FixedLenSIntFieldItem : public FixedLenBitArrayFieldItem
{
    friend class ItemSeqIter;

protected:
    explicit FixedLenSIntFieldItem(Type type) noexcept;

private:
    explicit FixedLenSIntFieldItem() noexcept;

public:
    const FixedLenSIntFc& cls() const noexcept
    {
        return FieldItem::cls().asFixedLenSInt();
    }

    long long val() const noexcept
    {
        return _mVal.i;
    }

    void accept(ItemVisitor& visitor) const override;
};

/*
 * Fixed-length unsigned integer field item.
 */
class FixedLenUIntFieldItem : public FixedLenBitArrayFieldItem
{
    friend class ItemSeqIter;

protected:
    explicit FixedLenUIntFieldItem(Type type) noexcept;

private:
    explicit FixedLenUIntFieldItem() noexcept;

public:
    const FixedLenUIntFc& cls() const noexcept
    {
        return FieldItem::cls().asFixedLenUInt();
    }

    unsigned long long val() const noexcept
    {
        return _mVal.u;
    }

    void accept(ItemVisitor& visitor) const override;
};

/*
 * Fixed-length floating point number field item.
 */
class FixedLenFloatFieldItem final : public FixedLenBitArrayFieldItem
{
    friend class ItemSeqIter;

private:
    explicit FixedLenFloatFieldItem() noexcept;

public:
    const FixedLenFloatFc& cls() const noexcept
    {
        return FieldItem::cls().asFixedLenFloat();
    }

    double val() const noexcept
    {
        return _mVal.d;
    }

    void accept(ItemVisitor& visitor) const override;
};

/*
 * Variable-length integer field item.
 */
class VarLenIntFieldItem : public Item, public NumberFieldItem
{
    friend class ItemSeqIter;

protected:
    explicit VarLenIntFieldItem(Type type) noexcept;

public:
    /*
     * Integer length.
     *
     * This is the length of the decoded integer, not the length of the
     * field (use fieldLen()).
     */
    bt2c::DataLen len() const noexcept
    {
        return _mLen;
    }

    /*
     * Field length.
     *
     * This is the length of the field itself, not the length of the
     * decoded integer (use len()).
     */
    bt2c::DataLen fieldLen() const noexcept
    {
        /* `*_mLen` doesn't need to be a multiple of 7 */
        return bt2c::DataLen::fromBytesDbg((*_mLen + 6) / 7);
    }

private:
    bt2c::DataLen _mLen;
};

/*
 * Variable-length signed integer field item.
 */
class VarLenSIntFieldItem : public VarLenIntFieldItem
{
    friend class ItemSeqIter;

protected:
    explicit VarLenSIntFieldItem(Type type) noexcept;

private:
    explicit VarLenSIntFieldItem() noexcept;

public:
    const VarLenSIntFc& cls() const noexcept
    {
        return FieldItem::cls().asVarLenSInt();
    }

    long long val() const noexcept
    {
        return _mVal.i;
    }

    void accept(ItemVisitor& visitor) const override;
};

/*
 * Variable-length unsigned integer field item.
 */
class VarLenUIntFieldItem : public VarLenIntFieldItem
{
    friend class ItemSeqIter;

protected:
    explicit VarLenUIntFieldItem(Type type) noexcept;

private:
    explicit VarLenUIntFieldItem() noexcept;

public:
    const VarLenUIntFc& cls() const noexcept
    {
        return FieldItem::cls().asVarLenUInt();
    }

    unsigned long long val() const noexcept
    {
        return _mVal.u;
    }

    void accept(ItemVisitor& visitor) const override;
};

/*
 * Null-terminated string field beginning item.
 *
 * This item indicates the beginning of a null-terminated string field.
 *
 * The next `StrFieldSubstrItem` items before the next
 * `NullTerminatedStrFieldEndItem are consecutive substrings of this
 * beginning null-terminated string field.
 */
class NullTerminatedStrFieldBeginItem final : public BeginItem, public FieldItem
{
    friend class ItemSeqIter;

private:
    explicit NullTerminatedStrFieldBeginItem() noexcept;

public:
    const NullTerminatedStrFc& cls() const noexcept
    {
        return FieldItem::cls().asNullTerminatedStr();
    }

    void accept(ItemVisitor& visitor) const override;
};

/*
 * Null-terminated string field end item.
 *
 * This item indicates the end of a null-terminated string field.
 */
class NullTerminatedStrFieldEndItem final : public EndItem, public FieldItem
{
    friend class ItemSeqIter;

private:
    explicit NullTerminatedStrFieldEndItem() noexcept;

public:
    const NullTerminatedStrFc& cls() const noexcept
    {
        return FieldItem::cls().asNullTerminatedStr();
    }

    void accept(ItemVisitor& visitor) const override;
};

/*
 * Raw data item.
 *
 * This item may occur zero or more times:
 *
 * Null-terminated string field:
 *     Between `NullTerminatedStrFieldBeginItem` and
 *     `NullTerminatedStrFieldEndItem  items.
 *
 * Static-length string field:
 *     Between `StaticLenStrFieldBeginItem` and
 *     `StaticLenStrFieldEndItem` items.
 *
 * Dynamic-length string field:
 *     Between `DynLenStrFieldBeginItem` and `DynLenStrFieldEndItem`
 *     items.
 *
 * Static-length BLOB field:
 *     Between `StaticLenBlobFieldBeginItem` and
 *     `StaticLenBlobFieldEndItem` items.
 *
 * Dynamic-length BLOB field:
 *     Between `DynLenBlobFieldBeginItem` and `DynLenBlobFieldEndItem`
 *     items.
 *
 * begin() points to the first byte of the raw data and end() points to
 * the byte _after_ the last byte of the raw data. Use size() to compute
 * the size of the raw data.
 *
 * The pointed raw data is actually part of a data buffer (see the `Buf`
 * class) which the medium (see the `Medium` class) provided (zero
 * copy).
 *
 * The concatenated data of all the raw data items between a pair of
 * string/BLOB field beginning and end items forms:
 *
 * For a string field:
 *     The actual encoded string value.
 *
 *     NOTE: An item sequence iterator doesn't perform any string
 *     conversion: use the StrFc::encoding() method of the string field
 *     class of the last string field beginning item to get the encoding
 *     of the concatenated encoded string parts.
 *
 *     Considering the concatenated data D of all the raw data items
 *     between a pair of string beginning and string end items, D may
 *     contain an encoded U+0000 (null) codepoint. In that case, the
 *     actual encoded string finishes at this point, and excludes said
 *     encoded U+0000 codepoint. For static-length and dynamic-length
 *     string fields, D may contain more garbage bytes after an encoded
 *     U+0000 codepoint. Such data is still part of the data stream, but
 *     not part of the encoded string.
 *
 *     For example, consider the following bytes of D representing a CTF
 *     23-byte static-length UTF-16LE string field:
 *
 *         68 00 65 00 6c 00 6c 00 6f 00 20 00 77 00 6f 00
 *         72 00 6c 00 64 00 20 00 3c d8 3b df 00 00 dd ff
 *         44 52 00 00 bd cc 4e
 *
 *     The encoded string is `hello world 🌻` (everything before the
 *     first encoded U+0000 codepoint, which is two zero bytes in
 *     UTF-16) while the bytes
 *
 *         00 00 dd ff 44 52 00 00 bd cc 4e
 *
 *     are garbage bytes.
 *
 * For a BLOB field:
 *     The whole BLOB data.
 */
class RawDataItem final : public Item
{
    friend class ItemSeqIter;

private:
    explicit RawDataItem() noexcept;

public:
    /*
     * Raw data of this item.
     */
    const bt2c::ConstBytes& data() const noexcept
    {
        return _mData;
    }

    /*
     * Length as a `bt2c::DataLen` object.
     */
    bt2c::DataLen len() const noexcept
    {
        return bt2c::DataLen::fromBytesDbg(_mData.size());
    }

    void accept(ItemVisitor& visitor) const override;

private:
    void _assign(const std::uint8_t * const begin, const std::uint8_t * const end) noexcept
    {
        _mData = bt2c::ConstBytes {begin, end};
    }

    bt2c::ConstBytes _mData;
};

/*
 * Abstract array field beginning item base class.
 */
class ArrayFieldBeginItem : public BeginItem, public FieldItem
{
    friend class ItemSeqIter;

protected:
    explicit ArrayFieldBeginItem(Type type) noexcept;

public:
    const ArrayFc& cls() const noexcept
    {
        return FieldItem::cls().asArray();
    }
};

/*
 * Abstract array field end item base class.
 */
class ArrayFieldEndItem : public EndItem, public FieldItem
{
    friend class ItemSeqIter;

protected:
    explicit ArrayFieldEndItem(Type type) noexcept;

public:
    const ArrayFc& cls() const noexcept
    {
        return FieldItem::cls().asArray();
    }
};

/*
 * Static-length array field beginning item.
 *
 * This item indicates the beginning of a static-length array field.
 *
 * The next items until the next `StaticLenArrayFieldEndItem` at the
 * same level are all part of this static-length array field.
 */
class StaticLenArrayFieldBeginItem final : public ArrayFieldBeginItem
{
    friend class ItemSeqIter;

private:
    explicit StaticLenArrayFieldBeginItem() noexcept;

public:
    const StaticLenArrayFc& cls() const noexcept
    {
        return FieldItem::cls().asStaticLenArray();
    }

    void accept(ItemVisitor& visitor) const override;
};

/*
 * Static-length array field end item.
 *
 * This item indicates the end of a static-length array field.
 */
class StaticLenArrayFieldEndItem final : public ArrayFieldEndItem
{
    friend class ItemSeqIter;

private:
    explicit StaticLenArrayFieldEndItem() noexcept;

public:
    const StaticLenArrayFc& cls() const noexcept
    {
        return FieldItem::cls().asStaticLenArray();
    }

    void accept(ItemVisitor& visitor) const override;
};

/*
 * Dynamic-length array field beginning item.
 *
 * This item indicates the beginning of a dynamic-length array field.
 *
 * The next items until the next `DynLenArrayFieldEndItem` at the same
 * level are all part of this dynamic-length array field.
 */
class DynLenArrayFieldBeginItem final : public ArrayFieldBeginItem
{
    friend class ItemSeqIter;

private:
    explicit DynLenArrayFieldBeginItem() noexcept;

public:
    const DynLenArrayFc& cls() const noexcept
    {
        return FieldItem::cls().asDynLenArray();
    }

    /*
     * Array length (number of elements).
     */
    std::size_t len() const noexcept
    {
        return _mLen;
    }

    void accept(ItemVisitor& visitor) const override;

private:
    std::size_t _mLen = 0;
};

/*
 * Dynamic-length array field end item.
 *
 * This item indicates the end of a dynamic-length array field.
 */
class DynLenArrayFieldEndItem final : public ArrayFieldEndItem
{
    friend class ItemSeqIter;

private:
    explicit DynLenArrayFieldEndItem() noexcept;

public:
    const DynLenArrayFc& cls() const noexcept
    {
        return FieldItem::cls().asDynLenArray();
    }

    void accept(ItemVisitor& visitor) const override;
};

/*
 * Abstract non-null-terminated string field beginning item base class.
 */
class NonNullTerminatedStrFieldBeginItem : public BeginItem, public FieldItem
{
    friend class ItemSeqIter;

protected:
    using BeginItem::BeginItem;

public:
    const NonNullTerminatedStrFc& cls() const noexcept
    {
        return FieldItem::cls().asNonNullTerminatedStr();
    }
};

/*
 * Abstract non-null-terminated string field end item base class.
 */
class NonNullTerminatedStrFieldEndItem : public EndItem, public FieldItem
{
    friend class ItemSeqIter;

protected:
    using EndItem::EndItem;

public:
    const NonNullTerminatedStrFc& cls() const noexcept
    {
        return FieldItem::cls().asNonNullTerminatedStr();
    }
};

/*
 * Static-length string field beginning item.
 *
 * This item indicates the beginning of a static-length string field.
 *
 * The next `StrFieldSubstrItem` items before the next
 * `StaticLenStrFieldEndItem` are consecutive substrings of this
 * beginning static-length string field.
 */
class StaticLenStrFieldBeginItem final : public NonNullTerminatedStrFieldBeginItem
{
    friend class ItemSeqIter;

private:
    explicit StaticLenStrFieldBeginItem() noexcept;

public:
    const StaticLenStrFc& cls() const noexcept
    {
        return FieldItem::cls().asStaticLenStr();
    }

    void accept(ItemVisitor& visitor) const override;
};

/*
 * Static-length string field end item.
 *
 * This item indicates the end of a static-length string field.
 */
class StaticLenStrFieldEndItem final : public NonNullTerminatedStrFieldEndItem
{
    friend class ItemSeqIter;

private:
    explicit StaticLenStrFieldEndItem() noexcept;

public:
    const StaticLenStrFc& cls() const noexcept
    {
        return FieldItem::cls().asStaticLenStr();
    }

    void accept(ItemVisitor& visitor) const override;
};

/*
 * Dynamic-length string field beginning item.
 *
 * This item indicates the beginning of a dynamic-length string field.
 *
 * The next `StrFieldSubstrItem` items before the next
 * `DynLenStrFieldEndItem` are consecutive substrings of this beginning
 * dynamic-length string field.
 */
class DynLenStrFieldBeginItem final : public NonNullTerminatedStrFieldBeginItem
{
    friend class ItemSeqIter;

private:
    explicit DynLenStrFieldBeginItem() noexcept;

public:
    const DynLenStrFc& cls() const noexcept
    {
        return FieldItem::cls().asDynLenStr();
    }

    /*
     * Field length.
     */
    bt2c::DataLen len() const noexcept
    {
        return _mLen;
    }

    void accept(ItemVisitor& visitor) const override;

protected:
    bt2c::DataLen _mLen = bt2c::DataLen::fromBits(0);
};

/*
 * Dynamic-length string field end item.
 *
 * This item indicates the end of a dynamic-length string field.
 */
class DynLenStrFieldEndItem final : public NonNullTerminatedStrFieldEndItem
{
    friend class ItemSeqIter;

private:
    explicit DynLenStrFieldEndItem() noexcept;

public:
    const DynLenStrFc& cls() const noexcept
    {
        return FieldItem::cls().asDynLenStr();
    }

    void accept(ItemVisitor& visitor) const override;
};

/*
 * Abstract BLOB field beginning item base class.
 */
class BlobFieldBeginItem : public BeginItem, public FieldItem
{
    friend class ItemSeqIter;

protected:
    using BeginItem::BeginItem;

public:
    const BlobFc& cls() const noexcept
    {
        return FieldItem::cls().asBlob();
    }
};

/*
 * Abstract BLOB field end item base class.
 */
class BlobFieldEndItem : public EndItem, public FieldItem
{
    friend class ItemSeqIter;

protected:
    using EndItem::EndItem;

public:
    const BlobFc& cls() const noexcept
    {
        return FieldItem::cls().asBlob();
    }
};

/*
 * Static-length BLOB field beginning item.
 *
 * This item indicates the beginning of a static-length BLOB field.
 *
 * The next `RawDataItem` items before the next
 * `StaticLenBlobFieldEndItem` are consecutive BLOB field sections of
 * this beginning static-length BLOB field.
 */
class StaticLenBlobFieldBeginItem final : public BlobFieldBeginItem
{
    friend class ItemSeqIter;

private:
    explicit StaticLenBlobFieldBeginItem() noexcept;

public:
    const StaticLenBlobFc& cls() const noexcept
    {
        return FieldItem::cls().asStaticLenBlob();
    }

    void accept(ItemVisitor& visitor) const override;
};

/*
 * Static-length BLOB field end item.
 *
 * This item indicates the end of a static-length BLOB field.
 */
class StaticLenBlobFieldEndItem final : public BlobFieldEndItem
{
    friend class ItemSeqIter;

private:
    explicit StaticLenBlobFieldEndItem() noexcept;

public:
    const StaticLenBlobFc& cls() const noexcept
    {
        return FieldItem::cls().asStaticLenBlob();
    }

    void accept(ItemVisitor& visitor) const override;
};

/*
 * Dynamic-length BLOB field beginning item.
 *
 * This item indicates the beginning of a dynamic-length BLOB field.
 *
 * The next `RawDataItem` items before the next `DynLenBlobFieldEndItem`
 * are consecutive BLOB field sections of this beginning dynamic-length
 * BLOB field.
 */
class DynLenBlobFieldBeginItem final : public BlobFieldBeginItem
{
    friend class ItemSeqIter;

private:
    explicit DynLenBlobFieldBeginItem() noexcept;

public:
    const DynLenBlobFc& cls() const noexcept
    {
        return FieldItem::cls().asDynLenBlob();
    }

    /*
     * Field length.
     */
    bt2c::DataLen len() const noexcept
    {
        return _mLen;
    }

    void accept(ItemVisitor& visitor) const override;

protected:
    bt2c::DataLen _mLen = bt2c::DataLen::fromBits(0);
};

/*
 * Dynamic-length BLOB field end item.
 *
 * This item indicates the end of a dynamic-length BLOB field.
 */
class DynLenBlobFieldEndItem final : public BlobFieldEndItem
{
    friend class ItemSeqIter;

private:
    explicit DynLenBlobFieldEndItem() noexcept;

public:
    const DynLenBlobFc& cls() const noexcept
    {
        return FieldItem::cls().asDynLenBlob();
    }

    void accept(ItemVisitor& visitor) const override;
};

/*
 * Structure field beginning item.
 *
 * This item indicates the beginning of a structure field.
 *
 * The next items until the next `StructFieldEndItem` at the same level
 * are all part of this structure field.
 */
class StructFieldBeginItem final : public BeginItem, public FieldItem
{
    friend class ItemSeqIter;

private:
    explicit StructFieldBeginItem() noexcept;

public:
    const StructFc& cls() const noexcept
    {
        return FieldItem::cls().asStruct();
    }

    void accept(ItemVisitor& visitor) const override;
};

/*
 * Structure field end item.
 *
 * This item indicates the end of a structure field.
 */
class StructFieldEndItem final : public EndItem, public FieldItem
{
    friend class ItemSeqIter;

private:
    explicit StructFieldEndItem() noexcept;

public:
    const StructFc& cls() const noexcept
    {
        return FieldItem::cls().asStruct();
    }

    void accept(ItemVisitor& visitor) const override;
};

/*
 * Abstract variant field beginning item base class.
 */
class VariantFieldBeginItem : public BeginItem, public FieldItem
{
    friend class ItemSeqIter;

protected:
    explicit VariantFieldBeginItem(Type type) noexcept;

public:
    unsigned long long selectedOptIndex() const noexcept
    {
        return _mSelectedOptIndex;
    }

private:
    unsigned long long _mSelectedOptIndex = 0;
};

/*
 * Abstract variant field end item base class.
 */
class VariantFieldEndItem : public EndItem, public FieldItem
{
    friend class ItemSeqIter;

protected:
    explicit VariantFieldEndItem(Type type) noexcept;
};

/*
 * Abstract variant field with integer selector beginning item base
 * class.
 */
template <typename VariantFcT, typename SelValT, Item::Type TypeV>
class VariantFieldWithIntSelBeginItem : public VariantFieldBeginItem
{
    friend class ItemSeqIter;

protected:
    explicit VariantFieldWithIntSelBeginItem() noexcept
        : VariantFieldBeginItem {TypeV}
    {
    }

public:
    const VariantFcT& cls() const noexcept
    {
        return static_cast<const VariantFcT&>(this->cls());
    }

    /*
     * Value of the variant field selector.
     */
    SelValT selVal() const noexcept
    {
        return _mSelVal;
    }

    /*
     * Selected option of the variant field class.
     */
    const typename VariantFcT::Opt& clsOpt() const noexcept
    {
        return *_mOpt;
    }

private:
    SelValT _mSelVal = 0;
    const typename VariantFcT::Opt *_mOpt = nullptr;
};

/*
 * Abstract variant field with integer selector end item base class.
 */
template <typename VariantFcT, Item::Type TypeV>
class VariantFieldWithIntSelEndItem : public VariantFieldEndItem
{
    friend class ItemSeqIter;

protected:
    explicit VariantFieldWithIntSelEndItem() noexcept
        : VariantFieldEndItem {TypeV}
    {
    }

public:
    const VariantFcT& cls() const noexcept
    {
        return static_cast<const VariantFcT&>(this->cls());
    }
};

/*
 * Variant field with unsigned integer selector beginning item.
 *
 * This item indicates the beginning of a variant field having an
 * unsigned integer selector.
 *
 * The next item is the selected field item of this variant field.
 * Expect a `VariantFieldWithUIntSelEndItem` after this next item at the
 * same level.
 */
class VariantFieldWithUIntSelBeginItem final :
    public VariantFieldWithIntSelBeginItem<VariantWithUIntSelFc, unsigned long long,
                                           Item::Type::VariantFieldWithUIntSelBegin>
{
    friend class ItemSeqIter;

private:
    explicit VariantFieldWithUIntSelBeginItem() = default;

public:
    void accept(ItemVisitor& visitor) const override;
};

/*
 * Variant field with unsigned integer selector end item.
 *
 * This item indicates the end of a variant field having an unsigned
 * integer selector.
 */
class VariantFieldWithUIntSelEndItem final :
    public VariantFieldWithIntSelEndItem<VariantWithUIntSelFc,
                                         Item::Type::VariantFieldWithUIntSelEnd>
{
    friend class ItemSeqIter;

private:
    explicit VariantFieldWithUIntSelEndItem() = default;

public:
    void accept(ItemVisitor& visitor) const override;
};

/*
 * Variant field with signed integer selector beginning item.
 *
 * This item indicates the beginning of a variant field having a
 * signed integer selector.
 *
 * The next item is the selected field item of this variant field.
 * Expect a `VariantFieldWithSIntSelEndItem` after this next item at the
 * same level.
 */
class VariantFieldWithSIntSelBeginItem final :
    public VariantFieldWithIntSelBeginItem<VariantWithSIntSelFc, long long,
                                           Item::Type::VariantFieldWithSIntSelBegin>
{
    friend class ItemSeqIter;

private:
    explicit VariantFieldWithSIntSelBeginItem() = default;

public:
    void accept(ItemVisitor& visitor) const override;
};

/*
 * Variant field with signed integer selector end item.
 *
 * This item indicates the end of a variant field having a signed
 * integer selector.
 */
class VariantFieldWithSIntSelEndItem final :
    public VariantFieldWithIntSelEndItem<VariantWithSIntSelFc,
                                         Item::Type::VariantFieldWithSIntSelEnd>
{
    friend class ItemSeqIter;

private:
    explicit VariantFieldWithSIntSelEndItem() = default;

public:
    void accept(ItemVisitor& visitor) const override;
};

/*
 * Abstract optional field beginning item base class.
 */
class OptionalFieldBeginItem : public BeginItem, public FieldItem
{
    friend class ItemSeqIter;

protected:
    explicit OptionalFieldBeginItem(Type type) noexcept;

public:
    const OptionalFc& cls() const noexcept
    {
        return FieldItem::cls().asOptional();
    }

    /*
     * Whether or not this optional field is enabled (contains data).
     */
    bool isEnabled() const noexcept
    {
        return _mIsEnabled;
    }

private:
    bool _mIsEnabled = false;
};

/*
 * Abstract optional field end item base class.
 */
class OptionalFieldEndItem : public EndItem, public FieldItem
{
    friend class ItemSeqIter;

protected:
    explicit OptionalFieldEndItem(Type type) noexcept;

public:
    const OptionalFc& cls() const noexcept
    {
        return FieldItem::cls().asOptional();
    }
};

/*
 * Optional field with boolean selector beginning item.
 *
 * This item indicates the beginning of an optional field having a
 * boolean selector.
 *
 * The next item, if isEnabled() returns `true`, is the contained field
 * item of this optional field. Expect an
 * `OptionalFieldWithBoolSelEndItem` after this at the same level.
 */
class OptionalFieldWithBoolSelBeginItem final : public OptionalFieldBeginItem
{
    friend class ItemSeqIter;

private:
    explicit OptionalFieldWithBoolSelBeginItem() noexcept;

public:
    const OptionalWithBoolSelFc& cls() const noexcept
    {
        return FieldItem::cls().asOptionalWithBoolSel();
    }

    /*
     * Value of the optional field selector.
     */
    bool selVal() const noexcept
    {
        return _mSelVal;
    }

    void accept(ItemVisitor& visitor) const override;

private:
    bool _mSelVal = false;
};

/*
 * Optional field with boolean selector end item.
 *
 * This item indicates the end of an optional field having a boolean
 * selector.
 */
class OptionalFieldWithBoolSelEndItem final : public OptionalFieldEndItem
{
    friend class ItemSeqIter;

private:
    explicit OptionalFieldWithBoolSelEndItem() noexcept;

public:
    const OptionalWithBoolSelFc& cls() const noexcept
    {
        return FieldItem::cls().asOptionalWithBoolSel();
    }

public:
    void accept(ItemVisitor& visitor) const override;
};

/*
 * Abstract optional field with integer selector beginning item base
 * class.
 */
template <typename OptionalFcT, typename SelValT, Item::Type TypeV>
class OptionalFieldWithIntSelBeginItem : public OptionalFieldBeginItem
{
    friend class ItemSeqIter;

protected:
    explicit OptionalFieldWithIntSelBeginItem() noexcept
        : OptionalFieldBeginItem {TypeV}
    {
    }

public:
    const OptionalFcT& cls() const noexcept
    {
        return static_cast<const OptionalFcT&>(this->cls());
    }

    /*
     * Value of the optional field selector.
     */
    SelValT selVal() const noexcept
    {
        return _mSelVal;
    }

private:
    SelValT _mSelVal = 0;
};

/*
 * Abstract optional field with integer selector end item base class.
 */
template <typename OptionalFcT, Item::Type TypeV>
class OptionalFieldWithIntSelEndItem : public OptionalFieldEndItem
{
    friend class ItemSeqIter;

protected:
    explicit OptionalFieldWithIntSelEndItem() noexcept
        : OptionalFieldEndItem {TypeV}
    {
    }

public:
    const OptionalFcT& cls() const noexcept
    {
        return static_cast<const OptionalFcT&>(this->cls());
    }
};

/*
 * Optional field with unsigned integer selector beginning item.
 *
 * This item indicates the beginning of an optional field having an
 * unsigned integer selector.
 *
 * The next item, if isEnabled() returns `true`, is the contained field
 * item of this optional field. Expect an
 * `OptionalFieldWithUIntSelEndItem` after this at the same level.
 */
class OptionalFieldWithUIntSelBeginItem final :
    public OptionalFieldWithIntSelBeginItem<OptionalWithUIntSelFc, unsigned long long,
                                            Item::Type::OptionalFieldWithUIntSelBegin>
{
    friend class ItemSeqIter;

private:
    explicit OptionalFieldWithUIntSelBeginItem() = default;

public:
    void accept(ItemVisitor& visitor) const override;
};

/*
 * Optional field with unsigned integer selector end item.
 *
 * This item indicates the end of an optional field with an
 * unsigned integer selector.
 */
class OptionalFieldWithUIntSelEndItem final :
    public OptionalFieldWithIntSelEndItem<OptionalWithUIntSelFc,
                                          Item::Type::OptionalFieldWithUIntSelEnd>
{
    friend class ItemSeqIter;

private:
    explicit OptionalFieldWithUIntSelEndItem() = default;

public:
    void accept(ItemVisitor& visitor) const override;
};

/*
 * Optional field with signed integer selector beginning item.
 *
 * This item indicates the beginning of an optional field having a
 * signed integer selector.
 *
 * The next item, if isEnabled() returns `true`, is the contained field
 * item of this optional field. Expect an
 * `OptionalFieldWithSIntSelEndItem` after this at the same level.
 */
class OptionalFieldWithSIntSelBeginItem final :
    public OptionalFieldWithIntSelBeginItem<OptionalWithSIntSelFc, long long,
                                            Item::Type::OptionalFieldWithSIntSelBegin>
{
    friend class ItemSeqIter;

private:
    explicit OptionalFieldWithSIntSelBeginItem() = default;

public:
    void accept(ItemVisitor& visitor) const override;
};

/*
 * Optional field with signed integer selector end item.
 *
 * This item indicates the end of an optional field with a signed
 * integer selector.
 */
class OptionalFieldWithSIntSelEndItem final :
    public OptionalFieldWithIntSelEndItem<OptionalWithSIntSelFc,
                                          Item::Type::OptionalFieldWithSIntSelEnd>
{
    friend class ItemSeqIter;

private:
    explicit OptionalFieldWithSIntSelEndItem() = default;

public:
    void accept(ItemVisitor& visitor) const override;
};

inline wise_enum::string_type format_as(const Item::Type type)
{
    return wise_enum::to_string(type);
}

inline const DataStreamInfoItem& Item::asDataStreamInfo() const noexcept
{
    return static_cast<const DataStreamInfoItem&>(*this);
}

inline const DefClkValItem& Item::asDefClkVal() const noexcept
{
    return static_cast<const DefClkValItem&>(*this);
}

inline const DynLenArrayFieldBeginItem& Item::asDynLenArrayFieldBegin() const noexcept
{
    return static_cast<const DynLenArrayFieldBeginItem&>(*this);
}

inline const DynLenArrayFieldEndItem& Item::asDynLenArrayFieldEnd() const noexcept
{
    return static_cast<const DynLenArrayFieldEndItem&>(*this);
}

inline const DynLenBlobFieldBeginItem& Item::asDynLenBlobFieldBegin() const noexcept
{
    return static_cast<const DynLenBlobFieldBeginItem&>(*this);
}

inline const DynLenBlobFieldEndItem& Item::asDynLenBlobFieldEnd() const noexcept
{
    return static_cast<const DynLenBlobFieldEndItem&>(*this);
}

inline const DynLenStrFieldBeginItem& Item::asDynLenStrFieldBegin() const noexcept
{
    return static_cast<const DynLenStrFieldBeginItem&>(*this);
}

inline const DynLenStrFieldEndItem& Item::asDynLenStrFieldEnd() const noexcept
{
    return static_cast<const DynLenStrFieldEndItem&>(*this);
}

inline const EventRecordBeginItem& Item::asEventRecordBegin() const noexcept
{
    return static_cast<const EventRecordBeginItem&>(*this);
}

inline const EventRecordEndItem& Item::asEventRecordEnd() const noexcept
{
    return static_cast<const EventRecordEndItem&>(*this);
}

inline const EventRecordInfoItem& Item::asEventRecordInfo() const noexcept
{
    return static_cast<const EventRecordInfoItem&>(*this);
}

inline const FixedLenBitArrayFieldItem& Item::asFixedLenBitArrayField() const noexcept
{
    return static_cast<const FixedLenBitArrayFieldItem&>(*this);
}

inline const FixedLenBitMapFieldItem& Item::asFixedLenBitMapField() const noexcept
{
    return static_cast<const FixedLenBitMapFieldItem&>(*this);
}

inline const FixedLenBoolFieldItem& Item::asFixedLenBoolField() const noexcept
{
    return static_cast<const FixedLenBoolFieldItem&>(*this);
}

inline const FixedLenFloatFieldItem& Item::asFixedLenFloatField() const noexcept
{
    return static_cast<const FixedLenFloatFieldItem&>(*this);
}

inline const FixedLenSIntFieldItem& Item::asFixedLenSIntField() const noexcept
{
    return static_cast<const FixedLenSIntFieldItem&>(*this);
}

inline const FixedLenUIntFieldItem& Item::asFixedLenUIntField() const noexcept
{
    return static_cast<const FixedLenUIntFieldItem&>(*this);
}

inline const NullTerminatedStrFieldBeginItem& Item::asNullTerminatedStrFieldBegin() const noexcept
{
    return static_cast<const NullTerminatedStrFieldBeginItem&>(*this);
}

inline const NullTerminatedStrFieldEndItem& Item::asNullTerminatedStrFieldEnd() const noexcept
{
    return static_cast<const NullTerminatedStrFieldEndItem&>(*this);
}

inline const OptionalFieldBeginItem& Item::asOptionalFieldBegin() const noexcept
{
    return static_cast<const OptionalFieldBeginItem&>(*this);
}

inline const OptionalFieldEndItem& Item::asOptionalFieldEnd() const noexcept
{
    return static_cast<const OptionalFieldEndItem&>(*this);
}

inline const OptionalFieldWithBoolSelBeginItem&
Item::asOptionalFieldWithBoolSelBegin() const noexcept
{
    return static_cast<const OptionalFieldWithBoolSelBeginItem&>(*this);
}

inline const OptionalFieldWithBoolSelEndItem& Item::asOptionalFieldWithBoolSelEnd() const noexcept
{
    return static_cast<const OptionalFieldWithBoolSelEndItem&>(*this);
}

inline const OptionalFieldWithSIntSelBeginItem&
Item::asOptionalFieldWithSIntSelBegin() const noexcept
{
    return static_cast<const OptionalFieldWithSIntSelBeginItem&>(*this);
}

inline const OptionalFieldWithSIntSelEndItem& Item::asOptionalFieldWithSIntSelEnd() const noexcept
{
    return static_cast<const OptionalFieldWithSIntSelEndItem&>(*this);
}

inline const OptionalFieldWithUIntSelBeginItem&
Item::asOptionalFieldWithUIntSelBegin() const noexcept
{
    return static_cast<const OptionalFieldWithUIntSelBeginItem&>(*this);
}

inline const OptionalFieldWithUIntSelEndItem& Item::asOptionalFieldWithUIntSelEnd() const noexcept
{
    return static_cast<const OptionalFieldWithUIntSelEndItem&>(*this);
}

inline const PktBeginItem& Item::asPktBegin() const noexcept
{
    return static_cast<const PktBeginItem&>(*this);
}

inline const PktContentBeginItem& Item::asPktContentBegin() const noexcept
{
    return static_cast<const PktContentBeginItem&>(*this);
}

inline const PktContentEndItem& Item::asPktContentEnd() const noexcept
{
    return static_cast<const PktContentEndItem&>(*this);
}

inline const PktEndItem& Item::asPktEnd() const noexcept
{
    return static_cast<const PktEndItem&>(*this);
}

inline const PktInfoItem& Item::asPktInfo() const noexcept
{
    return static_cast<const PktInfoItem&>(*this);
}

inline const PktMagicNumberItem& Item::asPktMagicNumber() const noexcept
{
    return static_cast<const PktMagicNumberItem&>(*this);
}

inline const ScopeBeginItem& Item::asScopeBegin() const noexcept
{
    return static_cast<const ScopeBeginItem&>(*this);
}

inline const ScopeEndItem& Item::asScopeEnd() const noexcept
{
    return static_cast<const ScopeEndItem&>(*this);
}

inline const ArrayFieldBeginItem& Item::asArrayFieldBegin() const noexcept
{
    return static_cast<const ArrayFieldBeginItem&>(*this);
}

inline const ArrayFieldEndItem& Item::asArrayFieldEnd() const noexcept
{
    return static_cast<const ArrayFieldEndItem&>(*this);
}

inline const StaticLenArrayFieldBeginItem& Item::asStaticLenArrayFieldBegin() const noexcept
{
    return static_cast<const StaticLenArrayFieldBeginItem&>(*this);
}

inline const StaticLenArrayFieldEndItem& Item::asStaticLenArrayFieldEnd() const noexcept
{
    return static_cast<const StaticLenArrayFieldEndItem&>(*this);
}

inline const BlobFieldBeginItem& Item::asBlobFieldBegin() const noexcept
{
    return static_cast<const BlobFieldBeginItem&>(*this);
}

inline const BlobFieldEndItem& Item::asBlobFieldEnd() const noexcept
{
    return static_cast<const BlobFieldEndItem&>(*this);
}

inline const StaticLenBlobFieldBeginItem& Item::asStaticLenBlobFieldBegin() const noexcept
{
    return static_cast<const StaticLenBlobFieldBeginItem&>(*this);
}

inline const StaticLenBlobFieldEndItem& Item::asStaticLenBlobFieldEnd() const noexcept
{
    return static_cast<const StaticLenBlobFieldEndItem&>(*this);
}

inline const NonNullTerminatedStrFieldBeginItem&
Item::asNonNullTerminatedStrFieldBegin() const noexcept
{
    return static_cast<const NonNullTerminatedStrFieldBeginItem&>(*this);
}

inline const NonNullTerminatedStrFieldEndItem& Item::asNonNullTerminatedStrFieldEnd() const noexcept
{
    return static_cast<const NonNullTerminatedStrFieldEndItem&>(*this);
}

inline const RawDataItem& Item::asRawData() const noexcept
{
    return static_cast<const RawDataItem&>(*this);
}

inline const StaticLenStrFieldBeginItem& Item::asStaticLenStrFieldBegin() const noexcept
{
    return static_cast<const StaticLenStrFieldBeginItem&>(*this);
}

inline const StaticLenStrFieldEndItem& Item::asStaticLenStrFieldEnd() const noexcept
{
    return static_cast<const StaticLenStrFieldEndItem&>(*this);
}

inline const StructFieldBeginItem& Item::asStructFieldBegin() const noexcept
{
    return static_cast<const StructFieldBeginItem&>(*this);
}

inline const StructFieldEndItem& Item::asStructFieldEnd() const noexcept
{
    return static_cast<const StructFieldEndItem&>(*this);
}

inline const MetadataStreamUuidItem& Item::asMetadataStreamUuid() const noexcept
{
    return static_cast<const MetadataStreamUuidItem&>(*this);
}

inline const VarLenSIntFieldItem& Item::asVarLenSIntField() const noexcept
{
    return static_cast<const VarLenSIntFieldItem&>(*this);
}

inline const VarLenUIntFieldItem& Item::asVarLenUIntField() const noexcept
{
    return static_cast<const VarLenUIntFieldItem&>(*this);
}

inline const VariantFieldBeginItem& Item::asVariantFieldBegin() const noexcept
{
    return static_cast<const VariantFieldBeginItem&>(*this);
}

inline const VariantFieldEndItem& Item::asVariantFieldEnd() const noexcept
{
    return static_cast<const VariantFieldEndItem&>(*this);
}

inline const VariantFieldWithSIntSelBeginItem& Item::asVariantFieldWithSIntSelBegin() const noexcept
{
    return static_cast<const VariantFieldWithSIntSelBeginItem&>(*this);
}

inline const VariantFieldWithSIntSelEndItem& Item::asVariantFieldWithSIntSelEnd() const noexcept
{
    return static_cast<const VariantFieldWithSIntSelEndItem&>(*this);
}

inline const VariantFieldWithUIntSelBeginItem& Item::asVariantFieldWithUIntSelBegin() const noexcept
{
    return static_cast<const VariantFieldWithUIntSelBeginItem&>(*this);
}

inline const VariantFieldWithUIntSelEndItem& Item::asVariantFieldWithUIntSelEnd() const noexcept
{
    return static_cast<const VariantFieldWithUIntSelEndItem&>(*this);
}

} /* namespace ctf::src */

#endif /* BABELTRACE_PLUGINS_CTF_COMMON_SRC_ITEM_SEQ_ITEM_HPP */
