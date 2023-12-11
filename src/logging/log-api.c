/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2016 wonder-mice
 * Copyright (c) 2016-2023 Philippe Proulx <pproulx@efficios.com>
 *
 * This is very inspired by zf_log.c (see
 * <https://github.com/wonder-mice/zf_log/>), but modified (mostly
 * stripped down) for the use cases of Babeltrace.
 */

#include <ctype.h>
#include <errno.h>
#include <glib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#if defined(_WIN32) || defined(_WIN64)
# include <windows.h>
#endif

#ifdef __linux__
# include <sys/prctl.h>
# include <sys/syscall.h>
#endif

#ifdef __MACH__
# include <pthread.h>
#endif

#ifdef __GNU__
# include <mach.h>
#endif

#include "common/assert.h"
#include "common/common.h"
#include "common/macros.h"
#include "compat/time.h"

#include "log-api.h"

#ifdef __CYGWIN__
extern unsigned long pthread_getsequence_np(pthread_t *);
#endif

/*
 * Thread-local logging message buffer to put the next message and write
 * it with a single system call.
 */
static __thread char msg_buf[4 * 4096];

/*
 * Returns the number of available bytes in `msg_buf` considering the
 * writing position `at`.
 */
static inline
size_t avail_msg_buf_bytes(char *at)
{
	return msg_buf + sizeof(msg_buf) - at;
}

/*
 * Appends the character `ch` to `msg_buf`.
 */
static inline
void append_char_to_msg_buf(char ** const at, const char ch)
{
	**at = ch;
	(*at)++;
}

/*
 * Appends a space character to `msg_buf`.
 */
static inline
void append_sp_to_msg_buf(char ** const at)
{
	append_char_to_msg_buf(at, ' ');
}

/*
 * Appends the null-terminated string `str` to `msg_buf`.
 */
static inline
void append_str_to_msg_buf(char ** const at, const char * const str)
{
	const size_t len = strlen(str);

	memcpy(*at, str, len);
	*at += len;
}

/*
 * Formats the unsigned integer `val` with sprintf() using `fmt` and
 * appends the resulting string to `msg_buf`.
 */
static inline
void append_uint_to_msg_buf(char ** const at, const char * const fmt,
		const unsigned int val)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
	const int written_len = sprintf(*at, fmt, val);
#pragma GCC diagnostic pop

	BT_ASSERT_DBG(written_len > 0);
	*at += (size_t) written_len;
}

/*
 * Thread-local cache of seconds and milliseconds to formatted date/time
 * string (null terminated).
 *
 * This is often useful because many log messages may happen during the
 * same millisecond.
 *
 * Does not need any kind of protection since it's TLS.
 */
struct date_time_cache {
	uint64_t s;
	uint32_t ms;
	char str[128];
};

static __thread struct date_time_cache date_time_cache = {0};

static
const char *date_time_cache_get(const struct timeval tv)
{
	const uint64_t s = (uint64_t) tv.tv_sec;
	const uint32_t ms = (uint32_t) (tv.tv_usec / 1000);

	if (date_time_cache.s != s || date_time_cache.ms != ms) {
		/* Add to cache now */
		struct tm tm;
		const time_t time_s = (time_t) tv.tv_sec;

		date_time_cache.s = (uint64_t) tv.tv_sec;
		date_time_cache.ms = (uint32_t) (tv.tv_usec / 1000);
		(void) bt_localtime_r(&time_s, &tm);
		(void) sprintf(date_time_cache.str, "%02u-%02u %02u:%02u:%02u.%03u",
			tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec, date_time_cache.ms);
	}

	return date_time_cache.str;
}

/*
 * Appends a formatted date/time for `tv` to `msg_buf`.
 */
static inline
void append_date_time_to_msg_buf(char ** const at, const struct timeval tv)
{
	const char *str = date_time_cache_get(tv);

	append_str_to_msg_buf(at, str);
}

/*
 * Appends the PID and TID to `msg_buf`.
 */
static inline
void append_pid_tid_to_msg_buf(char ** const at)
{
	const unsigned int pid = (unsigned int) getpid();
	unsigned int tid;

	/* Look at this beautiful portability */
#if defined(_WIN32) || defined(_WIN64)
	tid = (unsigned int) GetCurrentThreadId();
#elif defined(__CYGWIN__)
	{
		pthread_t thr = pthread_self();

		tid = (unsigned int) pthread_getsequence_np(&thr);
	}
#elif defined(__sun__)
	tid = (unsigned int) pthread_self();
#elif defined(__linux__)
	tid = (unsigned int) syscall(SYS_gettid);
#elif defined(__APPLE__) && defined(__MACH__)
	tid = (unsigned int) pthread_mach_thread_np(pthread_self());
#elif defined(__GNU__)
	{
		mach_port_t mach_port = mach_thread_self();

		mach_port_deallocate(mach_task_self(), mach_port);
		tid = (unsigned int) mach_port;
	}
#else
# error "Platform not supported"
#endif

	/* Append them now */
	append_uint_to_msg_buf(at, "%u", pid);
	append_sp_to_msg_buf(at);
	append_uint_to_msg_buf(at, "%u", tid);
}

