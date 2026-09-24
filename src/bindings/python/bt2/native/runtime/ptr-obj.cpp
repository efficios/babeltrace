/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#include <Python.h>

#include <cstdint>

#include "common/assert.h"

#include "exc.hpp"
#include "ptr-obj.hpp"
#include "utils.hpp"

namespace bt2py {
namespace {

/*
 * `PtrObj` Python object: an opaque, typed libbabeltrace2
 * object pointer.
 *
 * Doesn't own any libbabeltrace2 object reference: the Python layer
 * manages references explicitly (_get_ref() and _put_ref() methods).
 */
struct PtrObj final
{
    PyObject obBase;
    void *addr;
    const LibObjTypeTag *tag;
};

/* `PtrObj` type (heap type, created by initPtrObjType()) */
PyTypeObject *ptrObjPyTypeObj = nullptr;

/*
 * Returns the `PtrObj` object `pyObj` as its C++ structure.
 */
PtrObj& asPtrObj(PyObject& pyObj) noexcept
{
    return reinterpret_cast<PtrObj&>(pyObj);
}

/*
 * Frees the `PtrObj` object `self` (`tp_dealloc` slot).
 */
void deallocSlot(PyObject * const self) noexcept
{
    const auto type = Py_TYPE(self);

    type->tp_free(self);

    /* Release the type reference which PyType_GenericAlloc() acquired */
    Py_DECREF(type);
}

/*
 * Returns the representation of `self`: its libbabeltrace2 object type
 * and address (`tp_repr` slot).
 */
PyObject *reprSlot(PyObject * const self) noexcept
{
    const auto& ptrObj = asPtrObj(*self);

    return PyUnicode_FromFormat("<PtrObj %s * at %p>", ptrObj.tag->name, ptrObj.addr);
}

/*
 * Returns the hash of `self`, based on its address (`tp_hash` slot).
 */
Py_hash_t hashSlot(PyObject * const self) noexcept
{
    const auto hash =
        static_cast<Py_hash_t>(reinterpret_cast<std::uintptr_t>(asPtrObj(*self).addr) >> 4);

    /* -1 means "error" to the interpreter */
    return hash == -1 ? -2 : hash;
}

/*
 * Compares the addresses of `self` and `other` for equality or
 * inequality (`tp_richcompare` slot).
 */
PyObject *richCmpSlot(PyObject * const self, PyObject * const other, const int op) noexcept
{
    if ((op != Py_EQ && op != Py_NE) || Py_TYPE(other) != ptrObjPyTypeObj) {
        Py_RETURN_NOTIMPLEMENTED;
    }

    const auto eq = asPtrObj(*self).addr == asPtrObj(*other).addr;

    return &newPyObjRef(*((eq == (op == Py_EQ)) ? Py_True : Py_False));
}

/*
 * Returns the address of `self` as an integer (`nb_int` slot).
 *
 * Used by `bt2.object._BaseObject.addr`.
 */
PyObject *intSlot(PyObject * const self) noexcept
{
    return PyLong_FromVoidPtr(asPtrObj(*self).addr);
}

/*
 * Explicitly refuses to create a `PtrObj` object from Python code
 * (`tp_new` slot).
 *
 * `Py_TPFLAGS_DISALLOW_INSTANTIATION` only exists for Python 3.10+.
 */
PyObject *newSlot(PyTypeObject *, PyObject *, PyObject *) noexcept
{
    PyErr_SetString(PyExc_TypeError, "Cannot create `PtrObj` objects");
    return nullptr;
}

PyType_Slot ptrObjTypeSlots[] = {
    {Py_tp_dealloc, reinterpret_cast<void *>(deallocSlot)},
    {Py_tp_repr, reinterpret_cast<void *>(reprSlot)},
    {Py_tp_hash, reinterpret_cast<void *>(hashSlot)},
    {Py_tp_richcompare, reinterpret_cast<void *>(richCmpSlot)},
    {Py_tp_new, reinterpret_cast<void *>(newSlot)},
    {Py_nb_int, reinterpret_cast<void *>(intSlot)},
    {0, nullptr},
};

PyType_Spec ptrObjTypeSpec = {
    "bt2.native_bt.PtrObj", sizeof(PtrObj), 0, Py_TPFLAGS_DEFAULT, ptrObjTypeSlots,
};

} /* namespace */

/*
 * Creates the `PtrObj` Python type.
 */
void initPtrObjType()
{
    BT_ASSERT(!ptrObjPyTypeObj);

    /*
     * PyType_FromSpec() instead of a static `PyTypeObject`: the layout
     * of `PyTypeObject` changed within the supported Python versions.
     */
    ptrObjPyTypeObj =
        reinterpret_cast<PyTypeObject *>(&throwIfPyErr(PyType_FromSpec(&ptrObjTypeSpec)));
}

/*
 * Returns the `PtrObj` Python type.
 */
PyTypeObject& ptrObjPyType() noexcept
{
    BT_ASSERT_DBG(ptrObjPyTypeObj);
    return *ptrObjPyTypeObj;
}

/*
 * Creates a `PtrObj` object holding `addr` with the type tag `tag`,
 * or returns `None` if `addr` is `nullptr`.
 */
PyObject& createPtrObj(void * const addr, const LibObjTypeTag& tag)
{
    if (!addr) {
        return newPyNoneRef();
    }

    BT_ASSERT_DBG(ptrObjPyTypeObj);

    auto& pyObj = throwIfPyErr(PyType_GenericAlloc(ptrObjPyTypeObj, 0));
    auto& ptrObj = asPtrObj(pyObj);

    ptrObj.addr = addr;
    ptrObj.tag = &tag;
    return pyObj;
}

/*
 * Returns the address which the `PtrObj` object `pyObj` holds.
 */
void *ptrObjAddr(PyObject& pyObj, const LibObjTypeTag& tag) noexcept
{
    if (&pyObj == Py_None) {
        return nullptr;
    }

    BT_ASSERT_DBG(Py_TYPE(&pyObj) == ptrObjPyTypeObj);

    const auto& ptrObj = asPtrObj(pyObj);

    BT_ASSERT_DBG(ptrObj.tag == &tag);
    return ptrObj.addr;
}

} /* namespace bt2py */
