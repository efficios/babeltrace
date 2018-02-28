#ifndef BABELTRACE_CTF_IR_PACKET_INTERNAL_H
#define BABELTRACE_CTF_IR_PACKET_INTERNAL_H

/*
 * Babeltrace - CTF IR: Stream packet internal
 *
 * Copyright 2016 Philippe Proulx <pproulx@efficios.com>
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

#include <babeltrace/ctf-ir/fields.h>
#include <babeltrace/ctf-ir/stream.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/assert-internal.h>

struct bt_packet {
	struct bt_object base;
	struct bt_field *header;
	struct bt_field *context;
	struct bt_stream *stream;
	int frozen;
};

BT_HIDDEN
void _bt_packet_freeze(struct bt_packet *packet);

#ifdef BT_DEV_MODE
# define bt_packet_freeze	_bt_packet_freeze
#else
# define bt_packet_freeze
#endif /* BT_DEV_MODE */

static inline
struct bt_stream *bt_packet_borrow_stream(
		struct bt_packet *packet)
{
	BT_ASSERT(packet);
	return packet->stream;
}

#endif /* BABELTRACE_CTF_IR_PACKET_INTERNAL_H */
