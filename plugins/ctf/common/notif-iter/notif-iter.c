/*
 * Babeltrace - CTF notification iterator
 *
 * Copyright (c) 2015-2016 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2015-2016 Philippe Proulx <pproulx@efficios.com>
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

#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <babeltrace/ctf-ir/field-types.h>
#include <babeltrace/ctf-ir/field-path.h>
#include <babeltrace/ctf-ir/fields.h>
#include <babeltrace/ctf-ir/stream-class.h>
#include <babeltrace/ctf-ir/packet.h>
#include <babeltrace/ctf-ir/stream.h>
#include <babeltrace/ctf-ir/clock.h>
#include <babeltrace/ctf-ir/event-class.h>
#include <babeltrace/plugin/notification/packet.h>
#include <babeltrace/plugin/notification/event.h>
#include <babeltrace/ref.h>
#include <glib.h>

#define PRINT_ERR_STREAM	notit->err_stream
#define PRINT_PREFIX		"ctf-notif-iter"
#include "print.h"

#include "notif-iter.h"
#include "../btr/btr.h"

#define BYTES_TO_BITS(x)		((x) * 8)

struct bt_ctf_notif_iter;

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
	 * Field is owned by this.
	 */
	struct bt_ctf_field *base;

	/* index of next field to set */
	size_t index;
};

/* Visit stack */
struct stack {
	/* Entries (struct stack_entry *) (top is last element) */
	GPtrArray *entries;
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
	STATE_EMIT_NOTIF_NEW_PACKET,
	STATE_DSCOPE_STREAM_EVENT_HEADER_BEGIN,
	STATE_DSCOPE_STREAM_EVENT_HEADER_CONTINUE,
	STATE_AFTER_STREAM_EVENT_HEADER,
	STATE_DSCOPE_STREAM_EVENT_CONTEXT_BEGIN,
	STATE_DSCOPE_STREAM_EVENT_CONTEXT_CONTINUE,
	STATE_DSCOPE_EVENT_CONTEXT_BEGIN,
	STATE_DSCOPE_EVENT_CONTEXT_CONTINUE,
	STATE_DSCOPE_EVENT_PAYLOAD_BEGIN,
	STATE_DSCOPE_EVENT_PAYLOAD_CONTINUE,
	STATE_EMIT_NOTIF_EVENT,
	STATE_EMIT_NOTIF_END_OF_PACKET,
	STATE_SKIP_PACKET_PADDING,
};

/* CTF notification iterator */
struct bt_ctf_notif_iter {
	/* Visit stack */
	struct stack *stack;

	/* Error stream (may be NULL) */
	FILE *err_stream;

	/*
	 * Current dynamic scope field pointer.
	 *
	 * This is set when a dynamic scope field is first created by
	 * btr_compound_begin_cb(). It points to one of the fields in
	 * dscopes below.
	 */
	struct bt_ctf_field **cur_dscope_field;

	/* Trace and classes (owned by this) */
	struct {
		struct bt_ctf_trace *trace;
		struct bt_ctf_stream_class *stream_class;
		struct bt_ctf_event_class *event_class;
	} meta;

	/* Current packet (NULL if not created yet) */
	struct bt_ctf_packet *packet;

	/* Database of current dynamic scopes (owned by this) */
	struct {
		struct bt_ctf_field *trace_packet_header;
		struct bt_ctf_field *stream_packet_context;
		struct bt_ctf_field *stream_event_header;
		struct bt_ctf_field *stream_event_context;
		struct bt_ctf_field *event_context;
		struct bt_ctf_field *event_payload;
	} dscopes;

	/* Current state */
	enum state state;

	/* User buffer stuff */
	struct {
		/* Last address provided by medium */
		const uint8_t *addr;

		/* Buffer size provided by medium (bytes) */
		size_t sz;

		/* Offset within whole packet of addr (bits) */
		size_t packet_offset;

		/* Current position from addr (bits) */
		size_t at;
	} buf;

	/* Binary type reader */
	struct bt_ctf_btr *btr;

	/* Medium stuff */
	struct {
		struct bt_ctf_notif_iter_medium_ops medops;
		size_t max_request_sz;
		void *data;
	} medium;

	/* Current packet size (bits) (-1 if unknown) */
	size_t cur_packet_size;

	/* Current content size (bits) (-1 if unknown) */
	size_t cur_content_size;
};

static
void stack_entry_free_func(gpointer data)
{
	struct stack_entry *entry = data;

	bt_put(entry->base);
	g_free(entry);
}

static
struct stack *stack_new(struct bt_ctf_notif_iter *notit)
{
	struct stack *stack = NULL;

	stack = g_new0(struct stack, 1);
	if (!stack) {
		goto error;
	}

	stack->entries = g_ptr_array_new_with_free_func(stack_entry_free_func);
	if (!stack->entries) {
		goto error;
	}

	return stack;
error:
	g_free(stack);
	return NULL;
}

static
void stack_destroy(struct stack *stack)
{
	assert(stack);
	g_ptr_array_free(stack->entries, TRUE);
	g_free(stack);
}

static
int stack_push(struct stack *stack, struct bt_ctf_field *base)
{
	int ret = 0;
	struct stack_entry *entry;

	assert(stack);
	assert(base);
	entry = g_new0(struct stack_entry, 1);
	if (!entry) {
		ret = -1;
		goto end;
	}

	entry->base = bt_get(base);
	g_ptr_array_add(stack->entries, entry);

end:
	return ret;
}

static inline
unsigned int stack_size(struct stack *stack)
{
	assert(stack);

	return stack->entries->len;
}

static
void stack_pop(struct stack *stack)
{
	assert(stack);
	assert(stack_size(stack));
	g_ptr_array_remove_index(stack->entries, stack->entries->len - 1);
}

static inline
struct stack_entry *stack_top(struct stack *stack)
{
	assert(stack);
	assert(stack_size(stack));

	return g_ptr_array_index(stack->entries, stack->entries->len - 1);
}

static inline
bool stack_empty(struct stack *stack)
{
	return stack_size(stack) == 0;
}

static
void stack_clear(struct stack *stack)
{
	assert(stack);

	if (!stack_empty(stack)) {
		g_ptr_array_remove_range(stack->entries, 0, stack_size(stack));
	}

	assert(stack_empty(stack));
}

static inline
enum bt_ctf_notif_iter_status notif_iter_status_from_m_status(
		enum bt_ctf_notif_iter_medium_status m_status)
{
	return m_status;
}

static inline
size_t buf_size_bits(struct bt_ctf_notif_iter *notit)
{
	return BYTES_TO_BITS(notit->buf.sz);
}

static inline
size_t buf_available_bits(struct bt_ctf_notif_iter *notit)
{
	return buf_size_bits(notit) - notit->buf.at;
}

static inline
size_t packet_at(struct bt_ctf_notif_iter *notit)
{
	return notit->buf.packet_offset + notit->buf.at;
}

