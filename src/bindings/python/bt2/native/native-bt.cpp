/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#include <Python.h>

#include <functional>

/*
 * Include before "common/func-status.h" because
 * `<babeltrace2/babeltrace.h>` undefines the `__BT_FUNC_STATUS_*`
 * definitions.
 */
#include <babeltrace2/babeltrace.h>

#include "common/func-status.h"

#include "api.hpp"
#include "runtime/exc.hpp"
#include "runtime/ptr-obj.hpp"
#include "runtime/py-module-builder.hpp"
#include "runtime/utils.hpp"

namespace {

void registerCommon(bt2py::PyModuleBuilder& mb)
{
    mb.addObject("PtrObj", []() -> PyObject& {
        return bt2py::newPyObjRef(reinterpret_cast<PyObject&>(bt2py::ptrObjPyType()));
    });

    /* Used by bt2.utils._handle_func_status() */
    mb.addConstant("__BT_FUNC_STATUS_OK", BT_FUNC_STATUS_OK);
    mb.addConstant("__BT_FUNC_STATUS_ERROR", BT_FUNC_STATUS_ERROR);
    mb.addConstant("__BT_FUNC_STATUS_MEMORY_ERROR", BT_FUNC_STATUS_MEMORY_ERROR);
    mb.addConstant("__BT_FUNC_STATUS_END", BT_FUNC_STATUS_END);
    mb.addConstant("__BT_FUNC_STATUS_AGAIN", BT_FUNC_STATUS_AGAIN);
    mb.addConstant("__BT_FUNC_STATUS_OVERFLOW_ERROR", BT_FUNC_STATUS_OVERFLOW_ERROR);
    mb.addConstant("__BT_FUNC_STATUS_UNKNOWN_OBJECT", BT_FUNC_STATUS_UNKNOWN_OBJECT);
    mb.addConstant("__BT_FUNC_STATUS_NO_MATCH", BT_FUNC_STATUS_NO_MATCH);
    mb.addConstant("__BT_FUNC_STATUS_NOT_FOUND", BT_FUNC_STATUS_NOT_FOUND);
}

const bt2py::PyModuleBuilder& pyModuleBuilder()
{
    /*
     * Never destroyed: the module refers to the method table of the
     * builder for its whole lifetime, which can end after the
     * destruction of the static objects of this module (for example,
     * when a library destructor of libbabeltrace2 finalizes Python).
     */
    static const auto& mb = *std::invoke([] {
        const auto builder = new bt2py::PyModuleBuilder;

        registerCommon(*builder);
        bt2py::registerAutodiscApi(*builder);
        bt2py::registerConnectionApi(*builder);
        bt2py::registerErrorApi(*builder);
        bt2py::registerFormatErrorApi(*builder);
        bt2py::registerValueApi(*builder);
        bt2py::registerVersionApi(*builder);
        bt2py::registerClockClassApi(*builder);
        bt2py::registerClockSnapshotApi(*builder);
        bt2py::registerTraceClassApi(*builder);
        bt2py::registerTraceApi(*builder);
        bt2py::registerStreamClassApi(*builder);
        bt2py::registerStreamApi(*builder);
        bt2py::registerQueryExecutorApi(*builder);
        bt2py::registerPrivateQueryExecutorApi(*builder);
        bt2py::registerPortApi(*builder);
        bt2py::registerSelfComponentPortApi(*builder);
        bt2py::registerComponentApi(*builder);
        bt2py::registerSelfComponentApi(*builder);
        bt2py::registerComponentClassApi(*builder);
        bt2py::registerPacketApi(*builder);
        bt2py::registerMessageApi(*builder);
        bt2py::registerMessageIteratorApi(*builder);
        bt2py::registerSelfMessageIteratorApi(*builder);
        bt2py::registerLoggingApi(*builder);
        bt2py::registerInterrupterApi(*builder);
        bt2py::registerIntegerRangeSetApi(*builder);
        bt2py::registerFieldPathApi(*builder);
        bt2py::registerFieldLocationApi(*builder);
        bt2py::registerFieldClassApi(*builder);
        bt2py::registerFieldApi(*builder);
        bt2py::registerEventClassApi(*builder);
        bt2py::registerEventApi(*builder);
        bt2py::registerGraphApi(*builder);
        bt2py::registerMipApi(*builder);
        bt2py::registerPluginApi(*builder);
        bt2py::registerComponentClassDevApi(*builder);
        return builder;
    });

    return mb;
}

} /* namespace */

PyMODINIT_FUNC PyInit_native_bt();

PyMODINIT_FUNC PyInit_native_bt()
{
    try {
        /* Before building: some module objects are `PtrObj` objects */
        bt2py::initPtrObjType();
        return &pyModuleBuilder().build("bt2.native_bt");
    } catch (...) {
        bt2py::setPyErrFromCurCppExc();
        return nullptr;
    }
}