/*
 * Writes the initial part of the log message to `msg_buf`, without the
 * message and without resetting the terminal color).
 */
static
void common_write_init(char ** const at, const char * const file_name,
		const char * const func_name, const unsigned int line_no,
		const enum bt_log_level lvl, const char * const tag)
{
	const char *color_p = "";
	struct timeval tv;

	/* Get time immediately */
	gettimeofday(&tv, 0);

	/* Write the terminal color code to use, if any */
	switch (lvl) {
	case BT_LOG_INFO:
		color_p = bt_common_color_fg_blue();
		break;
	case BT_LOG_WARNING:
		color_p = bt_common_color_fg_yellow();
		break;
	case BT_LOG_ERROR:
	case BT_LOG_FATAL:
		color_p = bt_common_color_fg_red();
		break;
	default:
		break;
	}

	append_str_to_msg_buf(at, color_p);

	/* Write date/time */
	append_date_time_to_msg_buf(at, tv);
	append_sp_to_msg_buf(at);

	/* Write PID/TID */
	append_pid_tid_to_msg_buf(at);
	append_sp_to_msg_buf(at);

	/* Write log level letter */
	append_char_to_msg_buf(at, bt_log_get_letter_from_level(lvl));
	append_sp_to_msg_buf(at);

	/* Write tag */
	if (tag) {
		append_str_to_msg_buf(at, tag);
		append_sp_to_msg_buf(at);
	}

	/* Write source location */
	append_str_to_msg_buf(at, func_name);
	append_char_to_msg_buf(at, '@');
	append_str_to_msg_buf(at, file_name);
	append_uint_to_msg_buf(at, ":%u", line_no);
	append_sp_to_msg_buf(at);
}

/*
 * Writes the final part of the log message to `msg_buf` (resets the
 * terminal color and appends a newline), and then writes the whole log
 * message to the standard error.
 */
static
void common_write_fini(char ** const at)
{
	append_str_to_msg_buf(at, bt_common_color_reset());
	append_char_to_msg_buf(at, '\n');
	(void) write(STDERR_FILENO, msg_buf, *at - msg_buf);
}

void bt_log_write(const char * const file_name, const char * const func_name,
		const unsigned int line_no, const enum bt_log_level lvl,
		const char * const tag, const char * const msg)
{
	char *at = msg_buf;

	common_write_init(&at, file_name, func_name, line_no, lvl, tag);
	append_str_to_msg_buf(&at, msg);
	common_write_fini(&at);
}

_BT_LOG_PRINTFLIKE(6, 0)
static
void write_vprintf(const char * const file_name, const char * const func_name,
		const unsigned int line_no, const enum bt_log_level lvl,
		const char * const tag, const char * const fmt, va_list args)
{
	char *at = msg_buf;
	int written_len;

	common_write_init(&at, file_name, func_name, line_no, lvl, tag);
	written_len = vsnprintf(at, avail_msg_buf_bytes(at) - 16,
		fmt, args);
	if (written_len > 0) {
		at += (size_t) written_len;
	}

	common_write_fini(&at);
}

void bt_log_write_printf(const char * const file_name,
		const char * const func_name, const unsigned int line_no,
		const enum bt_log_level lvl, const char * const tag,
		const char * const fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	write_vprintf(file_name, func_name, line_no, lvl, tag, fmt, va);
	va_end(va);
}

/*
 * Writes the initial part of the log message to `msg_buf`, including
 * the initial message and `errno` string, without the message and
 * without resetting the terminal color).
 */
static
void common_write_errno_init(char ** const at, const char * const file_name,
		const char * const func_name, const unsigned int line_no,
		const enum bt_log_level lvl, const char * const tag,
		const char * const init_msg)
{
	BT_ASSERT_DBG(errno != 0);
	const char * const errno_msg = g_strerror(errno);

	common_write_init(at, file_name, func_name, line_no, lvl, tag);
	append_str_to_msg_buf(at, init_msg);
	append_char_to_msg_buf(at, ':');
	append_sp_to_msg_buf(at);
	append_str_to_msg_buf(at, errno_msg);
}