static inline
size_t remaining_content_bits(struct bt_ctf_notif_iter *notit)
{
	if (notit->cur_content_size == -1) {
		return -1;
	}

	return notit->cur_content_size - packet_at(notit);
}

static inline
size_t remaining_packet_bits(struct bt_ctf_notif_iter *notit)
{
	if (notit->cur_packet_size == -1) {
		return -1;
	}

	return notit->cur_packet_size - packet_at(notit);
}

static inline
void buf_consume_bits(struct bt_ctf_notif_iter *notit, size_t incr)
{
	notit->buf.at += incr;
}

static inline
bool buf_has_enough_bits(struct bt_ctf_notif_iter *notit, size_t sz)
{
	return buf_available_bits(notit) >= sz;
}

static
enum bt_ctf_notif_iter_status request_medium_bytes(struct bt_ctf_notif_iter *notit)
{
	uint8_t *buffer_addr;
	size_t buffer_sz;
	enum bt_ctf_notif_iter_medium_status m_status;

	m_status = notit->medium.medops.request_bytes(
		notit->medium.max_request_sz, &buffer_addr,
		&buffer_sz, notit->medium.data);
	if (m_status == BT_CTF_NOTIF_ITER_MEDIUM_STATUS_OK) {
		assert(buffer_sz != 0);

		/* New packet offset is old one + old size (in bits) */
		notit->buf.packet_offset += buf_size_bits(notit);

		/* Restart at the beginning of the new medium buffer */
		notit->buf.at = 0;

		/* New medium buffer size */
		notit->buf.sz = buffer_sz;

		/* New medium buffer address */
		notit->buf.addr = buffer_addr;
	}

	return notif_iter_status_from_m_status(m_status);
}

static inline
enum bt_ctf_notif_iter_status buf_ensure_available_bits(
		struct bt_ctf_notif_iter *notit)
{
	enum bt_ctf_notif_iter_status status = BT_CTF_NOTIF_ITER_STATUS_OK;

	if (buf_available_bits(notit) == 0) {
		/*
		 * This _cannot_ return BT_CTF_NOTIF_ITER_STATUS_OK
		 * _and_ no bits.
		 */
		status = request_medium_bytes(notit);
	}

	return status;
}

static
enum bt_ctf_notif_iter_status read_dscope_begin_state(
		struct bt_ctf_notif_iter *notit,
		struct bt_ctf_field_type *dscope_field_type,
		enum state done_state, enum state continue_state,
		struct bt_ctf_field **dscope_field)
{
	enum bt_ctf_notif_iter_status status = BT_CTF_NOTIF_ITER_STATUS_OK;
	enum bt_ctf_btr_status btr_status;
	size_t consumed_bits;

	status = buf_ensure_available_bits(notit);
	if (status != BT_CTF_NOTIF_ITER_STATUS_OK) {
		goto end;
	}

	bt_put(*dscope_field);
	notit->cur_dscope_field = dscope_field;
	consumed_bits = bt_ctf_btr_start(notit->btr, dscope_field_type,
		notit->buf.addr, notit->buf.at, packet_at(notit),
		notit->buf.sz, &btr_status);

	switch (btr_status) {
	case BT_CTF_BTR_STATUS_OK:
		/* type was read completely */
		notit->state = done_state;
		break;
	case BT_CTF_BTR_STATUS_EOF:
		notit->state = continue_state;
		break;
	default:
		PERR("Binary type reader failed to start\n");
		status = BT_CTF_NOTIF_ITER_STATUS_ERROR;
		goto end;
	}

	/* Consume bits now since we know we're not in an error state */
	buf_consume_bits(notit, consumed_bits);

end:
	return status;
}

static
enum bt_ctf_notif_iter_status read_dscope_continue_state(
		struct bt_ctf_notif_iter *notit, enum state done_state)
{
	enum bt_ctf_notif_iter_status status = BT_CTF_NOTIF_ITER_STATUS_OK;
	enum bt_ctf_btr_status btr_status;
	size_t consumed_bits;

	status = buf_ensure_available_bits(notit);
	if (status != BT_CTF_NOTIF_ITER_STATUS_OK) {
		goto end;
	}

	consumed_bits = bt_ctf_btr_continue(notit->btr, notit->buf.addr,
			notit->buf.sz, &btr_status);

	switch (btr_status) {
	case BT_CTF_BTR_STATUS_OK:
		/* Type was read completely. */
		notit->state = done_state;
		break;
	case BT_CTF_BTR_STATUS_EOF:
		/* Stay in this continue state. */
		break;
	default:
		PERR("Binary type reader failed to continue\n");
		status = BT_CTF_NOTIF_ITER_STATUS_ERROR;
		goto end;
	}

	/* Consume bits now since we know we're not in an error state. */
	buf_consume_bits(notit, consumed_bits);
end:
	return status;
}

static
void put_event_dscopes(struct bt_ctf_notif_iter *notit)
{
	BT_PUT(notit->dscopes.stream_event_header);
	BT_PUT(notit->dscopes.stream_event_context);
	BT_PUT(notit->dscopes.event_context);
	BT_PUT(notit->dscopes.event_payload);
}

static
void put_all_dscopes(struct bt_ctf_notif_iter *notit)
{
	BT_PUT(notit->dscopes.trace_packet_header);
	BT_PUT(notit->dscopes.stream_packet_context);
	put_event_dscopes(notit);
}

static
enum bt_ctf_notif_iter_status read_packet_header_begin_state(
		struct bt_ctf_notif_iter *notit)
{
	enum bt_ctf_notif_iter_status status = BT_CTF_NOTIF_ITER_STATUS_OK;
	struct bt_ctf_field_type *packet_header_type;

	/* Reset all dynamic scopes since we're reading a new packet */
	put_all_dscopes(notit);
	BT_PUT(notit->packet);
	BT_PUT(notit->meta.stream_class);
	BT_PUT(notit->meta.event_class);

	/* Packet header type is common to the whole trace */
	packet_header_type = bt_ctf_trace_get_packet_header_type(
			notit->meta.trace);
	if (!packet_header_type) {
		PERR("Failed to retrieve trace's packet header type\n");
		status = BT_CTF_NOTIF_ITER_STATUS_ERROR;
		goto end;
	}

	status = read_dscope_begin_state(notit, packet_header_type,
			STATE_AFTER_TRACE_PACKET_HEADER,
			STATE_DSCOPE_TRACE_PACKET_HEADER_CONTINUE,
			&notit->dscopes.trace_packet_header);
end:
	BT_PUT(packet_header_type);
	return status;
}

static
enum bt_ctf_notif_iter_status read_packet_header_continue_state(
		struct bt_ctf_notif_iter *notit)
{
	return read_dscope_continue_state(notit,
			STATE_AFTER_TRACE_PACKET_HEADER);
}

