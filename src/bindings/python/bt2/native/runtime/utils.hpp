/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#ifndef BABELTRACE_BINDINGS_PYTHON_BT2_NATIVE_RUNTIME_UTILS_HPP
#define BABELTRACE_BINDINGS_PYTHON_BT2_NATIVE_RUNTIME_UTILS_HPP

#include <Python.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "logging.hpp"

#include "common/assert.h"
#include "cpp-common/bt2c/logging.hpp"

#include "exc.hpp"

namespace bt2py {

/*
 * Returns a new reference to `obj`.
 *
 * Equivalent of Py_NewRef() (Python 3.10+).
 */
inline PyObject& newPyObjRef(PyObject& obj) noexcept
{
    Py_INCREF(&obj);
    return obj;
}

/*
 * Adds `obj` to the Python module `mod` as `name`, never stealing a
 * reference to `obj`.
 *
 * Equivalent of PyModule_AddObjectRef() (Python 3.10+): the
 * PyModule_AddObject() of older versions only steals the reference
 * on success.
 */
inline int pyModAddObjectRef(PyObject& mod, const char * const name, PyObject& obj) noexcept
{
#if PY_VERSION_HEX >= 0x030a0000
    return PyModule_AddObjectRef(&mod, name, &obj);
#else
    Py_INCREF(&obj);

    if (PyModule_AddObject(&mod, name, &obj) < 0) {
        Py_DECREF(&obj);
        return -1;
    }

    return 0;
#endif
}

namespace internal {

struct PyObjDeleter final
{
    void operator()(PyObject * const obj) const noexcept
    {
        Py_XDECREF(obj);
    }
};

} /* namespace internal */

/*
 * Owned Python object reference.
 */
using PyObjUP = std::unique_ptr<PyObject, internal::PyObjDeleter>;

/*
 * Returns an owned reference to `pyObj`, throwing `PyErrSet` if it's
 * `nullptr` (the Python API function which returned it set the Python
 * error indicator).
 *
 * `pyObj` must be a _new_ reference.
 */
inline PyObjUP ownPyObjOrThrow(PyObject * const pyObj)
{
    return PyObjUP {&throwIfPyErr(pyObj)};
}

/*
 * Returns a logger for the module named `moduleName`.
 *
 * The logger has the `BT2-NATIVE` tag and the current logging level of
 * the Python bindings.
 */
inline bt2c::Logger createLogger(std::string moduleName)
{
    return bt2c::Logger {std::move(moduleName), "BT2-NATIVE",
                         static_cast<bt2c::Logger::Level>(bt_python_bindings_bt2_log_level)};
}

/*
 * Returns a new owned reference to the borrowed `obj`.
 */
inline PyObjUP newPyObjUp(PyObject& obj) noexcept
{
    return PyObjUP {&newPyObjRef(obj)};
}

/*
 * Returns a new reference to `None`.
 */
inline PyObject& newPyNoneRef() noexcept
{
    return newPyObjRef(*Py_None);
}

/*
 * Returns a new owned reference to the Python object which is the
 * libbabeltrace2 object user data `data`.
 *
 * `data` must not be `nullptr`.
 */
inline PyObjUP pyObjFromUserData(void * const data) noexcept
{
    BT_ASSERT_DBG(data);
    return newPyObjUp(*static_cast<PyObject *>(data));
}

/*
 * Returns a new Python list of `len` items, `createItemFunc(i)`
 * returning a new reference to the item at the index `i`.
 *
 * `createItemFunc` receives an `std::uint64_t` index and returns
 * `PyObject&`, throwing on error.
 *
 * Throws `PyErrSet` on error.
 */
template <typename CreateItemFuncT>
PyObjUP createPyList(const std::uint64_t len, CreateItemFuncT&& createItemFunc)
{
    static_assert(std::is_same_v<std::invoke_result_t<CreateItemFuncT&, std::uint64_t>, PyObject&>,
                  "`createItemFunc` returns `PyObject&`.");

    auto pyList = ownPyObjOrThrow(PyList_New(static_cast<Py_ssize_t>(len)));

    /*
     * On error, the remaining items of `pyList` are `nullptr`, which
     * the list deallocator handles.
     */
    for (std::uint64_t i = 0; i < len; ++i) {
        /* Steals the returned reference */
        PyList_SET_ITEM(pyList.get(), static_cast<Py_ssize_t>(i), &createItemFunc(i));
    }

    return pyList;
}

/*
 * Returns a new Python list of the `count` strings of `strs`.
 *
 * Throws `PyErrSet` on error.
 */
inline PyObjUP pyStrListFromCStrs(const char * const * const strs, const std::uint64_t count)
{
    return createPyList(count, [strs](const auto index) -> PyObject& {
        return throwIfPyErr(PyUnicode_FromString(strs[index]));
    });
}

} /* namespace bt2py */

#endif /* BABELTRACE_BINDINGS_PYTHON_BT2_NATIVE_RUNTIME_UTILS_HPP */
