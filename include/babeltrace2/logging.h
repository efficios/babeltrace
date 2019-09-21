#ifndef BABELTRACE2_LOGGING_H
#define BABELTRACE2_LOGGING_H

/*
 * Copyright (c) 2010-2019 EfficiOS Inc. and Linux Foundation
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

#include <babeltrace2/logging-defs.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
@defgroup api-logging Logging

@brief
    Logging level enumerators and library logging control.

The logging API offers logging level enumerators (#bt_logging_level)
as well as functions to control libbabeltrace2's internal logging.

@note
    This API does \em not offer macros and functions to write logging
    statements: as of \bt_name_version_min_maj, the actual mechanism to
    log is implementation-defined for each user \bt_plugin.

libbabeltrace2 contains many hundreds of logging statements to help you
follow and debug your plugin or program.

The library's initial logging level is controlled by the
\c LIBBABELTRACE2_INIT_LOG_LEVEL environment variable. If this
environment variable is not set at library load time, the library's
initial logging level is #BT_LOGGING_LEVEL_NONE. See
\ref api-fund-logging to learn more.

Set libbabeltrace2's current logging level with
bt_logging_set_global_level().

\anchor api-logging-extra-lib bt_logging_set_global_level() only
controls <strong>libbabeltrace2</strong>'s logging level; it does \em
not control the logging level of:

- Individual \bt_p_comp: bt_graph_add_source_component(),
  bt_graph_add_source_component_with_initialize_method_data(),
  bt_graph_add_filter_component(),
  bt_graph_add_filter_component_with_initialize_method_data(),
  bt_graph_add_sink_component(), and
  bt_graph_add_sink_component_with_initialize_method_data() control
  this.

- A \ref api-qexec "query operation":
  bt_query_executor_set_logging_level() controls this.

- The bt_get_greatest_operative_mip_version() operation: its
  \bt_p{logging_level} parameter controls this.

As of \bt_name_version_min_maj, there's no module-specific logging level
control: bt_logging_set_global_level() sets the logging level of all the
library's modules.

libbabeltrace2 writes its logging statements to the standard error
stream. A logging line looks like this:

@code{.unparsed}
05-11 00:58:03.691 23402 23402 D VALUES bt_value_destroy@values.c:498 Destroying value: addr=0xb9c3eb0
@endcode

See \ref api-fund-logging to learn more about the logging statement
line's format.

You can set a \em minimal logging level at the \bt_name project's build
time (see \ref api-fund-logging to learn how). The logging statements
with a level that's less severe than the minimal logging level are \em
not built. For example, if the minimal logging level is
#BT_LOGGING_LEVEL_INFO, the #BT_LOGGING_LEVEL_TRACE and
#BT_LOGGING_LEVEL_DEBUG logging statements are not built. Use
bt_logging_get_minimal_level() to get the library's minimal logging
level.
*/

/*! @{ */

/*!
@brief
    Logging level enumerators.
*/
typedef enum bt_logging_level {
	/*!
	@brief
	    \em TRACE level.

	Low-level debugging context information.

	The \em TRACE logging statements can significantly
	impact performance.
	*/
	BT_LOGGING_LEVEL_TRACE		= __BT_LOGGING_LEVEL_TRACE,

	/*!
	@brief
	    \em DEBUG level.

	Debugging information, with a higher level of details than the
	\em TRACE level.

	The \em DEBUG logging statements do not significantly
	impact performance.
	*/
	BT_LOGGING_LEVEL_DEBUG		= __BT_LOGGING_LEVEL_DEBUG,

	/*!
	@brief
	    \em INFO level.

	Informational messages that highlight progress or important
	states of the application, plugins, or library.

	The \em INFO logging statements do not significantly
	impact performance.
	*/
	BT_LOGGING_LEVEL_INFO		= __BT_LOGGING_LEVEL_INFO,

	/*!
	@brief
	    \em WARNING level.

	Unexpected situations which still allow the execution to
	continue.

	The \em WARNING logging statements do not significantly
	impact performance.
	*/
	BT_LOGGING_LEVEL_WARNING	= __BT_LOGGING_LEVEL_WARNING,

	/*!
	@brief
	    \em ERROR level.

	Errors that might still allow the execution to continue.

	Usually, once one or more errors are reported at this level, the
	application, plugin, or library won't perform any more useful
	task, but it should still exit cleanly.

	The \em ERROR logging statements do not significantly
	impact performance.
	*/
	BT_LOGGING_LEVEL_ERROR		= __BT_LOGGING_LEVEL_ERROR,

	/*!
	@brief
	    \em FATAL level.

	Severe errors that lead the execution to abort immediately.

	The \em FATAL logging statements do not significantly
	impact performance.
	*/
	BT_LOGGING_LEVEL_FATAL		= __BT_LOGGING_LEVEL_FATAL,

	/*!
	@brief
	    Logging is disabled.
	*/
	BT_LOGGING_LEVEL_NONE		= __BT_LOGGING_LEVEL_NONE,
} bt_logging_level;

/*!
@brief
    Sets the logging level of all the libbabeltrace2 modules to
    \bt_p{logging_level}.

The library's global logging level
\ref api-logging-extra-lib "does not affect" the logging level of
individual components and query operations.

@param[in] logging_level
    New library's global logging level.

@sa bt_logging_get_global_level() &mdash;
    Returns the current library's global logging level.
*/
extern void bt_logging_set_global_level(bt_logging_level logging_level);

/*!
@brief
    Returns the current logging level of all the libbabeltrace2 modules.

@returns
    Library's current global logging level.

@sa bt_logging_set_global_level() &mdash;
    Sets the current library's global logging level.
*/
extern bt_logging_level bt_logging_get_global_level(void);

/*!
@brief
    Returns the library's minimal (build-time) logging level.

The library logging statements with a level that's less severe than the
minimal logging level are \em not built.

For example, if the minimal logging level is #BT_LOGGING_LEVEL_INFO, the
#BT_LOGGING_LEVEL_TRACE and #BT_LOGGING_LEVEL_DEBUG logging statements
are not built.

@returns
    Library's minimal logging level.

@sa bt_logging_get_global_level() &mdash;
    Returns the current library's global logging level.
*/
extern bt_logging_level bt_logging_get_minimal_level(void);

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_LOGGING_H */
