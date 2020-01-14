/*
 * Copyright 2019 Francis Deslauriers <francis.deslauriers@efficios.com>
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

#include <babeltrace2/babeltrace.h>

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "common/assert.h"
#include "common/common.h"
#include "common/macros.h"
#include "common/uuid.h"

#include "muxing.h"

struct message_to_compare {
	const bt_message *msg;
	const bt_trace *trace;
	const bt_stream *stream;
};

struct messages_to_compare {
	struct message_to_compare left;
	struct message_to_compare right;
};

static
int message_type_weight(const bt_message_type msg_type)
{
	int weight;

	switch (msg_type) {
	case BT_MESSAGE_TYPE_STREAM_BEGINNING:
		weight = 7;
		break;
	case BT_MESSAGE_TYPE_PACKET_BEGINNING:
		weight = 6;
		break;
	case BT_MESSAGE_TYPE_EVENT:
		weight = 5;
		break;
	case BT_MESSAGE_TYPE_DISCARDED_EVENTS:
		weight = 4;
		break;
	case BT_MESSAGE_TYPE_PACKET_END:
		weight = 3;
		break;
	case BT_MESSAGE_TYPE_MESSAGE_ITERATOR_INACTIVITY:
		weight = 2;
		break;
	case BT_MESSAGE_TYPE_DISCARDED_PACKETS:
		weight = 1;
		break;
	case BT_MESSAGE_TYPE_STREAM_END:
		weight = 0;
		break;
	default:
		bt_common_abort();
	}

	return weight;
}

/*
 * Compare 2 messages to order them in a determinitic way based on their
 * types.
 * Returns -1 is left mesage must go first
 * Returns 1 is right mesage must go first
 */
static
int compare_messages_by_type(struct messages_to_compare *msgs)
{
	bt_message_type left_msg_type = bt_message_get_type(msgs->left.msg);
	bt_message_type right_msg_type = bt_message_get_type(msgs->right.msg);

	return message_type_weight(right_msg_type) -
		message_type_weight(left_msg_type);
}

static
int compare_events(const bt_event *left_event, const bt_event *right_event)
{
	int ret = 0;
	const bt_event_class *left_event_class, *right_event_class;
	uint64_t left_event_class_id, right_event_class_id;
	const char *left_event_class_name, *right_event_class_name,
			*left_event_class_emf_uri, *right_event_class_emf_uri;
	bt_event_class_log_level left_event_class_log_level, right_event_class_log_level;
	bt_property_availability left_log_level_avail, right_log_level_avail;

	left_event_class = bt_event_borrow_class_const(left_event);
	right_event_class = bt_event_borrow_class_const(right_event);

	left_event_class_id = bt_event_class_get_id(left_event_class);
	right_event_class_id = bt_event_class_get_id(right_event_class);

	if (left_event_class_id > right_event_class_id) {
		ret = 1;
		goto end;
	} else if (left_event_class_id < right_event_class_id) {
		ret = -1;
		goto end;
	}

	left_event_class_name = bt_event_class_get_name(left_event_class);
	right_event_class_name = bt_event_class_get_name(right_event_class);
	if (left_event_class_name && right_event_class_name) {
		ret = strcmp(left_event_class_name, right_event_class_name);
		if (ret != 0) {
			goto end;
		}
	} else if (!left_event_class_name && right_event_class_name) {
		ret = -1;
		goto end;
	} else if (left_event_class_name && !right_event_class_name) {
		ret = 1;
		goto end;
	}

	left_log_level_avail = bt_event_class_get_log_level(left_event_class,
		&left_event_class_log_level);
	right_log_level_avail = bt_event_class_get_log_level(right_event_class,
		&right_event_class_log_level);

	if (left_log_level_avail == BT_PROPERTY_AVAILABILITY_AVAILABLE &&
			right_log_level_avail == BT_PROPERTY_AVAILABILITY_AVAILABLE) {
		ret = left_event_class_log_level - right_event_class_log_level;
		if (ret) {
			goto end;
		}
	} else if (left_log_level_avail == BT_PROPERTY_AVAILABILITY_AVAILABLE &&
			right_log_level_avail == BT_PROPERTY_AVAILABILITY_NOT_AVAILABLE) {
		ret = -1;
		goto end;
	} else if (left_log_level_avail == BT_PROPERTY_AVAILABILITY_NOT_AVAILABLE &&
			right_log_level_avail == BT_PROPERTY_AVAILABILITY_AVAILABLE) {
		ret = 1;
		goto end;
	}

	left_event_class_emf_uri = bt_event_class_get_emf_uri(left_event_class);
	right_event_class_emf_uri = bt_event_class_get_emf_uri(right_event_class);
	if (left_event_class_emf_uri && right_event_class_emf_uri) {
		ret = strcmp(left_event_class_emf_uri, right_event_class_emf_uri);
		if (ret != 0) {
			goto end;
		}
	} else if (!left_event_class_emf_uri && right_event_class_emf_uri) {
		ret = -1;
		goto end;
	} else if (left_event_class_emf_uri && !right_event_class_emf_uri) {
		ret = 1;
		goto end;
	}

end:
	return ret;
}

