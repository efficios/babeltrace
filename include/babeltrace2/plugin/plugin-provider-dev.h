/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2026 EfficiOS Inc. and Linux Foundation
 */

#ifndef BABELTRACE2_PLUGIN_PLUGIN_PROVIDER_DEV_H
#define BABELTRACE2_PLUGIN_PLUGIN_PROVIDER_DEV_H

/* IWYU pragma: private, include <babeltrace2/babeltrace.h> */

#ifndef __BT_IN_BABELTRACE_H
# error "Please include <babeltrace2/babeltrace.h> instead."
#endif

#include <stdint.h>

#include <babeltrace2/plugin/so-dev-common.h>
#include <babeltrace2/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
@defgroup api-plugin-provider-dev Plugin provider development

@brief
    Plugin provider development.

This module offers macros to create a \bt_name plugin provider.

Behind the scenes, the <code>BT_PLUGIN_PROVIDER*()</code> macros of this
module create and fill global tables which are located in sections of
the shared object with specific names.

@sa
    - \ref guide-write-plugin-provider
    - \ref guide-comp-link-plugin-provider-so

<h1>Self plugin provider</h1>

The type is a private view of a plugin provider from within a plugin
provider development method.

Set and get user data attached to a plugin provider with
bt_self_plugin_provider_set_data() and
bt_self_plugin_provider_get_data().

<h1>Logging</h1>

The methods of a plugin provider should drive their logging statements
using the logging level returned by
bt_self_plugin_provider_get_logging_level().
Because the effective logging level can change between calls, each
method must sample it with bt_self_plugin_provider_get_logging_level()
at its entry point and use that sampled value for the duration of the
call rather than caching it across calls.

<h1>Plugin provider definition C file structure</h1>

The structure of a \bt_name plugin provider definition C file is as such:

<ol>
  <li>
    Start with

    @code
    BT_PLUGIN_PROVIDER_MODULE();
    @endcode
  </li>

  <li>
    Define a \bt_name plugin provider with BT_PLUGIN_PROVIDER() if the
    name of the plugin provider is a valid C identifier, or with
    BT_PLUGIN_PROVIDER_WITH_ID() otherwise.

    See \ref api-plugin-provider-dev-custom-plugin-provider-id
    "Custom plugin provider ID" to learn more about plugin provider IDs.

    @note
        When you use BT_PLUGIN_PROVIDER(), the ID of the plugin provider is
        <code>auto</code>.
  </li>

  <li>
    \bt_dt_opt Use any of the following macros (or their
    <code>*_WITH_ID()</code> counterpart) \em once to set the properties
    of the plugin provider:

    - BT_PLUGIN_PROVIDER_AUTHOR()
    - BT_PLUGIN_PROVIDER_DESCRIPTION()
    - BT_PLUGIN_PROVIDER_LICENSE()
    - BT_PLUGIN_PROVIDER_VERSION()
  </li>

  <li>
    \bt_dt_opt Use any of the following macros (or their
    <code>*_WITH_ID()</code> counterpart) \em once to set the
    initialization and finalization functions of the plugin provider:

    - BT_PLUGIN_PROVIDER_INITIALIZE_FUNC()
    - BT_PLUGIN_PROVIDER_FINALIZE_FUNC()

    libbabeltrace2 executes the initialization function of a plugin
    provider when it loads its shared object.

    libbabeltrace2 executes the finalization function of a plugin
    provider when it destroys its shared object, and only if the
    initialization function, if any, succeeded.
  </li>

  <li>
    Use one or both of the following macros (or their
    <code>*_WITH_ID()</code> counterpart) \em once to set the plugin
    provider methods:

    - BT_PLUGIN_PROVIDER_CREATE_ALL_FROM_FILE_FUNC()
    - BT_PLUGIN_PROVIDER_CREATE_ALL_FROM_STATIC_FUNC()
  </li>
</ol>

<h1>\anchor api-plugin-provider-dev-custom-plugin-provider-id Custom plugin provider ID</h1>

The BT_PLUGIN_PROVIDER() macro defines a plugin with a specific name and
the ID <code>auto</code>.

All the <code>BT_PLUGIN_PROVIDER*()</code> macros which do not end with
<code>_WITH_ID</code> refer to the <code>auto</code> plugin provider.

There are two situations which demand that you use a custom plugin
provider ID:

- You want more than one plugin provider contained in your shared object
  file.

  Although the \bt_name project does not recommend this, it is possible.

  In this case, each plugin provider of the shared object needs its own,
  unique ID.

- You want to give the plugin provider a name which is not a valid C
  identifier.

  The BT_PLUGIN_PROVIDER() macro accepts a C identifier as the plugin
  provider name, while the BT_PLUGIN_PROVIDER_WITH_ID() accepts a C
  identifier for the ID and a C string for the name.

