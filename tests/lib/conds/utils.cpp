/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2020 Philippe Proulx <pproulx@efficios.com>
 */

#include <glib.h>
#include <stdlib.h>
#include <string.h>

#include <babeltrace2/babeltrace.h>

#include "common/assert.h"
#include "cpp-common/vendor/fmt/core.h"
#include "cpp-common/vendor/nlohmann/json.hpp"

#include "utils.hpp"

CondTrigger::CondTrigger(const Type type, const std::string& condId,
                         const bt2s::optional<std::string>& nameSuffix) noexcept :
    _mType {type},
    _mCondId {fmt::format("{}:{}", type == Type::PRE ? "pre" : "post", condId)},
    _mName {
        fmt::format("{}{}{}", condId, nameSuffix ? "-" : "", nameSuffix ? nameSuffix->data() : "")}
{
}

SimpleCondTrigger::SimpleCondTrigger(std::function<void()> func, const Type type,
                                     const std::string& condId,
                                     const bt2s::optional<std::string>& nameSuffix) :
    CondTrigger {type, condId, nameSuffix},
    _mFunc {std::move(func)}
{
}

namespace {

void listCondTriggers(const CondTriggers condTriggers) noexcept
{
    auto condTriggerArray = nlohmann::json::array();

    for (const auto condTrigger : condTriggers) {
        condTriggerArray.push_back(nlohmann::json {
            {"cond-id", condTrigger->condId()},
            {"name", condTrigger->name()},
        });
    }

    fmt::println("{}", condTriggerArray.dump());
}

} /* namespace */

void condMain(const int argc, const char ** const argv, const CondTriggers condTriggers) noexcept
{
    BT_ASSERT(argc >= 2);

    if (strcmp(argv[1], "list") == 0) {
        listCondTriggers(condTriggers);
    } else if (strcmp(argv[1], "run") == 0) {
        /*
         * It's expected that calling `*condTriggers[index]` below
         * aborts (calls bt_common_abort()). In this testing context, we
         * don't want any custom abortion command to run.
         */
        g_unsetenv("BABELTRACE_EXEC_ON_ABORT");

        /* Call the trigger */
        BT_ASSERT(argc >= 3);

        const auto index = atoi(argv[2]);

        BT_ASSERT(index >= 0 && index < condTriggers.size());
        (*condTriggers[index])();
    }
}
