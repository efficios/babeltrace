/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2022 Simon Marchi <simon.marchi@efficios.com>
 * Copyright (c) 2022 Philippe Proulx <eeppeliteloop@gmail.com>
 */

#ifndef BABELTRACE_CPP_COMMON_CFG_ERROR_REPORTING_HPP
#define BABELTRACE_CPP_COMMON_CFG_ERROR_REPORTING_HPP

#include <babeltrace2/babeltrace.h>

/*
 * Appends a cause to the error of the current thread using the logging
 * configuration `_log_cfg`.
 */
#define BT_APPEND_CAUSE_EX(_log_cfg, _fmt, ...)                                                    \
    do {                                                                                           \
        if ((_log_cfg).selfMsgIter()) {                                                            \
            BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_MESSAGE_ITERATOR((_log_cfg).selfMsgIter(),   \
                                                                       _fmt, ##__VA_ARGS__);       \
        } else if ((_log_cfg).selfComp()) {                                                        \
            BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_COMPONENT((_log_cfg).selfComp(), _fmt,       \
                                                                ##__VA_ARGS__);                    \
        } else if ((_log_cfg).selfCompCls()) {                                                     \
            BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_COMPONENT_CLASS((_log_cfg).selfCompCls(),    \
                                                                      _fmt, ##__VA_ARGS__);        \
        } else {                                                                                   \
            BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_UNKNOWN((_log_cfg).moduleName(), _fmt,       \
                                                              ##__VA_ARGS__);                      \
        }                                                                                          \
    } while (false)

#define BT_APPEND_CAUSE(_fmt, ...) BT_APPEND_CAUSE_EX((BT_CLOG_CFG), _fmt, ##__VA_ARGS__)

/*
 * Appends a cause to the error of the current thread using the logging
 * configuration `_log_cfg`.
 */
#define BT_APPEND_CAUSE_STR_EX(_log_cfg, _str) BT_APPEND_CAUSE_EX((_log_cfg), "%s", _str)

#define BT_APPEND_CAUSE_STR(_str) BT_APPEND_CAUSE_STR_EX((BT_CLOG_CFG), _str)

/*
 * Appends a cause with an errno message to the error of the current
 * thread using the logging configuration `_log_cfg`.
 */
#define BT_APPEND_CAUSE_ERRNO_EX(_log_cfg, _init_msg, _fmt, ...)                                   \
    do {                                                                                           \
        const auto errStr = g_strerror(errno);                                                     \
        BT_APPEND_CAUSE_EX((_log_cfg), "%s: %s" _fmt, _init_msg, errStr, ##__VA_ARGS__);           \
    } while (false)

#define BT_APPEND_CAUSE_ERRNO(_init_msg, _fmt, ...)                                                \
    BT_APPEND_CAUSE_EX((BT_CLOG_CFG), _init_msg, _fmt, ##__VA_ARGS__)

#endif /* BABELTRACE_CPP_COMMON_CFG_ERROR_REPORTING_HPP */
