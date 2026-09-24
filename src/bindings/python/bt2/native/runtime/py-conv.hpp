/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#ifndef BABELTRACE_BINDINGS_PYTHON_BT2_NATIVE_RUNTIME_PY_CONV_HPP
#define BABELTRACE_BINDINGS_PYTHON_BT2_NATIVE_RUNTIME_PY_CONV_HPP

#include <Python.h>

#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "common/assert.h"
#include "cpp-common/bt2/borrowed-object.hpp"
#include "cpp-common/bt2/optional-borrowed-object.hpp"
#include "cpp-common/bt2/shared-object.hpp"
#include "cpp-common/bt2c/c-string-view.hpp"
#include "cpp-common/bt2c/uuid.hpp"

#include "exc.hpp"
#include "ptr-obj.hpp"
#include "utils.hpp"

namespace bt2py {

/*
 * Converter between the C++ type `T` and Python objects.
 *
 * Each specialization offers one or both of:
 *
 * fromPy():
 *     Converts a borrowed Python object to a `T` value.
 *
 *     The Python layer is trusted: fromPy() asserts that the Python
 *     object has the expected type and range instead of raising a
 *     Python exception. It only throws `PyErrSet` on a genuine run-time
 *     error (a string which isn't UTF-8-encodable, for example).
 *
 * toPy():
 *     Converts a `T` value to a new Python object reference.
 *
 *     Throws `PyErrSet` on error.
 *
 * The Python function adapter (see `py-module-builder.hpp`) uses those to
 * convert the parameters and the return value of an adapted function.
 */
template <typename T, typename = void>
struct PyConv;

template <>
struct PyConv<bool> final
{
    static bool fromPy(PyObject& pyObj) noexcept
    {
        BT_ASSERT_DBG(PyBool_Check(&pyObj));
        return &pyObj == Py_True;
    }

    static PyObject& toPy(const bool val) noexcept
    {
        /* Never fails */
        return *PyBool_FromLong(val);
    }
};

namespace internal {

/*
 * Whether or not `T` is an integer type which the integer converters
 * below handle (any integral type except `bool`).
 */
template <typename T>
inline constexpr bool isInt = std::is_integral_v<T> && !std::is_same_v<T, bool>;

} /* namespace internal */

/*
 * Python's `int` to/from any signed integer type, notably `int`
 * for `bt_bool`.
 *
 * Returning a `bt_bool` value therefore makes a Python `int`: add a
 * libbabeltrace2 function returning `bt_bool` with
 * PyModuleBuilder::addLibBoolRetFunc() to make a Python `bool` instead.
 */
template <typename IntT>
struct PyConv<IntT, std::enable_if_t<internal::isInt<IntT> && std::is_signed_v<IntT>>> final
{
    static IntT fromPy(PyObject& pyObj) noexcept
    {
        BT_ASSERT_DBG(PyLong_Check(&pyObj));

        const auto val = PyLong_AsLongLong(&pyObj);

        BT_ASSERT_DBG(!(val == -1 && PyErr_Occurred()));

        if constexpr (sizeof(IntT) < sizeof(val)) {
            BT_ASSERT_DBG(val >= std::numeric_limits<IntT>::min() &&
                          val <= std::numeric_limits<IntT>::max());
        }

        return static_cast<IntT>(val);
    }

    static PyObject& toPy(const IntT val)
    {
        return throwIfPyErr(PyLong_FromLongLong(val));
    }
};

/*
 * Python's `int` to/from any unsigned integer type.
 *
 * The Python layer checks the range of any integer before passing it
 * to this module.
 */
template <typename IntT>
struct PyConv<IntT, std::enable_if_t<internal::isInt<IntT> && std::is_unsigned_v<IntT>>> final
{
    static IntT fromPy(PyObject& pyObj) noexcept
    {
        BT_ASSERT_DBG(PyLong_Check(&pyObj));

        const auto val = PyLong_AsUnsignedLongLong(&pyObj);

        BT_ASSERT_DBG(!(val == static_cast<unsigned long long>(-1) && PyErr_Occurred()));

        if constexpr (sizeof(IntT) < sizeof(val)) {
            BT_ASSERT_DBG(val <= std::numeric_limits<IntT>::max());
        }

        return static_cast<IntT>(val);
    }

