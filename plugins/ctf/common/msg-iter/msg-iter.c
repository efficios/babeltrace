/*
 * Babeltrace - CTF message iterator
 *
 * Copyright (c) 2015-2018 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2015-2018 Philippe Proulx <pproulx@efficios.com>
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

#define BT_LOG_TAG "PLUGIN-CTF-MSG-ITER"
#include "logging.h"

#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <babeltrace/assert-internal.h>
#include <string.h>
#include <babeltrace/babeltrace.h>
#include <babeltrace/common-internal.h>
#include <glib.h>
#include <stdlib.h>

#include "msg-iter.h"
#include "../bfcr/bfcr.h"

struct bt_msg_iter;

/* A visit stack entry */
struct stack_entry {
	/*
	 * Current base field, one of:
	 *
	 *   * string
	 *   * structure
	 *   * array
	 *   * sequence
	 *   * variant
	 *
	 * Field is borrowed.
	 */
	bt_field *base;

	/* Index of next field to set */
	size_t index;
};

/* Visit stack */
struct stack {
	/* Entries (struct stack_entry) */
	GArray *entries;

	/* Number of active entries */
	size_t size;
};

/* State */
enum state {
	STATE_INIT,
	STATE_DSCOPE_TRACE_PACKET_HEADER_BEGIN,
	STATE_DSCOPE_TRACE_PACKET_HEADER_CONTINUE,
	STATE_AFTER_TRACE_PACKET_HEADER,
	STATE_DSCOPE_STREAM_PACKET_CONTEXT_BEGIN,
	STATE_DSCOPE_STREAM_PACKET_CONTEXT_CONTINUE,
	STATE_AFTER_STREAM_PACKET_CONTEXT,
	STATE_CHECK_EMIT_MSG_STREAM_BEGINNING,
	STATE_EMIT_MSG_STREAM_BEGINNING,
	STATE_EMIT_MSG_STREAM_ACTIVITY_BEGINNING,
	STATE_CHECK_EMIT_MSG_DISCARDED_EVENTS,
	STATE_CHECK_EMIT_MSG_DISCARDED_PACKETS,
	STATE_EMIT_MSG_DISCARDED_EVENTS,
	STATE_EMIT_MSG_DISCARDED_PACKETS,
	STATE_EMIT_MSG_PACKET_BEGINNING,
	STATE_DSCOPE_EVENT_HEADER_BEGIN,
	STATE_DSCOPE_EVENT_HEADER_CONTINUE,
	STATE_AFTER_EVENT_HEADER,
	STATE_DSCOPE_EVENT_COMMON_CONTEXT_BEGIN,
	STATE_DSCOPE_EVENT_COMMON_CONTEXT_CONTINUE,
	STATE_DSCOPE_EVENT_SPEC_CONTEXT_BEGIN,
	STATE_DSCOPE_EVENT_SPEC_CONTEXT_CONTINUE,
	STATE_DSCOPE_EVENT_PAYLOAD_BEGIN,
	STATE_DSCOPE_EVENT_PAYLOAD_CONTINUE,
	STATE_EMIT_MSG_EVENT,
	STATE_SKIP_PACKET_PADDING,
	STATE_EMIT_MSG_PACKET_END_MULTI,
	STATE_EMIT_MSG_PACKET_END_SINGLE,
	STATE_CHECK_EMIT_MSG_STREAM_ACTIVITY_END,
	STATE_EMIT_MSG_STREAM_ACTIVITY_END,
	STATE_EMIT_MSG_STREAM_END,
	STATE_DONE,
};

struct end_of_packet_snapshots {
	uint64_t discarded_events;
	uint64_t packets;
	uint64_t beginning_clock;
	uint64_t end_clock;
};

/* CTF message iterator */
struct bt_msg_iter {
	/* Visit stack */
	struct stack *stack;

	/* Current message iterator to create messages (weak) */
	bt_self_message_iterator *msg_iter;

	/*
	 * True to emit stream beginning and stream activity beginning
	 * messages.
	 */
	bool emit_stream_begin_msg;

	/* True to emit stream end and stream activity end messages */
	bool emit_stream_end_msg;

	/* True to set the stream */
	bool set_stream;

	/*
	 * Current dynamic scope field pointer.
	 *
	 * This is set by read_dscope_begin_state() and contains the
	 * value of one of the pointers in `dscopes` below.
	 */
	bt_field *cur_dscope_field;

	/*
	 * True if we're done filling a string field from a text
	 * array/sequence payload.
	 */
	bool done_filling_string;

	/* Trace and classes */
	/* True to set IR fields */
	bool set_ir_fields;

	struct {
		struct ctf_trace_class *tc;
		struct ctf_stream_class *sc;
		struct ctf_event_class *ec;
	} meta;

	/* Current packet context field wrapper (NULL if not created yet) */
	bt_packet_context_field *packet_context_field;

	/* Current packet (NULL if not created yet) */
	bt_packet *packet;

	/* Current stream (NULL if not set yet) */
	bt_stream *stream;

	/* Current event (NULL if not created yet) */
	bt_event *event;

	/* Current event message (NULL if not created yet) */
	bt_message *event_msg;

	/* Database of current dynamic scopes */
	struct {
		bt_field *stream_packet_context;
		bt_field *event_common_context;
		bt_field *event_spec_context;
		bt_field *event_payload;
	} dscopes;

	/* Current state */
	enum state state;

	/* Current medium buffer data */
	struct {
		/* Last address provided by medium */
		const uint8_t *addr;

		/* Buffer size provided by medium (bytes) */
		size_t sz;

		/* Offset within whole packet of addr (bits) */
		size_t packet_offset;

		/* Current position from addr (bits) */
		size_t at;

		/* Position of the last event header from addr (bits) */
		size_t last_eh_at;
	} buf;

	/* Binary type reader */
	struct bt_bfcr *bfcr;

	/* Current medium data */
	struct {
		struct bt_msg_iter_medium_ops medops;
		size_t max_request_sz;
		void *data;
	} medium;

	/* Current packet size (bits) (-1 if unknown) */
	int64_t cur_exp_packet_total_size;

	/* Current content size (bits) (-1 if unknown) */
	int64_t cur_exp_packet_content_size;

	/* Current stream class ID */
	int64_t cur_stream_class_id;

	/* Current event class ID */
	int64_t cur_event_class_id;

	/* Current data stream ID */
	int64_t cur_data_stream_id;

	/*
	 * Offset, in the underlying media, of the current packet's
	 * start (-1 if unknown).
	 */
	off_t cur_packet_offset;

	/* Default clock's current value */
	uint64_t default_clock_snapshot;

	/* End of current packet snapshots */
	struct end_of_packet_snapshots snapshots;

	/* End of previous packet snapshots */
	struct end_of_packet_snapshots prev_packet_snapshots;

	/* Stored values (for sequence lengths, variant tags) */
	GArray *stored_values;
};

static inline
const char *state_string(enum state state)
{
	switch (state) {
	case STATE_INIT:
		return "STATE_INIT";
	case STATE_DSCOPE_TRACE_PACKET_HEADER_BEGIN:
		return "STATE_DSCOPE_TRACE_PACKET_HEADER_BEGIN";
	case STATE_DSCOPE_TRACE_PACKET_HEADER_CONTINUE:
		return "STATE_DSCOPE_TRACE_PACKET_HEADER_CONTINUE";
	case STATE_AFTER_TRACE_PACKET_HEADER:
		return "STATE_AFTER_TRACE_PACKET_HEADER";
	case STATE_DSCOPE_STREAM_PACKET_CONTEXT_BEGIN:
		return "STATE_DSCOPE_STREAM_PACKET_CONTEXT_BEGIN";
	case STATE_DSCOPE_STREAM_PACKET_CONTEXT_CONTINUE:
		return "STATE_DSCOPE_STREAM_PACKET_CONTEXT_CONTINUE";
	case STATE_AFTER_STREAM_PACKET_CONTEXT:
		return "STATE_AFTER_STREAM_PACKET_CONTEXT";
	case STATE_EMIT_MSG_STREAM_BEGINNING:
		return "STATE_EMIT_MSG_STREAM_BEGINNING";
	case STATE_EMIT_MSG_STREAM_ACTIVITY_BEGINNING:
		return "STATE_EMIT_MSG_STREAM_ACTIVITY_BEGINNING";
	case STATE_EMIT_MSG_PACKET_BEGINNING:
		return "STATE_EMIT_MSG_PACKET_BEGINNING";
	case STATE_EMIT_MSG_DISCARDED_EVENTS:
		return "STATE_EMIT_MSG_DISCARDED_EVENTS";
	case STATE_EMIT_MSG_DISCARDED_PACKETS:
		return "STATE_EMIT_MSG_DISCARDED_PACKETS";
	case STATE_DSCOPE_EVENT_HEADER_BEGIN:
		return "STATE_DSCOPE_EVENT_HEADER_BEGIN";
	case STATE_DSCOPE_EVENT_HEADER_CONTINUE:
		return "STATE_DSCOPE_EVENT_HEADER_CONTINUE";
	case STATE_AFTER_EVENT_HEADER:
		return "STATE_AFTER_EVENT_HEADER";
	case STATE_DSCOPE_EVENT_COMMON_CONTEXT_BEGIN:
		return "STATE_DSCOPE_EVENT_COMMON_CONTEXT_BEGIN";
	case STATE_DSCOPE_EVENT_COMMON_CONTEXT_CONTINUE:
		return "STATE_DSCOPE_EVENT_COMMON_CONTEXT_CONTINUE";
	case STATE_DSCOPE_EVENT_SPEC_CONTEXT_BEGIN:
		return "STATE_DSCOPE_EVENT_SPEC_CONTEXT_BEGIN";
	case STATE_DSCOPE_EVENT_SPEC_CONTEXT_CONTINUE:
		return "STATE_DSCOPE_EVENT_SPEC_CONTEXT_CONTINUE";
	case STATE_DSCOPE_EVENT_PAYLOAD_BEGIN:
		return "STATE_DSCOPE_EVENT_PAYLOAD_BEGIN";
	case STATE_DSCOPE_EVENT_PAYLOAD_CONTINUE:
		return "STATE_DSCOPE_EVENT_PAYLOAD_CONTINUE";
	case STATE_EMIT_MSG_EVENT:
		return "STATE_EMIT_MSG_EVENT";
	case STATE_SKIP_PACKET_PADDING:
		return "STATE_SKIP_PACKET_PADDING";
	case STATE_EMIT_MSG_PACKET_END_MULTI:
		return "STATE_EMIT_MSG_PACKET_END_MULTI";
	case STATE_EMIT_MSG_PACKET_END_SINGLE:
		return "STATE_EMIT_MSG_PACKET_END_SINGLE";
	case STATE_EMIT_MSG_STREAM_ACTIVITY_END:
		return "STATE_EMIT_MSG_STREAM_ACTIVITY_END";
	case STATE_EMIT_MSG_STREAM_END:
		return "STATE_EMIT_MSG_STREAM_END";
	case STATE_DONE:
		return "STATE_DONE";
	default:
		return "(unknown)";
	}
}

static
int bt_msg_iter_switch_packet(struct bt_msg_iter *notit);

static
struct stack *stack_new(struct bt_msg_iter *notit)
{
	struct stack *stack = NULL;

	stack = g_new0(struct stack, 1);
	if (!stack) {
		BT_LOGE_STR("Failed to allocate one stack.");
		goto error;
	}

	stack->entries = g_array_new(FALSE, TRUE, sizeof(struct stack_entry));
	if (!stack->entries) {
		BT_LOGE_STR("Failed to allocate a GArray.");
		goto error;
	}

	BT_LOGD("Created stack: notit-addr=%p, stack-addr=%p", notit, stack);
	goto end;

error:
	g_free(stack);
	stack = NULL;

end:
	return stack;
}

static
void stack_destroy(struct stack *stack)
{
	BT_ASSERT(stack);
	BT_LOGD("Destroying stack: addr=%p", stack);

	if (stack->entries) {
		g_array_free(stack->entries, TRUE);
	}

	g_free(stack);
}

static
void stack_push(struct stack *stack, bt_field *base)
{
	struct stack_entry *entry;

	BT_ASSERT(stack);
	BT_ASSERT(base);
	BT_LOGV("Pushing base field on stack: stack-addr=%p, "
		"stack-size-before=%zu, stack-size-after=%zu",
		stack, stack->size, stack->size + 1);

	if (stack->entries->len == stack->size) {
		g_array_set_size(stack->entries, stack->size + 1);
	}

	entry = &g_array_index(stack->entries, struct stack_entry, stack->size);
	entry->base = base;
	entry->index = 0;
	stack->size++;
}

static inline
unsigned int stack_size(struct stack *stack)
{
	BT_ASSERT(stack);
	return stack->size;
}

