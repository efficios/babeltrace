/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2020 Philippe Proulx <pproulx@efficios.com>
 */

#include <stdlib.h>
#include <string.h>

#include <babeltrace2/babeltrace.h>

#include "common/assert.h"
#include "cpp-common/vendor/fmt/core.h"
#include "cpp-common/vendor/nlohmann/json.hpp"

#include "../utils/run-in.hpp"
#include "utils.hpp"

namespace {

void runTrigger(const cond_trigger& trigger) noexcept
{
    switch (trigger.func_type) {
    case COND_TRIGGER_FUNC_TYPE_BASIC:
        trigger.func.basic();
        break;
    case COND_TRIGGER_FUNC_TYPE_RUN_IN_COMP_CLS_INIT:
        runInCompClsInit(trigger.func.run_in_comp_cls_init);
        break;
    default:
        abort();
    }
}

void listTriggers(const bt2s::span<const cond_trigger> triggers) noexcept
{
    auto triggerArray = nlohmann::json::array();

    for (auto& trigger : triggers) {
        auto triggerObj = nlohmann::json::object();

        /* Condition ID */
        triggerObj["cond-id"] = trigger.cond_id;

        /* Name starts with condition ID */
        std::string name = trigger.cond_id;

        if (trigger.suffix) {
            name += '-';
            name += trigger.suffix;
        }

        triggerObj["name"] = std::move(name);
        triggerArray.push_back(std::move(triggerObj));
    }

    fmt::println("{}", triggerArray.dump());
}

} /* namespace */

void condMain(const int argc, const char ** const argv,
              const bt2s::span<const cond_trigger> triggers) noexcept
{
    BT_ASSERT(argc >= 2);

    if (strcmp(argv[1], "list") == 0) {
        listTriggers(triggers);
    } else if (strcmp(argv[1], "run") == 0) {
        int index;

        BT_ASSERT(argc >= 3);
        index = atoi(argv[2]);
        BT_ASSERT(index >= 0 && index < triggers.size());
        runTrigger(triggers[index]);
    }
}
