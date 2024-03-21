/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2024 EfficiOS Inc.
 */

#include "cpp-common/bt2s/make-unique.hpp"
#include "cpp-common/vendor/fmt/format.h" /* IWYU pragma: keep */

#include "../utils/run-in.hpp"
#include "clk-cls-compat-postconds-triggers.hpp"

namespace {

/*
 * `RunIn` implementation to trigger clock (in)compatibility postcondition
 * assertions.
 */
class ClockClsCompatRunIn final : public RunIn
{
public:
    enum class MsgType
    {
        STREAM_BEG,
        MSG_ITER_INACTIVITY,
    };

    using CreateClockCls = bt2::ClockClass::Shared (*)(bt2::SelfComponent);

    explicit ClockClsCompatRunIn(const MsgType msgType1, const CreateClockCls createClockCls1,
                                 const MsgType msgType2,
                                 const CreateClockCls createClockCls2) noexcept :
        _mMsgType1 {msgType1},
        _mMsgType2 {msgType2}, _mCreateClockCls1 {createClockCls1}, _mCreateClockCls2 {
                                                                        createClockCls2}
    {
    }

    void onMsgIterNext(bt2::SelfMessageIterator self, bt2::ConstMessageArray& msgs) override
    {
        /* In case the expected assertion doesn't trigger, avoid looping indefinitely. */
        BT_ASSERT(!_mBeenThere);

        const auto traceCls = self.component().createTraceClass();
        const auto trace = traceCls->instantiate();

        msgs.append(this->_createOneMsg(self, _mMsgType1, _mCreateClockCls1, *trace));
        msgs.append(this->_createOneMsg(self, _mMsgType2, _mCreateClockCls2, *trace));
        _mBeenThere = true;
    }

private:
    static bt2::Message::Shared _createOneMsg(const bt2::SelfMessageIterator self,
                                              const MsgType msgType,
                                              const CreateClockCls createClockCls,
                                              const bt2::Trace trace)
    {
        const auto clockCls = createClockCls(self.component());

        switch (msgType) {
        case MsgType::STREAM_BEG:
        {
            const auto streamCls = trace.cls().createStreamClass();

            if (clockCls) {
                streamCls->defaultClockClass(*clockCls);
            }

            return self.createStreamBeginningMessage(*streamCls->instantiate(trace));
        }

        case MsgType::MSG_ITER_INACTIVITY:
            BT_ASSERT(clockCls);
            return self.createMessageIteratorInactivityMessage(*clockCls, 12);
        };

        bt_common_abort();
    }

    MsgType _mMsgType1, _mMsgType2;
    CreateClockCls _mCreateClockCls1, _mCreateClockCls2;
    bool _mBeenThere = false;
};

__attribute__((used)) const char *format_as(const ClockClsCompatRunIn::MsgType msgType)
{
    switch (msgType) {
    case ClockClsCompatRunIn::MsgType::STREAM_BEG:
        return "sb";

    case ClockClsCompatRunIn::MsgType::MSG_ITER_INACTIVITY:
        return "mii";
    }

    bt_common_abort();
}

bt2::ClockClass::Shared noClockClass(bt2::SelfComponent) noexcept
{
    return bt2::ClockClass::Shared {};
}

const bt2c::Uuid uuidA {"f00aaf65-ebec-4eeb-85b2-fc255cf1aa8a"};
const bt2c::Uuid uuidB {"03482981-a77b-4d7b-94c4-592bf9e91785"};

} /* namespace */

/*
 * Add clock class compatibility postcondition failures triggers.
 *
 * Each trigger below makes a message iterator return two messages with
 * incompatible clock classes, leading to a postcondition failure.
 */
