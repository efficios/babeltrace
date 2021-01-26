#ifndef BABELTRACE2_PLUGIN_PLUGIN_DEV_H
#define BABELTRACE2_PLUGIN_PLUGIN_DEV_H

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

#include <babeltrace2/graph/component-class-dev.h>
#include <babeltrace2/graph/message-iterator-class.h>
#include <babeltrace2/types.h>

/*
 * _BT_HIDDEN: set the hidden attribute for internal functions
 * On Windows, symbols are local unless explicitly exported,
 * see https://gcc.gnu.org/wiki/Visibility
 */
#if defined(_WIN32) || defined(__CYGWIN__)
#define _BT_HIDDEN
#else
#define _BT_HIDDEN __attribute__((visibility("hidden")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*!
@defgroup api-plugin-dev Plugin development

@brief
    Shared object plugin development.

This module offers macros to create a \bt_name shared object plugin.

Behind the scenes, the <code>BT_PLUGIN_*()</code> macros of this module
create and fill global tables which are located in sections of the
shared object with specific names. The \ref api-plugin functions can
load the resulting shared object file and create corresponding
\bt_plugin objects.

See \ref guide-comp-link-plugin-so.

<h1>Plugin definition C file structure</h1>

The structure of a \bt_name plugin definition C file is as such:

<ol>
  <li>
    Start with

    @code
    BT_PLUGIN_MODULE();
    @endcode
  </li>

  <li>
    Define a \bt_name plugin with BT_PLUGIN() if the plugin's name is a
    valid C identifier, or with BT_PLUGIN_WITH_ID() otherwise.

    See \ref api-plugin-dev-custom-plugin-id "Custom plugin ID" to
    learn more about plugin IDs.

    @note
        When you use BT_PLUGIN(), the plugin's ID is <code>auto</code>.
  </li>

  <li>
    \bt_dt_opt Use any of the following macros (or their
    <code>*_WITH_ID()</code> counterpart) \em once to set the properties
    of the plugin:

    - BT_PLUGIN_AUTHOR()
    - BT_PLUGIN_DESCRIPTION()
    - BT_PLUGIN_LICENSE()
    - BT_PLUGIN_VERSION()
  </li>

  <li>
    \bt_dt_opt Use any of the following macros (or their
    <code>*_WITH_ID()</code> counterpart) \em once to set the
    initialization and finalization functions of the plugin:

    - BT_PLUGIN_INITIALIZE_FUNC()
    - BT_PLUGIN_FINALIZE_FUNC()

    A plugin's initialization function is executed when the shared
    object is loaded (see \ref api-plugin).

    A plugin's finalization function is executed when the \bt_plugin
    object is destroyed, if the initialization function (if any)
    succeeded.
  </li>

  <li>
    Use any of the following macros (or their <code>*_WITH_ID()</code>
    counterpart) to add a component class to the plugin:

    - BT_PLUGIN_SOURCE_COMPONENT_CLASS()
    - BT_PLUGIN_FILTER_COMPONENT_CLASS()
    - BT_PLUGIN_SINK_COMPONENT_CLASS()
  </li>

  <li>
    \bt_dt_opt Depending on the type of the component class of step 5,
    use any of the following macros (or their <code>*_WITH_ID()</code>
    counterpart)
    \em once to set its properties:

    <dl>
      <dt>\bt_c_src_comp_cls</dt>
      <dd>
        - BT_PLUGIN_SOURCE_COMPONENT_CLASS_DESCRIPTION()
        - BT_PLUGIN_SOURCE_COMPONENT_CLASS_HELP()
      </dd>

      <dt>\bt_c_flt_comp_cls</dt>
      <dd>
        - BT_PLUGIN_FILTER_COMPONENT_CLASS_DESCRIPTION()
        - BT_PLUGIN_FILTER_COMPONENT_CLASS_HELP()
      </dd>

      <dt>\bt_c_sink_comp_cls</dt>
      <dd>
        - BT_PLUGIN_SINK_COMPONENT_CLASS_DESCRIPTION()
        - BT_PLUGIN_SINK_COMPONENT_CLASS_HELP()
      </dd>
    </dl>
  </li>

  <li>
    \bt_dt_opt Depending on the type of the component class of step 5,
    use any of the following macros (or their <code>*_WITH_ID()</code>
    counterpart) to set its optional methods:

    <dl>
      <dt>Source component class</dt>
      <dd>
        - BT_PLUGIN_SOURCE_COMPONENT_CLASS_FINALIZE_METHOD()
        - BT_PLUGIN_SOURCE_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD()
        - BT_PLUGIN_SOURCE_COMPONENT_CLASS_INITIALIZE_METHOD()
        - BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_FINALIZE_METHOD()
        - BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD()
        - BT_PLUGIN_SOURCE_COMPONENT_CLASS_OUTPUT_PORT_CONNECTED_METHOD()
        - BT_PLUGIN_SOURCE_COMPONENT_CLASS_QUERY_METHOD()
      </dd>

      <dt>Filter component class</dt>
      <dd>
        - BT_PLUGIN_FILTER_COMPONENT_CLASS_FINALIZE_METHOD()
        - BT_PLUGIN_FILTER_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD()
        - BT_PLUGIN_FILTER_COMPONENT_CLASS_INITIALIZE_METHOD()
        - BT_PLUGIN_FILTER_COMPONENT_CLASS_INPUT_PORT_CONNECTED_METHOD()
        - BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_FINALIZE_METHOD()
        - BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD()
        - BT_PLUGIN_FILTER_COMPONENT_CLASS_OUTPUT_PORT_CONNECTED_METHOD()
        - BT_PLUGIN_FILTER_COMPONENT_CLASS_QUERY_METHOD()
      </dd>

      <dt>Sink component class</dt>
      <dd>
        - BT_PLUGIN_SINK_COMPONENT_CLASS_FINALIZE_METHOD()
        - BT_PLUGIN_SINK_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD()
        - BT_PLUGIN_SINK_COMPONENT_CLASS_GRAPH_IS_CONFIGURED_METHOD()
        - BT_PLUGIN_SINK_COMPONENT_CLASS_INITIALIZE_METHOD()
        - BT_PLUGIN_SINK_COMPONENT_CLASS_INPUT_PORT_CONNECTED_METHOD()
        - BT_PLUGIN_SINK_COMPONENT_CLASS_QUERY_METHOD()
      </dd>
    </dl>
  </li>
</ol>

You can repeat steps 5 to 7 to add more than one component class to a
given plugin.

See \ref example-simple-plugin-def-file for a concrete example of how
to use the macros of this module.

<h1>\anchor api-plugin-dev-custom-plugin-id Custom plugin ID</h1>

The BT_PLUGIN() macro defines a plugin with a specific name and the
ID <code>auto</code>.

All the <code>BT_PLUGIN_*()</code> macros which do not end with
<code>_WITH_ID</code> refer to the <code>auto</code> plugin.

There are two situations which demand that you use a custom plugin ID:

- You want more than one plugin contained in your shared object file.

  Although the \bt_name project does not recommend this, it is possible.
  This is why bt_plugin_find_all_from_file() returns a \bt_plugin_set
  instead of a single \bt_plugin.

  In this case, each plugin of the shared object needs its own, unique
  ID.

- You want to give the plugin a name which is not a valid C identifier.

  The BT_PLUGIN() macro accepts a C identifier as the plugin name, while
  the BT_PLUGIN_WITH_ID() accepts a C identifier for the ID and a C
  string for the name.

To define a plugin with a specific ID, use BT_PLUGIN_WITH_ID(), for
example:

@code
BT_PLUGIN_WITH_ID(my_plugin_id, "my-plugin-name");
@endcode

Then, use the <code>BT_PLUGIN_*_WITH_ID()</code> macros to refer to
this specific plugin, for example:

@code
BT_PLUGIN_AUTHOR_WITH_ID(my_plugin_id, "Patrick Bouchard");
@endcode

@note
    @parblock
    You can still use the <code>auto</code> ID with BT_PLUGIN_WITH_ID()
    to use the simpler macros afterwards while still giving the plugin a
    name which is not a valid C identifier, for example:

    @code
    BT_PLUGIN_WITH_ID(auto, "my-plugin-name");
    BT_PLUGIN_AUTHOR("Patrick Bouchard");
    @endcode
    @endparblock

<h1>Custom component class ID</h1>

The BT_PLUGIN_SOURCE_COMPONENT_CLASS(),
BT_PLUGIN_FILTER_COMPONENT_CLASS(), and
BT_PLUGIN_SINK_COMPONENT_CLASS() add a component class with a specific
name to the plugin having the ID <code>auto</code>.

The name you pass to those macros must be a valid C identifier and it
also serves as the component class's ID within the <code>auto</code>
plugin.

There are two situations which demand that you use a custom component
class ID:

- You want to add the component class to a specific plugin (other than
  <code>auto</code>, if you have more than one).

- You want to give the component class a name which is not a valid C
  identifier.

  The <code>BT_PLUGIN_*_COMPONENT_CLASS_WITH_ID()</code> macros accept a
  C identifier for the component class ID and a string for its name.

For a given plugin and for a given component class type, all component
class IDs must be unique.

To add a component class having a specific ID to a plugin,
use the BT_PLUGIN_SOURCE_COMPONENT_CLASS_WITH_ID(),
BT_PLUGIN_FILTER_COMPONENT_CLASS_WITH_ID(), and
BT_PLUGIN_SINK_COMPONENT_CLASS_WITH_ID() macros, for example:

@code
BT_PLUGIN_SOURCE_COMPONENT_CLASS_WITH_ID(my_plugin_id, my_comp_class_id,
    "my-source", my_source_iter_next);
@endcode

@note
    @parblock
    The <code>BT_PLUGIN_*_COMPONENT_CLASS_WITH_ID()</code> macros
    specify the ID of the plugin to which to add the component class.

    If you use the BT_PLUGIN() macro to define your plugin, then its
    ID is <code>auto</code>:

    @code
    BT_PLUGIN_SOURCE_COMPONENT_CLASS_WITH_ID(auto, my_comp_class_id,
        "my-source", my_source_iter_next);
    @endcode
    @endparblock

Then, use the <code>BT_PLUGIN_*_COMPONENT_CLASS_*_WITH_ID()</code>
macros to refer to this specific component class, for example:

@code
BT_PLUGIN_SOURCE_COMPONENT_CLASS_FINALIZE_METHOD_WITH_ID(my_plugin_id,
    my_comp_class_id, my_source_finalize);
@endcode
*/

/*! @{ */

/*!
@name Type
@{

@typedef struct bt_self_plugin bt_self_plugin;

@brief
    Self plugin.

@}
*/

/*!
@name Plugin module
@{
*/

/*!
@brief
    Defines a plugin module.

In a plugin define C file, you must use this macro before you use any
other <code>BT_PLUGIN*()</code> macro.
*/
#define BT_PLUGIN_MODULE() \
	static struct __bt_plugin_descriptor const * const __bt_plugin_descriptor_dummy __BT_PLUGIN_DESCRIPTOR_ATTRS = NULL; \
	_BT_HIDDEN extern struct __bt_plugin_descriptor const *__BT_PLUGIN_DESCRIPTOR_BEGIN_SYMBOL __BT_PLUGIN_DESCRIPTOR_BEGIN_EXTRA; \
	_BT_HIDDEN extern struct __bt_plugin_descriptor const *__BT_PLUGIN_DESCRIPTOR_END_SYMBOL __BT_PLUGIN_DESCRIPTOR_END_EXTRA; \
	\
	static struct __bt_plugin_descriptor_attribute const * const __bt_plugin_descriptor_attribute_dummy __BT_PLUGIN_DESCRIPTOR_ATTRIBUTES_ATTRS = NULL; \
	_BT_HIDDEN extern struct __bt_plugin_descriptor_attribute const *__BT_PLUGIN_DESCRIPTOR_ATTRIBUTES_BEGIN_SYMBOL __BT_PLUGIN_DESCRIPTOR_ATTRIBUTES_BEGIN_EXTRA; \
	_BT_HIDDEN extern struct __bt_plugin_descriptor_attribute const *__BT_PLUGIN_DESCRIPTOR_ATTRIBUTES_END_SYMBOL __BT_PLUGIN_DESCRIPTOR_ATTRIBUTES_END_EXTRA; \
	\
	static struct __bt_plugin_component_class_descriptor const * const __bt_plugin_component_class_descriptor_dummy __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRS = NULL; \
	_BT_HIDDEN extern struct __bt_plugin_component_class_descriptor const *__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_BEGIN_SYMBOL __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_BEGIN_EXTRA; \
	_BT_HIDDEN extern struct __bt_plugin_component_class_descriptor const *__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_END_SYMBOL __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_END_EXTRA; \
	\
	static struct __bt_plugin_component_class_descriptor_attribute const * const __bt_plugin_component_class_descriptor_attribute_dummy __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTES_ATTRS = NULL; \
	_BT_HIDDEN extern struct __bt_plugin_component_class_descriptor_attribute const *__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTES_BEGIN_SYMBOL __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTES_BEGIN_EXTRA; \
	_BT_HIDDEN extern struct __bt_plugin_component_class_descriptor_attribute const *__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTES_END_SYMBOL __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTES_END_EXTRA; \
	\
	struct __bt_plugin_descriptor const * const *__bt_get_begin_section_plugin_descriptors(void) \
	{ \
		return &__BT_PLUGIN_DESCRIPTOR_BEGIN_SYMBOL; \
	} \
	struct __bt_plugin_descriptor const * const *__bt_get_end_section_plugin_descriptors(void) \
	{ \
		return &__BT_PLUGIN_DESCRIPTOR_END_SYMBOL; \
	} \
	struct __bt_plugin_descriptor_attribute const * const *__bt_get_begin_section_plugin_descriptor_attributes(void) \
	{ \
		return &__BT_PLUGIN_DESCRIPTOR_ATTRIBUTES_BEGIN_SYMBOL; \
	} \
	struct __bt_plugin_descriptor_attribute const * const *__bt_get_end_section_plugin_descriptor_attributes(void) \
	{ \
		return &__BT_PLUGIN_DESCRIPTOR_ATTRIBUTES_END_SYMBOL; \
	} \
	struct __bt_plugin_component_class_descriptor const * const *__bt_get_begin_section_component_class_descriptors(void) \
	{ \
		return &__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_BEGIN_SYMBOL; \
	} \
	struct __bt_plugin_component_class_descriptor const * const *__bt_get_end_section_component_class_descriptors(void) \
	{ \
		return &__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_END_SYMBOL; \
	} \
	struct __bt_plugin_component_class_descriptor_attribute const * const *__bt_get_begin_section_component_class_descriptor_attributes(void) \
	{ \
		return &__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTES_BEGIN_SYMBOL; \
	} \
	struct __bt_plugin_component_class_descriptor_attribute const * const *__bt_get_end_section_component_class_descriptor_attributes(void) \
	{ \
		return &__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTES_END_SYMBOL; \
	}