To define a plugin provider with a specific ID, use
BT_PLUGIN_PROVIDER_WITH_ID(), for example:

@code
BT_PLUGIN_PROVIDER_WITH_ID(my_plugin_provider_id, "my-plugin-provider-name");
@endcode

Then, use the <code>BT_PLUGIN_PROVIDER*_WITH_ID()</code> macros to refer
to this specific plugin provider, for example:

@code
BT_PLUGIN_PROVIDER_AUTHOR_WITH_ID(my_plugin_provider_id, "Julien Poulin");
@endcode

@note
    @parblock
    You can still use the <code>auto</code> ID with
    BT_PLUGIN_PROVIDER_WITH_ID() to use the simpler macros afterwards
    while still giving the plugin provider a name which is not a valid
    C identifier, for example:

    @code
    BT_PLUGIN_PROVIDER_WITH_ID(auto, "my-plugin-provider-name");
    BT_PLUGIN_PROVIDER_AUTHOR("Julien Poulin");
    @endcode
    @endparblock
*/

/*! @{ */

/*!
@name Types
@{

@typedef struct bt_self_plugin_provider bt_self_plugin_provider;

@brief
    Self plugin provider.

@typedef struct bt_plugin_provider_create_all_from_file_options bt_plugin_provider_create_all_from_file_options;

@brief
    Options for #bt_plugin_provider_create_all_from_file_func.

@typedef struct bt_plugin_provider_create_all_from_static_options bt_plugin_provider_create_all_from_static_options;

@brief
    Options for #bt_plugin_provider_create_all_from_static_func.

@}
*/

/*!
@name Plugin provider data
@{
*/

/*!
@brief
    Sets the user data of the plugin provider
    \bt_p{self_plugin_provider} to \bt_p{data}.

@param[in] self_plugin_provider
    Component instance.
@param[in] user_data
    New user data of \bt_p{self_plugin_provider}.

@bt_pre_not_null{self_plugin_provider}

@sa bt_self_plugin_provider_get_data() &mdash;
    Returns the user data of a plugin provider.
*/
extern void bt_self_plugin_provider_set_data(
		bt_self_plugin_provider *self_plugin_provider, void *user_data)
		__BT_NOEXCEPT;

/*!
@brief
    Returns the user data of the plugin provider
    \bt_p{self_plugin_provider}.

@param[in] self_plugin_provider
    Component instance.

@returns
    User data of \bt_p{self_plugin_provider}.

@bt_pre_not_null{self_plugin_provider}

@sa bt_self_plugin_provider_set_data() &mdash;
    Sets the user data of a plugin provider.
*/
extern void *bt_self_plugin_provider_get_data(
		const bt_self_plugin_provider *self_plugin_provider) __BT_NOEXCEPT;

/*! @} */

/*!
@name Plugin provider module
@{
*/

/*!
@brief
    Defines a plugin provider module.

In a plugin provider define C file, you must use this macro before you
use any other <code>BT_PLUGIN_PROVIDER*()</code> macro.
*/
#define BT_PLUGIN_PROVIDER_MODULE() \
	static struct __bt_plugin_provider_descriptor const * const __bt_plugin_provider_descriptor_dummy __BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRS = NULL; \
	_BT_HIDDEN extern struct __bt_plugin_provider_descriptor const *__BT_PLUGIN_PROVIDER_DESCRIPTOR_BEGIN_SYMBOL __BT_PLUGIN_PROVIDER_DESCRIPTOR_BEGIN_EXTRA; \
	_BT_HIDDEN extern struct __bt_plugin_provider_descriptor const *__BT_PLUGIN_PROVIDER_DESCRIPTOR_END_SYMBOL __BT_PLUGIN_PROVIDER_DESCRIPTOR_END_EXTRA; \
	static struct __bt_plugin_provider_descriptor_attribute const * const __bt_plugin_provider_descriptor_attribute_dummy __BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTES_ATTRS = NULL; \
	_BT_HIDDEN extern struct __bt_plugin_provider_descriptor_attribute const *__BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTES_BEGIN_SYMBOL __BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTES_BEGIN_EXTRA; \
	_BT_HIDDEN extern struct __bt_plugin_provider_descriptor_attribute const *__BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTES_END_SYMBOL __BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTES_END_EXTRA; \
	_BT_EXPORT struct __bt_plugin_provider_descriptor const * const *__bt_get_begin_section_plugin_provider_descriptors(void) \
	{ \
		return &__BT_PLUGIN_PROVIDER_DESCRIPTOR_BEGIN_SYMBOL; \
	} \
	_BT_EXPORT struct __bt_plugin_provider_descriptor const * const *__bt_get_end_section_plugin_provider_descriptors(void) \
	{ \
		return &__BT_PLUGIN_PROVIDER_DESCRIPTOR_END_SYMBOL; \
	} \
	_BT_EXPORT struct __bt_plugin_provider_descriptor_attribute const * const *__bt_get_begin_section_plugin_provider_descriptor_attributes(void) \
	{ \
		return &__BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTES_BEGIN_SYMBOL; \
	} \
	_BT_EXPORT struct __bt_plugin_provider_descriptor_attribute const * const *__bt_get_end_section_plugin_provider_descriptor_attributes(void) \
	{ \
		return &__BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTES_END_SYMBOL; \
	}

