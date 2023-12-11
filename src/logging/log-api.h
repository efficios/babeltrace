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

#ifndef BABELTRACE_LOGGING_LOG_API_H
#define BABELTRACE_LOGGING_LOG_API_H

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <babeltrace2/babeltrace.h>

/* Access private `__BT_LOGGING_LEVEL_*` macros. */
#define __BT_IN_BABELTRACE_H
#include <babeltrace2/logging-defs.h>
#undef __BT_IN_BABELTRACE_H

#include "common/macros.h"
#include "common/assert.h"

/* Log levels */
enum bt_log_level {
	BT_LOG_TRACE	= __BT_LOGGING_LEVEL_TRACE,
	BT_LOG_DEBUG	= __BT_LOGGING_LEVEL_DEBUG,
	BT_LOG_INFO	= __BT_LOGGING_LEVEL_INFO,
	BT_LOG_WARNING	= __BT_LOGGING_LEVEL_WARNING,
	BT_LOG_ERROR	= __BT_LOGGING_LEVEL_ERROR,
	BT_LOG_FATAL	= __BT_LOGGING_LEVEL_FATAL,
	BT_LOG_NONE	= __BT_LOGGING_LEVEL_NONE,
};

/*
 * `BT_LOG_MINIMAL_LEVEL` (constant integer, mandatory): minimal log
 * level to completely disable (not build) logging with levels that are
 * more verbose.
 */
#ifndef BT_LOG_MINIMAL_LEVEL
# error "`BT_LOG_MINIMAL_LEVEL` must to defined"
#endif

/* Internal: portable printf()-like attribute */
#if defined(__printflike)
# define _BT_LOG_PRINTFLIKE(_str_index, _first_to_check)	\
	__printflike(_str_index, _first_to_check)
#elif defined(__MINGW_PRINTF_FORMAT)
# define _BT_LOG_PRINTFLIKE(_str_index, _first_to_check) 	\
	__attribute__((format(__MINGW_PRINTF_FORMAT, _str_index, _first_to_check)))
#elif defined(__GNUC__)
# define _BT_LOG_PRINTFLIKE(_str_index, _first_to_check)	\
	__attribute__((format(__printf__, _str_index, _first_to_check)))
#else
# define _BT_LOG_PRINTFLIKE(_str_index, _first_to_check)
#endif

/*
 * Runs `_expr` if `_cond` is true.
 */
#define BT_LOG_IF(_cond, _expr)	do { if (_cond) { _expr; } } while (0)

/*
 * Returns whether or not `_lvl` is enabled at build time, that is, it's
 * equally or less verbose than `BT_LOG_MINIMAL_LEVEL`.
 *
 * See `BT_LOG_MINIMAL_LEVEL` to learn more.
 */
#define BT_LOG_ENABLED(_lvl)		((_lvl) >= BT_LOG_MINIMAL_LEVEL)
#define BT_LOG_ENABLED_TRACE		BT_LOG_ENABLED(__BT_LOGGING_LEVEL_TRACE)
#define BT_LOG_ENABLED_DEBUG		BT_LOG_ENABLED(__BT_LOGGING_LEVEL_DEBUG)
#define BT_LOG_ENABLED_INFO		BT_LOG_ENABLED(__BT_LOGGING_LEVEL_INFO)
#define BT_LOG_ENABLED_WARNING		BT_LOG_ENABLED(__BT_LOGGING_LEVEL_WARNING)
#define BT_LOG_ENABLED_ERROR		BT_LOG_ENABLED(__BT_LOGGING_LEVEL_ERROR)
#define BT_LOG_ENABLED_FATAL		BT_LOG_ENABLED(__BT_LOGGING_LEVEL_FATAL)

/*
 * Returns whether or not `_lvl` is enabled at run time, that is, it's
 * equally or less verbose than some current (run-time) level
 * `_cur_lvl`.
 */
#define BT_LOG_ON_CUR_LVL(_lvl, _cur_lvl)	\
	G_UNLIKELY(BT_LOG_ENABLED((_lvl)) && (_lvl) >= (_cur_lvl))

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Writes the log message `msg` using the file name `file_name`, the
 * function name `func_name`, the line number `line_no`, the log level
 * `lvl`, and the tag `tag`.
 *
 * NOTE: This function writes unconditionally, without checking the
 * current (run-time) log level.
 */
void bt_log_write(const char *file_name, const char *func_name,
		unsigned int line_no, enum bt_log_level lvl, const char *tag,
		const char *msg);

/*
 * Calls bt_log_write(), formatting the log message through sprintf()
 * with `fmt` and the following arguments.
 */
void bt_log_write_printf(const char *file_name, const char *func_name,
		unsigned int line_no, enum bt_log_level lvl, const char *tag,
		const char *fmt, ...) _BT_LOG_PRINTFLIKE(6, 7);

