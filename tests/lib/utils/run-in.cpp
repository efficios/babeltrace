/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2020-2023 EfficiOS, inc.
 */

#include <utility>

#include "run-in.hpp"
#include "common/assert.h"

struct RunInData final
{
    RunInCompClsQueryFunc compClsCtxFunc;
    RunInCompClsInitFunc compCtxFunc;
    RunInMsgIterClsInitFunc msgIterCtxFunc;
};

static const RunInData& runInDataFromMethodData(void * const methodData)
{
    return *static_cast<const RunInData *>(methodData);
}

static bt_component_class_initialize_method_status
compClsInit(bt_self_component_source * const selfComp, bt_self_component_source_configuration *,
            const bt_value *, void * const initMethodData)
{
    const auto status =
        bt_self_component_source_add_output_port(selfComp, "out", initMethodData, nullptr);

    BT_ASSERT(status == BT_SELF_COMPONENT_ADD_PORT_STATUS_OK);

    auto& data = runInDataFromMethodData(initMethodData);

    if (data.compCtxFunc) {
        data.compCtxFunc(bt_self_component_source_as_self_component(selfComp));
    }

    return BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK;
}

static bt_component_class_query_method_status
compClsQuery(bt_self_component_class_source * const selfCompCls, bt_private_query_executor *,
             const char *, const bt_value *, void * const methodData,
             const bt_value ** const result)
{
    auto& data = runInDataFromMethodData(methodData);

    if (data.compClsCtxFunc) {
        data.compClsCtxFunc(bt_self_component_class_source_as_self_component_class(selfCompCls));
    }

    *result = bt_value_null;
    return BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_OK;
}

static bt_message_iterator_class_initialize_method_status
msgIterClsInit(bt_self_message_iterator * const selfMsgIter,
               bt_self_message_iterator_configuration *, bt_self_component_port_output * const port)
{
    auto& data = runInDataFromMethodData(bt_self_component_port_get_data(
        bt_self_component_port_output_as_self_component_port(port)));

    if (data.msgIterCtxFunc) {
        data.msgIterCtxFunc(selfMsgIter);
    }

    return BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_OK;
}

static bt_message_iterator_class_next_method_status
msgIterClsNext(bt_self_message_iterator *, bt_message_array_const, uint64_t, uint64_t *)
{
    return BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_END;
}

void runIn(RunInCompClsQueryFunc compClsCtxFunc, RunInCompClsInitFunc compCtxFunc,
           RunInMsgIterClsInitFunc msgIterCtxFunc)
{
    RunInData data {std::move(compClsCtxFunc), std::move(compCtxFunc), std::move(msgIterCtxFunc)};

    /* Create and configure custom source component class */
    const auto msgIterCls = bt_message_iterator_class_create(msgIterClsNext);

    BT_ASSERT(msgIterCls);

    {
        const auto status =
            bt_message_iterator_class_set_initialize_method(msgIterCls, msgIterClsInit);

        BT_ASSERT(status == BT_MESSAGE_ITERATOR_CLASS_SET_METHOD_STATUS_OK);
    }

    const auto srcCompCls = bt_component_class_source_create("yo", msgIterCls);

    BT_ASSERT(srcCompCls);

    {
        const auto status =
            bt_component_class_source_set_initialize_method(srcCompCls, compClsInit);

        BT_ASSERT(status == BT_COMPONENT_CLASS_SET_METHOD_STATUS_OK);
    }

    {
        const auto status = bt_component_class_source_set_query_method(srcCompCls, compClsQuery);

        BT_ASSERT(status == BT_COMPONENT_CLASS_SET_METHOD_STATUS_OK);
    }

    /* Execute a query (executes `compClsCtxFunc`) */
    {
        const auto queryExec = bt_query_executor_create_with_method_data(
            bt_component_class_source_as_component_class(srcCompCls), "", nullptr, &data);

        BT_ASSERT(queryExec);

        const bt_value *queryRes;
        const auto status = bt_query_executor_query(queryExec, &queryRes);

        BT_ASSERT(status == BT_QUERY_EXECUTOR_QUERY_STATUS_OK);

        bt_value_put_ref(queryRes);
        bt_query_executor_put_ref(queryExec);
    }

    /* Create graph */
    const auto graph = bt_graph_create(0);

    BT_ASSERT(graph);

    /* Add custom source component (executes `compCtxFunc`) */
    const bt_component_source *srcComp;

    {
        const auto status = bt_graph_add_source_component_with_initialize_method_data(
            graph, srcCompCls, "whatever", NULL, &data, BT_LOGGING_LEVEL_NONE, &srcComp);

        BT_ASSERT(status == BT_GRAPH_ADD_COMPONENT_STATUS_OK);
    }

    /* Find `sink.utils.dummy` component class */
    const bt_plugin *utilsPlugin = nullptr;

    {
        const auto status =
            bt_plugin_find("utils", BT_TRUE, BT_TRUE, BT_TRUE, BT_TRUE, BT_TRUE, &utilsPlugin);

        BT_ASSERT(status == BT_PLUGIN_FIND_STATUS_OK);
    }

    /* Add `sink.utils.dummy` component */
    const bt_component_sink *sinkComp;

    {
        const auto dummyCompCls =
            bt_plugin_borrow_sink_component_class_by_name_const(utilsPlugin, "dummy");

        BT_ASSERT(dummyCompCls);

        const auto status = bt_graph_add_sink_component(graph, dummyCompCls, "the-sink", nullptr,
                                                        BT_LOGGING_LEVEL_NONE, &sinkComp);

        BT_ASSERT(status == BT_GRAPH_ADD_COMPONENT_STATUS_OK);
    }

    /* Connect ports */
    {
        const auto outPort = bt_component_source_borrow_output_port_by_name_const(srcComp, "out");

        BT_ASSERT(outPort);

        const auto inPort = bt_component_sink_borrow_input_port_by_name_const(sinkComp, "in");

        BT_ASSERT(inPort);

        const auto status = bt_graph_connect_ports(graph, outPort, inPort, nullptr);

        BT_ASSERT(status == BT_GRAPH_CONNECT_PORTS_STATUS_OK);
    }

    /* Run graph (executes `msgIterCtxFunc`) */
    const auto status = bt_graph_run(graph);

    BT_ASSERT(status == BT_GRAPH_RUN_STATUS_OK);

    /* Discard plugin and graph */
    bt_plugin_put_ref(utilsPlugin);
    bt_graph_put_ref(graph);
    bt_component_class_source_put_ref(srcCompCls);
    bt_message_iterator_class_put_ref(msgIterCls);
}

void runInCompClsQuery(RunInCompClsQueryFunc func)
{
    runIn(std::move(func), nullptr, nullptr);
}

void runInCompClsInit(RunInCompClsInitFunc func)
{
    runIn(nullptr, std::move(func), nullptr);
}

void runInMsgIterClsInit(RunInMsgIterClsInitFunc func)
{
    runIn(nullptr, nullptr, std::move(func));
}