static
int compare_streams(const bt_stream *left_stream, const bt_stream *right_stream)
{
	int ret = 0;
	const char *left_stream_name, *right_stream_name,
	      *left_stream_class_name, *right_stream_class_name;
	const bt_stream_class *left_stream_class, *right_stream_class;

	/*
	 * No need to compare stream id as it was checked earlier and if we are
	 * here it means they are identical or both absent.
	 */
	BT_ASSERT(bt_stream_get_id(left_stream) ==
		bt_stream_get_id(right_stream));

	/* Compare stream name. */
	left_stream_name = bt_stream_get_name(left_stream);
	right_stream_name = bt_stream_get_name(right_stream);

	if (left_stream_name && right_stream_name) {
		ret = strcmp(left_stream_name, right_stream_name);
		if (ret != 0) {
			goto end;
		}
	} else if (!left_stream_name && right_stream_name) {
		ret = -1;
		goto end;
	} else if (left_stream_name && !right_stream_name) {
		ret = 1;
		goto end;
	}

	left_stream_class = bt_stream_borrow_class_const(left_stream);
	right_stream_class = bt_stream_borrow_class_const(right_stream);

	/*
	 * No need to compare stream class id as it was checked earlier and if
	 * we are here it means they are identical.
	 */
	BT_ASSERT(bt_stream_class_get_id(left_stream_class) ==
		bt_stream_class_get_id(right_stream_class));

	/* Compare stream class name. */
	left_stream_class_name = bt_stream_class_get_name(left_stream_class);
	right_stream_class_name = bt_stream_class_get_name(right_stream_class);

	if (left_stream_class_name && right_stream_class_name) {
		ret = strcmp(left_stream_class_name, right_stream_class_name);
		if (ret != 0) {
			goto end;
		}
	} else if (!left_stream_class_name && right_stream_class_name) {
		ret = -1;
		goto end;
	} else if (left_stream_class_name && !right_stream_class_name) {
		ret = 1;
		goto end;
	}

	/* Compare stream class automatic event class id assignment. */
	if (bt_stream_class_assigns_automatic_event_class_id(left_stream_class) &&
			!bt_stream_class_assigns_automatic_event_class_id(right_stream_class)) {
		ret = 1;
		goto end;
	} else if (!bt_stream_class_assigns_automatic_event_class_id(left_stream_class) &&
			bt_stream_class_assigns_automatic_event_class_id(right_stream_class)) {
		ret = -1;
		goto end;
	}

	/* Compare stream class automatic stream id assignment. */
	if (bt_stream_class_assigns_automatic_stream_id(left_stream_class) &&
			!bt_stream_class_assigns_automatic_stream_id(right_stream_class)) {
		ret = 1;
		goto end;
	} else if (!bt_stream_class_assigns_automatic_stream_id(left_stream_class) &&
			bt_stream_class_assigns_automatic_stream_id(right_stream_class)) {
		ret = -1;
		goto end;
	}

	/* Compare stream class support of discarded events. */
	if (bt_stream_class_supports_discarded_events(left_stream_class) &&
			!bt_stream_class_supports_discarded_events(right_stream_class)) {
		ret = 1;
		goto end;
	} else if (!bt_stream_class_supports_discarded_events(left_stream_class) &&
			bt_stream_class_supports_discarded_events(right_stream_class)) {
		ret = -1;
		goto end;
	}

	/* Compare stream class discarded events default clock snapshot. */
	if (bt_stream_class_discarded_events_have_default_clock_snapshots(left_stream_class) &&
			!bt_stream_class_discarded_events_have_default_clock_snapshots(right_stream_class)) {
		ret = 1;
		goto end;
	} else if (!bt_stream_class_discarded_events_have_default_clock_snapshots(left_stream_class) &&
			bt_stream_class_discarded_events_have_default_clock_snapshots(right_stream_class)) {
		ret = -1;
		goto end;
	}

	/* Compare stream class support of packets. */
	if (bt_stream_class_supports_packets(left_stream_class) &&
			!bt_stream_class_supports_packets(right_stream_class)) {
		ret = 1;
		goto end;
	} else if (!bt_stream_class_supports_packets(left_stream_class) &&
			bt_stream_class_supports_packets(right_stream_class)) {
		ret = -1;
		goto end;
	}

	if (!bt_stream_class_supports_packets(left_stream_class)) {
		/* Skip all packet related checks. */
		goto end;
	}

	/*
	 * Compare stream class presence of discarded packets beginning default
	 * clock snapshot.
	 */
	if (bt_stream_class_packets_have_beginning_default_clock_snapshot(left_stream_class) &&
			!bt_stream_class_packets_have_beginning_default_clock_snapshot(right_stream_class)) {
		ret = 1;
		goto end;
	} else if (!bt_stream_class_packets_have_beginning_default_clock_snapshot(left_stream_class) &&
			bt_stream_class_packets_have_beginning_default_clock_snapshot(right_stream_class)) {
		ret = -1;
		goto end;
	}

	/*
	 * Compare stream class presence of discarded packets end default clock
	 * snapshot.
	 */
	if (bt_stream_class_packets_have_end_default_clock_snapshot(left_stream_class) &&
			!bt_stream_class_packets_have_end_default_clock_snapshot(right_stream_class)) {
		ret = 1;
		goto end;
	} else if (!bt_stream_class_packets_have_end_default_clock_snapshot(left_stream_class) &&
			bt_stream_class_packets_have_end_default_clock_snapshot(right_stream_class)) {
		ret = -1;
		goto end;
	}

	/* Compare stream class support of discarded packets. */
	if (bt_stream_class_supports_discarded_packets(left_stream_class) &&
			!bt_stream_class_supports_discarded_packets(right_stream_class)) {
		ret = 1;
		goto end;
	} else if (!bt_stream_class_supports_discarded_packets(left_stream_class) &&
			bt_stream_class_supports_discarded_packets(right_stream_class)) {
		ret = -1;
		goto end;
	}

	/* Compare stream class discarded packets default clock snapshot. */
	if (bt_stream_class_discarded_packets_have_default_clock_snapshots(left_stream_class) &&
			!bt_stream_class_discarded_packets_have_default_clock_snapshots(right_stream_class)) {
		ret = 1;
		goto end;
	} else if (!bt_stream_class_discarded_packets_have_default_clock_snapshots(left_stream_class) &&
			bt_stream_class_discarded_packets_have_default_clock_snapshots(right_stream_class)) {
		ret = -1;
		goto end;
	}

end:
	return ret;
}

