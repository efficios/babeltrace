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
#include <assert.h>

struct bt_packet {
	struct bt_object base;
	struct bt_field *header;
	struct bt_field *context;
	struct bt_stream *stream;
	int frozen;
};

static inline
struct bt_private_packet *bt_private_packet_from_packet(
		struct bt_packet *packet)
{
	return (void *) packet;
}

static inline
struct bt_packet *bt_packet_borrow_from_private(
		struct bt_private_packet *private_packet)
{
	return (void *) private_packet;
}

BT_HIDDEN
void bt_packet_freeze(struct bt_packet *packet);

static inline
struct bt_stream *bt_packet_borrow_stream(
		struct bt_packet *packet)
{
	assert(packet);
	return packet->stream;
}

#endif /* BABELTRACE_CTF_IR_PACKET_INTERNAL_H */
