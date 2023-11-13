/*
 * Copyright 2019-2020 (c) Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_INTERNAL_BORROWED_OBJ_HPP
#define BABELTRACE_CPP_COMMON_BT2_INTERNAL_BORROWED_OBJ_HPP

#include <functional>
#include <type_traits>

#include "common/assert.h"

namespace bt2 {
namespace internal {

/*
 * An instance of this class wraps a pointer to a libbabeltrace2 object
 * of type `LibObjT` without managing any reference counting.
 *
 * This is an abstract base class for any libbabeltrace2 object wrapper.
 *
 * `LibObjT` is the direct libbabeltrace2 object type, for example
 * `bt_stream_class` or `const bt_value`.
 *
 * Methods of a derived class can call libObjPtr() to access the
 * libbabeltrace2 object pointer.
 */
template <typename LibObjT>
class BorrowedObj
{
    static_assert(!std::is_pointer<LibObjT>::value, "`LibObjT` must not be a pointer");

    /*
     * This makes it possible for a `BorrowedObj<const bt_something>`
     * instance to get assigned an instance of
     * `BorrowedObj<bt_something>` ("copy" constructor and "assignment"
     * operator).
     *
     * C++ forbids the other way around.
     */
    template <typename>
    friend class BorrowedObj;

private:
    /*
     * Provides `val` which indicates whether or not you can assign this
     * object from a borrowed object of type `OtherLibObjT`.
     */
    template <typename OtherLibObjT>
    struct _AssignableFromConst final
    {
        /*
         * If `LibObjT` is const (for example, `const bt_value`), then
         * you may always assign from its non-const equivalent (for
         * example, `bt_value`). In C (correct):
         *
         *     bt_value * const meow = bt_value_bool_create_init(BT_TRUE);
         *     const bt_value * const mix = meow;
         *
         * If `LibObjT` is non-const, then you may not assign from its
         * const equivalent. In C (not correct):
         *
         *     const bt_value * const meow =
         *         bt_value_array_borrow_element_by_index_const(some_val, 17);
         *     bt_value * const mix = meow;
         */
        static constexpr bool val =
            std::is_const<LibObjT>::value || !std::is_const<OtherLibObjT>::value;
    };

protected:
    /* libbabeltrace2 object pointer */
    using _LibObjPtr = LibObjT *;

    /* This complete borrowed object */
    using _ThisBorrowedObj = BorrowedObj<LibObjT>;

    /*
     * Builds a borrowed object to wrap the libbabeltrace2 object
     * pointer `libObjPtr`.
     *
     * `libObjPtr` must not be `nullptr`.
     */
    explicit BorrowedObj(const _LibObjPtr libObjPtr) noexcept : _mLibObjPtr {libObjPtr}
    {
        BT_ASSERT(libObjPtr);
    }

    /* Default copy operations */
    BorrowedObj(const BorrowedObj&) noexcept = default;
    BorrowedObj& operator=(const BorrowedObj&) noexcept = default;

    /*
     * Generic "copy" constructor.
     *
     * This converting constructor accepts both an instance of
     * `_ThisBorrowedObj` and an instance (`other`) of
     * `BorrowedObj<ConstLibObjT>`, where `ConstLibObjT` is the `const`
     * version of `LibObjT`, if applicable.
     *
     * This makes it possible for a `BorrowedObj<const bt_something>`
     * instance to be built from an instance of
     * `BorrowedObj<bt_something>`. C++ forbids the other way around.
     */
    template <typename OtherLibObjT>
    BorrowedObj(const BorrowedObj<OtherLibObjT>& other) noexcept : BorrowedObj {other._mLibObjPtr}
    {
        static_assert(_AssignableFromConst<OtherLibObjT>::val,
                      "Don't assign a non-const wrapper from a const wrapper.");
    }

    /*
     * Generic "assignment" operator.
     *
     * This operator accepts both an instance of
     * `_ThisBorrowedObj` and an instance (`other`) of
     * `BorrowedObj<ConstLibObjT>`, where `ConstLibObjT` is the `const`
     * version of `LibObjT`, if applicable.
     *
     * This makes it possible for a `BorrowedObj<const bt_something>`
     * instance to get assigned an instance of
     * `BorrowedObj<bt_something>`. C++ forbids the other way around,
     * therefore we use `_EnableIfAssignableT` to show a more relevant
     * context in the compiler error message.
     */
    template <typename OtherLibObjT>
    _ThisBorrowedObj& operator=(const BorrowedObj<OtherLibObjT>& other) noexcept
    {
        static_assert(_AssignableFromConst<OtherLibObjT>::val,
                      "Don't assign a non-const wrapper from a const wrapper.");

        _mLibObjPtr = other._mLibObjPtr;
        return *this;
    }

public:
    /*
     * Returns a hash of this object, solely based on its raw libbabeltrace2
     * pointer.
     */
    std::size_t hash() const noexcept
    {
        return std::hash<_LibObjPtr> {}(_mLibObjPtr);
    }

    /*
     * Returns whether or not this object is the exact same as `other`,
     * solely based on the raw libbabeltrace2 pointers.
     */
    bool isSame(const _ThisBorrowedObj& other) const noexcept
    {
        return _mLibObjPtr == other._mLibObjPtr;
    }

    /* Wrapped libbabeltrace2 object pointer */
    _LibObjPtr libObjPtr() const noexcept
    {
        return _mLibObjPtr;
    }

private:
    _LibObjPtr _mLibObjPtr;
};

} /* namespace internal */
} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_INTERNAL_BORROWED_OBJ_HPP */