static
int compare_clock_snapshots_and_clock_classes(const bt_clock_snapshot *left_cs,
		const bt_clock_snapshot *right_cs)
{
	int ret;
	uint64_t left_freq, right_freq, left_prec, right_prec;
	uint64_t left_cs_value, right_cs_value;
	const bt_clock_class *left_clock_class, *right_clock_class;
	const char *left_clock_class_name, *right_clock_class_name;
	left_cs_value = bt_clock_snapshot_get_value(left_cs);
	right_cs_value = bt_clock_snapshot_get_value(right_cs);
	bt_uuid left_clock_class_uuid, right_clock_class_uuid;

	ret = left_cs_value - right_cs_value;
	if (ret != 0) {
		goto end;
	}

	left_clock_class = bt_clock_snapshot_borrow_clock_class_const(left_cs);
	right_clock_class = bt_clock_snapshot_borrow_clock_class_const(right_cs);

	left_clock_class_uuid = bt_clock_class_get_uuid(left_clock_class);
	right_clock_class_uuid = bt_clock_class_get_uuid(right_clock_class);

	if (left_clock_class_uuid && !right_clock_class_uuid) {
		ret = -1;
		goto end;
	} else if (!left_clock_class_uuid && right_clock_class_uuid) {
		ret = 1;
		goto end;
	} else if (left_clock_class_uuid && right_clock_class_uuid) {
		ret = bt_uuid_compare(left_clock_class_uuid,
			right_clock_class_uuid);
		if (ret != 0) {
			goto end;
		}
	}


	left_clock_class_name = bt_clock_class_get_name(left_clock_class);
	right_clock_class_name = bt_clock_class_get_name(right_clock_class);

	if (left_clock_class_name && !right_clock_class_name) {
		ret = -1;
		goto end;
	} else if (!left_clock_class_name && right_clock_class_name) {
		ret = 1;
		goto end;
	} else if (left_clock_class_name && right_clock_class_name) {
		ret = strcmp(left_clock_class_name, right_clock_class_name);
		if (ret != 0) {
			goto end;
		}
	}

	left_freq = bt_clock_class_get_frequency(left_clock_class);
	right_freq = bt_clock_class_get_frequency(right_clock_class);

	ret = right_freq - left_freq;
	if (ret != 0) {
		goto end;
	}

	left_prec = bt_clock_class_get_precision(left_clock_class);
	right_prec = bt_clock_class_get_precision(right_clock_class);

	ret = right_prec - left_prec;
	if (ret != 0) {
		goto end;
	}

end:
	return ret;
}

