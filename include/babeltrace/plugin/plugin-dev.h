#ifndef BABELTRACE_PLUGIN_PLUGIN_DEV_H
#define BABELTRACE_PLUGIN_PLUGIN_DEV_H

/*
 * This is the header that you need to include for the development of
 * a Babeltrace plug-in.
 *
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

/* For enum bt_plugin_status */
#include <babeltrace/plugin/plugin-const.h>

/* For bt_component_class_type */
#include <babeltrace/graph/component-class-const.h>

/* For component class method type definitions */
#include <babeltrace/graph/component-class-source.h>
#include <babeltrace/graph/component-class-filter.h>
#include <babeltrace/graph/component-class-sink.h>

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

/*
 * Plugin interface's version, not synced with Babeltrace's version
 * (internal use).
 */
#define __BT_PLUGIN_VERSION_MAJOR	1
#define __BT_PLUGIN_VERSION_MINOR	0

/* Plugin initialization function type */
typedef enum bt_self_plugin_status {
	BT_SELF_PLUGIN_STATUS_OK = 0,
	BT_SELF_PLUGIN_STATUS_NOMEM = -12,
	BT_SELF_PLUGIN_STATUS_ERROR = -1,
} bt_self_plugin_status;

typedef struct bt_self_plugin bt_self_plugin;

typedef bt_self_plugin_status (*bt_plugin_init_func)(
		bt_self_plugin *plugin);

/* Plugin exit function type */
typedef void (*bt_plugin_exit_func)(void);

/* Plugin descriptor: describes a single plugin (internal use) */
struct __bt_plugin_descriptor {
	/* Plugin's interface major version number */
	uint32_t major;

	/* Plugin's interface minor version number */
	uint32_t minor;

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
		bt_plugin_init_func init;

		/* BT_PLUGIN_DESCRIPTOR_ATTRIBUTE_TYPE_EXIT */
		bt_plugin_exit_func exit;

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
			bt_component_class_source_message_iterator_next_method msg_iter_next;
		} source;

		/* BT_COMPONENT_CLASS_TYPE_FILTER */
		struct {
			bt_component_class_filter_message_iterator_next_method msg_iter_next;
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
	BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_INIT_METHOD					= 2,
	BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_FINALIZE_METHOD				= 3,
	BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_QUERY_METHOD				= 4,
	BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_ACCEPT_INPUT_PORT_CONNECTION_METHOD		= 5,
	BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_ACCEPT_OUTPUT_PORT_CONNECTION_METHOD	= 6,
	BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_INPUT_PORT_CONNECTED_METHOD			= 7,
	BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_OUTPUT_PORT_CONNECTED_METHOD		= 8,
	BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_MSG_ITER_INIT_METHOD			= 11,
	BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_MSG_ITER_FINALIZE_METHOD			= 12,
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

		/* BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_INIT_METHOD */
		bt_component_class_source_init_method source_init_method;
		bt_component_class_filter_init_method filter_init_method;
		bt_component_class_sink_init_method sink_init_method;

		/* BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_FINALIZE_METHOD */
		bt_component_class_source_finalize_method source_finalize_method;
		bt_component_class_filter_finalize_method filter_finalize_method;
		bt_component_class_sink_finalize_method sink_finalize_method;

		/* BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_QUERY_METHOD */
		bt_component_class_source_query_method source_query_method;
		bt_component_class_filter_query_method filter_query_method;
		bt_component_class_sink_query_method sink_query_method;

		/* BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_ACCEPT_INPUT_PORT_CONNECTION_METHOD */
		bt_component_class_filter_accept_input_port_connection_method filter_accept_input_port_connection_method;
		bt_component_class_sink_accept_input_port_connection_method sink_accept_input_port_connection_method;

		/* BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_ACCEPT_OUTPUT_PORT_CONNECTION_METHOD */
		bt_component_class_source_accept_output_port_connection_method source_accept_output_port_connection_method;
		bt_component_class_filter_accept_output_port_connection_method filter_accept_output_port_connection_method;

		/* BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_INPUT_PORT_CONNECTED_METHOD */
		bt_component_class_filter_input_port_connected_method filter_input_port_connected_method;
		bt_component_class_sink_input_port_connected_method sink_input_port_connected_method;

		/* BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_OUTPUT_PORT_CONNECTED_METHOD */
		bt_component_class_source_output_port_connected_method source_output_port_connected_method;
		bt_component_class_filter_output_port_connected_method filter_output_port_connected_method;

		/* BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_MSG_ITER_INIT_METHOD */
		bt_component_class_source_message_iterator_init_method source_msg_iter_init_method;
		bt_component_class_filter_message_iterator_init_method filter_msg_iter_init_method;

		/* BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_MSG_ITER_FINALIZE_METHOD */
		bt_component_class_source_message_iterator_finalize_method source_msg_iter_finalize_method;
		bt_component_class_filter_message_iterator_finalize_method filter_msg_iter_finalize_method;
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
 * Variable attributes for a plugin descriptor pointer to be added to
 * the plugin descriptor section (internal use).
 */
#ifdef __APPLE__
#define __BT_PLUGIN_DESCRIPTOR_ATTRS \
	__attribute__((section("__DATA,btp_desc"), used))

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
	__attribute__((section("__bt_plugin_descriptors"), used))

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
	__attribute__((section("__DATA,btp_desc_att"), used))

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
	__attribute__((section("__bt_plugin_descriptor_attributes"), used))

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
	__attribute__((section("__DATA,btp_cc_desc"), used))

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
	__attribute__((section("__bt_plugin_component_class_descriptors"), used))

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
	__attribute__((section("__DATA,btp_cc_desc_att"), used))

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
	__attribute__((section("__bt_plugin_component_class_descriptor_attributes"), used))

#define __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTES_BEGIN_SYMBOL \
	__start___bt_plugin_component_class_descriptor_attributes

#define __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTES_END_SYMBOL \
	__stop___bt_plugin_component_class_descriptor_attributes

#define __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTES_BEGIN_EXTRA

#define __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTES_END_EXTRA
#endif

/*
 * Declares a plugin descriptor pointer variable with a custom ID.
 *
 * _id: ID (any valid C identifier except `auto`).
 */
#define BT_PLUGIN_DECLARE(_id) extern struct __bt_plugin_descriptor __bt_plugin_descriptor_##_id

/*
 * Defines a plugin descriptor with a custom ID.
 *
 * _id:   ID (any valid C identifier except `auto`).
 * _name: Plugin's name (C string).
 */
#define BT_PLUGIN_WITH_ID(_id, _name)					\
	struct __bt_plugin_descriptor __bt_plugin_descriptor_##_id = {	\
		.major = __BT_PLUGIN_VERSION_MAJOR,			\
		.minor = __BT_PLUGIN_VERSION_MINOR,			\
		.name = _name,						\
	};								\
	static struct __bt_plugin_descriptor const * const __bt_plugin_descriptor_##_id##_ptr __BT_PLUGIN_DESCRIPTOR_ATTRS = &__bt_plugin_descriptor_##_id

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
		.value._attr_name = _x,					\
	};								\
	static struct __bt_plugin_descriptor_attribute const * const __bt_plugin_descriptor_attribute_##_id##_##_attr_name##_ptr __BT_PLUGIN_DESCRIPTOR_ATTRIBUTES_ATTRS = &__bt_plugin_descriptor_attribute_##_id##_##_attr_name

