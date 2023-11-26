/*
 * Copyright (c) 2019-2020 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_SHARED_OBJECT_HPP
#define BABELTRACE_CPP_COMMON_BT2_SHARED_OBJECT_HPP

#include <utility>

#include "common/assert.h"

#include "optional-borrowed-object.hpp"

namespace bt2 {

/*
 * An instance of this class wraps an optional instance of `ObjT` and
 * manages the reference counting of the underlying libbabeltrace2
 * object.
 *
 * When you move a shared object, it becomes empty, in that operator*()
 * and operator->() will either fail to assert in debug mode or trigger
 * a segmentation fault.
 *
 * The default constructor builds an empty shared object. You may also
 * call the reset() method to make a shared object empty. Check whether
 * or not a shared object is empty with the `bool` operator.
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
class SharedObject final
{
    /*
     * This makes it possible for a
     * `SharedObject<Something, bt_something, ...>` instance to get
     * assigned an instance of
     * `SharedObject<SpecificSomething, bt_something, ...>` (copy/move
     * constructors and assignment operators), given that
     * `SpecificSomething` inherits `Something`.
     */
    template <typename, typename, typename>
    friend class SharedObject;

public:
    /*
     * Builds an empty shared object.
     */
    explicit SharedObject() noexcept
    {
    }

private:
    /*
     * Builds a shared object from `obj` without getting a reference.
     */
    explicit SharedObject(const ObjT& obj) noexcept : _mObj {obj}
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
    SharedObject(const SharedObject<OtherObjT, OtherLibObjT, RefFuncsT>& other, int) noexcept :
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
    SharedObject(SharedObject<OtherObjT, OtherLibObjT, RefFuncsT>&& other, int) noexcept :
        _mObj {other._mObj}
    {
        /* Reset moved-from object */
        other._reset();
    }

public:
    /*
     * Builds a shared object from `obj` without getting a reference.
     */
    static SharedObject createWithoutRef(const ObjT& obj) noexcept
    {
        return SharedObject {obj};
    }

    /*
     * Builds a shared object from `libObjPtr` without getting a
     * reference.
     */
    static SharedObject createWithoutRef(LibObjT * const libObjPtr) noexcept
    {
        return SharedObject::createWithoutRef(ObjT {libObjPtr});
    }

    /*
     * Builds a shared object from `obj`, immediately getting a new
     * reference.
     */
    static SharedObject createWithRef(const ObjT& obj) noexcept
    {
        SharedObject sharedObj {obj};

        sharedObj._getRef();
        return sharedObj;
    }

    /*
     * Builds a shared object from `libObjPtr`, immediately getting a
     * new reference.
     */
    static SharedObject createWithRef(LibObjT * const libObjPtr) noexcept
    {
        return SharedObject::createWithRef(ObjT {libObjPtr});
    }

    /*
     * Copy constructor.
     */
    SharedObject(const SharedObject& other) noexcept : SharedObject {other, 0}
    {
    }

    /*
     * Move constructor.
     */
    SharedObject(SharedObject&& other) noexcept : SharedObject {std::move(other), 0}
    {
    }

    /*
     * Copy assignment operator.
     */
    SharedObject& operator=(const SharedObject& other) noexcept
    {
        /* Use generic "copy" assignment operator */
        return this->operator=<ObjT, LibObjT>(other);
    }

    /*
     * Move assignment operator.
     */
    SharedObject& operator=(SharedObject&& other) noexcept
    {
        /* Use generic "move" assignment operator */
        return this->operator=<ObjT, LibObjT>(std::move(other));
    }

    /*
     * Generic "copy" constructor.
     *
     * See the `friend class SharedObject` comment above.
     */
    template <typename OtherObjT, typename OtherLibObjT>
    SharedObject(const SharedObject<OtherObjT, OtherLibObjT, RefFuncsT>& other) noexcept :
        SharedObject {other, 0}
    {
    }

    /*
     * Generic "move" constructor.
     *
     * See the `friend class SharedObject` comment above.
     */
    template <typename OtherObjT, typename OtherLibObjT>
    SharedObject(SharedObject<OtherObjT, OtherLibObjT, RefFuncsT>&& other) noexcept :
        SharedObject {std::move(other), 0}
    {
    }

    /*
     * Generic "copy" assignment operator.
     *
     * See the `friend class SharedObject` comment above.
     */
    template <typename OtherObjT, typename OtherLibObjT>
    SharedObject& operator=(const SharedObject<OtherObjT, OtherLibObjT, RefFuncsT>& other) noexcept
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
     * See the `friend class SharedObject` comment above.
     */
    template <typename OtherObjT, typename OtherLibObjT>
    SharedObject& operator=(SharedObject<OtherObjT, OtherLibObjT, RefFuncsT>&& other) noexcept
    {
        /* Put current object's reference */
        this->_putRef();

        /* Set new current object */
        _mObj = other._mObj;

        /* Reset moved-from object */
        other._reset();

        return *this;
    }

    ~SharedObject()
    {
        this->_putRef();
    }

    ObjT operator*() const noexcept
    {
        BT_ASSERT_DBG(_mObj);
        return *_mObj;
    }

    BorrowedObjectProxy<ObjT> operator->() const noexcept
    {
        BT_ASSERT_DBG(_mObj);
        return _mObj.operator->();
    }

    operator bool() const noexcept
    {
        return _mObj.hasObject();
    }

    /*
     * Makes this shared object empty.
     */
    void reset() noexcept
    {
        if (_mObj) {
            this->_putRef();
            this->_reset();
        }
    }

    /*
     * Transfers the reference of the object which this shared object
     * wrapper manages and returns it, making the caller become an
     * active owner.
     *
     * This method makes this object empty.
     */
    ObjT release() noexcept
    {
        BT_ASSERT_DBG(_mObj);

        const auto obj = *_mObj;

        this->_reset();
        return obj;
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
        RefFuncsT::get(_mObj.libObjPtr());
    }

    /*
     * Puts a reference using the configured libbabeltrace2 reference
     * decrementation function.
     */
    void _putRef() const noexcept
    {
        RefFuncsT::put(_mObj.libObjPtr());
    }

    OptionalBorrowedObject<ObjT> _mObj;
};

} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_SHARED_OBJECT_HPP */