/*! @} */

/*!
@name Plugin definition
@{
*/

/*!
@brief
    Defines a plugin named \bt_p{_name} and having the ID \bt_p{_id}.

@param[in] _id
    @parblock
    C identifier.

    Plugin's ID, unique amongst all the plugin IDs of the same shared
    object.
    @endparblock
@param[in] _name
    @parblock
    <code>const char *</code>

    Plugin's name.
    @endparblock

@bt_pre_not_null{_name}
*/
#define BT_PLUGIN_WITH_ID(_id, _name)					\
	struct __bt_plugin_descriptor __bt_plugin_descriptor_##_id = {	\
		.name = _name,						\
	};								\
	static struct __bt_plugin_descriptor const * const __bt_plugin_descriptor_##_id##_ptr __BT_PLUGIN_DESCRIPTOR_ATTRS = &__bt_plugin_descriptor_##_id

/*!
@brief
    Alias of BT_PLUGIN_WITH_ID() with the \bt_p{_id} parameter set to
    <code>auto</code>.
*/
#define BT_PLUGIN(_name) 		static BT_PLUGIN_WITH_ID(auto, #_name)

/*! @} */

/*!
@name Plugin properties
@{
*/

/*!
@brief
    Sets the description of the plugin having the ID \bt_p{_id} to
    \bt_p{_description}.

See the \ref api-comp-cls-prop-descr "description" property.

@param[in] _id
    @parblock
    C identifier.

    ID of the plugin of which to set the description.
    @endparblock
@param[in] _description
    @parblock
    <code>const char *</code>

    Plugin's description.
    @endparblock

@bt_pre_not_null{_description}
*/
#define BT_PLUGIN_DESCRIPTION_WITH_ID(_id, _description) \
	__BT_PLUGIN_DESCRIPTOR_ATTRIBUTE(description, BT_PLUGIN_DESCRIPTOR_ATTRIBUTE_TYPE_DESCRIPTION, _id, _description)

/*!
@brief
    Alias of BT_PLUGIN_DESCRIPTION_WITH_ID() with the \bt_p{_id}
    parameter set to <code>auto</code>.
*/
#define BT_PLUGIN_DESCRIPTION(_description) 	BT_PLUGIN_DESCRIPTION_WITH_ID(auto, _description)


/*!
@brief
    Sets the name(s) of the author(s) of the plugin having the ID
    \bt_p{_id} to \bt_p{_author}.

See the \ref api-plugin-prop-author "author name(s)" property.

@param[in] _id
    @parblock
    C identifier.

    ID of the plugin of which to set the author(s).
    @endparblock
@param[in] _author
    @parblock
    <code>const char *</code>

    Plugin's author(s).
    @endparblock

@bt_pre_not_null{_author}
*/
#define BT_PLUGIN_AUTHOR_WITH_ID(_id, _author) \
	__BT_PLUGIN_DESCRIPTOR_ATTRIBUTE(author, BT_PLUGIN_DESCRIPTOR_ATTRIBUTE_TYPE_AUTHOR, _id, _author)

/*!
@brief
    Alias of BT_PLUGIN_AUTHOR_WITH_ID() with the \bt_p{_id}
    parameter set to <code>auto</code>.
*/
#define BT_PLUGIN_AUTHOR(_author) 		BT_PLUGIN_AUTHOR_WITH_ID(auto, _author)

/*!
@brief
    Sets the license (name or full) of the plugin having the ID
    \bt_p{_id} to \bt_p{_license}.

See the \ref api-plugin-prop-license "license" property.

@param[in] _id
    @parblock
    C identifier.

    ID of the plugin of which to set the license.
    @endparblock
@param[in] _license
    @parblock
    <code>const char *</code>

    Plugin's license.
    @endparblock

@bt_pre_not_null{_license}
*/
#define BT_PLUGIN_LICENSE_WITH_ID(_id, _license) \
	__BT_PLUGIN_DESCRIPTOR_ATTRIBUTE(license, BT_PLUGIN_DESCRIPTOR_ATTRIBUTE_TYPE_LICENSE, _id, _license)

/*!
@brief
    Alias of BT_PLUGIN_LICENSE_WITH_ID() with the \bt_p{_id}
    parameter set to <code>auto</code>.
*/
#define BT_PLUGIN_LICENSE(_license) 		BT_PLUGIN_LICENSE_WITH_ID(auto, _license)

/*!
@brief
    Sets the version of the plugin having the ID \bt_p{_id} to
    \bt_p{_version}.

See the \ref api-plugin-prop-version "version" property.

@param[in] _id
    @parblock
    C identifier.

    ID of the plugin of which to set the version.
    @endparblock
@param[in] _major
    @parblock
    <code>unsigned int</code>

    Plugin's major version.
    @endparblock
@param[in] _minor
    @parblock
    <code>unsigned int</code>

    Plugin's minor version.
    @endparblock
@param[in] _patch
    @parblock
    <code>unsigned int</code>

    Plugin's patch version.
    @endparblock
@param[in] _extra
    @parblock
    <code>const char *</code>

    Plugin's version's extra information.

    Can be \c NULL if the plugin's version has no extra information.
    @endparblock
*/
#define BT_PLUGIN_VERSION_WITH_ID(_id, _major, _minor, _patch, _extra) \
	__BT_PLUGIN_DESCRIPTOR_ATTRIBUTE(version, BT_PLUGIN_DESCRIPTOR_ATTRIBUTE_TYPE_VERSION, _id, __BT_PLUGIN_VERSION_STRUCT_VALUE(_major, _minor, _patch, _extra))

/*!
@brief
    Alias of BT_PLUGIN_VERSION_WITH_ID() with the \bt_p{_id}
    parameter set to <code>auto</code>.
*/
#define BT_PLUGIN_VERSION(_major, _minor, _patch, _extra) BT_PLUGIN_VERSION_WITH_ID(auto, _major, _minor, _patch, _extra)

/*! @} */

/*!
@name Plugin functions
@{
*/

/*!
@brief
    Status codes for #bt_plugin_initialize_func.
*/
typedef enum bt_plugin_initialize_func_status {
	/*!
	@brief
	    Success.
	*/
	BT_PLUGIN_INITIALIZE_FUNC_STATUS_OK		= __BT_FUNC_STATUS_OK,

	/*!
	@brief
	    Out of memory.
	*/
	BT_PLUGIN_INITIALIZE_FUNC_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,

	/*!
	@brief
	    Error.
	*/
	BT_PLUGIN_INITIALIZE_FUNC_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
} bt_plugin_initialize_func_status;

/*!
@brief
    User plugin initialization function.

@param[in] self_plugin
    @parblock
    Plugin instance.

    This parameter is a private view of the \bt_plugin object for
    this function.

    As of \bt_name_version_min_maj, there's no self plugin API.
    @endparblock

@retval #BT_PLUGIN_INITIALIZE_FUNC_STATUS_OK
    Success.
@retval #BT_PLUGIN_INITIALIZE_FUNC_STATUS_MEMORY_ERROR
    Out of memory.
@retval #BT_PLUGIN_INITIALIZE_FUNC_STATUS_ERROR
    Error.

@bt_pre_not_null{self_plugin}
*/
typedef bt_plugin_initialize_func_status (*bt_plugin_initialize_func)(
		bt_self_plugin *self_plugin);

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
    #bt_plugin_initialize_func

    Plugin's initialization function.
    @endparblock

@bt_pre_not_null{_func}
*/
#define BT_PLUGIN_INITIALIZE_FUNC_WITH_ID(_id, _func) \
	__BT_PLUGIN_DESCRIPTOR_ATTRIBUTE(init, BT_PLUGIN_DESCRIPTOR_ATTRIBUTE_TYPE_INIT, _id, _func)

/*!
@brief
    Alias of BT_PLUGIN_INITIALIZE_FUNC_WITH_ID() with the \bt_p{_id}
    parameter set to <code>auto</code>.
*/
#define BT_PLUGIN_INITIALIZE_FUNC(_func) 	BT_PLUGIN_INITIALIZE_FUNC_WITH_ID(auto, _func)

/*!
@brief
    User plugin finalization function.
*/
typedef void (*bt_plugin_finalize_func)(void);

/*!
@brief
    Sets the finalization function of the plugin having the ID
    \bt_p{_id} to \bt_p{_func}.

@param[in] _id
    @parblock
    C identifier.

    ID of the plugin of which to set the finalization function.
    @endparblock
@param[in] _func
    @parblock
    #bt_plugin_finalize_func

    Plugin's finalization function.
    @endparblock

@bt_pre_not_null{_func}
*/
#define BT_PLUGIN_FINALIZE_FUNC_WITH_ID(_id, _func) \
	__BT_PLUGIN_DESCRIPTOR_ATTRIBUTE(exit, BT_PLUGIN_DESCRIPTOR_ATTRIBUTE_TYPE_EXIT, _id, _func)

/*!
@brief
    Alias of BT_PLUGIN_FINALIZE_FUNC_WITH_ID() with the \bt_p{_id}
    parameter set to <code>auto</code>.
*/
#define BT_PLUGIN_FINALIZE_FUNC(_func)	BT_PLUGIN_FINALIZE_FUNC_WITH_ID(auto, _func)

/*! @} */

/*!
@name Component class adding
@{
*/

/*!
@brief
    Adds a \bt_src_comp_cls named \bt_p{_name}, having the ID
    \bt_p{_component_class_id} and the message iterator class's "next"
    method \bt_p{_message_iterator_class_next_method}, to the plugin
    having the ID \bt_p{_plugin_id}.

@param[in] _plugin_id
    @parblock
    C identifier.

    ID of the plugin to which to add the source component class.
    @endparblock
@param[in] _component_class_id
    @parblock
    C identifier.

    Source component class's ID, unique amongst all the source component
    class IDs of the same plugin.
    @endparblock
@param[in] _name
    @parblock
    <code>const char *</code>

    Source component class's name, unique amongst all the source
    component class names of the same plugin.
    @endparblock
@param[in] _message_iterator_class_next_method
    @parblock
    #bt_message_iterator_class_next_method

    Source component class's message iterator class's "next" method.
    @endparblock

@bt_pre_not_null{_name}
@bt_pre_not_null{_message_iterator_class_next_method}
*/
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_WITH_ID(_plugin_id, _component_class_id, _name, _message_iterator_class_next_method) \
	static struct __bt_plugin_component_class_descriptor __bt_plugin_source_component_class_descriptor_##_plugin_id##_##_component_class_id = { \
		.plugin_descriptor = &__bt_plugin_descriptor_##_plugin_id,		\
		.name = _name,								\
		.type = BT_COMPONENT_CLASS_TYPE_SOURCE,					\
		.methods = { .source = { .msg_iter_next = _message_iterator_class_next_method } },	\
	};										\
	static struct __bt_plugin_component_class_descriptor const * const __bt_plugin_source_component_class_descriptor_##_plugin_id##_##_component_class_id##_ptr __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRS = &__bt_plugin_source_component_class_descriptor_##_plugin_id##_##_component_class_id

