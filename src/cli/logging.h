#ifndef CLI_LOGGING_H
#define CLI_LOGGING_H

/*
 * Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
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

#define BT_LOG_OUTPUT_LEVEL bt_cli_log_level
#include "logging/log.h"

BT_LOG_LEVEL_EXTERN_SYMBOL(bt_cli_log_level);

#define BT_CLI_LOG_AND_APPEND(_lvl, _fmt, ...)				\
	do {								\
		BT_LOG_WRITE(_lvl, BT_LOG_TAG, _fmt, ##__VA_ARGS__);	\
		(void) BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_UNKNOWN( \
			"Babeltrace CLI", _fmt, ##__VA_ARGS__);		\
	} while (0)

#define BT_CLI_LOGE_APPEND_CAUSE(_fmt, ...)				\
	BT_CLI_LOG_AND_APPEND(BT_LOG_ERROR, _fmt, ##__VA_ARGS__)
#define BT_CLI_LOGW_APPEND_CAUSE(_fmt, ...)				\
	BT_CLI_LOG_AND_APPEND(BT_LOG_WARNING, _fmt, ##__VA_ARGS__)

#endif /* CLI_LOGGING_H */