static inline
bool is_struct_type(struct bt_ctf_field_type *field_type)
{
	return bt_ctf_field_type_get_type_id(field_type) ==
			BT_CTF_TYPE_ID_STRUCT;
}

static inline
bool is_variant_type(struct bt_ctf_field_type *field_type)
{
	return bt_ctf_field_type_get_type_id(field_type) ==
			BT_CTF_TYPE_ID_VARIANT;
}

static inline
enum bt_ctf_notif_iter_status set_current_stream_class(struct bt_ctf_notif_iter *notit)
{
	enum bt_ctf_notif_iter_status status = BT_CTF_NOTIF_ITER_STATUS_OK;
	struct bt_ctf_field_type *packet_header_type;
	struct bt_ctf_field_type *stream_id_field_type = NULL;
	uint64_t stream_id;

	/* Is there any "stream_id" field in the packet header? */
	packet_header_type = bt_ctf_trace_get_packet_header_type(
			notit->meta.trace);
	if (!packet_header_type) {
		PERR("Failed to retrieve trace's packet header type\n");
		status = BT_CTF_NOTIF_ITER_STATUS_ERROR;
		goto end;
	}

	assert(is_struct_type(packet_header_type));

	// TODO: optimalize!
	stream_id_field_type =
			bt_ctf_field_type_structure_get_field_type_by_name(
				packet_header_type, "stream_id");
	if (stream_id_field_type) {
		/* Find appropriate stream class using current stream ID */
		struct bt_ctf_field *stream_id_field = NULL;
		int ret;

		assert(notit->dscopes.trace_packet_header);

		// TODO: optimalize!
		stream_id_field = bt_ctf_field_structure_get_field(
				notit->dscopes.trace_packet_header, "stream_id");
		assert(stream_id_field);
		ret = bt_ctf_field_unsigned_integer_get_value(
				stream_id_field, &stream_id);
		assert(!ret);
		BT_PUT(stream_id_field);
	} else {
		/* Only one stream: pick the first stream class */
		assert(bt_ctf_trace_get_stream_class_count(
				notit->meta.trace) == 1);
		stream_id = 0;
	}

	BT_PUT(notit->meta.stream_class);

	// TODO: get by ID
	notit->meta.stream_class = bt_ctf_trace_get_stream_class(
			notit->meta.trace, stream_id);
	if (!notit->meta.stream_class) {
		PERR("Cannot find stream class with ID %" PRIu64 "\n",
			stream_id);
		status = BT_CTF_NOTIF_ITER_STATUS_ERROR;
		goto end;
	}

end:
	BT_PUT(packet_header_type);
	BT_PUT(stream_id_field_type);

	return status;
}

static
enum bt_ctf_notif_iter_status after_packet_header_state(
		struct bt_ctf_notif_iter *notit)
{
	enum bt_ctf_notif_iter_status status;

	status = set_current_stream_class(notit);
	if (status == BT_CTF_NOTIF_ITER_STATUS_OK) {
		notit->state = STATE_DSCOPE_STREAM_PACKET_CONTEXT_BEGIN;
	}

	return status;
}

static
enum bt_ctf_notif_iter_status read_packet_context_begin_state(
		struct bt_ctf_notif_iter *notit)
{
	enum bt_ctf_notif_iter_status status = BT_CTF_NOTIF_ITER_STATUS_OK;
	struct bt_ctf_field_type *packet_context_type;

	assert(notit->meta.stream_class);
	packet_context_type = bt_ctf_stream_class_get_packet_context_type(
			notit->meta.stream_class);
	if (!packet_context_type) {
		PERR("Failed to retrieve stream class's packet context\n");
		status = BT_CTF_NOTIF_ITER_STATUS_ERROR;
		goto end;
	}

	status = read_dscope_begin_state(notit, packet_context_type,
			STATE_AFTER_STREAM_PACKET_CONTEXT,
			STATE_DSCOPE_STREAM_PACKET_CONTEXT_CONTINUE,
			&notit->dscopes.stream_packet_context);

end:
	BT_PUT(packet_context_type);
	return status;
}

static
enum bt_ctf_notif_iter_status read_packet_context_continue_state(
		struct bt_ctf_notif_iter *notit)
{
	return read_dscope_continue_state(notit,
			STATE_AFTER_STREAM_PACKET_CONTEXT);
}

static
enum bt_ctf_notif_iter_status set_current_packet_content_sizes(
		struct bt_ctf_notif_iter *notit)
{
	enum bt_ctf_notif_iter_status status = BT_CTF_NOTIF_ITER_STATUS_OK;
	struct bt_ctf_field *packet_size_field = NULL;
	struct bt_ctf_field *content_size_field = NULL;
	uint64_t content_size = -1, packet_size = -1;

	assert(notit->dscopes.stream_packet_context);

	packet_size_field = bt_ctf_field_structure_get_field(
			notit->dscopes.stream_packet_context, "packet_size");
	content_size_field = bt_ctf_field_structure_get_field(
			notit->dscopes.stream_packet_context, "content_size");
	if (packet_size_field) {
		int ret = bt_ctf_field_unsigned_integer_get_value(
				packet_size_field, &packet_size);

		assert(!ret);
		if (packet_size == 0) {
			PERR("Decoded packet size is 0\n");
			status = BT_CTF_NOTIF_ITER_STATUS_ERROR;
			goto end;
		} else if ((packet_size % 8) != 0) {
			PERR("Decoded packet size is not a multiple of 8\n");
			status = BT_CTF_NOTIF_ITER_STATUS_ERROR;
			goto end;
		}
	}

	if (content_size_field) {
		int ret = bt_ctf_field_unsigned_integer_get_value(
			content_size_field, &content_size);

		assert(!ret);
	} else {
		content_size = packet_size;
	}

	notit->cur_packet_size = packet_size;
	notit->cur_content_size = content_size;
end:
	BT_PUT(packet_size_field);
	BT_PUT(content_size_field);
	return status;
}

static
enum bt_ctf_notif_iter_status after_packet_context_state(
		struct bt_ctf_notif_iter *notit)
{
	enum bt_ctf_notif_iter_status status;

	status = set_current_packet_content_sizes(notit);
	if (status == BT_CTF_NOTIF_ITER_STATUS_OK) {
		notit->state = STATE_EMIT_NOTIF_NEW_PACKET;
	}

	return status;
}

static
enum bt_ctf_notif_iter_status read_event_header_begin_state(
		struct bt_ctf_notif_iter *notit)
{
	enum bt_ctf_notif_iter_status status = BT_CTF_NOTIF_ITER_STATUS_OK;
	struct bt_ctf_field_type *event_header_type = NULL;