/*
 * Defines a plugin initialization function attribute attached to a
 * specific plugin descriptor.
 *
 * _id: Plugin descriptor ID (C identifier).
 * _x:  Initialization function (bt_plugin_init_func).
 */
#define BT_PLUGIN_INIT_WITH_ID(_id, _x) \
	__BT_PLUGIN_DESCRIPTOR_ATTRIBUTE(init, BT_PLUGIN_DESCRIPTOR_ATTRIBUTE_TYPE_INIT, _id, _x)

/*
 * Defines a plugin exit function attribute attached to a specific
 * plugin descriptor.
 *
 * _id: Plugin descriptor ID (C identifier).
 * _x:  Exit function (bt_plugin_exit_func).
 */
#define BT_PLUGIN_EXIT_WITH_ID(_id, _x) \
	__BT_PLUGIN_DESCRIPTOR_ATTRIBUTE(exit, BT_PLUGIN_DESCRIPTOR_ATTRIBUTE_TYPE_EXIT, _id, _x)

/*
 * Defines an author attribute attached to a specific plugin descriptor.
 *
 * _id: Plugin descriptor ID (C identifier).
 * _x:  Author (C string).
 */
#define BT_PLUGIN_AUTHOR_WITH_ID(_id, _x) \
	__BT_PLUGIN_DESCRIPTOR_ATTRIBUTE(author, BT_PLUGIN_DESCRIPTOR_ATTRIBUTE_TYPE_AUTHOR, _id, _x)

/*
 * Defines a license attribute attached to a specific plugin descriptor.
 *
 * _id: Plugin descriptor ID (C identifier).
 * _x:  License (C string).
 */
#define BT_PLUGIN_LICENSE_WITH_ID(_id, _x) \
	__BT_PLUGIN_DESCRIPTOR_ATTRIBUTE(license, BT_PLUGIN_DESCRIPTOR_ATTRIBUTE_TYPE_LICENSE, _id, _x)

/*
 * Defines a description attribute attached to a specific plugin
 * descriptor.
 *
 * _id: Plugin descriptor ID (C identifier).
 * _x:  Description (C string).
 */
#define BT_PLUGIN_DESCRIPTION_WITH_ID(_id, _x) \
	__BT_PLUGIN_DESCRIPTOR_ATTRIBUTE(description, BT_PLUGIN_DESCRIPTOR_ATTRIBUTE_TYPE_DESCRIPTION, _id, _x)

#define __BT_PLUGIN_VERSION_STRUCT_VALUE(_major, _minor, _patch, _extra) \
	{.major = _major, .minor = _minor, .patch = _patch, .extra = _extra,}

/*
 * Defines a version attribute attached to a specific plugin descriptor.
 *
 * _id:    Plugin descriptor ID (C identifier).
 * _major: Plugin's major version (uint32_t).
 * _minor: Plugin's minor version (uint32_t).
 * _patch: Plugin's patch version (uint32_t).
 * _extra: Plugin's version extra information (C string).
 */
#define BT_PLUGIN_VERSION_WITH_ID(_id, _major, _minor, _patch, _extra) \
	__BT_PLUGIN_DESCRIPTOR_ATTRIBUTE(version, BT_PLUGIN_DESCRIPTOR_ATTRIBUTE_TYPE_VERSION, _id, __BT_PLUGIN_VERSION_STRUCT_VALUE(_major, _minor, _patch, _extra))

/*
 * Defines a source component class descriptor with a custom ID.
 *
 * _id:                     ID (any valid C identifier except `auto`).
 * _comp_class_id:          Component class ID (C identifier).
 * _name:                   Component class name (C string).
 * _msg_iter_next_method: Component class's iterator next method
 *                          (bt_component_class_source_message_iterator_next_method).
 */
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_WITH_ID(_id, _comp_class_id, _name, _msg_iter_next_method) \
	static struct __bt_plugin_component_class_descriptor __bt_plugin_source_component_class_descriptor_##_id##_##_comp_class_id = { \
		.plugin_descriptor = &__bt_plugin_descriptor_##_id,	\
		.name = _name,						\
		.type = BT_COMPONENT_CLASS_TYPE_SOURCE,			\
		.methods.source = {					\
			.msg_iter_next = _msg_iter_next_method,	\
		},							\
	};								\
	static struct __bt_plugin_component_class_descriptor const * const __bt_plugin_source_component_class_descriptor_##_id##_##_comp_class_id##_ptr __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRS = &__bt_plugin_source_component_class_descriptor_##_id##_##_comp_class_id