/*!
@brief
    Alias of BT_PLUGIN_SOURCE_COMPONENT_CLASS_WITH_ID() with the
    \bt_p{_plugin_id} parameter set to <code>auto</code>, the
    \bt_p{_component_class_id} parameter set to \bt_p{_name}, and
    the \bt_p{_name} parameter set to a string version of \bt_p{_name}.

@param[in] _name
    @parblock
    C identifier

    Passed as both the \bt_p{_component_class_id} and the
    \bt_p{_name} (once converted to a string) parameters of
    BT_PLUGIN_SOURCE_COMPONENT_CLASS_WITH_ID().
    @endparblock
*/
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS(_name, _message_iterator_class_next_method) \
	BT_PLUGIN_SOURCE_COMPONENT_CLASS_WITH_ID(auto, _name, #_name, _message_iterator_class_next_method)

/*!
@brief
    Adds a \bt_flt_comp_cls named \bt_p{_name}, having the ID
    \bt_p{_component_class_id} and the message iterator class's "next"
    method \bt_p{_message_iterator_class_next_method}, to the plugin
    having the ID \bt_p{_plugin_id}.

@param[in] _plugin_id
    @parblock
    C identifier.

    ID of the plugin to which to add the filter component class.
    @endparblock
@param[in] _component_class_id
    @parblock
    C identifier.

    Filter component class's ID, unique amongst all the filter component
    class IDs of the same plugin.
    @endparblock
@param[in] _name
    @parblock
    <code>const char *</code>

    Filter component class's name, unique amongst all the filter
    component class names of the same plugin.
    @endparblock
@param[in] _message_iterator_class_next_method
    @parblock
    #bt_message_iterator_class_next_method

    Filter component class's message iterator class's "next" method.
    @endparblock

@bt_pre_not_null{_name}
@bt_pre_not_null{_message_iterator_class_next_method}
*/
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_WITH_ID(_plugin_id, _component_class_id, _name, _message_iterator_class_next_method) \
	static struct __bt_plugin_component_class_descriptor __bt_plugin_filter_component_class_descriptor_##_plugin_id##_##_component_class_id = { \
		.plugin_descriptor = &__bt_plugin_descriptor_##_plugin_id,		\
		.name = _name,								\
		.type = BT_COMPONENT_CLASS_TYPE_FILTER,					\
		.methods = { .filter = { .msg_iter_next = _message_iterator_class_next_method } },	\
	};										\
	static struct __bt_plugin_component_class_descriptor const * const __bt_plugin_filter_component_class_descriptor_##_plugin_id##_##_component_class_id##_ptr __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRS = &__bt_plugin_filter_component_class_descriptor_##_plugin_id##_##_component_class_id

/*!
@brief
    Alias of BT_PLUGIN_FILTER_COMPONENT_CLASS_WITH_ID() with the
    \bt_p{_plugin_id} parameter set to <code>auto</code>, the
    \bt_p{_component_class_id} parameter set to \bt_p{_name}, and
    the \bt_p{_name} parameter set to a string version of \bt_p{_name}.

@param[in] _name
    @parblock
    C identifier

    Passed as both the \bt_p{_component_class_id} and the
    \bt_p{_name} (once converted to a string) parameters of
    BT_PLUGIN_FILTER_COMPONENT_CLASS_WITH_ID().
    @endparblock
*/
#define BT_PLUGIN_FILTER_COMPONENT_CLASS(_name, _message_iterator_class_next_method) \
	BT_PLUGIN_FILTER_COMPONENT_CLASS_WITH_ID(auto, _name, #_name, _message_iterator_class_next_method)

/*!
@brief
    Adds a \bt_sink_comp_cls named \bt_p{_name}, having the ID
    \bt_p{_component_class_id} and the consuming method
    \bt_p{_consume_method}, to the plugin
    having the ID \bt_p{_plugin_id}.

@param[in] _plugin_id
    @parblock
    C identifier.

    ID of the plugin to which to add the sink component class.
    @endparblock
@param[in] _component_class_id
    @parblock
    C identifier.

    Sink component class's ID, unique amongst all the sink component
    class IDs of the same plugin.
    @endparblock
@param[in] _name
    @parblock
    <code>const char *</code>

    Sink component class's name, unique amongst all the sink
    component class names of the same plugin.
    @endparblock
@param[in] _consume_method
    @parblock
    #bt_component_class_sink_consume_method

    Sink component class's message iterator class's "next" method.
    @endparblock

@bt_pre_not_null{_name}
@bt_pre_not_null{_consume_method}
*/
#define BT_PLUGIN_SINK_COMPONENT_CLASS_WITH_ID(_plugin_id, _component_class_id, _name, _consume_method) \
	static struct __bt_plugin_component_class_descriptor __bt_plugin_sink_component_class_descriptor_##_plugin_id##_##_component_class_id = { \
		.plugin_descriptor = &__bt_plugin_descriptor_##_plugin_id, \
		.name = _name,						\
		.type = BT_COMPONENT_CLASS_TYPE_SINK,			\
		.methods = { .sink = { .consume = _consume_method } },	\
	};								\
	static struct __bt_plugin_component_class_descriptor const * const __bt_plugin_sink_component_class_descriptor_##_plugin_id##_##_component_class_id##_ptr __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRS = &__bt_plugin_sink_component_class_descriptor_##_plugin_id##_##_component_class_id