	/* Check if we have some content left */
	if (notit->cur_content_size >= 0) {
		if (packet_at(notit) == notit->cur_content_size) {
			/* No more events! */
			notit->state = STATE_EMIT_NOTIF_END_OF_PACKET;
			goto end;
		} else if (packet_at(notit) > notit->cur_content_size) {
			/* That's not supposed to happen */
			PERR("Cursor passed packet's content size:\n");
			PERR("\tDecoded content size: %zu\n",
				notit->cur_content_size);
			PERR("\tCursor position:      %zu\n", packet_at(notit));
			status = BT_CTF_NOTIF_ITER_STATUS_ERROR;
			goto end;
		}
	}

	event_header_type = bt_ctf_stream_class_get_event_header_type(
		notit->meta.stream_class);
	if (!event_header_type) {
		PERR("Failed to retrieve stream class's event header type\n");
		status = BT_CTF_NOTIF_ITER_STATUS_ERROR;
		goto end;
	}

	put_event_dscopes(notit);
	status = read_dscope_begin_state(notit, event_header_type,
		STATE_AFTER_STREAM_EVENT_HEADER,
		STATE_DSCOPE_STREAM_EVENT_HEADER_CONTINUE,
		&notit->dscopes.stream_event_header);
end:
	BT_PUT(event_header_type);

	return status;
}

static
enum bt_ctf_notif_iter_status read_event_header_continue_state(
		struct bt_ctf_notif_iter *notit)
{
	return read_dscope_continue_state(notit,
		STATE_AFTER_STREAM_EVENT_HEADER);
}

static inline
enum bt_ctf_notif_iter_status set_current_event_class(struct bt_ctf_notif_iter *notit)
{
	/*
	 * The assert() calls in this function are okay because it is
	 * assumed here that all the metadata objects have been
	 * validated for CTF correctness before decoding actual streams.
	 */

	enum bt_ctf_notif_iter_status status = BT_CTF_NOTIF_ITER_STATUS_OK;
	struct bt_ctf_field_type *event_header_type;
	struct bt_ctf_field_type *id_field_type = NULL;
	struct bt_ctf_field_type *v_field_type = NULL;
	uint64_t event_id = -1ULL;
	int ret;

	event_header_type = bt_ctf_stream_class_get_event_header_type(
		notit->meta.stream_class);
	if (!event_header_type) {
		PERR("Failed to retrieve stream class's event header type\n");
		status = BT_CTF_NOTIF_ITER_STATUS_ERROR;
		goto end;
	}

	/* Is there any "id"/"v" field in the event header? */
	assert(is_struct_type(event_header_type));
	id_field_type = bt_ctf_field_type_structure_get_field_type_by_name(
		event_header_type, "id");
	v_field_type = bt_ctf_field_type_structure_get_field_type_by_name(
		event_header_type, "v");
	assert(notit->dscopes.stream_event_header);
	if (v_field_type) {
		/*
		 *  _   _____ _____
		 * | | |_   _|_   _| __   __ _
		 * | |   | |   | || '_ \ / _` |
		 * | |___| |   | || | | | (_| |  S P E C I A L
		 * |_____|_|   |_||_| |_|\__, |  C A S E ™
		 *                       |___/
		 */
		struct bt_ctf_field *v_field = NULL;
		struct bt_ctf_field *v_struct_field = NULL;
		struct bt_ctf_field *v_struct_id_field = NULL;

		// TODO: optimalize!
		v_field = bt_ctf_field_structure_get_field(
			notit->dscopes.stream_event_header, "v");
		assert(v_field);

		v_struct_field =
			bt_ctf_field_variant_get_current_field(v_field);
		if (!v_struct_field) {
			goto end_v_field_type;
		}

		// TODO: optimalize!
		v_struct_id_field =
			bt_ctf_field_structure_get_field(v_struct_field, "id");
		if (!v_struct_id_field) {
			goto end_v_field_type;
		}

		ret = bt_ctf_field_unsigned_integer_get_value(
			v_struct_id_field, &event_id);
		if (ret) {
			event_id = -1ULL;
		}

end_v_field_type:
		BT_PUT(v_field);
		BT_PUT(v_struct_field);
		BT_PUT(v_struct_id_field);
	}

	if (id_field_type && event_id == -1ULL) {
		/* Check "id" field */
		struct bt_ctf_field *id_field = NULL;
		int ret;

		// TODO: optimalize!
		id_field = bt_ctf_field_structure_get_field(
			notit->dscopes.stream_event_header, "id");
		assert(id_field);
		assert(bt_ctf_field_is_integer(id_field) ||
			bt_ctf_field_is_enumeration(id_field));

		if (bt_ctf_field_is_integer(id_field)) {
			ret = bt_ctf_field_unsigned_integer_get_value(
				id_field, &event_id);
		} else {
			struct bt_ctf_field *container;

			container = bt_ctf_field_enumeration_get_container(
				id_field);
			assert(container);
			ret = bt_ctf_field_unsigned_integer_get_value(
				container, &event_id);
			BT_PUT(container);
		}
		assert(!ret);
		BT_PUT(id_field);
	}

	if (event_id == -1ULL) {
		/* Event ID not found: single event? */
		assert(bt_ctf_stream_class_get_event_class_count(
			notit->meta.stream_class) == 1);
		event_id = 0;
	}

	BT_PUT(notit->meta.event_class);
	notit->meta.event_class = bt_ctf_stream_class_get_event_class_by_id(
		notit->meta.stream_class, event_id);
	if (!notit->meta.event_class) {
		PERR("Cannot find event class with ID %" PRIu64 "\n", event_id);
		status = BT_CTF_NOTIF_ITER_STATUS_ERROR;
		goto end;
	}

end:
	BT_PUT(event_header_type);
	BT_PUT(id_field_type);
	BT_PUT(v_field_type);

	return status;
}

static
enum bt_ctf_notif_iter_status after_event_header_state(
		struct bt_ctf_notif_iter *notit)
{
	enum bt_ctf_notif_iter_status status;

	status = set_current_packet_content_sizes(notit);
	if (status != BT_CTF_NOTIF_ITER_STATUS_OK) {
		PERR("Failed to set current packet and content sizes\n");
		goto end;
	}

	status = set_current_event_class(notit);
	if (status != BT_CTF_NOTIF_ITER_STATUS_OK) {
		PERR("Failed to set current event class\n");
		goto end;
	}

	notit->state = STATE_DSCOPE_STREAM_EVENT_CONTEXT_BEGIN;

end:
	return status;
}

static
enum bt_ctf_notif_iter_status read_stream_event_context_begin_state(
		struct bt_ctf_notif_iter *notit)
{
	enum bt_ctf_notif_iter_status status = BT_CTF_NOTIF_ITER_STATUS_OK;
	struct bt_ctf_field_type *stream_event_context_type;

	stream_event_context_type = bt_ctf_stream_class_get_event_context_type(
		notit->meta.stream_class);
	if (!stream_event_context_type) {
		PERR("Failed to retrieve stream class's event context type\n");
		status = BT_CTF_NOTIF_ITER_STATUS_ERROR;
		goto end;
	}

