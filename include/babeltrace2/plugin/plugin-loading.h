#ifndef BABELTRACE2_PLUGIN_PLUGIN_LOADING_H
#define BABELTRACE2_PLUGIN_PLUGIN_LOADING_H

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

#include <stdint.h>
#include <stddef.h>

#include <babeltrace2/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
@defgroup api-plugin Plugin loading

@brief
    Plugin loading functions.

A <strong><em>plugin</em></strong> is a package of \bt_p_comp_cls:

@image html plugin.png "A plugin is a package of component classes."

@attention
    The plugin loading API offers functions to <em>find and load</em>
    existing plugins and use the packaged \bt_p_comp_cls. To \em write a
    plugin, see \ref api-plugin-dev.

There are three types of plugins:

<dl>
  <dt>Shared object plugin</dt>
  <dd>
    <code>.so</code> file on Unix systems;
    <code>.dll</code> file on Windows systems.
  </dd>

  <dt>Python&nbsp;3 plugin</dt>
  <dd>
      <code>.py</code> file which starts with the
      <code>bt_plugin_</code> prefix.
  </dd>

  <dt>Static plugin</dt>
  <dd>
      A plugin built directly into libbabeltrace2 or into the
      user application.
  </dd>
</dl>

libbabeltrace2 loads shared object and Python plugins. Those plugins
need libbabeltrace2 in turn to create and use \bt_name objects:

@image html linking.png "libbabeltrace2 loads plugins which need libbabeltrace2."

A plugin is a \ref api-fund-shared-object "shared object": get a new
reference with bt_plugin_get_ref() and put an existing reference with
bt_plugin_put_ref().

Get the number of \bt_comp_cls in a plugin with
bt_plugin_get_source_component_class_count(),
bt_plugin_get_filter_component_class_count(), and
bt_plugin_get_sink_component_class_count().

Borrow a \bt_comp_cls by index from a plugin with
bt_plugin_borrow_source_component_class_by_index_const(),
bt_plugin_borrow_filter_component_class_by_index_const(), and
bt_plugin_borrow_sink_component_class_by_index_const().

Borrow a \bt_comp_cls by name from a plugin with
bt_plugin_borrow_source_component_class_by_name_const(),
bt_plugin_borrow_filter_component_class_by_name_const(), and
bt_plugin_borrow_sink_component_class_by_name_const().

The bt_plugin_find_all(), bt_plugin_find_all_from_file(),
bt_plugin_find_all_from_dir(), and bt_plugin_find_all_from_static()
functions return a <strong>plugin set</strong>, that is, a shared object
containing one or more plugins.

<h1>Find and load plugins</h1>

\anchor api-plugin-def-dirs The bt_plugin_find() and
bt_plugin_find_all() functions find and load plugins from the default
plugin search directories and from the static plugins.

The plugin search order is:

-# The colon-separated (or semicolon-separated on Windows) list of
   directories in the \c BABELTRACE_PLUGIN_PATH environment variable,
   if it's set.

   The function searches each directory in this list, without recursing.

-# <code>$HOME/.local/lib/babeltrace2/plugins</code>,
   without recursing.

-# The system \bt_name plugin directory, typically
   <code>/usr/lib/babeltrace2/plugins</code> or
   <code>/usr/local/lib/babeltrace2/plugins</code> on Linux,
   without recursing.

-# The static plugins.

Both bt_plugin_find() and bt_plugin_find_all() functions have dedicated
boolean parameters to include or exclude each of the four locations
above.

<h2>Find and load a plugin by name</h2>

Find and load a plugin by name with bt_plugin_find().

bt_plugin_find() tries to find a plugin with a specific name within
the \ref api-plugin-def-dirs "default plugin search directories"
and static plugins.

<h2>Find and load all the plugins from the default directories</h2>

Load all the plugins found in the
\ref api-plugin-def-dirs "default plugin search directories"
and static plugins with bt_plugin_find_all().

<h2>Find and load plugins from a specific file or directory</h2>

Find and load plugins from a specific file (<code>.so</code>,
<code>.dll</code>, or <code>.py</code>) with
bt_plugin_find_all_from_file().

A single shared object file can contain multiple plugins, although it's
not common practice to do so.

Find and load plugins from a specific directory with
bt_plugin_find_all_from_dir(). This function can search for plugins
within the given directory recursively or not.

<h2>Find and load static plugins</h2>

Find and load static plugins with bt_plugin_find_all_from_static().

A static plugin is built directly into the application or library
instead of being a separate shared object file.

<h1>Plugin properties</h1>

A plugin has the following properties:

