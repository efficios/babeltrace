/*
 * The MIT License (MIT)
 *
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
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

%{
#include <babeltrace/logging.h>
%}

/* Log levels */
enum bt_logging_level {
	BT_LOGGING_LEVEL_VERBOSE = 1,
	BT_LOGGING_LEVEL_DEBUG = 2,
	BT_LOGGING_LEVEL_INFO = 3,
	BT_LOGGING_LEVEL_WARN = 4,
	BT_LOGGING_LEVEL_ERROR = 5,
	BT_LOGGING_LEVEL_FATAL = 6,
	BT_LOGGING_LEVEL_NONE = 0xff,
};

/* Logging functions */
enum bt_logging_level bt_logging_get_minimal_level(void);
enum bt_logging_level bt_logging_get_global_level(void);
void bt_logging_set_global_level(enum bt_logging_level log_level);