	status = read_dscope_begin_state(notit, stream_event_context_type,
		STATE_DSCOPE_EVENT_CONTEXT_BEGIN,
		STATE_DSCOPE_STREAM_EVENT_CONTEXT_CONTINUE,
		&notit->dscopes.stream_event_context);

end:
	BT_PUT(stream_event_context_type);

	return status;
}

static
enum bt_ctf_notif_iter_status read_stream_event_context_continue_state(
		struct bt_ctf_notif_iter *notit)
{
	return read_dscope_continue_state(notit,
		STATE_DSCOPE_EVENT_CONTEXT_BEGIN);
}

static
enum bt_ctf_notif_iter_status read_event_context_begin_state(
		struct bt_ctf_notif_iter *notit)
{
	enum bt_ctf_notif_iter_status status = BT_CTF_NOTIF_ITER_STATUS_OK;
	struct bt_ctf_field_type *event_context_type;

	event_context_type = bt_ctf_event_class_get_context_type(
		notit->meta.event_class);
	if (!event_context_type) {
		PERR("Failed to retrieve event class's context type\n");
		status = BT_CTF_NOTIF_ITER_STATUS_ERROR;
		goto end;
	}

	status = read_dscope_begin_state(notit, event_context_type,
		STATE_DSCOPE_EVENT_PAYLOAD_BEGIN,
		STATE_DSCOPE_EVENT_CONTEXT_CONTINUE,
		&notit->dscopes.event_context);

end:
	BT_PUT(event_context_type);

	return status;
}

static
enum bt_ctf_notif_iter_status read_event_context_continue_state(
		struct bt_ctf_notif_iter *notit)
{
	return read_dscope_continue_state(notit,
		STATE_DSCOPE_EVENT_PAYLOAD_BEGIN);
}

static
enum bt_ctf_notif_iter_status read_event_payload_begin_state(
		struct bt_ctf_notif_iter *notit)
{
	enum bt_ctf_notif_iter_status status = BT_CTF_NOTIF_ITER_STATUS_OK;
	struct bt_ctf_field_type *event_payload_type;

	event_payload_type = bt_ctf_event_class_get_payload_type(
		notit->meta.event_class);
	if (!event_payload_type) {
		PERR("Failed to retrieve event class's payload type\n");
		status = BT_CTF_NOTIF_ITER_STATUS_ERROR;
		goto end;
	}

	status = read_dscope_begin_state(notit, event_payload_type,
		STATE_EMIT_NOTIF_EVENT,
		STATE_DSCOPE_EVENT_PAYLOAD_CONTINUE,
		&notit->dscopes.event_payload);

end:
	BT_PUT(event_payload_type);

	return status;
}

static
enum bt_ctf_notif_iter_status read_event_payload_continue_state(
		struct bt_ctf_notif_iter *notit)
{
	return read_dscope_continue_state(notit, STATE_EMIT_NOTIF_EVENT);
}

static
enum bt_ctf_notif_iter_status skip_packet_padding_state(
		struct bt_ctf_notif_iter *notit)
{
	enum bt_ctf_notif_iter_status status = BT_CTF_NOTIF_ITER_STATUS_OK;
	size_t bits_to_skip;

	assert(notit->cur_packet_size > 0);
	bits_to_skip = notit->cur_packet_size - packet_at(notit);
	if (bits_to_skip == 0) {
		notit->state = STATE_DSCOPE_TRACE_PACKET_HEADER_BEGIN;
		goto end;
	} else {
		size_t bits_to_consume;
		status = buf_ensure_available_bits(notit);
		if (status != BT_CTF_NOTIF_ITER_STATUS_OK) {
			goto end;
		}

		bits_to_consume = MIN(buf_available_bits(notit), bits_to_skip);
		buf_consume_bits(notit, bits_to_consume);
		bits_to_skip = notit->cur_packet_size - packet_at(notit);
		if (bits_to_skip == 0) {
			notit->state = STATE_DSCOPE_TRACE_PACKET_HEADER_BEGIN;
			goto end;
		}
	}

end:
	return status;
}

static inline
enum bt_ctf_notif_iter_status handle_state(struct bt_ctf_notif_iter *notit)
{
	enum bt_ctf_notif_iter_status status = BT_CTF_NOTIF_ITER_STATUS_OK;

	PDBG("Handling state %d\n", notit->state);

	// TODO: optimalize!
	switch (notit->state) {
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
	case STATE_EMIT_NOTIF_NEW_PACKET:
		notit->state = STATE_DSCOPE_STREAM_EVENT_HEADER_BEGIN;
		break;
	case STATE_DSCOPE_STREAM_EVENT_HEADER_BEGIN:
		status = read_event_header_begin_state(notit);
		break;
	case STATE_DSCOPE_STREAM_EVENT_HEADER_CONTINUE:
		status = read_event_header_continue_state(notit);
		break;
	case STATE_AFTER_STREAM_EVENT_HEADER:
		status = after_event_header_state(notit);
		break;
	case STATE_DSCOPE_STREAM_EVENT_CONTEXT_BEGIN:
		status = read_stream_event_context_begin_state(notit);
		break;
	case STATE_DSCOPE_STREAM_EVENT_CONTEXT_CONTINUE:
		status = read_stream_event_context_continue_state(notit);
		break;
	case STATE_DSCOPE_EVENT_CONTEXT_BEGIN:
		status = read_event_context_begin_state(notit);
		break;
	case STATE_DSCOPE_EVENT_CONTEXT_CONTINUE:
		status = read_event_context_continue_state(notit);
		break;
	case STATE_DSCOPE_EVENT_PAYLOAD_BEGIN:
		status = read_event_payload_begin_state(notit);
		break;
	case STATE_DSCOPE_EVENT_PAYLOAD_CONTINUE:
		status = read_event_payload_continue_state(notit);
		break;
	case STATE_EMIT_NOTIF_EVENT:
		notit->state = STATE_DSCOPE_STREAM_EVENT_HEADER_BEGIN;
		break;
	case STATE_SKIP_PACKET_PADDING:
		status = skip_packet_padding_state(notit);
		break;
	case STATE_EMIT_NOTIF_END_OF_PACKET:
		notit->state = STATE_SKIP_PACKET_PADDING;
		break;
	}

	return status;
}

void bt_ctf_notif_iter_reset(struct bt_ctf_notif_iter *notit)
{
	assert(notit);
	stack_clear(notit->stack);
	BT_PUT(notit->meta.stream_class);
	BT_PUT(notit->meta.event_class);
	BT_PUT(notit->packet);
	put_all_dscopes(notit);
	notit->buf.addr = NULL;
	notit->buf.sz = 0;
	notit->buf.at = 0;
	notit->buf.packet_offset = 0;
	notit->state = STATE_INIT;
	notit->cur_content_size = -1;
	notit->cur_packet_size = -1;
}