    static PyObject& toPy(const IntT val)
    {
        return throwIfPyErr(PyLong_FromUnsignedLongLong(val));
    }
};

template <>
struct PyConv<double> final
{
    static double fromPy(PyObject& pyObj) noexcept
    {
        BT_ASSERT_DBG(PyFloat_Check(&pyObj));
        return PyFloat_AS_DOUBLE(&pyObj);
    }

    static PyObject& toPy(const double val)
    {
        return throwIfPyErr(PyFloat_FromDouble(val));
    }
};

/*
 * Python's `float` to/from `float`.
 *
 * The Python layer checks that a finite value is within the range of
 * `float` before passing it to this module.
 */
template <>
struct PyConv<float> final
{
    static float fromPy(PyObject& pyObj) noexcept
    {
        const auto val = PyConv<double>::fromPy(pyObj);

        BT_ASSERT_DBG(!std::isfinite(val) || (val >= std::numeric_limits<float>::lowest() &&
                                              val <= std::numeric_limits<float>::max()));
        return static_cast<float>(val);
    }

    static PyObject& toPy(const float val)
    {
        return PyConv<double>::toPy(val);
    }
};

namespace internal {

/*
 * Returns the UTF-8 buffer of the Python string `pyObj`.
 *
 * The buffer remains valid for the duration of the adapted call since
 * the interpreter holds a reference to the arguments.
 *
 * Throws `PyErrSet` if `pyObj` isn't encodable (lone surrogate, for
 * example): this is user data, not a bug.
 */
inline const char *pyStrUtf8(PyObject& pyObj)
{
    BT_ASSERT_DBG(PyUnicode_Check(&pyObj));

    const auto str = PyUnicode_AsUTF8(&pyObj);

    if (!str) {
        throw PyErrSet {};
    }

    return str;
}

/*
 * Returns a new Python string from `str`, or a new reference to `None`
 * if `str` is `nullptr`.
 *
 * Decodes with the `surrogateescape` error handler because such a
 * string may come from a trace or from the file system (object name,
 * trace environment entry, plugin path, and so on), which isn't
 * necessarily valid UTF-8.
 */
inline PyObject& pyStrFromCStr(const char * const str)
{
    if (!str) {
        return newPyNoneRef();
    }

    return throwIfPyErr(
        PyUnicode_DecodeUTF8(str, static_cast<Py_ssize_t>(std::strlen(str)), "surrogateescape"));
}

} /* namespace internal */

/*
 * Python's `str` or `None` to/from `const char *`.
 */
template <>
struct PyConv<const char *> final
{
    static const char *fromPy(PyObject& pyObj)
    {
        if (&pyObj == Py_None) {
            return nullptr;
        }

        return internal::pyStrUtf8(pyObj);
    }

    static PyObject& toPy(const char * const str)
    {
        return internal::pyStrFromCStr(str);
    }
};

/*
 * Python's `str` or `None` to/from `bt2c::CStringView`.
 *
 * `None` corresponds to an empty view (data() returns `nullptr`).
 */
template <>
struct PyConv<bt2c::CStringView> final
{
    static bt2c::CStringView fromPy(PyObject& pyObj)
    {
        return PyConv<const char *>::fromPy(pyObj);
    }

    static PyObject& toPy(const bt2c::CStringView str)
    {
        return internal::pyStrFromCStr(str.data());
    }
};

/*
 * Python's `str` from `std::string`.
 *
 * Return value only: for an adapted function which builds a string
 * (for example, a formatted error).
 */
template <>
struct PyConv<std::string> final
{
    static PyObject& toPy(const std::string& str)
    {
        return throwIfPyErr(
            PyUnicode_FromStringAndSize(str.data(), static_cast<Py_ssize_t>(str.size())));
    }
};

/*
 * Python's `bytes` (16 bytes) to/from `bt2c::UuidView`.
 *
 * The view of fromPy() remains valid for the duration of the adapted
 * call since the interpreter holds a reference to the arguments.
 */
template <>
struct PyConv<bt2c::UuidView> final
{
    static bt2c::UuidView fromPy(PyObject& pyObj) noexcept
    {
        BT_ASSERT_DBG(PyBytes_Check(&pyObj));
        BT_ASSERT_DBG(PyBytes_GET_SIZE(&pyObj) == static_cast<Py_ssize_t>(bt2c::UuidView::size()));

        return bt2c::UuidView {
            reinterpret_cast<const bt2c::UuidView::Val *>(PyBytes_AS_STRING(&pyObj))};
    }