static
void stack_pop(struct stack *stack)
{
	BT_ASSERT(stack);
	BT_ASSERT(stack_size(stack));
	BT_LOGV("Popping from stack: "
		"stack-addr=%p, stack-size-before=%zu, stack-size-after=%zu",
		stack, stack->size, stack->size - 1);
	stack->size--;
}

static inline
struct stack_entry *stack_top(struct stack *stack)
{
	BT_ASSERT(stack);
	BT_ASSERT(stack_size(stack));
	return &g_array_index(stack->entries, struct stack_entry,
		stack->size - 1);
}

static inline
bool stack_empty(struct stack *stack)
{
	return stack_size(stack) == 0;
}

static
void stack_clear(struct stack *stack)
{
	BT_ASSERT(stack);
	stack->size = 0;
}

static inline
enum bt_msg_iter_status msg_iter_status_from_m_status(
		enum bt_msg_iter_medium_status m_status)
{
	/* They are the same */
	return (int) m_status;
}

static inline
size_t buf_size_bits(struct bt_msg_iter *notit)
{
	return notit->buf.sz * 8;
}

static inline
size_t buf_available_bits(struct bt_msg_iter *notit)
{
	return buf_size_bits(notit) - notit->buf.at;
}

static inline
size_t packet_at(struct bt_msg_iter *notit)
{
	return notit->buf.packet_offset + notit->buf.at;
}

static inline
void buf_consume_bits(struct bt_msg_iter *notit, size_t incr)
{
	BT_LOGV("Advancing cursor: notit-addr=%p, cur-before=%zu, cur-after=%zu",
		notit, notit->buf.at, notit->buf.at + incr);
	notit->buf.at += incr;
}

static
enum bt_msg_iter_status request_medium_bytes(
		struct bt_msg_iter *notit)
{
	uint8_t *buffer_addr = NULL;
	size_t buffer_sz = 0;
	enum bt_msg_iter_medium_status m_status;

	BT_LOGV("Calling user function (request bytes): notit-addr=%p, "
		"request-size=%zu", notit, notit->medium.max_request_sz);
	m_status = notit->medium.medops.request_bytes(
		notit->medium.max_request_sz, &buffer_addr,
		&buffer_sz, notit->medium.data);
	BT_LOGV("User function returned: status=%s, buf-addr=%p, buf-size=%zu",
		bt_msg_iter_medium_status_string(m_status),
		buffer_addr, buffer_sz);
	if (m_status == BT_MSG_ITER_MEDIUM_STATUS_OK) {
		BT_ASSERT(buffer_sz != 0);

		/* New packet offset is old one + old size (in bits) */
		notit->buf.packet_offset += buf_size_bits(notit);

		/* Restart at the beginning of the new medium buffer */
		notit->buf.at = 0;
		notit->buf.last_eh_at = SIZE_MAX;

		/* New medium buffer size */
		notit->buf.sz = buffer_sz;

		/* New medium buffer address */
		notit->buf.addr = buffer_addr;

		BT_LOGV("User function returned new bytes: "
			"packet-offset=%zu, cur=%zu, size=%zu, addr=%p",
			notit->buf.packet_offset, notit->buf.at,
			notit->buf.sz, notit->buf.addr);
		BT_LOGV_MEM(buffer_addr, buffer_sz, "Returned bytes at %p:",
			buffer_addr);
	} else if (m_status == BT_MSG_ITER_MEDIUM_STATUS_EOF) {
		/*
		 * User returned end of stream: validate that we're not
		 * in the middle of a packet header, packet context, or
		 * event.
		 */
		if (notit->cur_exp_packet_total_size >= 0) {
			if (packet_at(notit) ==
					notit->cur_exp_packet_total_size) {
				goto end;
			}
		} else {
			if (packet_at(notit) == 0) {
				goto end;
			}

			if (notit->buf.last_eh_at != SIZE_MAX &&
					notit->buf.at == notit->buf.last_eh_at) {
				goto end;
			}
		}

		/* All other states are invalid */
		BT_LOGW("User function returned %s, but message iterator is in an unexpected state: "
			"state=%s, cur-packet-size=%" PRId64 ", cur=%zu, "
			"packet-cur=%zu, last-eh-at=%zu",
			bt_msg_iter_medium_status_string(m_status),
			state_string(notit->state),
			notit->cur_exp_packet_total_size,
			notit->buf.at, packet_at(notit),
			notit->buf.last_eh_at);
		m_status = BT_MSG_ITER_MEDIUM_STATUS_ERROR;
	} else if (m_status < 0) {
		BT_LOGW("User function failed: status=%s",
			bt_msg_iter_medium_status_string(m_status));
	}

end:
	return msg_iter_status_from_m_status(m_status);
}

static inline
enum bt_msg_iter_status buf_ensure_available_bits(
		struct bt_msg_iter *notit)
{
	enum bt_msg_iter_status status = BT_MSG_ITER_STATUS_OK;

	if (unlikely(buf_available_bits(notit) == 0)) {
		/*
		 * This _cannot_ return BT_MSG_ITER_STATUS_OK
		 * _and_ no bits.
		 */
		status = request_medium_bytes(notit);
	}

	return status;
}

static
enum bt_msg_iter_status read_dscope_begin_state(
		struct bt_msg_iter *notit,
		struct ctf_field_class *dscope_fc,
		enum state done_state, enum state continue_state,
		bt_field *dscope_field)
{
	enum bt_msg_iter_status status = BT_MSG_ITER_STATUS_OK;
	enum bt_bfcr_status bfcr_status;
	size_t consumed_bits;

	notit->cur_dscope_field = dscope_field;
	BT_LOGV("Starting BFCR: notit-addr=%p, bfcr-addr=%p, fc-addr=%p",
		notit, notit->bfcr, dscope_fc);
	consumed_bits = bt_bfcr_start(notit->bfcr, dscope_fc,
		notit->buf.addr, notit->buf.at, packet_at(notit),
		notit->buf.sz, &bfcr_status);
	BT_LOGV("BFCR consumed bits: size=%zu", consumed_bits);

	switch (bfcr_status) {
	case BT_BFCR_STATUS_OK:
		/* Field class was read completely */
		BT_LOGV_STR("Field was completely decoded.");
		notit->state = done_state;
		break;
	case BT_BFCR_STATUS_EOF:
		BT_LOGV_STR("BFCR needs more data to decode field completely.");
		notit->state = continue_state;
		break;
	default:
		BT_LOGW("BFCR failed to start: notit-addr=%p, bfcr-addr=%p, "
			"status=%s", notit, notit->bfcr,
			bt_bfcr_status_string(bfcr_status));
		status = BT_MSG_ITER_STATUS_ERROR;
		goto end;
	}

	/* Consume bits now since we know we're not in an error state */
	buf_consume_bits(notit, consumed_bits);

end:
	return status;
}

static
enum bt_msg_iter_status read_dscope_continue_state(
		struct bt_msg_iter *notit, enum state done_state)
{
	enum bt_msg_iter_status status = BT_MSG_ITER_STATUS_OK;
	enum bt_bfcr_status bfcr_status;
	size_t consumed_bits;

	BT_LOGV("Continuing BFCR: notit-addr=%p, bfcr-addr=%p",
		notit, notit->bfcr);

	status = buf_ensure_available_bits(notit);
	if (status != BT_MSG_ITER_STATUS_OK) {
		if (status < 0) {
			BT_LOGW("Cannot ensure that buffer has at least one byte: "
				"msg-addr=%p, status=%s",
				notit, bt_msg_iter_status_string(status));
		} else {
			BT_LOGV("Cannot ensure that buffer has at least one byte: "
				"msg-addr=%p, status=%s",
				notit, bt_msg_iter_status_string(status));
		}

		goto end;
	}

	consumed_bits = bt_bfcr_continue(notit->bfcr, notit->buf.addr,
		notit->buf.sz, &bfcr_status);
	BT_LOGV("BFCR consumed bits: size=%zu", consumed_bits);

	switch (bfcr_status) {
	case BT_BFCR_STATUS_OK:
		/* Type was read completely. */
		BT_LOGV_STR("Field was completely decoded.");
		notit->state = done_state;
		break;
	case BT_BFCR_STATUS_EOF:
		/* Stay in this continue state. */
		BT_LOGV_STR("BFCR needs more data to decode field completely.");
		break;
	default:
		BT_LOGW("BFCR failed to continue: notit-addr=%p, bfcr-addr=%p, "
			"status=%s", notit, notit->bfcr,
			bt_bfcr_status_string(bfcr_status));
		status = BT_MSG_ITER_STATUS_ERROR;
		goto end;
	}

	/* Consume bits now since we know we're not in an error state. */
	buf_consume_bits(notit, consumed_bits);
end:
	return status;
}

static
void release_event_dscopes(struct bt_msg_iter *notit)
{
	notit->dscopes.event_common_context = NULL;
	notit->dscopes.event_spec_context = NULL;
	notit->dscopes.event_payload = NULL;
}

static
void release_all_dscopes(struct bt_msg_iter *notit)
{
	notit->dscopes.stream_packet_context = NULL;

	if (notit->packet_context_field) {
		bt_packet_context_field_release(notit->packet_context_field);
		notit->packet_context_field = NULL;
	}

	release_event_dscopes(notit);
}

static
enum bt_msg_iter_status read_packet_header_begin_state(
		struct bt_msg_iter *notit)
{
	struct ctf_field_class *packet_header_fc = NULL;
	enum bt_msg_iter_status ret = BT_MSG_ITER_STATUS_OK;

	if (bt_msg_iter_switch_packet(notit)) {
		BT_LOGW("Cannot switch packet: notit-addr=%p", notit);
		ret = BT_MSG_ITER_STATUS_ERROR;
		goto end;
	}

	/*
	 * Make sure at least one bit is available for this packet. An
	 * empty packet is impossible. If we reach the end of the medium
	 * at this point, then it's considered the end of the stream.
	 */
	ret = buf_ensure_available_bits(notit);
	switch (ret) {
	case BT_MSG_ITER_STATUS_OK:
		break;
	case BT_MSG_ITER_STATUS_EOF:
		ret = BT_MSG_ITER_STATUS_OK;
		notit->state = STATE_CHECK_EMIT_MSG_STREAM_ACTIVITY_END;
		goto end;
	default:
		goto end;
	}

	/* Packet header class is common to the whole trace class. */
	packet_header_fc = notit->meta.tc->packet_header_fc;
	if (!packet_header_fc) {
		notit->state = STATE_AFTER_TRACE_PACKET_HEADER;
		goto end;
	}

	notit->cur_stream_class_id = -1;
	notit->cur_event_class_id = -1;
	notit->cur_data_stream_id = -1;
	BT_LOGV("Decoding packet header field:"
		"notit-addr=%p, trace-class-addr=%p, fc-addr=%p",
		notit, notit->meta.tc, packet_header_fc);
	ret = read_dscope_begin_state(notit, packet_header_fc,
		STATE_AFTER_TRACE_PACKET_HEADER,
		STATE_DSCOPE_TRACE_PACKET_HEADER_CONTINUE, NULL);
	if (ret < 0) {
		BT_LOGW("Cannot decode packet header field: "
			"notit-addr=%p, trace-class-addr=%p, "
			"fc-addr=%p",
			notit, notit->meta.tc, packet_header_fc);
	}

end:
	return ret;
}

static
enum bt_msg_iter_status read_packet_header_continue_state(
		struct bt_msg_iter *notit)
{
	return read_dscope_continue_state(notit,
		STATE_AFTER_TRACE_PACKET_HEADER);
}

static inline
enum bt_msg_iter_status set_current_stream_class(struct bt_msg_iter *notit)
{
	enum bt_msg_iter_status status = BT_MSG_ITER_STATUS_OK;
	struct ctf_stream_class *new_stream_class = NULL;

	if (notit->cur_stream_class_id == -1) {
		/*
		 * No current stream class ID field, therefore only one
		 * stream class.
		 */
		if (notit->meta.tc->stream_classes->len != 1) {
			BT_LOGW("Need exactly one stream class since there's "
				"no stream class ID field: "
				"notit-addr=%p", notit);
			status = BT_MSG_ITER_STATUS_ERROR;
			goto end;
		}

		new_stream_class = notit->meta.tc->stream_classes->pdata[0];
		notit->cur_stream_class_id = new_stream_class->id;
	}

	new_stream_class = ctf_trace_class_borrow_stream_class_by_id(
		notit->meta.tc, notit->cur_stream_class_id);
	if (!new_stream_class) {
		BT_LOGW("No stream class with ID of stream class ID to use in trace class: "
			"notit-addr=%p, stream-class-id=%" PRIu64 ", "
			"trace-class-addr=%p",
			notit, notit->cur_stream_class_id, notit->meta.tc);
		status = BT_MSG_ITER_STATUS_ERROR;
		goto end;
	}