<dl>
  <dt>
    \anchor api-plugin-prop-name
    Name
  </dt>
  <dd>
    Name of the plugin.

    The plugin's name is not related to its file name. For example,
    a plugin found in the file \c patente.so can be named
    <code>Dan</code>.

    Use bt_plugin_get_name().
  </dd>

  <dt>
    \anchor api-plugin-prop-descr
    \bt_dt_opt Description
  </dt>
  <dd>
    Description of the plugin.

    Use bt_plugin_get_description().
  </dd>

  <dt>
    \anchor api-plugin-prop-author
    \bt_dt_opt Author name(s)
  </dt>
  <dd>
    Name(s) of the plugin's author(s).

    Use bt_plugin_get_author().
  </dd>

  <dt>
    \anchor api-plugin-prop-license
    \bt_dt_opt License
  </dt>
  <dd>
    License or license name of the plugin.

    Use bt_plugin_get_license().
  </dd>

  <dt>
    \anchor api-plugin-prop-path
    \bt_dt_opt Path
  </dt>
  <dd>
    Path of the file which contains the plugin.

    A static plugin has no path property.

    Get bt_plugin_get_path().
  </dd>

  <dt>
    \anchor api-plugin-prop-version
    \bt_dt_opt Version
  </dt>
  <dd>
    Version of the plugin (major, minor, patch, and extra information).

    The plugin's version is completely user-defined: the library does
    not use this property in any way to verify the plugin's
    compatibility.

    Use bt_plugin_get_version().
  </dd>
</dl>
*/

/*! @{ */

/*!
@name Type
@{

@typedef struct bt_plugin bt_plugin;

@brief
    Plugin.


@typedef struct bt_plugin_set bt_plugin_set;

@brief
    Set of plugins.

@}
*/

/*!
@name Find and load plugins
@{
*/