static
const bt_stream *borrow_stream(const bt_message *msg)
{
	bt_message_type msg_type = bt_message_get_type(msg);
	const bt_stream *stream = NULL;
	const bt_packet *packet = NULL;
	const bt_event *event = NULL;

	switch (msg_type) {
	case BT_MESSAGE_TYPE_STREAM_BEGINNING:
		stream = bt_message_stream_beginning_borrow_stream_const(msg);
		break;
	case BT_MESSAGE_TYPE_STREAM_END:
		stream = bt_message_stream_end_borrow_stream_const(msg);
		break;
	case BT_MESSAGE_TYPE_PACKET_BEGINNING:
		packet = bt_message_packet_beginning_borrow_packet_const(msg);
		stream = bt_packet_borrow_stream_const(packet);
		break;
	case BT_MESSAGE_TYPE_PACKET_END:
		packet = bt_message_packet_end_borrow_packet_const(msg);
		stream = bt_packet_borrow_stream_const(packet);
		break;
	case BT_MESSAGE_TYPE_EVENT:
		event = bt_message_event_borrow_event_const(msg);
		stream = bt_event_borrow_stream_const(event);
		break;
	case BT_MESSAGE_TYPE_DISCARDED_EVENTS:
		stream = bt_message_discarded_events_borrow_stream_const(msg);
		break;
	case BT_MESSAGE_TYPE_DISCARDED_PACKETS:
		stream = bt_message_discarded_packets_borrow_stream_const(msg);
		break;
	case BT_MESSAGE_TYPE_MESSAGE_ITERATOR_INACTIVITY:
		goto end;
	default:
		bt_common_abort();
	}

end:
	return stream;
}

static
const bt_trace *borrow_trace(const bt_message *msg)
{
	const bt_trace *trace = NULL;
	const bt_stream *stream = NULL;

	stream = borrow_stream(msg);
	if (stream) {
		trace = bt_stream_borrow_trace_const(stream);
	}

	return trace;
}

static
int compare_messages_by_trace_name(struct messages_to_compare *msgs)
{
	int ret = 0;
	const char *left_trace_name = NULL, *right_trace_name = NULL;

	if (msgs->left.trace && !msgs->right.trace) {
		ret = -1;
		goto end;
	}

	if (!msgs->left.trace && msgs->right.trace) {
		ret = 1;
		goto end;
	}

	if (!msgs->left.trace && !msgs->right.trace) {
		ret = 0;
		goto end;
	}

	left_trace_name = bt_trace_get_name(msgs->left.trace);
	right_trace_name = bt_trace_get_name(msgs->right.trace);

	if (left_trace_name && !right_trace_name) {
		ret = -1;
		goto end;
	}

	if (!left_trace_name && right_trace_name) {
		ret = 1;
		goto end;
	}

	if (!left_trace_name && !right_trace_name) {
		ret = 0;
		goto end;
	}

	ret = strcmp(left_trace_name, right_trace_name);
end:
	return ret;
}

