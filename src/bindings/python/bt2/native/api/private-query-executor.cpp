/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#include <Python.h>

#include <babeltrace2/babeltrace.h>

#include "../api.hpp"
#include "../runtime/py-module-builder.hpp"

namespace bt2py {

void registerPrivateQueryExecutorApi(PyModuleBuilder& mb)
{
    mb.addLibFunc<bt_private_query_executor_as_query_executor_const>();
}

} /* namespace bt2py */