	if (notit->meta.sc) {
		if (new_stream_class != notit->meta.sc) {
			BT_LOGW("Two packets refer to two different stream classes within the same packet sequence: "
				"notit-addr=%p, prev-stream-class-addr=%p, "
				"prev-stream-class-id=%" PRId64 ", "
				"next-stream-class-addr=%p, "
				"next-stream-class-id=%" PRId64 ", "
				"trace-addr=%p",
				notit, notit->meta.sc,
				notit->meta.sc->id,
				new_stream_class,
				new_stream_class->id,
				notit->meta.tc);
			status = BT_MSG_ITER_STATUS_ERROR;
			goto end;
		}
	} else {
		notit->meta.sc = new_stream_class;
	}

	BT_LOGV("Set current stream class: "
		"notit-addr=%p, stream-class-addr=%p, "
		"stream-class-id=%" PRId64,
		notit, notit->meta.sc, notit->meta.sc->id);

end:
	return status;
}

static inline
enum bt_msg_iter_status set_current_stream(struct bt_msg_iter *notit)
{
	enum bt_msg_iter_status status = BT_MSG_ITER_STATUS_OK;
	bt_stream *stream = NULL;

	BT_LOGV("Calling user function (get stream): notit-addr=%p, "
		"stream-class-addr=%p, stream-class-id=%" PRId64,
		notit, notit->meta.sc,
		notit->meta.sc->id);
	stream = notit->medium.medops.borrow_stream(
		notit->meta.sc->ir_sc, notit->cur_data_stream_id,
		notit->medium.data);
	bt_stream_get_ref(stream);
	BT_LOGV("User function returned: stream-addr=%p", stream);
	if (!stream) {
		BT_LOGW_STR("User function failed to return a stream object "
			"for the given stream class.");
		status = BT_MSG_ITER_STATUS_ERROR;
		goto end;
	}

	if (notit->stream && stream != notit->stream) {
		BT_LOGW("User function returned a different stream than the "
			"previous one for the same sequence of packets.");
		status = BT_MSG_ITER_STATUS_ERROR;
		goto end;
	}

	BT_STREAM_MOVE_REF(notit->stream, stream);

end:
	bt_stream_put_ref(stream);
	return status;
}

static inline
enum bt_msg_iter_status set_current_packet(struct bt_msg_iter *notit)
{
	enum bt_msg_iter_status status = BT_MSG_ITER_STATUS_OK;
	bt_packet *packet = NULL;

	BT_LOGV("Creating packet for packet message: "
		"notit-addr=%p", notit);
	BT_LOGV("Creating packet from stream: "
		"notit-addr=%p, stream-addr=%p, "
		"stream-class-addr=%p, "
		"stream-class-id=%" PRId64,
		notit, notit->stream, notit->meta.sc,
		notit->meta.sc->id);

	/* Create packet */
	BT_ASSERT(notit->stream);
	packet = bt_packet_create(notit->stream);
	if (!packet) {
		BT_LOGE("Cannot create packet from stream: "
			"notit-addr=%p, stream-addr=%p, "
			"stream-class-addr=%p, "
			"stream-class-id=%" PRId64,
			notit, notit->stream, notit->meta.sc,
			notit->meta.sc->id);
		goto error;
	}

	goto end;

error:
	BT_PACKET_PUT_REF_AND_RESET(packet);
	status = BT_MSG_ITER_STATUS_ERROR;

end:
	BT_PACKET_MOVE_REF(notit->packet, packet);
	return status;
}

static
enum bt_msg_iter_status after_packet_header_state(
		struct bt_msg_iter *notit)
{
	enum bt_msg_iter_status status;

	status = set_current_stream_class(notit);
	if (status != BT_MSG_ITER_STATUS_OK) {
		goto end;
	}

	notit->state = STATE_DSCOPE_STREAM_PACKET_CONTEXT_BEGIN;

end:
	return status;
}

static
enum bt_msg_iter_status read_packet_context_begin_state(
		struct bt_msg_iter *notit)
{
	enum bt_msg_iter_status status = BT_MSG_ITER_STATUS_OK;
	struct ctf_field_class *packet_context_fc;

	BT_ASSERT(notit->meta.sc);
	packet_context_fc = notit->meta.sc->packet_context_fc;
	if (!packet_context_fc) {
		BT_LOGV("No packet packet context field class in stream class: continuing: "
			"notit-addr=%p, stream-class-addr=%p, "
			"stream-class-id=%" PRId64,
			notit, notit->meta.sc,
			notit->meta.sc->id);
		notit->state = STATE_AFTER_STREAM_PACKET_CONTEXT;
		goto end;
	}

	BT_ASSERT(!notit->packet_context_field);

	if (packet_context_fc->in_ir) {
		/*
		 * Create free packet context field from stream class.
		 * This field is going to be moved to the packet once we
		 * create it. We cannot create the packet now because a
		 * packet is created from a stream, and this API must be
		 * able to return the packet context properties without
		 * creating a stream
		 * (bt_msg_iter_get_packet_properties()).
		 */
		notit->packet_context_field =
			bt_packet_context_field_create(
				notit->meta.sc->ir_sc);
		if (!notit->packet_context_field) {
			BT_LOGE_STR("Cannot create packet context field wrapper from stream class.");
			status = BT_MSG_ITER_STATUS_ERROR;
			goto end;
		}

		notit->dscopes.stream_packet_context =
			bt_packet_context_field_borrow_field(
				notit->packet_context_field);
		BT_ASSERT(notit->dscopes.stream_packet_context);
	}

	BT_LOGV("Decoding packet context field: "
		"notit-addr=%p, stream-class-addr=%p, "
		"stream-class-id=%" PRId64 ", fc-addr=%p",
		notit, notit->meta.sc,
		notit->meta.sc->id, packet_context_fc);
	status = read_dscope_begin_state(notit, packet_context_fc,
		STATE_AFTER_STREAM_PACKET_CONTEXT,
		STATE_DSCOPE_STREAM_PACKET_CONTEXT_CONTINUE,
		notit->dscopes.stream_packet_context);
	if (status < 0) {
		BT_LOGW("Cannot decode packet context field: "
			"notit-addr=%p, stream-class-addr=%p, "
			"stream-class-id=%" PRId64 ", fc-addr=%p",
			notit, notit->meta.sc,
			notit->meta.sc->id,
			packet_context_fc);
	}

end:
	return status;
}

static
enum bt_msg_iter_status read_packet_context_continue_state(
		struct bt_msg_iter *notit)
{
	return read_dscope_continue_state(notit,
			STATE_AFTER_STREAM_PACKET_CONTEXT);
}

static
enum bt_msg_iter_status set_current_packet_content_sizes(
		struct bt_msg_iter *notit)
{
	enum bt_msg_iter_status status = BT_MSG_ITER_STATUS_OK;

	if (notit->cur_exp_packet_total_size == -1) {
		if (notit->cur_exp_packet_content_size != -1) {
			BT_LOGW("Content size is set, but packet size is not: "
				"notit-addr=%p, packet-context-field-addr=%p, "
				"packet-size=%" PRId64 ", content-size=%" PRId64,
				notit, notit->dscopes.stream_packet_context,
				notit->cur_exp_packet_total_size,
				notit->cur_exp_packet_content_size);
			status = BT_MSG_ITER_STATUS_ERROR;
			goto end;
		}
	} else {
		if (notit->cur_exp_packet_content_size == -1) {
			notit->cur_exp_packet_content_size =
				notit->cur_exp_packet_total_size;
		}
	}

	if (notit->cur_exp_packet_content_size >
			notit->cur_exp_packet_total_size) {
		BT_LOGW("Invalid packet or content size: "
			"content size is greater than packet size: "
			"notit-addr=%p, packet-context-field-addr=%p, "
			"packet-size=%" PRId64 ", content-size=%" PRId64,
			notit, notit->dscopes.stream_packet_context,
			notit->cur_exp_packet_total_size,
			notit->cur_exp_packet_content_size);
		status = BT_MSG_ITER_STATUS_ERROR;
		goto end;
	}

	BT_LOGV("Set current packet and content sizes: "
		"notit-addr=%p, packet-size=%" PRIu64 ", content-size=%" PRIu64,
		notit, notit->cur_exp_packet_total_size,
		notit->cur_exp_packet_content_size);

end:
	return status;
}

static
enum bt_msg_iter_status after_packet_context_state(struct bt_msg_iter *notit)
{
	enum bt_msg_iter_status status;

	status = set_current_packet_content_sizes(notit);
	if (status != BT_MSG_ITER_STATUS_OK) {
		goto end;
	}

	if (notit->stream) {
		/*
		 * Stream exists, which means we already emitted at
		 * least one packet beginning message, so the initial
		 * stream beginning message was also emitted.
		 */
		notit->state = STATE_CHECK_EMIT_MSG_DISCARDED_EVENTS;
	} else {
		notit->state = STATE_CHECK_EMIT_MSG_STREAM_BEGINNING;
	}

end:
	return status;
}

static
enum bt_msg_iter_status read_event_header_begin_state(struct bt_msg_iter *notit)
{
	enum bt_msg_iter_status status = BT_MSG_ITER_STATUS_OK;
	struct ctf_field_class *event_header_fc = NULL;

	/* Reset the position of the last event header */
	notit->buf.last_eh_at = notit->buf.at;
	notit->cur_event_class_id = -1;

	/* Check if we have some content left */
	if (notit->cur_exp_packet_content_size >= 0) {
		if (unlikely(packet_at(notit) ==
				notit->cur_exp_packet_content_size)) {
			/* No more events! */
			BT_LOGV("Reached end of packet: notit-addr=%p, "
				"cur=%zu", notit, packet_at(notit));
			notit->state = STATE_EMIT_MSG_PACKET_END_MULTI;
			goto end;
		} else if (unlikely(packet_at(notit) >
				notit->cur_exp_packet_content_size)) {
			/* That's not supposed to happen */
			BT_LOGV("Before decoding event header field: cursor is passed the packet's content: "
				"notit-addr=%p, content-size=%" PRId64 ", "
				"cur=%zu", notit,
				notit->cur_exp_packet_content_size,
				packet_at(notit));
			status = BT_MSG_ITER_STATUS_ERROR;
			goto end;
		}
	} else {
		/*
		 * "Infinite" content: we're done when the medium has
		 * nothing else for us.
		 */
		status = buf_ensure_available_bits(notit);
		switch (status) {
		case BT_MSG_ITER_STATUS_OK:
			break;
		case BT_MSG_ITER_STATUS_EOF:
			status = BT_MSG_ITER_STATUS_OK;
			notit->state = STATE_EMIT_MSG_PACKET_END_SINGLE;
			goto end;
		default:
			goto end;
		}
	}

	release_event_dscopes(notit);
	BT_ASSERT(notit->meta.sc);
	event_header_fc = notit->meta.sc->event_header_fc;
	if (!event_header_fc) {
		notit->state = STATE_AFTER_EVENT_HEADER;
		goto end;
	}

	BT_LOGV("Decoding event header field: "
		"notit-addr=%p, stream-class-addr=%p, "
		"stream-class-id=%" PRId64 ", "
		"fc-addr=%p",
		notit, notit->meta.sc,
		notit->meta.sc->id,
		event_header_fc);
	status = read_dscope_begin_state(notit, event_header_fc,
		STATE_AFTER_EVENT_HEADER,
		STATE_DSCOPE_EVENT_HEADER_CONTINUE, NULL);
	if (status < 0) {
		BT_LOGW("Cannot decode event header field: "
			"notit-addr=%p, stream-class-addr=%p, "
			"stream-class-id=%" PRId64 ", fc-addr=%p",
			notit, notit->meta.sc,
			notit->meta.sc->id,
			event_header_fc);
	}

end:
	return status;
}

static
enum bt_msg_iter_status read_event_header_continue_state(
		struct bt_msg_iter *notit)
{
	return read_dscope_continue_state(notit,
		STATE_AFTER_EVENT_HEADER);
}

static inline
enum bt_msg_iter_status set_current_event_class(struct bt_msg_iter *notit)
{
	enum bt_msg_iter_status status = BT_MSG_ITER_STATUS_OK;

	struct ctf_event_class *new_event_class = NULL;

	if (notit->cur_event_class_id == -1) {
		/*
		 * No current event class ID field, therefore only one
		 * event class.
		 */
		if (notit->meta.sc->event_classes->len != 1) {
			BT_LOGW("Need exactly one event class since there's "
				"no event class ID field: "
				"notit-addr=%p", notit);
			status = BT_MSG_ITER_STATUS_ERROR;
			goto end;
		}

		new_event_class = notit->meta.sc->event_classes->pdata[0];
		notit->cur_event_class_id = new_event_class->id;
	}

