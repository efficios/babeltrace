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
#include "internal/borrowed-obj.hpp"

namespace bt2 {

class ConstIndexFieldPathItem;

enum class FieldPathItemType
{
    INDEX = BT_FIELD_PATH_ITEM_TYPE_INDEX,
    CURRENT_ARRAY_ELEMENT = BT_FIELD_PATH_ITEM_TYPE_CURRENT_ARRAY_ELEMENT,
    CURRENT_OPTION_CONTENT = BT_FIELD_PATH_ITEM_TYPE_CURRENT_OPTION_CONTENT,
};

class ConstFieldPathItem : public internal::BorrowedObj<const bt_field_path_item>
{
public:
    explicit ConstFieldPathItem(const _LibObjPtr libObjPtr) noexcept : _ThisBorrowedObj {libObjPtr}
    {
    }

    ConstFieldPathItem(const ConstFieldPathItem& fpItem) noexcept : _ThisBorrowedObj {fpItem}
    {
    }

    ConstFieldPathItem& operator=(const ConstFieldPathItem& fpItem) noexcept
    {
        _ThisBorrowedObj::operator=(fpItem);
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
        return bt_field_path_item_get_type(this->_libObjPtr());
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
        return bt_field_path_item_index_get_index(this->_libObjPtr());
    }
};

inline ConstIndexFieldPathItem ConstFieldPathItem::asIndex() const noexcept
{
    BT_ASSERT_DBG(this->isIndex());
    return ConstIndexFieldPathItem {this->_libObjPtr()};
}

namespace internal {

struct FieldPathRefFuncs final
{
    static void get(const bt_field_path * const libObjPtr)
    {
        bt_field_path_get_ref(libObjPtr);
    }

    static void put(const bt_field_path * const libObjPtr)
    {
        bt_field_path_put_ref(libObjPtr);
    }
};

} // namespace internal

class ConstFieldPath final : public internal::BorrowedObj<const bt_field_path>
{
public:
    using Shared =
        internal::SharedObj<ConstFieldPath, const bt_field_path, internal::FieldPathRefFuncs>;

    enum class Scope
    {
        PACKET_CONTEXT = BT_FIELD_PATH_SCOPE_PACKET_CONTEXT,
        EVENT_COMMON_CONTEXT = BT_FIELD_PATH_SCOPE_EVENT_COMMON_CONTEXT,
        EVENT_SPECIFIC_CONTEXT = BT_FIELD_PATH_SCOPE_EVENT_SPECIFIC_CONTEXT,
        EVENT_PAYLOAD = BT_FIELD_PATH_SCOPE_EVENT_PAYLOAD,
    };

    explicit ConstFieldPath(const _LibObjPtr libObjPtr) noexcept : _ThisBorrowedObj {libObjPtr}
    {
    }

    ConstFieldPath(const ConstFieldPath& clkSnapshot) noexcept : _ThisBorrowedObj {clkSnapshot}
    {
    }

    ConstFieldPath& operator=(const ConstFieldPath& clkSnapshot) noexcept
    {
        _ThisBorrowedObj::operator=(clkSnapshot);
        return *this;
    }

    Scope rootScope() const noexcept
    {
        return static_cast<Scope>(bt_field_path_get_root_scope(this->_libObjPtr()));
    }

    std::uint64_t size() const noexcept
    {
        return bt_field_path_get_item_count(this->_libObjPtr());
    }

    ConstFieldPathItem operator[](const std::uint64_t index) const noexcept
    {
        return ConstFieldPathItem {
            bt_field_path_borrow_item_by_index_const(this->_libObjPtr(), index)};
    }

    Shared shared() const noexcept
    {
        return Shared {*this};
    }
};

} // namespace bt2

#endif // BABELTRACE_CPP_COMMON_BT2_FIELD_PATH_HPP