/*
 * Writes the log message `msg` using the file name `file_name`, the
 * function name `func_name`, the line number `line_no`, the log level
 * `lvl`, and the tag `tag`, and also dumps `mem_len` bytes of
 * `mem_data`.
 *
 * NOTE: This function writes unconditionally, without checking the
 * current (run-time) log level.
 */
void bt_log_write_mem(const char *file_name, const char *func_name,
		unsigned int line_no, enum bt_log_level lvl, const char *tag,
		const void *mem_data, size_t mem_len,
		const char *msg);

/*
 * Calls bt_log_write_mem(), formatting the log message through
 * sprintf() with `fmt` and the following arguments.
 */
void bt_log_write_mem_printf(const char *file_name, const char *func_name,
		unsigned int line_no, enum bt_log_level lvl, const char *tag,
		const void *mem_data, size_t mem_len,
		const char *fmt, ...) _BT_LOG_PRINTFLIKE(8, 9);

/*
 * Writes:
 *
 * 1. `init_msg`
 * 2. The string `: `
 * 3. The message corresponding to `errno` (current error number)
 * 4. `msg`
 *
 * This function uses the file name `file_name`, the function name
 * `func_name`, the line number `line_no`, the log level `lvl`, and the
 * tag `tag`.
 *
 * NOTE: This function writes unconditionally, without checking the
 * current (run-time) log level.
 */
void bt_log_write_errno(const char *file_name, const char *func_name,
		unsigned int line_no, enum bt_log_level lvl, const char *tag,
		const char *init_msg, const char *msg);

/*
 * Calls bt_log_write_errno(), formatting the log message through
 * sprintf() with `fmt` and the following arguments.
 */
void bt_log_write_errno_printf(const char *file_name, const char *func_name,
		unsigned int line_no, enum bt_log_level lvl, const char *tag,
		const char *init_msg,
		const char *fmt, ...) _BT_LOG_PRINTFLIKE(7, 8);

#ifdef __cplusplus
}
#endif

/*
 * Calls bt_log_write() if logging is enabled at run time for the
 * current level `_cur_lvl`.
 *
 * Passes the current file name, function name, and line number to
 * bt_log_write().
 */
#define BT_LOG_WRITE_CUR_LVL(_lvl, _cur_lvl, _tag, _msg)		\
	do {								\
		if (BT_LOG_ON_CUR_LVL((_lvl), (_cur_lvl))) {		\
			bt_log_write(__FILE__, __func__, __LINE__,	\
				(_lvl), (_tag), (_msg));		\
		}							\
	} while (0)

/*
 * Calls bt_log_write_printf() if logging is enabled at run time for the
 * current level `_cur_lvl`.
 *
 * Passes the current file name, function name, and line number to
 * bt_log_write_printf().
 */