/*
 * Defines a filter component class descriptor with a custom ID.
 *
 * _id:                     ID (any valid C identifier except `auto`).
 * _comp_class_id:          Component class ID (C identifier).
 * _name:                   Component class name (C string).
 * _msg_iter_next_method: Component class's iterator next method
 *                          (bt_component_class_filter_message_iterator_next_method).
 */
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_WITH_ID(_id, _comp_class_id, _name, _msg_iter_next_method) \
	static struct __bt_plugin_component_class_descriptor __bt_plugin_filter_component_class_descriptor_##_id##_##_comp_class_id = { \
		.plugin_descriptor = &__bt_plugin_descriptor_##_id,	\
		.name = _name,						\
		.type = BT_COMPONENT_CLASS_TYPE_FILTER,			\
		.methods.filter = {					\
			.msg_iter_next = _msg_iter_next_method,	\
		},							\
	};								\
	static struct __bt_plugin_component_class_descriptor const * const __bt_plugin_filter_component_class_descriptor_##_id##_##_comp_class_id##_ptr __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRS = &__bt_plugin_filter_component_class_descriptor_##_id##_##_comp_class_id

/*
 * Defines a sink component class descriptor with a custom ID.
 *
 * _id:                 ID (any valid C identifier except `auto`).
 * _comp_class_id:      Component class ID (C identifier).
 * _name:               Component class name (C string).
 * _consume_method:     Component class's iterator consume method
 *                      (bt_component_class_sink_consume_method).
 */
#define BT_PLUGIN_SINK_COMPONENT_CLASS_WITH_ID(_id, _comp_class_id, _name, _consume_method) \
	static struct __bt_plugin_component_class_descriptor __bt_plugin_sink_component_class_descriptor_##_id##_##_comp_class_id = { \
		.plugin_descriptor = &__bt_plugin_descriptor_##_id,	\
		.name = _name,						\
		.type = BT_COMPONENT_CLASS_TYPE_SINK,			\
		.methods.sink = {					\
			.consume = _consume_method,			\
		},							\
	};								\
	static struct __bt_plugin_component_class_descriptor const * const __bt_plugin_sink_component_class_descriptor_##_id##_##_comp_class_id##_ptr __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRS = &__bt_plugin_sink_component_class_descriptor_##_id##_##_comp_class_id

/*
 * Defines a component class descriptor attribute (generic, internal
 * use).
 *
 * _id:            Plugin descriptor ID (C identifier).
 * _comp_class_id: Component class ID (C identifier).
 * _type:          Component class type (`source`, `filter`, or `sink`).
 * _attr_name:     Name of the attribute (C identifier).
 * _attr_type:     Type of the attribute
 *                 (enum __bt_plugin_descriptor_attribute_type).
 * _x:             Value.
 */
#define __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(_attr_name, _attr_type, _id, _comp_class_id, _type, _x) \
	static struct __bt_plugin_component_class_descriptor_attribute __bt_plugin_##_type##_component_class_descriptor_attribute_##_id##_##_comp_class_id##_##_attr_name = { \
		.comp_class_descriptor = &__bt_plugin_##_type##_component_class_descriptor_##_id##_##_comp_class_id, \
		.type_name = #_attr_name,				\
		.type = _attr_type,					\
		.value._attr_name = _x,					\
	};								\
	static struct __bt_plugin_component_class_descriptor_attribute const * const __bt_plugin_##_type##_component_class_descriptor_attribute_##_id##_##_comp_class_id##_##_attr_name##_ptr __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTES_ATTRS = &__bt_plugin_##_type##_component_class_descriptor_attribute_##_id##_##_comp_class_id##_##_attr_name

/*
 * Defines a description attribute attached to a specific source
 * component class descriptor.
 *
 * _id:            Plugin descriptor ID (C identifier).
 * _comp_class_id: Component class descriptor ID (C identifier).
 * _x:             Description (C string).
 */
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_DESCRIPTION_WITH_ID(_id, _comp_class_id, _x) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(description, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_DESCRIPTION, _id, _comp_class_id, source, _x)

/*
 * Defines a description attribute attached to a specific filter
 * component class descriptor.
 *
 * _id:            Plugin descriptor ID (C identifier).
 * _comp_class_id: Component class descriptor ID (C identifier).
 * _x:             Description (C string).
 */
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_DESCRIPTION_WITH_ID(_id, _comp_class_id, _x) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(description, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_DESCRIPTION, _id, _comp_class_id, filter, _x)

/*
 * Defines a description attribute attached to a specific sink
 * component class descriptor.
 *
 * _id:            Plugin descriptor ID (C identifier).
 * _comp_class_id: Component class descriptor ID (C identifier).
 * _x:             Description (C string).
 */
#define BT_PLUGIN_SINK_COMPONENT_CLASS_DESCRIPTION_WITH_ID(_id, _comp_class_id, _x) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(description, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_DESCRIPTION, _id, _comp_class_id, sink, _x)

/*
 * Defines a help attribute attached to a specific source component
 * class descriptor.
 *
 * _id:            Plugin descriptor ID (C identifier).
 * _comp_class_id: Component class descriptor ID (C identifier).
 * _x:             Help (C string).
 */
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_HELP_WITH_ID(_id, _comp_class_id, _x) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(help, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_HELP, _id, _comp_class_id, source, _x)

