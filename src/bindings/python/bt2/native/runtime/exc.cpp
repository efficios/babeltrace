/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#include <Python.h>

#include "py-common/py-common.h"

#include <new>

#include <babeltrace2/babeltrace.h>

#include "common/assert.h"
#include "cpp-common/bt2/exc.hpp"
#include "cpp-common/bt2c/glib-up.hpp"
#include "cpp-common/bt2c/logging.hpp"

#include "exc.hpp"
#include "ptr-obj.hpp"
#include "utils.hpp"

namespace bt2py {
namespace {

/*
 * Returns the attribute `name` of the `bt2` package, or `nullptr`
 * (Python error indicator set) if there's no such attribute.
 */
PyObjUP bt2PyAttr(const char * const name) noexcept
{
    /* Already imported: `bt2` imports this module */
    const PyObjUP pyMod {PyImport_ImportModule("bt2")};

    if (!pyMod) {
        return nullptr;
    }

    return PyObjUP {PyObject_GetAttrString(pyMod.get(), name)};
}

/*
 * Sets the Python error indicator to an instance of the exception
 * class named `name` within the `bt2` package, with the message `msg`.
 *
 * Falls back to `RuntimeError` if there's no such class.
 */
void setBt2PyErr(const char * const name, const char * const msg) noexcept
{
    const auto pyExcType = bt2PyAttr(name);

    if (pyExcType) {
        PyErr_SetString(pyExcType.get(), msg);
        return;
    }

    PyErr_Clear();
    PyErr_SetString(PyExc_RuntimeError, msg);
}

/*
 * Moves the libbabeltrace2 error which the `bt2._Error` exception
 * `pyExc` wraps back as the error of the current thread, and returns
 * the formatted traceback and message of `pyExc`, or `nullptr`
 * on error.
 *
 * Excludes the causes of the wrapped error from the returned string:
 * they're already part of the error of the current thread.
 */
bt2c::GStringUP restoreBt2ErrorAndFormat(PyObject& pyExc, PyObject * const pyExcTb,
                                         const int logLevel) noexcept
{
    {
        const PyObjUP pyErrPtr {PyObject_GetAttrString(&pyExc, "_ptr")};

        BT_ASSERT(pyErrPtr);

        /* Steal the error: `bt2._Error` doesn't release it anymore */
        const auto ret = PyObject_SetAttrString(&pyExc, "_ptr", Py_None);

        BT_ASSERT(ret == 0);
        bt_current_thread_move_error(libObjPtrFromPtrObj<const bt_error>(*pyErrPtr));
    }

    bt2c::GStringUP str {bt_py_common_format_tb(pyExcTb, logLevel)};

    if (!str) {
        /* bt_py_common_format_tb() logs itself */
        return nullptr;
    }

    g_string_prepend(str.get(), "Traceback (most recent call last):\n");

    const PyObjUP pyMsg {PyObject_GetAttrString(&pyExc, "_msg")};

    BT_ASSERT(pyMsg);
    g_string_append_printf(str.get(), "\nbt2._Error: %s", PyUnicode_AsUTF8(pyMsg.get()));
    return str;
}

/*
 * Appends one cause per exception of the chain ending with `pyExc`,
 * from the oldest, to the error of the current thread using the actor
 * of `logger`.
 *
 * `pyBt2ErrType` is the `bt2._Error` Python type.
 */
void appendCausesFromPyExcChain(const bt2c::Logger& logger, PyObject& pyExc,
                                PyObject& pyBt2ErrType) noexcept
{
    const PyObjUP pyExcCause {PyException_GetCause(&pyExc)};

    /* Oldest first */
    if (pyExcCause) {
        appendCausesFromPyExcChain(logger, *pyExcCause, pyBt2ErrType);
    }

    const PyObjUP pyExcTb {PyException_GetTraceback(&pyExc)};
    const auto logLevel = static_cast<int>(logger.level());
    bt2c::GStringUP str;

    if (PyErr_GivenExceptionMatches(&pyExc, &pyBt2ErrType)) {
        /*
         * Never raised with a (Python) cause: moving its error back
         * must happen before appending any other cause.
         */
        BT_ASSERT(!pyExcCause);
        str = restoreBt2ErrorAndFormat(pyExc, pyExcTb.get(), logLevel);
    } else {
        const PyObjUP pyExcType {PyObject_Type(&pyExc)};

        str.reset(
            bt_py_common_format_exception(pyExcType.get(), &pyExc, pyExcTb.get(), logLevel, false));
    }

    if (!str) {
        /* The formatting function logs itself */
        return;
    }

    logger.appendCauseStr(__FILE__, __LINE__, "", str->str);
}

} /* namespace */

void setPyErrFromCurCppExc() noexcept
{
    try {
        throw;
    } catch (const PyErrSet&) {
        /* Already set; keep before `std::exception` below */
    } catch (const bt2::MemoryError&) {
        setBt2PyErr("_MemoryError", "Memory error");
    } catch (const bt2::OverflowError&) {
        setBt2PyErr("_OverflowError", "Overflow error");
    } catch (const bt2::Error& exc) {
        setBt2PyErr("_Error", exc.what());
    } catch (const bt2::TryAgain&) {
        setBt2PyErr("TryAgain", "Try again");
    } catch (const bt2c::End&) {
        setBt2PyErr("Stop", "End");
    } catch (const bt2::UnknownObject&) {
        setBt2PyErr("UnknownObject", "Unknown object");
    } catch (const std::bad_alloc&) {
        PyErr_NoMemory();
    } catch (const std::exception& exc) {
        PyErr_SetString(PyExc_SystemError, exc.what());
    } catch (...) {
        PyErr_SetString(PyExc_SystemError, "Unknown C++ exception");
    }
}

void logErrorAndAppendCausesFromCurPyExc(const bt2c::Logger& logger) noexcept
{
    BT_ASSERT(PyErr_Occurred());

    /* Log the whole chain; doesn't modify the Python error indicator */
    {
        const bt2c::GStringUP str {
            bt_py_common_format_current_exception(static_cast<int>(logger.level()))};

        /* On error, bt_py_common_format_current_exception() logs itself */
        if (str) {
            BT_CPPLOGE_SPEC(logger, "{}", str->str);
        }
    }

    PyObject *pyExcType;
    PyObject *pyExc;
    PyObject *pyExcTb;

    PyErr_Fetch(&pyExcType, &pyExc, &pyExcTb);
    PyErr_NormalizeException(&pyExcType, &pyExc, &pyExcTb);

    /* Own the fetched references: this clears the Python error indicator */
    const PyObjUP pyExcTypeUp {pyExcType};
    const PyObjUP pyExcUp {pyExc};
    const PyObjUP pyExcTbUp {pyExcTb};

    BT_ASSERT(pyExc);

    /*
     * Attach the traceback so that PyException_GetTraceback() returns
     * it within appendCausesFromPyExcChain().
     */
    if (pyExcTb) {
        PyException_SetTraceback(pyExc, pyExcTb);
    }

    const auto pyBt2ErrType = bt2PyAttr("_Error");

    /* `bt2` imports this module: `bt2._Error` exists */
    BT_ASSERT(pyBt2ErrType);
    appendCausesFromPyExcChain(logger, *pyExc, *pyBt2ErrType);
}

void throwCppExcFromCurPyExc(const bt2c::Logger& logger)
{
    BT_ASSERT(PyErr_Occurred());

    /*
     * Fetch the current Python exception: looking up an attribute of
     * the `bt2` package fails while the Python error indicator is set.
     */
    PyObject *pyExcType;
    PyObject *pyExc;
    PyObject *pyExcTb;

    PyErr_Fetch(&pyExcType, &pyExc, &pyExcTb);

    /* Own the fetched references: this clears the Python error indicator */
    PyObjUP pyExcTypeUp {pyExcType};
    PyObjUP pyExcUp {pyExc};
    PyObjUP pyExcTbUp {pyExcTb};

    /*
     * Returns whether or not the fetched Python exception is an
     * instance of the `bt2` package class named `name`.
     */
    const auto pyExcIs = [pyExcType](const char * const name) {
        const auto pyBt2ExcType = bt2PyAttr(name);

        /* `bt2` imports this module: the class exists */
        BT_ASSERT(pyBt2ExcType);
        return PyErr_GivenExceptionMatches(pyExcType, pyBt2ExcType.get()) != 0;
    };

    if (pyExcIs("TryAgain")) {
        throw bt2::TryAgain {};
    } else if (pyExcIs("Stop")) {
        throw bt2c::End {};
    } else if (pyExcIs("UnknownObject")) {
        throw bt2::UnknownObject {};
    }

    const auto isMemError = pyExcIs("_MemoryError");

    /* Restore for logErrorAndAppendCausesFromCurPyExc(), which clears it */
    PyErr_Restore(pyExcTypeUp.release(), pyExcUp.release(), pyExcTbUp.release());
    logErrorAndAppendCausesFromCurPyExc(logger);

    if (isMemError) {
        throw bt2::MemoryError {};
    }

    throw bt2::Error {};
}

} /* namespace bt2py */
