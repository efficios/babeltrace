/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2023 EfficiOS Inc.
 */

#include "common/assert.h"

#include "utils/run-in.hpp"

#include "tap/tap.h"

namespace {

constexpr int NR_TESTS = 2;

class TestStringClear final : public RunIn
{
public:
    void onMsgIterInit(const bt2::SelfMessageIterator self) override
    {
        /* Boilerplate to get a string field */
        const auto traceCls = self.component().createTraceClass();
        const auto streamCls = traceCls->createStreamClass();
        const auto eventCls = streamCls->createEventClass();
        const auto payloadCls = traceCls->createStructureFieldClass();

        payloadCls->appendMember("str", *traceCls->createStringFieldClass());
        eventCls->payloadFieldClass(*payloadCls);

        const auto trace = traceCls->instantiate();
        const auto stream = streamCls->instantiate(*trace);
        const auto msg = self.createEventMessage(*eventCls, *stream);
        const auto field = (*msg->event().payloadField())["str"]->asString();

        /* Set the field to a known non-empty value */
        *field = "pomme";
        BT_ASSERT(field.value() == "pomme");

        /* Clear the field, verify its value and length */
        field.clear();
        ok(field.value() == "", "string field is empty");
        ok(field.length() == 0, "string field length is 0");
    }
};

} /* namespace */

int main()
{
    plan_tests(NR_TESTS);

    TestStringClear testStringClear;
    runIn(testStringClear);

    return exit_status();
}