/*!
@brief
    Alias of BT_PLUGIN_SINK_COMPONENT_CLASS_WITH_ID() with the
    \bt_p{_plugin_id} parameter set to <code>auto</code>, the
    \bt_p{_component_class_id} parameter set to \bt_p{_name}, and
    the \bt_p{_name} parameter set to a string version of \bt_p{_name}.

@param[in] _name
    @parblock
    C identifier

    Passed as both the \bt_p{_component_class_id} and the
    \bt_p{_name} (once converted to a string) parameters of
    BT_PLUGIN_SINK_COMPONENT_CLASS_WITH_ID().
    @endparblock
*/
#define BT_PLUGIN_SINK_COMPONENT_CLASS(_name, _consume_method) \
	BT_PLUGIN_SINK_COMPONENT_CLASS_WITH_ID(auto, _name, #_name, _consume_method)

/*! @} */

/*!
@name Source component class properties
@{
*/

/*!
@brief
    Sets the description of the \bt_src_comp_cls having the ID
    \bt_p{_component_class_id} in the plugin having the ID
    \bt_p{_plugin_id} to \bt_p{_description}.

See the \ref api-comp-cls-prop-descr "description" property.

@param[in] _plugin_id
    @parblock
    C identifier.

    ID of the plugin which contains the source component class of which
    to set the description.
    @endparblock
@param[in] _component_class_id
    @parblock
    C identifier.

    ID of the source component class, within the plugin having the ID
    \bt_p{_plugin_id}, of which to set the description to
    \bt_p{_description}.
    @endparblock
@param[in] _description
    @parblock
    <code>const char *</code>

    Source component class's description.
    @endparblock

@bt_pre_not_null{_description}
*/
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_DESCRIPTION_WITH_ID(_plugin_id, _component_class_id, _description) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(description, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_DESCRIPTION, _plugin_id, _component_class_id, source, _description)

/*!
@brief
    Alias of BT_PLUGIN_SOURCE_COMPONENT_CLASS_DESCRIPTION_WITH_ID() with
    the \bt_p{_plugin_id} parameter set to <code>auto</code> and the
    \bt_p{_component_class_id} parameter set to \bt_p{_name}.
*/
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_DESCRIPTION(_name, _description) \
	BT_PLUGIN_SOURCE_COMPONENT_CLASS_DESCRIPTION_WITH_ID(auto, _name, _description)

/*!
@brief
    Sets the help text of the \bt_src_comp_cls having the ID
    \bt_p{_component_class_id} in the plugin having the ID
    \bt_p{_plugin_id} to \bt_p{_help_text}.

See the \ref api-comp-cls-prop-help "help text" property.

@param[in] _plugin_id
    @parblock
    C identifier.

    ID of the plugin which contains the source component class of which
    to set the help text.
    @endparblock
@param[in] _component_class_id
    @parblock
    C identifier.

    ID of the source component class, within the plugin having the ID
    \bt_p{_plugin_id}, of which to set the help text to
    \bt_p{_help_text}.
    @endparblock
@param[in] _help_text
    @parblock
    <code>const char *</code>

    Source component class's help text.
    @endparblock

@bt_pre_not_null{_help_text}
*/
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_HELP_WITH_ID(_plugin_id, _component_class_id, _help_text) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(help, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_HELP, _plugin_id, _component_class_id, source, _help_text)

/*!
@brief
    Alias of BT_PLUGIN_SOURCE_COMPONENT_CLASS_HELP_WITH_ID() with the
    \bt_p{_plugin_id} parameter set to <code>auto</code> and the
    \bt_p{_component_class_id} parameter set to \bt_p{_name}.
*/
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_HELP(_name, _help_text) \
	BT_PLUGIN_SOURCE_COMPONENT_CLASS_HELP_WITH_ID(auto, _name, _help_text)

/*! @} */

/*!
@name Filter component class properties
@{
*/

/*!
@brief
    Sets the description of the \bt_flt_comp_cls having the ID
    \bt_p{_component_class_id} in the plugin having the ID
    \bt_p{_plugin_id} to \bt_p{_description}.

See the \ref api-comp-cls-prop-descr "description" property.

@param[in] _plugin_id
    @parblock
    C identifier.

    ID of the plugin which contains the filter component class of which
    to set the description.
    @endparblock
@param[in] _component_class_id
    @parblock
    C identifier.

    ID of the filter component class, within the plugin having the ID
    \bt_p{_plugin_id}, of which to set the description to
    \bt_p{_description}.
    @endparblock
@param[in] _description
    @parblock
    <code>const char *</code>

    Filter component class's description.
    @endparblock

@bt_pre_not_null{_description}
*/
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_DESCRIPTION_WITH_ID(_plugin_id, _component_class_id, _description) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(description, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_DESCRIPTION, _plugin_id, _component_class_id, filter, _description)

/*!
@brief
    Alias of BT_PLUGIN_FILTER_COMPONENT_CLASS_DESCRIPTION_WITH_ID() with
    the \bt_p{_plugin_id} parameter set to <code>auto</code> and the
    \bt_p{_component_class_id} parameter set to \bt_p{_name}.
*/
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_DESCRIPTION(_name, _description) \
	BT_PLUGIN_FILTER_COMPONENT_CLASS_DESCRIPTION_WITH_ID(auto, _name, _description)

/*!
@brief
    Sets the help text of the \bt_flt_comp_cls having the ID
    \bt_p{_component_class_id} in the plugin having the ID
    \bt_p{_plugin_id} to \bt_p{_help_text}.

See the \ref api-comp-cls-prop-help "help text" property.

@param[in] _plugin_id
    @parblock
    C identifier.

    ID of the plugin which contains the filter component class of which
    to set the help text.
    @endparblock
@param[in] _component_class_id
    @parblock
    C identifier.

    ID of the filter component class, within the plugin having the ID
    \bt_p{_plugin_id}, of which to set the help text to
    \bt_p{_help_text}.
    @endparblock
@param[in] _help_text
    @parblock
    <code>const char *</code>

    Filter component class's help text.
    @endparblock

@bt_pre_not_null{_help_text}
*/
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_HELP_WITH_ID(_plugin_id, _component_class_id, _help_text) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(help, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_HELP, _plugin_id, _component_class_id, filter, _help_text)

/*!
@brief
    Alias of BT_PLUGIN_FILTER_COMPONENT_CLASS_HELP_WITH_ID() with the
    \bt_p{_plugin_id} parameter set to <code>auto</code> and the
    \bt_p{_component_class_id} parameter set to \bt_p{_name}.
*/
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_HELP(_name, _help_text) \
	BT_PLUGIN_FILTER_COMPONENT_CLASS_HELP_WITH_ID(auto, _name, _help_text)

/*! @} */

/*!
@name Sink component class properties
@{
*/

/*!
@brief
    Sets the description of the \bt_sink_comp_cls having the ID
    \bt_p{_component_class_id} in the plugin having the ID
    \bt_p{_plugin_id} to \bt_p{_description}.

See the \ref api-comp-cls-prop-descr "description" property.

@param[in] _plugin_id
    @parblock
    C identifier.

    ID of the plugin which contains the sink component class of which
    to set the description.
    @endparblock
@param[in] _component_class_id
    @parblock
    C identifier.

    ID of the sink component class, within the plugin having the ID
    \bt_p{_plugin_id}, of which to set the description to
    \bt_p{_description}.
    @endparblock
@param[in] _description
    @parblock
    <code>const char *</code>

    Sink component class's description.
    @endparblock

@bt_pre_not_null{_description}
*/
#define BT_PLUGIN_SINK_COMPONENT_CLASS_DESCRIPTION_WITH_ID(_plugin_id, _component_class_id, _description) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(description, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_DESCRIPTION, _plugin_id, _component_class_id, sink, _description)

/*!
@brief
    Alias of BT_PLUGIN_SINK_COMPONENT_CLASS_DESCRIPTION_WITH_ID() with
    the \bt_p{_plugin_id} parameter set to <code>auto</code> and the
    \bt_p{_component_class_id} parameter set to \bt_p{_name}.
*/
#define BT_PLUGIN_SINK_COMPONENT_CLASS_DESCRIPTION(_name, _description) \
	BT_PLUGIN_SINK_COMPONENT_CLASS_DESCRIPTION_WITH_ID(auto, _name, _description)

/*!
@brief
    Sets the help text of the \bt_sink_comp_cls having the ID
    \bt_p{_component_class_id} in the plugin having the ID
    \bt_p{_plugin_id} to \bt_p{_help_text}.

See the \ref api-comp-cls-prop-help "help text" property.

@param[in] _plugin_id
    @parblock
    C identifier.

    ID of the plugin which contains the sink component class of which
    to set the help text.
    @endparblock
@param[in] _component_class_id
    @parblock
    C identifier.

    ID of the sink component class, within the plugin having the ID
    \bt_p{_plugin_id}, of which to set the help text to
    \bt_p{_help_text}.
    @endparblock
@param[in] _help_text
    @parblock
    <code>const char *</code>

    Sink component class's help text.
    @endparblock

@bt_pre_not_null{_help_text}
*/
#define BT_PLUGIN_SINK_COMPONENT_CLASS_HELP_WITH_ID(_plugin_id, _component_class_id, _help_text) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(help, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_HELP, _plugin_id, _component_class_id, sink, _help_text)

/*!
@brief
    Alias of BT_PLUGIN_SINK_COMPONENT_CLASS_HELP_WITH_ID() with
    the \bt_p{_plugin_id} parameter set to <code>auto</code> and the
    \bt_p{_component_class_id} parameter set to \bt_p{_name}.
*/
#define BT_PLUGIN_SINK_COMPONENT_CLASS_HELP(_name, _help_text) \
	BT_PLUGIN_SINK_COMPONENT_CLASS_HELP_WITH_ID(auto, _name, _help_text)

/*! @} */

/*!
@name Source component class methods
@{
*/

/*!
@brief
    Sets the finalization method of the \bt_src_comp_cls having the ID
    \bt_p{_component_class_id} in the plugin having the ID
    \bt_p{_plugin_id} to \bt_p{_method}.

See the \ref api-comp-cls-dev-meth-fini "finalize" method.

@param[in] _plugin_id
    @parblock
    C identifier.

    ID of the plugin which contains the source component class of which
    to set the finalization method.
    @endparblock
@param[in] _component_class_id
    @parblock
    C identifier.

    ID of the source component class, within the plugin having the ID
    \bt_p{_plugin_id}, of which to set the finalization method to
    \bt_p{_method}.
    @endparblock
@param[in] _method
    @parblock
    #bt_component_class_source_finalize_method

    Source component class's finalization method.
    @endparblock

@bt_pre_not_null{_method}
*/
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_FINALIZE_METHOD_WITH_ID(_plugin_id, _component_class_id, _method) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(source_finalize_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_FINALIZE_METHOD, _plugin_id, _component_class_id, source, _method)

/*!
@brief
    Alias of
    BT_PLUGIN_SOURCE_COMPONENT_CLASS_FINALIZE_METHOD_WITH_ID()
    with the \bt_p{_plugin_id} parameter set to <code>auto</code> and
    the \bt_p{_component_class_id} parameter set to \bt_p{_name}.
*/
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_FINALIZE_METHOD(_name, _method) \
	BT_PLUGIN_SOURCE_COMPONENT_CLASS_FINALIZE_METHOD_WITH_ID(auto, _name, _method)

/*!
@brief
    Sets the \"get supported \bt_mip versions\" method of the
    \bt_src_comp_cls having the ID \bt_p{_component_class_id} in the
    plugin having the ID \bt_p{_plugin_id} to \bt_p{_method}.

See the \ref api-comp-cls-dev-meth-mip "get supported MIP versions"
method.

@param[in] _plugin_id
    @parblock
    C identifier.

    ID of the plugin which contains the source component class of which
    to set the "get supported MIP versions" method.
    @endparblock
@param[in] _component_class_id
    @parblock
    C identifier.

    ID of the source component class, within the plugin having the ID
    \bt_p{_plugin_id}, of which to set the "get supported MIP versions"
    method to \bt_p{_method}.
    @endparblock
@param[in] _method
    @parblock
    #bt_component_class_source_get_supported_mip_versions_method

    Source component class's "get supported MIP versions" method.
    @endparblock

@bt_pre_not_null{_method}
*/
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD_WITH_ID(_plugin_id, _component_class_id, _method) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(source_get_supported_mip_versions_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_GET_SUPPORTED_MIP_VERSIONS_METHOD, _plugin_id, _component_class_id, source, _method)

/*!
@brief
    Alias of
    BT_PLUGIN_SOURCE_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD_WITH_ID()
    with the \bt_p{_plugin_id} parameter set to <code>auto</code> and
    the \bt_p{_component_class_id} parameter set to \bt_p{_name}.
*/
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD(_name, _method) \
	BT_PLUGIN_SOURCE_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD_WITH_ID(auto, _name, _method)

/*!
@brief
    Sets the initialization method of the \bt_src_comp_cls having the ID
    \bt_p{_component_class_id} in the plugin having the ID
    \bt_p{_plugin_id} to \bt_p{_method}.

See the \ref api-comp-cls-dev-meth-init "initialize" method.

@param[in] _plugin_id
    @parblock
    C identifier.

    ID of the plugin which contains the source component class of which
    to set the initialization method.
    @endparblock
@param[in] _component_class_id
    @parblock
    C identifier.

    ID of the source component class, within the plugin having the ID
    \bt_p{_plugin_id}, of which to set the initialization method to
    \bt_p{_method}.
    @endparblock
@param[in] _method
    @parblock
    #bt_component_class_source_initialize_method

    Source component class's initialization method.
    @endparblock

@bt_pre_not_null{_method}
*/
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_INITIALIZE_METHOD_WITH_ID(_plugin_id, _component_class_id, _method) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(source_initialize_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_INITIALIZE_METHOD, _plugin_id, _component_class_id, source, _method)

/*!
@brief
    Alias of
    BT_PLUGIN_SOURCE_COMPONENT_CLASS_INITIALIZE_METHOD_WITH_ID()
    with the \bt_p{_plugin_id} parameter set to <code>auto</code> and
    the \bt_p{_component_class_id} parameter set to \bt_p{_name}.
*/
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_INITIALIZE_METHOD(_name, _method) \
	BT_PLUGIN_SOURCE_COMPONENT_CLASS_INITIALIZE_METHOD_WITH_ID(auto, _name, _method)

/*!
@brief
    Sets the finalization method of the \bt_msg_iter_cls of the
    \bt_src_comp_cls having the ID \bt_p{_component_class_id} in the
    plugin having the ID \bt_p{_plugin_id} to \bt_p{_method}.

See the \ref api-msg-iter-cls-meth-fini "finalize" method.

@param[in] _plugin_id
    @parblock
    C identifier.

    ID of the plugin which contains the source component class of which
    to set the finalization method of the message iterator class.
    @endparblock
@param[in] _component_class_id
    @parblock
    C identifier.

    ID of the source component class, within the plugin having the ID
    \bt_p{_plugin_id}, of which to set the finalization method of the
    message iterator class to \bt_p{_method}.
    @endparblock
@param[in] _method
    @parblock
    #bt_message_iterator_class_finalize_method

    Source component class's message iterator class's finalization method.
    @endparblock

@bt_pre_not_null{_method}
*/
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_FINALIZE_METHOD_WITH_ID(_plugin_id, _component_class_id, _method) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(msg_iter_finalize_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_MSG_ITER_FINALIZE_METHOD, _plugin_id, _component_class_id, source, _method)

/*!
@brief
    Alias of
    BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_FINALIZE_METHOD_WITH_ID()
    with the \bt_p{_plugin_id} parameter set to <code>auto</code> and
    the \bt_p{_component_class_id} parameter set to \bt_p{_name}.
*/
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_FINALIZE_METHOD(_name, _method) \
	BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_FINALIZE_METHOD_WITH_ID(auto, _name, _method)

/*!
@brief
    Sets the initialization method of the \bt_msg_iter_cls of the
    \bt_src_comp_cls having the ID \bt_p{_component_class_id} in the
    plugin having the ID \bt_p{_plugin_id} to \bt_p{_method}.

See the \ref api-msg-iter-cls-meth-init "initialize" method.

@param[in] _plugin_id
    @parblock
    C identifier.

    ID of the plugin which contains the source component class of which
    to set the initialization method of the message iterator class.
    @endparblock
@param[in] _component_class_id
    @parblock
    C identifier.

    ID of the source component class, within the plugin having the ID
    \bt_p{_plugin_id}, of which to set the initialization method of the
    message iterator class to \bt_p{_method}.
    @endparblock
@param[in] _method
    @parblock
    #bt_message_iterator_class_initialize_method

    Source component class's message iterator class's initialization
    method.
    @endparblock

@bt_pre_not_null{_method}
*/
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_WITH_ID(_plugin_id, _component_class_id, _method) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(msg_iter_initialize_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_MSG_ITER_INITIALIZE_METHOD, _plugin_id, _component_class_id, source, _method)

/*!
@brief
    Alias of
    BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_WITH_ID()
    with the \bt_p{_plugin_id} parameter set to <code>auto</code> and
    the \bt_p{_component_class_id} parameter set to \bt_p{_name}.
*/
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD(_name, _method) \
	BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_WITH_ID(auto, _name, _method)

/*!
@brief
    Sets the "seek beginning" and "can seek beginning?" methods of the
    \bt_msg_iter_cls of the \bt_src_comp_cls having the ID
    \bt_p{_component_class_id} in the plugin having the ID
    \bt_p{_plugin_id} to \bt_p{_seek_method} and
    \bt_p{_can_seek_method}.

See the \ref api-msg-iter-cls-meth-seek-beg "seek beginning" and
\ref api-msg-iter-cls-meth-can-seek-beg "can seek beginning?" methods.

@param[in] _plugin_id
    @parblock
    C identifier.

    ID of the plugin which contains the source component class of which
    to set the "seek beginning" and "can seek beginning?" methods of the
    message iterator class.
    @endparblock
@param[in] _component_class_id
    @parblock
    C identifier.

    ID of the source component class, within the plugin having the ID
    \bt_p{_plugin_id}, of which to set the "seek beginning" and "can
    seek beginning" methods of the message iterator class to
    \bt_p{_seek_method} and \bt_p{_can_seek_method}.
    @endparblock
@param[in] _seek_method
    @parblock
    #bt_message_iterator_class_seek_beginning_method

    Source component class's message iterator class's "seek beginning"
    method.
    @endparblock
@param[in] _can_seek_method
    @parblock
    #bt_message_iterator_class_can_seek_beginning_method

    Source component class's message iterator class's
    "can seek beginning?" method.

    Can be \c NULL, in which case it is equivalent to setting a method
    which always returns #BT_TRUE.
    @endparblock

@bt_pre_not_null{_seek_method}
@bt_pre_not_null{_can_seek_method}
*/
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHODS_WITH_ID(_plugin_id, _component_class_id, _seek_method, _can_seek_method) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(msg_iter_seek_beginning_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_MSG_ITER_SEEK_BEGINNING_METHOD, _plugin_id, _component_class_id, source, _seek_method); \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(msg_iter_can_seek_beginning_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_MSG_ITER_CAN_SEEK_BEGINNING_METHOD, _plugin_id, _component_class_id, source, _can_seek_method)

/*!
@brief
    Alias of
    BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHODS()
    with the \bt_p{_plugin_id} parameter set to <code>auto</code> and
    the \bt_p{_component_class_id} parameter set to \bt_p{_name}.
*/
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHODS(_name, _seek_method, _can_seek_method) \
	BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHODS_WITH_ID(auto, _name, _seek_method, _can_seek_method)

/*!
@brief
    Sets the "seek ns from origin" and "can seek ns from origin?"
    methods of the \bt_msg_iter_cls of the \bt_src_comp_cls having the
    ID \bt_p{_component_class_id} in the plugin having the ID
    \bt_p{_plugin_id} to \bt_p{_seek_method} and
    \bt_p{_can_seek_method}.

See the \ref api-msg-iter-cls-meth-seek-ns "seek ns from origin" and
\ref api-msg-iter-cls-meth-can-seek-ns "can seek ns from origin?"
methods.

@param[in] _plugin_id
    @parblock
    C identifier.

    ID of the plugin which contains the source component class of which
    to set the "seek ns from origin" and "can seek ns from origin?"
    methods of the message iterator class.
    @endparblock
@param[in] _component_class_id
    @parblock
    C identifier.

    ID of the source component class, within the plugin having the ID
    \bt_p{_plugin_id}, of which to set the "seek ns from origin" and
    "can seek ns from origin" methods of the message iterator class to
    \bt_p{_seek_method} and \bt_p{_can_seek_method}.
    @endparblock
@param[in] _seek_method
    @parblock
    #bt_message_iterator_class_seek_ns_from_origin_method

    Source component class's message iterator class's "seek ns from
    origin" method.
    @endparblock
@param[in] _can_seek_method
    @parblock
    #bt_message_iterator_class_can_seek_ns_from_origin_method

    Source component class's message iterator class's "can seek ns from
    origin?" method.

    Can be \c NULL, in which case it is equivalent to setting a method
    which always returns #BT_TRUE.
    @endparblock

@bt_pre_not_null{_seek_method}
@bt_pre_not_null{_can_seek_method}
*/
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_SEEK_NS_FROM_ORIGIN_METHODS_WITH_ID(_plugin_id, _component_class_id, _seek_method, _can_seek_method) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(msg_iter_seek_ns_from_origin_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_MSG_ITER_SEEK_NS_FROM_ORIGIN_METHOD, _plugin_id, _component_class_id, source, _seek_method); \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(msg_iter_can_seek_ns_from_origin_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_MSG_ITER_CAN_SEEK_NS_FROM_ORIGIN_METHOD, _plugin_id, _component_class_id, source, _can_seek_method)

/*!
@brief
    Alias of
    BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_SEEK_NS_FROM_ORIGIN_METHODS_WITH_ID()
    with the \bt_p{_plugin_id} parameter set to <code>auto</code> and
    the \bt_p{_component_class_id} parameter set to \bt_p{_name}.
*/
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_SEEK_NS_FROM_ORIGIN_METHODS(_name, _seek_method, _can_seek_method) \
	BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_SEEK_NS_FROM_ORIGIN_METHODS_WITH_ID(auto, _name, _seek_method, _can_seek_method)

/*!
@brief
    Sets the "output port connected" method of the \bt_src_comp_cls
    having the ID \bt_p{_component_class_id} in the plugin having the ID
    \bt_p{_plugin_id} to \bt_p{_method}.

See the
\ref api-comp-cls-dev-meth-oport-connected "output port connected"
method.

@param[in] _plugin_id
    @parblock
    C identifier.

    ID of the plugin which contains the source component class of which
    to set the "output port connected" method.
    @endparblock
@param[in] _component_class_id
    @parblock
    C identifier.

    ID of the source component class, within the plugin having the ID
    \bt_p{_plugin_id}, of which to set the "output port connected"
    method to \bt_p{_method}.
    @endparblock
@param[in] _method
    @parblock
    #bt_component_class_source_output_port_connected_method

    Source component class's "output port connected" method.
    @endparblock

@bt_pre_not_null{_method}
*/
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_OUTPUT_PORT_CONNECTED_METHOD_WITH_ID(_plugin_id, _component_class_id, _method) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(source_output_port_connected_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_OUTPUT_PORT_CONNECTED_METHOD, _plugin_id, _component_class_id, source, _method)

/*!
@brief
    Alias of
    BT_PLUGIN_SOURCE_COMPONENT_CLASS_OUTPUT_PORT_CONNECTED_METHOD_WITH_ID()
    with the \bt_p{_plugin_id} parameter set to <code>auto</code> and
    the \bt_p{_component_class_id} parameter set to \bt_p{_name}.
*/
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_OUTPUT_PORT_CONNECTED_METHOD(_name, _method) \
	BT_PLUGIN_SOURCE_COMPONENT_CLASS_OUTPUT_PORT_CONNECTED_METHOD_WITH_ID(auto, _name, _method)

/*!
@brief
    Sets the query method of the \bt_src_comp_cls
    having the ID \bt_p{_component_class_id} in the plugin having the ID
    \bt_p{_plugin_id} to \bt_p{_method}.

See the \ref api-comp-cls-dev-meth-query "query" method.

@param[in] _plugin_id
    @parblock
    C identifier.

    ID of the plugin which contains the source component class of which
    to set the query method.
    @endparblock
@param[in] _component_class_id
    @parblock
    C identifier.

    ID of the source component class, within the plugin having the ID
    \bt_p{_plugin_id}, of which to set the query
    method to \bt_p{_method}.
    @endparblock
@param[in] _method
    @parblock
    #bt_component_class_source_query_method

    Source component class's query method.
    @endparblock

@bt_pre_not_null{_method}
*/
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_QUERY_METHOD_WITH_ID(_plugin_id, _component_class_id, _method) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(source_query_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_QUERY_METHOD, _plugin_id, _component_class_id, source, _method)

/*!
@brief
    Alias of
    BT_PLUGIN_SOURCE_COMPONENT_CLASS_QUERY_METHOD_WITH_ID()
    with the \bt_p{_plugin_id} parameter set to <code>auto</code> and
    the \bt_p{_component_class_id} parameter set to \bt_p{_name}.
*/
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_QUERY_METHOD(_name, _method) \
	BT_PLUGIN_SOURCE_COMPONENT_CLASS_QUERY_METHOD_WITH_ID(auto, _name, _method)

/*! @} */

/*!
@name Filter component class methods
@{
*/

/*!
@brief
    Sets the finalization method of the \bt_flt_comp_cls having the ID
    \bt_p{_component_class_id} in the plugin having the ID
    \bt_p{_plugin_id} to \bt_p{_method}.

See the \ref api-comp-cls-dev-meth-fini "finalize" method.

@param[in] _plugin_id
    @parblock
    C identifier.

    ID of the plugin which contains the filter component class of which
    to set the finalization method.
    @endparblock
@param[in] _component_class_id
    @parblock
    C identifier.

    ID of the filter component class, within the plugin having the ID
    \bt_p{_plugin_id}, of which to set the finalization method to
    \bt_p{_method}.
    @endparblock
@param[in] _method
    @parblock
    #bt_component_class_filter_finalize_method

    Filter component class's finalization method.
    @endparblock

@bt_pre_not_null{_method}
*/
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_FINALIZE_METHOD_WITH_ID(_plugin_id, _component_class_id, _method) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(filter_finalize_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_FINALIZE_METHOD, _plugin_id, _component_class_id, filter, _method)

/*!
@brief
    Alias of
    BT_PLUGIN_FILTER_COMPONENT_CLASS_FINALIZE_METHOD_WITH_ID()
    with the \bt_p{_plugin_id} parameter set to <code>auto</code> and
    the \bt_p{_component_class_id} parameter set to \bt_p{_name}.
*/
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_FINALIZE_METHOD(_name, _method) \
	BT_PLUGIN_FILTER_COMPONENT_CLASS_FINALIZE_METHOD_WITH_ID(auto, _name, _method)

/*!
@brief
    Sets the \"get supported \bt_mip versions\" method of the
    \bt_flt_comp_cls having the ID \bt_p{_component_class_id} in the
    plugin having the ID \bt_p{_plugin_id} to \bt_p{_method}.

See the \ref api-comp-cls-dev-meth-mip "get supported MIP versions"
method.

@param[in] _plugin_id
    @parblock
    C identifier.

    ID of the plugin which contains the filter component class of which
    to set the "get supported MIP versions" method.
    @endparblock
@param[in] _component_class_id
    @parblock
    C identifier.

    ID of the filter component class, within the plugin having the ID
    \bt_p{_plugin_id}, of which to set the "get supported MIP versions"
    method to \bt_p{_method}.
    @endparblock
@param[in] _method
    @parblock
    #bt_component_class_filter_get_supported_mip_versions_method

    Filter component class's "get supported MIP versions" method.
    @endparblock

@bt_pre_not_null{_method}
*/
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD_WITH_ID(_plugin_id, _component_class_id, _method) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(filter_get_supported_mip_versions_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_GET_SUPPORTED_MIP_VERSIONS_METHOD, _plugin_id, _component_class_id, filter, _method)

/*!
@brief
    Alias of
    BT_PLUGIN_FILTER_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD_WITH_ID()
    with the \bt_p{_plugin_id} parameter set to <code>auto</code> and
    the \bt_p{_component_class_id} parameter set to \bt_p{_name}.
*/
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD(_name, _method) \
	BT_PLUGIN_FILTER_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD_WITH_ID(auto, _name, _method)

/*!
@brief
    Sets the initialization method of the \bt_flt_comp_cls having the ID
    \bt_p{_component_class_id} in the plugin having the ID
    \bt_p{_plugin_id} to \bt_p{_method}.

See the \ref api-comp-cls-dev-meth-init "initialize" method.

@param[in] _plugin_id
    @parblock
    C identifier.

    ID of the plugin which contains the filter component class of which
    to set the initialization method.
    @endparblock
@param[in] _component_class_id
    @parblock
    C identifier.

    ID of the filter component class, within the plugin having the ID
    \bt_p{_plugin_id}, of which to set the initialization method to
    \bt_p{_method}.
    @endparblock
@param[in] _method
    @parblock
    #bt_component_class_filter_initialize_method

    Filter component class's initialization method.
    @endparblock

@bt_pre_not_null{_method}
*/
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_INITIALIZE_METHOD_WITH_ID(_plugin_id, _component_class_id, _method) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(filter_initialize_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_INITIALIZE_METHOD, _plugin_id, _component_class_id, filter, _method)

/*!
@brief
    Alias of
    BT_PLUGIN_FILTER_COMPONENT_CLASS_INITIALIZE_METHOD_WITH_ID()
    with the \bt_p{_plugin_id} parameter set to <code>auto</code> and
    the \bt_p{_component_class_id} parameter set to \bt_p{_name}.
*/
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_INITIALIZE_METHOD(_name, _method) \
	BT_PLUGIN_FILTER_COMPONENT_CLASS_INITIALIZE_METHOD_WITH_ID(auto, _name, _method)

/*!
@brief
    Sets the "input port connected" method of the \bt_flt_comp_cls
    having the ID \bt_p{_component_class_id} in the plugin having the ID
    \bt_p{_plugin_id} to \bt_p{_method}.

See the
\ref api-comp-cls-dev-meth-iport-connected "input port connected"
method.

@param[in] _plugin_id
    @parblock
    C identifier.

    ID of the plugin which contains the filter component class of which
    to set the "input port connected" method.
    @endparblock
@param[in] _component_class_id
    @parblock
    C identifier.

    ID of the filter component class, within the plugin having the ID
    \bt_p{_plugin_id}, of which to set the "input port connected"
    method to \bt_p{_method}.
    @endparblock
@param[in] _method
    @parblock
    #bt_component_class_filter_input_port_connected_method

    Filter component class's "input port connected" method.
    @endparblock

@bt_pre_not_null{_method}
*/
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_INPUT_PORT_CONNECTED_METHOD_WITH_ID(_plugin_id, _component_class_id, _method) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(filter_input_port_connected_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_INPUT_PORT_CONNECTED_METHOD, _plugin_id, _component_class_id, filter, _method)

/*!
@brief
    Alias of
    BT_PLUGIN_FILTER_COMPONENT_CLASS_INPUT_PORT_CONNECTED_METHOD_WITH_ID()
    with the \bt_p{_plugin_id} parameter set to <code>auto</code> and
    the \bt_p{_component_class_id} parameter set to \bt_p{_name}.
*/
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_INPUT_PORT_CONNECTED_METHOD(_name, _method) \
	BT_PLUGIN_FILTER_COMPONENT_CLASS_INPUT_PORT_CONNECTED_METHOD_WITH_ID(auto, _name, _method)

/*!
@brief
    Sets the finalization method of the \bt_msg_iter_cls of the
    \bt_flt_comp_cls having the ID \bt_p{_component_class_id} in the
    plugin having the ID \bt_p{_plugin_id} to \bt_p{_method}.

See the \ref api-msg-iter-cls-meth-fini "finalize" method.

@param[in] _plugin_id
    @parblock
    C identifier.

    ID of the plugin which contains the filter component class of which
    to set the finalization method of the message iterator class.
    @endparblock
@param[in] _component_class_id
    @parblock
    C identifier.

    ID of the filter component class, within the plugin having the ID
    \bt_p{_plugin_id}, of which to set the finalization method of the
    message iterator class to \bt_p{_method}.
    @endparblock
@param[in] _method
    @parblock
    #bt_message_iterator_class_finalize_method

    Filter component class's message iterator class's finalization method.
    @endparblock

@bt_pre_not_null{_method}
*/
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_FINALIZE_METHOD_WITH_ID(_plugin_id, _component_class_id, _method) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(msg_iter_finalize_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_MSG_ITER_FINALIZE_METHOD, _plugin_id, _component_class_id, filter, _method)

/*!
@brief
    Alias of
    BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_FINALIZE_METHOD_WITH_ID()
    with the \bt_p{_plugin_id} parameter set to <code>auto</code> and
    the \bt_p{_component_class_id} parameter set to \bt_p{_name}.
*/
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_FINALIZE_METHOD(_name, _method) \
	BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_FINALIZE_METHOD_WITH_ID(auto, _name, _method)

/*!
@brief
    Sets the initialization method of the \bt_msg_iter_cls of the
    \bt_flt_comp_cls having the ID \bt_p{_component_class_id} in the
    plugin having the ID \bt_p{_plugin_id} to \bt_p{_method}.

See the \ref api-msg-iter-cls-meth-init "initialize" method.

@param[in] _plugin_id
    @parblock
    C identifier.

    ID of the plugin which contains the filter component class of which
    to set the initialization method of the message iterator class.
    @endparblock
@param[in] _component_class_id
    @parblock
    C identifier.

    ID of the filter component class, within the plugin having the ID
    \bt_p{_plugin_id}, of which to set the initialization method of the
    message iterator class to \bt_p{_method}.
    @endparblock
@param[in] _method
    @parblock
    #bt_message_iterator_class_initialize_method

    Filter component class's message iterator class's initialization
    method.
    @endparblock

@bt_pre_not_null{_method}
*/
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_WITH_ID(_plugin_id, _component_class_id, _method) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(msg_iter_initialize_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_MSG_ITER_INITIALIZE_METHOD, _plugin_id, _component_class_id, filter, _method)

/*!
@brief
    Alias of
    BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_WITH_ID()
    with the \bt_p{_plugin_id} parameter set to <code>auto</code> and
    the \bt_p{_component_class_id} parameter set to \bt_p{_name}.
*/
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD(_name, _method) \
	BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_WITH_ID(auto, _name, _method)

/*!
@brief
    Sets the "seek beginning" and "can seek beginning?" methods of the
    \bt_msg_iter_cls of the \bt_flt_comp_cls having the ID
    \bt_p{_component_class_id} in the plugin having the ID
    \bt_p{_plugin_id} to \bt_p{_seek_method} and
    \bt_p{_can_seek_method}.

See the \ref api-msg-iter-cls-meth-seek-beg "seek beginning" and
\ref api-msg-iter-cls-meth-can-seek-beg "can seek beginning?" methods.

@param[in] _plugin_id
    @parblock
    C identifier.

    ID of the plugin which contains the filter component class of which
    to set the "seek beginning" and "can seek beginning?" methods of the
    message iterator class.
    @endparblock
@param[in] _component_class_id
    @parblock
    C identifier.

    ID of the filter component class, within the plugin having the ID
    \bt_p{_plugin_id}, of which to set the "seek beginning" and "can
    seek beginning" methods of the message iterator class to
    \bt_p{_seek_method} and \bt_p{_can_seek_method}.
    @endparblock
@param[in] _seek_method
    @parblock
    #bt_message_iterator_class_seek_beginning_method

    Filter component class's message iterator class's "seek beginning"
    method.
    @endparblock
@param[in] _can_seek_method
    @parblock
    #bt_message_iterator_class_can_seek_beginning_method

    Filter component class's message iterator class's
    "can seek beginning?" method.

    Can be \c NULL, in which case it is equivalent to setting a method
    which always returns #BT_TRUE.
    @endparblock

@bt_pre_not_null{_seek_method}
@bt_pre_not_null{_can_seek_method}
*/
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHODS_WITH_ID(_plugin_id, _component_class_id, _seek_method, _can_seek_method) \
		__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(msg_iter_seek_beginning_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_MSG_ITER_SEEK_BEGINNING_METHOD, _plugin_id, _component_class_id, filter, _seek_method); \
		__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(msg_iter_can_seek_beginning_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_MSG_ITER_CAN_SEEK_BEGINNING_METHOD, _plugin_id, _component_class_id, filter, _can_seek_method);

/*!
@brief
    Alias of
    BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHODS_WITH_ID()
    with the \bt_p{_plugin_id} parameter set to <code>auto</code> and
    the \bt_p{_component_class_id} parameter set to \bt_p{_name}.
*/
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHODS(_name, _seek_method, _can_seek_method) \
	BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHODS_WITH_ID(auto, _name, _seek_method, _can_seek_method)

/*!
@brief
    Sets the "seek ns from origin" and "can seek ns from origin?"
    methods of the \bt_msg_iter_cls of the \bt_flt_comp_cls having the
    ID \bt_p{_component_class_id} in the plugin having the ID
    \bt_p{_plugin_id} to \bt_p{_seek_method} and
    \bt_p{_can_seek_method}.

See the \ref api-msg-iter-cls-meth-seek-ns "seek ns from origin" and
\ref api-msg-iter-cls-meth-can-seek-ns "can seek ns from origin?"
methods.

@param[in] _plugin_id
    @parblock
    C identifier.

    ID of the plugin which contains the filter component class of which
    to set the "seek ns from origin" and "can seek ns from origin?"
    methods of the message iterator class.
    @endparblock
@param[in] _component_class_id
    @parblock
    C identifier.

    ID of the filter component class, within the plugin having the ID
    \bt_p{_plugin_id}, of which to set the "seek ns from origin" and
    "can seek ns from origin" methods of the message iterator class to
    \bt_p{_seek_method} and \bt_p{_can_seek_method}.
    @endparblock
@param[in] _seek_method
    @parblock
    #bt_message_iterator_class_seek_ns_from_origin_method

    Filter component class's message iterator class's "seek ns from
    origin" method.
    @endparblock
@param[in] _can_seek_method
    @parblock
    #bt_message_iterator_class_can_seek_ns_from_origin_method

    Filter component class's message iterator class's "can seek ns from
    origin?" method.

    Can be \c NULL, in which case it is equivalent to setting a method
    which always returns #BT_TRUE.
    @endparblock

@bt_pre_not_null{_seek_method}
@bt_pre_not_null{_can_seek_method}
*/
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_SEEK_NS_FROM_ORIGIN_METHODS_WITH_ID(_plugin_id, _component_class_id, _seek_method, _can_seek_method) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(msg_iter_seek_ns_from_origin_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_MSG_ITER_SEEK_NS_FROM_ORIGIN_METHOD, _plugin_id, _component_class_id, filter, _seek_method); \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(msg_iter_can_seek_ns_from_origin_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_MSG_ITER_CAN_SEEK_NS_FROM_ORIGIN_METHOD, _plugin_id, _component_class_id, filter, _can_seek_method)

/*!
@brief
    Alias of
    BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_SEEK_NS_FROM_ORIGIN_METHODS_WITH_ID()
    with the \bt_p{_plugin_id} parameter set to <code>auto</code> and
    the \bt_p{_component_class_id} parameter set to \bt_p{_name}.
*/
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_SEEK_NS_FROM_ORIGIN_METHODS(_name, _seek_method, _can_seek_method) \
	BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_SEEK_NS_FROM_ORIGIN_METHODS_WITH_ID(auto, _name, _seek_method, _can_seek_method)

/*!
@brief
    Sets the "output port connected" method of the \bt_flt_comp_cls
    having the ID \bt_p{_component_class_id} in the plugin having the ID
    \bt_p{_plugin_id} to \bt_p{_method}.

See the
\ref api-comp-cls-dev-meth-oport-connected "output port connected"
method.

@param[in] _plugin_id
    @parblock
    C identifier.

    ID of the plugin which contains the filter component class of which
    to set the "output port connected" method.
    @endparblock
@param[in] _component_class_id
    @parblock
    C identifier.

    ID of the filter component class, within the plugin having the ID
    \bt_p{_plugin_id}, of which to set the "output port connected"
    method to \bt_p{_method}.
    @endparblock
@param[in] _method
    @parblock
    #bt_component_class_filter_output_port_connected_method

    Filter component class's "output port connected" method.
    @endparblock

@bt_pre_not_null{_method}
*/
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_OUTPUT_PORT_CONNECTED_METHOD_WITH_ID(_plugin_id, _component_class_id, _method) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(filter_output_port_connected_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_OUTPUT_PORT_CONNECTED_METHOD, _plugin_id, _component_class_id, filter, _method)

/*!
@brief
    Alias of
    BT_PLUGIN_FILTER_COMPONENT_CLASS_OUTPUT_PORT_CONNECTED_METHOD_WITH_ID()
    with the \bt_p{_plugin_id} parameter set to <code>auto</code> and
    the \bt_p{_component_class_id} parameter set to \bt_p{_name}.
*/
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_OUTPUT_PORT_CONNECTED_METHOD(_name, _method) \
	BT_PLUGIN_FILTER_COMPONENT_CLASS_OUTPUT_PORT_CONNECTED_METHOD_WITH_ID(auto, _name, _method)

/*!
@brief
    Sets the query method of the \bt_flt_comp_cls
    having the ID \bt_p{_component_class_id} in the plugin having the ID
    \bt_p{_plugin_id} to \bt_p{_method}.

See the \ref api-comp-cls-dev-meth-query "query" method.

@param[in] _plugin_id
    @parblock
    C identifier.

    ID of the plugin which contains the filter component class of which
    to set the query method.
    @endparblock
@param[in] _component_class_id
    @parblock
    C identifier.

    ID of the filter component class, within the plugin having the ID
    \bt_p{_plugin_id}, of which to set the query
    method to \bt_p{_method}.
    @endparblock
@param[in] _method
    @parblock
    #bt_component_class_filter_query_method

    Filter component class's query method.
    @endparblock

@bt_pre_not_null{_method}
*/
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_QUERY_METHOD_WITH_ID(_plugin_id, _component_class_id, _method) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(filter_query_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_QUERY_METHOD, _plugin_id, _component_class_id, filter, _method)

/*!
@brief
    Alias of
    BT_PLUGIN_FILTER_COMPONENT_CLASS_QUERY_METHOD_WITH_ID()
    with the \bt_p{_plugin_id} parameter set to <code>auto</code> and
    the \bt_p{_component_class_id} parameter set to \bt_p{_name}.
*/
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_QUERY_METHOD(_name, _method) \
	BT_PLUGIN_FILTER_COMPONENT_CLASS_QUERY_METHOD_WITH_ID(auto, _name, _method)

/*! @} */

/*!
@name Sink component class methods
@{
*/

/*!
@brief
    Sets the finalization method of the \bt_sink_comp_cls having the ID
    \bt_p{_component_class_id} in the plugin having the ID
    \bt_p{_plugin_id} to \bt_p{_method}.

See the \ref api-comp-cls-dev-meth-fini "finalize" method.

@param[in] _plugin_id
    @parblock
    C identifier.

    ID of the plugin which contains the sink component class of which
    to set the finalization method.
    @endparblock
@param[in] _component_class_id
    @parblock
    C identifier.

    ID of the sink component class, within the plugin having the ID
    \bt_p{_plugin_id}, of which to set the finalization method to
    \bt_p{_method}.
    @endparblock
@param[in] _method
    @parblock
    #bt_component_class_sink_finalize_method

    Sink component class's finalization method.
    @endparblock

@bt_pre_not_null{_method}
*/
#define BT_PLUGIN_SINK_COMPONENT_CLASS_FINALIZE_METHOD_WITH_ID(_plugin_id, _component_class_id, _method) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(sink_finalize_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_FINALIZE_METHOD, _plugin_id, _component_class_id, sink, _method)

/*!
@brief
    Alias of
    BT_PLUGIN_SINK_COMPONENT_CLASS_FINALIZE_METHOD_WITH_ID()
    with the \bt_p{_plugin_id} parameter set to <code>auto</code> and
    the \bt_p{_component_class_id} parameter set to \bt_p{_name}.
*/
#define BT_PLUGIN_SINK_COMPONENT_CLASS_FINALIZE_METHOD(_name, _method) \
	BT_PLUGIN_SINK_COMPONENT_CLASS_FINALIZE_METHOD_WITH_ID(auto, _name, _method)

/*!
@brief
    Sets the \"get supported \bt_mip versions\" method of the
    \bt_sink_comp_cls having the ID \bt_p{_component_class_id} in the
    plugin having the ID \bt_p{_plugin_id} to \bt_p{_method}.

See the \ref api-comp-cls-dev-meth-mip "get supported MIP versions"
method.

@param[in] _plugin_id
    @parblock
    C identifier.

    ID of the plugin which contains the sink component class of which
    to set the "get supported MIP versions" method.
    @endparblock
@param[in] _component_class_id
    @parblock
    C identifier.

    ID of the sink component class, within the plugin having the ID
    \bt_p{_plugin_id}, of which to set the "get supported MIP versions"
    method to \bt_p{_method}.
    @endparblock
@param[in] _method
    @parblock
    #bt_component_class_sink_get_supported_mip_versions_method

    Sink component class's "get supported MIP versions" method.
    @endparblock

@bt_pre_not_null{_method}
*/
#define BT_PLUGIN_SINK_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD_WITH_ID(_plugin_id, _component_class_id, _method) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(sink_get_supported_mip_versions_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_GET_SUPPORTED_MIP_VERSIONS_METHOD, _plugin_id, _component_class_id, sink, _method)

/*!
@brief
    Alias of
    BT_PLUGIN_SINK_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD_WITH_ID()
    with the \bt_p{_plugin_id} parameter set to <code>auto</code> and
    the \bt_p{_component_class_id} parameter set to \bt_p{_name}.
*/
#define BT_PLUGIN_SINK_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD(_name, _method) \
	BT_PLUGIN_SINK_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD_WITH_ID(auto, _name, _method)

/*!
@brief
    Sets the "graph is configured" method of the \bt_sink_comp_cls
    having the ID \bt_p{_component_class_id} in the plugin having the ID
    \bt_p{_plugin_id} to \bt_p{_method}.

See the
\ref api-comp-cls-dev-meth-graph-configured "graph is configured"
method.

@param[in] _plugin_id
    @parblock
    C identifier.

    ID of the plugin which contains the sink component class of which
    to set the "graph is configured" method.
    @endparblock
@param[in] _component_class_id
    @parblock
    C identifier.

    ID of the sink component class, within the plugin having the ID
    \bt_p{_plugin_id}, of which to set the "graph is configured"
    method to \bt_p{_method}.
    @endparblock
@param[in] _method
    @parblock
    #bt_component_class_sink_graph_is_configured_method

    Sink component class's "graph is configured" method.
    @endparblock

@bt_pre_not_null{_method}
*/
#define BT_PLUGIN_SINK_COMPONENT_CLASS_GRAPH_IS_CONFIGURED_METHOD_WITH_ID(_plugin_id, _component_class_id, _method) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(sink_graph_is_configured_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_GRAPH_IS_CONFIGURED_METHOD, _plugin_id, _component_class_id, sink, _method)

/*!
@brief
    Alias of
    BT_PLUGIN_SINK_COMPONENT_CLASS_GRAPH_IS_CONFIGURED_METHOD_WITH_ID()
    with the \bt_p{_plugin_id} parameter set to <code>auto</code> and
    the \bt_p{_component_class_id} parameter set to \bt_p{_name}.
*/
#define BT_PLUGIN_SINK_COMPONENT_CLASS_GRAPH_IS_CONFIGURED_METHOD(_name, _method) \
	BT_PLUGIN_SINK_COMPONENT_CLASS_GRAPH_IS_CONFIGURED_METHOD_WITH_ID(auto, _name, _method)

/*!
@brief
    Sets the initialization method of the \bt_sink_comp_cls having the
    ID \bt_p{_component_class_id} in the plugin having the ID
    \bt_p{_plugin_id} to \bt_p{_method}.

See the \ref api-comp-cls-dev-meth-init "initialize" method.

@param[in] _plugin_id
    @parblock
    C identifier.

    ID of the plugin which contains the sink component class of which
    to set the initialization method.
    @endparblock
@param[in] _component_class_id
    @parblock
    C identifier.

    ID of the sink component class, within the plugin having the ID
    \bt_p{_plugin_id}, of which to set the initialization method to
    \bt_p{_method}.
    @endparblock
@param[in] _method
    @parblock
    #bt_component_class_sink_initialize_method

    Sink component class's initialization method.
    @endparblock

@bt_pre_not_null{_method}
*/
#define BT_PLUGIN_SINK_COMPONENT_CLASS_INITIALIZE_METHOD_WITH_ID(_plugin_id, _component_class_id, _method) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(sink_initialize_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_INITIALIZE_METHOD, _plugin_id, _component_class_id, sink, _method)

/*!
@brief
    Alias of
    BT_PLUGIN_SINK_COMPONENT_CLASS_INITIALIZE_METHOD_WITH_ID()
    with the \bt_p{_plugin_id} parameter set to <code>auto</code> and
    the \bt_p{_component_class_id} parameter set to \bt_p{_name}.
*/
#define BT_PLUGIN_SINK_COMPONENT_CLASS_INITIALIZE_METHOD(_name, _method) \
	BT_PLUGIN_SINK_COMPONENT_CLASS_INITIALIZE_METHOD_WITH_ID(auto, _name, _method)

/*!
@brief
    Sets the "input port connected" method of the \bt_sink_comp_cls
    having the ID \bt_p{_component_class_id} in the plugin having the ID
    \bt_p{_plugin_id} to \bt_p{_method}.

See the
\ref api-comp-cls-dev-meth-iport-connected "input port connected"
method.

@param[in] _plugin_id
    @parblock
    C identifier.

    ID of the plugin which contains the sink component class of which
    to set the "input port connected" method.
    @endparblock
@param[in] _component_class_id
    @parblock
    C identifier.

    ID of the sink component class, within the plugin having the ID
    \bt_p{_plugin_id}, of which to set the "input port connected"
    method to \bt_p{_method}.
    @endparblock
@param[in] _method
    @parblock
    #bt_component_class_sink_input_port_connected_method

    Sink component class's "input port connected" method.
    @endparblock

@bt_pre_not_null{_method}
*/
#define BT_PLUGIN_SINK_COMPONENT_CLASS_INPUT_PORT_CONNECTED_METHOD_WITH_ID(_plugin_id, _component_class_id, _method) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(sink_input_port_connected_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_INPUT_PORT_CONNECTED_METHOD, _plugin_id, _component_class_id, sink, _method)

/*!
@brief
    Alias of
    BT_PLUGIN_SINK_COMPONENT_CLASS_INPUT_PORT_CONNECTED_METHOD_WITH_ID()
    with the \bt_p{_plugin_id} parameter set to <code>auto</code> and
    the \bt_p{_component_class_id} parameter set to \bt_p{_name}.
*/
#define BT_PLUGIN_SINK_COMPONENT_CLASS_INPUT_PORT_CONNECTED_METHOD(_name, _method) \
	BT_PLUGIN_SINK_COMPONENT_CLASS_INPUT_PORT_CONNECTED_METHOD_WITH_ID(auto, _name, _method)

/*!
@brief
    Sets the query method of the \bt_sink_comp_cls
    having the ID \bt_p{_component_class_id} in the plugin having the ID
    \bt_p{_plugin_id} to \bt_p{_method}.

See the \ref api-comp-cls-dev-meth-query "query" method.

@param[in] _plugin_id
    @parblock
    C identifier.

    ID of the plugin which contains the sink component class of which
    to set the query method.
    @endparblock
@param[in] _component_class_id
    @parblock
    C identifier.

    ID of the sink component class, within the plugin having the ID
    \bt_p{_plugin_id}, of which to set the query
    method to \bt_p{_method}.
    @endparblock
@param[in] _method
    @parblock
    #bt_component_class_sink_query_method

    Sink component class's query method.
    @endparblock

@bt_pre_not_null{_method}
*/
#define BT_PLUGIN_SINK_COMPONENT_CLASS_QUERY_METHOD_WITH_ID(_plugin_id, _component_class_id, _method) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(sink_query_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_QUERY_METHOD, _plugin_id, _component_class_id, sink, _method)

/*!
@brief
    Alias of
    BT_PLUGIN_SINK_COMPONENT_CLASS_QUERY_METHOD_WITH_ID()
    with the \bt_p{_plugin_id} parameter set to <code>auto</code> and
    the \bt_p{_component_class_id} parameter set to \bt_p{_name}.
*/
#define BT_PLUGIN_SINK_COMPONENT_CLASS_QUERY_METHOD(_name, _method) \
	BT_PLUGIN_SINK_COMPONENT_CLASS_QUERY_METHOD_WITH_ID(auto, _name, _method)

/*! @} */

/*! @} */

/* Plugin descriptor: describes a single plugin (internal use) */
struct __bt_plugin_descriptor {
	/* Plugin's name */
	const char *name;
} __attribute__((packed));

/* Type of a plugin attribute (internal use) */
enum __bt_plugin_descriptor_attribute_type {
	BT_PLUGIN_DESCRIPTOR_ATTRIBUTE_TYPE_INIT		= 0,
	BT_PLUGIN_DESCRIPTOR_ATTRIBUTE_TYPE_EXIT		= 1,
	BT_PLUGIN_DESCRIPTOR_ATTRIBUTE_TYPE_AUTHOR		= 2,
	BT_PLUGIN_DESCRIPTOR_ATTRIBUTE_TYPE_LICENSE		= 3,
	BT_PLUGIN_DESCRIPTOR_ATTRIBUTE_TYPE_DESCRIPTION		= 4,
	BT_PLUGIN_DESCRIPTOR_ATTRIBUTE_TYPE_VERSION		= 5,
};

/* Plugin (user) version */
struct __bt_plugin_descriptor_version {
	uint32_t major;
	uint32_t minor;
	uint32_t patch;
	const char *extra;
};

/* Plugin attribute (internal use) */
struct __bt_plugin_descriptor_attribute {
	/* Plugin descriptor to which to associate this attribute */
	const struct __bt_plugin_descriptor *plugin_descriptor;

	/* Name of the attribute's type for debug purposes */
	const char *type_name;

	/* Attribute's type */
	enum __bt_plugin_descriptor_attribute_type type;

	/* Attribute's value (depends on attribute's type) */
	union {
		/* BT_PLUGIN_DESCRIPTOR_ATTRIBUTE_TYPE_INIT */
		bt_plugin_initialize_func init;

		/* BT_PLUGIN_DESCRIPTOR_ATTRIBUTE_TYPE_EXIT */
		bt_plugin_finalize_func exit;

		/* BT_PLUGIN_DESCRIPTOR_ATTRIBUTE_TYPE_AUTHOR */
		const char *author;

		/* BT_PLUGIN_DESCRIPTOR_ATTRIBUTE_TYPE_LICENSE */
		const char *license;

		/* BT_PLUGIN_DESCRIPTOR_ATTRIBUTE_TYPE_DESCRIPTION */
		const char *description;

		/* BT_PLUGIN_DESCRIPTOR_ATTRIBUTE_TYPE_VERSION */
		struct __bt_plugin_descriptor_version version;
	} value;
} __attribute__((packed));

/* Component class descriptor (internal use) */
struct __bt_plugin_component_class_descriptor {
	/*
	 * Plugin descriptor to which to associate this component
	 * class descriptor.
	 */
	const struct __bt_plugin_descriptor *plugin_descriptor;

	/* Component class name */
	const char *name;

	/* Component class type */
	bt_component_class_type type;

	/* Mandatory methods (depends on component class type) */
	union {
		/* BT_COMPONENT_CLASS_TYPE_SOURCE */
		struct {
			bt_message_iterator_class_next_method msg_iter_next;
		} source;

		/* BT_COMPONENT_CLASS_TYPE_FILTER */
		struct {
			bt_message_iterator_class_next_method msg_iter_next;
		} filter;

		/* BT_COMPONENT_CLASS_TYPE_SINK */
		struct {
			bt_component_class_sink_consume_method consume;
		} sink;
	} methods;
} __attribute__((packed));

/* Type of a component class attribute (internal use) */
enum __bt_plugin_component_class_descriptor_attribute_type {
	BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_DESCRIPTION					= 0,
	BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_HELP					= 1,
	BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_GET_SUPPORTED_MIP_VERSIONS_METHOD		= 2,
	BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_INITIALIZE_METHOD					= 3,
	BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_FINALIZE_METHOD				= 4,
	BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_QUERY_METHOD				= 5,
	BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_INPUT_PORT_CONNECTED_METHOD			= 6,
	BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_OUTPUT_PORT_CONNECTED_METHOD		= 7,
	BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_GRAPH_IS_CONFIGURED_METHOD			= 8,
	BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_MSG_ITER_INITIALIZE_METHOD			= 9,
	BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_MSG_ITER_FINALIZE_METHOD			= 10,
	BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_MSG_ITER_SEEK_NS_FROM_ORIGIN_METHOD		= 11,
	BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_MSG_ITER_SEEK_BEGINNING_METHOD		= 12,
	BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_MSG_ITER_CAN_SEEK_NS_FROM_ORIGIN_METHOD	= 13,
	BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_MSG_ITER_CAN_SEEK_BEGINNING_METHOD		= 14,
};

/* Component class attribute (internal use) */
struct __bt_plugin_component_class_descriptor_attribute {
	/*
	 * Component class plugin attribute to which to associate this
	 * component class attribute.
	 */
	const struct __bt_plugin_component_class_descriptor *comp_class_descriptor;

	/* Name of the attribute's type for debug purposes */
	const char *type_name;

	/* Attribute's type */
	enum __bt_plugin_component_class_descriptor_attribute_type type;

	/* Attribute's value (depends on attribute's type) */
	union {
		/* BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_DESCRIPTION */
		const char *description;

		/* BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_HELP */
		const char *help;

		/* BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_GET_SUPPORTED_MIP_VERSIONS_METHOD */
		bt_component_class_source_get_supported_mip_versions_method source_get_supported_mip_versions_method;
		bt_component_class_filter_get_supported_mip_versions_method filter_get_supported_mip_versions_method;
		bt_component_class_sink_get_supported_mip_versions_method sink_get_supported_mip_versions_method;

		/* BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_INITIALIZE_METHOD */
		bt_component_class_source_initialize_method source_initialize_method;
		bt_component_class_filter_initialize_method filter_initialize_method;
		bt_component_class_sink_initialize_method sink_initialize_method;

		/* BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_FINALIZE_METHOD */
		bt_component_class_source_finalize_method source_finalize_method;
		bt_component_class_filter_finalize_method filter_finalize_method;
		bt_component_class_sink_finalize_method sink_finalize_method;

		/* BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_QUERY_METHOD */
		bt_component_class_source_query_method source_query_method;
		bt_component_class_filter_query_method filter_query_method;
		bt_component_class_sink_query_method sink_query_method;

		/* BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_INPUT_PORT_CONNECTED_METHOD */
		bt_component_class_filter_input_port_connected_method filter_input_port_connected_method;
		bt_component_class_sink_input_port_connected_method sink_input_port_connected_method;

		/* BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_OUTPUT_PORT_CONNECTED_METHOD */
		bt_component_class_source_output_port_connected_method source_output_port_connected_method;
		bt_component_class_filter_output_port_connected_method filter_output_port_connected_method;

		/* BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_GRAPH_IS_CONFIGURED_METHOD */
		bt_component_class_sink_graph_is_configured_method sink_graph_is_configured_method;

		/* BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_MSG_ITER_INITIALIZE_METHOD */
		bt_message_iterator_class_initialize_method msg_iter_initialize_method;

		/* BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_MSG_ITER_FINALIZE_METHOD */
		bt_message_iterator_class_finalize_method msg_iter_finalize_method;

		/* BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_MSG_ITER_SEEK_NS_FROM_ORIGIN_METHOD */
		bt_message_iterator_class_seek_ns_from_origin_method msg_iter_seek_ns_from_origin_method;

		/* BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_MSG_ITER_SEEK_BEGINNING_METHOD */
		bt_message_iterator_class_seek_beginning_method msg_iter_seek_beginning_method;

		/* BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_MSG_ITER_CAN_SEEK_NS_FROM_ORIGIN_METHOD */
		bt_message_iterator_class_can_seek_ns_from_origin_method msg_iter_can_seek_ns_from_origin_method;

		/* BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_MSG_ITER_CAN_SEEK_BEGINNING_METHOD */
		bt_message_iterator_class_can_seek_beginning_method msg_iter_can_seek_beginning_method;
	} value;
} __attribute__((packed));

struct __bt_plugin_descriptor const * const *__bt_get_begin_section_plugin_descriptors(void);
struct __bt_plugin_descriptor const * const *__bt_get_end_section_plugin_descriptors(void);
struct __bt_plugin_descriptor_attribute const * const *__bt_get_begin_section_plugin_descriptor_attributes(void);
struct __bt_plugin_descriptor_attribute const * const *__bt_get_end_section_plugin_descriptor_attributes(void);
struct __bt_plugin_component_class_descriptor const * const *__bt_get_begin_section_component_class_descriptors(void);
struct __bt_plugin_component_class_descriptor const * const *__bt_get_end_section_component_class_descriptors(void);
struct __bt_plugin_component_class_descriptor_attribute const * const *__bt_get_begin_section_component_class_descriptor_attributes(void);
struct __bt_plugin_component_class_descriptor_attribute const * const *__bt_get_end_section_component_class_descriptor_attributes(void);

/*
 * Defines a plugin attribute (generic, internal use).
 *
 * _attr_name: Name of the attribute (C identifier).
 * _attr_type: Type of the attribute (enum __bt_plugin_descriptor_attribute_type).
 * _id:        Plugin descriptor ID (C identifier).
 * _x:         Value.
 */
#define __BT_PLUGIN_DESCRIPTOR_ATTRIBUTE(_attr_name, _attr_type, _id, _x) \
	static struct __bt_plugin_descriptor_attribute __bt_plugin_descriptor_attribute_##_id##_##_attr_name = { \
		.plugin_descriptor = &__bt_plugin_descriptor_##_id,	\
		.type_name = #_attr_name,				\
		.type = _attr_type,					\
		.value = { ._attr_name = _x },				\
	};								\
	static struct __bt_plugin_descriptor_attribute const * const __bt_plugin_descriptor_attribute_##_id##_##_attr_name##_ptr __BT_PLUGIN_DESCRIPTOR_ATTRIBUTES_ATTRS = &__bt_plugin_descriptor_attribute_##_id##_##_attr_name

