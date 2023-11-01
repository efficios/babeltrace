/*
 * Copyright (c) 2020 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_FIELD_PATH_HPP
#define BABELTRACE_CPP_COMMON_BT2_FIELD_PATH_HPP

#include <cstdint>

#include <babeltrace2/babeltrace.h>

#include "common/assert.h"

#include "borrowed-object.hpp"
#include "common-iterator.hpp"
#include "shared-object.hpp"

namespace bt2 {

class ConstIndexFieldPathItem;

enum class FieldPathItemType
{
    INDEX = BT_FIELD_PATH_ITEM_TYPE_INDEX,
    CURRENT_ARRAY_ELEMENT = BT_FIELD_PATH_ITEM_TYPE_CURRENT_ARRAY_ELEMENT,
    CURRENT_OPTION_CONTENT = BT_FIELD_PATH_ITEM_TYPE_CURRENT_OPTION_CONTENT,
};

class ConstFieldPathItem : public BorrowedObject<const bt_field_path_item>
{
public:
    explicit ConstFieldPathItem(const _LibObjPtr libObjPtr) noexcept :
        _ThisBorrowedObject {libObjPtr}
    {
    }

    ConstFieldPathItem(const ConstFieldPathItem& fpItem) noexcept : _ThisBorrowedObject {fpItem}
    {
    }

    ConstFieldPathItem& operator=(const ConstFieldPathItem& fpItem) noexcept
    {
        _ThisBorrowedObject::operator=(fpItem);
        return *this;
    }

    FieldPathItemType type() const noexcept
    {
        return static_cast<FieldPathItemType>(this->_libType());
    }

    bool isIndex() const noexcept
    {
        return this->_libType() == BT_FIELD_PATH_ITEM_TYPE_INDEX;
    }

    bool isCurrentArrayElement() const noexcept
    {
        return this->_libType() == BT_FIELD_PATH_ITEM_TYPE_CURRENT_ARRAY_ELEMENT;
    }

    bool isCurrentOptionContent() const noexcept
    {
        return this->_libType() == BT_FIELD_PATH_ITEM_TYPE_CURRENT_OPTION_CONTENT;
    }

    ConstIndexFieldPathItem asIndex() const noexcept;

private:
    bt_field_path_item_type _libType() const noexcept
    {
        return bt_field_path_item_get_type(this->libObjPtr());
    }
};

class ConstIndexFieldPathItem final : public ConstFieldPathItem
{
public:
    explicit ConstIndexFieldPathItem(const _LibObjPtr libObjPtr) noexcept :
        ConstFieldPathItem {libObjPtr}
    {
        BT_ASSERT_DBG(this->isIndex());
    }

    ConstIndexFieldPathItem(const ConstIndexFieldPathItem& fpItem) noexcept :
        ConstFieldPathItem {fpItem}
    {
    }

    ConstIndexFieldPathItem& operator=(const ConstIndexFieldPathItem& fpItem) noexcept
    {
        ConstFieldPathItem::operator=(fpItem);
        return *this;
    }

    std::uint64_t index() const noexcept
    {
        return bt_field_path_item_index_get_index(this->libObjPtr());
    }
};

inline ConstIndexFieldPathItem ConstFieldPathItem::asIndex() const noexcept
{
    BT_ASSERT_DBG(this->isIndex());
    return ConstIndexFieldPathItem {this->libObjPtr()};
}

namespace internal {

struct FieldPathRefFuncs final
{
    static void get(const bt_field_path * const libObjPtr) noexcept
    {
        bt_field_path_get_ref(libObjPtr);
    }

    static void put(const bt_field_path * const libObjPtr) noexcept
    {
        bt_field_path_put_ref(libObjPtr);
    }
};

} /* namespace internal */

class ConstFieldPath final : public BorrowedObject<const bt_field_path>
{
public:
    using Shared = SharedObject<ConstFieldPath, const bt_field_path, internal::FieldPathRefFuncs>;

    using Iterator = CommonIterator<ConstFieldPath, ConstFieldPathItem>;

    enum class Scope
    {
        PACKET_CONTEXT = BT_FIELD_PATH_SCOPE_PACKET_CONTEXT,
        EVENT_COMMON_CONTEXT = BT_FIELD_PATH_SCOPE_EVENT_COMMON_CONTEXT,
        EVENT_SPECIFIC_CONTEXT = BT_FIELD_PATH_SCOPE_EVENT_SPECIFIC_CONTEXT,
        EVENT_PAYLOAD = BT_FIELD_PATH_SCOPE_EVENT_PAYLOAD,
    };

    explicit ConstFieldPath(const _LibObjPtr libObjPtr) noexcept : _ThisBorrowedObject {libObjPtr}
    {
    }

    ConstFieldPath(const ConstFieldPath& fieldPath) noexcept : _ThisBorrowedObject {fieldPath}
    {
    }

    ConstFieldPath& operator=(const ConstFieldPath& fieldPath) noexcept
    {
        _ThisBorrowedObject::operator=(fieldPath);
        return *this;
    }

    Scope rootScope() const noexcept
    {
        return static_cast<Scope>(bt_field_path_get_root_scope(this->libObjPtr()));
    }

    std::uint64_t size() const noexcept
    {
        return bt_field_path_get_item_count(this->libObjPtr());
    }

    ConstFieldPathItem operator[](const std::uint64_t index) const noexcept
    {
        return ConstFieldPathItem {
            bt_field_path_borrow_item_by_index_const(this->libObjPtr(), index)};
    }

    Iterator begin() const noexcept
    {
        return Iterator {*this, 0};
    }

    Iterator end() const noexcept
    {
        return Iterator {*this, this->size()};
    }

    Shared shared() const noexcept
    {
        return Shared::createWithRef(*this);
    }
};

} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_FIELD_PATH_HPP */
