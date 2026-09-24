/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#include <Python.h>

#include <optional>
#include <tuple>

#include <babeltrace2/babeltrace.h>

#include "common/assert.h"
#include "cpp-common/bt2/component-class.hpp"
#include "cpp-common/bt2/graph.hpp"
#include "cpp-common/bt2/logging.hpp"
#include "cpp-common/bt2/optional-borrowed-object.hpp"
#include "cpp-common/bt2/value.hpp"
#include "cpp-common/bt2c/c-string-view.hpp"

#include "../api.hpp"
#include "../runtime/exc.hpp"
#include "../runtime/py-conv.hpp"
#include "../runtime/py-module-builder.hpp"
#include "../runtime/utils.hpp"

namespace bt2py {
namespace {

/*
 * Adds a component named `name` to `graph` from `compCls` with the
 * parameters `params`, the initialization method data `pyInitMethodObj`
 * (`None` means no method data), and the logging level `logLevel`,
 * returning the added component.
 *
 * Doesn't get a reference to `pyInitMethodObj`: the initialization
 * method of a Python component class only borrows it.
 */
template <typename CompClsT>
auto addComp(const bt2::Graph graph, const CompClsT compCls, const bt2c::CStringView name,
             const bt2::OptionalBorrowedObject<bt2::ConstMapValue> params,
             PyObject * const pyInitMethodObj, const bt2::LoggingLevel logLevel)
{
    if (pyInitMethodObj == Py_None) {
        return graph.addComponent(compCls, name, params, logLevel);
    }

    /* The component class receives `pyInitMethodObj` as is */
    return graph.addComponent(compCls, name, *pyInitMethodObj, params, logLevel);
}

/*
 * Connects `upstreamPort` to `downstreamPort` within `graph`, returning
 * the resulting connection.
 */
bt2::ConstConnection connectPorts(const bt2::Graph graph, const bt2::ConstOutputPort upstreamPort,
                                  const bt2::ConstInputPort downstreamPort)
{
    return graph.connectPorts(upstreamPort, downstreamPort);
}

/*
 * Port added listener calling the Python callable `data` with:
 *
 * 1. The component `comp` (`PtrObj` object).
 * 2. `CompClsTypeV`.
 * 3. The added port `port` (`PtrObj` object).
 * 4. `PortTypeV`.
 */
template <typename CompT, bt_component_class_type CompClsTypeV, typename PortT,
          bt_port_type PortTypeV>
bt_graph_listener_func_status listener(const CompT * const comp, const PortT * const port,
                                       void * const data) noexcept
{
    try {
        const auto pyComp = toPyObjUp(comp);
        const auto pyCompClsType = toPyObjUp(CompClsTypeV);
        const auto pyPort = toPyObjUp(port);
        const auto pyPortType = toPyObjUp(PortTypeV);
        const auto pyRes = ownPyObjOrThrow(PyObject_CallFunctionObjArgs(
            static_cast<PyObject *>(data), pyComp.get(), pyCompClsType.get(), pyPort.get(),
            pyPortType.get(), nullptr));

        BT_ASSERT(pyRes.get() == Py_None);
        return BT_GRAPH_LISTENER_FUNC_STATUS_OK;
    } catch (const PyErrSet&) {
        logErrorAndAppendCausesFromCurPyExc(createLogger("Graph's port added listener (Python)"));
        return BT_GRAPH_LISTENER_FUNC_STATUS_ERROR;
    }
}

/*
 * Adds the Python callable `pyCallable` as a listener of _all_ the port
 * added events of `graph`, returning the four listener IDs (source
 * output, filter input, filter output, and sink input), or `None`
 * on error.
 *
 * `pyCallable` accepts a component (`PtrObj` object), its component
 * class type, a port (`PtrObj` object), and its type.
 *
 * Doesn't get a reference to `pyCallable`: the Python layer keeps it
 * alive as long as `graph` exists.
 */
std::optional<std::tuple<bt_listener_id, bt_listener_id, bt_listener_id, bt_listener_id>>
addPortAddedListener(bt_graph * const graph, PyObject * const pyCallable) noexcept
{
    BT_ASSERT_DBG(PyCallable_Check(pyCallable));

    /* One native listener per component class type and port type */
    constexpr auto srcOutputListener = listener<bt_component_source, BT_COMPONENT_CLASS_TYPE_SOURCE,
                                                bt_port_output, BT_PORT_TYPE_OUTPUT>;
    constexpr auto fltInputListener = listener<bt_component_filter, BT_COMPONENT_CLASS_TYPE_FILTER,
                                               bt_port_input, BT_PORT_TYPE_INPUT>;
    constexpr auto fltOutputListener = listener<bt_component_filter, BT_COMPONENT_CLASS_TYPE_FILTER,
                                                bt_port_output, BT_PORT_TYPE_OUTPUT>;
    constexpr auto sinkInputListener = listener<bt_component_sink, BT_COMPONENT_CLASS_TYPE_SINK,
                                                bt_port_input, BT_PORT_TYPE_INPUT>;

    /*
     * On error, each library function below already appended a cause
     * to the error of the current thread.
     */
    bt_listener_id srcOutputId = 0;

    if (bt_graph_add_source_component_output_port_added_listener(graph, srcOutputListener,
                                                                 pyCallable, &srcOutputId) !=
        BT_GRAPH_ADD_LISTENER_STATUS_OK) {
        return std::nullopt;
    }

    bt_listener_id fltInputId = 0;

    if (bt_graph_add_filter_component_input_port_added_listener(
            graph, fltInputListener, pyCallable, &fltInputId) != BT_GRAPH_ADD_LISTENER_STATUS_OK) {
        return std::nullopt;
    }

    bt_listener_id fltOutputId = 0;

    if (bt_graph_add_filter_component_output_port_added_listener(graph, fltOutputListener,
                                                                 pyCallable, &fltOutputId) !=
        BT_GRAPH_ADD_LISTENER_STATUS_OK) {
        return std::nullopt;
    }

    bt_listener_id sinkInputId = 0;

    if (bt_graph_add_sink_component_input_port_added_listener(graph, sinkInputListener, pyCallable,
                                                              &sinkInputId) !=
        BT_GRAPH_ADD_LISTENER_STATUS_OK) {
        return std::nullopt;
    }

    return std::make_tuple(srcOutputId, fltInputId, fltOutputId, sinkInputId);
}

} /* namespace */

void registerGraphApi(PyModuleBuilder& mb)
{
    mb.addLibFunc<bt_graph_create>();
    mb.addLibFunc<bt_graph_get_ref>();
    mb.addLibFunc<bt_graph_put_ref>();

    mb.addFunc<addComp<bt2::ConstSourceComponentClass>>("graph_add_source_component");
    mb.addFunc<addComp<bt2::ConstFilterComponentClass>>("graph_add_filter_component");
    mb.addFunc<addComp<bt2::ConstSinkComponentClass>>("graph_add_sink_component");

    mb.addFunc<connectPorts>("graph_connect_ports");

    mb.addLibFunc<bt_graph_run>();
    mb.addLibFunc<bt_graph_run_once>();

    mb.addLibFunc<bt_graph_add_interrupter>();
    mb.addLibFunc<bt_graph_borrow_default_interrupter>();

    mb.addFunc<addPortAddedListener>("graph_add_port_added_listener");
}

} /* namespace bt2py */