	new_event_class = ctf_stream_class_borrow_event_class_by_id(
		notit->meta.sc, notit->cur_event_class_id);
	if (!new_event_class) {
		BT_LOGW("No event class with ID of event class ID to use in stream class: "
			"notit-addr=%p, stream-class-id=%" PRIu64 ", "
			"event-class-id=%" PRIu64 ", "
			"trace-class-addr=%p",
			notit, notit->meta.sc->id, notit->cur_event_class_id,
			notit->meta.tc);
		status = BT_MSG_ITER_STATUS_ERROR;
		goto end;
	}

	notit->meta.ec = new_event_class;
	BT_LOGV("Set current event class: "
		"notit-addr=%p, event-class-addr=%p, "
		"event-class-id=%" PRId64 ", "
		"event-class-name=\"%s\"",
		notit, notit->meta.ec, notit->meta.ec->id,
		notit->meta.ec->name->str);

end:
	return status;
}

static inline
enum bt_msg_iter_status set_current_event_message(
		struct bt_msg_iter *notit)
{
	enum bt_msg_iter_status status = BT_MSG_ITER_STATUS_OK;
	bt_message *msg = NULL;

	BT_ASSERT(notit->meta.ec);
	BT_ASSERT(notit->packet);
	BT_LOGV("Creating event message from event class and packet: "
		"notit-addr=%p, ec-addr=%p, ec-name=\"%s\", packet-addr=%p",
		notit, notit->meta.ec,
		notit->meta.ec->name->str,
		notit->packet);
	BT_ASSERT(notit->msg_iter);
	BT_ASSERT(notit->meta.sc);

	if (bt_stream_class_borrow_default_clock_class(notit->meta.sc->ir_sc)) {
		msg = bt_message_event_create_with_default_clock_snapshot(
			notit->msg_iter, notit->meta.ec->ir_ec,
			notit->packet, notit->default_clock_snapshot);
	} else {
		msg = bt_message_event_create(notit->msg_iter,
			notit->meta.ec->ir_ec, notit->packet);
	}

	if (!msg) {
		BT_LOGE("Cannot create event message: "
			"notit-addr=%p, ec-addr=%p, ec-name=\"%s\", "
			"packet-addr=%p",
			notit, notit->meta.ec,
			notit->meta.ec->name->str,
			notit->packet);
		goto error;
	}

	goto end;

error:
	BT_MESSAGE_PUT_REF_AND_RESET(msg);
	status = BT_MSG_ITER_STATUS_ERROR;

end:
	BT_MESSAGE_MOVE_REF(notit->event_msg, msg);
	return status;
}

static
enum bt_msg_iter_status after_event_header_state(
		struct bt_msg_iter *notit)
{
	enum bt_msg_iter_status status;

	status = set_current_event_class(notit);
	if (status != BT_MSG_ITER_STATUS_OK) {
		goto end;
	}

	status = set_current_event_message(notit);
	if (status != BT_MSG_ITER_STATUS_OK) {
		goto end;
	}

	notit->event = bt_message_event_borrow_event(
		notit->event_msg);
	BT_ASSERT(notit->event);
	notit->state = STATE_DSCOPE_EVENT_COMMON_CONTEXT_BEGIN;

end:
	return status;
}

static
enum bt_msg_iter_status read_event_common_context_begin_state(
		struct bt_msg_iter *notit)
{
	enum bt_msg_iter_status status = BT_MSG_ITER_STATUS_OK;
	struct ctf_field_class *event_common_context_fc;

	event_common_context_fc = notit->meta.sc->event_common_context_fc;
	if (!event_common_context_fc) {
		notit->state = STATE_DSCOPE_EVENT_SPEC_CONTEXT_BEGIN;
		goto end;
	}

	if (event_common_context_fc->in_ir) {
		BT_ASSERT(!notit->dscopes.event_common_context);
		notit->dscopes.event_common_context =
			bt_event_borrow_common_context_field(
				notit->event);
		BT_ASSERT(notit->dscopes.event_common_context);
	}

	BT_LOGV("Decoding event common context field: "
		"notit-addr=%p, stream-class-addr=%p, "
		"stream-class-id=%" PRId64 ", "
		"fc-addr=%p",
		notit, notit->meta.sc,
		notit->meta.sc->id,
		event_common_context_fc);
	status = read_dscope_begin_state(notit, event_common_context_fc,
		STATE_DSCOPE_EVENT_SPEC_CONTEXT_BEGIN,
		STATE_DSCOPE_EVENT_COMMON_CONTEXT_CONTINUE,
		notit->dscopes.event_common_context);
	if (status < 0) {
		BT_LOGW("Cannot decode event common context field: "
			"notit-addr=%p, stream-class-addr=%p, "
			"stream-class-id=%" PRId64 ", fc-addr=%p",
			notit, notit->meta.sc,
			notit->meta.sc->id,
			event_common_context_fc);
	}

end:
	return status;
}

static
enum bt_msg_iter_status read_event_common_context_continue_state(
		struct bt_msg_iter *notit)
{
	return read_dscope_continue_state(notit,
		STATE_DSCOPE_EVENT_SPEC_CONTEXT_BEGIN);
}

static
enum bt_msg_iter_status read_event_spec_context_begin_state(
		struct bt_msg_iter *notit)
{
	enum bt_msg_iter_status status = BT_MSG_ITER_STATUS_OK;
	struct ctf_field_class *event_spec_context_fc;

	event_spec_context_fc = notit->meta.ec->spec_context_fc;
	if (!event_spec_context_fc) {
		notit->state = STATE_DSCOPE_EVENT_PAYLOAD_BEGIN;
		goto end;
	}

	if (event_spec_context_fc->in_ir) {
		BT_ASSERT(!notit->dscopes.event_spec_context);
		notit->dscopes.event_spec_context =
			bt_event_borrow_specific_context_field(
				notit->event);
		BT_ASSERT(notit->dscopes.event_spec_context);
	}

	BT_LOGV("Decoding event specific context field: "
		"notit-addr=%p, event-class-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64 ", "
		"fc-addr=%p",
		notit, notit->meta.ec,
		notit->meta.ec->name->str,
		notit->meta.ec->id,
		event_spec_context_fc);
	status = read_dscope_begin_state(notit, event_spec_context_fc,
		STATE_DSCOPE_EVENT_PAYLOAD_BEGIN,
		STATE_DSCOPE_EVENT_SPEC_CONTEXT_CONTINUE,
		notit->dscopes.event_spec_context);
	if (status < 0) {
		BT_LOGW("Cannot decode event specific context field: "
			"notit-addr=%p, event-class-addr=%p, "
			"event-class-name=\"%s\", "
			"event-class-id=%" PRId64 ", fc-addr=%p",
			notit, notit->meta.ec,
			notit->meta.ec->name->str,
			notit->meta.ec->id,
			event_spec_context_fc);
	}

end:
	return status;
}

static
enum bt_msg_iter_status read_event_spec_context_continue_state(
		struct bt_msg_iter *notit)
{
	return read_dscope_continue_state(notit,
		STATE_DSCOPE_EVENT_PAYLOAD_BEGIN);
}

static
enum bt_msg_iter_status read_event_payload_begin_state(
		struct bt_msg_iter *notit)
{
	enum bt_msg_iter_status status = BT_MSG_ITER_STATUS_OK;
	struct ctf_field_class *event_payload_fc;

	event_payload_fc = notit->meta.ec->payload_fc;
	if (!event_payload_fc) {
		notit->state = STATE_EMIT_MSG_EVENT;
		goto end;
	}

	if (event_payload_fc->in_ir) {
		BT_ASSERT(!notit->dscopes.event_payload);
		notit->dscopes.event_payload =
			bt_event_borrow_payload_field(
				notit->event);
		BT_ASSERT(notit->dscopes.event_payload);
	}

	BT_LOGV("Decoding event payload field: "
		"notit-addr=%p, event-class-addr=%p, "
		"event-class-name=\"%s\", event-class-id=%" PRId64 ", "
		"fc-addr=%p",
		notit, notit->meta.ec,
		notit->meta.ec->name->str,
		notit->meta.ec->id,
		event_payload_fc);
	status = read_dscope_begin_state(notit, event_payload_fc,
		STATE_EMIT_MSG_EVENT,
		STATE_DSCOPE_EVENT_PAYLOAD_CONTINUE,
		notit->dscopes.event_payload);
	if (status < 0) {
		BT_LOGW("Cannot decode event payload field: "
			"notit-addr=%p, event-class-addr=%p, "
			"event-class-name=\"%s\", "
			"event-class-id=%" PRId64 ", fc-addr=%p",
			notit, notit->meta.ec,
			notit->meta.ec->name->str,
			notit->meta.ec->id,
			event_payload_fc);
	}

end:
	return status;
}

static
enum bt_msg_iter_status read_event_payload_continue_state(
		struct bt_msg_iter *notit)
{
	return read_dscope_continue_state(notit, STATE_EMIT_MSG_EVENT);
}

static
enum bt_msg_iter_status skip_packet_padding_state(struct bt_msg_iter *notit)
{
	enum bt_msg_iter_status status = BT_MSG_ITER_STATUS_OK;
	size_t bits_to_skip;

	BT_ASSERT(notit->cur_exp_packet_total_size > 0);
	bits_to_skip = notit->cur_exp_packet_total_size - packet_at(notit);
	if (bits_to_skip == 0) {
		notit->state = STATE_DSCOPE_TRACE_PACKET_HEADER_BEGIN;
		goto end;
	} else {
		size_t bits_to_consume;

		BT_LOGV("Trying to skip %zu bits of padding: notit-addr=%p, size=%zu",
			bits_to_skip, notit, bits_to_skip);
		status = buf_ensure_available_bits(notit);
		if (status != BT_MSG_ITER_STATUS_OK) {
			goto end;
		}

		bits_to_consume = MIN(buf_available_bits(notit), bits_to_skip);
		BT_LOGV("Skipping %zu bits of padding: notit-addr=%p, size=%zu",
			bits_to_consume, notit, bits_to_consume);
		buf_consume_bits(notit, bits_to_consume);
		bits_to_skip = notit->cur_exp_packet_total_size -
			packet_at(notit);
		if (bits_to_skip == 0) {
			notit->state = STATE_DSCOPE_TRACE_PACKET_HEADER_BEGIN;
			goto end;
		}
	}

end:
	return status;
}

static
enum bt_msg_iter_status check_emit_msg_stream_beginning_state(
		struct bt_msg_iter *notit)
{
	enum bt_msg_iter_status status = BT_MSG_ITER_STATUS_OK;

	if (notit->set_stream) {
		status = set_current_stream(notit);
		if (status != BT_MSG_ITER_STATUS_OK) {
			goto end;
		}
	}

	if (notit->emit_stream_begin_msg) {
		notit->state = STATE_EMIT_MSG_STREAM_BEGINNING;
	} else {
		/* Stream's first packet */
		notit->state = STATE_CHECK_EMIT_MSG_DISCARDED_EVENTS;
	}

end:
	return status;
}

static
enum bt_msg_iter_status check_emit_msg_discarded_events(
		struct bt_msg_iter *notit)
{
	notit->state = STATE_EMIT_MSG_DISCARDED_EVENTS;

	if (notit->prev_packet_snapshots.discarded_events == UINT64_C(-1)) {
		if (notit->snapshots.discarded_events == 0 ||
				notit->snapshots.discarded_events == UINT64_C(-1)) {
			/*
			 * Stream's first packet with no discarded
			 * events or no information about discarded
			 * events: do not emit.
			 */
			notit->state = STATE_CHECK_EMIT_MSG_DISCARDED_PACKETS;
		}
	} else {
		/*
		 * If the previous packet has a value for this counter,
		 * then this counter is defined for the whole stream.
		 */
		BT_ASSERT(notit->snapshots.discarded_events != UINT64_C(-1));

		if (notit->snapshots.discarded_events -
				notit->prev_packet_snapshots.discarded_events == 0) {
			/*
			 * No discarded events since previous packet: do
			 * not emit.
			 */
			notit->state = STATE_CHECK_EMIT_MSG_DISCARDED_PACKETS;
		}
	}

	return BT_MSG_ITER_STATUS_OK;
}

static
enum bt_msg_iter_status check_emit_msg_discarded_packets(
		struct bt_msg_iter *notit)
{
	notit->state = STATE_EMIT_MSG_DISCARDED_PACKETS;

