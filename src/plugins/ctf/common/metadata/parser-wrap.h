#ifndef BABELTRACE_PLUGINS_CTF_COMMON_METADATA_PARSER_WRAP_H
#define BABELTRACE_PLUGINS_CTF_COMMON_METADATA_PARSER_WRAP_H

/*
 * Copyright 2019 EfficiOS Inc.
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

/*
 * Small wrapper around the bison-generated parser.h to conditionally define
 * YYDEBUG (and therefore the yydebug declaration).
 */

#include "logging/log.h"

#if BT_LOG_ENABLED_TRACE
# define YYDEBUG 1
# define YYFPRINTF(_stream, _fmt, args...) BT_LOGT(_fmt, ## args)
#else
# define YYDEBUG 0
#endif

#define ALLOW_INCLUDE_PARSER_H
#include "parser.h"
#undef ALLOW_INCLUDE_PARSER_H

#endif
