/*
 * Copyright (c) 2021 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_LOGGING_HPP
#define BABELTRACE_CPP_COMMON_BT2_LOGGING_HPP

#include <babeltrace2/babeltrace.h>

#include "common/macros.h"
#include "cpp-common/vendor/wise-enum/wise_enum.h"

namespace bt2 {

/* Avoid `-Wshadow` error on GCC, conflicting with `bt2::Error` */
BT_DIAG_PUSH
BT_DIAG_IGNORE_SHADOW

/* clang-format off */

WISE_ENUM_CLASS(LoggingLevel,
    (Trace,     BT_LOGGING_LEVEL_TRACE),
    (Debug,     BT_LOGGING_LEVEL_DEBUG),
    (Info,      BT_LOGGING_LEVEL_INFO),
    (Warning,   BT_LOGGING_LEVEL_WARNING),
    (Error,     BT_LOGGING_LEVEL_ERROR),
    (Fatal,     BT_LOGGING_LEVEL_FATAL),
    (None,      BT_LOGGING_LEVEL_NONE))

/* clang-format on */

BT_DIAG_POP

} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_LOGGING_HPP */