static
int compare_messages_by_trace_uuid(struct messages_to_compare *msgs)
{
	int ret = 0;
	bt_uuid left_trace_uuid = NULL, right_trace_uuid = NULL;

	if (msgs->left.trace && !msgs->right.trace) {
		ret = -1;
		goto end;
	}

	if (!msgs->left.trace && msgs->right.trace) {
		ret = 1;
		goto end;
	}

	if (!msgs->left.trace && !msgs->right.trace) {
		ret = 0;
		goto end;
	}

	left_trace_uuid = bt_trace_get_uuid(msgs->left.trace);
	right_trace_uuid = bt_trace_get_uuid(msgs->right.trace);

	if (left_trace_uuid && !right_trace_uuid) {
		ret = -1;
		goto end;
	}

	if (!left_trace_uuid && right_trace_uuid) {
		ret = 1;
		goto end;
	}

	if (!left_trace_uuid && !right_trace_uuid) {
		ret = 0;
		goto end;
	}

	ret = bt_uuid_compare(left_trace_uuid, right_trace_uuid);
end:
	return ret;
}

static
int compare_messages_by_stream_class_id(struct messages_to_compare *msgs)
{
	int ret = 0;
	uint64_t left_stream_class_id = 0, right_stream_class_id = 0;

	if (msgs->left.stream && !msgs->right.stream) {
		ret = -1;
		goto end;
	}

	if (!msgs->left.stream && msgs->right.stream) {
		ret = 1;
		goto end;
	}

	if (!msgs->left.stream && !msgs->right.stream) {
		ret = 0;
		goto end;
	}

	left_stream_class_id = bt_stream_class_get_id(
		bt_stream_borrow_class_const(msgs->left.stream));

	right_stream_class_id = bt_stream_class_get_id(
		bt_stream_borrow_class_const(msgs->right.stream));

	if (left_stream_class_id == right_stream_class_id) {
		ret = 0;
		goto end;
	}

	ret = (left_stream_class_id < right_stream_class_id) ? -1 : 1;

end:
	return ret;
}

static
int compare_messages_by_stream_id(struct messages_to_compare *msgs)
{
	int ret = 0;
	uint64_t left_stream_id = 0, right_stream_id = 0;

	if (msgs->left.stream && !msgs->right.stream) {
		ret = -1;
		goto end;
	}

	if (!msgs->left.stream && msgs->right.stream) {
		ret = 1;
		goto end;
	}

	if (!msgs->left.stream && !msgs->right.stream) {
		ret = 0;
		goto end;
	}

	left_stream_id = bt_stream_get_id(msgs->left.stream);
	right_stream_id = bt_stream_get_id(msgs->right.stream);

	if (left_stream_id == right_stream_id) {
		ret = 0;
		goto end;
	}

	ret = (left_stream_id < right_stream_id) ? -1 : 1;

end:
	return ret;
}

