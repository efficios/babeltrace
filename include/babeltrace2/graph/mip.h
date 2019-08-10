#ifndef BABELTRACE2_GRAPH_MIP_H
#define BABELTRACE2_GRAPH_MIP_H

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

#include <babeltrace2/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum bt_get_greatest_operative_mip_version_status {
	BT_GET_GREATEST_OPERATIVE_MIP_VERSION_STATUS_OK			= __BT_FUNC_STATUS_OK,
	BT_GET_GREATEST_OPERATIVE_MIP_VERSION_STATUS_ERROR		= __BT_FUNC_STATUS_ERROR,
	BT_GET_GREATEST_OPERATIVE_MIP_VERSION_STATUS_MEMORY_ERROR	= __BT_FUNC_STATUS_MEMORY_ERROR,
	BT_GET_GREATEST_OPERATIVE_MIP_VERSION_STATUS_NO_MATCH		= __BT_FUNC_STATUS_NO_MATCH,
} bt_get_greatest_operative_mip_version_status;

extern bt_get_greatest_operative_mip_version_status
bt_get_greatest_operative_mip_version(
		const bt_component_descriptor_set *comp_descriptor_set,
		bt_logging_level log_level, uint64_t *operative_mip_version);

extern uint64_t bt_get_maximal_mip_version(void);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_GRAPH_MIP_H */
