/*
 * Copyright (c) 2019-2020 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_INTERNAL_SHARED_OBJ_HPP
#define BABELTRACE_CPP_COMMON_BT2_INTERNAL_SHARED_OBJ_HPP

#include "common/assert.h"
#include "cpp-common/optional.hpp"

namespace bt2 {
namespace internal {

/*
 * An instance of this class wraps an optional instance of `ObjT` and
 * manages the reference counting of the underlying libbabeltrace2
 * object.
 *
 * When you move a shared object, it becomes invalid, in that
 * operator*() and operator->() will either fail to assert in debug mode
 * or trigger a segmentation fault.
 *
 * `LibObjT` is the direct libbabeltrace2 object type, for example
 * `bt_stream_class` or `const bt_value`.
 *
 * RefFuncsT::get() must accept a `const LibObjT *` value and increment
 * its reference count.
 *
 * RefFuncsT::put() must accept a `const LibObjT *` value and decrement
 * its reference count.
 */
template <typename ObjT, typename LibObjT, typename RefFuncsT>
class SharedObj final
{
    /*
     * This makes it possible for a
     * `SharedObj<Something, bt_something, ...>` instance to get
     * assigned an instance of
     * `SharedObj<SpecificSomething, bt_something, ...>` (copy/move
     * constructors and assignment operators), given that
     * `SpecificSomething` inherits `Something`.
     */
    template <typename AnyObjT, typename AnyLibObjT, typename AnyRefFuncsT>
    friend class SharedObj;

private:
    /*
     * Builds a shared object from `obj` without getting a reference.
     */
    explicit SharedObj(const ObjT& obj) noexcept : _mObj {obj}
    {
    }

    /*
     * Common generic "copy" constructor.
     *
     * This constructor is meant to be delegated to by the copy
     * constructor and the generic "copy" constructor.
     *
     * The second parameter, of type `int`, makes it possible to
     * delegate by deduction as you can't explicit the template
     * parameters when delegating to a constructor template.
     */
    template <typename OtherObjT, typename OtherLibObjT>
    SharedObj(const SharedObj<OtherObjT, OtherLibObjT, RefFuncsT>& other, int) noexcept :
        _mObj {other._mObj}
    {
        this->_getRef();
    }

    /*
     * Common generic "move" constructor.
     *
     * See the comment of the common generic "copy" constructor above.
     */
    template <typename OtherObjT, typename OtherLibObjT>
    SharedObj(SharedObj<OtherObjT, OtherLibObjT, RefFuncsT>&& other, int) noexcept :
        _mObj {other._mObj}
    {
        /* Reset moved-from object */
        other._reset();
    }

public:
    /*
     * Builds a shared object from `obj` without getting a reference.
     */
    static SharedObj createWithoutRef(const ObjT& obj) noexcept
    {
        return SharedObj {obj};
    }

    /*
     * Builds a shared object from `libObjPtr` without getting a
     * reference.
     */
    static SharedObj createWithoutRef(LibObjT * const libObjPtr) noexcept
    {
        return SharedObj::createWithoutRef(ObjT {libObjPtr});
    }

    /*
     * Builds a shared object from `obj`, immediately getting a new
     * reference.
     */
    static SharedObj createWithRef(const ObjT& obj) noexcept
    {
        SharedObj sharedObj {obj};

        sharedObj._getRef();
        return sharedObj;
    }

    /*
     * Builds a shared object from `libObjPtr`, immediately getting a new
     * reference.
     */
    static SharedObj createWithRef(LibObjT * const libObjPtr) noexcept
    {
        return SharedObj::createWithRef(ObjT {libObjPtr});
    }

    /*
     * Copy constructor.
     */
    SharedObj(const SharedObj& other) noexcept : SharedObj {other, 0}
    {
    }

    /*
     * Move constructor.
     */
    SharedObj(SharedObj&& other) noexcept : SharedObj {std::move(other), 0}
    {
    }

    /*
     * Copy assignment operator.
     */
    SharedObj& operator=(const SharedObj& other) noexcept
    {
        /* Use generic "copy" assignment operator */
        return this->operator=<ObjT, LibObjT>(other);
    }

    /*
     * Move assignment operator.
     */
    SharedObj& operator=(SharedObj&& other) noexcept
    {
        /* Use generic "move" assignment operator */
        return this->operator=<ObjT, LibObjT>(std::move(other));
    }

    /*
     * Generic "copy" constructor.
     *
     * See the `friend class SharedObj` comment above.
     */
    template <typename OtherObjT, typename OtherLibObjT>
    SharedObj(const SharedObj<OtherObjT, OtherLibObjT, RefFuncsT>& other) noexcept :
        SharedObj {other, 0}
    {
    }

    /*
     * Generic "move" constructor.
     *
     * See the `friend class SharedObj` comment above.
     */
    template <typename OtherObjT, typename OtherLibObjT>
    SharedObj(SharedObj<OtherObjT, OtherLibObjT, RefFuncsT>&& other) noexcept :
        SharedObj {std::move(other), 0}
    {
    }

    /*
     * Generic "copy" assignment operator.
     *
     * See the `friend class SharedObj` comment above.
     */
    template <typename OtherObjT, typename OtherLibObjT>
    SharedObj& operator=(const SharedObj<OtherObjT, OtherLibObjT, RefFuncsT>& other) noexcept
    {
        /* Put current object's reference */
        this->_putRef();

        /* Set new current object and get a reference */
        _mObj = other._mObj;
        this->_getRef();

        return *this;
    }

    /*
     * Generic "move" assignment operator.
     *
     * See the `friend class SharedObj` comment above.
     */
    template <typename OtherObjT, typename OtherLibObjT>
    SharedObj& operator=(SharedObj<OtherObjT, OtherLibObjT, RefFuncsT>&& other) noexcept
    {
        /* Put current object's reference */
        this->_putRef();

        /* Set new current object */
        _mObj = other._mObj;

        /* Reset moved-from object */
        other._reset();

        return *this;
    }

    ~SharedObj()
    {
        this->_putRef();
    }

    ObjT& operator*() noexcept
    {
        BT_ASSERT_DBG(_mObj);
        return *_mObj;
    }

    const ObjT& operator*() const noexcept
    {
        BT_ASSERT_DBG(_mObj);
        return *_mObj;
    }

    ObjT *operator->() noexcept
    {
        BT_ASSERT_DBG(_mObj);
        return &*_mObj;
    }

    const ObjT *operator->() const noexcept
    {
        BT_ASSERT_DBG(_mObj);
        return &*_mObj;
    }

private:
    /*
     * Resets this shared object.
     *
     * To be used when moving it.
     */
    void _reset() noexcept
    {
        _mObj.reset();
    }

    /*
     * Gets a new reference using the configured libbabeltrace2
     * reference incrementation function.
     */
    void _getRef() const noexcept
    {
        if (_mObj) {
            RefFuncsT::get(_mObj->libObjPtr());
        }
    }

    /*
     * Puts a reference using the configured libbabeltrace2 reference
     * decrementation function.
     */
    void _putRef() const noexcept
    {
        if (_mObj) {
            RefFuncsT::put(_mObj->libObjPtr());
        }
    }

    nonstd::optional<ObjT> _mObj;
};

} /* namespace internal */
} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_INTERNAL_SHARED_OBJ_HPP */
