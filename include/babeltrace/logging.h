#ifndef BABELTRACE_LOGGING_H
#define BABELTRACE_LOGGING_H

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

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
@defgroup logging Logging
@ingroup apiref
@brief Logging.

@code
#include <babeltrace/logging.h>
@endcode

The functions in this module control the Babeltrace library's logging
behaviour.

You can set the current global log level of the library with
bt_logging_set_global_level(). Note that, if the level you set is below
the minimal logging level (configured at build time, which you can get
with bt_logging_get_minimal_level()), the logging statement between the
current global log level and the minimal log level are not executed.

@file
@brief Logging functions.
@sa logging

@addtogroup logging
@{
*/

/**
@brief Log levels.
*/
enum bt_logging_level {
	/// Additional, low-level debugging context information.
	BT_LOGGING_LEVEL_VERBOSE	= 1,

	/**
	Debugging information, only useful when searching for the
	cause of a bug.
	*/
	BT_LOGGING_LEVEL_DEBUG		= 2,

	/**
	Non-debugging information and failure to load optional
	subsystems.
	*/
	BT_LOGGING_LEVEL_INFO		= 3,

	/**
	Errors caused by a bad usage of the library, that is, a
	non-observance of the documented function preconditions.

	The library's and object's states remain consistent when a
	warning is issued.
	*/
	BT_LOGGING_LEVEL_WARN		= 4,

	/**
	An important error from which the library cannot recover, but
	the executed stack of functions can still return cleanly.
	*/
	BT_LOGGING_LEVEL_ERROR		= 5,

	/**
	The library cannot continue to work in this condition: it must
	terminate immediately, without even returning to the user's
	execution.
	*/
	BT_LOGGING_LEVEL_FATAL		= 6,

	/// Logging is disabled.
	BT_LOGGING_LEVEL_NONE		= 0xff,
};

/**
@brief	Returns the minimal log level of the Babeltrace library.

The minimal log level is defined at the library's build time. Any
logging statement with a level below the minimal log level is not
compiled. This means that it is useless, although possible, to set the
global log level with bt_logging_set_global_level() below this level.

@returns	Minimal, build time log level.

@sa bt_logging_get_global_level(): Returns the current global log level.
*/
extern enum bt_logging_level bt_logging_get_minimal_level(void);

/**
@brief	Returns the current global log level of the Babeltrace library.

@returns	Current global log level.

@sa bt_logging_set_global_level(): Sets the current global log level.
@sa bt_logging_get_minimal_level(): Returns the minimal log level.
*/
extern enum bt_logging_level bt_logging_get_global_level(void);

/**
@brief	Sets the current global log level of the Babeltrace library
	to \p log_level.

If \p log_level is below what bt_logging_get_minimal_level() returns,
the logging statements with a level between \p log_level and the minimal
log level cannot be executed.

@param[in] log_level	Library's new global log level.

@sa bt_logging_get_global_level(): Returns the global log level.
*/
extern void bt_logging_set_global_level(enum bt_logging_level log_level);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_LOGGING_H */
