#ifndef BABELTRACE2_ERROR_CAUSE_CONST_H
#define BABELTRACE2_ERROR_CAUSE_CONST_H

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
#include <babeltrace2/graph/component-class-const.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum bt_error_cause_actor_type {
	BT_ERROR_CAUSE_ACTOR_TYPE_UNKNOWN		= 1 << 0,
	BT_ERROR_CAUSE_ACTOR_TYPE_COMPONENT		= 1 << 1,
	BT_ERROR_CAUSE_ACTOR_TYPE_COMPONENT_CLASS	= 1 << 2,
	BT_ERROR_CAUSE_ACTOR_TYPE_MESSAGE_ITERATOR	= 1 << 3,
} bt_error_cause_actor_type;

extern
bt_error_cause_actor_type bt_error_cause_get_actor_type(
		const bt_error_cause *cause);

extern
const char *bt_error_cause_get_message(const bt_error_cause *cause);

extern
const char *bt_error_cause_get_module_name(const bt_error_cause *cause);

extern
const char *bt_error_cause_get_file_name(const bt_error_cause *cause);

extern
uint64_t bt_error_cause_get_line_number(const bt_error_cause *cause);

extern
const char *bt_error_cause_component_actor_get_component_name(
		const bt_error_cause *cause);

extern
bt_component_class_type bt_error_cause_component_actor_get_component_class_type(
		const bt_error_cause *cause);

extern
const char *bt_error_cause_component_actor_get_component_class_name(
		const bt_error_cause *cause);

extern
const char *bt_error_cause_component_actor_get_plugin_name(
		const bt_error_cause *cause);

extern
bt_component_class_type
bt_error_cause_component_class_actor_get_component_class_type(
		const bt_error_cause *cause);

extern
const char *bt_error_cause_component_class_actor_get_component_class_name(
		const bt_error_cause *cause);

extern
const char *bt_error_cause_component_class_actor_get_plugin_name(
		const bt_error_cause *cause);

extern
const char *bt_error_cause_message_iterator_actor_get_component_name(
		const bt_error_cause *cause);

extern
const char *bt_error_cause_message_iterator_actor_get_component_output_port_name(
		const bt_error_cause *cause);

extern
bt_component_class_type
bt_error_cause_message_iterator_actor_get_component_class_type(
		const bt_error_cause *cause);

extern
const char *bt_error_cause_message_iterator_actor_get_component_class_name(
		const bt_error_cause *cause);

extern
const char *bt_error_cause_message_iterator_actor_get_plugin_name(
		const bt_error_cause *cause);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_ERROR_CAUSE_CONST_H */
