/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#include <Python.h>

#include <babeltrace2/babeltrace.h>

#include "cpp-common/bt2/logging.hpp"

#include "../api.hpp"
#include "../runtime/py-module-builder.hpp"

namespace bt2py {

void registerLoggingApi(PyModuleBuilder& mb)
{
    mb.addWiseEnumConstants<bt2::LoggingLevel>("LOGGING_LEVEL_");

    mb.addLibFunc<bt_logging_get_minimal_level>();
    mb.addLibFunc<bt_logging_get_global_level>();
    mb.addLibFunc<bt_logging_set_global_level>();
}

} /* namespace bt2py */