/*
 * Defines a help attribute attached to a specific filter component
 * class descriptor.
 *
 * _id:            Plugin descriptor ID (C identifier).
 * _comp_class_id: Component class descriptor ID (C identifier).
 * _x:             Help (C string).
 */
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_HELP_WITH_ID(_id, _comp_class_id, _x) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(help, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_HELP, _id, _comp_class_id, filter, _x)

/*
 * Defines a help attribute attached to a specific sink component class
 * descriptor.
 *
 * _id:            Plugin descriptor ID (C identifier).
 * _comp_class_id: Component class descriptor ID (C identifier).
 * _x:             Help (C string).
 */
#define BT_PLUGIN_SINK_COMPONENT_CLASS_HELP_WITH_ID(_id, _comp_class_id, _x) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(help, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_HELP, _id, _comp_class_id, sink, _x)

/*
 * Defines an initialization method attribute attached to a specific
 * source component class descriptor.
 *
 * _id:            Plugin descriptor ID (C identifier).
 * _comp_class_id: Component class descriptor ID (C identifier).
 * _x:             Initialization method (bt_component_class_source_init_method).
 */
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_INIT_METHOD_WITH_ID(_id, _comp_class_id, _x) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(source_init_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_INIT_METHOD, _id, _comp_class_id, source, _x)

/*
 * Defines an initialization method attribute attached to a specific
 * filter component class descriptor.
 *
 * _id:            Plugin descriptor ID (C identifier).
 * _comp_class_id: Component class descriptor ID (C identifier).
 * _x:             Initialization method (bt_component_class_filter_init_method).
 */
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_INIT_METHOD_WITH_ID(_id, _comp_class_id, _x) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(filter_init_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_INIT_METHOD, _id, _comp_class_id, filter, _x)

/*
 * Defines an initialization method attribute attached to a specific
 * sink component class descriptor.
 *
 * _id:            Plugin descriptor ID (C identifier).
 * _comp_class_id: Component class descriptor ID (C identifier).
 * _x:             Initialization method (bt_component_class_sink_init_method).
 */
#define BT_PLUGIN_SINK_COMPONENT_CLASS_INIT_METHOD_WITH_ID(_id, _comp_class_id, _x) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(sink_init_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_INIT_METHOD, _id, _comp_class_id, sink, _x)

/*
 * Defines a finalization method attribute attached to a specific source
 * component class descriptor.
 *
 * _id:            Plugin descriptor ID (C identifier).
 * _comp_class_id: Component class descriptor ID (C identifier).
 * _x:             Finalize method (bt_component_class_source_finalize_method).
 */
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_FINALIZE_METHOD_WITH_ID(_id, _comp_class_id, _x) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(source_finalize_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_FINALIZE_METHOD, _id, _comp_class_id, source, _x)

/*
 * Defines a finalization method attribute attached to a specific filter
 * component class descriptor.
 *
 * _id:            Plugin descriptor ID (C identifier).
 * _comp_class_id: Component class descriptor ID (C identifier).
 * _x:             Finalize method (bt_component_class_filter_finalize_method).
 */
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_FINALIZE_METHOD_WITH_ID(_id, _comp_class_id, _x) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(filter_finalize_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_FINALIZE_METHOD, _id, _comp_class_id, filter, _x)

/*
 * Defines a finalization method attribute attached to a specific sink
 * component class descriptor.
 *
 * _id:            Plugin descriptor ID (C identifier).
 * _comp_class_id: Component class descriptor ID (C identifier).
 * _x:             Finalize method (bt_component_class_sink_finalize_method).
 */
#define BT_PLUGIN_SINK_COMPONENT_CLASS_FINALIZE_METHOD_WITH_ID(_id, _comp_class_id, _x) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(sink_finalize_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_FINALIZE_METHOD, _id, _comp_class_id, sink, _x)

/*
 * Defines a query method attribute attached to a specific source
 * component class descriptor.
 *
 * _id:            Plugin descriptor ID (C identifier).
 * _comp_class_id: Component class descriptor ID (C identifier).
 * _x:             Finalize method (bt_component_class_source_query_method).
 */
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_QUERY_METHOD_WITH_ID(_id, _comp_class_id, _x) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(source_query_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_QUERY_METHOD, _id, _comp_class_id, source, _x)

/*
 * Defines a query method attribute attached to a specific filter
 * component class descriptor.
 *
 * _id:            Plugin descriptor ID (C identifier).
 * _comp_class_id: Component class descriptor ID (C identifier).
 * _x:             Finalize method (bt_component_class_filter_query_method).
 */
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_QUERY_METHOD_WITH_ID(_id, _comp_class_id, _x) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(filter_query_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_QUERY_METHOD, _id, _comp_class_id, filter, _x)

/*
 * Defines a query method attribute attached to a specific sink
 * component class descriptor.
 *
 * _id:            Plugin descriptor ID (C identifier).
 * _comp_class_id: Component class descriptor ID (C identifier).
 * _x:             Finalize method (bt_component_class_sink_query_method).
 */
#define BT_PLUGIN_SINK_COMPONENT_CLASS_QUERY_METHOD_WITH_ID(_id, _comp_class_id, _x) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(sink_query_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_QUERY_METHOD, _id, _comp_class_id, sink, _x)

/*
 * Defines an accept input port connection method attribute attached to
 * a specific filter component class descriptor.
 *
 * _id:            Plugin descriptor ID (C identifier).
 * _comp_class_id: Component class descriptor ID (C identifier).
 * _x:             Accept port connection method
 *                 (bt_component_class_filter_accept_input_port_connection_method).
 */
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_ACCEPT_INPUT_PORT_CONNECTION_METHOD_WITH_ID(_id, _comp_class_id, _x) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(filter_accept_input_port_connection_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_ACCEPT_INPUT_PORT_CONNECTION_METHOD, _id, _comp_class_id, filter, _x)

