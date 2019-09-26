#ifndef BABELTRACE2_GRAPH_COMPONENT_DESCRIPTOR_SET_H
#define BABELTRACE2_GRAPH_COMPONENT_DESCRIPTOR_SET_H

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

#include <babeltrace2/types.h>
#include <babeltrace2/logging.h>

#ifdef __cplusplus
extern "C" {
#endif

extern bt_component_descriptor_set *bt_component_descriptor_set_create(void);

typedef enum bt_component_descriptor_set_add_descriptor_status {
	BT_COMPONENT_DESCRIPTOR_SET_ADD_DESCRIPTOR_STATUS_OK		= __BT_FUNC_STATUS_OK,
	BT_COMPONENT_DESCRIPTOR_SET_ADD_DESCRIPTOR_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
} bt_component_descriptor_set_add_descriptor_status;

extern bt_component_descriptor_set_add_descriptor_status
bt_component_descriptor_set_add_descriptor(
		bt_component_descriptor_set *comp_descriptor_set,
		const bt_component_class *component_class,
		const bt_value *params);

extern bt_component_descriptor_set_add_descriptor_status
bt_component_descriptor_set_add_descriptor_with_initialize_method_data(
		bt_component_descriptor_set *comp_descriptor_set,
		const bt_component_class *component_class,
		const bt_value *params, void *init_method_data);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_GRAPH_COMPONENT_DESCRIPTOR_SET_H */
