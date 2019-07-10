#ifndef BABELTRACE2_TRACE_IR_PACKET_H
#define BABELTRACE2_TRACE_IR_PACKET_H

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

extern bt_packet *bt_packet_create(const bt_stream *stream);

extern bt_stream *bt_packet_borrow_stream(bt_packet *packet);

extern
bt_field *bt_packet_borrow_context_field(bt_packet *packet);

typedef enum bt_packet_move_context_field_status {
	BT_PACKET_MOVE_CONTEXT_FIELD_STATUS_OK	= __BT_FUNC_STATUS_OK,
} bt_packet_move_context_field_status;

extern
bt_packet_move_context_field_status bt_packet_move_context_field(
		bt_packet *packet, bt_packet_context_field *context);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE2_TRACE_IR_PACKET_H */
