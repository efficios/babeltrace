/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2024 EfficiOS Inc.
 */

#include "cpp-common/bt2/component-class-dev.hpp"
#include "cpp-common/bt2/component-class.hpp"
#include "cpp-common/bt2/error.hpp"
#include "cpp-common/bt2/graph.hpp"
#include "cpp-common/bt2/plugin-load.hpp"
#include "cpp-common/bt2c/call.hpp"
#include "cpp-common/vendor/fmt/format.h" /* IWYU pragma: keep */

#include "tap/tap.h"

namespace {

/* The types of messages a `TestSourceIter` is instructed to send. */
enum class MsgType
{
    /* Send stream beginning and stream end messages. */
    STREAM,

    /* Send a message iterator inactivity message. */
    MSG_ITER_INACTIVITY,
};

__attribute__((used)) const char *format_as(MsgType msgType) noexcept
{
    switch (msgType) {
    case MsgType::STREAM:
        return "stream beginning/end";

    case MsgType::MSG_ITER_INACTIVITY:
        return "message iterator inactivity";
    }

    bt_common_abort();
}

using CreateClockClass = bt2::ClockClass::Shared (*)(bt2::SelfComponent);

struct TestSourceData final
{
    /* The function to call to obtain a clock class. */
    CreateClockClass createClockClass;

    /* The type of messages to send. */
    MsgType msgType;

    /* If not empty, the clock snapshot to set on the message. */
    bt2s::optional<std::uint64_t> clockSnapshot;
};

class TestSource;

class TestSourceIter final : public bt2::UserMessageIterator<TestSourceIter, TestSource>
{
    friend bt2::UserMessageIterator<TestSourceIter, TestSource>;

public:
    explicit TestSourceIter(const bt2::SelfMessageIterator self,
                            bt2::SelfMessageIteratorConfiguration,
                            const bt2::SelfComponentOutputPort port) :
        bt2::UserMessageIterator<TestSourceIter, TestSource> {self, "TEST-SRC-MSG-ITER"},
        _mData {&port.data<const TestSourceData>()}, _mSelf {self}
    {
    }

private:
    void _next(bt2::ConstMessageArray& msgs)
    {
        if (_mDone) {
            return;
        }

        const auto clockCls = _mData->createClockClass(_mSelf.component());

        switch (_mData->msgType) {
        case MsgType::STREAM:
        {
            const auto traceCls = _mSelf.component().createTraceClass();
            const auto streamCls = traceCls->createStreamClass();

            if (clockCls) {
                streamCls->defaultClockClass(*clockCls);
            }

            const auto stream = streamCls->instantiate(*traceCls->instantiate());

            /* Create stream beginning message. */
            msgs.append(bt2c::call([&] {
                const auto streamBeginningMsg = this->_createStreamBeginningMessage(*stream);

                /* Set clock snapshot if instructed to. */
                if (_mData->clockSnapshot) {
                    streamBeginningMsg->defaultClockSnapshot(*_mData->clockSnapshot);
                }

                return streamBeginningMsg;
            }));

            /*
             * The iterator needs to send a stream end message to avoid
             * a postcondition assertion failure, where it's ended but
             * didn't end all streams.
             *
             * The stream end messages don't play a role in the test
             * otherwise.
             */
            msgs.append(this->_createStreamEndMessage(*stream));
            break;
        }

        case MsgType::MSG_ITER_INACTIVITY:
            msgs.append(
                this->_createMessageIteratorInactivityMessage(*clockCls, *_mData->clockSnapshot));
            break;
        }

        _mDone = true;
    }

    bool _mDone = false;
    const TestSourceData *_mData;
    bt2::SelfMessageIterator _mSelf;
};

class TestSource final : public bt2::UserSourceComponent<TestSource, TestSourceIter, TestSourceData>
{
    friend class TestSourceIter;

    using _ThisUserSourceComponent =
        bt2::UserSourceComponent<TestSource, TestSourceIter, TestSourceData>;

public:
    static constexpr auto name = "test-source";

    explicit TestSource(const bt2::SelfSourceComponent self, bt2::ConstMapValue,
                        TestSourceData * const data) :
        _ThisUserSourceComponent {self, "TEST-SRC"},
        _mData {*data}
    {
        this->_addOutputPort("out", _mData);
    }

private:
    TestSourceData _mData;
};

class ErrorTestCase final
{
public:
    /* Intentionally not explicit */
    ErrorTestCase(CreateClockClass createClockClass1Param, CreateClockClass createClockClass2Param,
                  const char * const testName, const char * const expectedCauseMsg) :
        _mCreateClockClass1 {createClockClass1Param},
        _mCreateClockClass2 {createClockClass2Param}, _mTestName {testName}, _mExpectedCauseMsg {
                                                                                 expectedCauseMsg}
    {
    }