/*! @} */

/*!
@name Plugin provider definition
@{
*/

/*!
@brief
    Defines a plugin provider named \bt_p{_name} and having the ID \bt_p{_id}.

@param[in] _id
    @parblock
    C identifier.

    ID of the plugin provider, unique amongst all the plugin provider IDs of
    the same shared object.
    @endparblock
@param[in] _name
    @parblock
    <code>const char *</code>

    Plugin provider's name.
    @endparblock

@bt_pre_not_null{_name}
*/
#define BT_PLUGIN_PROVIDER_WITH_ID(_id, _name)							\
	struct __bt_plugin_provider_descriptor __bt_plugin_provider_descriptor_##_id = {	\
		.name = _name,									\
	};											\
	static struct __bt_plugin_provider_descriptor const * const __bt_plugin_provider_descriptor_##_id##_ptr __BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRS = &__bt_plugin_provider_descriptor_##_id

/*!
@brief
    Alias of BT_PLUGIN_PROVIDER_WITH_ID() with the \bt_p{_id} parameter
    set to <code>auto</code>.
*/
#define BT_PLUGIN_PROVIDER(_name) 		static BT_PLUGIN_PROVIDER_WITH_ID(auto, #_name)

/*! @} */

/*!
@name Plugin provider properties
@{
*/

/*!
@brief
    Sets the description of the plugin provider having the ID \bt_p{_id}
    to \bt_p{_description}.

See the \ref api-plugin-provider-prop-descr "description"
property.

@param[in] _id
    @parblock
    C identifier.

    ID of the plugin provider of which to set the description.
    @endparblock
@param[in] _description
    @parblock
    <code>const char *</code>

    Plugin provider's description.
    @endparblock

@bt_pre_not_null{_description}
*/
#define BT_PLUGIN_PROVIDER_DESCRIPTION_WITH_ID(_id, _description) \
	__BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE(description, BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE_TYPE_DESCRIPTION, _id, _description)

/*!
@brief
    Alias of BT_PLUGIN_PROVIDER_DESCRIPTION_WITH_ID() with the \bt_p{_id}
    parameter set to <code>auto</code>.
*/
#define BT_PLUGIN_PROVIDER_DESCRIPTION(_description) 	BT_PLUGIN_PROVIDER_DESCRIPTION_WITH_ID(auto, _description)

/*!
@brief
    Sets the name(s) of the author(s) of the plugin provider having the
    ID \bt_p{_id} to \bt_p{_author}.

See the \ref api-plugin-provider-prop-author "author name(s)"
property.

@param[in] _id
    @parblock
    C identifier.

    ID of the plugin provider of which to set the author(s).
    @endparblock
@param[in] _author
    @parblock
    <code>const char *</code>

    Plugin provider's author(s).
    @endparblock

@bt_pre_not_null{_author}
*/
#define BT_PLUGIN_PROVIDER_AUTHOR_WITH_ID(_id, _author) \
	__BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE(author, BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE_TYPE_AUTHOR, _id, _author)

/*!
@brief
    Alias of BT_PLUGIN_PROVIDER_AUTHOR_WITH_ID() with the \bt_p{_id}
    parameter set to <code>auto</code>.
*/
#define BT_PLUGIN_PROVIDER_AUTHOR(_author) 		BT_PLUGIN_PROVIDER_AUTHOR_WITH_ID(auto, _author)

/*!
@brief
    Sets the license (name or full) of the plugin provider having the ID
    \bt_p{_id} to \bt_p{_license}.

See the \ref api-plugin-provider-prop-license "license"
property.

@param[in] _id
    @parblock
    C identifier.

    ID of the plugin provider of which to set the license.
    @endparblock
@param[in] _license
    @parblock
    <code>const char *</code>

    Plugin provider's license.
    @endparblock

@bt_pre_not_null{_license}
*/
#define BT_PLUGIN_PROVIDER_LICENSE_WITH_ID(_id, _license) \
	__BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE(license, BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE_TYPE_LICENSE, _id, _license)

/*!
@brief
    Alias of BT_PLUGIN_PROVIDER_LICENSE_WITH_ID() with the \bt_p{_id}
    parameter set to <code>auto</code>.
*/
#define BT_PLUGIN_PROVIDER_LICENSE(_license) 		BT_PLUGIN_PROVIDER_LICENSE_WITH_ID(auto, _license)

