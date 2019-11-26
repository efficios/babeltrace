#ifndef _BABELTRACE_STRING_FORMAT_FORMAT_ERROR_H
#define _BABELTRACE_STRING_FORMAT_FORMAT_ERROR_H

/*
 * Copyright EfficiOS, Inc.
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

#include <babeltrace2/babeltrace.h>
#include <common/common.h>
#include <common/macros.h>
#include <glib.h>

BT_HIDDEN
gchar *format_bt_error_cause(
		const bt_error_cause *error_cause,
		unsigned int columns,
		bt_logging_level log_level,
		enum bt_common_color_when use_colors);

BT_HIDDEN
gchar *format_bt_error(
		const bt_error *error,
		unsigned int columns,
		bt_logging_level log_level,
		enum bt_common_color_when use_colors);

#endif /* _BABELTRACE_STRING_FORMAT_FORMAT_ERROR_H */
