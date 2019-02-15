#ifndef BABELTRACE_TRACE_IR_PACKET_H
#define BABELTRACE_TRACE_IR_PACKET_H

/*
 * Copyright 2016-2018 Philippe Proulx <pproulx@efficios.com>
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
 *
 * The Common Trace Format (CTF) Specification is available at
 * http://www.efficios.com/ctf
 */

/* For bt_packet, bt_packet_context_field, bt_stream */
#include <babeltrace/types.h>

/* For bt_packet_status */
#include <babeltrace/trace-ir/packet-const.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern bt_packet *bt_packet_create(const bt_stream *stream);

extern bt_stream *bt_packet_borrow_stream(bt_packet *packet);

extern
bt_field *bt_packet_borrow_context_field(bt_packet *packet);

extern
bt_packet_status bt_packet_move_context_field(bt_packet *packet,
		bt_packet_context_field *context);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_TRACE_IR_PACKET_H */