	if (notit->prev_packet_snapshots.packets == UINT64_C(-1)) {
		/*
		 * Stream's first packet or no information about
		 * discarded packets: do not emit. In other words, if
		 * this is the first packet and its sequence number is
		 * not 0, do not consider that packets were previously
		 * lost: we might be reading a partial stream (LTTng
		 * snapshot for example).
		 */
		notit->state = STATE_EMIT_MSG_PACKET_BEGINNING;
	} else {
		/*
		 * If the previous packet has a value for this counter,
		 * then this counter is defined for the whole stream.
		 */
		BT_ASSERT(notit->snapshots.packets != UINT64_C(-1));

		if (notit->snapshots.packets -
				notit->prev_packet_snapshots.packets <= 1) {
			/*
			 * No discarded packets since previous packet:
			 * do not emit.
			 */
			notit->state = STATE_EMIT_MSG_PACKET_BEGINNING;
		}
	}

	return BT_MSG_ITER_STATUS_OK;
}

static
enum bt_msg_iter_status check_emit_msg_stream_activity_end(
		struct bt_msg_iter *notit)
{
	if (notit->emit_stream_end_msg) {
		notit->state = STATE_EMIT_MSG_STREAM_ACTIVITY_END;
	} else {
		notit->state = STATE_DONE;
	}

	return BT_MSG_ITER_STATUS_OK;
}

static inline
enum bt_msg_iter_status handle_state(struct bt_msg_iter *notit)
{
	enum bt_msg_iter_status status = BT_MSG_ITER_STATUS_OK;
	const enum state state = notit->state;

	BT_LOGV("Handling state: notit-addr=%p, state=%s",
		notit, state_string(state));

	// TODO: optimalize!
	switch (state) {
	case STATE_INIT:
		notit->state = STATE_DSCOPE_TRACE_PACKET_HEADER_BEGIN;
		break;
	case STATE_DSCOPE_TRACE_PACKET_HEADER_BEGIN:
		status = read_packet_header_begin_state(notit);
		break;
	case STATE_DSCOPE_TRACE_PACKET_HEADER_CONTINUE:
		status = read_packet_header_continue_state(notit);
		break;
	case STATE_AFTER_TRACE_PACKET_HEADER:
		status = after_packet_header_state(notit);
		break;
	case STATE_DSCOPE_STREAM_PACKET_CONTEXT_BEGIN:
		status = read_packet_context_begin_state(notit);
		break;
	case STATE_DSCOPE_STREAM_PACKET_CONTEXT_CONTINUE:
		status = read_packet_context_continue_state(notit);
		break;
	case STATE_AFTER_STREAM_PACKET_CONTEXT:
		status = after_packet_context_state(notit);
		break;
	case STATE_CHECK_EMIT_MSG_STREAM_BEGINNING:
		status = check_emit_msg_stream_beginning_state(notit);
		break;
	case STATE_EMIT_MSG_STREAM_BEGINNING:
		notit->state = STATE_EMIT_MSG_STREAM_ACTIVITY_BEGINNING;
		break;
	case STATE_EMIT_MSG_STREAM_ACTIVITY_BEGINNING:
		notit->state = STATE_CHECK_EMIT_MSG_DISCARDED_EVENTS;
		break;
	case STATE_CHECK_EMIT_MSG_DISCARDED_EVENTS:
		status = check_emit_msg_discarded_events(notit);
		break;
	case STATE_EMIT_MSG_DISCARDED_EVENTS:
		notit->state = STATE_CHECK_EMIT_MSG_DISCARDED_PACKETS;
		break;
	case STATE_CHECK_EMIT_MSG_DISCARDED_PACKETS:
		status = check_emit_msg_discarded_packets(notit);
		break;
	case STATE_EMIT_MSG_DISCARDED_PACKETS:
		notit->state = STATE_EMIT_MSG_PACKET_BEGINNING;
		break;
	case STATE_EMIT_MSG_PACKET_BEGINNING:
		notit->state = STATE_DSCOPE_EVENT_HEADER_BEGIN;
		break;
	case STATE_DSCOPE_EVENT_HEADER_BEGIN:
		status = read_event_header_begin_state(notit);
		break;
	case STATE_DSCOPE_EVENT_HEADER_CONTINUE:
		status = read_event_header_continue_state(notit);
		break;
	case STATE_AFTER_EVENT_HEADER:
		status = after_event_header_state(notit);
		break;
	case STATE_DSCOPE_EVENT_COMMON_CONTEXT_BEGIN:
		status = read_event_common_context_begin_state(notit);
		break;
	case STATE_DSCOPE_EVENT_COMMON_CONTEXT_CONTINUE:
		status = read_event_common_context_continue_state(notit);
		break;
	case STATE_DSCOPE_EVENT_SPEC_CONTEXT_BEGIN:
		status = read_event_spec_context_begin_state(notit);
		break;
	case STATE_DSCOPE_EVENT_SPEC_CONTEXT_CONTINUE:
		status = read_event_spec_context_continue_state(notit);
		break;
	case STATE_DSCOPE_EVENT_PAYLOAD_BEGIN:
		status = read_event_payload_begin_state(notit);
		break;
	case STATE_DSCOPE_EVENT_PAYLOAD_CONTINUE:
		status = read_event_payload_continue_state(notit);
		break;
	case STATE_EMIT_MSG_EVENT:
		notit->state = STATE_DSCOPE_EVENT_HEADER_BEGIN;
		break;
	case STATE_SKIP_PACKET_PADDING:
		status = skip_packet_padding_state(notit);
		break;
	case STATE_EMIT_MSG_PACKET_END_MULTI:
		notit->state = STATE_SKIP_PACKET_PADDING;
		break;
	case STATE_EMIT_MSG_PACKET_END_SINGLE:
		notit->state = STATE_CHECK_EMIT_MSG_STREAM_ACTIVITY_END;
		break;
	case STATE_CHECK_EMIT_MSG_STREAM_ACTIVITY_END:
		status = check_emit_msg_stream_activity_end(notit);
		break;
	case STATE_EMIT_MSG_STREAM_ACTIVITY_END:
		notit->state = STATE_EMIT_MSG_STREAM_END;
		break;
	case STATE_EMIT_MSG_STREAM_END:
		notit->state = STATE_DONE;
		break;
	case STATE_DONE:
		break;
	default:
		BT_LOGD("Unknown CTF plugin message iterator state: "
			"notit-addr=%p, state=%d", notit, notit->state);
		abort();
	}

	BT_LOGV("Handled state: notit-addr=%p, status=%s, "
		"prev-state=%s, cur-state=%s",
		notit, bt_msg_iter_status_string(status),
		state_string(state), state_string(notit->state));
	return status;
}

BT_HIDDEN
void bt_msg_iter_reset_for_next_stream_file(struct bt_msg_iter *notit)
{
	BT_ASSERT(notit);
	BT_LOGD("Resetting message iterator: addr=%p", notit);
	stack_clear(notit->stack);
	notit->meta.sc = NULL;
	notit->meta.ec = NULL;
	BT_PACKET_PUT_REF_AND_RESET(notit->packet);
	BT_STREAM_PUT_REF_AND_RESET(notit->stream);
	BT_MESSAGE_PUT_REF_AND_RESET(notit->event_msg);
	release_all_dscopes(notit);
	notit->cur_dscope_field = NULL;

	if (notit->packet_context_field) {
		bt_packet_context_field_release(notit->packet_context_field);
		notit->packet_context_field = NULL;
	}

	notit->buf.addr = NULL;
	notit->buf.sz = 0;
	notit->buf.at = 0;
	notit->buf.last_eh_at = SIZE_MAX;
	notit->buf.packet_offset = 0;
	notit->state = STATE_INIT;
	notit->cur_exp_packet_content_size = -1;
	notit->cur_exp_packet_total_size = -1;
	notit->cur_packet_offset = -1;
	notit->cur_event_class_id = -1;
	notit->snapshots.beginning_clock = UINT64_C(-1);
	notit->snapshots.end_clock = UINT64_C(-1);
}

/**
 * Resets the internal state of a CTF message iterator.
 */
BT_HIDDEN
void bt_msg_iter_reset(struct bt_msg_iter *notit)
{
	bt_msg_iter_reset_for_next_stream_file(notit);
	notit->cur_stream_class_id = -1;
	notit->cur_data_stream_id = -1;
	notit->emit_stream_begin_msg = true;
	notit->emit_stream_end_msg = true;
	notit->snapshots.discarded_events = UINT64_C(-1);
	notit->snapshots.packets = UINT64_C(-1);
	notit->prev_packet_snapshots.discarded_events = UINT64_C(-1);
	notit->prev_packet_snapshots.packets = UINT64_C(-1);
	notit->prev_packet_snapshots.beginning_clock = UINT64_C(-1);
	notit->prev_packet_snapshots.end_clock = UINT64_C(-1);
}

static
int bt_msg_iter_switch_packet(struct bt_msg_iter *notit)
{
	int ret = 0;

	/*
	 * We don't put the stream class here because we need to make
	 * sure that all the packets processed by the same message
	 * iterator refer to the same stream class (the first one).
	 */
	BT_ASSERT(notit);

	if (notit->cur_exp_packet_total_size != -1) {
		notit->cur_packet_offset += notit->cur_exp_packet_total_size;
	}

	BT_LOGV("Switching packet: notit-addr=%p, cur=%zu, "
		"packet-offset=%" PRId64, notit, notit->buf.at,
		notit->cur_packet_offset);
	stack_clear(notit->stack);
	notit->meta.ec = NULL;
	BT_PACKET_PUT_REF_AND_RESET(notit->packet);
	BT_MESSAGE_PUT_REF_AND_RESET(notit->event_msg);
	release_all_dscopes(notit);
	notit->cur_dscope_field = NULL;

	/*
	 * Adjust current buffer so that addr points to the beginning of the new
	 * packet.
	 */
	if (notit->buf.addr) {
		size_t consumed_bytes = (size_t) (notit->buf.at / CHAR_BIT);

		/* Packets are assumed to start on a byte frontier. */
		if (notit->buf.at % CHAR_BIT) {
			BT_LOGW("Cannot switch packet: current position is not a multiple of 8: "
				"notit-addr=%p, cur=%zu", notit, notit->buf.at);
			ret = -1;
			goto end;
		}

		notit->buf.addr += consumed_bytes;
		notit->buf.sz -= consumed_bytes;
		notit->buf.at = 0;
		notit->buf.packet_offset = 0;
		BT_LOGV("Adjusted buffer: addr=%p, size=%zu",
			notit->buf.addr, notit->buf.sz);
	}

	notit->cur_exp_packet_content_size = -1;
	notit->cur_exp_packet_total_size = -1;
	notit->cur_stream_class_id = -1;
	notit->cur_event_class_id = -1;
	notit->cur_data_stream_id = -1;
	notit->prev_packet_snapshots = notit->snapshots;
	notit->snapshots.discarded_events = UINT64_C(-1);
	notit->snapshots.packets = UINT64_C(-1);
	notit->snapshots.beginning_clock = UINT64_C(-1);
	notit->snapshots.end_clock = UINT64_C(-1);

end:
	return ret;
}

static
bt_field *borrow_next_field(struct bt_msg_iter *notit)
{
	bt_field *next_field = NULL;
	bt_field *base_field;
	const bt_field_class *base_fc;
	size_t index;

	BT_ASSERT(!stack_empty(notit->stack));
	index = stack_top(notit->stack)->index;
	base_field = stack_top(notit->stack)->base;
	BT_ASSERT(base_field);
	base_fc = bt_field_borrow_class_const(base_field);
	BT_ASSERT(base_fc);

	switch (bt_field_class_get_type(base_fc)) {
	case BT_FIELD_CLASS_TYPE_STRUCTURE:
	{
		BT_ASSERT(index <
			bt_field_class_structure_get_member_count(
				bt_field_borrow_class_const(
						base_field)));
		next_field =
			bt_field_structure_borrow_member_field_by_index(
				base_field, index);
		break;
	}
	case BT_FIELD_CLASS_TYPE_STATIC_ARRAY:
	case BT_FIELD_CLASS_TYPE_DYNAMIC_ARRAY:
		BT_ASSERT(index < bt_field_array_get_length(base_field));
		next_field = bt_field_array_borrow_element_field_by_index(
			base_field, index);
		break;
	case BT_FIELD_CLASS_TYPE_VARIANT:
		BT_ASSERT(index == 0);
		next_field = bt_field_variant_borrow_selected_option_field(
			base_field);
		break;
	default:
		abort();
	}

	BT_ASSERT(next_field);
	return next_field;
}