/*!
@brief
    Sets the version of the plugin provider having the ID \bt_p{_id} to
    \bt_p{_version}.

See the \ref api-plugin-provider-prop-version "version" property.

@param[in] _id
    @parblock
    C identifier.

    ID of the plugin provider of which to set the version.
    @endparblock
@param[in] _major
    @parblock
    <code>unsigned int</code>

    Major version of the plugin provider.
    @endparblock
@param[in] _minor
    @parblock
    <code>unsigned int</code>

    Minor version of the plugin provider.
    @endparblock
@param[in] _patch
    @parblock
    <code>unsigned int</code>

    Patch version of the plugin provider.
    @endparblock
@param[in] _extra
    @parblock
    <code>const char *</code>

    Extra information of the version of the plugin provider.

    Can be \c NULL if the version of the plugin provider has no extra information.
    @endparblock
*/
#define BT_PLUGIN_PROVIDER_VERSION_WITH_ID(_id, _major, _minor, _patch, _extra) \
	__BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE(version, BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE_TYPE_VERSION, _id, __BT_PLUGIN_PROVIDER_VERSION_STRUCT_VALUE(_major, _minor, _patch, _extra))

/*!
@brief
    Alias of BT_PLUGIN_PROVIDER_VERSION_WITH_ID() with the \bt_p{_id}
    parameter set to <code>auto</code>.
*/
#define BT_PLUGIN_PROVIDER_VERSION(_major, _minor, _patch, _extra) BT_PLUGIN_PROVIDER_VERSION_WITH_ID(auto, _major, _minor, _patch, _extra)

/*! @} */

/*!
@name Plugin provider functions
@{
*/