static
int compare_messages_same_type(struct messages_to_compare *msgs)
{
	int ret = 0;

	/*
	 * Both messages are of the same type, we must compare characterics of
	 * the messages such as the attributes of the event in a event message.
	 */
	BT_ASSERT(bt_message_get_type(msgs->left.msg) ==
		bt_message_get_type(msgs->right.msg));

	switch (bt_message_get_type(msgs->left.msg)) {
	case BT_MESSAGE_TYPE_STREAM_BEGINNING:
		/* Fall-through */
	case BT_MESSAGE_TYPE_STREAM_END:
		/* Fall-through */
	case BT_MESSAGE_TYPE_PACKET_BEGINNING:
		/* Fall-through */
	case BT_MESSAGE_TYPE_PACKET_END:
		ret = compare_streams(msgs->left.stream, msgs->right.stream);
		if (ret) {
			goto end;
		}

		break;
	case BT_MESSAGE_TYPE_EVENT:
	{
		const bt_event *left_event, *right_event;
		left_event = bt_message_event_borrow_event_const(msgs->left.msg);
		right_event = bt_message_event_borrow_event_const(msgs->right.msg);

		ret = compare_events(left_event, right_event);
		if (ret) {
			goto end;
		}

		ret = compare_streams(msgs->left.stream, msgs->right.stream);
		if (ret) {
			goto end;
		}
		break;
	}
	case BT_MESSAGE_TYPE_DISCARDED_EVENTS:
	{
		const bt_stream_class *left_stream_class;
		bt_property_availability left_event_count_avail,
					 right_event_count_avail;
		uint64_t left_event_count, right_event_count;

		/*
		 * Compare streams first to check if there is a
		 * mismatch about discarded event related configuration
		 * in the stream class.
		 */
		ret = compare_streams(msgs->left.stream, msgs->right.stream);
		if (ret) {
			goto end;
		}

		left_stream_class = bt_stream_borrow_class_const(msgs->left.stream);
		if (bt_stream_class_discarded_events_have_default_clock_snapshots(
				left_stream_class)) {
			const bt_clock_snapshot *left_beg_cs =
				bt_message_discarded_events_borrow_beginning_default_clock_snapshot_const(msgs->left.msg);
			const bt_clock_snapshot *right_beg_cs =
				bt_message_discarded_events_borrow_beginning_default_clock_snapshot_const(msgs->right.msg);
			const bt_clock_snapshot *left_end_cs =
				bt_message_discarded_events_borrow_end_default_clock_snapshot_const(msgs->left.msg);
			const bt_clock_snapshot *right_end_cs =
				bt_message_discarded_events_borrow_end_default_clock_snapshot_const(msgs->right.msg);

			ret = compare_clock_snapshots_and_clock_classes(
				left_beg_cs, right_beg_cs);
			if (ret) {
				goto end;
			}

			ret = compare_clock_snapshots_and_clock_classes(
				left_end_cs, right_end_cs);
			if (ret) {
				goto end;
			}
		}

		left_event_count_avail =
			bt_message_discarded_events_get_count(
				msgs->left.msg, &left_event_count);
		right_event_count_avail =
			bt_message_discarded_events_get_count(
				msgs->right.msg, &right_event_count);
		if (left_event_count_avail == BT_PROPERTY_AVAILABILITY_AVAILABLE &&
			right_event_count_avail == BT_PROPERTY_AVAILABILITY_AVAILABLE) {
			ret = left_event_count - right_event_count;
			if (ret != 0) {
				goto end;
			}
		} else if (left_event_count_avail == BT_PROPERTY_AVAILABILITY_AVAILABLE &&
			right_event_count_avail == BT_PROPERTY_AVAILABILITY_NOT_AVAILABLE) {
			ret = -1;
			goto end;
		} else if (left_event_count_avail == BT_PROPERTY_AVAILABILITY_NOT_AVAILABLE &&
			right_event_count_avail == BT_PROPERTY_AVAILABILITY_AVAILABLE) {
			ret = 1;
			goto end;
		}

		break;
	}
	case BT_MESSAGE_TYPE_DISCARDED_PACKETS:
	{
		const bt_stream_class *left_stream_class;
		bt_property_availability left_packet_count_avail,
					 right_packet_count_avail;
		uint64_t left_packet_count, right_packet_count;

		/*
		 * Compare streams first to check if there is a
		 * mismatch about discarded packets related
		 * configuration in the stream class.
		 */
		ret = compare_streams(msgs->left.stream, msgs->right.stream);
		if (ret) {
			goto end;
		}

		left_stream_class = bt_stream_borrow_class_const(msgs->left.stream);

		if (bt_stream_class_discarded_packets_have_default_clock_snapshots(
				left_stream_class)) {
			const bt_clock_snapshot *left_beg_cs =
				bt_message_discarded_packets_borrow_beginning_default_clock_snapshot_const(msgs->left.msg);
			const bt_clock_snapshot *right_beg_cs =
				bt_message_discarded_packets_borrow_beginning_default_clock_snapshot_const(msgs->right.msg);
			const bt_clock_snapshot *left_end_cs =
				bt_message_discarded_packets_borrow_end_default_clock_snapshot_const(msgs->left.msg);
			const bt_clock_snapshot *right_end_cs =
				bt_message_discarded_packets_borrow_end_default_clock_snapshot_const(msgs->right.msg);

			ret = compare_clock_snapshots_and_clock_classes(
				left_beg_cs, right_beg_cs);
			if (ret) {
				goto end;
			}

			ret = compare_clock_snapshots_and_clock_classes(
				left_end_cs, right_end_cs);
			if (ret) {
				goto end;
			}
		}

		left_packet_count_avail = bt_message_discarded_packets_get_count(
			msgs->left.msg, &left_packet_count);
		right_packet_count_avail = bt_message_discarded_packets_get_count(
			msgs->right.msg, &right_packet_count);
		if (left_packet_count_avail == BT_PROPERTY_AVAILABILITY_AVAILABLE &&
			right_packet_count_avail == BT_PROPERTY_AVAILABILITY_AVAILABLE) {
			ret = left_packet_count - right_packet_count;
			if (ret != 0) {
				goto end;
			}
		} else if (left_packet_count_avail == BT_PROPERTY_AVAILABILITY_AVAILABLE &&
			right_packet_count_avail == BT_PROPERTY_AVAILABILITY_NOT_AVAILABLE) {
			ret = -1;
			goto end;
		} else if (left_packet_count_avail == BT_PROPERTY_AVAILABILITY_NOT_AVAILABLE &&
			right_packet_count_avail == BT_PROPERTY_AVAILABILITY_AVAILABLE) {
			ret = 1;
			goto end;
		}

		break;
	}
	case BT_MESSAGE_TYPE_MESSAGE_ITERATOR_INACTIVITY:
	{
		const bt_clock_snapshot *left_cs =
			bt_message_message_iterator_inactivity_borrow_clock_snapshot_const(msgs->left.msg);
		const bt_clock_snapshot *right_cs =
			bt_message_message_iterator_inactivity_borrow_clock_snapshot_const(msgs->right.msg);

		ret = compare_clock_snapshots_and_clock_classes(
			left_cs, right_cs);
		if (ret != 0) {
			goto end;
		}

		break;
	}
	default:
		bt_common_abort();
	}

end:
	return ret;
}

