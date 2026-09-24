/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#include <Python.h>

#include "py-common/py-common.h"

#include <string_view>
#include <unordered_set>

#include <glib.h>

#include "common/assert.h"
#include "common/common.h"
#include "cpp-common/bt2c/logging.hpp"

#include "ptr-obj.hpp"
#include "py-module-builder.hpp"
#include "utils.hpp"

namespace bt2py {
namespace internal {

void callPyDestructionListener(const PyObjUP pyCallable, void * const addr,
                               const LibObjTypeTag& tag) noexcept
{
    const auto logger = createLogger("Destruction listener");
    PyObjUP pyLibObjPtr;

    try {
        pyLibObjPtr.reset(&createPtrObj(addr, tag));
    } catch (...) {
        BT_CPPLOGF_SPEC(logger, "Failed to create a `PtrObj` object: type={}", tag.name);
        bt_common_abort();
    }

    const PyObjUP pyRes {
        PyObject_CallFunctionObjArgs(pyCallable.get(), pyLibObjPtr.get(), nullptr)};

    if (!pyRes) {
        /* Doesn't modify the Python error indicator */
        const auto gstr = bt_py_common_format_current_exception(static_cast<int>(logger.level()));

        /* On error, bt_py_common_format_current_exception() logs itself */
        if (gstr) {
            BT_CPPLOGW_SPEC(logger, "`{}` destruction listener failed: {}", tag.name, gstr->str);
            g_string_free(gstr, TRUE);
        }

        PyErr_Clear();
        return;
    }

    BT_ASSERT(pyRes.get() == Py_None);
}

} /* namespace internal */

bool PyModuleBuilder::_namesAreUnique() const
{
    std::unordered_set<std::string_view> names;

    for (auto it = _mMethodDefs.begin(); it != std::prev(_mMethodDefs.end()); ++it) {
        if (!names.insert(it->ml_name).second) {
            return false;
        }
    }

    for (const auto& nameVal : _mConsts) {
        if (!names.insert(nameVal.first).second) {
            return false;
        }
    }

    for (const auto& nameCreateFunc : _mObjs) {
        if (!names.insert(nameCreateFunc.first).second) {
            return false;
        }
    }

    return true;
}

PyObject& PyModuleBuilder::build(const bt2c::CStringView name) const
{
    BT_ASSERT(_namesAreUnique());

    _mModDef = {
        PyModuleDef_HEAD_INIT,
        name.data(),
        nullptr,
        -1,
        /* `PyModuleDef::m_methods` isn't a pointer to const */
        const_cast<PyMethodDef *>(_mMethodDefs.data()),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
    };

    /* On error, `mod` releases the partially built module */
    const auto mod = ownPyObjOrThrow(PyModule_Create(&_mModDef));

    for (const auto& nameVal : _mConsts) {
        const auto pyVal = ownPyObjOrThrow(PyLong_FromLongLong(nameVal.second));

        throwIfPyErr(pyModAddObjectRef(*mod, nameVal.first.c_str(), *pyVal));
    }

    for (const auto& nameCreateFunc : _mObjs) {
        const PyObjUP pyObj {&nameCreateFunc.second()};

        throwIfPyErr(pyModAddObjectRef(*mod, nameCreateFunc.first.c_str(), *pyObj));
    }

    return newPyObjRef(*mod);
}

} /* namespace bt2py */
