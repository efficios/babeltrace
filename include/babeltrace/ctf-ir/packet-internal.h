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

#include <stdbool.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/ctf-ir/packet.h>
#include <babeltrace/ctf-ir/fields.h>
#include <babeltrace/ctf-ir/stream.h>
#include <babeltrace/ctf-ir/field-wrapper-internal.h>
#include <babeltrace/object-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/ctf-ir/clock-value-set-internal.h>

struct bt_packet_prop_uint64 {
	enum bt_packet_property_availability avail;
	uint64_t value;
};

struct bt_packet {
	struct bt_object base;
	struct bt_field_wrapper *header;
	struct bt_field_wrapper *context;
	struct bt_stream *stream;
	struct bt_clock_value_set begin_cv_set;
	struct bt_clock_value_set end_cv_set;
	struct bt_packet_prop_uint64 discarded_event_counter;
	struct bt_packet_prop_uint64 seq_num;
	struct bt_packet_prop_uint64 discarded_event_count;
	struct bt_packet_prop_uint64 discarded_packet_count;
	bool props_are_set;

	struct {
		/*
		 * We keep this here to avoid keeping a reference on the
		 * previous packet object: those properties are
		 * snapshots of the previous packet's properties when
		 * calling bt_packet_create(). We know that the previous
		 * packet's properties do not change afterwards because
		 * we freeze the previous packet when bt_packet_create()
		 * is successful.
		 */
		enum bt_packet_previous_packet_availability avail;
		struct bt_packet_prop_uint64 discarded_event_counter;
		struct bt_packet_prop_uint64 seq_num;
		struct {
			enum bt_packet_property_availability avail;

			/* Owned by this (copy of previous packet's or NULL) */
			struct bt_clock_value *cv;
		} default_end_cv;
	} prev_packet_info;

	int frozen;
};

BT_HIDDEN
void _bt_packet_set_is_frozen(struct bt_packet *packet, bool is_frozen);

#ifdef BT_DEV_MODE
# define bt_packet_set_is_frozen	_bt_packet_set_is_frozen
#else
# define bt_packet_set_is_frozen(_packet, _is_frozen)
#endif /* BT_DEV_MODE */

BT_HIDDEN
struct bt_packet *bt_packet_new(struct bt_stream *stream);

BT_HIDDEN
void bt_packet_recycle(struct bt_packet *packet);

BT_HIDDEN
void bt_packet_destroy(struct bt_packet *packet);

/*
 * Sets a packet's properties using its current context fields and its
 * previous packet's snapshotted properties (if any). The packet context
 * field must not be modified until the packet is recycled.
 *
 * This function does NOT set the `props_are_set` flag (does not
 * validate the properties): call bt_packet_validate_properties() to
 * mark the properties as definitely cached.
 */
BT_HIDDEN
int bt_packet_set_properties(struct bt_packet *packet);

static inline
void bt_packet_invalidate_properties(struct bt_packet *packet)
{
	BT_ASSERT(packet);
	packet->props_are_set = false;
}

static inline
void bt_packet_validate_properties(struct bt_packet *packet)
{
	BT_ASSERT(packet);
	packet->props_are_set = true;
}

#endif /* BABELTRACE_CTF_IR_PACKET_INTERNAL_H */
