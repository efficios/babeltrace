/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2019 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_ERROR_INTERNAL_H
#define BABELTRACE_ERROR_INTERNAL_H

#include <stdarg.h>
#include <glib.h>
#include <babeltrace2/babeltrace.h>
#include "lib/object.h"
#include "common/macros.h"

struct bt_error_cause {
	enum bt_error_cause_actor_type actor_type;
	GString *module_name;
	GString *message;
	GString *file_name;
	uint64_t line_no;
};

struct bt_error_cause_component_class_id {
	GString *name;
	bt_component_class_type type;
	GString *plugin_name;
};

struct bt_error_cause_component_actor {
	struct bt_error_cause base;
	GString *comp_name;
	struct bt_error_cause_component_class_id comp_class_id;
};

struct bt_error_cause_component_class_actor {
	struct bt_error_cause base;
	struct bt_error_cause_component_class_id comp_class_id;
};

struct bt_error_cause_message_iterator_actor {
	struct bt_error_cause base;
	GString *comp_name;
	GString *output_port_name;
	struct bt_error_cause_component_class_id comp_class_id;
};

struct bt_error {
	/*
	 * Array of `struct bt_error_cause *` (or an extension); owned
	 * by this.
	 */
	GPtrArray *causes;
};

static inline
const char *bt_error_cause_actor_type_string(
		enum bt_error_cause_actor_type actor_type)
{
	switch (actor_type) {
	case BT_ERROR_CAUSE_ACTOR_TYPE_UNKNOWN:
		return "UNKNOWN";
	case BT_ERROR_CAUSE_ACTOR_TYPE_COMPONENT:
		return "COMPONENT";
	case BT_ERROR_CAUSE_ACTOR_TYPE_COMPONENT_CLASS:
		return "COMPONENT_CLASS";
	case BT_ERROR_CAUSE_ACTOR_TYPE_MESSAGE_ITERATOR:
		return "MESSAGE_ITERATOR";
	default:
		return "(unknown)";
	}
};

struct bt_error *bt_error_create(void);

void bt_error_destroy(struct bt_error *error);

__BT_ATTR_FORMAT_PRINTF(5, 0)
int bt_error_append_cause_from_unknown(struct bt_error *error,
		const char *module_name, const char *file_name,
		uint64_t line_no, const char *msg_fmt, va_list args);

__BT_ATTR_FORMAT_PRINTF(5, 0)
int bt_error_append_cause_from_component(
		struct bt_error *error, bt_self_component *self_comp,
		const char *file_name, uint64_t line_no,
		const char *msg_fmt, va_list args);

__BT_ATTR_FORMAT_PRINTF(5, 0)
int bt_error_append_cause_from_component_class(
		struct bt_error *error,
		bt_self_component_class *self_comp_class,
		const char *file_name, uint64_t line_no,
		const char *msg_fmt, va_list args);

__BT_ATTR_FORMAT_PRINTF(5, 0)
int bt_error_append_cause_from_message_iterator(
		struct bt_error *error, bt_self_message_iterator *self_iter,
		const char *file_name, uint64_t line_no,
		const char *msg_fmt, va_list args);

#endif /* BABELTRACE_ERROR_INTERNAL_H */
