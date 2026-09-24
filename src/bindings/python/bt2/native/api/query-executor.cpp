/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#include <Python.h>

#include <babeltrace2/babeltrace.h>

#include "cpp-common/bt2/component-class.hpp"
#include "cpp-common/bt2/optional-borrowed-object.hpp"
#include "cpp-common/bt2/query-executor.hpp"
#include "cpp-common/bt2/value.hpp"
#include "cpp-common/bt2c/c-string-view.hpp"

#include "../api.hpp"
#include "../runtime/py-module-builder.hpp"

namespace bt2py {
namespace {

/*
 * Creates a query executor to query the object `obj` from `compCls`
 * with the parameters `params` and the method data `pyMethodObj`
 * (`None` means no method data).
 *
 * Doesn't get a reference to `pyMethodObj`: the Python layer keeps it
 * alive as long as the query executor exists.
 */
bt2::QueryExecutor::Shared
createQueryExec(const bt2::ConstComponentClass compCls, const bt2c::CStringView obj,
                const bt2::OptionalBorrowedObject<bt2::ConstValue> params,
                PyObject * const pyMethodObj)
{
    if (pyMethodObj == Py_None) {
        return bt2::QueryExecutor::create(compCls, obj, params);
    }

    return bt2::QueryExecutor::create(compCls, obj, *pyMethodObj, params);
}

/*
 * Queries with `queryExec`, returning the result.
 */
bt2::ConstValue::Shared query(const bt2::QueryExecutor queryExec)
{
    return queryExec.query();
}

} /* namespace */

void registerQueryExecutorApi(PyModuleBuilder& mb)
{
    mb.addFunc<createQueryExec>("query_executor_create");
    mb.addLibFunc<bt_query_executor_get_ref>();
    mb.addLibFunc<bt_query_executor_put_ref>();

    mb.addFunc<query>("query_executor_query");

    mb.addLibFunc<bt_query_executor_add_interrupter>();
    mb.addLibFunc<bt_query_executor_borrow_default_interrupter>();
    mb.addLibBoolRetFunc<bt_query_executor_is_interrupted>();

    mb.addLibFunc<bt_query_executor_set_logging_level>();
    mb.addLibFunc<bt_query_executor_get_logging_level>();
}

} /* namespace bt2py */
