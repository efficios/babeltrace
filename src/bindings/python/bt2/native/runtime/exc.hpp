/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#ifndef BABELTRACE_BINDINGS_PYTHON_BT2_NATIVE_RUNTIME_EXC_HPP
#define BABELTRACE_BINDINGS_PYTHON_BT2_NATIVE_RUNTIME_EXC_HPP

#include <Python.h>

#include <exception>
#include <utility>

#include <fmt/format.h>

namespace bt2c {

class Logger;

} /* namespace bt2c */

namespace bt2py {

/*
 * Thrown when the Python error indicator is already set.
 *
 * Whatever catches this must only return `nullptr` to the Python
 * interpreter.
 *
 * Inherits `std::exception` for generic handlers: catch it before
 * `std::exception` to avoid overwriting the Python error indicator.
 */
class PyErrSet final : public std::exception
{
public:
    explicit PyErrSet() noexcept = default;

    /*
     * Generic message: the actual error is in the Python
     * error indicator.
     */
    const char *what() const noexcept override
    {
        return "Python error indicator is set";
    }
};

/*
 * Returns `pyObj`, throwing `PyErrSet` if it's `nullptr` (the Python
 * API function which returned it set the Python error indicator).
 */
inline PyObject& throwIfPyErr(PyObject * const pyObj)
{
    if (!pyObj) {
        throw PyErrSet {};
    }

    return *pyObj;
}

/*
 * Returns `res`, throwing `PyErrSet` if it's negative (the Python API
 * function which returned it set the Python error indicator).
 */
inline int throwIfPyErr(const int res)
{
    if (res < 0) {
        throw PyErrSet {};
    }

    return res;
}

/*
 * Sets the Python error indicator to an exception of type `excType`
 * with the message formatted from `fmt` and `args`, and then
 * throws `PyErrSet`.
 */
template <typename... ArgTs>
[[noreturn]] void throwPyErr(PyObject& excType, fmt::format_string<ArgTs...> fmt, ArgTs&&...args)
{
    PyErr_SetString(&excType, fmt::format(fmt, std::forward<ArgTs>(args)...).c_str());
    throw PyErrSet {};
}

/*
 * Sets the Python error indicator from the C++ exception currently
 * being handled.
 *
 * Only call this from a `catch` block.
 *
 * Translates:
 *
 * `bt2py::PyErrSet`:
 *     Nothing (already set)
 *
 * `bt2::MemoryError`:
 *     `bt2._MemoryError`
 *
 * `bt2::OverflowError`:
 *     `bt2._OverflowError`
 *
 * `bt2::Error`:
 *     `bt2._Error` (takes the current thread error)
 *
 * `bt2::TryAgain`:
 *     `bt2.TryAgain`
 *
 * `bt2c::End`:
 *     `bt2.Stop`
 *
 * `bt2::UnknownObject`:
 *     `bt2.UnknownObject`
 *
 * `std::bad_alloc`:
 *     `MemoryError`
 *
 * Anything else:
 *     `SystemError`
 */
void setPyErrFromCurCppExc() noexcept;

/*
 * Logs the current Python exception with `logger` at the error level,
 * appends one cause per exception of its chain, from the oldest, to the
 * error of the current thread using the actor of `logger`, and then
 * clears the Python error indicator.
 *
 * If the oldest exception of the chain is a `bt2._Error`, first moves
 * the libbabeltrace2 error which it wraps back as the error of the
 * current thread.
 *
 * The Python error indicator must be set.
 *
 * For a native callback which calls Python code and can only report an
 * error with a status (for example, a graph listener).
 */
void logErrorAndAppendCausesFromCurPyExc(const bt2c::Logger& logger) noexcept;

/*
 * Translates the current Python exception to a C++ exception, clearing
 * the Python error indicator, and throws it.
 *
 * The Python error indicator must be set.
 *
 * For a component class or message iterator method which the C++
 * component class bridge of `cpp-common/bt2/component-class-dev.hpp`
 * calls: the bridge then translates the C++ exception to a method
 * status.
 *
 * Translates:
 *
 * `bt2.TryAgain`:
 *     `bt2::TryAgain`
 *
 * `bt2.Stop`:
 *     `bt2c::End`
 *
 * `bt2.UnknownObject`:
 *     `bt2::UnknownObject`
 *
 * `bt2._MemoryError`:
 *     `bt2::MemoryError`, after calling
 *     logErrorAndAppendCausesFromCurPyExc() with `logger`.
 *
 * Anything else:
 *     `bt2::Error`, after calling logErrorAndAppendCausesFromCurPyExc()
 *     with `logger`.
 */
[[noreturn]] void throwCppExcFromCurPyExc(const bt2c::Logger& logger);

} /* namespace bt2py */

#endif /* BABELTRACE_BINDINGS_PYTHON_BT2_NATIVE_RUNTIME_EXC_HPP */