    static PyObject& toPy(const bt2c::UuidView uuid)
    {
        return throwIfPyErr(PyBytes_FromStringAndSize(reinterpret_cast<const char *>(uuid.data()),
                                                      static_cast<Py_ssize_t>(uuid.size())));
    }
};

/*
 * Python's `int` to/from enumeration, through the converter of its
 * underlying type.
 */
template <typename EnumT>
struct PyConv<EnumT, std::enable_if_t<std::is_enum_v<EnumT>>> final
{
    static EnumT fromPy(PyObject& pyObj) noexcept
    {
        return static_cast<EnumT>(_UnderlyingPyConv::fromPy(pyObj));
    }

    static PyObject& toPy(const EnumT val)
    {
        return _UnderlyingPyConv::toPy(static_cast<std::underlying_type_t<EnumT>>(val));
    }

private:
    using _UnderlyingPyConv = PyConv<std::underlying_type_t<EnumT>>;
};

/*
 * `PtrObj` object or `None` to/from raw libbabeltrace2 pointer.
 *
 * For libbabeltrace2 functions which the Python layer calls directly.
 *
 * Doesn't change the reference count of the libbabeltrace2 object: the
 * Python layer knows whether a returned pointer is a new or a
 * borrowed reference.
 */
template <typename LibObjT>
struct PyConv<LibObjT *, std::enable_if_t<ptrObjCanHold<LibObjT>>> final
{
    static LibObjT *fromPy(PyObject& pyObj) noexcept
    {
        return libObjPtrFromPtrObj<LibObjT>(pyObj);
    }

    static PyObject& toPy(LibObjT * const libObjPtr)
    {
        return createPtrObj(libObjPtr);
    }
};

namespace internal {

template <typename LibObjT>
std::true_type isWrapperTest(const bt2::BorrowedObject<LibObjT>&);

std::false_type isWrapperTest(...);

/*
 * Whether or not `T` is a libbabeltrace2 object wrapper, that is, a
 * class which publicly derives from `bt2::BorrowedObject`.
 */
template <typename T>
inline constexpr bool isWrapper = decltype(isWrapperTest(std::declval<T>()))::value;

} /* namespace internal */

/*
 * `PtrObj` object to/from libbabeltrace2 object wrapper.
 *
 * Doesn't change the reference count of the underlying libbabeltrace2
 * object: the Python layer gets a reference itself when it needs one.
 *
 * fromPy() doesn't expect `None`: use `bt2::OptionalBorrowedObject`
 * for that.
 */
template <typename ObjT>
struct PyConv<ObjT, std::enable_if_t<internal::isWrapper<ObjT>>> final
{
    static ObjT fromPy(PyObject& pyObj) noexcept
    {
        const auto libObjPtr = libObjPtrFromPtrObj<std::remove_pointer_t<_LibObjPtr>>(pyObj);

        BT_ASSERT_DBG(libObjPtr);
        return ObjT {libObjPtr};
    }

    static PyObject& toPy(const ObjT obj)
    {
        return createPtrObj(obj.libObjPtr());
    }

private:
    using _LibObjPtr = typename ObjT::LibObjPtr;
};

/*
 * `PtrObj` object or `None` object
 * to/from `bt2::OptionalBorrowedObject`.
 */
template <typename ObjT>
struct PyConv<bt2::OptionalBorrowedObject<ObjT>> final
{
    static bt2::OptionalBorrowedObject<ObjT> fromPy(PyObject& pyObj) noexcept
    {
        return libObjPtrFromPtrObj<std::remove_pointer_t<typename ObjT::LibObjPtr>>(pyObj);
    }