#define __BT_PLUGIN_VERSION_STRUCT_VALUE(_major, _minor, _patch, _extra) \
	{.major = _major, .minor = _minor, .patch = _patch, .extra = _extra,}

/*
 * Defines a component class descriptor attribute (generic, internal
 * use).
 *
 * _id:            Plugin descriptor ID (C identifier).
 * _component_class_id: Component class ID (C identifier).
 * _type:          Component class type (`source`, `filter`, or `sink`).
 * _attr_name:     Name of the attribute (C identifier).
 * _attr_type:     Type of the attribute
 *                 (enum __bt_plugin_descriptor_attribute_type).
 * _x:             Value.
 */
#define __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(_attr_name, _attr_type, _id, _component_class_id, _type, _x) \
	static struct __bt_plugin_component_class_descriptor_attribute __bt_plugin_##_type##_component_class_descriptor_attribute_##_id##_##_component_class_id##_##_attr_name = { \
		.comp_class_descriptor = &__bt_plugin_##_type##_component_class_descriptor_##_id##_##_component_class_id, \
		.type_name = #_attr_name,				\
		.type = _attr_type,					\
		.value = { ._attr_name = _x },				\
	};								\
	static struct __bt_plugin_component_class_descriptor_attribute const * const __bt_plugin_##_type##_component_class_descriptor_attribute_##_id##_##_component_class_id##_##_attr_name##_ptr __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTES_ATTRS = &__bt_plugin_##_type##_component_class_descriptor_attribute_##_id##_##_component_class_id##_##_attr_name

