#ifndef BABELTRACE_PLUGIN_PLUGIN_DEV_H
#define BABELTRACE_PLUGIN_PLUGIN_DEV_H

/*
 * BabelTrace - Babeltrace Plug-in Development API
 *
 * This is the header that you need to include for the development of
 * a Babeltrace plug-in.
 *
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
 *
 * Author: Jérémie Galarneau <jeremie.galarneau@efficios.com>
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
#include <babeltrace/plugin/plugin.h>
#include <babeltrace/component/component-class.h>

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
typedef enum bt_plugin_status (*bt_plugin_init_func)(
		struct bt_plugin *plugin);

/* Plugin exit function type */
typedef enum bt_plugin_status (*bt_plugin_exit_func)(void);

/*
 * Function to call from a plugin's initialization function to add a
 * component class to a plugin object.
 */
extern enum bt_plugin_status bt_plugin_add_component_class(
		struct bt_plugin *plugin,
		struct bt_component_class *component_class);

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
};

/* Plugin attribute (internal use) */
struct __bt_plugin_descriptor_attribute {
	/* Plugin descriptor to which to associate this attribute */
	const struct __bt_plugin_descriptor *plugin_descriptor;

	/* Attribute's type */
	enum __bt_plugin_descriptor_attribute_type type;

	/* Name of the attribute's type for debug purposes */
	const char *type_name;

	/* Attribute's value */
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
	} value;
} __attribute__((packed));

/* Component class descriptor (internal use) */
struct __bt_plugin_component_class_descriptor {
	/*
	 * Plugin descriptor to which to associate this component
	 * class descriptor.
	 */
	const struct __bt_plugin_descriptor *plugin_descriptor;

	/* Component type */
	enum bt_component_type type;

	/* Component class name */
	const char *name;

	/* Component initialization function */
	bt_component_init_cb init_cb;
} __attribute__((packed));

/* Type of a component class attribute (internal use) */
enum __bt_plugin_component_class_descriptor_attribute_type {
	BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_DESCRIPTION	= 0,
};

/* Component class attribute (internal use) */
struct __bt_plugin_component_class_descriptor_attribute {
	/*
	 * Component class plugin attribute to which to associate this
	 * component class attribute.
	 */
	const struct __bt_plugin_component_class_descriptor *comp_class_descriptor;

	/* Attribute's type */
	enum __bt_plugin_component_class_descriptor_attribute_type type;

	/* Name of the attribute's type for debug purposes */
	const char *type_name;

	/* Attribute's value */
	union {
		/* BT_PLUGIN_DESCRIPTOR_ATTRIBUTE_TYPE_DESCRIPTION */
		const char *description;
	} value;
} __attribute__((packed));

/*
 * Variable attributes for a plugin descriptor pointer to be added to
 * the plugin descriptor section (internal use).
 */
#define __BT_PLUGIN_DESCRIPTOR_ATTRS \
	__attribute__((section("__bt_plugin_descriptors"), used))

/*
 * Variable attributes for a plugin attribute pointer to be added to
 * the plugin attribute section (internal use).
 */
#define __BT_PLUGIN_DESCRIPTOR_ATTRIBUTES_ATTRS \
	__attribute__((section("__bt_plugin_descriptor_attributes"), used))

/*
 * Variable attributes for a component class descriptor pointer to be
 * added to the component class descriptor section (internal use).
 */
#define __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRS \
	__attribute__((section("__bt_plugin_component_class_descriptors"), used))

/*
 * Variable attributes for a component class descriptor attribute
 * pointer to be added to the component class descriptor attribute
 * section (internal use).
 */
#define __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTES_ATTRS \
	__attribute__((section("__bt_plugin_component_class_descriptor_attributes"), used))

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
	static struct __bt_plugin_descriptor const * const __bt_plugin_descriptor_##_id##_ptr __BT_PLUGIN_DESCRIPTOR_ATTRS = &__bt_plugin_descriptor_##_id; \
	extern struct __bt_plugin_descriptor const *__start___bt_plugin_descriptors; \
	extern struct __bt_plugin_descriptor const *__stop___bt_plugin_descriptors

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
		.type = _attr_type,					\
		.type_name = #_attr_name,				\
		.value._attr_name = _x,					\
	};								\
	static struct __bt_plugin_descriptor_attribute const * const __bt_plugin_descriptor_attribute_##_id##_##_attr_name##_ptr __BT_PLUGIN_DESCRIPTOR_ATTRIBUTES_ATTRS = &__bt_plugin_descriptor_attribute_##_id##_##_attr_name; \
	extern struct __bt_plugin_descriptor_attribute const *__start___bt_plugin_descriptor_attributes; \
	extern struct __bt_plugin_descriptor_attribute const *__stop___bt_plugin_descriptor_attributes

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