static
struct bt_ctf_field *get_next_field(struct bt_ctf_notif_iter *notit)
{
	struct bt_ctf_field *next_field = NULL;
	struct bt_ctf_field *base_field;
	struct bt_ctf_field_type *base_type;
	size_t index;

	assert(!stack_empty(notit->stack));
	index = stack_top(notit->stack)->index;
	base_field = stack_top(notit->stack)->base;
	base_type = bt_ctf_field_get_type(base_field);
	if (!base_type) {
		PERR("Failed to get base field's type\n");
		goto end;
	}

	switch (bt_ctf_field_type_get_type_id(base_type)) {
	case BT_CTF_TYPE_ID_STRUCT:
		next_field = bt_ctf_field_structure_get_field_by_index(
			base_field, index);
		break;
	case BT_CTF_TYPE_ID_ARRAY:
		next_field = bt_ctf_field_array_get_field(base_field, index);
		break;
	case BT_CTF_TYPE_ID_SEQUENCE:
		next_field = bt_ctf_field_sequence_get_field(base_field, index);
		break;
	case BT_CTF_TYPE_ID_VARIANT:
		next_field = bt_ctf_field_variant_get_current_field(base_field);
		break;
	default:
		assert(false);
		break;
	}

end:
	BT_PUT(base_type);

	return next_field;
}

static
enum bt_ctf_btr_status btr_signed_int_cb(int64_t value,
		struct bt_ctf_field_type *type, void *data)
{
	enum bt_ctf_btr_status status = BT_CTF_BTR_STATUS_OK;
	struct bt_ctf_field *field = NULL;
	struct bt_ctf_field *int_field = NULL;
	struct bt_ctf_notif_iter *notit = data;
	int ret;

	/* create next field */
	field = get_next_field(notit);
	if (!field) {
		PERR("Failed to get next field (signed int)\n");
		status = BT_CTF_BTR_STATUS_ERROR;
		goto end;
	}

	switch(bt_ctf_field_type_get_type_id(type)) {
	case BT_CTF_TYPE_ID_INTEGER:
		/* Integer field is created field */
		BT_MOVE(int_field, field);
		break;
	case BT_CTF_TYPE_ID_ENUM:
		int_field = bt_ctf_field_enumeration_get_container(field);
		break;
	default:
		break;
	}

	if (!int_field) {
		PERR("Failed to get integer field\n");
		status = BT_CTF_BTR_STATUS_ERROR;
		goto end;
	}

	ret = bt_ctf_field_signed_integer_set_value(int_field, value);
	assert(!ret);
	stack_top(notit->stack)->index++;

end:
	BT_PUT(field);
	BT_PUT(int_field);

	return status;
}

static
enum bt_ctf_btr_status btr_unsigned_int_cb(uint64_t value,
		struct bt_ctf_field_type *type, void *data)
{
	enum bt_ctf_btr_status status = BT_CTF_BTR_STATUS_OK;
	struct bt_ctf_field *field = NULL;
	struct bt_ctf_field *int_field = NULL;
	struct bt_ctf_notif_iter *notit = data;
	int ret;

	/* Create next field */
	field = get_next_field(notit);
	if (!field) {
		PERR("Failed to get next field (unsigned int)\n");
		status = BT_CTF_BTR_STATUS_ERROR;
		goto end;
	}

	switch(bt_ctf_field_type_get_type_id(type)) {
	case BT_CTF_TYPE_ID_INTEGER:
		/* Integer field is created field */
		BT_MOVE(int_field, field);
		break;
	case BT_CTF_TYPE_ID_ENUM:
		int_field = bt_ctf_field_enumeration_get_container(field);
		break;
	default:
		break;
	}

	if (!int_field) {
		PERR("Failed to get integer field\n");
		status = BT_CTF_BTR_STATUS_ERROR;
		goto end;
	}

	ret = bt_ctf_field_unsigned_integer_set_value(int_field, value);
	assert(!ret);
	stack_top(notit->stack)->index++;

end:
	BT_PUT(field);
	BT_PUT(int_field);

	return status;
}

static
enum bt_ctf_btr_status btr_floating_point_cb(double value,
		struct bt_ctf_field_type *type, void *data)
{
	enum bt_ctf_btr_status status = BT_CTF_BTR_STATUS_OK;
	struct bt_ctf_field *field = NULL;
	struct bt_ctf_notif_iter *notit = data;
	int ret;

	/* Create next field */
	field = get_next_field(notit);
	if (!field) {
		PERR("Failed to get next field (floating point number)\n");
		status = BT_CTF_BTR_STATUS_ERROR;
		goto end;
	}

	ret = bt_ctf_field_floating_point_set_value(field, value);
	assert(!ret);
	stack_top(notit->stack)->index++;

end:
	BT_PUT(field);

	return status;
}

static
enum bt_ctf_btr_status btr_string_begin_cb(
		struct bt_ctf_field_type *type, void *data)
{
	enum bt_ctf_btr_status status = BT_CTF_BTR_STATUS_OK;
	struct bt_ctf_field *field = NULL;
	struct bt_ctf_notif_iter *notit = data;
	int ret;

	/* Create next field */
	field = get_next_field(notit);
	if (!field) {
		PERR("Failed to get next field (string)\n");
		status = BT_CTF_BTR_STATUS_ERROR;
		goto end;
	}

	/*
	 * Push on stack. Not a compound type per se, but we know that only
	 * btr_string_cb() may be called between this call and a subsequent
	 * call to btr_string_end_cb().
	 */
	ret = stack_push(notit->stack, field);
	if (ret) {
		PERR("Failed to push string field onto the stack\n");
		status = BT_CTF_BTR_STATUS_ERROR;
		goto end;
	}

end:
	BT_PUT(field);

	return status;
}

static
enum bt_ctf_btr_status btr_string_cb(const char *value,
		size_t len, struct bt_ctf_field_type *type, void *data)
{
	enum bt_ctf_btr_status status = BT_CTF_BTR_STATUS_OK;
	struct bt_ctf_field *field = NULL;
	struct bt_ctf_notif_iter *notit = data;
	int ret;

	/* Get string field */
	field = stack_top(notit->stack)->base;
	assert(field);

	/* Append current string */
	ret = bt_ctf_field_string_append_len(field, value, len);
	if (ret) {
		PERR("Failed to append a string to a string field\n");
		status = BT_CTF_BTR_STATUS_ERROR;
		goto end;
	}

end:
	return status;
}

static
enum bt_ctf_btr_status btr_string_end_cb(
		struct bt_ctf_field_type *type, void *data)
{
	struct bt_ctf_notif_iter *notit = data;

	/* Pop string field */
	stack_pop(notit->stack);

	/* Go to next field */
	stack_top(notit->stack)->index++;

	return BT_CTF_BTR_STATUS_OK;
}