static
void update_default_clock(struct bt_msg_iter *notit, uint64_t new_val,
		uint64_t new_val_size)
{
	uint64_t new_val_mask;
	uint64_t cur_value_masked;

	BT_ASSERT(new_val_size > 0);

	/*
	 * Special case for a 64-bit new value, which is the limit
	 * of a clock value as of this version: overwrite the
	 * current value directly.
	 */
	if (new_val_size == 64) {
		notit->default_clock_snapshot = new_val;
		goto end;
	}

	new_val_mask = (1ULL << new_val_size) - 1;
	cur_value_masked = notit->default_clock_snapshot & new_val_mask;

	if (new_val < cur_value_masked) {
		/*
		 * It looks like a wrap happened on the number of bits
		 * of the requested new value. Assume that the clock
		 * value wrapped only one time.
		 */
		notit->default_clock_snapshot += new_val_mask + 1;
	}

	/* Clear the low bits of the current clock value. */
	notit->default_clock_snapshot &= ~new_val_mask;

	/* Set the low bits of the current clock value. */
	notit->default_clock_snapshot |= new_val;

end:
	BT_LOGV("Updated default clock's value from integer field's value: "
		"value=%" PRIu64, notit->default_clock_snapshot);
}

static
enum bt_bfcr_status bfcr_unsigned_int_cb(uint64_t value,
		struct ctf_field_class *fc, void *data)
{
	struct bt_msg_iter *notit = data;
	enum bt_bfcr_status status = BT_BFCR_STATUS_OK;
	bt_field *field = NULL;
	struct ctf_field_class_int *int_fc = (void *) fc;

	BT_LOGV("Unsigned integer function called from BFCR: "
		"notit-addr=%p, bfcr-addr=%p, fc-addr=%p, "
		"fc-type=%d, fc-in-ir=%d, value=%" PRIu64,
		notit, notit->bfcr, fc, fc->type, fc->in_ir, value);

	if (likely(int_fc->meaning == CTF_FIELD_CLASS_MEANING_NONE)) {
		goto update_def_clock;
	}

	switch (int_fc->meaning) {
	case CTF_FIELD_CLASS_MEANING_EVENT_CLASS_ID:
		notit->cur_event_class_id = value;
		break;
	case CTF_FIELD_CLASS_MEANING_DATA_STREAM_ID:
		notit->cur_data_stream_id = value;
		break;
	case CTF_FIELD_CLASS_MEANING_PACKET_BEGINNING_TIME:
		notit->snapshots.beginning_clock = value;
		break;
	case CTF_FIELD_CLASS_MEANING_PACKET_END_TIME:
		notit->snapshots.end_clock = value;
		break;
	case CTF_FIELD_CLASS_MEANING_STREAM_CLASS_ID:
		notit->cur_stream_class_id = value;
		break;
	case CTF_FIELD_CLASS_MEANING_MAGIC:
		if (value != 0xc1fc1fc1) {
			BT_LOGW("Invalid CTF magic number: notit-addr=%p, "
				"magic=%" PRIx64, notit, value);
			status = BT_BFCR_STATUS_ERROR;
			goto end;
		}

		break;
	case CTF_FIELD_CLASS_MEANING_PACKET_COUNTER_SNAPSHOT:
		notit->snapshots.packets = value;
		break;
	case CTF_FIELD_CLASS_MEANING_DISC_EV_REC_COUNTER_SNAPSHOT:
		notit->snapshots.discarded_events = value;
		break;
	case CTF_FIELD_CLASS_MEANING_EXP_PACKET_TOTAL_SIZE:
		notit->cur_exp_packet_total_size = value;
		break;
	case CTF_FIELD_CLASS_MEANING_EXP_PACKET_CONTENT_SIZE:
		notit->cur_exp_packet_content_size = value;
		break;
	default:
		abort();
	}

update_def_clock:
	if (unlikely(int_fc->mapped_clock_class)) {
		update_default_clock(notit, value, int_fc->base.size);
	}

	if (unlikely(int_fc->storing_index >= 0)) {
		g_array_index(notit->stored_values, uint64_t,
			(uint64_t) int_fc->storing_index) = value;
	}

	if (unlikely(!fc->in_ir)) {
		goto end;
	}

	field = borrow_next_field(notit);
	BT_ASSERT(field);
	BT_ASSERT(bt_field_borrow_class_const(field) == fc->ir_fc);
	BT_ASSERT(bt_field_get_class_type(field) ==
		  BT_FIELD_CLASS_TYPE_UNSIGNED_INTEGER ||
		  bt_field_get_class_type(field) ==
		  BT_FIELD_CLASS_TYPE_UNSIGNED_ENUMERATION);
	bt_field_unsigned_integer_set_value(field, value);
	stack_top(notit->stack)->index++;

end:
	return status;
}

static
enum bt_bfcr_status bfcr_unsigned_int_char_cb(uint64_t value,
		struct ctf_field_class *fc, void *data)
{
	int ret;
	struct bt_msg_iter *notit = data;
	enum bt_bfcr_status status = BT_BFCR_STATUS_OK;
	bt_field *string_field = NULL;
	struct ctf_field_class_int *int_fc = (void *) fc;
	char str[2] = {'\0', '\0'};

	BT_LOGV("Unsigned integer character function called from BFCR: "
		"notit-addr=%p, bfcr-addr=%p, fc-addr=%p, "
		"fc-type=%d, fc-in-ir=%d, value=%" PRIu64,
		notit, notit->bfcr, fc, fc->type, fc->in_ir, value);
	BT_ASSERT(int_fc->meaning == CTF_FIELD_CLASS_MEANING_NONE);
	BT_ASSERT(!int_fc->mapped_clock_class);
	BT_ASSERT(int_fc->storing_index < 0);

	if (unlikely(!fc->in_ir)) {
		goto end;
	}

	if (notit->done_filling_string) {
		goto end;
	}

	if (value == 0) {
		notit->done_filling_string = true;
		goto end;
	}

	string_field = stack_top(notit->stack)->base;
	BT_ASSERT(bt_field_get_class_type(string_field) ==
		  BT_FIELD_CLASS_TYPE_STRING);

	/* Append character */
	str[0] = (char) value;
	ret = bt_field_string_append_with_length(string_field, str, 1);
	if (ret) {
		BT_LOGE("Cannot append character to string field's value: "
			"notit-addr=%p, field-addr=%p, ret=%d",
			notit, string_field, ret);
		status = BT_BFCR_STATUS_ERROR;
		goto end;
	}

end:
	return status;
}

static
enum bt_bfcr_status bfcr_signed_int_cb(int64_t value,
		struct ctf_field_class *fc, void *data)
{
	enum bt_bfcr_status status = BT_BFCR_STATUS_OK;
	bt_field *field = NULL;
	struct bt_msg_iter *notit = data;
	struct ctf_field_class_int *int_fc = (void *) fc;

	BT_LOGV("Signed integer function called from BFCR: "
		"notit-addr=%p, bfcr-addr=%p, fc-addr=%p, "
		"fc-type=%d, fc-in-ir=%d, value=%" PRId64,
		notit, notit->bfcr, fc, fc->type, fc->in_ir, value);
	BT_ASSERT(int_fc->meaning == CTF_FIELD_CLASS_MEANING_NONE);

	if (unlikely(int_fc->storing_index >= 0)) {
		g_array_index(notit->stored_values, uint64_t,
			(uint64_t) int_fc->storing_index) = (uint64_t) value;
	}

	if (unlikely(!fc->in_ir)) {
		goto end;
	}

	field = borrow_next_field(notit);
	BT_ASSERT(field);
	BT_ASSERT(bt_field_borrow_class_const(field) == fc->ir_fc);
	BT_ASSERT(bt_field_get_class_type(field) ==
		  BT_FIELD_CLASS_TYPE_SIGNED_INTEGER ||
		  bt_field_get_class_type(field) ==
		  BT_FIELD_CLASS_TYPE_SIGNED_ENUMERATION);
	bt_field_signed_integer_set_value(field, value);
	stack_top(notit->stack)->index++;

end:
	return status;
}

static
enum bt_bfcr_status bfcr_floating_point_cb(double value,
		struct ctf_field_class *fc, void *data)
{
	enum bt_bfcr_status status = BT_BFCR_STATUS_OK;
	bt_field *field = NULL;
	struct bt_msg_iter *notit = data;

	BT_LOGV("Floating point number function called from BFCR: "
		"notit-addr=%p, bfcr-addr=%p, fc-addr=%p, "
		"fc-type=%d, fc-in-ir=%d, value=%f",
		notit, notit->bfcr, fc, fc->type, fc->in_ir, value);

	if (unlikely(!fc->in_ir)) {
		goto end;
	}

	field = borrow_next_field(notit);
	BT_ASSERT(field);
	BT_ASSERT(bt_field_borrow_class_const(field) == fc->ir_fc);
	BT_ASSERT(bt_field_get_class_type(field) ==
		  BT_FIELD_CLASS_TYPE_REAL);
	bt_field_real_set_value(field, value);
	stack_top(notit->stack)->index++;

end:
	return status;
}

static
enum bt_bfcr_status bfcr_string_begin_cb(
		struct ctf_field_class *fc, void *data)
{
	bt_field *field = NULL;
	struct bt_msg_iter *notit = data;
	int ret;

	BT_LOGV("String (beginning) function called from BFCR: "
		"notit-addr=%p, bfcr-addr=%p, fc-addr=%p, "
		"fc-type=%d, fc-in-ir=%d",
		notit, notit->bfcr, fc, fc->type, fc->in_ir);

	if (unlikely(!fc->in_ir)) {
		goto end;
	}

	field = borrow_next_field(notit);
	BT_ASSERT(field);
	BT_ASSERT(bt_field_borrow_class_const(field) == fc->ir_fc);
	BT_ASSERT(bt_field_get_class_type(field) ==
		  BT_FIELD_CLASS_TYPE_STRING);
	ret = bt_field_string_clear(field);
	BT_ASSERT(ret == 0);

	/*
	 * Push on stack. Not a compound class per se, but we know that
	 * only bfcr_string_cb() may be called between this call and a
	 * subsequent call to bfcr_string_end_cb().
	 */
	stack_push(notit->stack, field);

end:
	return BT_BFCR_STATUS_OK;
}

static
enum bt_bfcr_status bfcr_string_cb(const char *value,
		size_t len, struct ctf_field_class *fc, void *data)
{
	enum bt_bfcr_status status = BT_BFCR_STATUS_OK;
	bt_field *field = NULL;
	struct bt_msg_iter *notit = data;
	int ret;

	BT_LOGV("String (substring) function called from BFCR: "
		"notit-addr=%p, bfcr-addr=%p, fc-addr=%p, "
		"fc-type=%d, fc-in-ir=%d, string-length=%zu",
		notit, notit->bfcr, fc, fc->type, fc->in_ir,
		len);

	if (unlikely(!fc->in_ir)) {
		goto end;
	}

	field = stack_top(notit->stack)->base;
	BT_ASSERT(field);

	/* Append current substring */
	ret = bt_field_string_append_with_length(field, value, len);
	if (ret) {
		BT_LOGE("Cannot append substring to string field's value: "
			"notit-addr=%p, field-addr=%p, string-length=%zu, "
			"ret=%d", notit, field, len, ret);
		status = BT_BFCR_STATUS_ERROR;
		goto end;
	}

end:
	return status;
}

static
enum bt_bfcr_status bfcr_string_end_cb(
		struct ctf_field_class *fc, void *data)
{
	struct bt_msg_iter *notit = data;

	BT_LOGV("String (end) function called from BFCR: "
		"notit-addr=%p, bfcr-addr=%p, fc-addr=%p, "
		"fc-type=%d, fc-in-ir=%d",
		notit, notit->bfcr, fc, fc->type, fc->in_ir);

	if (unlikely(!fc->in_ir)) {
		goto end;
	}

	/* Pop string field */
	stack_pop(notit->stack);

	/* Go to next field */
	stack_top(notit->stack)->index++;

end:
	return BT_BFCR_STATUS_OK;
}

enum bt_bfcr_status bfcr_compound_begin_cb(
		struct ctf_field_class *fc, void *data)
{
	struct bt_msg_iter *notit = data;
	bt_field *field;

	BT_LOGV("Compound (beginning) function called from BFCR: "
		"notit-addr=%p, bfcr-addr=%p, fc-addr=%p, "
		"fc-type=%d, fc-in-ir=%d",
		notit, notit->bfcr, fc, fc->type, fc->in_ir);

	if (!fc->in_ir) {
		goto end;
	}

	/* Borrow field */
	if (stack_empty(notit->stack)) {
		/* Root: already set by read_dscope_begin_state() */
		field = notit->cur_dscope_field;
	} else {
		field = borrow_next_field(notit);
		BT_ASSERT(field);
	}

	/* Push field */
	BT_ASSERT(field);
	BT_ASSERT(bt_field_borrow_class_const(field) == fc->ir_fc);
	stack_push(notit->stack, field);