BT_HIDDEN
int common_muxing_compare_messages(const bt_message *left_msg,
		const bt_message *right_msg)
{
	int ret = 0;
	struct messages_to_compare msgs;

	BT_ASSERT(left_msg != right_msg);

	msgs.left.msg = left_msg;
	msgs.left.trace = borrow_trace(left_msg);
	msgs.left.stream = borrow_stream(left_msg);

	msgs.right.msg = right_msg;
	msgs.right.trace = borrow_trace(right_msg);
	msgs.right.stream = borrow_stream(right_msg);

	/* Same timestamp: compare trace UUIDs. */
	ret = compare_messages_by_trace_uuid(&msgs);
	if (ret) {
		goto end;
	}

	/* Same timestamp and trace UUID: compare trace names. */
	ret = compare_messages_by_trace_name(&msgs);
	if (ret) {
		goto end;
	}

	/*
	 * Same timestamp, trace name, and trace UUID: compare stream class
	 * IDs.
	 */
	ret = compare_messages_by_stream_class_id(&msgs);
	if (ret) {
		goto end;
	}

	/*
	 * Same timestamp, trace name, trace UUID, and stream class ID: compare
	 * stream IDs.
	 */
	ret = compare_messages_by_stream_id(&msgs);
	if (ret) {
		goto end;
	}

	if (bt_message_get_type(msgs.left.msg) !=
			bt_message_get_type(msgs.right.msg)) {
		/*
		 * The messages are of different type, we order (arbitrarily)
		 * in the following way:
		 * SB < PB < EV < DE < MI < PE < DP < SE
		 */
		ret = compare_messages_by_type(&msgs);
		if (ret) {
			goto end;
		}
	} else {
		/* The messages are of the same type. */
		ret = compare_messages_same_type(&msgs);
		if (ret) {
			goto end;
		}
	}

end:
	return ret;
}