#define BT_LOG_WRITE_PRINTF_CUR_LVL(_lvl, _cur_lvl, _tag, _fmt, ...)	\
	do {								\
		if (BT_LOG_ON_CUR_LVL((_lvl), (_cur_lvl))) {		\
			bt_log_write_printf(__FILE__, __func__,		\
				__LINE__, (_lvl), (_tag), (_fmt),	\
				##__VA_ARGS__);				\
		}							\
	} while (0)

/*
 * Calls bt_log_write_mem() if logging is enabled at run time for the
 * current level `_cur_lvl`.
 *
 * Passes the current file name, function name, and line number to
 * bt_log_write_mem().
 */
#define BT_LOG_WRITE_MEM_CUR_LVL(_lvl, _cur_lvl, _tag, _mem_data, _mem_len, _msg) \
	do {								\
		if (BT_LOG_ON_CUR_LVL((_lvl), (_cur_lvl))) {		\
			bt_log_write_mem(__FILE__, __func__, __LINE__,	\
				(_lvl), (_tag), (_mem_data),		\
				(_mem_len), (_msg));			\
		}							\
	} while (0)

/*
 * Calls bt_log_write_mem_printf() if logging is enabled at run time for
 * the current level `_cur_lvl`.
 *
 * Passes the current file name, function name, and line number to
 * bt_log_write_mem_printf().
 */
#define BT_LOG_WRITE_MEM_PRINTF_CUR_LVL(_lvl, _cur_lvl, _tag, _mem_data, _mem_len, _fmt, ...) \
	do {								\
		if (BT_LOG_ON_CUR_LVL((_lvl), (_cur_lvl))) {		\
			bt_log_write_mem_printf(__FILE__, __func__,	\
				__LINE__, (_lvl), (_tag), (_mem_data),	\
				(_mem_len), (_fmt), ##__VA_ARGS__);	\
		}							\
	} while (0)

/*
 * Calls bt_log_write_errno() if logging is enabled at run time for the
 * current level `_cur_lvl`.
 *
 * Passes the current file name, function name, and line number to
 * bt_log_write_errno().
 */
#define BT_LOG_WRITE_ERRNO_CUR_LVL(_lvl, _cur_lvl, _tag, _init_msg, _msg) \
	do {								\
		if (BT_LOG_ON_CUR_LVL((_lvl), (_cur_lvl))) {		\
			bt_log_write_errno(__FILE__, __func__,		\
				__LINE__, (_lvl), (_tag), (_init_msg),	\
				(_msg));				\
		}							\
	} while (0)

/*
 * Calls bt_log_write_errno_printf() if logging is enabled at run time
 * for the current level `_cur_lvl`.
 *
 * Passes the current file name, function name, and line number to
 * bt_log_write_errno_printf().
 */
#define BT_LOG_WRITE_ERRNO_PRINTF_CUR_LVL(_lvl, _cur_lvl, _tag, _init_msg, _fmt, ...) \
	do {								\
		if (BT_LOG_ON_CUR_LVL((_lvl), (_cur_lvl))) {		\
			bt_log_write_errno_printf(__FILE__, __func__,	\
				__LINE__, (_lvl), (_tag), (_init_msg),	\
				(_fmt), ##__VA_ARGS__);			\
		}							\
	} while (0)

/*
 * Returns the equivalent letter of the log level `level`.
 *
 * `level` must be a valid log level.
 */
static inline
char bt_log_get_letter_from_level(const enum bt_log_level level)
{
	char letter;

	switch (level) {
	case BT_LOG_TRACE:
		letter = 'T';
		break;
	case BT_LOG_DEBUG:
		letter = 'D';
		break;
	case BT_LOG_INFO:
		letter = 'I';
		break;
	case BT_LOG_WARNING:
		letter = 'W';
		break;
	case BT_LOG_ERROR:
		letter = 'E';
		break;
	case BT_LOG_FATAL:
		letter = 'F';
		break;
	case BT_LOG_NONE:
		letter = 'N';
		break;
	default:
		abort();
	}

	return letter;
}

/*
 * Returns the log level as an integer for the string `str`, or -1 if
 * `str` is not a valid log level string.
 */
static inline
int bt_log_get_level_from_string(const char * const str)
{
	int level = -1;

	BT_ASSERT(str);

	if (strcmp(str, "TRACE") == 0 || strcmp(str, "T") == 0) {
		level = BT_LOG_TRACE;
	} else if (strcmp(str, "DEBUG") == 0 || strcmp(str, "D") == 0) {
		level = BT_LOG_DEBUG;
	} else if (strcmp(str, "INFO") == 0 || strcmp(str, "I") == 0) {
		level = BT_LOG_INFO;
	} else if (strcmp(str, "WARN") == 0 ||
			strcmp(str, "WARNING") == 0 ||
			strcmp(str, "W") == 0) {
		level = BT_LOG_WARNING;
	} else if (strcmp(str, "ERROR") == 0 || strcmp(str, "E") == 0) {
		level = BT_LOG_ERROR;
	} else if (strcmp(str, "FATAL") == 0 || strcmp(str, "F") == 0) {
		level = BT_LOG_FATAL;
	} else if (strcmp(str, "NONE") == 0 || strcmp(str, "N") == 0) {
		level = BT_LOG_NONE;
	} else {
		/* FIXME: Should we warn here? How? */
	}

	return level;
}

/*
 * Returns the log level as an integer for the letter `letter`, or -1 if
 * `letter` is not a valid log level string.
 */
static inline
int bt_log_get_level_from_letter(const char letter)
{
	const char str[] = {letter, '\0'};

	return bt_log_get_level_from_string(str);
}

/*
 * Returns the log level for the value of the environment variable named
 * `env_var_name`, or `BT_LOG_NONE` if not a valid log level string.
 */
static inline
enum bt_log_level bt_log_get_level_from_env(const char *env_var_name)
{
	const char * const varval = getenv(env_var_name);
	enum bt_log_level level = BT_LOG_NONE;
	int int_level;

	if (!varval) {
		goto end;
	}

	int_level = bt_log_get_level_from_string(varval);
	if (int_level < 0) {
		/* FIXME: Should we warn here? How? */
		int_level = BT_LOG_NONE;
	}

	level = (enum bt_log_level) int_level;

end:
	return level;
}

/*
 * Declares the variable named `_level_sym` as an external symbol
 * containing a log level.
 */
#define BT_LOG_LEVEL_EXTERN_SYMBOL(_level_sym)				\
	extern enum bt_log_level _level_sym

/*
 * 1. Defines the log level variable `_level_sym`, initializing it to
 *    `BT_LOG_NONE` (logging disabled).
 *
 * 2. Defines a library constructor named _bt_log_level_ctor() which
 *    initializes the log level variable `_level_sym` from the value of
 *    the environment variable named `_env_var_name`.
 */
#define BT_LOG_INIT_LOG_LEVEL(_level_sym, _env_var_name)		\
	enum bt_log_level _level_sym = BT_LOG_NONE;					\
	static								\
	void __attribute__((constructor)) _bt_log_level_ctor(void)	\
	{								\
		_level_sym = bt_log_get_level_from_env(_env_var_name);	\
	}

#endif /* BABELTRACE_LOGGING_LOG_API_H */