	/*
	 * Change BFCR "unsigned int" callback if it's a text
	 * array/sequence.
	 */
	if (fc->type == CTF_FIELD_CLASS_TYPE_ARRAY ||
			fc->type == CTF_FIELD_CLASS_TYPE_SEQUENCE) {
		struct ctf_field_class_array_base *array_fc = (void *) fc;

		if (array_fc->is_text) {
			int ret;

			BT_ASSERT(bt_field_get_class_type(field) ==
				  BT_FIELD_CLASS_TYPE_STRING);
			notit->done_filling_string = false;
			ret = bt_field_string_clear(field);
			BT_ASSERT(ret == 0);
			bt_bfcr_set_unsigned_int_cb(notit->bfcr,
				bfcr_unsigned_int_char_cb);
		}
	}

end:
	return BT_BFCR_STATUS_OK;
}

enum bt_bfcr_status bfcr_compound_end_cb(
		struct ctf_field_class *fc, void *data)
{
	struct bt_msg_iter *notit = data;

	BT_LOGV("Compound (end) function called from BFCR: "
		"notit-addr=%p, bfcr-addr=%p, fc-addr=%p, "
		"fc-type=%d, fc-in-ir=%d",
		notit, notit->bfcr, fc, fc->type, fc->in_ir);

	if (!fc->in_ir) {
		goto end;
	}

	BT_ASSERT(!stack_empty(notit->stack));
	BT_ASSERT(bt_field_borrow_class_const(stack_top(notit->stack)->base) ==
		fc->ir_fc);

	/*
	 * Reset BFCR "unsigned int" callback if it's a text
	 * array/sequence.
	 */
	if (fc->type == CTF_FIELD_CLASS_TYPE_ARRAY ||
			fc->type == CTF_FIELD_CLASS_TYPE_SEQUENCE) {
		struct ctf_field_class_array_base *array_fc = (void *) fc;

		if (array_fc->is_text) {
			BT_ASSERT(bt_field_get_class_type(
				stack_top(notit->stack)->base) ==
				BT_FIELD_CLASS_TYPE_STRING);
			bt_bfcr_set_unsigned_int_cb(notit->bfcr,
				bfcr_unsigned_int_cb);
		}
	}

	/* Pop stack */
	stack_pop(notit->stack);

	/* If the stack is not empty, increment the base's index */
	if (!stack_empty(notit->stack)) {
		stack_top(notit->stack)->index++;
	}

end:
	return BT_BFCR_STATUS_OK;
}

static
int64_t bfcr_get_sequence_length_cb(struct ctf_field_class *fc, void *data)
{
	bt_field *seq_field;
	struct bt_msg_iter *notit = data;
	struct ctf_field_class_sequence *seq_fc = (void *) fc;
	int64_t length = -1;
	int ret;

	length = (uint64_t) g_array_index(notit->stored_values, uint64_t,
		seq_fc->stored_length_index);
	seq_field = stack_top(notit->stack)->base;
	BT_ASSERT(seq_field);
	ret = bt_field_dynamic_array_set_length(seq_field, (uint64_t) length);
	if (ret) {
		BT_LOGE("Cannot set dynamic array field's length field: "
			"notit-addr=%p, field-addr=%p, "
			"length=%" PRIu64, notit, seq_field, length);
	}

	return length;
}

static
struct ctf_field_class *bfcr_borrow_variant_selected_field_class_cb(
		struct ctf_field_class *fc, void *data)
{
	int ret;
	uint64_t i;
	int64_t option_index = -1;
	struct bt_msg_iter *notit = data;
	struct ctf_field_class_variant *var_fc = (void *) fc;
	struct ctf_named_field_class *selected_option = NULL;
	struct ctf_field_class *ret_fc = NULL;
	union {
		uint64_t u;
		int64_t i;
	} tag;

	/* Get variant's tag */
	tag.u = g_array_index(notit->stored_values, uint64_t,
		var_fc->stored_tag_index);

	/*
	 * Check each range to find the selected option's index.
	 */
	if (var_fc->tag_fc->base.is_signed) {
		for (i = 0; i < var_fc->ranges->len; i++) {
			struct ctf_field_class_variant_range *range =
				ctf_field_class_variant_borrow_range_by_index(
					var_fc, i);

			if (tag.i >= range->range.lower.i &&
					tag.i <= range->range.upper.i) {
				option_index = (int64_t) range->option_index;
				break;
			}
		}
	} else {
		for (i = 0; i < var_fc->ranges->len; i++) {
			struct ctf_field_class_variant_range *range =
				ctf_field_class_variant_borrow_range_by_index(
					var_fc, i);

			if (tag.u >= range->range.lower.u &&
					tag.u <= range->range.upper.u) {
				option_index = (int64_t) range->option_index;
				break;
			}
		}
	}

	if (option_index < 0) {
		BT_LOGW("Cannot find variant field class's option: "
			"notit-addr=%p, var-fc-addr=%p, u-tag=%" PRIu64 ", "
			"i-tag=%" PRId64, notit, var_fc, tag.u, tag.i);
		goto end;
	}

	selected_option = ctf_field_class_variant_borrow_option_by_index(
		var_fc, (uint64_t) option_index);

	if (selected_option->fc->in_ir) {
		bt_field *var_field = stack_top(notit->stack)->base;

		ret = bt_field_variant_select_option_field(
			var_field, option_index);
		if (ret) {
			BT_LOGW("Cannot select variant field's option field: "
				"notit-addr=%p, var-field-addr=%p, "
				"opt-index=%" PRId64, notit, var_field,
				option_index);
			goto end;
		}
	}

	ret_fc = selected_option->fc;

end:
	return ret_fc;
}

static
void create_msg_stream_beginning(struct bt_msg_iter *notit,
		bt_message **message)
{
	bt_message *ret = NULL;

	BT_ASSERT(notit->stream);
	BT_ASSERT(notit->msg_iter);
	ret = bt_message_stream_beginning_create(notit->msg_iter,
		notit->stream);
	if (!ret) {
		BT_LOGE("Cannot create stream beginning message: "
			"notit-addr=%p, stream-addr=%p",
			notit, notit->stream);
		return;
	}

	*message = ret;
}

static
void create_msg_stream_activity_beginning(struct bt_msg_iter *notit,
		bt_message **message)
{
	bt_message *ret = NULL;

	BT_ASSERT(notit->stream);
	BT_ASSERT(notit->msg_iter);
	ret = bt_message_stream_activity_beginning_create(notit->msg_iter,
		notit->stream);
	if (!ret) {
		BT_LOGE("Cannot create stream activity beginning message: "
			"notit-addr=%p, stream-addr=%p",
			notit, notit->stream);
		return;
	}

	*message = ret;
}

static
void create_msg_stream_activity_end(struct bt_msg_iter *notit,
		bt_message **message)
{
	bt_message *ret = NULL;

	if (!notit->stream) {
		BT_LOGE("Cannot create stream for stream message: "
			"notit-addr=%p", notit);
		return;
	}

	BT_ASSERT(notit->stream);
	BT_ASSERT(notit->msg_iter);
	ret = bt_message_stream_activity_end_create(notit->msg_iter,
		notit->stream);
	if (!ret) {
		BT_LOGE("Cannot create stream activity end message: "
			"notit-addr=%p, stream-addr=%p",
			notit, notit->stream);
		return;
	}

	*message = ret;
}

static
void create_msg_stream_end(struct bt_msg_iter *notit, bt_message **message)
{
	bt_message *ret;

	if (!notit->stream) {
		BT_LOGE("Cannot create stream for stream message: "
			"notit-addr=%p", notit);
		return;
	}

	BT_ASSERT(notit->msg_iter);
	ret = bt_message_stream_end_create(notit->msg_iter,
		notit->stream);
	if (!ret) {
		BT_LOGE("Cannot create stream end message: "
			"notit-addr=%p, stream-addr=%p",
			notit, notit->stream);
		return;
	}

	*message = ret;
}

static
void create_msg_packet_beginning(struct bt_msg_iter *notit,
		bt_message **message)
{
	int ret;
	enum bt_msg_iter_status status;
	bt_message *msg = NULL;
	const bt_stream_class *sc;

	status = set_current_packet(notit);
	if (status != BT_MSG_ITER_STATUS_OK) {
		goto end;
	}

	BT_ASSERT(notit->packet);
	sc = notit->meta.sc->ir_sc;
	BT_ASSERT(sc);

	if (notit->packet_context_field) {
		ret = bt_packet_move_context_field(
			notit->packet, notit->packet_context_field);
		if (ret) {
			goto end;
		}

		notit->packet_context_field = NULL;

		/*
		 * At this point notit->dscopes.stream_packet_context
		 * has the same value as the packet context field within
		 * notit->packet.
		 */
		BT_ASSERT(bt_packet_borrow_context_field(
			notit->packet) ==
			notit->dscopes.stream_packet_context);
	}

	BT_ASSERT(notit->msg_iter);

	if (notit->snapshots.beginning_clock == UINT64_C(-1)) {
		msg = bt_message_packet_beginning_create(notit->msg_iter,
			notit->packet);
	} else {
		msg = bt_message_packet_beginning_create_with_default_clock_snapshot(
			notit->msg_iter, notit->packet,
			notit->snapshots.beginning_clock);
	}

	if (!msg) {
		BT_LOGE("Cannot create packet beginning message: "
			"notit-addr=%p, packet-addr=%p",
			notit, notit->packet);
		goto end;
	}

	*message = msg;

end:
	return;
}

static
void create_msg_packet_end(struct bt_msg_iter *notit, bt_message **message)
{
	bt_message *msg;

	if (!notit->packet) {
		return;
	}

	/* Update default clock from packet's end time */
	if (notit->snapshots.end_clock != UINT64_C(-1)) {
		notit->default_clock_snapshot = notit->snapshots.end_clock;
	}

	BT_ASSERT(notit->msg_iter);

	if (notit->snapshots.end_clock == UINT64_C(-1)) {
		msg = bt_message_packet_end_create(notit->msg_iter,
			notit->packet);
	} else {
		msg = bt_message_packet_end_create_with_default_clock_snapshot(
			notit->msg_iter, notit->packet,
			notit->snapshots.end_clock);
	}

	if (!msg) {
		BT_LOGE("Cannot create packet end message: "
			"notit-addr=%p, packet-addr=%p",
			notit, notit->packet);
		return;

	}

	BT_PACKET_PUT_REF_AND_RESET(notit->packet);
	*message = msg;
}

static
void create_msg_discarded_events(struct bt_msg_iter *notit,
		bt_message **message)
{
	bt_message *msg;
	uint64_t beginning_raw_value = UINT64_C(-1);
	uint64_t end_raw_value = UINT64_C(-1);
	uint64_t count = UINT64_C(-1);

	BT_ASSERT(notit->msg_iter);
	BT_ASSERT(notit->stream);

	if (notit->prev_packet_snapshots.discarded_events == UINT64_C(-1)) {
		/*
		 * We discarded events, but before (and possibly
		 * including) the current packet: use this packet's time
		 * range, and do not have a specific count.
		 */
		beginning_raw_value = notit->snapshots.beginning_clock;
		end_raw_value = notit->snapshots.end_clock;
	} else {
		count = notit->snapshots.discarded_events -
			notit->prev_packet_snapshots.discarded_events;
		BT_ASSERT(count > 0);
		beginning_raw_value = notit->prev_packet_snapshots.end_clock;
		end_raw_value = notit->snapshots.end_clock;
	}

	if (beginning_raw_value != UINT64_C(-1) &&
			end_raw_value != UINT64_C(-1)) {
		msg = bt_message_discarded_events_create_with_default_clock_snapshots(
			notit->msg_iter, notit->stream, beginning_raw_value,
			end_raw_value);
	} else {
		msg = bt_message_discarded_events_create(notit->msg_iter,
			notit->stream);
	}

	if (!msg) {
		BT_LOGE("Cannot create discarded events message: "
			"notit-addr=%p, stream-addr=%p",
			notit, notit->stream);
		return;
	}

	if (count != UINT64_C(-1)) {
		bt_message_discarded_events_set_count(msg, count);
	}

	*message = msg;
}

static
void create_msg_discarded_packets(struct bt_msg_iter *notit,
		bt_message **message)
{
	bt_message *msg;

	BT_ASSERT(notit->msg_iter);
	BT_ASSERT(notit->stream);
	BT_ASSERT(notit->prev_packet_snapshots.packets !=
		UINT64_C(-1));

	if (notit->prev_packet_snapshots.end_clock != UINT64_C(-1) &&
			notit->snapshots.beginning_clock != UINT64_C(-1)) {
		msg = bt_message_discarded_packets_create_with_default_clock_snapshots(
			notit->msg_iter, notit->stream,
			notit->prev_packet_snapshots.end_clock,
			notit->snapshots.beginning_clock);
	} else {
		msg = bt_message_discarded_packets_create(notit->msg_iter,
			notit->stream);
	}

	if (!msg) {
		BT_LOGE("Cannot create discarded packets message: "
			"notit-addr=%p, stream-addr=%p",
			notit, notit->stream);
		return;
	}

	bt_message_discarded_packets_set_count(msg,
		notit->snapshots.packets -
			notit->prev_packet_snapshots.packets - 1);
	*message = msg;
}

