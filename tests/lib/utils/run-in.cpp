/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2020-2023 EfficiOS, inc.
 */

#include <utility>

#include "common/assert.h"
#include "cpp-common/bt2/component-class-dev.hpp"
#include "cpp-common/bt2/component-class.hpp"
#include "cpp-common/bt2/graph.hpp"
#include "cpp-common/bt2/query-executor.hpp"

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
    bt2::QueryExecutor::create(*srcCompCls, "object-name", data)->query();

    /* Create graph */
    const auto graph = bt2::Graph::create(0);

    /* Add custom source component (executes `compCtxFunc`) */
    const auto srcComp = graph->addComponent(*srcCompCls, "the-source", data);

    /* Add dummy sink component */
    const auto sinkComp =
        graph->addComponent(*bt2::SinkComponentClass::create<DummySink>(), "the-sink");

    /* Connect ports */
    const auto outPort = srcComp.outputPorts()["out"];
    BT_ASSERT(outPort);

    const auto inPort = sinkComp.inputPorts()["in"];
    BT_ASSERT(inPort);

    graph->connectPorts(*outPort, *inPort);

    /* Run graph (executes `msgIterCtxFunc`) */
    graph->run();
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
