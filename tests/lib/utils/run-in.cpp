/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2020-2023 EfficiOS, inc.
 */

#include "common/assert.h"
#include "cpp-common/bt2/component-class-dev.hpp"
#include "cpp-common/bt2/component-class.hpp"
#include "cpp-common/bt2/graph.hpp"
#include "cpp-common/bt2/plugin-load.hpp"
#include "cpp-common/bt2/plugin.hpp"
#include "cpp-common/bt2/query-executor.hpp"
#include "cpp-common/bt2c/call.hpp"

#include "run-in.hpp"

void RunIn::onQuery(bt2::SelfComponentClass)
{
}

void RunIn::onCompInit(bt2::SelfComponent)
{
}

void RunIn::onMsgIterInit(bt2::SelfMessageIterator)
{
}

void RunIn::onMsgIterNext(bt2::SelfMessageIterator, bt2::ConstMessageArray&)
{
}

namespace {

class RunInSource;

class RunInSourceMsgIter final : public bt2::UserMessageIterator<RunInSourceMsgIter, RunInSource>
{
public:
    explicit RunInSourceMsgIter(const bt2::SelfMessageIterator self,
                                bt2::SelfMessageIteratorConfiguration,
                                const bt2::SelfComponentOutputPort port) :
        bt2::UserMessageIterator<RunInSourceMsgIter, RunInSource> {self, "RUN-IN-SRC-MSG-ITER"},
        _mRunIn {&port.data<RunIn>()}, _mSelf {self}
    {
        _mRunIn->onMsgIterInit(self);
    }

    void _next(bt2::ConstMessageArray& msgs)
    {
        _mRunIn->onMsgIterNext(_mSelf, msgs);
    }

private:
    RunIn *_mRunIn;
    bt2::SelfMessageIterator _mSelf;
};

class RunInSource final :
    public bt2::UserSourceComponent<RunInSource, RunInSourceMsgIter, RunIn, RunIn>
{
public:
    static constexpr auto name = "run-in-src";

    explicit RunInSource(const bt2::SelfSourceComponent self, bt2::ConstMapValue,
                         RunIn * const runIn) :
        bt2::UserSourceComponent<RunInSource, RunInSourceMsgIter, RunIn, RunIn> {self,
                                                                                 "RUN-IN-SRC"},
        _mRunIn {runIn}
    {
        this->_addOutputPort("out", *runIn);
        _mRunIn->onCompInit(self);
    }

    static bt2::Value::Shared _query(const bt2::SelfComponentClass self, bt2::PrivateQueryExecutor,
                                     bt2c::CStringView, bt2::ConstValue, RunIn *data)
    {
        data->onQuery(self);
        return bt2::NullValue {}.shared();
    }

private:
    RunIn *_mRunIn;
};

} /* namespace */

void runIn(RunIn& runIn)
{
    const auto srcCompCls = bt2::SourceComponentClass::create<RunInSource>();

    /* Execute a query */
    bt2::QueryExecutor::create(*srcCompCls, "object-name", runIn)->query();

    /* Create graph */
    const auto graph = bt2::Graph::create(0);

    /* Add custom source component (executes `compCtxFunc`) */
    const auto srcComp = graph->addComponent(*srcCompCls, "the-source", runIn);

    /* Add dummy sink component */
    const auto sinkComp = bt2c::call([&] {
        const auto utilsPlugin = bt2::findPlugin("utils");

        BT_ASSERT(utilsPlugin);

        const auto dummySinkCompCls = utilsPlugin->sinkComponentClasses()["dummy"];

        BT_ASSERT(dummySinkCompCls);

        return graph->addComponent(*dummySinkCompCls, "the-sink");
    });

    /* Connect ports */
    const auto outPort = srcComp.outputPorts()["out"];
    BT_ASSERT(outPort);

    const auto inPort = sinkComp.inputPorts()["in"];
    BT_ASSERT(inPort);

    graph->connectPorts(*outPort, *inPort);

    /* Run graph (executes `msgIterCtxFunc`) */
    graph->run();
}