BT_HIDDEN
struct bt_msg_iter *bt_msg_iter_create(struct ctf_trace_class *tc,
		size_t max_request_sz,
		struct bt_msg_iter_medium_ops medops, void *data)
{
	struct bt_msg_iter *notit = NULL;
	struct bt_bfcr_cbs cbs = {
		.classes = {
			.signed_int = bfcr_signed_int_cb,
			.unsigned_int = bfcr_unsigned_int_cb,
			.floating_point = bfcr_floating_point_cb,
			.string_begin = bfcr_string_begin_cb,
			.string = bfcr_string_cb,
			.string_end = bfcr_string_end_cb,
			.compound_begin = bfcr_compound_begin_cb,
			.compound_end = bfcr_compound_end_cb,
		},
		.query = {
			.get_sequence_length = bfcr_get_sequence_length_cb,
			.borrow_variant_selected_field_class = bfcr_borrow_variant_selected_field_class_cb,
		},
	};

	BT_ASSERT(tc);
	BT_ASSERT(medops.request_bytes);
	BT_ASSERT(medops.borrow_stream);
	BT_LOGD("Creating CTF plugin message iterator: "
		"trace-addr=%p, max-request-size=%zu, "
		"data=%p", tc, max_request_sz, data);
	notit = g_new0(struct bt_msg_iter, 1);
	if (!notit) {
		BT_LOGE_STR("Failed to allocate one CTF plugin message iterator.");
		goto end;
	}
	notit->meta.tc = tc;
	notit->medium.medops = medops;
	notit->medium.max_request_sz = max_request_sz;
	notit->medium.data = data;
	notit->stack = stack_new(notit);
	notit->stored_values = g_array_new(FALSE, TRUE, sizeof(uint64_t));
	g_array_set_size(notit->stored_values, tc->stored_value_count);

	if (!notit->stack) {
		BT_LOGE_STR("Failed to create field stack.");
		goto error;
	}

	notit->bfcr = bt_bfcr_create(cbs, notit);
	if (!notit->bfcr) {
		BT_LOGE_STR("Failed to create binary class reader (BFCR).");
		goto error;
	}

	bt_msg_iter_reset(notit);
	BT_LOGD("Created CTF plugin message iterator: "
		"trace-addr=%p, max-request-size=%zu, "
		"data=%p, notit-addr=%p",
		tc, max_request_sz, data, notit);
	notit->cur_packet_offset = 0;

end:
	return notit;

error:
	bt_msg_iter_destroy(notit);
	notit = NULL;
	goto end;
}

void bt_msg_iter_destroy(struct bt_msg_iter *notit)
{
	BT_PACKET_PUT_REF_AND_RESET(notit->packet);
	BT_STREAM_PUT_REF_AND_RESET(notit->stream);
	release_all_dscopes(notit);

	BT_LOGD("Destroying CTF plugin message iterator: addr=%p", notit);

	if (notit->stack) {
		BT_LOGD_STR("Destroying field stack.");
		stack_destroy(notit->stack);
	}

	if (notit->bfcr) {
		BT_LOGD("Destroying BFCR: bfcr-addr=%p", notit->bfcr);
		bt_bfcr_destroy(notit->bfcr);
	}

	if (notit->stored_values) {
		g_array_free(notit->stored_values, TRUE);
	}

	g_free(notit);
}

enum bt_msg_iter_status bt_msg_iter_get_next_message(
		struct bt_msg_iter *notit,
		bt_self_message_iterator *msg_iter, bt_message **message)
{
	enum bt_msg_iter_status status = BT_MSG_ITER_STATUS_OK;

	BT_ASSERT(notit);
	BT_ASSERT(message);
	notit->msg_iter = msg_iter;
	notit->set_stream = true;
	BT_LOGV("Getting next message: notit-addr=%p", notit);

	while (true) {
		status = handle_state(notit);
		if (unlikely(status == BT_MSG_ITER_STATUS_AGAIN)) {
			BT_LOGV_STR("Medium returned BT_MSG_ITER_STATUS_AGAIN.");
			goto end;
		} else if (unlikely(status != BT_MSG_ITER_STATUS_OK)) {
			BT_LOGW("Cannot handle state: notit-addr=%p, state=%s",
				notit, state_string(notit->state));
			goto end;
		}

		switch (notit->state) {
		case STATE_EMIT_MSG_EVENT:
			BT_ASSERT(notit->event_msg);
			*message = notit->event_msg;
			notit->event_msg = NULL;
			goto end;
		case STATE_EMIT_MSG_DISCARDED_EVENTS:
			/* create_msg_discared_events() logs errors */
			create_msg_discarded_events(notit, message);

			if (!*message) {
				status = BT_MSG_ITER_STATUS_ERROR;
			}

			goto end;
		case STATE_EMIT_MSG_DISCARDED_PACKETS:
			/* create_msg_discared_packets() logs errors */
			create_msg_discarded_packets(notit, message);

			if (!*message) {
				status = BT_MSG_ITER_STATUS_ERROR;
			}

			goto end;
		case STATE_EMIT_MSG_PACKET_BEGINNING:
			/* create_msg_packet_beginning() logs errors */
			create_msg_packet_beginning(notit, message);

			if (!*message) {
				status = BT_MSG_ITER_STATUS_ERROR;
			}

			goto end;
		case STATE_EMIT_MSG_PACKET_END_SINGLE:
		case STATE_EMIT_MSG_PACKET_END_MULTI:
			/* create_msg_packet_end() logs errors */
			create_msg_packet_end(notit, message);

			if (!*message) {
				status = BT_MSG_ITER_STATUS_ERROR;
			}

			goto end;
		case STATE_EMIT_MSG_STREAM_ACTIVITY_BEGINNING:
			/* create_msg_stream_activity_beginning() logs errors */
			create_msg_stream_activity_beginning(notit, message);

			if (!*message) {
				status = BT_MSG_ITER_STATUS_ERROR;
			}

			goto end;
		case STATE_EMIT_MSG_STREAM_ACTIVITY_END:
			/* create_msg_stream_activity_end() logs errors */
			create_msg_stream_activity_end(notit, message);

			if (!*message) {
				status = BT_MSG_ITER_STATUS_ERROR;
			}

			goto end;
		case STATE_EMIT_MSG_STREAM_BEGINNING:
			/* create_msg_stream_beginning() logs errors */
			create_msg_stream_beginning(notit, message);

			if (!*message) {
				status = BT_MSG_ITER_STATUS_ERROR;
			}

			goto end;
		case STATE_EMIT_MSG_STREAM_END:
			/* create_msg_stream_end() logs errors */
			create_msg_stream_end(notit, message);

			if (!*message) {
				status = BT_MSG_ITER_STATUS_ERROR;
			}

			goto end;
		case STATE_DONE:
			status = BT_MSG_ITER_STATUS_EOF;
			goto end;
		default:
			/* Non-emitting state: continue */
			break;
		}
	}

end:
	return status;
}

static
enum bt_msg_iter_status read_packet_header_context_fields(
		struct bt_msg_iter *notit)
{
	int ret;
	enum bt_msg_iter_status status = BT_MSG_ITER_STATUS_OK;

	BT_ASSERT(notit);
	notit->set_stream = false;

	if (notit->state == STATE_EMIT_MSG_PACKET_BEGINNING) {
		/* We're already there */
		goto end;
	}

	while (true) {
		status = handle_state(notit);
		if (unlikely(status == BT_MSG_ITER_STATUS_AGAIN)) {
			BT_LOGV_STR("Medium returned BT_MSG_ITER_STATUS_AGAIN.");
			goto end;
		} else if (unlikely(status != BT_MSG_ITER_STATUS_OK)) {
			BT_LOGW("Cannot handle state: notit-addr=%p, state=%s",
				notit, state_string(notit->state));
			goto end;
		}

		switch (notit->state) {
		case STATE_EMIT_MSG_PACKET_BEGINNING:
			/*
			 * Packet header and context fields are
			 * potentially decoded (or they don't exist).
			 */
			goto end;
		case STATE_INIT:
		case STATE_DSCOPE_TRACE_PACKET_HEADER_BEGIN:
		case STATE_DSCOPE_TRACE_PACKET_HEADER_CONTINUE:
		case STATE_AFTER_TRACE_PACKET_HEADER:
		case STATE_DSCOPE_STREAM_PACKET_CONTEXT_BEGIN:
		case STATE_DSCOPE_STREAM_PACKET_CONTEXT_CONTINUE:
		case STATE_AFTER_STREAM_PACKET_CONTEXT:
		case STATE_CHECK_EMIT_MSG_STREAM_BEGINNING:
		case STATE_EMIT_MSG_STREAM_BEGINNING:
		case STATE_EMIT_MSG_STREAM_ACTIVITY_BEGINNING:
		case STATE_CHECK_EMIT_MSG_DISCARDED_EVENTS:
		case STATE_EMIT_MSG_DISCARDED_EVENTS:
		case STATE_CHECK_EMIT_MSG_DISCARDED_PACKETS:
		case STATE_EMIT_MSG_DISCARDED_PACKETS:
			/* Non-emitting state: continue */
			break;
		default:
			/*
			 * We should never get past the
			 * STATE_EMIT_MSG_PACKET_BEGINNING state.
			 */
			BT_LOGF("Unexpected state: notit-addr=%p, state=%s",
				notit, state_string(notit->state));
			abort();
		}
	}

end:
	ret = set_current_packet_content_sizes(notit);
	if (ret) {
		status = BT_MSG_ITER_STATUS_ERROR;
		goto end;
	}

	return status;
}

BT_HIDDEN
void bt_msg_iter_set_medops_data(struct bt_msg_iter *notit,
		void *medops_data)
{
	BT_ASSERT(notit);
	notit->medium.data = medops_data;
}

BT_HIDDEN
enum bt_msg_iter_status bt_msg_iter_seek(struct bt_msg_iter *notit,
		off_t offset)
{
	enum bt_msg_iter_status ret = BT_MSG_ITER_STATUS_OK;
	enum bt_msg_iter_medium_status medium_status;

	BT_ASSERT(notit);
	if (offset < 0) {
		BT_LOGE("Cannot seek to negative offset: offset=%jd", offset);
		ret = BT_MSG_ITER_STATUS_INVAL;
		goto end;
	}

	if (!notit->medium.medops.seek) {
		ret = BT_MSG_ITER_STATUS_UNSUPPORTED;
		BT_LOGD("Aborting seek as the iterator's underlying media does not implement seek support.");
		goto end;
	}

	medium_status = notit->medium.medops.seek(
		BT_MSG_ITER_SEEK_WHENCE_SET, offset, notit->medium.data);
	if (medium_status != BT_MSG_ITER_MEDIUM_STATUS_OK) {
		if (medium_status == BT_MSG_ITER_MEDIUM_STATUS_EOF) {
			ret = BT_MSG_ITER_STATUS_EOF;
		} else {
			ret = BT_MSG_ITER_STATUS_ERROR;
			goto end;
		}
	}

	bt_msg_iter_reset(notit);
	notit->cur_packet_offset = offset;

end:
	return ret;
}

BT_HIDDEN
enum bt_msg_iter_status bt_msg_iter_get_packet_properties(
		struct bt_msg_iter *notit,
		struct bt_msg_iter_packet_properties *props)
{
	enum bt_msg_iter_status status;

	BT_ASSERT(notit);
	BT_ASSERT(props);
	status = read_packet_header_context_fields(notit);
	if (status != BT_MSG_ITER_STATUS_OK) {
		goto end;
	}

	props->exp_packet_total_size = notit->cur_exp_packet_total_size;
	props->exp_packet_content_size = notit->cur_exp_packet_content_size;
	BT_ASSERT(props->stream_class_id >= 0);
	props->stream_class_id = (uint64_t) notit->cur_stream_class_id;
	props->data_stream_id = notit->cur_data_stream_id;
	props->snapshots.discarded_events = notit->snapshots.discarded_events;
	props->snapshots.packets = notit->snapshots.packets;
	props->snapshots.beginning_clock = notit->snapshots.beginning_clock;
	props->snapshots.end_clock = notit->snapshots.end_clock;

end:
	return status;
}

BT_HIDDEN
void bt_msg_iter_set_emit_stream_beginning_message(struct bt_msg_iter *notit,
		bool val)
{
	notit->emit_stream_begin_msg = val;
}

BT_HIDDEN
void bt_msg_iter_set_emit_stream_end_message(struct bt_msg_iter *notit,
		bool val)
{
	notit->emit_stream_end_msg = val;
}
