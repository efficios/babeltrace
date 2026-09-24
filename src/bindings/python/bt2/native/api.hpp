/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#ifndef BABELTRACE_BINDINGS_PYTHON_BT2_NATIVE_API_HPP
#define BABELTRACE_BINDINGS_PYTHON_BT2_NATIVE_API_HPP

namespace bt2py {

class PyModuleBuilder;

/*
 * Each function below adds the functions, constants, and objects of
 * one libbabeltrace2 API area to `mb`.
 */
void registerAutodiscApi(PyModuleBuilder& mb);
void registerConnectionApi(PyModuleBuilder& mb);
void registerErrorApi(PyModuleBuilder& mb);
void registerFormatErrorApi(PyModuleBuilder& mb);
void registerValueApi(PyModuleBuilder& mb);
void registerVersionApi(PyModuleBuilder& mb);
void registerClockClassApi(PyModuleBuilder& mb);
void registerClockSnapshotApi(PyModuleBuilder& mb);
void registerTraceClassApi(PyModuleBuilder& mb);
void registerTraceApi(PyModuleBuilder& mb);
void registerStreamClassApi(PyModuleBuilder& mb);
void registerStreamApi(PyModuleBuilder& mb);
void registerQueryExecutorApi(PyModuleBuilder& mb);
void registerPrivateQueryExecutorApi(PyModuleBuilder& mb);
void registerPortApi(PyModuleBuilder& mb);
void registerSelfComponentPortApi(PyModuleBuilder& mb);
void registerComponentApi(PyModuleBuilder& mb);
void registerSelfComponentApi(PyModuleBuilder& mb);
void registerComponentClassApi(PyModuleBuilder& mb);
void registerPacketApi(PyModuleBuilder& mb);
void registerMessageApi(PyModuleBuilder& mb);
void registerMessageIteratorApi(PyModuleBuilder& mb);
void registerSelfMessageIteratorApi(PyModuleBuilder& mb);
void registerLoggingApi(PyModuleBuilder& mb);
void registerInterrupterApi(PyModuleBuilder& mb);
void registerIntegerRangeSetApi(PyModuleBuilder& mb);
void registerFieldPathApi(PyModuleBuilder& mb);
void registerFieldLocationApi(PyModuleBuilder& mb);
void registerFieldClassApi(PyModuleBuilder& mb);
void registerFieldApi(PyModuleBuilder& mb);
void registerEventClassApi(PyModuleBuilder& mb);
void registerEventApi(PyModuleBuilder& mb);
void registerGraphApi(PyModuleBuilder& mb);
void registerMipApi(PyModuleBuilder& mb);
void registerPluginApi(PyModuleBuilder& mb);
void registerComponentClassDevApi(PyModuleBuilder& mb);

} /* namespace bt2py */

#endif /* BABELTRACE_BINDINGS_PYTHON_BT2_NATIVE_API_HPP */