/*
 * Defines an accept input port connection method attribute attached to
 * a specific sink component class descriptor.
 *
 * _id:            Plugin descriptor ID (C identifier).
 * _comp_class_id: Component class descriptor ID (C identifier).
 * _x:             Accept port connection method
 *                 (bt_component_class_sink_accept_input_port_connection_method).
 */
#define BT_PLUGIN_SINK_COMPONENT_CLASS_ACCEPT_INPUT_PORT_CONNECTION_METHOD_WITH_ID(_id, _comp_class_id, _x) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(sink_accept_input_port_connection_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_ACCEPT_INPUT_PORT_CONNECTION_METHOD, _id, _comp_class_id, sink, _x)

/*
 * Defines an accept output port connection method attribute attached to
 * a specific source component class descriptor.
 *
 * _id:            Plugin descriptor ID (C identifier).
 * _comp_class_id: Component class descriptor ID (C identifier).
 * _x:             Accept port connection method
 *                 (bt_component_class_source_accept_output_port_connection_method).
 */
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_ACCEPT_OUTPUT_PORT_CONNECTION_METHOD_WITH_ID(_id, _comp_class_id, _x) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(source_accept_output_port_connection_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_ACCEPT_OUTPUT_PORT_CONNECTION_METHOD, _id, _comp_class_id, source, _x)

/*
 * Defines an accept output port connection method attribute attached to
 * a specific filter component class descriptor.
 *
 * _id:            Plugin descriptor ID (C identifier).
 * _comp_class_id: Component class descriptor ID (C identifier).
 * _x:             Accept port connection method
 *                 (bt_component_class_filter_accept_output_port_connection_method).
 */
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_ACCEPT_OUTPUT_PORT_CONNECTION_METHOD_WITH_ID(_id, _comp_class_id, _x) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(filter_accept_output_port_connection_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_ACCEPT_OUTPUT_PORT_CONNECTION_METHOD, _id, _comp_class_id, filter, _x)

/*
 * Defines an input port connected method attribute attached to a
 * specific filter component class descriptor.
 *
 * _id:            Plugin descriptor ID (C identifier).
 * _comp_class_id: Component class descriptor ID (C identifier).
 * _x:             Port connected method
 *                 (bt_component_class_filter_input_port_connected_method).
 */
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_INPUT_PORT_CONNECTED_METHOD_WITH_ID(_id, _comp_class_id, _x) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(filter_input_port_connected_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_INPUT_PORT_CONNECTED_METHOD, _id, _comp_class_id, filter, _x)

/*
 * Defines an input port connected method attribute attached to a
 * specific sink component class descriptor.
 *
 * _id:            Plugin descriptor ID (C identifier).
 * _comp_class_id: Component class descriptor ID (C identifier).
 * _x:             Port connected method
 *                 (bt_component_class_sink_input_port_connected_method).
 */
#define BT_PLUGIN_SINK_COMPONENT_CLASS_INPUT_PORT_CONNECTED_METHOD_WITH_ID(_id, _comp_class_id, _x) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(sink_input_port_connected_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_INPUT_PORT_CONNECTED_METHOD, _id, _comp_class_id, sink, _x)

/*
 * Defines an output port connected method attribute attached to a
 * specific source component class descriptor.
 *
 * _id:            Plugin descriptor ID (C identifier).
 * _comp_class_id: Component class descriptor ID (C identifier).
 * _x:             Port connected method
 *                 (bt_component_class_source_output_port_connected_method).
 */
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_OUTPUT_PORT_CONNECTED_METHOD_WITH_ID(_id, _comp_class_id, _x) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(source_output_port_connected_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_OUTPUT_PORT_CONNECTED_METHOD, _id, _comp_class_id, source, _x)

/*
 * Defines an output port connected method attribute attached to a
 * specific filter component class descriptor.
 *
 * _id:            Plugin descriptor ID (C identifier).
 * _comp_class_id: Component class descriptor ID (C identifier).
 * _x:             Port connected method
 *                 (bt_component_class_filter_output_port_connected_method).
 */
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_OUTPUT_PORT_CONNECTED_METHOD_WITH_ID(_id, _comp_class_id, _x) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(filter_output_port_connected_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_OUTPUT_PORT_CONNECTED_METHOD, _id, _comp_class_id, filter, _x)

/*
 * Defines an iterator initialization method attribute attached to a
 * specific source component class descriptor.
 *
 * _id:            Plugin descriptor ID (C identifier).
 * _comp_class_id: Component class descriptor ID (C identifier).
 * _x:             Iterator initialization method
 *                 (bt_component_class_source_message_iterator_init_method).
 */
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_INIT_METHOD_WITH_ID(_id, _comp_class_id, _x) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(source_msg_iter_init_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_MSG_ITER_INIT_METHOD, _id, _comp_class_id, source, _x)

/*
 * Defines an iterator finalize method attribute attached to a specific
 * source component class descriptor.
 *
 * _id:            Plugin descriptor ID (C identifier).
 * _comp_class_id: Component class descriptor ID (C identifier).
 * _x:             Iterator finalize method
 *                 (bt_component_class_source_message_iterator_finalize_method).
 */
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_FINALIZE_METHOD_WITH_ID(_id, _comp_class_id, _x) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(source_msg_iter_finalize_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_MSG_ITER_FINALIZE_METHOD, _id, _comp_class_id, source, _x)

/*
 * Defines an iterator initialization method attribute attached to a
 * specific filter component class descriptor.
 *
 * _id:            Plugin descriptor ID (C identifier).
 * _comp_class_id: Component class descriptor ID (C identifier).
 * _x:             Iterator initialization method
 *                 (bt_component_class_filter_message_iterator_init_method).
 */
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_INIT_METHOD_WITH_ID(_id, _comp_class_id, _x) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(filter_msg_iter_init_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_MSG_ITER_INIT_METHOD, _id, _comp_class_id, filter, _x)

