#ifndef BABELTRACE_LIB_LOGGING_INTERNAL_H
#define BABELTRACE_LIB_LOGGING_INTERNAL_H

/*
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <babeltrace/babeltrace-internal.h>
#include <stdarg.h>

#define BT_LOG_OUTPUT_LEVEL bt_lib_log_level

#include <babeltrace/logging-internal.h>

extern
int bt_lib_log_level;

#define BT_LIB_LOG(_lvl, _fmt, ...)					\
	do {								\
		if (BT_LOG_ON(_lvl)) {					\
			bt_lib_log(_BT_LOG_SRCLOC_FUNCTION, __FILE__,	\
				__LINE__, _lvl, _BT_LOG_TAG,		\
				(_fmt), ##__VA_ARGS__);			\
		}							\
	} while (0)

/*
 * The six macros below are logging statements which are specialized
 * for the Babeltrace library.
 *
 * `_fmt` is a typical printf()-style format string, with the following
 * limitations:
 *
 * * The `*` width specifier is not accepted.
 * * The `*` precision specifier is not accepted.
 * * The `j` and `t` length modifiers are not accepted.
 * * The `n` format specifier is not accepted.
 * * The format specifiers defined in <inttypes.h> are not accepted
 *   except for `PRId64`, `PRIu64`, `PRIx64`, `PRIX64`, `PRIo64`, and
 *   `PRIi64`.
 *
 * The Babeltrace extension conversion specifier is accepted. Its syntax
 * is:
 *
 * 1. Introductory `%!` sequence.
 *
 * 2. Optional: `[` followed by a custom prefix for the printed fields
 *    of this specifier, followed by `]`. The standard form is to end
 *    this prefix with `-` so that, for example, with the prefix
 *    `prefix-`, the complete field name is `prefix-addr`.
 *
 * 3. Optional: `+` to print extended fields. This depends on the
 *    provided format specifier.
 *
 * 4. Format specifier (see below).
 *
 * The available format specifiers are:
 *
 *   `r`:
 *       Reference count information. The parameter is any Babeltrace
 *       object.
 *
 *   `F`:
 *       CTF IR field type. The parameter type is `struct bt_field_type *`.
 *
 *   `f`:
 *       CTF IR field. The parameter type is `struct bt_field *`.
 *
 *   `P`:
 *       Field path. The parameter type is `struct bt_field_path *`.
 *
 *   `E`:
 *       CTF IR event class. The parameter type is `struct bt_event_class *`.
 *
 *   `e`:
 *       CTF IR event. The parameter type is `struct bt_event *`.
 *
 *   `S`:
 *       CTF IR stream class. The parameter type is `struct bt_stream_class *`.
 *
 *   `s`:
 *       CTF IR stream. The parameter type is `struct bt_stream *`.
 *
 *   `a`:
 *       Packet. The parameter type is `struct bt_packet *`.
 *
 *   `t`:
 *       CTF IR trace. The parameter type is `struct bt_trace *`.
 *
 *   `K`:
 *       Clock class. The parameter type is `struct bt_clock_class *`.
 *
 *   `k`:
 *       Clock value. The parameter type is `struct bt_clock_value *`.
 *
 *   `v`:
 *       Value. The parameter type is `struct bt_value *`.
 *
 *   `n`:
 *       Notification. The parameter type is `struct bt_notification *`.
 *
 *   `i`:
 *       Notification iterator. The parameter type is
 *       `struct bt_notification_iterator *`.
 *
 *   `C`:
 *       Component class. The parameter type is `struct bt_component_class *`.
 *
 *   `c`:
 *       Component. The parameter type is `struct bt_component *`.
 *
 *   `p`:
 *       Port. The parameter type is `struct bt_port *`.
 *
 *   `x`:
 *       Connection. The parameter type is `struct bt_connection *`.
 *
 *   `g`:
 *       Graph. The parameter type is `struct bt_graph *`.
 *
 *   `u`:
 *       Plugin. The parameter type is `struct bt_plugin *`.
 *
 *   `o`:
 *       Object pool. The parameter type is `struct bt_object_pool *`.
 *
 * Conversion specifier examples:
 *
 *     %!f
 *     %![my-event-]+e
 *     %!t
 *     %!+F
 *
 * The string `, ` is printed between individual fields, but not after
 * the last one. Therefore you must put this separator in the format
 * string between two Babeltrace objects, e.g.:
 *
 *     BT_LIB_LOGW("Message: count=%u, %!E, %!+C", count, event_class,
 *                 clock_class);
 *
 * Example with a custom prefix:
 *
 *     BT_LIB_LOGI("Some message: %![ec-a-]e, %![ec-b-]+e", ec_a, ec_b);
 *
 * It is safe to pass NULL as any Babeltrace object parameter: the
 * macros only print its null address.
 */
#define BT_LIB_LOGF(_fmt, ...)	BT_LIB_LOG(BT_LOG_FATAL, _fmt, ##__VA_ARGS__)
#define BT_LIB_LOGE(_fmt, ...)	BT_LIB_LOG(BT_LOG_ERROR, _fmt, ##__VA_ARGS__)
#define BT_LIB_LOGW(_fmt, ...)	BT_LIB_LOG(BT_LOG_WARN, _fmt, ##__VA_ARGS__)
#define BT_LIB_LOGI(_fmt, ...)	BT_LIB_LOG(BT_LOG_INFO, _fmt, ##__VA_ARGS__)
#define BT_LIB_LOGD(_fmt, ...)	BT_LIB_LOG(BT_LOG_DEBUG, _fmt, ##__VA_ARGS__)
#define BT_LIB_LOGV(_fmt, ...)	BT_LIB_LOG(BT_LOG_VERBOSE, _fmt, ##__VA_ARGS__)

/*
 * Log statement, specialized for the Babeltrace library.
 *
 * Use one of the BT_LIB_LOGF*() macros above instead of calling this
 * function directly.
 */
BT_HIDDEN
void bt_lib_log(const char *func, const char *file, unsigned line,
		int lvl, const char *tag, const char *fmt, ...);

#endif /* BABELTRACE_LIB_LOGGING_INTERNAL_H */