/*
 * Clang supports the no_sanitize variable attribute on global variables.
 * GCC only supports the no_sanitize_address function attribute, which is
 * not what we need. This is fine because, as far as we have seen, gcc
 * does not insert red zones around global variables.
 */
#if defined(__clang__)
# if __has_feature(address_sanitizer)
#  define __bt_plugin_variable_attribute_no_sanitize_address \
	__attribute__((no_sanitize("address")))
# else
#  define __bt_plugin_variable_attribute_no_sanitize_address
# endif
#else
#  define __bt_plugin_variable_attribute_no_sanitize_address
#endif

/*
 * Variable attributes for a plugin descriptor pointer to be added to
 * the plugin descriptor section (internal use).
 */
#ifdef __APPLE__
#define __BT_PLUGIN_DESCRIPTOR_ATTRS \
	__attribute__((section("__DATA,btp_desc"), used)) \
	__bt_plugin_variable_attribute_no_sanitize_address

#define __BT_PLUGIN_DESCRIPTOR_BEGIN_SYMBOL \
	__start___bt_plugin_descriptors

#define __BT_PLUGIN_DESCRIPTOR_END_SYMBOL \
	__stop___bt_plugin_descriptors

#define __BT_PLUGIN_DESCRIPTOR_BEGIN_EXTRA \
	__asm("section$start$__DATA$btp_desc")