enum bt_ctf_btr_status btr_compound_begin_cb(
		struct bt_ctf_field_type *type, void *data)
{
	enum bt_ctf_btr_status status = BT_CTF_BTR_STATUS_OK;
	struct bt_ctf_notif_iter *notit = data;
	struct bt_ctf_field *field;
	int ret;

	/* Create field */
	if (stack_empty(notit->stack)) {
		/* Root: create dynamic scope field */
		*notit->cur_dscope_field = bt_ctf_field_create(type);
		field = *notit->cur_dscope_field;

		/*
		 * Field will be put at the end of this function
		 * (stack_push() will take one reference, but this
		 * reference is lost upon the equivalent stack_pop()
		 * later), so also get it for our context to own it.
		 */
		bt_get(*notit->cur_dscope_field);
	} else {
		field = get_next_field(notit);
	}

	if (!field) {
		PERR("Failed to get next field or create dynamic scope field\n");
		status = BT_CTF_BTR_STATUS_ERROR;
		goto end;
	}

	/* Push field */
	ret = stack_push(notit->stack, field);
	if (ret) {
		PERR("Failed to push compound field onto the stack\n");
		status = BT_CTF_BTR_STATUS_ERROR;
		goto end;
	}

end:
	BT_PUT(field);

	return status;
}

enum bt_ctf_btr_status btr_compound_end_cb(
		struct bt_ctf_field_type *type, void *data)
{
	struct bt_ctf_notif_iter *notit = data;

	assert(!stack_empty(notit->stack));

	/* Pop stack */
	stack_pop(notit->stack);

	/* If the stack is not empty, increment the base's index */
	if (!stack_empty(notit->stack)) {
		stack_top(notit->stack)->index++;
	}

	return BT_CTF_BTR_STATUS_OK;
}

static
struct bt_ctf_field *resolve_field(struct bt_ctf_notif_iter *notit,
		struct bt_ctf_field_path *path)
{
	struct bt_ctf_field *field = NULL;
	unsigned int i;

	switch (bt_ctf_field_path_get_root_scope(path)) {
	case BT_CTF_SCOPE_TRACE_PACKET_HEADER:
		field = notit->dscopes.trace_packet_header;
		break;
	case BT_CTF_SCOPE_STREAM_PACKET_CONTEXT:
		field = notit->dscopes.stream_packet_context;
		break;
	case BT_CTF_SCOPE_STREAM_EVENT_HEADER:
		field = notit->dscopes.stream_event_header;
		break;
	case BT_CTF_SCOPE_STREAM_EVENT_CONTEXT:
		field = notit->dscopes.stream_event_context;
		break;
	case BT_CTF_SCOPE_EVENT_CONTEXT:
		field = notit->dscopes.event_context;
		break;
	case BT_CTF_SCOPE_EVENT_FIELDS:
		field = notit->dscopes.event_payload;
		break;
	default:
		break;
	}

	if (!field) {
		goto end;
	}

	bt_get(field);

	for (i = 0; i < bt_ctf_field_path_get_index_count(path); ++i) {
		struct bt_ctf_field *next_field = NULL;
		struct bt_ctf_field_type *field_type;
		int index = bt_ctf_field_path_get_index(path, i);

		field_type = bt_ctf_field_get_type(field);
		if (!field_type) {
			BT_PUT(field);
			goto end;
		}

		if (is_struct_type(field_type)) {
			next_field = bt_ctf_field_structure_get_field_by_index(
				field, index);
		} else if (is_variant_type(field_type)) {
			next_field =
				bt_ctf_field_variant_get_current_field(field);
		}

		BT_PUT(field);
		BT_PUT(field_type);

		if (!next_field) {
			goto end;
		}

		/* Move next field -> field */
		BT_MOVE(field, next_field);
	}

end:
	return field;
}

static
int64_t btr_get_sequence_length_cb(struct bt_ctf_field_type *type, void *data)
{
	int64_t ret = -1;
	int iret;
	struct bt_ctf_field_path *field_path;
	struct bt_ctf_notif_iter *notit = data;
	struct bt_ctf_field *field = NULL;
	uint64_t length;

	field_path = bt_ctf_field_type_sequence_get_length_field_path(type);
	if (!field_path) {
		goto end;
	}

	field = resolve_field(notit, field_path);
	if (!field) {
		goto end;
	}

	iret = bt_ctf_field_unsigned_integer_get_value(field, &length);
	if (iret) {
		goto end;
	}

	ret = (int64_t) length;

end:
	BT_PUT(field);
	BT_PUT(field_path);

	return ret;
}

static
struct bt_ctf_field_type *btr_get_variant_type_cb(
		struct bt_ctf_field_type *type, void *data)
{
	struct bt_ctf_field_path *path;
	struct bt_ctf_notif_iter *notit = data;
	struct bt_ctf_field *tag_field = NULL;
	struct bt_ctf_field *selected_field = NULL;
	struct bt_ctf_field_type *selected_field_type = NULL;

	path = bt_ctf_field_type_variant_get_tag_field_path(type);
	if (!path) {
		goto end;
	}

	tag_field = resolve_field(notit, path);
	if (!tag_field) {
		goto end;
	}

	/*
	 * We found the enumeration tag field instance which should be
	 * able to select a current field for this variant. This
	 * callback function we're in is called _after_
	 * compound_begin(), so the current stack top's base field is
	 * the variant field in question. We get the selected field here
	 * thanks to this tag field (thus creating the selected field),
	 * which will also provide us with its type. Then, this field
	 * will remain the current selected one until the next callback
	 * function call which is used to fill the current selected
	 * field.
	 */
	selected_field = bt_ctf_field_variant_get_field(
		stack_top(notit->stack)->base, tag_field);
	if (!selected_field) {
		goto end;
	}

	selected_field_type = bt_ctf_field_get_type(selected_field);

end:
	BT_PUT(tag_field);
	BT_PUT(selected_field);

	return selected_field_type;
}

static
struct bt_ctf_event *create_event(struct bt_ctf_notif_iter *notit)
{
	int ret;
	struct bt_ctf_event *event;

	/* Create event object. */
	event = bt_ctf_event_create(notit->meta.event_class);
	if (!event) {
		goto error;
	}

	/* Set header, stream event context, context, and payload fields. */
	ret = bt_ctf_event_set_header(event,
		notit->dscopes.stream_event_header);
	if (ret) {
		goto error;
	}

	ret = bt_ctf_event_set_stream_event_context(event,
		notit->dscopes.stream_event_context);
	if (ret) {
		goto error;
	}

	ret = bt_ctf_event_set_event_context(event,
		notit->dscopes.event_context);
	if (ret) {
		goto error;
	}

	ret = bt_ctf_event_set_payload_field(event,
		notit->dscopes.event_payload);
	if (ret) {
		goto error;
	}

	/* Associate with current packet. */
	assert(notit->packet);
	ret = bt_ctf_event_set_packet(event, notit->packet);
	if (ret) {
		goto error;
	}