void bt_log_write_errno(const char * const file_name,
		const char * const func_name, const unsigned int line_no,
		const enum bt_log_level lvl, const char * const tag,
		const char * const init_msg, const char * const msg)
{
	char *at = msg_buf;

	common_write_errno_init(&at, file_name, func_name, line_no, lvl,
		tag, init_msg);
	append_str_to_msg_buf(&at, msg);
	common_write_fini(&at);
}

void bt_log_write_errno_printf(const char * const file_name,
		const char * const func_name, const unsigned int line_no,
		const enum bt_log_level lvl, const char * const tag,
		const char * const init_msg, const char * const fmt, ...)
{
	char *at = msg_buf;
	int written_len;
	va_list va;

	common_write_errno_init(&at, file_name, func_name, line_no, lvl,
		tag, init_msg);
	va_start(va, fmt);
	written_len = vsnprintf(at, avail_msg_buf_bytes(at) - 16, fmt, va);
	va_end(va);
	if (written_len > 0) {
		at += (size_t) written_len;
	}

	common_write_fini(&at);
}

/*
 * Logs `mem_len` bytes of `mem_data` on a single line.
 */
static
void write_mem_line(const char * const file_name, const char * const func_name,
		const unsigned int line_no, const enum bt_log_level lvl,
		const char * const tag, const uint8_t * const mem_data,
		const size_t mem_len, const size_t max_mem_line_len)
{
	static const char * const hex_chars = "0123456789abcdef";
	char *at = msg_buf;
	size_t i;

	common_write_init(&at, file_name, func_name, line_no, lvl, tag);

	/* Write hexadecimal representation */
	for (i = 0; i < mem_len; i++) {
		const uint8_t byte = mem_data[i];

		/* Write nibble */
		append_char_to_msg_buf(&at, hex_chars[byte >> 4]);
		append_char_to_msg_buf(&at, hex_chars[byte & 0xf]);

		/* Add a space */
		append_sp_to_msg_buf(&at);
	}

	/* Insert spaces to align the following ASCII representation */
	for (i = 0; i < max_mem_line_len - mem_len; i++) {
		append_sp_to_msg_buf(&at);
		append_sp_to_msg_buf(&at);
		append_sp_to_msg_buf(&at);
	}

	/* Insert a vertical line between the representations */
	append_str_to_msg_buf(&at, "| ");

	/* Write the ASCII representation */
	for (i = 0; i < mem_len; i++) {
		const uint8_t byte = mem_data[i];

		if (isprint(byte)) {
			append_char_to_msg_buf(&at, (char) byte);
		} else {
			/* Non-printable character */
			append_char_to_msg_buf(&at, '.');
		}
	}

	common_write_fini(&at);
}

/*
 * Logs `mem_len` bytes of `mem_data` on one or more lines.
 */
static
void write_mem_lines(const char * const file_name, const char * const func_name,
		const unsigned int line_no, const enum bt_log_level lvl,
		const char * const tag, const uint8_t * const mem_data,
		const size_t mem_len)
{
	const uint8_t *mem_at = mem_data;
	size_t rem_mem_len = mem_len;

	if (!mem_data || mem_len == 0) {
		/* Nothing to write */
		goto end;
	}

	while (rem_mem_len > 0) {
		static const size_t max_mem_line_len = 16;

		/* Number of bytes to write on this line */
		const size_t mem_line_len = rem_mem_len > max_mem_line_len ?
			max_mem_line_len : rem_mem_len;

		/* Log those bytes */
		write_mem_line(file_name, func_name, line_no, lvl, tag,
			mem_at, mem_line_len, max_mem_line_len);

		/* Adjust for next iteration */
		rem_mem_len -= mem_line_len;
		mem_at += mem_line_len;
	}

end:
	return;
}

void bt_log_write_mem(const char * const file_name, const char * const func_name,
		const unsigned int line_no, const enum bt_log_level lvl,
		const char * const tag, const void * const mem_data,
		const size_t mem_len, const char * const msg)
{
	bt_log_write(file_name, func_name, line_no, lvl, tag, msg);
	write_mem_lines(file_name, func_name, line_no, lvl, tag,
		mem_data, mem_len);
}

void bt_log_write_mem_printf(const char * const file_name,
		const char * const func_name, const unsigned int line_no,
		const enum bt_log_level lvl, const char * const tag,
		const void * const mem_data, const size_t mem_len,
		const char * const fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	write_vprintf(file_name, func_name, line_no, lvl, tag, fmt, va);
	va_end(va);

	write_mem_lines(file_name, func_name, line_no, lvl, tag,
		mem_data, mem_len);
}