/*
 * Defines an iterator finalize method attribute attached to a specific
 * filter component class descriptor.
 *
 * _id:            Plugin descriptor ID (C identifier).
 * _comp_class_id: Component class descriptor ID (C identifier).
 * _x:             Iterator finalize method
 *                 (bt_component_class_filter_message_iterator_finalize_method).
 */
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_FINALIZE_METHOD_WITH_ID(_id, _comp_class_id, _x) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(filter_msg_iter_finalize_method, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_MSG_ITER_FINALIZE_METHOD, _id, _comp_class_id, filter, _x)

/*
 * Defines a plugin descriptor with an automatic ID.
 *
 * _name: Plugin's name (C string).
 */
#define BT_PLUGIN(_name) 		static BT_PLUGIN_WITH_ID(auto, #_name)

/*
 * Defines a plugin initialization function attribute attached to the
 * automatic plugin descriptor.
 *
 * _x: Initialization function (bt_plugin_init_func).
 */
#define BT_PLUGIN_INIT(_x) 		BT_PLUGIN_INIT_WITH_ID(auto, _x)

 /*
 * Defines a plugin exit function attribute attached to the automatic
 * plugin descriptor.
 *
 * _x: Exit function (bt_plugin_exit_func).
 */
#define BT_PLUGIN_EXIT(_x) 		BT_PLUGIN_EXIT_WITH_ID(auto, _x)

/*
 * Defines an author attribute attached to the automatic plugin
 * descriptor.
 *
 * _x: Author (C string).
 */
#define BT_PLUGIN_AUTHOR(_x) 		BT_PLUGIN_AUTHOR_WITH_ID(auto, _x)

/*
 * Defines a license attribute attached to the automatic plugin
 * descriptor.
 *
 * _x: License (C string).
 */
#define BT_PLUGIN_LICENSE(_x) 		BT_PLUGIN_LICENSE_WITH_ID(auto, _x)

/*
 * Defines a description attribute attached to the automatic plugin
 * descriptor.
 *
 * _x: Description (C string).
 */
#define BT_PLUGIN_DESCRIPTION(_x) 	BT_PLUGIN_DESCRIPTION_WITH_ID(auto, _x)

/*
 * Defines a version attribute attached to the automatic plugin
 * descriptor.
 *
 * _major: Plugin's major version (uint32_t).
 * _minor: Plugin's minor version (uint32_t).
 * _patch: Plugin's patch version (uint32_t).
 * _extra: Plugin's version extra information (C string).
 */
#define BT_PLUGIN_VERSION(_major, _minor, _patch, _extra) BT_PLUGIN_VERSION_WITH_ID(auto, _major, _minor, _patch, _extra)