    static PyObject& toPy(const bt2::OptionalBorrowedObject<ObjT> optObj)
    {
        return createPtrObj(optObj.libObjPtr());
    }
};

/*
 * `PtrObj` object or `None` from `bt2::SharedObject`.
 *
 * Transfers the reference of the shared object to the Python layer,
 * which must eventually put it (`_create_from_ptr()` in the
 * Python layer).
 */
template <typename ObjT, typename LibObjT, typename RefFuncsT>
struct PyConv<bt2::SharedObject<ObjT, LibObjT, RefFuncsT>> final
{
    static PyObject& toPy(bt2::SharedObject<ObjT, LibObjT, RefFuncsT> sharedObj)
    {
        if (!sharedObj) {
            return newPyNoneRef();
        }

        /* On error, `sharedObj` still owns (and puts) the reference */
        auto& pyObj = createPtrObj((*sharedObj).libObjPtr());

        sharedObj.release();
        return pyObj;
    }
};

/*
 * Any Python object as is (borrowed reference).
 *
 * For a custom function which needs the Python object itself, for
 * example a Python callable.
 */
template <>
struct PyConv<PyObject *> final
{
    static PyObject *fromPy(PyObject& pyObj) noexcept
    {
        return &pyObj;
    }
};

/*
 * Any Python object from `bt2py::PyObjUP`.
 *
 * Transfers the owned reference to the interpreter: for an adapted
 * function which builds its Python return value itself.
 */
template <>
struct PyConv<PyObjUP> final
{
    static PyObject& toPy(PyObjUP pyObj) noexcept
    {
        BT_ASSERT_DBG(pyObj);
        return *pyObj.release();
    }
};

/*
 * Python object or `None` from `std::optional`.
 */
template <typename T>
struct PyConv<std::optional<T>> final
{
    static PyObject& toPy(std::optional<T> opt)
    {
        if (!opt) {
            return newPyNoneRef();
        }

        return PyConv<T>::toPy(std::move(*opt));
    }
};

/*
 * Python tuple from `std::tuple`, converting each element with its
 * own converter.
 *
 * For an adapted function which has output parameters, for example
 * returning both a status and a value.
 */
template <typename... Ts>
struct PyConv<std::tuple<Ts...>> final
{
    static PyObject& toPy(std::tuple<Ts...> tuple)
    {
        return _toPy(std::move(tuple), std::index_sequence_for<Ts...> {});
    }

private:
    template <std::size_t... IndexVs>
    static PyObject& _toPy(std::tuple<Ts...> tuple, std::index_sequence<IndexVs...>)
    {
        /*
         * On error, the remaining items of `pyTuple` are `NULL`, which
         * the tuple deallocator handles.
         */
        auto pyTuple = ownPyObjOrThrow(PyTuple_New(sizeof...(Ts)));

        /* Steals the reference to each converted element */
        (PyTuple_SET_ITEM(pyTuple.get(), IndexVs,
                          &PyConv<Ts>::toPy(std::get<IndexVs>(std::move(tuple)))),
         ...);

        return *pyTuple.release();
    }
};

/*
 * Returns a new owned Python object reference for the value `val`,
 * converted with its `PyConv` converter.
 *
 * For a Python object to pass to a Python callable.
 *
 * Throws `PyErrSet` on error.
 */
template <typename T>
PyObjUP toPyObjUp(const T& val)
{
    return PyObjUP {&PyConv<T>::toPy(val)};
}

/*
 * Returns a new owned reference to `pyObj`, or to `None` if `pyObj`
 * is `nullptr`.
 *
 * PyConv<PyObject *>::toPy() doesn't exist because the ownership of a
 * returned `PyObject *` is ambiguous; here, `pyObj` is borrowed.
 */
inline PyObjUP toPyObjUp(PyObject * const pyObj) noexcept
{
    return newPyObjUp(pyObj ? *pyObj : *Py_None);
}

} /* namespace bt2py */

#endif /* BABELTRACE_BINDINGS_PYTHON_BT2_NATIVE_RUNTIME_PY_CONV_HPP */
