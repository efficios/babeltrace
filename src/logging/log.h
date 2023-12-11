/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2016 wonder-mice
 * Copyright (c) 2016-2023 Philippe Proulx <pproulx@efficios.com>
 *
 * This is very inspired by zf_log.h (see
 * <https://github.com/wonder-mice/zf_log/>), but modified (mostly
 * stripped down) for the use cases of Babeltrace.
 */

#ifndef BABELTRACE_LOGGING_LOG_H
#define BABELTRACE_LOGGING_LOG_H

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <babeltrace2/babeltrace.h>

#include "log-api.h"

/*
 * `BT_LOG_OUTPUT_LEVEL` (expression having the type `int`, optional):
 * valid expression evaluating to the current (run-time) log level.
 *
 * This is configured per translation unit.
 *
 * Note that BT_LOG*() statements will evaluate this expression each
 * time if their level is equally or less verbose than
 * `BT_LOG_MINIMAL_LEVEL`. Keep this expression as simple as possible,
 * otherwise it will not only add run-time overhead, but also will
 * increase the size of call sites (which will result in larger
 * executable).
 *
 * Example:
 *
 *     #define BT_LOG_OUTPUT_LEVEL log_level
 *     #include "logging/log.h"
 *
 * If missing, you may only use logging macros having a `_cur_lvl`
 * parameter.
 */
#ifdef BT_LOG_OUTPUT_LEVEL
# define _BT_LOG_OUTPUT_LEVEL	BT_LOG_OUTPUT_LEVEL
#else
# define _BT_LOG_OUTPUT_LEVEL	"Please define `BT_LOG_OUTPUT_LEVEL`"
#endif

/*
 * `BT_LOG_TAG` (expression having the type `const char *`, optional):
 * component or module identifier.
 *
 * This is configured per translation unit.
 *
 * Example:
 *
 *     // ...
 *     #define BT_LOG_TAG "MY.MODULE"
 *     #include "logging/log.h"
 */
#ifdef BT_LOG_TAG
# define _BT_LOG_TAG    BT_LOG_TAG
#else
# define _BT_LOG_TAG    NULL
#endif

/*
 * BT_LOG_ON_CUR_LVL() using `BT_LOG_OUTPUT_LEVEL` as the current
 * (run-time) level.
 */
#define BT_LOG_ON(_lvl)		BT_LOG_ON_CUR_LVL(_lvl, _BT_LOG_OUTPUT_LEVEL)
#define BT_LOG_ON_TRACE		BT_LOG_ON(BT_LOG_TRACE)
#define BT_LOG_ON_DEBUG		BT_LOG_ON(BT_LOG_DEBUG)
#define BT_LOG_ON_INFO		BT_LOG_ON(BT_LOG_INFO)
#define BT_LOG_ON_WARNING	BT_LOG_ON(BT_LOG_WARNING)
#define BT_LOG_ON_ERROR		BT_LOG_ON(BT_LOG_ERROR)
#define BT_LOG_ON_FATAL		BT_LOG_ON(BT_LOG_FATAL)

/*
 * BT_LOG_WRITE_CUR_LVL() using `BT_LOG_OUTPUT_LEVEL` as the current
 * (run-time) level.
 */
#define BT_LOG_WRITE(_lvl, _tag, _msg)					\
	BT_LOG_WRITE_CUR_LVL((_lvl), _BT_LOG_OUTPUT_LEVEL, (_tag), (_msg))

/*
 * BT_LOG_WRITE_PRINTF_CUR_LVL() using `BT_LOG_OUTPUT_LEVEL` as the
 * current (run-time) level.
 */
