/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2020-2023 EfficiOS, inc.
 */

#include <utility>

#include "common/assert.h"
#include "cpp-common/bt2/component-class-dev.hpp"
#include "cpp-common/bt2/component-class.hpp"

#include "run-in.hpp"

namespace {

struct RunInData final
{
    RunInCompClsQueryFunc compClsCtxFunc;
    RunInCompClsInitFunc compCtxFunc;
    RunInMsgIterClsInitFunc msgIterCtxFunc;
};

class RunInSource;

class RunInSourceMsgIter final : public bt2::UserMessageIterator<RunInSourceMsgIter, RunInSource>
{
public:
    explicit RunInSourceMsgIter(const bt2::SelfMessageIterator self,
                                bt2::SelfMessageIteratorConfiguration,
                                const bt2::SelfComponentOutputPort port) :
        bt2::UserMessageIterator<RunInSourceMsgIter, RunInSource> {self, "RUN-IN-SRC-MSG-ITER"}
    {
        const auto& data = port.data<const RunInData>();

        if (data.msgIterCtxFunc) {
            data.msgIterCtxFunc(self);
        }
    }

    void _next(bt2::ConstMessageArray&)
    {
    }
};

class RunInSource final :
    public bt2::UserSourceComponent<RunInSource, RunInSourceMsgIter, const RunInData,
                                    const RunInData>
{
public:
    static constexpr auto name = "run-in-src";

    explicit RunInSource(const bt2::SelfSourceComponent self, bt2::ConstMapValue,
                         const RunInData * const runInData) :
        bt2::UserSourceComponent<RunInSource, RunInSourceMsgIter, const RunInData,
                                 const RunInData> {self, "RUN-IN-SRC"},
        _mRunInData {runInData}
    {
        this->_addOutputPort("out", *runInData);

        if (_mRunInData->compCtxFunc) {
            _mRunInData->compCtxFunc(self);
        }
    }

    static bt2::Value::Shared _query(const bt2::SelfComponentClass self, bt2::PrivateQueryExecutor,
                                     bt2c::CStringView, bt2::ConstValue,
                                     const RunInData * const data)
    {
        if (data->compClsCtxFunc) {
            data->compClsCtxFunc(self);
        }

        return bt2::NullValue {}.shared();
    }

private:
    const RunInData *_mRunInData;
};

class DummySink : public bt2::UserSinkComponent<DummySink>
{
public:
    static constexpr auto name = "dummy";

    explicit DummySink(const bt2::SelfSinkComponent self, bt2::ConstMapValue, void *) :
        bt2::UserSinkComponent<DummySink>(self, "DUMMY-SINK")
    {
        this->_addInputPort("in");
    }

    void _graphIsConfigured()
    {
        _mMsgIter = this->_createMessageIterator(this->_inputPorts()["in"]);
    }

    bool _consume()
    {
        return _mMsgIter->next().has_value();
    }

private:
    bt2::MessageIterator::Shared _mMsgIter;
};

} /* namespace */

void runIn(RunInCompClsQueryFunc compClsCtxFunc, RunInCompClsInitFunc compCtxFunc,
           RunInMsgIterClsInitFunc msgIterCtxFunc)
{
    RunInData data {std::move(compClsCtxFunc), std::move(compCtxFunc), std::move(msgIterCtxFunc)};
    const auto srcCompCls = bt2::SourceComponentClass::create<RunInSource>();

    /* Execute a query (executes `compClsCtxFunc`) */
    {
        const auto queryExec = bt_query_executor_create_with_method_data(
            bt_component_class_source_as_component_class(srcCompCls->libObjPtr()), "", nullptr,
            &data);

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
            graph, srcCompCls->libObjPtr(), "the-source", NULL, &data, BT_LOGGING_LEVEL_NONE,
            &srcComp);

        BT_ASSERT(status == BT_GRAPH_ADD_COMPONENT_STATUS_OK);
    }

    /* Add dummy sink component */
    const bt_component_sink *sinkComp;

    {
        const auto sinkCompCls = bt2::SinkComponentClass::create<DummySink>();
        const auto status = bt_graph_add_sink_component_with_initialize_method_data(
            graph, sinkCompCls->libObjPtr(), "the-sink", nullptr, nullptr, BT_LOGGING_LEVEL_NONE,
            &sinkComp);

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
    {
        const auto status = bt_graph_run(graph);

        BT_ASSERT(status == BT_GRAPH_RUN_STATUS_OK);
    }

    /* Discard owned objects */
    bt_graph_put_ref(graph);
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
