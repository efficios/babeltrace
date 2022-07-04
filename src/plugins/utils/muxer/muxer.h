/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 * Copyright 2017 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_PLUGINS_UTILS_MUXER_H
#define BABELTRACE_PLUGINS_UTILS_MUXER_H

#include <stdint.h>
#include <babeltrace2/babeltrace.h>
#include "common/macros.h"

bt_component_class_initialize_method_status muxer_init(
		bt_self_component_filter *self_comp,
		bt_self_component_filter_configuration *config,
		const bt_value *params, void *init_data);

void muxer_finalize(bt_self_component_filter *self_comp);

bt_message_iterator_class_initialize_method_status muxer_msg_iter_init(
		bt_self_message_iterator *self_msg_iter,
		bt_self_message_iterator_configuration *config,
		bt_self_component_port_output *self_port);

void muxer_msg_iter_finalize(
		bt_self_message_iterator *self_msg_iter);

bt_message_iterator_class_next_method_status muxer_msg_iter_next(
		bt_self_message_iterator *self_msg_iter,
		bt_message_array_const msgs, uint64_t capacity,
		uint64_t *count);

bt_component_class_port_connected_method_status muxer_input_port_connected(
		bt_self_component_filter *comp,
		bt_self_component_port_input *self_port,
		const bt_port_output *other_port);

bt_message_iterator_class_can_seek_beginning_method_status
muxer_msg_iter_can_seek_beginning(
		bt_self_message_iterator *message_iterator, bt_bool *can_seek);

bt_message_iterator_class_seek_beginning_method_status muxer_msg_iter_seek_beginning(
		bt_self_message_iterator *message_iterator);

#endif /* BABELTRACE_PLUGINS_UTILS_MUXER_H */