    void run() const noexcept;

private:
    void _runOne(MsgType msgType1, MsgType msgType2) const noexcept;

    CreateClockClass _mCreateClockClass1;
    CreateClockClass _mCreateClockClass2;
    const char *_mTestName;
    const char *_mExpectedCauseMsg;
};

bt2::ClockClass::Shared noClockClass(bt2::SelfComponent) noexcept
{
    return bt2::ClockClass::Shared {};
}

void ErrorTestCase::run() const noexcept
{
    static constexpr std::array<MsgType, 2> msgTypes {
        MsgType::STREAM,
        MsgType::MSG_ITER_INACTIVITY,
    };

    for (const auto msgType1 : msgTypes) {
        for (const auto msgType2 : msgTypes) {
            /*
             * It's not possible to create message iterator inactivity
             * messages without a clock class. Skip those cases.
             */
            if ((msgType1 == MsgType::MSG_ITER_INACTIVITY && _mCreateClockClass1 == noClockClass) ||
                (msgType2 == MsgType::MSG_ITER_INACTIVITY && _mCreateClockClass2 == noClockClass)) {
                continue;
            }

            /*
             * The test scenarios depend on the message with the first
             * clock class going through the muxer first.
             *
             * Between a message with a clock snapshot and a message
             * without a clock snapshot, the muxer always picks the
             * message without a clock snapshot first.
             *
             * Message iterator inactivity messages always have a clock
             * snapshot. Therefore, if:
             *
             * First message:
             *     A message iterator inactivity message (always has a
             *     clock snapshot).
             *
             * Second message:
             *     Doesn't have a clock class (never has a clock
             *     snapshot).
             *
             * Then there's no way for the first message to go through
             * first.
             *
             * Skip those cases.
             */
            if (msgType1 == MsgType::MSG_ITER_INACTIVITY && _mCreateClockClass2 == noClockClass) {
                continue;
            }

            this->_runOne(msgType1, msgType2);
        }
    }
}

std::string makeSpecTestName(const char * const testName, const MsgType msgType1,
                             const MsgType msgType2)
{
    return fmt::format("{} ({}, {})", testName, msgType1, msgType2);
}

void ErrorTestCase::_runOne(const MsgType msgType1, const MsgType msgType2) const noexcept
{
    const auto specTestName = makeSpecTestName(_mTestName, msgType1, msgType2);
    const auto srcCompCls = bt2::SourceComponentClass::create<TestSource>();
    const auto graph = bt2::Graph::create(0);

    {
        /*
         * The test scenarios depend on the message with the first clock class going through the
         * muxer first. Between a message with a clock snapshot and a message without a clock
         * snapshot, the muxer always picks the message without a clock snapshot first.
         *
         * Therefore, for the first message, only set a clock snapshot when absolutely necessary,
         * that is when the message type is "message iterator inactivity".
         *
         * For the second message, always set a clock snapshot when possible, that is when a clock
         * class is defined for that message.
         */
        const auto srcComp1 =
            graph->addComponent(*srcCompCls, "source-1",
                                TestSourceData {_mCreateClockClass1, msgType1,
                                                msgType1 == MsgType::MSG_ITER_INACTIVITY ?
                                                    bt2s::optional<std::uint64_t> {10} :
                                                    bt2s::nullopt});
        const auto srcComp2 =
            graph->addComponent(*srcCompCls, "source-2",
                                TestSourceData {_mCreateClockClass2, msgType2,
                                                _mCreateClockClass2 != noClockClass ?
                                                    bt2s::optional<std::uint64_t> {20} :
                                                    bt2s::nullopt});

        const auto utilsPlugin = bt2::findPlugin("utils");

        BT_ASSERT(utilsPlugin);

        /* Add muxer component */
        const auto muxerComp = bt2c::call([&] {
            const auto muxerCompCls = utilsPlugin->filterComponentClasses()["muxer"];

            BT_ASSERT(muxerCompCls);

            return graph->addComponent(*muxerCompCls, "the-muxer");
        });

        /* Add dummy sink component */
        const auto sinkComp = bt2c::call([&] {
            const auto dummySinkCompCls = utilsPlugin->sinkComponentClasses()["dummy"];

            BT_ASSERT(dummySinkCompCls);

            return graph->addComponent(*dummySinkCompCls, "the-sink");
        });

        /* Connect ports */
        graph->connectPorts(*srcComp1.outputPorts()["out"], *muxerComp.inputPorts()["in0"]);
        graph->connectPorts(*srcComp2.outputPorts()["out"], *muxerComp.inputPorts()["in1"]);
        graph->connectPorts(*muxerComp.outputPorts()["out"], *sinkComp.inputPorts()["in"]);
    }

    const auto thrown = bt2c::call([&graph] {
        try {
            graph->run();
        } catch (const bt2::Error&) {
            return true;
        }

        return false;
    });

    ok(thrown, "%s - `bt2::Error` thrown", specTestName.c_str());

    const auto error = bt2::takeCurrentThreadError();

    ok(error, "%s - current thread has an error", specTestName.c_str());
    ok(error.length() > 0, "%s - error has at least one cause", specTestName.c_str());

    const auto cause = error[0];

    if (!ok(cause.message().startsWith(_mExpectedCauseMsg), "%s - cause's message is expected",
            specTestName.c_str())) {
        diag("expected: %s", _mExpectedCauseMsg);
        diag("actual: %s", cause.message().data());
    }

    ok(cause.actorTypeIsMessageIterator(), "%s - cause's actor type is message iterator",
       specTestName.c_str());
    ok(cause.asMessageIterator().componentName() == "the-muxer",
       "%s - causes's component name is `the-muxer`", specTestName.c_str());
}

const bt2c::Uuid uuidA {"f00aaf65-ebec-4eeb-85b2-fc255cf1aa8a"};
const bt2c::Uuid uuidB {"03482981-a77b-4d7b-94c4-592bf9e91785"};

const ErrorTestCase errorTestCases[] = {
    {noClockClass,
     [](const bt2::SelfComponent self) {
         return self.createClockClass();
     },
     "no clock class followed by clock class", "Expecting no clock class, but got one"},

    {[](const bt2::SelfComponent self) {
         return self.createClockClass();
     },
     noClockClass, "clock class with Unix epoch origin followed by no clock class",
     "Expecting a clock class, but got none"},

    {[](const bt2::SelfComponent self) {
         return self.createClockClass();
     },
     [](const bt2::SelfComponent self) {
         const auto clockCls = self.createClockClass();

         clockCls->originIsUnixEpoch(false);
         return clockCls;
     },
     "clock class with Unix epoch origin followed by clock class with other origin",
     "Expecting a clock class having a Unix epoch origin, but got one not having a Unix epoch origin"},

    {[](const bt2::SelfComponent self) {
         const auto clockCls = self.createClockClass();

         clockCls->originIsUnixEpoch(false).uuid(uuidA);
         return clockCls;
     },
     noClockClass, "clock class with other origin and a UUID followed by no clock class",
     "Expecting a clock class, but got none"},

    {[](const bt2::SelfComponent self) {
         const auto clockCls = self.createClockClass();

         clockCls->originIsUnixEpoch(false).uuid(uuidA);
         return clockCls;
     },
     [](const bt2::SelfComponent self) {
         return self.createClockClass();
     },
     "clock class with other origin and a UUID followed by clock class with Unix epoch origin",
     "Expecting a clock class not having a Unix epoch origin, but got one having a Unix epoch origin"},

    {[](const bt2::SelfComponent self) {
         const auto clockCls = self.createClockClass();

         clockCls->originIsUnixEpoch(false).uuid(uuidA);
         return clockCls;
     },
     [](const bt2::SelfComponent self) {
         const auto clockCls = self.createClockClass();

         clockCls->originIsUnixEpoch(false);
         return clockCls;
     },
     "clock class with other origin and a UUID followed by clock class with other origin and no UUID",
     "Expecting a clock class with a UUID, but got one without a UUID"},

    {[](const bt2::SelfComponent self) {
         const auto clockCls = self.createClockClass();

         clockCls->originIsUnixEpoch(false).uuid(uuidA);
         return clockCls;
     },
     [](const bt2::SelfComponent self) {
         const auto clockCls = self.createClockClass();

         clockCls->originIsUnixEpoch(false).uuid(uuidB);
         return clockCls;
     },
     "clock class with other origin and a UUID followed by clock class with other origin and another UUID",
     "Expecting a clock class with a specific UUID, but got one with a different UUID"},

    {[](const bt2::SelfComponent self) {
         const auto clockCls = self.createClockClass();

         clockCls->originIsUnixEpoch(false);
         return clockCls;
     },
     noClockClass, "clock class with other origin and no UUID followed by no clock class",
     "Expecting a clock class, but got none"},

    {[](const bt2::SelfComponent self) {
         const auto clockCls = self.createClockClass();

         clockCls->originIsUnixEpoch(false);
         return clockCls;
     },

     [](const bt2::SelfComponent self) {
         const auto clockCls = self.createClockClass();

         clockCls->originIsUnixEpoch(false);
         return clockCls;
     },
     "clock class with other origin and no UUID followed by different clock class",
     "Unexpected clock class"},
};

} /* namespace */

int main()
{
    plan_tests(150);

    for (auto& errorTestCase : errorTestCases) {
        errorTestCase.run();
    }

    return exit_status();
}
