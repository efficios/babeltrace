/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2024 EfficiOS, Inc.
 */

#include <cpp-common/bt2/plugin.hpp>

#include <cpp-common/vendor/fmt/format.h>

#include <babeltrace2/babeltrace.h>

#include "cpp-common/bt2c/c-string-view.hpp"

#include "tap/tap.h"

namespace {

void testFailOnLoadErrorTrue(const char * const pluginDir)
{
    plan_tests(1);

    try {
        bt2::findAllPluginsFromDir(pluginDir, false, true);
        bt_common_abort();
    } catch (const bt2::Error& exc) {
        fmt::print("{}\n", exc.what());

        const auto error = bt_current_thread_take_error();

        /*
         * The last error cause must be the one which the initialization
         * function of our plugin appended.
         */
        const auto cause = bt_error_borrow_cause_by_index(error, 0);
        const bt2c::CStringView msg {bt_error_cause_get_message(cause)};

        ok(msg == "This is the error message", "Message of error cause 0 is expected");
        bt_error_release(error);
    }
}

void testFailOnLoadErrorFalse(const char * const pluginDir)
{
    plan_tests(1);

    const auto plugins = bt2::findAllPluginsFromDir(pluginDir, false, false);

    ok(!plugins, "No plugin set returned");
}

} /* namespace */

int main(const int argc, const char ** const argv)
{
    if (argc != 3) {
        fmt::print(stderr,
                   "Usage: {} INIT-FAIL-PLUGIN-DIR FAIL-ON-LOAD-ERROR\n"
                   "\n"
                   "FAIL-ON-LOAD-ERROR must be `yes` or `no`\n",
                   argv[0]);
        return 1;
    }

    const auto pluginDir = argv[1];
    const bt2c::CStringView failOnLoadErrorStr {argv[2]};

    if (failOnLoadErrorStr == "yes") {
        testFailOnLoadErrorTrue(pluginDir);
    } else if (failOnLoadErrorStr == "no") {
        testFailOnLoadErrorFalse(pluginDir);
    } else {
        fmt::print(stderr,
                   "ERROR: Invalid value `{}` for FAIL-ON-LOAD-ERROR (expecting `yes` or `no`).\n",
                   failOnLoadErrorStr);
        return 1;
    }

    return exit_status();
}