#define __BT_PLUGIN_DESCRIPTOR_END_EXTRA \
	__asm("section$end$__DATA$btp_desc")

#else

#define __BT_PLUGIN_DESCRIPTOR_ATTRS \
	__attribute__((section("__bt_plugin_descriptors"), used)) \
	__bt_plugin_variable_attribute_no_sanitize_address

#define __BT_PLUGIN_DESCRIPTOR_BEGIN_SYMBOL \
	__start___bt_plugin_descriptors

#define __BT_PLUGIN_DESCRIPTOR_END_SYMBOL \
	__stop___bt_plugin_descriptors

#define __BT_PLUGIN_DESCRIPTOR_BEGIN_EXTRA

#define __BT_PLUGIN_DESCRIPTOR_END_EXTRA
#endif

/*
 * Variable attributes for a plugin attribute pointer to be added to
 * the plugin attribute section (internal use).
 */
#ifdef __APPLE__
#define __BT_PLUGIN_DESCRIPTOR_ATTRIBUTES_ATTRS \
	__attribute__((section("__DATA,btp_desc_att"), used)) \
	__bt_plugin_variable_attribute_no_sanitize_address

#define __BT_PLUGIN_DESCRIPTOR_ATTRIBUTES_BEGIN_SYMBOL \
	__start___bt_plugin_descriptor_attributes

