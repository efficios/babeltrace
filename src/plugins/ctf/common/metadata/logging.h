#ifndef CTF_METADATA_LOGGING_H
#define CTF_METADATA_LOGGING_H

/*
 * Copyright (c) 2017 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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
#include "logging/log.h"

/*
 * This global log level is for the generated lexer and parser: we can't
 * use a contextual log level for their "tracing", so they rely on this.
 */
BT_LOG_LEVEL_EXTERN_SYMBOL(ctf_plugin_metadata_log_level);

/*
 * To be used by functions without a context structure to pass all the
 * logging configuration at once.
 */
struct meta_log_config {
	bt_logging_level log_level;

	/* Weak */
	bt_self_component *self_comp;
};

#define _BT_LOGT_LINENO(_lineno, _msg, args...) \
	BT_LOGT("At line %u in metadata stream: " _msg, _lineno, ## args)

#define _BT_LOGW_LINENO(_lineno, _msg, args...) \
	BT_LOGW("At line %u in metadata stream: " _msg, _lineno, ## args)

#define _BT_LOGE_LINENO(_lineno, _msg, args...) \
	BT_LOGE("At line %u in metadata stream: " _msg, _lineno, ## args)

#define _BT_COMP_LOGT_LINENO(_lineno, _msg, args...) \
	BT_COMP_LOGT("At line %u in metadata stream: " _msg, _lineno, ## args)

#define _BT_COMP_LOGW_LINENO(_lineno, _msg, args...) \
	BT_COMP_LOGW("At line %u in metadata stream: " _msg, _lineno, ## args)

#define _BT_COMP_LOGE_LINENO(_lineno, _msg, args...) \
	BT_COMP_LOGE("At line %u in metadata stream: " _msg, _lineno, ## args)

#endif /* CTF_METADATA_LOGGING_H */