	goto end;
error:
	BT_PUT(event);
end:
	return event;
}

static
void create_packet(struct bt_ctf_notif_iter *notit)
{
	int ret;
	struct bt_ctf_stream *stream = NULL;
	struct bt_ctf_packet *packet = NULL;

	/* Ask the user for the stream */
	stream = notit->medium.medops.get_stream(notit->meta.stream_class,
			notit->medium.data);
	if (!stream) {
		goto error;
	}

	/* Create packet */
	packet = bt_ctf_packet_create(stream);
	if (!packet) {
		goto error;
	}

	/* Set packet's context and header fields */
	if (notit->dscopes.trace_packet_header) {
		ret = bt_ctf_packet_set_header(packet,
				notit->dscopes.trace_packet_header);
		if (ret) {
			goto error;
		}
	}

	if (notit->dscopes.stream_packet_context) {
		ret = bt_ctf_packet_set_context(packet,
				notit->dscopes.stream_packet_context);
		if (ret) {
			goto error;
		}
	}

	goto end;
error:
	BT_PUT(packet);
end:
	BT_MOVE(notit->packet, packet);
}

static
void notify_new_packet(struct bt_ctf_notif_iter *notit,
		struct bt_notification **notification)
{
	struct bt_notification *ret;

	/* Initialize the iterator's current packet */
	create_packet(notit);
	if (!notit->packet) {
		return;
	}

	ret = bt_notification_packet_start_create(notit->packet);
	if (!ret) {
		return;
	}
	*notification = ret;
}

static
void notify_end_of_packet(struct bt_ctf_notif_iter *notit,
		struct bt_notification **notification)
{
	struct bt_notification *ret;

	if (!notit->packet) {
		return;
	}

	ret = bt_notification_packet_end_create(notit->packet);
	if (!ret) {
		return;
	}
	BT_PUT(notit->packet);
	*notification = ret;
}

static
void notify_event(struct bt_ctf_notif_iter *notit,
		struct bt_notification **notification)
{
	struct bt_ctf_event *event;
	struct bt_notification *ret = NULL;

	/* Create event */
	event = create_event(notit);
	if (!event) {
		goto end;
	}

	ret = bt_notification_event_create(event);
	if (!ret) {
		goto end;
	}
	*notification = ret;
end:
	BT_PUT(event);
}

static
void notify_eos(struct bt_ctf_notif_iter *notit,
		struct bt_notification **notification)
{
	struct bt_ctf_event *event;
	struct bt_notification *ret = NULL;

	/* Create event */
	event = create_event(notit);
	if (!event) {
		goto end;
	}

	ret = bt_notification_stream_end_create(event);
	if (!ret) {
		goto end;
	}
	*notification = ret;
end:
	BT_PUT(event);
}

struct bt_ctf_notif_iter *bt_ctf_notif_iter_create(struct bt_ctf_trace *trace,
		size_t max_request_sz,
		struct bt_ctf_notif_iter_medium_ops medops,
		void *data, FILE *err_stream)
{
	struct bt_ctf_notif_iter *notit = NULL;
	struct bt_ctf_btr_cbs cbs = {
		.types = {
			.signed_int = btr_signed_int_cb,
			.unsigned_int = btr_unsigned_int_cb,
			.floating_point = btr_floating_point_cb,
			.string_begin = btr_string_begin_cb,
			.string = btr_string_cb,
			.string_end = btr_string_end_cb,
			.compound_begin = btr_compound_begin_cb,
			.compound_end = btr_compound_end_cb,
		},
		.query = {
			.get_sequence_length = btr_get_sequence_length_cb,
			.get_variant_type = btr_get_variant_type_cb,
		},
	};

	assert(trace);
	assert(medops.request_bytes);
	notit = g_new0(struct bt_ctf_notif_iter, 1);
	if (!notit) {
		PERR("Failed to allocate memory for CTF notification iterator\n");
		goto end;
	}

	notit->meta.trace = trace;
	bt_get(notit->meta.trace);
	notit->medium.medops = medops;
	notit->medium.max_request_sz = max_request_sz;
	notit->medium.data = data;
	notit->err_stream = err_stream;
	notit->stack = stack_new(notit);
	if (!notit->stack) {
		PERR("Failed to create stack\n");
		bt_ctf_notif_iter_destroy(notit);
		notit = NULL;
		goto end;
	}

	notit->btr = bt_ctf_btr_create(cbs, notit, err_stream);
	if (!notit->btr) {
		PERR("Failed to create binary type reader\n");
		bt_ctf_notif_iter_destroy(notit);
		notit = NULL;
		goto end;
	}

	bt_ctf_notif_iter_reset(notit);

end:
	return notit;
}

void bt_ctf_notif_iter_destroy(struct bt_ctf_notif_iter *notit)
{
	BT_PUT(notit->meta.trace);
	BT_PUT(notit->meta.stream_class);
	BT_PUT(notit->meta.event_class);
	BT_PUT(notit->packet);
	put_all_dscopes(notit);

	if (notit->stack) {
		stack_destroy(notit->stack);
	}

	if (notit->btr) {
		bt_ctf_btr_destroy(notit->btr);
	}

	g_free(notit);
}

enum bt_ctf_notif_iter_status bt_ctf_notif_iter_get_next_notification(
		struct bt_ctf_notif_iter *notit,
		struct bt_notification **notification)
{
	enum bt_ctf_notif_iter_status status = BT_CTF_NOTIF_ITER_STATUS_OK;

	assert(notit);
	assert(notification);

	while (true) {
		status = handle_state(notit);
		if (status != BT_CTF_NOTIF_ITER_STATUS_OK) {
			if (status == BT_CTF_NOTIF_ITER_STATUS_EOF) {
				PDBG("Medium operation reported end of stream\n");
			} else {
				PERR("Failed to handle state:\n");
				PERR("\tState: %d\n", notit->state);
			}
			goto end;
		}

		switch (notit->state) {
		case STATE_EMIT_NOTIF_NEW_PACKET:
			PDBG("Emitting new packet notification\n");
			notify_new_packet(notit, notification);
			if (!*notification) {
				status = BT_CTF_NOTIF_ITER_STATUS_ERROR;
			}
			goto end;
		case STATE_EMIT_NOTIF_EVENT:
			PDBG("Emitting event notification\n");
			notify_event(notit, notification);
			if (!*notification) {
				status = BT_CTF_NOTIF_ITER_STATUS_ERROR;
			}
			goto end;
		case STATE_EMIT_NOTIF_END_OF_PACKET:
			PDBG("Emitting end of packet notification\n");
			notify_end_of_packet(notit, notification);
			if (!*notification) {
				status = BT_CTF_NOTIF_ITER_STATUS_ERROR;
			}
			goto end;
		default:
			/* Non-emitting state: continue */
			break;
		}
	}

end:
	return status;
}