/*
 * Defines a source component class attached to the automatic plugin
 * descriptor. Its ID is the same as its name, hence its name must be a
 * C identifier in this version.
 *
 * _name:                   Component class name (C identifier).
 * _msg_iter_next_method: Component class's iterator next method
 *                          (bt_component_class_source_message_iterator_next_method).
 */
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS(_name, _msg_iter_next_method) \
	BT_PLUGIN_SOURCE_COMPONENT_CLASS_WITH_ID(auto, _name, #_name, _msg_iter_next_method)

/*
 * Defines a filter component class attached to the automatic plugin
 * descriptor. Its ID is the same as its name, hence its name must be a
 * C identifier in this version.
 *
 * _name:                   Component class name (C identifier).
 * _msg_iter_next_method: Component class's iterator next method
 *                          (bt_component_class_filter_message_iterator_next_method).
 */
#define BT_PLUGIN_FILTER_COMPONENT_CLASS(_name, _msg_iter_next_method) \
	BT_PLUGIN_FILTER_COMPONENT_CLASS_WITH_ID(auto, _name, #_name, _msg_iter_next_method)

/*
 * Defines a sink component class attached to the automatic plugin
 * descriptor. Its ID is the same as its name, hence its name must be a
 * C identifier in this version.
 *
 * _name:           Component class name (C identifier).
 * _consume_method: Component class's consume method
 *                  (bt_component_class_sink_consume_method).
 */
#define BT_PLUGIN_SINK_COMPONENT_CLASS(_name, _consume_method) \
	BT_PLUGIN_SINK_COMPONENT_CLASS_WITH_ID(auto, _name, #_name, _consume_method)

/*
 * Defines a description attribute attached to a source component class
 * descriptor which is attached to the automatic plugin descriptor.
 *
 * _name: Component class name (C identifier).
 * _x:    Description (C string).
 */
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_DESCRIPTION(_name, _x) \
	BT_PLUGIN_SOURCE_COMPONENT_CLASS_DESCRIPTION_WITH_ID(auto, _name, _x)

/*
 * Defines a description attribute attached to a filter component class
 * descriptor which is attached to the automatic plugin descriptor.
 *
 * _name: Component class name (C identifier).
 * _x:    Description (C string).
 */
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_DESCRIPTION(_name, _x) \
	BT_PLUGIN_FILTER_COMPONENT_CLASS_DESCRIPTION_WITH_ID(auto, _name, _x)

/*
 * Defines a description attribute attached to a sink component class
 * descriptor which is attached to the automatic plugin descriptor.
 *
 * _name: Component class name (C identifier).
 * _x:    Description (C string).
 */
#define BT_PLUGIN_SINK_COMPONENT_CLASS_DESCRIPTION(_name, _x) \
	BT_PLUGIN_SINK_COMPONENT_CLASS_DESCRIPTION_WITH_ID(auto, _name, _x)

/*
 * Defines a help attribute attached to a source component class
 * descriptor which is attached to the automatic plugin descriptor.
 *
 * _name: Component class name (C identifier).
 * _x:    Help (C string).
 */
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_HELP(_name, _x) \
	BT_PLUGIN_SOURCE_COMPONENT_CLASS_HELP_WITH_ID(auto, _name, _x)

/*
 * Defines a help attribute attached to a filter component class
 * descriptor which is attached to the automatic plugin descriptor.
 *
 * _name: Component class name (C identifier).
 * _x:    Help (C string).
 */
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_HELP(_name, _x) \
	BT_PLUGIN_FILTER_COMPONENT_CLASS_HELP_WITH_ID(auto, _name, _x)

/*
 * Defines a help attribute attached to a sink component class
 * descriptor which is attached to the automatic plugin descriptor.
 *
 * _name: Component class name (C identifier).
 * _x:    Help (C string).
 */
#define BT_PLUGIN_SINK_COMPONENT_CLASS_HELP(_name, _x) \
	BT_PLUGIN_SINK_COMPONENT_CLASS_HELP_WITH_ID(auto, _name, _x)

/*
 * Defines an initialization method attribute attached to a source
 * component class descriptor which is attached to the automatic plugin
 * descriptor.
 *
 * _name: Component class name (C identifier).
 * _x:    Initialization method (bt_component_class_source_init_method).
 */
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_INIT_METHOD(_name, _x) \
	BT_PLUGIN_SOURCE_COMPONENT_CLASS_INIT_METHOD_WITH_ID(auto, _name, _x)

/*
 * Defines an initialization method attribute attached to a filter
 * component class descriptor which is attached to the automatic plugin
 * descriptor.
 *
 * _name: Component class name (C identifier).
 * _x:    Initialization method (bt_component_class_filter_init_method).
 */
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_INIT_METHOD(_name, _x) \
	BT_PLUGIN_FILTER_COMPONENT_CLASS_INIT_METHOD_WITH_ID(auto, _name, _x)

/*
 * Defines an initialization method attribute attached to a sink
 * component class descriptor which is attached to the automatic plugin
 * descriptor.
 *
 * _name: Component class name (C identifier).
 * _x:    Initialization method (bt_component_class_sink_init_method).
 */
#define BT_PLUGIN_SINK_COMPONENT_CLASS_INIT_METHOD(_name, _x) \
	BT_PLUGIN_SINK_COMPONENT_CLASS_INIT_METHOD_WITH_ID(auto, _name, _x)

/*
 * Defines a finalization method attribute attached to a source component
 * class descriptor which is attached to the automatic plugin
 * descriptor.
 *
 * _name: Component class name (C identifier).
 * _x:    Initialization method (bt_component_class_source_finalize_method).
 */
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_FINALIZE_METHOD(_name, _x) \
	BT_PLUGIN_SOURCE_COMPONENT_CLASS_FINALIZE_METHOD_WITH_ID(auto, _name, _x)

/*
 * Defines a finalization method attribute attached to a filter component
 * class descriptor which is attached to the automatic plugin
 * descriptor.
 *
 * _name: Component class name (C identifier).
 * _x:    Initialization method (bt_component_class_filter_finalize_method).
 */
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_FINALIZE_METHOD(_name, _x) \
	BT_PLUGIN_FILTER_COMPONENT_CLASS_FINALIZE_METHOD_WITH_ID(auto, _name, _x)

/*
 * Defines a finalization method attribute attached to a sink component class
 * descriptor which is attached to the automatic plugin descriptor.
 *
 * _name: Component class name (C identifier).
 * _x:    Initialization method (bt_component_class_sink_finalize_method).
 */
#define BT_PLUGIN_SINK_COMPONENT_CLASS_FINALIZE_METHOD(_name, _x) \
	BT_PLUGIN_SINK_COMPONENT_CLASS_FINALIZE_METHOD_WITH_ID(auto, _name, _x)

/*
 * Defines a query method attribute attached to a source component
 * class descriptor which is attached to the automatic plugin
 * descriptor.
 *
 * _name: Component class name (C identifier).
 * _x:    Initialization method (bt_component_class_source_query_method).
 */
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_QUERY_METHOD(_name, _x) \
	BT_PLUGIN_SOURCE_COMPONENT_CLASS_QUERY_METHOD_WITH_ID(auto, _name, _x)

/*
 * Defines a query method attribute attached to a filter component
 * class descriptor which is attached to the automatic plugin
 * descriptor.
 *
 * _name: Component class name (C identifier).
 * _x:    Initialization method (bt_component_class_filter_query_method).
 */
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_QUERY_METHOD(_name, _x) \
	BT_PLUGIN_FILTER_COMPONENT_CLASS_QUERY_METHOD_WITH_ID(auto, _name, _x)

/*
 * Defines a query method attribute attached to a sink component
 * class descriptor which is attached to the automatic plugin
 * descriptor.
 *
 * _name: Component class name (C identifier).
 * _x:    Initialization method (bt_component_class_sink_query_method).
 */
#define BT_PLUGIN_SINK_COMPONENT_CLASS_QUERY_METHOD(_name, _x) \
	BT_PLUGIN_SINK_COMPONENT_CLASS_QUERY_METHOD_WITH_ID(auto, _name, _x)

/*
 * Defines an accept input port connection method attribute attached to
 * a filter component class descriptor which is attached to the
 * automatic plugin descriptor.
 *
 * _name: Component class name (C identifier).
 * _x:    Accept port connection method
 *        (bt_component_class_filter_accept_input_port_connection_method).
 */
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_ACCEPT_INPUT_PORT_CONNECTION_METHOD(_name, _x) \
	BT_PLUGIN_FILTER_COMPONENT_CLASS_ACCEPT_INPUT_PORT_CONNECTION_METHOD_WITH_ID(auto, _name, _x)

/*
 * Defines an accept input port connection method attribute attached to
 * a sink component class descriptor which is attached to the automatic
 * plugin descriptor.
 *
 * _name: Component class name (C identifier).
 * _x:    Accept port connection method
 *        (bt_component_class_sink_accept_input_port_connection_method).
 */
#define BT_PLUGIN_SINK_COMPONENT_CLASS_ACCEPT_INPUT_PORT_CONNECTION_METHOD(_name, _x) \
	BT_PLUGIN_SINK_COMPONENT_CLASS_ACCEPT_INPUT_PORT_CONNECTION_METHOD_WITH_ID(auto, _name, _x)

/*
 * Defines an accept output port connection method attribute attached to
 * a source component class descriptor which is attached to the
 * automatic plugin descriptor.
 *
 * _name: Component class name (C identifier).
 * _x:    Accept port connection method
 *        (bt_component_class_source_accept_output_port_connection_method).
 */
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_ACCEPT_OUTPUT_PORT_CONNECTION_METHOD(_name, _x) \
	BT_PLUGIN_SOURCE_COMPONENT_CLASS_ACCEPT_OUTPUT_PORT_CONNECTION_METHOD_WITH_ID(auto, _name, _x)

/*
 * Defines an accept output port connection method attribute attached to
 * a filter component class descriptor which is attached to the
 * automatic plugin descriptor.
 *
 * _name: Component class name (C identifier).
 * _x:    Accept port connection method
 *        (bt_component_class_filter_accept_output_port_connection_method).
 */
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_ACCEPT_OUTPUT_PORT_CONNECTION_METHOD(_name, _x) \
	BT_PLUGIN_FILTER_COMPONENT_CLASS_ACCEPT_OUTPUT_PORT_CONNECTION_METHOD_WITH_ID(auto, _name, _x)

/*
 * Defines an input port connected method attribute attached to a filter
 * component class descriptor which is attached to the automatic plugin
 * descriptor.
 *
 * _name: Component class name (C identifier).
 * _x:    Port connected (bt_component_class_filter_input_port_connected_method).
 */
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_INPUT_PORT_CONNECTED_METHOD(_name, _x) \
	BT_PLUGIN_FILTER_COMPONENT_CLASS_INPUT_PORT_CONNECTED_METHOD_WITH_ID(auto, _name, _x)

/*
 * Defines an input port connected method attribute attached to a sink
 * component class descriptor which is attached to the automatic plugin
 * descriptor.
 *
 * _name: Component class name (C identifier).
 * _x:    Port connected (bt_component_class_sink_input_port_connected_method).
 */
#define BT_PLUGIN_SINK_COMPONENT_CLASS_INPUT_PORT_CONNECTED_METHOD(_name, _x) \
	BT_PLUGIN_SINK_COMPONENT_CLASS_INPUT_PORT_CONNECTED_METHOD_WITH_ID(auto, _name, _x)

/*
 * Defines an output port connected method attribute attached to a source
 * component class descriptor which is attached to the automatic plugin
 * descriptor.
 *
 * _name: Component class name (C identifier).
 * _x:    Port connected (bt_component_class_source_output_port_connected_method).
 */
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_OUTPUT_PORT_CONNECTED_METHOD(_name, _x) \
	BT_PLUGIN_SOURCE_COMPONENT_CLASS_OUTPUT_PORT_CONNECTED_METHOD_WITH_ID(auto, _name, _x)

/*
 * Defines an output port connected method attribute attached to a filter
 * component class descriptor which is attached to the automatic plugin
 * descriptor.
 *
 * _name: Component class name (C identifier).
 * _x:    Port connected (bt_component_class_filter_output_port_connected_method).
 */
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_OUTPUT_PORT_CONNECTED_METHOD(_name, _x) \
	BT_PLUGIN_FILTER_COMPONENT_CLASS_OUTPUT_PORT_CONNECTED_METHOD_WITH_ID(auto, _name, _x)

/*
 * Defines an iterator initialization method attribute attached to a
 * source component class descriptor which is attached to the automatic
 * plugin descriptor.
 *
 * _name: Component class name (C identifier).
 * _x:    Iterator initialization method
 *        (bt_component_class_source_message_iterator_init_method).
 */
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_INIT_METHOD(_name, _x) \
	BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_INIT_METHOD_WITH_ID(auto, _name, _x)

/*
 * Defines an iterator finalize method attribute attached to a source
 * component class descriptor which is attached to the automatic plugin
 * descriptor.
 *
 * _name: Component class name (C identifier).
 * _x:    Iterator finalize method
 *        (bt_component_class_source_message_iterator_finalize_method).
 */
#define BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_FINALIZE_METHOD(_name, _x) \
	BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_FINALIZE_METHOD_WITH_ID(auto, _name, _x)

/*
 * Defines an iterator initialization method attribute attached to a
 * filter component class descriptor which is attached to the automatic
 * plugin descriptor.
 *
 * _name: Component class name (C identifier).
 * _x:    Iterator initialization method
 *        (bt_component_class_filter_message_iterator_init_method).
 */
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_INIT_METHOD(_name, _x) \
	BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_INIT_METHOD_WITH_ID(auto, _name, _x)

/*
 * Defines an iterator finalize method attribute attached to a filter
 * component class descriptor which is attached to the automatic plugin
 * descriptor.
 *
 * _name: Component class name (C identifier).
 * _x:    Iterator finalize method
 *        (bt_component_class_filter_message_iterator_finalize_method).
 */
#define BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_FINALIZE_METHOD(_name, _x) \
	BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_FINALIZE_METHOD_WITH_ID(auto, _name, _x)

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

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_PLUGIN_PLUGIN_DEV_H */
