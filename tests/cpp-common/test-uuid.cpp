/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2024 EfficiOS, Inc.
 */

#include "cpp-common/bt2c/fmt.hpp"
#include "cpp-common/bt2c/uuid.hpp"
#include "cpp-common/vendor/fmt/format.h"

#include "tap/tap.h"

namespace {

constexpr auto uuidStr = "c2281e4a-699b-4b78-903f-2f8407fe2b77";
const bt2c::Uuid uuid {uuidStr};
const bt2c::UuidView uuidView {uuid};

void testFormatAs()
{
    const auto resUuid = fmt::to_string(uuid);
    const auto resUuidView = fmt::to_string(uuidView);

    ok(resUuid == uuidStr, "result of format_as() for `Uuid` is expected");
    ok(resUuidView == uuidStr, "result of format_as() for `UuidView` is expected");
}

} /* namespace */

int main()
{
    plan_tests(2);
    testFormatAs();
    return exit_status();
}
