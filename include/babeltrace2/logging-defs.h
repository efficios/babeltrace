/*
 * No include guards here: it is safe to include this file multiple
 * times.
 */

/*
 * Copyright (c) 2019 EfficiOS Inc.
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

#ifndef __BT_IN_BABELTRACE_H
# error "Please include <babeltrace2/babeltrace.h> instead."
#endif

/*
 * This is NOT part of the API.
 *
 * These macros are used internally in preprocessor conditions to define
 * logging macros.
 */

#ifndef __BT_LOGGING_LEVEL_TRACE
# define __BT_LOGGING_LEVEL_TRACE	1
#endif

#ifndef __BT_LOGGING_LEVEL_DEBUG
# define __BT_LOGGING_LEVEL_DEBUG	2
#endif

#ifndef __BT_LOGGING_LEVEL_INFO
# define __BT_LOGGING_LEVEL_INFO	3
#endif

#ifndef __BT_LOGGING_LEVEL_WARNING
# define __BT_LOGGING_LEVEL_WARNING	4
#endif

#ifndef __BT_LOGGING_LEVEL_ERROR
# define __BT_LOGGING_LEVEL_ERROR	5
#endif

#ifndef __BT_LOGGING_LEVEL_FATAL
# define __BT_LOGGING_LEVEL_FATAL	6
#endif

#ifndef __BT_LOGGING_LEVEL_NONE
# define __BT_LOGGING_LEVEL_NONE	0xff
#endif