/*!
@brief
    Status codes for bt_plugin_find().
*/
typedef enum bt_plugin_find_status {
	/*!
	@brief
	    Success.
	*/
	BT_PLUGIN_FIND_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Plugin not found.
	*/
	BT_PLUGIN_FIND_STATUS_NOT_FOUND		= __BT_FUNC_STATUS_NOT_FOUND,

	/*!
	@brief
	    Out of memory.
	*/
	BT_PLUGIN_FIND_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    Error.
	*/
	BT_PLUGIN_FIND_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_plugin_find_status;

/*!
@brief
    Finds and loads a single plugin which has the name
    \bt_p{plugin_name} from the default plugin search directories and
    static plugins, setting \bt_p{*plugin} to the result.

This function returns the first plugin which has the name
\bt_p{plugin_name} within, in order:

-# <strong>If the \bt_p{find_in_std_env_var} parameter is
   #BT_TRUE</strong>,
   the colon-separated (or semicolon-separated on Windows) list of
   directories in the \c BABELTRACE_PLUGIN_PATH environment variable,
   if it's set.

   The function searches each directory in this list, without recursing.

-# <strong>If the \bt_p{find_in_user_dir} parameter is
   #BT_TRUE</strong>, <code>$HOME/.local/lib/babeltrace2/plugins</code>,
   without recursing.

-# <strong>If the \bt_p{find_in_sys_dir} is #BT_TRUE</strong>, the
   system \bt_name plugin directory, typically
   <code>/usr/lib/babeltrace2/plugins</code> or
   <code>/usr/local/lib/babeltrace2/plugins</code> on Linux, without
   recursing.

-# <strong>If the \bt_p{find_in_static} is #BT_TRUE</strong>,
   the static plugins.

@note
    A plugin's name is not related to the name of its file (shared
    object or Python file). For example, a plugin found in the file
    \c patente.so can be named <code>Dan</code>.

If this function finds a file which looks like a plugin (shared object
file or Python file with the \c bt_plugin_ prefix), but it fails to load
it for any reason, the function:

<dl>
  <dt>If \bt_p{fail_on_load_error} is #BT_TRUE</dt>
  <dd>Returns #BT_PLUGIN_FIND_STATUS_ERROR.</dd>

  <dt>If \bt_p{fail_on_load_error} is #BT_FALSE</dt>
  <dd>Ignores the loading error and continues searching.</dd>
</dl>

If this function doesn't find any plugin, it returns
#BT_PLUGIN_FIND_STATUS_NOT_FOUND and does \em not set \bt_p{*plugin}.

@param[in] plugin_name
    Name of the plugin to find and load.
@param[in] find_in_std_env_var
    #BT_TRUE to try to find the plugin named \bt_p{plugin_name} in the
    colon-separated (or semicolon-separated on Windows) list of
    directories in the \c BABELTRACE_PLUGIN_PATH environment variable.
@param[in] find_in_user_dir
    #BT_TRUE to try to find the plugin named \bt_p{plugin_name} in
    the <code>$HOME/.local/lib/babeltrace2/plugins</code> directory,
    without recursing.
@param[in] find_in_sys_dir
    #BT_TRUE to try to find the plugin named \bt_p{plugin_name} in
    the system \bt_name plugin directory.
@param[in] find_in_static
    #BT_TRUE to try to find the plugin named \bt_p{plugin_name} in the
    static plugins.
@param[in] fail_on_load_error
    #BT_TRUE to make this function return #BT_PLUGIN_FIND_STATUS_ERROR
    on any plugin loading error instead of ignoring it.
@param[out] plugin
    <strong>On success</strong>, \bt_p{*plugin} is a new plugin
    reference of named \bt_p{plugin_name}.

@retval #BT_PLUGIN_FIND_STATUS_OK
    Success.
@retval #BT_PLUGIN_FIND_STATUS_NOT_FOUND
    Plugin not found.
@retval #BT_PLUGIN_FIND_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_PLUGIN_FIND_STATUS_ERROR
    Error.

@bt_pre_not_null{plugin_name}
@pre
    At least one of the \bt_p{find_in_std_env_var},
    \bt_p{find_in_user_dir}, \bt_p{find_in_sys_dir}, and
    \bt_p{find_in_static} parameters is #BT_TRUE.
@bt_pre_not_null{plugin}

@sa bt_plugin_find_all() &mdash;
    Finds and loads all plugins from the default plugin search
    directories and static plugins.
*/
extern bt_plugin_find_status bt_plugin_find(const char *plugin_name,
		bt_bool find_in_std_env_var, bt_bool find_in_user_dir,
		bt_bool find_in_sys_dir, bt_bool find_in_static,
		bt_bool fail_on_load_error, const bt_plugin **plugin);

/*!
@brief
    Status codes for bt_plugin_find_all().
*/
typedef enum bt_plugin_find_all_status {
	/*!
	@brief
	    Success.
	*/
	BT_PLUGIN_FIND_ALL_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    No plugins found.
	*/
	BT_PLUGIN_FIND_ALL_STATUS_NOT_FOUND	= __BT_FUNC_STATUS_NOT_FOUND,

	/*!
	@brief
	    Out of memory.
	*/
	BT_PLUGIN_FIND_ALL_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    Error.
	*/
	BT_PLUGIN_FIND_ALL_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_plugin_find_all_status;

/*!
@brief
    Finds and loads all the plugins from the default
    plugin search directories and static plugins, setting
    \bt_p{*plugins} to the result.

This function returns all the plugins within, in order:

-# <strong>If the \bt_p{find_in_std_env_var} parameter is
   #BT_TRUE</strong>,
   the colon-separated (or semicolon-separated on Windows) list of
   directories in the \c BABELTRACE_PLUGIN_PATH environment variable,
   if it's set.

   The function searches each directory in this list, without recursing.

-# <strong>If the \bt_p{find_in_user_dir} parameter is
   #BT_TRUE</strong>, <code>$HOME/.local/lib/babeltrace2/plugins</code>,
   without recursing.

-# <strong>If the \bt_p{find_in_sys_dir} is #BT_TRUE</strong>, the
   system \bt_name plugin directory, typically
   <code>/usr/lib/babeltrace2/plugins</code> or
   <code>/usr/local/lib/babeltrace2/plugins</code> on Linux, without
   recursing.

-# <strong>If the \bt_p{find_in_static} is #BT_TRUE</strong>,
   the static plugins.

During the search process, if a found plugin shares the name of an
already loaded plugin, this function ignores it and continues.

If this function finds a file which looks like a plugin (shared object
file or Python file with the \c bt_plugin_ prefix), but it fails to load
it for any reason, the function:

<dl>
  <dt>If \bt_p{fail_on_load_error} is #BT_TRUE</dt>
  <dd>Returns #BT_PLUGIN_FIND_ALL_STATUS_ERROR.</dd>

  <dt>If \bt_p{fail_on_load_error} is #BT_FALSE</dt>
  <dd>Ignores the loading error and continues searching.</dd>
</dl>

If this function doesn't find any plugin, it returns
#BT_PLUGIN_FIND_ALL_STATUS_NOT_FOUND and does \em not set
\bt_p{*plugins}.

@param[in] find_in_std_env_var
    #BT_TRUE to try to find all the plugins in the
    colon-separated (or semicolon-separated on Windows) list of
    directories in the \c BABELTRACE_PLUGIN_PATH environment variable.
@param[in] find_in_user_dir
    #BT_TRUE to try to find all the plugins in
    the <code>$HOME/.local/lib/babeltrace2/plugins</code> directory,
    without recursing.
@param[in] find_in_sys_dir
    #BT_TRUE to try to find all the plugins in the system \bt_name
    plugin directory.
@param[in] find_in_static
    #BT_TRUE to try to find all the plugins in the static plugins.
@param[in] fail_on_load_error
    #BT_TRUE to make this function return
    #BT_PLUGIN_FIND_ALL_STATUS_ERROR on any plugin loading error instead
    of ignoring it.
@param[out] plugins
    <strong>On success</strong>, \bt_p{*plugins} is a new plugin set
    reference which contains all the plugins found from the default
    plugin search directories and static plugins.

@retval #BT_PLUGIN_FIND_ALL_STATUS_OK
    Success.
@retval #BT_PLUGIN_FIND_ALL_STATUS_NOT_FOUND
    No plugins found.
@retval #BT_PLUGIN_FIND_ALL_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_PLUGIN_FIND_ALL_STATUS_ERROR
    Error.

@pre
    At least one of the \bt_p{find_in_std_env_var},
    \bt_p{find_in_user_dir}, \bt_p{find_in_sys_dir}, and
    \bt_p{find_in_static} parameters is #BT_TRUE.
@bt_pre_not_null{plugins}

@sa bt_plugin_find() &mdash;
    Finds and loads a single plugin by name from the default plugin search
    directories and static plugins.
*/
bt_plugin_find_all_status bt_plugin_find_all(bt_bool find_in_std_env_var,
		bt_bool find_in_user_dir, bt_bool find_in_sys_dir,
		bt_bool find_in_static, bt_bool fail_on_load_error,
		const bt_plugin_set **plugins);

/*!
@brief
    Status codes for bt_plugin_find_all_from_file().
*/
typedef enum bt_plugin_find_all_from_file_status {
	/*!
	@brief
	    Success.
	*/
	BT_PLUGIN_FIND_ALL_FROM_FILE_STATUS_OK			= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    No plugins found.
	*/
	BT_PLUGIN_FIND_ALL_FROM_FILE_STATUS_NOT_FOUND		= __BT_FUNC_STATUS_NOT_FOUND,

	/*!
	@brief
	    Out of memory.
	*/
	BT_PLUGIN_FIND_ALL_FROM_FILE_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    Error.
	*/
	BT_PLUGIN_FIND_ALL_FROM_FILE_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_plugin_find_all_from_file_status;

/*!
@brief
    Finds and loads all the plugins from the file with path \bt_p{path},
    setting \bt_p{*plugins} to the result.

@note
    A plugin's name is not related to the name of its file (shared
    object or Python file). For example, a plugin found in the file
    \c patente.so can be named <code>Dan</code>.

If any plugin loading error occurs during this function's execution, the
function:

<dl>
  <dt>If \bt_p{fail_on_load_error} is #BT_TRUE</dt>
  <dd>Returns #BT_PLUGIN_FIND_ALL_FROM_FILE_STATUS_ERROR.</dd>

  <dt>If \bt_p{fail_on_load_error} is #BT_FALSE</dt>
  <dd>Ignores the loading error and continues.</dd>
</dl>

If this function doesn't find any plugin, it returns
#BT_PLUGIN_FIND_ALL_FROM_FILE_STATUS_NOT_FOUND and does \em not set
\bt_p{*plugins}.

@param[in] path
    Path of the file in which to find and load all the plugins.
@param[in] fail_on_load_error
    #BT_TRUE to make this function return
    #BT_PLUGIN_FIND_ALL_FROM_FILE_STATUS_ERROR on any plugin loading
    error instead of ignoring it.
@param[out] plugins
    <strong>On success</strong>, \bt_p{*plugins} is a new plugin set
    reference which contains all the plugins found in the file with path
    \bt_p{path}.

@retval #BT_PLUGIN_FIND_ALL_FROM_FILE_STATUS_OK
    Success.
@retval #BT_PLUGIN_FIND_ALL_FROM_FILE_STATUS_NOT_FOUND
    No plugins found.
@retval #BT_PLUGIN_FIND_ALL_FROM_FILE_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_PLUGIN_FIND_ALL_FROM_FILE_STATUS_ERROR
    Error.

@bt_pre_not_null{path}
@pre
    \bt_p{path} is the path of a regular file.
@bt_pre_not_null{plugins}

@sa bt_plugin_find_all_from_dir() &mdash;
    Finds and loads all plugins from a given directory.
*/
extern bt_plugin_find_all_from_file_status bt_plugin_find_all_from_file(
		const char *path, bt_bool fail_on_load_error,
		const bt_plugin_set **plugins);

/*!
@brief
    Status codes for bt_plugin_find_all_from_dir().
*/
typedef enum bt_plugin_find_all_from_dir_status {
	/*!
	@brief
	    Success.
	*/
	BT_PLUGIN_FIND_ALL_FROM_DIR_STATUS_OK			= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    No plugins found.
	*/
	BT_PLUGIN_FIND_ALL_FROM_DIR_STATUS_NOT_FOUND		= __BT_FUNC_STATUS_NOT_FOUND,

	/*!
	@brief
	    Out of memory.
	*/
	BT_PLUGIN_FIND_ALL_FROM_DIR_STATUS_MEMORY_ERROR		= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    Error.
	*/
	BT_PLUGIN_FIND_ALL_FROM_DIR_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_plugin_find_all_from_dir_status;

/*!
@brief
    Finds and loads all the plugins from the directory with path
    \bt_p{path}, setting \bt_p{*plugins} to the result.

If \bt_p{recurse} is #BT_TRUE, this function recurses into the
subdirectories of \bt_p{path} to find plugins.

During the search process, if a found plugin shares the name of an
already loaded plugin, this function ignores it and continues.

@attention
    As of \bt_name_version_min_maj, the file and directory traversal
    order is undefined.

If any plugin loading error occurs during this function's execution, the
function:

<dl>
  <dt>If \bt_p{fail_on_load_error} is #BT_TRUE</dt>
  <dd>Returns #BT_PLUGIN_FIND_ALL_FROM_DIR_STATUS_ERROR.</dd>

  <dt>If \bt_p{fail_on_load_error} is #BT_FALSE</dt>
  <dd>Ignores the loading error and continues.</dd>
</dl>

If this function doesn't find any plugin, it returns
#BT_PLUGIN_FIND_ALL_FROM_DIR_STATUS_NOT_FOUND and does \em not set
\bt_p{*plugins}.

@param[in] path
    Path of the directory in which to find and load all the plugins.
@param[in] recurse
    #BT_TRUE to make this function recurse into the subdirectories
    of \bt_p{path}.
@param[in] fail_on_load_error
    #BT_TRUE to make this function return
    #BT_PLUGIN_FIND_ALL_FROM_DIR_STATUS_ERROR on any plugin loading
    error instead of ignoring it.
@param[out] plugins
    <strong>On success</strong>, \bt_p{*plugins} is a new plugin set
    reference which contains all the plugins found in the directory with
    path \bt_p{path}.

@retval #BT_PLUGIN_FIND_ALL_FROM_DIR_STATUS_OK
    Success.
@retval #BT_PLUGIN_FIND_ALL_FROM_DIR_STATUS_NOT_FOUND
    No plugins found.
@retval #BT_PLUGIN_FIND_ALL_FROM_DIR_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_PLUGIN_FIND_ALL_FROM_DIR_STATUS_ERROR
    Error.

@bt_pre_not_null{path}
@pre
    \bt_p{path} is the path of a directory.
@bt_pre_not_null{plugins}

@sa bt_plugin_find_all_from_file() &mdash;
    Finds and loads all plugins from a given file.
*/
extern bt_plugin_find_all_from_dir_status bt_plugin_find_all_from_dir(
		const char *path, bt_bool recurse, bt_bool fail_on_load_error,
		const bt_plugin_set **plugins);

/*!
@brief
    Status codes for bt_plugin_find_all_from_static().
*/
typedef enum bt_plugin_find_all_from_static_status {
	/*!
	@brief
	    Success.
	*/
	BT_PLUGIN_FIND_ALL_FROM_STATIC_STATUS_OK			= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    No static plugins found.
	*/
	BT_PLUGIN_FIND_ALL_FROM_STATIC_STATUS_NOT_FOUND			= __BT_FUNC_STATUS_NOT_FOUND,

	/*!
	@brief
	    Out of memory.
	*/
	BT_PLUGIN_FIND_ALL_FROM_STATIC_STATUS_MEMORY_ERROR		= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    Error.
	*/
	BT_PLUGIN_FIND_ALL_FROM_STATIC_STATUS_ERROR			= __BT_FUNC_STATUS_ERROR,
} bt_plugin_find_all_from_static_status;

/*!
@brief
    Finds and loads all the static plugins,
    setting \bt_p{*plugins} to the result.

A static plugin is built directly into the application or library
instead of being a separate shared object file.

If any plugin loading error occurs during this function's execution, the
function:

<dl>
  <dt>If \bt_p{fail_on_load_error} is #BT_TRUE</dt>
  <dd>Returns #BT_PLUGIN_FIND_ALL_FROM_STATIC_STATUS_ERROR.</dd>

  <dt>If \bt_p{fail_on_load_error} is #BT_FALSE</dt>
  <dd>Ignores the loading error and continues.</dd>
</dl>

If this function doesn't find any plugin, it returns
#BT_PLUGIN_FIND_ALL_FROM_STATIC_STATUS_NOT_FOUND and does \em not set
\bt_p{*plugins}.

@param[in] fail_on_load_error
    #BT_TRUE to make this function return
    #BT_PLUGIN_FIND_ALL_FROM_STATIC_STATUS_ERROR on any plugin loading
    error instead of ignoring it.
@param[out] plugins
    <strong>On success</strong>, \bt_p{*plugins} is a new plugin set
    reference which contains all the static plugins.

@retval #BT_PLUGIN_FIND_ALL_FROM_STATIC_STATUS_OK
    Success.
@retval #BT_PLUGIN_FIND_ALL_FROM_STATIC_STATUS_NOT_FOUND
    No static plugins found.
@retval #BT_PLUGIN_FIND_ALL_FROM_STATIC_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_PLUGIN_FIND_ALL_FROM_STATIC_STATUS_ERROR
    Error.

@bt_pre_not_null{path}
@bt_pre_not_null{plugins}
*/
extern bt_plugin_find_all_from_static_status bt_plugin_find_all_from_static(
		bt_bool fail_on_load_error, const bt_plugin_set **plugins);

/*! @} */

/*!
@name Plugin properties
@{
*/

/*!
@brief
    Returns the name of the plugin \bt_p{plugin}.

See the \ref api-plugin-prop-name "name" property.

@param[in] plugin
    Plugin of which to get the name.

@returns
    @parblock
    Name of \bt_p{plugin}.

    The returned pointer remains valid as long as \bt_p{plugin} exists.
    @endparblock

@bt_pre_not_null{plugin}
*/
extern const char *bt_plugin_get_name(const bt_plugin *plugin);

/*!
@brief
    Returns the description of the plugin \bt_p{plugin}.

See the \ref api-plugin-prop-descr "description" property.

@param[in] plugin
    Plugin of which to get description.

@returns
    @parblock
    Description of \bt_p{plugin}, or \c NULL if not available.

    The returned pointer remains valid as long as \bt_p{plugin} exists.
    @endparblock

@bt_pre_not_null{plugin}
*/
extern const char *bt_plugin_get_description(const bt_plugin *plugin);

/*!
@brief
    Returns the name(s) of the author(s) of the plugin \bt_p{plugin}.

See the \ref api-plugin-prop-author "author name(s)" property.

@param[in] plugin
    Plugin of which to get the author name(s).

@returns
    @parblock
    Author name(s) of \bt_p{plugin}, or \c NULL if not available.

    The returned pointer remains valid as long as \bt_p{plugin} exists.
    @endparblock

@bt_pre_not_null{plugin}
*/
extern const char *bt_plugin_get_author(const bt_plugin *plugin);

/*!
@brief
    Returns the license text or the license name of the plugin
    \bt_p{plugin}.

See the \ref api-plugin-prop-license "license" property.

@param[in] plugin
    Plugin of which to get the license.

@returns
    @parblock
    License of \bt_p{plugin}, or \c NULL if not available.

    The returned pointer remains valid as long as \bt_p{plugin} exists.
    @endparblock

@bt_pre_not_null{plugin}
*/
extern const char *bt_plugin_get_license(const bt_plugin *plugin);

/*!
@brief
    Returns the path of the file which contains the plugin
    \bt_p{plugin}.

See the \ref api-plugin-prop-path "path" property.

This function returns \c NULL if \bt_p{plugin} is a static plugin
because a static plugin has no path property.

@param[in] plugin
    Plugin of which to get the containing file's path.

@returns
    @parblock
    Path of the file which contains \bt_p{plugin}, or \c NULL if
    not available.

    The returned pointer remains valid as long as \bt_p{plugin} exists.
    @endparblock

@bt_pre_not_null{plugin}
*/
extern const char *bt_plugin_get_path(const bt_plugin *plugin);

/*!
@brief
    Returns the version of the plugin \bt_p{plugin}.

See the \ref api-plugin-prop-version "version" property.

@param[in] plugin
    Plugin of which to get the version.
@param[out] major
    <strong>If not \c NULL and this function returns
    #BT_PROPERTY_AVAILABILITY_AVAILABLE</strong>, \bt_p{*major} is the
    major version of \bt_p{plugin}.
@param[out] minor
    <strong>If not \c NULL and this function returns
    #BT_PROPERTY_AVAILABILITY_AVAILABLE</strong>, \bt_p{*minor} is the
    minor version of \bt_p{plugin}.
@param[out] patch
    <strong>If not \c NULL and this function returns
    #BT_PROPERTY_AVAILABILITY_AVAILABLE</strong>, \bt_p{*patch} is the
    patch version of \bt_p{plugin}.
@param[out] extra
    @parblock
    <strong>If not \c NULL and this function returns
    #BT_PROPERTY_AVAILABILITY_AVAILABLE</strong>, \bt_p{*extra} is the
    version's extra information of \bt_p{plugin}.

    \bt_p{*extra} can be \c NULL if the plugin's version has no extra
    information.

    \bt_p{*extra} remains valid as long as \bt_p{plugin} exists.
    @endparblock

@retval #BT_PROPERTY_AVAILABILITY_AVAILABLE
    The version of \bt_p{plugin} is available.
@retval #BT_PROPERTY_AVAILABILITY_NOT_AVAILABLE
    The version of \bt_p{plugin} is not available.

@bt_pre_not_null{plugin}
*/
extern bt_property_availability bt_plugin_get_version(
		const bt_plugin *plugin, unsigned int *major,
		unsigned int *minor, unsigned int *patch, const char **extra);

/*! @} */

/*!
@name Plugin component class access
@{
*/

/*!
@brief
    Returns the number of source component classes contained in the
    plugin \bt_p{plugin}.

@param[in] plugin
    Plugin of which to get the number of contained source
    component classes.

@returns
    Number of contained source component classes in \bt_p{plugin}.

@bt_pre_not_null{plugin}
*/
extern uint64_t bt_plugin_get_source_component_class_count(
		const bt_plugin *plugin);

/*!
@brief
    Returns the number of filter component classes contained in the
    plugin \bt_p{plugin}.

@param[in] plugin
    Plugin of which to get the number of contained filter
    component classes.

@returns
    Number of contained filter component classes in \bt_p{plugin}.

@bt_pre_not_null{plugin}
*/
extern uint64_t bt_plugin_get_filter_component_class_count(
		const bt_plugin *plugin);

/*!
@brief
    Returns the number of sink component classes contained in the
    plugin \bt_p{plugin}.

@param[in] plugin
    Plugin of which to get the number of contained sink
    component classes.

@returns
    Number of contained sink component classes in \bt_p{plugin}.

@bt_pre_not_null{plugin}
*/
extern uint64_t bt_plugin_get_sink_component_class_count(
		const bt_plugin *plugin);

/*!
@brief
    Borrows the source component class at index \bt_p{index} from the
    plugin \bt_p{plugin}.

@param[in] plugin
    Plugin from which to borrow the source component class at index
    \bt_p{index}.
@param[in] index
    Index of the source component class to borrow from \bt_p{plugin}.

@returns
    @parblock
    \em Borrowed reference of the source component class of
    \bt_p{plugin} at index \bt_p{index}.

    The returned pointer remains valid as long as \bt_p{plugin} exists.
    @endparblock

@bt_pre_not_null{plugin}
@pre
    \bt_p{index} is less than the number of source component classes in
    \bt_p{plugin} (as returned by
    bt_plugin_get_source_component_class_count()).

@sa bt_plugin_borrow_source_component_class_by_name_const() &mdash;
    Borrows a source component class by name from a plugin.
*/
extern const bt_component_class_source *
bt_plugin_borrow_source_component_class_by_index_const(
		const bt_plugin *plugin, uint64_t index);

/*!
@brief
    Borrows the filter component class at index \bt_p{index} from the
    plugin \bt_p{plugin}.

@param[in] plugin
    Plugin from which to borrow the filter component class at index
    \bt_p{index}.
@param[in] index
    Index of the filter component class to borrow from \bt_p{plugin}.

@returns
    @parblock
    \em Borrowed reference of the filter component class of
    \bt_p{plugin} at index \bt_p{index}.

    The returned pointer remains valid as long as \bt_p{plugin} exists.
    @endparblock

@bt_pre_not_null{plugin}
@pre
    \bt_p{index} is less than the number of filter component classes in
    \bt_p{plugin} (as returned by
    bt_plugin_get_source_component_class_count()).

@sa bt_plugin_borrow_source_component_class_by_name_const() &mdash;
    Borrows a filter component class by name from a plugin.
*/
extern const bt_component_class_filter *
bt_plugin_borrow_filter_component_class_by_index_const(
		const bt_plugin *plugin, uint64_t index);

/*!
@brief
    Borrows the sink component class at index \bt_p{index} from the
    plugin \bt_p{plugin}.

@param[in] plugin
    Plugin from which to borrow the sink component class at index
    \bt_p{index}.
@param[in] index
    Index of the sink component class to borrow from \bt_p{plugin}.

@returns
    @parblock
    \em Borrowed reference of the sink component class of
    \bt_p{plugin} at index \bt_p{index}.

    The returned pointer remains valid as long as \bt_p{plugin} exists.
    @endparblock

@bt_pre_not_null{plugin}
@pre
    \bt_p{index} is less than the number of sink component classes in
    \bt_p{plugin} (as returned by
    bt_plugin_get_source_component_class_count()).

@sa bt_plugin_borrow_source_component_class_by_name_const() &mdash;
    Borrows a sink component class by name from a plugin.
*/
extern const bt_component_class_sink *
bt_plugin_borrow_sink_component_class_by_index_const(
		const bt_plugin *plugin, uint64_t index);

/*!
@brief
    Borrows the source component class named \bt_p{name} from the
    plugin \bt_p{plugin}.

If no source component class has the name \bt_p{name} within
\bt_p{plugin}, this function returns \c NULL.

@param[in] plugin
    Plugin from which to borrow the source component class named
    \bt_p{name}.
@param[in] name
    Name of the source component class to borrow from \bt_p{plugin}.

@returns
    @parblock
    \em Borrowed reference of the source component class of
    \bt_p{plugin} named \bt_p{name}, or \c NULL if no source component
    class is named \bt_p{name} within \bt_p{plugin}.

    The returned pointer remains valid as long as \bt_p{plugin} exists.
    @endparblock

@bt_pre_not_null{plugin}
@bt_pre_not_null{name}

@sa bt_plugin_borrow_source_component_class_by_index_const() &mdash;
    Borrows a source component class by index from a plugin.
*/
extern const bt_component_class_source *
bt_plugin_borrow_source_component_class_by_name_const(
		const bt_plugin *plugin, const char *name);

/*!
@brief
    Borrows the filter component class named \bt_p{name} from the
    plugin \bt_p{plugin}.

If no filter component class has the name \bt_p{name} within
\bt_p{plugin}, this function returns \c NULL.

@param[in] plugin
    Plugin from which to borrow the filter component class named
    \bt_p{name}.
@param[in] name
    Name of the filter component class to borrow from \bt_p{plugin}.

@returns
    @parblock
    \em Borrowed reference of the filter component class of
    \bt_p{plugin} named \bt_p{name}, or \c NULL if no filter component
    class is named \bt_p{name} within \bt_p{plugin}.

    The returned pointer remains valid as long as \bt_p{plugin} exists.
    @endparblock

@bt_pre_not_null{plugin}
@bt_pre_not_null{name}

@sa bt_plugin_borrow_filter_component_class_by_index_const() &mdash;
    Borrows a filter component class by index from a plugin.
*/
extern const bt_component_class_filter *
bt_plugin_borrow_filter_component_class_by_name_const(
		const bt_plugin *plugin, const char *name);

/*!
@brief
    Borrows the sink component class named \bt_p{name} from the
    plugin \bt_p{plugin}.

If no sink component class has the name \bt_p{name} within
\bt_p{plugin}, this function returns \c NULL.

@param[in] plugin
    Plugin from which to borrow the sink component class named
    \bt_p{name}.
@param[in] name
    Name of the sink component class to borrow from \bt_p{plugin}.

@returns
    @parblock
    \em Borrowed reference of the sink component class of
    \bt_p{plugin} named \bt_p{name}, or \c NULL if no sink component
    class is named \bt_p{name} within \bt_p{plugin}.

    The returned pointer remains valid as long as \bt_p{plugin} exists.
    @endparblock

@bt_pre_not_null{plugin}
@bt_pre_not_null{name}

@sa bt_plugin_borrow_sink_component_class_by_index_const() &mdash;
    Borrows a sink component class by index from a plugin.
*/
extern const bt_component_class_sink *
bt_plugin_borrow_sink_component_class_by_name_const(
		const bt_plugin *plugin, const char *name);

/*! @} */

/*!
@name Plugin reference count
@{
*/

/*!
@brief
    Increments the \ref api-fund-shared-object "reference count" of
    the plugin \bt_p{plugin}.

@param[in] plugin
    @parblock
    Plugin of which to increment the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_plugin_put_ref() &mdash;
    Decrements the reference count of a plugin.
*/
extern void bt_plugin_get_ref(const bt_plugin *plugin);

/*!
@brief
    Decrements the \ref api-fund-shared-object "reference count" of
    the plugin \bt_p{plugin}.

@param[in] plugin
    @parblock
    Plugin of which to decrement the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_plugin_get_ref() &mdash;
    Increments the reference count of a plugin.
*/
extern void bt_plugin_put_ref(const bt_plugin *plugin);

/*!
@brief
    Decrements the reference count of the plugin \bt_p{_plugin}, and
    then sets \bt_p{_plugin} to \c NULL.

@param _plugin
    @parblock
    Plugin of which to decrement the reference count.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_plugin}
*/
#define BT_PLUGIN_PUT_REF_AND_RESET(_plugin)		\
	do {						\
		bt_plugin_put_ref(_plugin);		\
		(_plugin) = NULL;			\
	} while (0)

/*!
@brief
    Decrements the reference count of the plugin \bt_p{_dst}, sets
    \bt_p{_dst} to \bt_p{_src}, and then sets \bt_p{_src} to \c NULL.

This macro effectively moves a plugin reference from the expression
\bt_p{_src} to the expression \bt_p{_dst}, putting the existing
\bt_p{_dst} reference.

@param _dst
    @parblock
    Destination expression.

    Can contain \c NULL.
    @endparblock
@param _src
    @parblock
    Source expression.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_dst}
@bt_pre_assign_expr{_src}
*/
#define BT_PLUGIN_MOVE_REF(_dst, _src)		\
	do {					\
		bt_plugin_put_ref(_dst);	\
		(_dst) = (_src);		\
		(_src) = NULL;			\
	} while (0)

/*! @} */

/*!
@name Plugin set plugin access
@{
*/

/*!
@brief
    Returns the number of plugins contained in the
    plugin set \bt_p{plugin_set}.

@param[in] plugin_set
    Plugin set of which to get the number of contained plugins.

@returns
    Number of contained plugins in \bt_p{plugin_set}.

@bt_pre_not_null{plugin}
*/
extern uint64_t bt_plugin_set_get_plugin_count(
		const bt_plugin_set *plugin_set);

/*!
@brief
    Borrows the plugin at index \bt_p{index} from the plugin set
    \bt_p{plugin_set}.

@param[in] plugin_set
    Plugin set from which to borrow the plugin at index \bt_p{index}.
@param[in] index
    Index of the plugin to borrow from \bt_p{plugin_set}.

@returns
    @parblock
    \em Borrowed reference of the plugin of \bt_p{plugin_set} at index
    \bt_p{index}.

    The returned pointer remains valid until \bt_p{plugin_set} is
    modified.
    @endparblock

@bt_pre_not_null{plugin_set}
@pre
    \bt_p{index} is less than the number of plugins in
    \bt_p{plugin_set} (as returned by bt_plugin_set_get_plugin_count()).
*/
extern const bt_plugin *bt_plugin_set_borrow_plugin_by_index_const(
		const bt_plugin_set *plugin_set, uint64_t index);

/*! @} */

/*!
@name Plugin set reference count
@{
*/

/*!
@brief
    Increments the \ref api-fund-shared-object "reference count" of
    the plugin set \bt_p{plugin_set}.

@param[in] plugin_set
    @parblock
    Plugin set of which to increment the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_plugin_set_put_ref() &mdash;
    Decrements the reference count of a plugin set.
*/
extern void bt_plugin_set_get_ref(const bt_plugin_set *plugin_set);

/*!
@brief
    Decrements the \ref api-fund-shared-object "reference count" of
    the plugin set \bt_p{plugin_set}.

@param[in] plugin_set
    @parblock
    Plugin set of which to decrement the reference count.

    Can be \c NULL.
    @endparblock

@sa bt_plugin_set_get_ref() &mdash;
    Increments the reference count of a plugin set.
*/
extern void bt_plugin_set_put_ref(const bt_plugin_set *plugin_set);

/*!
@brief
    Decrements the reference count of the plugin set \bt_p{_plugin_set},
    and then sets \bt_p{_plugin_set} to \c NULL.

@param _plugin_set
    @parblock
    Plugin set of which to decrement the reference count.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_plugin_set}
*/
#define BT_PLUGIN_SET_PUT_REF_AND_RESET(_plugin_set)	\
	do {						\
		bt_plugin_set_put_ref(_plugin_set);	\
		(_plugin_set) = NULL;			\
	} while (0)

/*!
@brief
    Decrements the reference count of the plugin set \bt_p{_dst}, sets
    \bt_p{_dst} to \bt_p{_src}, and then sets \bt_p{_src} to \c NULL.

This macro effectively moves a plugin set reference from the expression
\bt_p{_src} to the expression \bt_p{_dst}, putting the existing
\bt_p{_dst} reference.

@param _dst
    @parblock
    Destination expression.

    Can contain \c NULL.
    @endparblock
@param _src
    @parblock
    Source expression.

    Can contain \c NULL.
    @endparblock

@bt_pre_assign_expr{_dst}
@bt_pre_assign_expr{_src}
*/
#define BT_PLUGIN_SET_MOVE_REF(_dst, _src)	\
	do {					\
		bt_plugin_set_put_ref(_dst);	\
		(_dst) = (_src);		\
		(_src) = NULL;			\
	} while (0)

/*! @} */

/*! @} */

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_PLUGIN_PLUGIN_LOADING_H */
