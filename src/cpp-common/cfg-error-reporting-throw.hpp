/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2022 Francis Deslauriers <francis.deslauriers@efficios.com>
 * Copyright (c) 2022 Simon Marchi <simon.marchi@efficios.com>
 * Copyright (c) 2022 Philippe Proulx <eeppeliteloop@gmail.com>
 */

#ifndef BABELTRACE_CPP_COMMON_CFG_ERROR_REPORTING_THROW_HPP
#define BABELTRACE_CPP_COMMON_CFG_ERROR_REPORTING_THROW_HPP

#include <babeltrace2/babeltrace.h>

#include "cfg-error-reporting.hpp"

/*
 * Appends a cause to the error of the current thread using the
 * configuration `_log_cfg` and throws an instance of `_exc_cls`.
 */
#define BT_APPEND_CAUSE_AND_THROW_EX(_log_cfg, _exc_cls, _fmt, ...)                                \
    do {                                                                                           \
        BT_APPEND_CAUSE_EX((_log_cfg), _fmt, ##__VA_ARGS__);                                       \
        throw _exc_cls {};                                                                         \
    } while (false)

#define BT_APPEND_CAUSE_AND_THROW(_exc_cls, _fmt, ...)                                             \
    BT_APPEND_CAUSE_AND_THROW_EX((BT_CLOG_CFG), _exc_cls, _fmt, ##__VA_ARGS__)

/*
 * Appends a cause to the error of the current thread using the
 * configuration `_log_cfg` and rethrows.
 */
#define BT_APPEND_CAUSE_AND_RETHROW_EX(_log_cfg, _fmt, ...)                                        \
    do {                                                                                           \
        BT_APPEND_CAUSE_EX((_log_cfg), _fmt, ##__VA_ARGS__);                                       \
        throw;                                                                                     \
    } while (false)

#define BT_APPEND_CAUSE_AND_RETHROW(_fmt, ...)                                                     \
    BT_APPEND_CAUSE_AND_RETHROW_EX((BT_CLOG_CFG), _fmt, ##__VA_ARGS__)

/*
 * Appends a cause to the error of the current thread using the
 * configuration `_log_cfg` and throws an instance of `_exc_cls`.
 */
#define BT_APPEND_CAUSE_STR_AND_THROW_EX(_log_cfg, _exc_cls, _str)                                 \
    do {                                                                                           \
        BT_APPEND_CAUSE_STR_EX((_log_cfg), _str);                                                  \
        throw _exc_cls {};                                                                         \
    } while (false)

#define BT_APPEND_CAUSE_STR_AND_THROW(_exc_cls, _str)                                              \
    BT_APPEND_CAUSE_STR_AND_THROW_EX((BT_CLOG_CFG), _exc_cls, _str)

/*
 * Appends a cause to the error of the current thread using the
 * configuration `_log_cfg` and rethrows.
 */
#define BT_APPEND_CAUSE_STR_AND_RETHROW_EX(_log_cfg, _str)                                         \
    do {                                                                                           \
        BT_APPEND_CAUSE_STR_EX((_log_cfg), _str);                                                  \
        throw;                                                                                     \
    } while (false)

#define BT_APPEND_CAUSE_STR_AND_RETHROW(_str)                                                      \
    BT_APPEND_CAUSE_STR_AND_RETHROW_EX((BT_CLOG_CFG), _str)

/*
 * Appends a cause with an errno message to the error of the current
 * thread, using the initial message `_init_msg` and the configuration
 * `_log_cfg`, and throws an instance of `_exc_cls`.
 */
#define BT_APPEND_CAUSE_ERRNO_AND_THROW_EX(_log_cfg, _exc_cls, _init_msg, _fmt, ...)               \
    do {                                                                                           \
        BT_APPEND_CAUSE_ERRNO_EX((_log_cfg), _init_msg, _fmt, ##__VA_ARGS__);                      \
        throw _exc_cls {};                                                                         \
    } while (false)

#define BT_APPEND_CAUSE_ERRNO_AND_THROW(_exc_cls, _init_msg, _fmt, ...)                            \
    BT_APPEND_CAUSE_ERRNO_AND_THROW_EX((BT_CLOG_CFG), _exc_cls, _init_msg, _fmt, ##__VA_ARGS__)

/*
 * Appends a cause with an errno message to the error of the current
 * thread, using the initial message `_init_msg` and the configuration
 * `_log_cfg`, and rethrows.
 */
#define BT_APPEND_CAUSE_ERRNO_AND_RETHROW_EX(_log_cfg, _init_msg, _fmt, ...)                       \
    do {                                                                                           \
        BT_APPEND_CAUSE_ERRNO_EX((_log_cfg), _init_msg, _fmt, ##__VA_ARGS__);                      \
        throw;                                                                                     \
    } while (false)

#define BT_APPEND_CAUSE_ERRNO_AND_RETHROW(_init_msg, _fmt, ...)                                    \
    BT_APPEND_CAUSE_ERRNO_AND_RETHROW_EX((BT_CLOG_CFG), _init_msg, _fmt, ##__VA_ARGS__)

#endif /* BABELTRACE_CPP_COMMON_CFG_ERROR_REPORTING_THROW_HPP */
