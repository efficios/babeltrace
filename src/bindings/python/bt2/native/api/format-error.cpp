/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#include <Python.h>

#include <string>

#include <babeltrace2/babeltrace.h>

#include "common/assert.h"
#include "common/common.h"
#include "cpp-common/bt2/error.hpp"
#include "string-format/format-error.hpp"

#include "../api.hpp"
#include "../runtime/py-module-builder.hpp"
#include "../runtime/utils.hpp"

namespace bt2py {
namespace {

constexpr auto formatColumns = 80U;

std::string formatErrorCause(const bt2::ConstErrorCause errorCause)
{
    return formatBtErrorCause(errorCause, formatColumns,
                              createLogger("Error cause formatting (Python)"),
                              BT_COMMON_COLOR_WHEN_NEVER);
}

std::string formatError(const bt_error * const error)
{
    BT_ASSERT_DBG(error);
    return formatBtError(bt2::ConstError {error}, formatColumns,
                         createLogger("Error formatting (Python)"), BT_COMMON_COLOR_WHEN_NEVER);
}

} /* namespace */

void registerFormatErrorApi(PyModuleBuilder& mb)
{
    mb.addFunc<formatError>("format_error");
    mb.addFunc<formatErrorCause>("format_error_cause");
}

} /* namespace bt2py */