#define __BT_PLUGIN_DESCRIPTOR_ATTRIBUTES_END_SYMBOL \
	__stop___bt_plugin_descriptor_attributes

#define __BT_PLUGIN_DESCRIPTOR_ATTRIBUTES_BEGIN_EXTRA \
	__asm("section$start$__DATA$btp_desc_att")

#define __BT_PLUGIN_DESCRIPTOR_ATTRIBUTES_END_EXTRA \
	__asm("section$end$__DATA$btp_desc_att")

#else

#define __BT_PLUGIN_DESCRIPTOR_ATTRIBUTES_ATTRS \
	__attribute__((section("__bt_plugin_descriptor_attributes"), used)) \
	__bt_plugin_variable_attribute_no_sanitize_address

#define __BT_PLUGIN_DESCRIPTOR_ATTRIBUTES_BEGIN_SYMBOL \
	__start___bt_plugin_descriptor_attributes

#define __BT_PLUGIN_DESCRIPTOR_ATTRIBUTES_END_SYMBOL \
	__stop___bt_plugin_descriptor_attributes

#define __BT_PLUGIN_DESCRIPTOR_ATTRIBUTES_BEGIN_EXTRA

#define __BT_PLUGIN_DESCRIPTOR_ATTRIBUTES_END_EXTRA
#endif

/*
 * Variable attributes for a component class descriptor pointer to be
 * added to the component class descriptor section (internal use).
 */
#ifdef __APPLE__
#define __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRS \
	__attribute__((section("__DATA,btp_cc_desc"), used)) \
	__bt_plugin_variable_attribute_no_sanitize_address

#define __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_BEGIN_SYMBOL \
	__start___bt_plugin_component_class_descriptors

#define __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_END_SYMBOL \
	__stop___bt_plugin_component_class_descriptors

#define __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_BEGIN_EXTRA \
	__asm("section$start$__DATA$btp_cc_desc")

#define __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_END_EXTRA \
	__asm("section$end$__DATA$btp_cc_desc")

#else

#define __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRS \
	__attribute__((section("__bt_plugin_component_class_descriptors"), used)) \
	__bt_plugin_variable_attribute_no_sanitize_address

#define __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_BEGIN_SYMBOL \
	__start___bt_plugin_component_class_descriptors

#define __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_END_SYMBOL \
	__stop___bt_plugin_component_class_descriptors

#define __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_BEGIN_EXTRA

#define __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_END_EXTRA
#endif

/*
 * Variable attributes for a component class descriptor attribute
 * pointer to be added to the component class descriptor attribute
 * section (internal use).
 */
#ifdef __APPLE__
#define __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTES_ATTRS \
	__attribute__((section("__DATA,btp_cc_desc_att"), used)) \
	__bt_plugin_variable_attribute_no_sanitize_address

#define __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTES_BEGIN_SYMBOL \
	__start___bt_plugin_component_class_descriptor_attributes

#define __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTES_END_SYMBOL \
	__stop___bt_plugin_component_class_descriptor_attributes

#define __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTES_BEGIN_EXTRA \
	__asm("section$start$__DATA$btp_cc_desc_att")

#define __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTES_END_EXTRA \
	__asm("section$end$__DATA$btp_cc_desc_att")

#else

#define __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTES_ATTRS \
	__attribute__((section("__bt_plugin_component_class_descriptor_attributes"), used)) \
	__bt_plugin_variable_attribute_no_sanitize_address

#define __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTES_BEGIN_SYMBOL \
	__start___bt_plugin_component_class_descriptor_attributes

#define __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTES_END_SYMBOL \
	__stop___bt_plugin_component_class_descriptor_attributes

#define __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTES_BEGIN_EXTRA

#define __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTES_END_EXTRA
#endif

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_PLUGIN_PLUGIN_DEV_H */