/*
 * Defines a component class descriptor with a custom ID.
 *
 * _id:            ID (any valid C identifier except `auto`).
 * _comp_class_id: Component class ID (C identifier).
 * _type:          Component class type (enum bt_component_type).
 * _name:          Component class name (C string).
 * _init_func:     Component class's initialization function
 *                 (bt_component_init_cb).
 */
#define BT_PLUGIN_COMPONENT_CLASS_WITH_ID(_id, _comp_class_id, _type, _name, _init_cb) \
	static struct __bt_plugin_component_class_descriptor __bt_plugin_component_class_descriptor_##_id##_##_comp_class_id##_##_type = { \
		.plugin_descriptor = &__bt_plugin_descriptor_##_id,	\
		.type = _type,						\
		.name = _name,						\
		.init_cb = _init_cb,					\
	};								\
	static struct __bt_plugin_component_class_descriptor const * const __bt_plugin_component_class_descriptor_##_id##_##_comp_class_id##_##_type##_ptr __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRS = &__bt_plugin_component_class_descriptor_##_id##_##_comp_class_id##_##_type; \
	extern struct __bt_plugin_component_class_descriptor const *__start___bt_plugin_component_class_descriptors; \
	extern struct __bt_plugin_component_class_descriptor const *__stop___bt_plugin_component_class_descriptors

/*
 * Defines a component class descriptor attribute (generic, internal
 * use).
 *
 * _id:            Plugin descriptor ID (C identifier).
 * _comp_class_id: Component class ID (C identifier).
 * _type:          Component class type (enum bt_component_type).
 * _attr_name:     Name of the attribute (C identifier).
 * _attr_type:     Type of the attribute
 *                 (enum __bt_plugin_descriptor_attribute_type).
 * _x:             Value.
 */
#define __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(_attr_name, _attr_type, _id, _comp_class_id, _type, _x) \
	static struct __bt_plugin_component_class_descriptor_attribute __bt_plugin_component_class_descriptor_attribute_##_id##_##_comp_class_id##_##_type##_##_attr_name = { \
		.comp_class_descriptor = &__bt_plugin_component_class_descriptor_##_id##_##_comp_class_id##_##_type, \
		.type = _attr_type,					\
		.type_name = #_attr_name,				\
		.value.description = _x,				\
	};								\
	static struct __bt_plugin_component_class_descriptor_attribute const * const __bt_plugin_component_class_descriptor_attribute_##_id##_##_comp_class_id##_##_type##_##_attr_name##_ptr __BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTES_ATTRS = &__bt_plugin_component_class_descriptor_attribute_##_id##_##_comp_class_id##_##_type##_##_attr_name; \
	extern struct __bt_plugin_component_class_descriptor_attribute const *__start___bt_plugin_component_class_descriptor_attributes; \
	extern struct __bt_plugin_component_class_descriptor_attribute const *__stop___bt_plugin_component_class_descriptor_attributes

/*
 * Defines a description attribute attached to a specific component
 * class descriptor.
 *
 * _id:            Plugin descriptor ID (C identifier).
 * _comp_class_id: Component class descriptor ID (C identifier).
 * _type:          Component class type (enum bt_component_type).
 * _x:             Description (C string).
 */
#define BT_PLUGIN_COMPONENT_CLASS_DESCRIPTION_WITH_ID(_id, _comp_class_id, _type, _x) \
	__BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE(description, BT_PLUGIN_COMPONENT_CLASS_DESCRIPTOR_ATTRIBUTE_TYPE_DESCRIPTION, _id, _comp_class_id, _type, _x)

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
 * Defines a component class attached to the automatic plugin
 * descriptor. Its ID is the same as its name, hence its name must be a
 * C identifier in this version.
 *
 * _type:          Component class type (enum bt_component_type).
 * _name:          Component class name (C identifier).
 * _init_cb:       Component class's initialization function
 *                 (bt_component_init_cb).
 */
#define BT_PLUGIN_COMPONENT_CLASS(_type, _name, _init_cb) \
	BT_PLUGIN_COMPONENT_CLASS_WITH_ID(auto, _name, _type, #_name, _init_cb)

/*
 * Defines a description attribute attached to a component class
 * descriptor which is attached to the automatic plugin descriptor.
 *
 * _type:          Component class type (enum bt_component_type).
 * _name:          Component class name (C identifier).
 * _x: Description (C string).
 */
#define BT_PLUGIN_COMPONENT_CLASS_DESCRIPTION(_type, _name, _x) \
	BT_PLUGIN_COMPONENT_CLASS_DESCRIPTION_WITH_ID(auto, _name, _type, _x)

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_PLUGIN_PLUGIN_DEV_H */