void addClkClsCompatTriggers(CondTriggers& triggers)
{
    const auto addValidCases = [&triggers](
                                   const ClockClsCompatRunIn::CreateClockCls createClockCls1,
                                   const ClockClsCompatRunIn::CreateClockCls createClockCls2,
                                   const char * const condId) {
        /*
         * Add triggers for all possible combinations of message types.
         *
         * It's not possible to create message iterator inactivity messages
         * without a clock class.
         */
        static constexpr std::array<ClockClsCompatRunIn::MsgType, 2> msgTypes {
            ClockClsCompatRunIn::MsgType::STREAM_BEG,
            ClockClsCompatRunIn::MsgType::MSG_ITER_INACTIVITY,
        };

        const auto isInvalidCase = [](const ClockClsCompatRunIn::MsgType msgType,
                                      const ClockClsCompatRunIn::CreateClockCls createClockCls) {
            return msgType == ClockClsCompatRunIn::MsgType::MSG_ITER_INACTIVITY &&
                   createClockCls == noClockClass;
        };

        for (const auto msgType1 : msgTypes) {
            if (isInvalidCase(msgType1, createClockCls1)) {
                continue;
            }

            for (const auto msgType2 : msgTypes) {
                if (isInvalidCase(msgType2, createClockCls2)) {
                    continue;
                }

                triggers.emplace_back(bt2s::make_unique<RunInCondTrigger<ClockClsCompatRunIn>>(
                    ClockClsCompatRunIn {msgType1, createClockCls1, msgType2, createClockCls2},
                    CondTrigger::Type::POST, condId, fmt::format("{}-{}", msgType1, msgType2)));
            }
        }
    };

    addValidCases(
        noClockClass,
        [](const bt2::SelfComponent self) {
            return self.createClockClass();
        },
        "message-iterator-class-next-method:stream-class-has-no-clock-class");

    addValidCases(
        [](const bt2::SelfComponent self) {
            return self.createClockClass();
        },
        noClockClass,
        "message-iterator-class-next-method:stream-class-has-clock-class-with-unix-epoch-origin");

    addValidCases(
        [](const bt2::SelfComponent self) {
            return self.createClockClass();
        },
        [](const bt2::SelfComponent self) {
            const auto clockCls = self.createClockClass();

            clockCls->originIsUnixEpoch(false);
            return clockCls;
        },
        "message-iterator-class-next-method:clock-class-has-unix-epoch-origin");

    addValidCases(
        [](const bt2::SelfComponent self) {
            const auto clockCls = self.createClockClass();

            clockCls->originIsUnixEpoch(false).uuid(uuidA);
            return clockCls;
        },
        noClockClass, "message-iterator-class-next-method:stream-class-has-clock-class-with-uuid");

    addValidCases(
        [](const bt2::SelfComponent self) {
            const auto clockCls = self.createClockClass();

            clockCls->originIsUnixEpoch(false).uuid(uuidA);
            return clockCls;
        },
        [](const bt2::SelfComponent self) {
            return self.createClockClass();
        },
        "message-iterator-class-next-method:clock-class-has-non-unix-epoch-origin");

    addValidCases(
        [](const bt2::SelfComponent self) {
            const auto clockCls = self.createClockClass();

            clockCls->originIsUnixEpoch(false).uuid(uuidA);
            return clockCls;
        },
        [](const bt2::SelfComponent self) {
            const auto clockCls = self.createClockClass();

            clockCls->originIsUnixEpoch(false);
            return clockCls;
        },
        "message-iterator-class-next-method:clock-class-has-uuid");

    addValidCases(
        [](const bt2::SelfComponent self) {
            const auto clockCls = self.createClockClass();

            clockCls->originIsUnixEpoch(false).uuid(uuidA);
            return clockCls;
        },
        [](const bt2::SelfComponent self) {
            const auto clockCls = self.createClockClass();

            clockCls->originIsUnixEpoch(false).uuid(uuidB);
            return clockCls;
        },
        "message-iterator-class-next-method:clock-class-has-expected-uuid");

    addValidCases(
        [](const bt2::SelfComponent self) {
            const auto clockCls = self.createClockClass();

            clockCls->originIsUnixEpoch(false);
            return clockCls;
        },
        noClockClass, "message-iterator-class-next-method:stream-class-has-clock-class");

    addValidCases(
        [](const bt2::SelfComponent self) {
            const auto clockCls = self.createClockClass();

            clockCls->originIsUnixEpoch(false);
            return clockCls;
        },
        [](const bt2::SelfComponent self) {
            const auto clockCls = self.createClockClass();

            clockCls->originIsUnixEpoch(false);
            return clockCls;
        },
        "message-iterator-class-next-method:clock-class-is-expected");
}