#define BT_LOG_WRITE_PRINTF(_lvl, _tag, _fmt, ...)			\
	BT_LOG_WRITE_PRINTF_CUR_LVL((_lvl), _BT_LOG_OUTPUT_LEVEL,	\
		(_tag), (_fmt), ##__VA_ARGS__)

/*
 * BT_LOG_WRITE_MEM_CUR_LVL() using `BT_LOG_OUTPUT_LEVEL` as the current
 * (run-time) level.
 */
#define BT_LOG_WRITE_MEM(_lvl, _tag, _mem_data, _mem_len, _msg)		\
	BT_LOG_WRITE_MEM_CUR_LVL((_lvl), _BT_LOG_OUTPUT_LEVEL, (_tag),	\
		(_mem_data), (_mem_len), (_msg))

/*
 * BT_LOG_WRITE_MEM_PRINTF_CUR_LVL() using `BT_LOG_OUTPUT_LEVEL` as the
 * current (run-time) level.
 */
#define BT_LOG_WRITE_MEM_PRINTF(_lvl, _tag, _mem_data, _mem_len, _fmt, ...) \
	BT_LOG_WRITE_MEM_PRINTF_CUR_LVL((_lvl), _BT_LOG_OUTPUT_LEVEL,	\
		(_tag), (_mem_data), (_mem_len), (_fmt), ##__VA_ARGS__)

/*
 * BT_LOG_WRITE_ERRNO_CUR_LVL() using `BT_LOG_OUTPUT_LEVEL` as the
 * current (run-time) level.
 */
#define BT_LOG_WRITE_ERRNO(_lvl, _tag, _init_msg, _msg)			\
	BT_LOG_WRITE_ERRNO_CUR_LVL((_lvl), _BT_LOG_OUTPUT_LEVEL,	\
		(_tag), (_init_msg), (_msg))

/*
 * BT_LOG_WRITE_ERRNO_PRINTF_CUR_LVL() using `BT_LOG_OUTPUT_LEVEL` as
 * the current (run-time) level.
 */
#define BT_LOG_WRITE_ERRNO_PRINTF(_lvl, _tag, _init_msg, _fmt, ...)	\
	BT_LOG_WRITE_ERRNO_PRINTF_CUR_LVL((_lvl), _BT_LOG_OUTPUT_LEVEL,	\
		(_tag), (_init_msg), (_fmt), ##__VA_ARGS__)

/* Internal: no-op function when a log level is disabled */
static inline
void _bt_log_unused(int dummy __attribute__((unused)), ...)
{
}

#define _BT_LOG_UNUSED(...) \
	do { if (0) _bt_log_unused(0, ##__VA_ARGS__); } while (0)

/* Trace level logging macros using `BT_LOG_TAG` */
#if BT_LOG_ENABLED_TRACE
# define BT_LOGT_STR_CUR_LVL(_cur_lvl, _msg) \
	BT_LOG_WRITE_CUR_LVL(BT_LOG_TRACE, (_cur_lvl), _BT_LOG_TAG, (_msg))
# define BT_LOGT_STR(_msg) \
	BT_LOG_WRITE(BT_LOG_TRACE, _BT_LOG_TAG, (_msg))
# define BT_LOGT_CUR_LVL(_cur_lvl, _fmt, ...) \
	BT_LOG_WRITE_PRINTF_CUR_LVL(BT_LOG_TRACE, (_cur_lvl), _BT_LOG_TAG, (_fmt), ##__VA_ARGS__)
# define BT_LOGT(_fmt, ...) \
	BT_LOG_WRITE_PRINTF(BT_LOG_TRACE, _BT_LOG_TAG, (_fmt), ##__VA_ARGS__)
# define BT_LOGT_MEM_STR_CUR_LVL(_cur_lvl, _mem_data, _mem_len, _msg) \
	BT_LOG_WRITE_MEM_CUR_LVL(BT_LOG_TRACE, (_cur_lvl), _BT_LOG_TAG, (_mem_data), (_mem_len), (_msg))
# define BT_LOGT_MEM_STR(_mem_data, _mem_len, _msg) \
	BT_LOG_WRITE_MEM(BT_LOG_TRACE, _BT_LOG_TAG, (_mem_data), (_mem_len), (_msg))
# define BT_LOGT_MEM_CUR_LVL(_cur_lvl, _mem_data, _mem_len, _fmt, ...) \
	BT_LOG_WRITE_MEM_PRINTF_CUR_LVL(BT_LOG_TRACE, (_cur_lvl), _BT_LOG_TAG, (_mem_data), (_mem_len), (_fmt), ##__VA_ARGS__)
# define BT_LOGT_MEM(_mem_data, _mem_len, _fmt, ...) \
	BT_LOG_WRITE_MEM_PRINTF(BT_LOG_TRACE, _BT_LOG_TAG, (_mem_data), (_mem_len), (_fmt), ##__VA_ARGS__)
# define BT_LOGT_ERRNO_STR_CUR_LVL(_cur_lvl, _init_msg, _msg) \
	BT_LOG_WRITE_ERRNO_CUR_LVL(BT_LOG_TRACE, (_cur_lvl), _BT_LOG_TAG, (_init_msg), (_msg))
# define BT_LOGT_ERRNO_STR(_init_msg, _msg) \
	BT_LOG_WRITE_ERRNO(BT_LOG_TRACE, _BT_LOG_TAG, (_init_msg), (_msg))
# define BT_LOGT_ERRNO_CUR_LVL(_cur_lvl, _init_msg, _fmt, ...) \
	BT_LOG_WRITE_ERRNO_PRINTF_CUR_LVL(BT_LOG_TRACE, (_cur_lvl), _BT_LOG_TAG, (_init_msg), (_fmt), ##__VA_ARGS__)
# define BT_LOGT_ERRNO(_init_msg, _fmt, ...) \
	BT_LOG_WRITE_ERRNO_PRINTF(BT_LOG_TRACE, _BT_LOG_TAG, (_init_msg), (_fmt), ##__VA_ARGS__)
#else
# define BT_LOGT_STR_CUR_LVL(...)	_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGT_STR(...)		_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGT_CUR_LVL(...)		_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGT(...)			_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGT_MEM_STR_CUR_LVL(...)	_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGT_MEM_STR(...)		_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGT_MEM_CUR_LVL(...)	_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGT_MEM(...)		_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGT_ERRNO_STR_CUR_LVL(...)	_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGT_ERRNO_STR(...)		_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGT_ERRNO_CUR_LVL(...)	_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGT_ERRNO(...)		_BT_LOG_UNUSED(__VA_ARGS__)
#endif /* BT_LOG_ENABLED_TRACE */

/* Debug level logging macros using `BT_LOG_TAG` */
#if BT_LOG_ENABLED_DEBUG
# define BT_LOGD_STR_CUR_LVL(_cur_lvl, _msg) \
	BT_LOG_WRITE_CUR_LVL(BT_LOG_DEBUG, (_cur_lvl), _BT_LOG_TAG, (_msg))
# define BT_LOGD_STR(_msg) \
	BT_LOG_WRITE(BT_LOG_DEBUG, _BT_LOG_TAG, (_msg))
# define BT_LOGD_CUR_LVL(_cur_lvl, _fmt, ...) \
	BT_LOG_WRITE_PRINTF_CUR_LVL(BT_LOG_DEBUG, (_cur_lvl), _BT_LOG_TAG, (_fmt), ##__VA_ARGS__)
# define BT_LOGD(_fmt, ...) \
	BT_LOG_WRITE_PRINTF(BT_LOG_DEBUG, _BT_LOG_TAG, (_fmt), ##__VA_ARGS__)
# define BT_LOGD_MEM_STR_CUR_LVL(_cur_lvl, _mem_data, _mem_len, _msg) \
	BT_LOG_WRITE_MEM_CUR_LVL(BT_LOG_DEBUG, (_cur_lvl), _BT_LOG_TAG, (_mem_data), (_mem_len), (_msg))
# define BT_LOGD_MEM_STR(_mem_data, _mem_len, _msg) \
	BT_LOG_WRITE_MEM(BT_LOG_DEBUG, _BT_LOG_TAG, (_mem_data), (_mem_len), (_msg))
# define BT_LOGD_MEM_CUR_LVL(_cur_lvl, _mem_data, _mem_len, _fmt, ...) \
	BT_LOG_WRITE_MEM_PRINTF_CUR_LVL(BT_LOG_DEBUG, (_cur_lvl), _BT_LOG_TAG, (_mem_data), (_mem_len), (_fmt), ##__VA_ARGS__)
# define BT_LOGD_MEM(_mem_data, _mem_len, _fmt, ...) \
	BT_LOG_WRITE_MEM_PRINTF(BT_LOG_DEBUG, _BT_LOG_TAG, (_mem_data), (_mem_len), (_fmt), ##__VA_ARGS__)
# define BT_LOGD_ERRNO_STR_CUR_LVL(_cur_lvl, _init_msg, _msg) \
	BT_LOG_WRITE_ERRNO_CUR_LVL(BT_LOG_DEBUG, (_cur_lvl), _BT_LOG_TAG, (_init_msg), (_msg))
# define BT_LOGD_ERRNO_STR(_init_msg, _msg) \
	BT_LOG_WRITE_ERRNO(BT_LOG_DEBUG, _BT_LOG_TAG, (_init_msg), (_msg))
# define BT_LOGD_ERRNO_CUR_LVL(_cur_lvl, _init_msg, _fmt, ...) \
	BT_LOG_WRITE_ERRNO_PRINTF_CUR_LVL(BT_LOG_DEBUG, (_cur_lvl), _BT_LOG_TAG, (_init_msg), (_fmt), ##__VA_ARGS__)
# define BT_LOGD_ERRNO(_init_msg, _fmt, ...) \
	BT_LOG_WRITE_ERRNO_PRINTF(BT_LOG_DEBUG, _BT_LOG_TAG, (_init_msg), (_fmt), ##__VA_ARGS__)
#else
# define BT_LOGD_STR_CUR_LVL(...)	_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGD_STR(...)		_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGD_CUR_LVL(...)		_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGD(...)			_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGD_MEM_STR_CUR_LVL(...)	_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGD_MEM_STR(...)		_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGD_MEM_CUR_LVL(...)	_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGD_MEM(...)		_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGD_ERRNO_STR_CUR_LVL(...)	_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGD_ERRNO_STR(...)		_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGD_ERRNO_CUR_LVL(...)	_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGD_ERRNO(...)		_BT_LOG_UNUSED(__VA_ARGS__)
#endif /* BT_LOG_ENABLED_DEBUG */

/* Info level logging macros using `BT_LOG_TAG` */
#if BT_LOG_ENABLED_INFO
# define BT_LOGI_STR_CUR_LVL(_cur_lvl, _msg) \
	BT_LOG_WRITE_CUR_LVL(BT_LOG_INFO, (_cur_lvl), _BT_LOG_TAG, (_msg))
# define BT_LOGI_STR(_msg) \
	BT_LOG_WRITE(BT_LOG_INFO, _BT_LOG_TAG, (_msg))
# define BT_LOGI_CUR_LVL(_cur_lvl, _fmt, ...) \
	BT_LOG_WRITE_PRINTF_CUR_LVL(BT_LOG_INFO, (_cur_lvl), _BT_LOG_TAG, (_fmt), ##__VA_ARGS__)
# define BT_LOGI(_fmt, ...) \
	BT_LOG_WRITE_PRINTF(BT_LOG_INFO, _BT_LOG_TAG, (_fmt), ##__VA_ARGS__)
# define BT_LOGI_MEM_STR_CUR_LVL(_cur_lvl, _mem_data, _mem_len, _msg) \
	BT_LOG_WRITE_MEM_CUR_LVL(BT_LOG_INFO, (_cur_lvl), _BT_LOG_TAG, (_mem_data), (_mem_len), (_msg))
# define BT_LOGI_MEM_STR(_mem_data, _mem_len, _msg) \
	BT_LOG_WRITE_MEM(BT_LOG_INFO, _BT_LOG_TAG, (_mem_data), (_mem_len), (_msg))
# define BT_LOGI_MEM_CUR_LVL(_cur_lvl, _mem_data, _mem_len, _fmt, ...) \
	BT_LOG_WRITE_MEM_PRINTF_CUR_LVL(BT_LOG_INFO, (_cur_lvl), _BT_LOG_TAG, (_mem_data), (_mem_len), (_fmt), ##__VA_ARGS__)
# define BT_LOGI_MEM(_mem_data, _mem_len, _fmt, ...) \
	BT_LOG_WRITE_MEM_PRINTF(BT_LOG_INFO, _BT_LOG_TAG, (_mem_data), (_mem_len), (_fmt), ##__VA_ARGS__)
# define BT_LOGI_ERRNO_STR_CUR_LVL(_cur_lvl, _init_msg, _msg) \
	BT_LOG_WRITE_ERRNO_CUR_LVL(BT_LOG_INFO, (_cur_lvl), _BT_LOG_TAG, (_init_msg), (_msg))
# define BT_LOGI_ERRNO_STR(_init_msg, _msg) \
	BT_LOG_WRITE_ERRNO(BT_LOG_INFO, _BT_LOG_TAG, (_init_msg), (_msg))
# define BT_LOGI_ERRNO_CUR_LVL(_cur_lvl, _init_msg, _fmt, ...) \
	BT_LOG_WRITE_ERRNO_PRINTF_CUR_LVL(BT_LOG_INFO, (_cur_lvl), _BT_LOG_TAG, (_init_msg), (_fmt), ##__VA_ARGS__)
# define BT_LOGI_ERRNO(_init_msg, _fmt, ...) \
	BT_LOG_WRITE_ERRNO_PRINTF(BT_LOG_INFO, _BT_LOG_TAG, (_init_msg), (_fmt), ##__VA_ARGS__)
#else
# define BT_LOGI_STR_CUR_LVL(...)	_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGI_STR(...)		_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGI_CUR_LVL(...)		_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGI(...)			_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGI_MEM_STR_CUR_LVL(...)	_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGI_MEM_STR(...)		_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGI_MEM_CUR_LVL(...)	_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGI_MEM(...)		_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGI_ERRNO_STR_CUR_LVL(...)	_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGI_ERRNO_STR(...)		_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGI_ERRNO_CUR_LVL(...)	_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGI_ERRNO(...)		_BT_LOG_UNUSED(__VA_ARGS__)
#endif /* BT_LOG_ENABLED_INFO */

/* Warning level logging macros using `BT_LOG_TAG` */
#if BT_LOG_ENABLED_WARNING
# define BT_LOGW_STR_CUR_LVL(_cur_lvl, _msg) \
	BT_LOG_WRITE_CUR_LVL(BT_LOG_WARNING, (_cur_lvl), _BT_LOG_TAG, (_msg))
# define BT_LOGW_STR(_msg) \
	BT_LOG_WRITE(BT_LOG_WARNING, _BT_LOG_TAG, (_msg))
# define BT_LOGW_CUR_LVL(_cur_lvl, _fmt, ...) \
	BT_LOG_WRITE_PRINTF_CUR_LVL(BT_LOG_WARNING, (_cur_lvl), _BT_LOG_TAG, (_fmt), ##__VA_ARGS__)
# define BT_LOGW(_fmt, ...) \
	BT_LOG_WRITE_PRINTF(BT_LOG_WARNING, _BT_LOG_TAG, (_fmt), ##__VA_ARGS__)
# define BT_LOGW_MEM_STR_CUR_LVL(_cur_lvl, _mem_data, _mem_len, _msg) \
	BT_LOG_WRITE_MEM_CUR_LVL(BT_LOG_WARNING, (_cur_lvl), _BT_LOG_TAG, (_mem_data), (_mem_len), (_msg))
# define BT_LOGW_MEM_STR(_mem_data, _mem_len, _msg) \
	BT_LOG_WRITE_MEM(BT_LOG_WARNING, _BT_LOG_TAG, (_mem_data), (_mem_len), (_msg))
# define BT_LOGW_MEM_CUR_LVL(_cur_lvl, _mem_data, _mem_len, _fmt, ...) \
	BT_LOG_WRITE_MEM_PRINTF_CUR_LVL(BT_LOG_WARNING, (_cur_lvl), _BT_LOG_TAG, (_mem_data), (_mem_len), (_fmt), ##__VA_ARGS__)
# define BT_LOGW_MEM(_mem_data, _mem_len, _fmt, ...) \
	BT_LOG_WRITE_MEM_PRINTF(BT_LOG_WARNING, _BT_LOG_TAG, (_mem_data), (_mem_len), (_fmt), ##__VA_ARGS__)
# define BT_LOGW_ERRNO_STR_CUR_LVL(_cur_lvl, _init_msg, _msg) \
	BT_LOG_WRITE_ERRNO_CUR_LVL(BT_LOG_WARNING, (_cur_lvl), _BT_LOG_TAG, (_init_msg), (_msg))
# define BT_LOGW_ERRNO_STR(_init_msg, _msg) \
	BT_LOG_WRITE_ERRNO(BT_LOG_WARNING, _BT_LOG_TAG, (_init_msg), (_msg))
# define BT_LOGW_ERRNO_CUR_LVL(_cur_lvl, _init_msg, _fmt, ...) \
	BT_LOG_WRITE_ERRNO_PRINTF_CUR_LVL(BT_LOG_WARNING, (_cur_lvl), _BT_LOG_TAG, (_init_msg), (_fmt), ##__VA_ARGS__)
# define BT_LOGW_ERRNO(_init_msg, _fmt, ...) \
	BT_LOG_WRITE_ERRNO_PRINTF(BT_LOG_WARNING, _BT_LOG_TAG, (_init_msg), (_fmt), ##__VA_ARGS__)
#else
# define BT_LOGW_STR_CUR_LVL(...)	_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGW_STR(...)		_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGW_CUR_LVL(...)		_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGW(...)			_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGW_MEM_STR_CUR_LVL(...)	_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGW_MEM_STR(...)		_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGW_MEM_CUR_LVL(...)	_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGW_MEM(...)		_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGW_ERRNO_STR_CUR_LVL(...)	_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGW_ERRNO_STR(...)		_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGW_ERRNO_CUR_LVL(...)	_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGW_ERRNO(...)		_BT_LOG_UNUSED(__VA_ARGS__)
#endif /* BT_LOG_ENABLED_WARNING */

/* Error level logging macros using `BT_LOG_TAG` */
#if BT_LOG_ENABLED_ERROR
# define BT_LOGE_STR_CUR_LVL(_cur_lvl, _msg) \
	BT_LOG_WRITE_CUR_LVL(BT_LOG_ERROR, (_cur_lvl), _BT_LOG_TAG, (_msg))
# define BT_LOGE_STR(_msg) \
	BT_LOG_WRITE(BT_LOG_ERROR, _BT_LOG_TAG, (_msg))
# define BT_LOGE_CUR_LVL(_cur_lvl, _fmt, ...) \
	BT_LOG_WRITE_PRINTF_CUR_LVL(BT_LOG_ERROR, (_cur_lvl), _BT_LOG_TAG, (_fmt), ##__VA_ARGS__)
# define BT_LOGE(_fmt, ...) \
	BT_LOG_WRITE_PRINTF(BT_LOG_ERROR, _BT_LOG_TAG, (_fmt), ##__VA_ARGS__)
# define BT_LOGE_MEM_STR_CUR_LVL(_cur_lvl, _mem_data, _mem_len, _msg) \
	BT_LOG_WRITE_MEM_CUR_LVL(BT_LOG_ERROR, (_cur_lvl), _BT_LOG_TAG, (_mem_data), (_mem_len), (_msg))
# define BT_LOGE_MEM_STR(_mem_data, _mem_len, _msg) \
	BT_LOG_WRITE_MEM(BT_LOG_ERROR, _BT_LOG_TAG, (_mem_data), (_mem_len), (_msg))
# define BT_LOGE_MEM_CUR_LVL(_cur_lvl, _mem_data, _mem_len, _fmt, ...) \
	BT_LOG_WRITE_MEM_PRINTF_CUR_LVL(BT_LOG_ERROR, (_cur_lvl), _BT_LOG_TAG, (_mem_data), (_mem_len), (_fmt), ##__VA_ARGS__)
# define BT_LOGE_MEM(_mem_data, _mem_len, _fmt, ...) \
	BT_LOG_WRITE_MEM_PRINTF(BT_LOG_ERROR, _BT_LOG_TAG, (_mem_data), (_mem_len), (_fmt), ##__VA_ARGS__)
# define BT_LOGE_ERRNO_STR_CUR_LVL(_cur_lvl, _init_msg, _msg) \
	BT_LOG_WRITE_ERRNO_CUR_LVL(BT_LOG_ERROR, (_cur_lvl), _BT_LOG_TAG, (_init_msg), (_msg))
# define BT_LOGE_ERRNO_STR(_init_msg, _msg) \
	BT_LOG_WRITE_ERRNO(BT_LOG_ERROR, _BT_LOG_TAG, (_init_msg), (_msg))
# define BT_LOGE_ERRNO_CUR_LVL(_cur_lvl, _init_msg, _fmt, ...) \
	BT_LOG_WRITE_ERRNO_PRINTF_CUR_LVL(BT_LOG_ERROR, (_cur_lvl), _BT_LOG_TAG, (_init_msg), (_fmt), ##__VA_ARGS__)
# define BT_LOGE_ERRNO(_init_msg, _fmt, ...) \
	BT_LOG_WRITE_ERRNO_PRINTF(BT_LOG_ERROR, _BT_LOG_TAG, (_init_msg), (_fmt), ##__VA_ARGS__)
#else
# define BT_LOGE_STR_CUR_LVL(...)	_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGE_STR(...)		_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGE_CUR_LVL(...)		_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGE(...)			_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGE_MEM_STR_CUR_LVL(...)	_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGE_MEM_STR(...)		_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGE_MEM_CUR_LVL(...)	_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGE_MEM(...)		_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGE_ERRNO_STR_CUR_LVL(...)	_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGE_ERRNO_STR(...)		_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGE_ERRNO_CUR_LVL(...)	_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGE_ERRNO(...)		_BT_LOG_UNUSED(__VA_ARGS__)
#endif /* BT_LOG_ENABLED_ERROR */

/* Fatal level logging macros using `BT_LOG_TAG` */
#if BT_LOG_ENABLED_FATAL
# define BT_LOGF_STR_CUR_LVL(_cur_lvl, _msg) \
	BT_LOG_WRITE_CUR_LVL(BT_LOG_FATAL, (_cur_lvl), _BT_LOG_TAG, (_msg))
# define BT_LOGF_STR(_msg) \
	BT_LOG_WRITE(BT_LOG_FATAL, _BT_LOG_TAG, (_msg))
# define BT_LOGF_CUR_LVL(_cur_lvl, _fmt, ...) \
	BT_LOG_WRITE_PRINTF_CUR_LVL(BT_LOG_FATAL, (_cur_lvl), _BT_LOG_TAG, (_fmt), ##__VA_ARGS__)
# define BT_LOGF(_fmt, ...) \
	BT_LOG_WRITE_PRINTF(BT_LOG_FATAL, _BT_LOG_TAG, (_fmt), ##__VA_ARGS__)
# define BT_LOGF_MEM_STR_CUR_LVL(_cur_lvl, _mem_data, _mem_len, _msg) \
	BT_LOG_WRITE_MEM_CUR_LVL(BT_LOG_FATAL, (_cur_lvl), _BT_LOG_TAG, (_mem_data), (_mem_len), (_msg))
# define BT_LOGF_MEM_STR(_mem_data, _mem_len, _msg) \
	BT_LOG_WRITE_MEM(BT_LOG_FATAL, _BT_LOG_TAG, (_mem_data), (_mem_len), (_msg))
# define BT_LOGF_MEM_CUR_LVL(_cur_lvl, _mem_data, _mem_len, _fmt, ...) \
	BT_LOG_WRITE_MEM_PRINTF_CUR_LVL(BT_LOG_FATAL, (_cur_lvl), _BT_LOG_TAG, (_mem_data), (_mem_len), (_fmt), ##__VA_ARGS__)
# define BT_LOGF_MEM(_mem_data, _mem_len, _fmt, ...) \
	BT_LOG_WRITE_MEM_PRINTF(BT_LOG_FATAL, _BT_LOG_TAG, (_mem_data), (_mem_len), (_fmt), ##__VA_ARGS__)
# define BT_LOGF_ERRNO_STR_CUR_LVL(_cur_lvl, _init_msg, _msg) \
	BT_LOG_WRITE_ERRNO_CUR_LVL(BT_LOG_FATAL, (_cur_lvl), _BT_LOG_TAG, (_init_msg), (_msg))
# define BT_LOGF_ERRNO_STR(_init_msg, _msg) \
	BT_LOG_WRITE_ERRNO(BT_LOG_FATAL, _BT_LOG_TAG, (_init_msg), (_msg))
# define BT_LOGF_ERRNO_CUR_LVL(_cur_lvl, _init_msg, _fmt, ...) \
	BT_LOG_WRITE_ERRNO_PRINTF_CUR_LVL(BT_LOG_FATAL, (_cur_lvl), _BT_LOG_TAG, (_init_msg), (_fmt), ##__VA_ARGS__)
# define BT_LOGF_ERRNO(_init_msg, _fmt, ...) \
	BT_LOG_WRITE_ERRNO_PRINTF(BT_LOG_FATAL, _BT_LOG_TAG, (_init_msg), (_fmt), ##__VA_ARGS__)
#else
# define BT_LOGF_STR_CUR_LVL(...)	_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGF_STR(...)		_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGF_CUR_LVL(...)		_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGF(...)			_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGF_MEM_STR_CUR_LVL(...)	_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGF_MEM_STR(...)		_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGF_MEM_CUR_LVL(...)	_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGF_MEM(...)		_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGF_ERRNO_STR_CUR_LVL(...)	_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGF_ERRNO_STR(...)		_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGF_ERRNO_CUR_LVL(...)	_BT_LOG_UNUSED(__VA_ARGS__)
# define BT_LOGF_ERRNO(...)		_BT_LOG_UNUSED(__VA_ARGS__)
#endif /* BT_LOG_ENABLED_FATAL */

/*
 * Tell the world that `log.h` was included without having to rely on
 * the specific include guard.
 */
#define BT_LOG_SUPPORTED

#endif /* BABELTRACE_LOGGING_LOG_H */
