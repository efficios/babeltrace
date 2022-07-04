/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#ifndef BABELTRACE_PLUGIN_PLUGIN_SO_INTERNAL_H
#define BABELTRACE_PLUGIN_PLUGIN_SO_INTERNAL_H

#include <glib.h>
#include <gmodule.h>
#include <stdbool.h>
#include <babeltrace2/types.h>
#include "common/macros.h"

struct bt_plugin;
struct bt_component_class;

struct bt_plugin_so_shared_lib_handle {
	struct bt_object base;
	GString *path;
	GModule *module;

	/* True if initialization function was called */
	bt_bool init_called;
	bt_plugin_finalize_func exit;
};

struct bt_plugin_so_spec_data {
	/* Shared lib. handle: owned by this */
	struct bt_plugin_so_shared_lib_handle *shared_lib_handle;

	/* Pointers to plugin's memory: do NOT free */
	const struct __bt_plugin_descriptor *descriptor;
	bt_plugin_initialize_func init;
	const struct __bt_plugin_descriptor_version *version;
};

int bt_plugin_so_create_all_from_file(const char *path,
		bool fail_on_load_error, struct bt_plugin_set **plugin_set_out);

int bt_plugin_so_create_all_from_static(bool fail_on_load_error,
		struct bt_plugin_set **plugin_set_out);

void bt_plugin_so_on_add_component_class(struct bt_plugin *plugin,
		struct bt_component_class *comp_class);

#endif /* BABELTRACE_PLUGIN_PLUGIN_SO_INTERNAL_H */