/*!
@brief
    Status codes for #bt_plugin_provider_initialize_func.
*/
typedef enum bt_plugin_provider_initialize_func_status {
	/*!
	@brief
	    Success.
	*/
	BT_PLUGIN_PROVIDER_INITIALIZE_FUNC_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_PLUGIN_PROVIDER_INITIALIZE_FUNC_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    Error.
	*/
	BT_PLUGIN_PROVIDER_INITIALIZE_FUNC_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_plugin_provider_initialize_func_status;

/*!
@brief
    User plugin provider initialization function.

@param[in] self_plugin_provider
    @parblock
    Plugin provider instance.

    This parameter is a private view of the plugin provider object
    for this function.
    @endparblock

@retval #BT_PLUGIN_PROVIDER_INITIALIZE_FUNC_STATUS_OK
    Success.
@retval #BT_PLUGIN_PROVIDER_INITIALIZE_FUNC_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_PLUGIN_PROVIDER_INITIALIZE_FUNC_STATUS_ERROR
    Error.

@bt_pre_not_null{self_plugin_provider}
*/
typedef bt_plugin_provider_initialize_func_status (*bt_plugin_provider_initialize_func)(
		bt_self_plugin_provider *self_plugin_provider);

/*!
@brief
    Sets the initialization function of the plugin having the ID
    \bt_p{_id} to \bt_p{_func}.

@param[in] _id
    @parblock
    C identifier.

    ID of the plugin of which to set the initialization function.
    @endparblock
@param[in] _func
    @parblock
    #bt_plugin_provider_initialize_func

    Initialization function of the plugin provider.
    @endparblock

@bt_pre_not_null{_func}
*/
#define BT_PLUGIN_PROVIDER_INITIALIZE_FUNC_WITH_ID(_id, _func) \
	__BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE(init, BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE_TYPE_INIT, _id, _func)

/*!
@brief
    Alias of BT_PLUGIN_PROVIDER_INITIALIZE_FUNC_WITH_ID() with the \bt_p{_id}
    parameter set to <code>auto</code>.
*/
#define BT_PLUGIN_PROVIDER_INITIALIZE_FUNC(_func) 	BT_PLUGIN_PROVIDER_INITIALIZE_FUNC_WITH_ID(auto, _func)

/*!
@brief
    User plugin finalization function.

@param[in] self_plugin_provider
    @parblock
    Plugin provider instance.

    This parameter is a private view of the plugin provider object
    for this function.
    @endparblock

@bt_pre_not_null{self_plugin_provider}
*/
typedef void (*bt_plugin_provider_finalize_func)(
	bt_self_plugin_provider *self_plugin_provider);

/*!
@brief
    Sets the finalization function of the plugin provider having the ID
    \bt_p{_id} to \bt_p{_func}.

@param[in] _id
    @parblock
    C identifier.

    ID of the plugin provider of which to set the finalization function.
    @endparblock
@param[in] _func
    @parblock
    #bt_plugin_provider_finalize_func

    Finalization function of the plugin provider.
    @endparblock

@bt_pre_not_null{_func}
*/
#define BT_PLUGIN_PROVIDER_FINALIZE_FUNC_WITH_ID(_id, _func) \
	__BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE(exit, BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE_TYPE_EXIT, _id, _func)

/*!
@brief
    Alias of BT_PLUGIN_PROVIDER_FINALIZE_FUNC_WITH_ID() with the \bt_p{_id}
    parameter set to <code>auto</code>.
*/
#define BT_PLUGIN_PROVIDER_FINALIZE_FUNC(_func)	BT_PLUGIN_PROVIDER_FINALIZE_FUNC_WITH_ID(auto, _func)

/*!
@brief
    Returns whether or not a
    #bt_plugin_provider_create_all_from_file_func function called with
    \bt_p{options} must ignore a plugin loading error.

@param[in] options
    Options to get the "fail on load error" option value from.

@retval #BT_TRUE
    A #bt_plugin_provider_create_all_from_file_func function called
    with \bt_p{options} must return
    #BT_PLUGIN_PROVIDER_CREATE_ALL_FROM_FILE_FUNC_STATUS_ERROR if
    there's any plugin loading error.
@retval #BT_FALSE
    A #bt_plugin_provider_create_all_from_file_func function called
    with \bt_p{options} must ignore any plugin loading error.

@bt_pre_not_null{options}
*/
extern bt_bool
bt_plugin_provider_create_all_from_file_options_get_fail_on_load_error(
		const bt_plugin_provider_create_all_from_file_options *options)
		__BT_NOEXCEPT;

/*!
@brief
    Status codes for #bt_plugin_provider_create_all_from_file_func.
*/
typedef enum bt_plugin_provider_create_all_from_file_func_status {
	/*!
	@brief
	    Success.
	*/
	BT_PLUGIN_PROVIDER_CREATE_ALL_FROM_FILE_FUNC_STATUS_OK			= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    No plugins found.
	*/
	BT_PLUGIN_PROVIDER_CREATE_ALL_FROM_FILE_FUNC_STATUS_NOT_FOUND		= __BT_FUNC_STATUS_NOT_FOUND,

	/*!
	@brief
	    Out of memory.
	*/
	BT_PLUGIN_PROVIDER_CREATE_ALL_FROM_FILE_FUNC_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    Error.
	*/
	BT_PLUGIN_PROVIDER_CREATE_ALL_FROM_FILE_FUNC_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_plugin_provider_create_all_from_file_func_status;

/*!
@brief
    Finds and loads all the plugins from the file with path \bt_p{path},
    adding them to \bt_p{plugin_set}.

@param[in] self_plugin_provider
    @parblock
    Plugin provider instance.

    This parameter is a private view of the plugin provider object
    for this function.
    @endparblock
@param[in] path
    Path of the file in which to find and load \em all the plugins.
@param[in] options
    @parblock
    Options to consider during a call.

    Depending on the result of
    bt_plugin_provider_create_all_from_file_options_get_fail_on_load_error()
    called with \bt_p{options}:

    <dl>
      <dt>#BT_TRUE</dt>
      <dd>
        Return #BT_PLUGIN_PROVIDER_CREATE_ALL_FROM_FILE_FUNC_STATUS_ERROR
        if there's any plugin loading error.
      </dd>
      <dt>#BT_FALSE</dt>
      <dd>Ignore any plugin loading error.</dd>
    </dl>
    @endparblock
@param[in] plugin_set
    Plugin set to which to add the plugins found in the file with path
    \bt_p{path}.

@retval #BT_PLUGIN_PROVIDER_CREATE_ALL_FROM_FILE_FUNC_STATUS_OK
    Success.
@retval #BT_PLUGIN_PROVIDER_CREATE_ALL_FROM_FILE_FUNC_STATUS_NOT_FOUND
    No plugins found.
@retval #BT_PLUGIN_PROVIDER_CREATE_ALL_FROM_FILE_FUNC_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_PLUGIN_PROVIDER_CREATE_ALL_FROM_FILE_FUNC_STATUS_ERROR
    Error.

@bt_pre_not_null{self_plugin_provider}
@bt_pre_not_null{path}
@pre
    \bt_p{path} is the path of a regular file.
@bt_pre_not_null{options}
@bt_pre_not_null{plugin_set}
@pre
    \bt_p{plugin_set} is empty.
@post
    If this function returns
    #BT_PLUGIN_PROVIDER_CREATE_ALL_FROM_FILE_FUNC_STATUS_OK, then
    \bt_p{plugin_set} contains at least one plugin.
*/
typedef bt_plugin_provider_create_all_from_file_func_status
(*bt_plugin_provider_create_all_from_file_func)(
		bt_self_plugin_provider *self_plugin_provider,
		const char *path,
		const bt_plugin_provider_create_all_from_file_options *options,
		bt_plugin_set *plugin_set);

/*!
@brief
    Sets the "create all from file" function of the plugin provider
    having the ID \bt_p{_id} to \bt_p{_func}.

@param[in] _id
    @parblock
    C identifier.

    ID of the plugin provider of which to set the create all from file
    function.
    @endparblock
@param[in] _func
    @parblock
    #bt_plugin_provider_create_all_from_file_func

    "Create all from file" function of the plugin provider.
    @endparblock

@bt_pre_not_null{_func}
*/
#define BT_PLUGIN_PROVIDER_CREATE_ALL_FROM_FILE_FUNC_WITH_ID(_id, _func) \
	__BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE(create_all_from_file, BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE_TYPE_CREATE_ALL_FROM_FILE, _id, _func)

/*!
@brief
    Alias of BT_PLUGIN_PROVIDER_CREATE_ALL_FROM_FILE_FUNC_WITH_ID()
    with the \bt_p{_id} parameter set to <code>auto</code>.
*/
#define BT_PLUGIN_PROVIDER_CREATE_ALL_FROM_FILE_FUNC(_func)	BT_PLUGIN_PROVIDER_CREATE_ALL_FROM_FILE_FUNC_WITH_ID(auto, _func)

/*!
@brief
    Returns whether or not a
    #bt_plugin_provider_create_all_from_static_func function called with
    \bt_p{options} must ignore a plugin loading error.

@param[in] options
    Options to get the "fail on load error" option value from.

@retval #BT_TRUE
    A #bt_plugin_provider_create_all_from_static_func function called
    with \bt_p{options} must return
    #BT_PLUGIN_PROVIDER_CREATE_ALL_FROM_STATIC_FUNC_STATUS_ERROR if
    there's any plugin loading error.
@retval #BT_FALSE
    A #bt_plugin_provider_create_all_from_static_func function called
    with \bt_p{options} must ignore any plugin loading error.

@bt_pre_not_null{options}
*/
extern bt_bool
bt_plugin_provider_create_all_from_static_options_get_fail_on_load_error(
		const bt_plugin_provider_create_all_from_static_options *options)
		__BT_NOEXCEPT;

/*!
@brief
    Status codes for #bt_plugin_provider_create_all_from_static_func.
*/
typedef enum bt_plugin_provider_create_all_from_static_func_status {
	/*!
	@brief
	    Success.
	*/
	BT_PLUGIN_PROVIDER_CREATE_ALL_FROM_STATIC_FUNC_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    No plugins found.
	*/
	BT_PLUGIN_PROVIDER_CREATE_ALL_FROM_STATIC_FUNC_STATUS_NOT_FOUND		= __BT_FUNC_STATUS_NOT_FOUND,

	/*!
	@brief
	    Out of memory.
	*/
	BT_PLUGIN_PROVIDER_CREATE_ALL_FROM_STATIC_FUNC_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    Error.
	*/
	BT_PLUGIN_PROVIDER_CREATE_ALL_FROM_STATIC_FUNC_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_plugin_provider_create_all_from_static_func_status;

/*!
@brief
    Finds and loads all the static plugins, adding them to
    \bt_p{plugin_set}.

A static plugin is built directly into the application or library
instead of being a separate shared object file.

@param[in] self_plugin_provider
    @parblock
    Plugin provider instance.

    This parameter is a private view of the plugin provider object
    for this function.
    @endparblock
@param[in] options
    @parblock
    Options to consider during a call.

    Depending on the result of
    bt_plugin_provider_create_all_from_static_options_get_fail_on_load_error()
    called with \bt_p{options}:

    <dl>
      <dt>#BT_TRUE</dt>
      <dd>
        Return #BT_PLUGIN_PROVIDER_CREATE_ALL_FROM_STATIC_FUNC_STATUS_ERROR
        if there's any plugin loading error.
      </dd>
      <dt>#BT_FALSE</dt>
      <dd>Ignore any plugin loading error.</dd>
    </dl>
    @endparblock
@param[in] plugin_set
    Plugin set to which to add the static plugins.

@retval #BT_PLUGIN_PROVIDER_CREATE_ALL_FROM_STATIC_FUNC_STATUS_OK
    Success.
@retval #BT_PLUGIN_PROVIDER_CREATE_ALL_FROM_STATIC_FUNC_STATUS_NOT_FOUND
    No plugins found.
@retval #BT_PLUGIN_PROVIDER_CREATE_ALL_FROM_STATIC_FUNC_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_PLUGIN_PROVIDER_CREATE_ALL_FROM_STATIC_FUNC_STATUS_ERROR
    Error.

@bt_pre_not_null{self_plugin_provider}
@bt_pre_not_null{options}
@bt_pre_not_null{plugin_set}
@pre
    \bt_p{plugin_set} is empty.
@post
    If this function returns
    #BT_PLUGIN_PROVIDER_CREATE_ALL_FROM_STATIC_FUNC_STATUS_OK, then
    \bt_p{plugin_set} contains at least one plugin.
*/
typedef bt_plugin_provider_create_all_from_static_func_status
(*bt_plugin_provider_create_all_from_static_func)(
		bt_self_plugin_provider *self_plugin_provider,
		const bt_plugin_provider_create_all_from_static_options *options,
		bt_plugin_set *plugin_set);

/*!
@brief
    Returns the logging level to use within a plugin provider method.

A plugin provider method should drive its own logging statements
using this logging level.

The effective logging level can change between calls, therefore a method
must sample it with this function at its entry point and use that
sampled value for the duration of the call rather than caching it
across calls.

@param[in] self_plugin_provider
    Plugin provider of which to get the logging level.

@returns
    Logging level to use.

@bt_pre_not_null{self_plugin_provider}
*/
extern int bt_self_plugin_provider_get_logging_level(
		const bt_self_plugin_provider *self_plugin_provider) __BT_NOEXCEPT;

/*!
@brief
    Sets the create all from static function of the plugin provider having
    the ID \bt_p{_id} to \bt_p{_func}.

@param[in] _id
    @parblock
    C identifier.

    ID of the plugin provider of which to set the create all from static
    function.
    @endparblock
@param[in] _func
    @parblock
    #bt_plugin_provider_create_all_from_static_func

    "Create all from static" function of the plugin provider.
    @endparblock

@bt_pre_not_null{_func}
*/
#define BT_PLUGIN_PROVIDER_CREATE_ALL_FROM_STATIC_FUNC_WITH_ID(_id, _func) \
	__BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE(create_all_from_static, BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE_TYPE_CREATE_ALL_FROM_STATIC, _id, _func)

/*!
@brief
    Alias of BT_PLUGIN_PROVIDER_CREATE_ALL_FROM_STATIC_FUNC_WITH_ID()
    with the \bt_p{_id} parameter set to <code>auto</code>.
*/
#define BT_PLUGIN_PROVIDER_CREATE_ALL_FROM_STATIC_FUNC(_func)	BT_PLUGIN_PROVIDER_CREATE_ALL_FROM_STATIC_FUNC_WITH_ID(auto, _func)

/*! @} */

/*! @} */

/* Plugin provider descriptor: describes a single plugin provider (internal use) */
struct __bt_plugin_provider_descriptor {
	/* Plugin provider's name */
	const char *name;
} __attribute__((packed));

/* Type of a plugin provider attribute (internal use) */
enum __bt_plugin_provider_descriptor_attribute_type {
	BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE_TYPE_INIT			= 0,
	BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE_TYPE_EXIT			= 1,
	BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE_TYPE_CREATE_ALL_FROM_FILE	= 2,
	BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE_TYPE_CREATE_ALL_FROM_STATIC	= 3,
	BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE_TYPE_AUTHOR			= 4,
	BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE_TYPE_LICENSE			= 5,
	BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE_TYPE_DESCRIPTION		= 6,
	BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE_TYPE_VERSION			= 7,
};

/* Plugin provider attribute (internal use) */
struct __bt_plugin_provider_descriptor_attribute {
	/* Plugin provider descriptor to which to associate this attribute */
	const struct __bt_plugin_provider_descriptor *plugin_provider_descriptor;

	/* Name of the attribute's type for debug purposes */
	const char *type_name;

	/* Attribute's type */
	enum __bt_plugin_provider_descriptor_attribute_type type;

	/* Attribute's value (depends on attribute's type) */
	union {
		/* BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE_TYPE_INIT */
		bt_plugin_provider_initialize_func init;

		/* BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE_TYPE_EXIT */
		bt_plugin_provider_finalize_func exit;

		/* BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE_TYPE_CREATE_ALL_FROM_FILE */
		bt_plugin_provider_create_all_from_file_func create_all_from_file;

		/* BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE_TYPE_CREATE_ALL_FROM_STATIC */
		bt_plugin_provider_create_all_from_static_func create_all_from_static;

		/* BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE_TYPE_AUTHOR */
		const char *author;

		/* BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE_TYPE_LICENSE */
		const char *license;

		/* BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE_TYPE_DESCRIPTION */
		const char *description;

		/* BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE_TYPE_VERSION */
		struct __bt_object_descriptor_version version;
	} value;
} __attribute__((packed));

struct __bt_plugin_provider_descriptor const * const *__bt_get_begin_section_plugin_provider_descriptors(void);
struct __bt_plugin_provider_descriptor const * const *__bt_get_end_section_plugin_provider_descriptors(void);
struct __bt_plugin_provider_descriptor_attribute const * const *__bt_get_begin_section_plugin_provider_descriptor_attributes(void);
struct __bt_plugin_provider_descriptor_attribute const * const *__bt_get_end_section_plugin_provider_descriptor_attributes(void);

/*
 * Defines a plugin provider attribute (generic, internal use).
 *
 * _attr_name: Name of the attribute (C identifier).
 * _attr_type: Type of the attribute (enum __bt_plugin_provider_descriptor_attribute_type).
 * _id:        Plugin provider descriptor ID (C identifier).
 * _x:         Value.
 */
#define __BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTE(_attr_name, _attr_type, _id, _x) \
	static struct __bt_plugin_provider_descriptor_attribute __bt_plugin_provider_descriptor_attribute_##_id##_##_attr_name = { \
		.plugin_provider_descriptor = &__bt_plugin_provider_descriptor_##_id,	\
		.type_name = #_attr_name,				\
		.type = _attr_type,					\
		.value = { ._attr_name = _x },				\
	};								\
	static struct __bt_plugin_provider_descriptor_attribute const * const __bt_plugin_provider_descriptor_attribute_##_id##_##_attr_name##_ptr __BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTES_ATTRS = &__bt_plugin_provider_descriptor_attribute_##_id##_##_attr_name

#define __BT_PLUGIN_PROVIDER_VERSION_STRUCT_VALUE(_major, _minor, _patch, _extra) \
	{.major = _major, .minor = _minor, .patch = _patch, .extra = _extra,}

/*
 * Clang supports the no_sanitize variable attribute on global variables.
 * GCC only supports the no_sanitize_address function attribute, which is
 * not what we need. This is fine because, as far as we have seen, gcc
 * does not insert red zones around global variables.
 */
#if defined(__clang__)
# if __has_feature(address_sanitizer)
#  define __bt_plugin_provider_variable_attribute_no_sanitize_address \
	__attribute__((no_sanitize("address")))
# else
#  define __bt_plugin_provider_variable_attribute_no_sanitize_address
# endif
#else
#  define __bt_plugin_provider_variable_attribute_no_sanitize_address
#endif

/*
 * Variable attributes for a plugin provider descriptor pointer to be
 * added to the plugin provider descriptor section (internal use).
 */
#ifdef __APPLE__
#define __BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRS \
	__attribute__((section("__DATA,btpp_desc"), used)) \
	__bt_plugin_provider_variable_attribute_no_sanitize_address

#define __BT_PLUGIN_PROVIDER_DESCRIPTOR_BEGIN_SYMBOL \
	__start___bt_plugin_provider_descriptors

#define __BT_PLUGIN_PROVIDER_DESCRIPTOR_END_SYMBOL \
	__stop___bt_plugin_provider_descriptors

#define __BT_PLUGIN_PROVIDER_DESCRIPTOR_BEGIN_EXTRA \
	__asm("section$start$__DATA$btpp_desc")

#define __BT_PLUGIN_PROVIDER_DESCRIPTOR_END_EXTRA \
	__asm("section$end$__DATA$btpp_desc")

#else

#define __BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRS \
	__attribute__((section("__bt_plugin_provider_descriptors"), used)) \
	__bt_plugin_provider_variable_attribute_no_sanitize_address

#define __BT_PLUGIN_PROVIDER_DESCRIPTOR_BEGIN_SYMBOL \
	__start___bt_plugin_provider_descriptors

#define __BT_PLUGIN_PROVIDER_DESCRIPTOR_END_SYMBOL \
	__stop___bt_plugin_provider_descriptors

#define __BT_PLUGIN_PROVIDER_DESCRIPTOR_BEGIN_EXTRA

#define __BT_PLUGIN_PROVIDER_DESCRIPTOR_END_EXTRA
#endif

/*
 * Variable attributes for a plugin provider attribute pointer to be
 * added to the plugin provider attribute section (internal use).
 */
#ifdef __APPLE__
#define __BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTES_ATTRS \
	__attribute__((section("__DATA,btpp_desc_att"), used)) \
	__bt_plugin_provider_variable_attribute_no_sanitize_address

#define __BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTES_BEGIN_SYMBOL \
	__start___bt_plugin_provider_descriptor_attributes

#define __BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTES_END_SYMBOL \
	__stop___bt_plugin_provider_descriptor_attributes

#define __BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTES_BEGIN_EXTRA \
	__asm("section$start$__DATA$btpp_desc_att")

#define __BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTES_END_EXTRA \
	__asm("section$end$__DATA$btpp_desc_att")

#else

#define __BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTES_ATTRS \
	__attribute__((section("__bt_plugin_provider_descriptor_attributes"), used)) \
	__bt_plugin_provider_variable_attribute_no_sanitize_address

#define __BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTES_BEGIN_SYMBOL \
	__start___bt_plugin_provider_descriptor_attributes

#define __BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTES_END_SYMBOL \
	__stop___bt_plugin_provider_descriptor_attributes

#define __BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTES_BEGIN_EXTRA

#define __BT_PLUGIN_PROVIDER_DESCRIPTOR_ATTRIBUTES_END_EXTRA
#endif

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_PLUGIN_PLUGIN_PROVIDER_DEV_H */
