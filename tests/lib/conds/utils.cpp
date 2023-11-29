/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2020 Philippe Proulx <pproulx@efficios.com>
 */

#include <iostream>

#include <assert.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <babeltrace2/babeltrace.h>

#include "common/assert.h"
#include "cpp-common/nlohmann/json.hpp"

#include "../utils/run-in.hpp"
#include "utils.hpp"

static void run_trigger(const struct cond_trigger *trigger)
{
    switch (trigger->func_type) {
    case COND_TRIGGER_FUNC_TYPE_BASIC:
        trigger->func.basic();
        break;
    case COND_TRIGGER_FUNC_TYPE_RUN_IN_COMP_CLS_INIT:
        runInCompClsInit(trigger->func.run_in_comp_cls_init);
        break;
    default:
        abort();
    }
}

static void list_triggers(const struct cond_trigger triggers[], size_t trigger_count)
{
    nlohmann::json trigger_array = nlohmann::json::array();

    for (size_t i = 0; i < trigger_count; i++) {
        nlohmann::json trigger_obj = nlohmann::json::object();
        const cond_trigger& trigger = triggers[i];

        /* Condition ID */
        trigger_obj["cond-id"] = trigger.cond_id;

        /* Name starts with condition ID */
        std::string name = trigger.cond_id;

        if (trigger.suffix) {
            name += '-';
            name += trigger.suffix;
        }

        trigger_obj["name"] = std::move(name);
        trigger_array.push_back(std::move(trigger_obj));
    }

    auto str = trigger_array.dump();
    std::cout << str;
    std::flush(std::cout);
}

void cond_main(int argc, const char *argv[], const struct cond_trigger triggers[],
               size_t trigger_count)
{
    BT_ASSERT(argc >= 2);

    if (strcmp(argv[1], "list") == 0) {
        list_triggers(triggers, trigger_count);
    } else if (strcmp(argv[1], "run") == 0) {
        int index;

        BT_ASSERT(argc >= 3);
        index = atoi(argv[2]);
        BT_ASSERT(index >= 0 && index < trigger_count);
        run_trigger(&triggers[index]);
    }
}
