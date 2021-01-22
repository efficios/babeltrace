/*
 * Copyright (c) 2021 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_LOGGING_HPP
#define BABELTRACE_CPP_COMMON_BT2_LOGGING_HPP

#include <babeltrace2/babeltrace.h>

namespace bt2 {

enum class LoggingLevel
{
    TRACE = BT_LOGGING_LEVEL_TRACE,
    DEBUG = BT_LOGGING_LEVEL_DEBUG,
    INFO = BT_LOGGING_LEVEL_INFO,
    WARNING = BT_LOGGING_LEVEL_WARNING,
    ERROR = BT_LOGGING_LEVEL_ERROR,
    FATAL = BT_LOGGING_LEVEL_FATAL,
    NONE = BT_LOGGING_LEVEL_NONE,
};

} // namespace bt2

#endif // BABELTRACE_CPP_COMMON_BT2_LOGGING_HPP
