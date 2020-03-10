/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * BabelTrace - Trace Trimmer Plug-in
 */

#ifndef BABELTRACE_PLUGINS_UTILS_TRIMMER_H
#define BABELTRACE_PLUGINS_UTILS_TRIMMER_H

#include "common/macros.h"
#include <babeltrace2/babeltrace.h>

BT_HIDDEN
void trimmer_finalize(bt_self_component_filter *self_comp);

BT_HIDDEN
bt_component_class_initialize_method_status trimmer_init(
		bt_self_component_filter *self_comp,
		bt_self_component_filter_configuration *config,
		const bt_value *params, void *init_data);

BT_HIDDEN
bt_message_iterator_class_initialize_method_status trimmer_msg_iter_init(
		bt_self_message_iterator *self_msg_iter,
		bt_self_message_iterator_configuration *config,
		bt_self_component_port_output *port);

BT_HIDDEN
bt_message_iterator_class_next_method_status trimmer_msg_iter_next(
		bt_self_message_iterator *self_msg_iter,
		bt_message_array_const msgs, uint64_t capacity,
		uint64_t *count);

BT_HIDDEN
void trimmer_msg_iter_finalize(bt_self_message_iterator *self_msg_iter);

#endif /* BABELTRACE_PLUGINS_UTILS_TRIMMER_H */
