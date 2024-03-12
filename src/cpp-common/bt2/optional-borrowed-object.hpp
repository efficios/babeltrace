/*
 * Copyright (c) 2023 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_OPTIONAL_BORROWED_OBJECT_HPP
#define BABELTRACE_CPP_COMMON_BT2_OPTIONAL_BORROWED_OBJECT_HPP

#include <type_traits>

#include "borrowed-object-proxy.hpp"

namespace bt2 {

/*
 * An instance of this class template manages an optional contained
 * borrowed object of type `ObjT`, that is, a borrowed object that may
 * or may not be present.
 *
 * Such an object considers that a `nullptr` libbabeltrace2 object
 * pointer means none. Therefore, using an `OptionalBorrowedObject`
 * isn't more costly, in time and space, as using a libbabeltrace2
 * object pointer in C, but offers the typical C++ optional interface.
 *
 * There's no `bt2s::nullopt` equivalent: just use the default
 * constructor or call reset().
 *
 * There are a constructors an assignment operators which accept an
 * instance of another wrapper object or of another optional borrowed
 * object. For those, static assertions make sure that the assignment
 * would work with borrowed objects, for example:
 *
 *     auto sharedIntVal = createValue(23L);
 *
 *     OptionalBorrowedObject<Value> myVal {*sharedIntVal};
 *
 * This is needed because `OptionalBorrowedObject` only keeps a
 * libbabeltrace2 pointer (`_mLibObjPtr`), therefore it doesn't
 * automatically know the relation between wrapper classes.
 */
template <typename ObjT>
class OptionalBorrowedObject final
{
public:
    using Obj = ObjT;
    using LibObjPtr = typename ObjT::LibObjPtr;

    /*
     * Builds an optional borrowed object without an object.
     *
     * Intentionally not explicit.
     */
    OptionalBorrowedObject() noexcept = default;

    /*
     * Builds an optional borrowed object with an instance of `ObjT`
     * wrapping the libbabeltrace2 pointer `libObjPtr`.
     *
     * Intentionally not explicit.
     */
    OptionalBorrowedObject(const LibObjPtr libObjPtr) noexcept : _mLibObjPtr {libObjPtr}
    {
    }

    /*
     * Builds an optional borrowed object with an instance of `ObjT`
     * constructed from `obj`.
     *
     * It must be possible to construct an instance of `ObjT` with an
     * instance of `OtherObjT`.
     *
     * Intentionally not explicit.
     */
    template <typename OtherObjT>
    OptionalBorrowedObject(const OtherObjT obj) noexcept : _mLibObjPtr {obj.libObjPtr()}
    {
        static_assert(std::is_constructible<ObjT, OtherObjT>::value,
                      "`ObjT` is constructible with an instance of `OtherObjT`.");
    }

    /*
     * Builds an optional borrowed object from `optObj`, with or without
     * an object.
     *
     * It must be possible to construct an instance of `ObjT` with an
     * instance of `OtherObjT`.
     *
     * Intentionally not explicit.
     */
    template <typename OtherObjT>
    OptionalBorrowedObject(const OptionalBorrowedObject<OtherObjT> optObj) noexcept :
        _mLibObjPtr {optObj.libObjPtr()}
    {
        static_assert(std::is_constructible<ObjT, OtherObjT>::value,
                      "`ObjT` is constructible with an instance of `OtherObjT`.");
    }

    /*
     * Makes this optional borrowed object have an instance of `ObjT`
     * wrapping the libbabeltrace2 pointer `libObjPtr`.
     */
    OptionalBorrowedObject& operator=(const LibObjPtr libObjPtr) noexcept
    {
        _mLibObjPtr = libObjPtr;
        return *this;
    }

    /*
     * Makes this optional borrowed object have an instance of `ObjT`
     * constructed from `obj`.
     *
     * It must be possible to construct an instance of `ObjT` with an
     * instance of `OtherObjT`.
     */
    template <typename OtherObjT>
    OptionalBorrowedObject& operator=(const ObjT obj) noexcept
    {
        static_assert(std::is_constructible<ObjT, OtherObjT>::value,
                      "`ObjT` is constructible with an instance of `OtherObjT`.");
        _mLibObjPtr = obj.libObjPtr();
        return *this;
    }

    /*
     * Sets this optional borrowed object to `optObj`.
     *
     * It must be possible to construct an instance of `ObjT` with an
     * instance of `OtherObjT`.
     */
    template <typename OtherObjT>
    OptionalBorrowedObject& operator=(const OptionalBorrowedObject<ObjT> optObj) noexcept
    {
        static_assert(std::is_constructible<ObjT, OtherObjT>::value,
                      "`ObjT` is constructible with an instance of `OtherObjT`.");
        _mLibObjPtr = optObj.libObjPtr();
        return *this;
    }

    /* Wrapped libbabeltrace2 object pointer (may be `nullptr`) */
    LibObjPtr libObjPtr() const noexcept
    {
        return _mLibObjPtr;
    }

    ObjT object() const noexcept
    {
        return ObjT {_mLibObjPtr};
    }

    ObjT operator*() const noexcept
    {
        return this->object();
    }

    /*
     * We want to return the address of an `ObjT` instance here, but we
     * only have a library pointer (`_mLibObjPtr`) because an `ObjT`
     * instance may not wrap `nullptr`.
     *
     * Therefore, return a proxy object which holds an internal
     * `ObjT` instance and implements operator->() itself.
     *
     * In other words, doing `myOptObj->something()` is equivalent to
     * calling `myOptObj.operator->().operator->()->something()`.
     */
    BorrowedObjectProxy<ObjT> operator->() const noexcept
    {
        return BorrowedObjectProxy<ObjT> {_mLibObjPtr};
    }

    bool hasObject() const noexcept
    {
        return _mLibObjPtr;
    }

    explicit operator bool() const noexcept
    {
        return this->hasObject();
    }

    void reset() noexcept
    {
        _mLibObjPtr = nullptr;
    }

private:
    typename ObjT::LibObjPtr _mLibObjPtr = nullptr;
};

} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_OPTIONAL_BORROWED_OBJECT_HPP */
