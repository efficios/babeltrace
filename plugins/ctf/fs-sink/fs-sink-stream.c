/*
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
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

#define BT_LOG_TAG "PLUGIN-CTF-FS-SINK-STREAM"
#include "logging.h"

#include <babeltrace/babeltrace.h>
#include <stdio.h>
#include <stdbool.h>
#include <glib.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/ctfser-internal.h>
#include <babeltrace/endian-internal.h>

#include "fs-sink-trace.h"
#include "fs-sink-stream.h"
#include "translate-trace-ir-to-ctf-ir.h"

BT_HIDDEN
void fs_sink_stream_destroy(struct fs_sink_stream *stream)
{
	if (!stream) {
		goto end;
	}

	bt_ctfser_fini(&stream->ctfser);

	if (stream->file_name) {
		g_string_free(stream->file_name, TRUE);
		stream->file_name = NULL;
	}

	bt_packet_put_ref(stream->packet_state.packet);
	g_free(stream);

end:
	return;
}

static
bool stream_file_name_exists(struct fs_sink_trace *trace, const char *name)
{
	bool exists = false;
	GHashTableIter iter;
	gpointer key, value;

	g_hash_table_iter_init(&iter, trace->streams);

	while (g_hash_table_iter_next(&iter, &key, &value)) {
		struct fs_sink_stream *stream = value;

		if (strcmp(name, stream->file_name->str) == 0) {
			exists = true;
			goto end;
		}
	}

end:
	return exists;
}

static
GString *sanitize_stream_file_name(const char *file_name)
{
	GString *san_file_name = g_string_new(NULL);
	const char *ch;
	gchar *basename;

	BT_ASSERT(san_file_name);
	BT_ASSERT(file_name);
	basename = g_path_get_basename(file_name);

	for (ch = basename; *ch != '\0'; ch++) {
		if (*ch == '/') {
			g_string_append_c(san_file_name, '_');
		} else {
			g_string_append_c(san_file_name, *ch);
		}
	}

	/* Do not allow `.` and `..` either */
	if (strcmp(san_file_name->str, ".") == 0 ||
			strcmp(san_file_name->str, "..") == 0) {
		g_string_assign(san_file_name, "stream");
	}

	g_free(basename);
	return san_file_name;
}

static
GString *make_unique_stream_file_name(struct fs_sink_trace *trace,
		const char *base)
{
	GString *san_base = sanitize_stream_file_name(base);
	GString *name = g_string_new(san_base->str);
	unsigned int suffix = 0;

	BT_ASSERT(name);

	while (stream_file_name_exists(trace, name->str) &&
			strcmp(name->str, "metadata") == 0) {
		g_string_printf(name, "%s-%u", san_base->str, suffix);
		suffix++;
	}

	g_string_free(san_base, TRUE);
	return name;
}

static
void set_stream_file_name(struct fs_sink_stream *stream)
{
	const char *base_name = bt_stream_get_name(stream->ir_stream);

	if (!base_name) {
		base_name = "stream";
	}

	BT_ASSERT(!stream->file_name);
	stream->file_name = make_unique_stream_file_name(stream->trace,
		base_name);
}

BT_HIDDEN
struct fs_sink_stream *fs_sink_stream_create(struct fs_sink_trace *trace,
		const bt_stream *ir_stream)
{
	struct fs_sink_stream *stream = g_new0(struct fs_sink_stream, 1);
	int ret;
	GString *path = g_string_new(trace->path->str);

	if (!stream) {
		goto end;
	}

	stream->trace = trace;
	stream->ir_stream = ir_stream;
	stream->packet_state.beginning_cs = UINT64_C(-1);
	stream->packet_state.end_cs = UINT64_C(-1);
	stream->prev_packet_state.end_cs = UINT64_C(-1);
	stream->prev_packet_state.discarded_events_counter = UINT64_C(-1);
	stream->prev_packet_state.seq_num = UINT64_C(-1);
	ret = try_translate_stream_class_trace_ir_to_ctf_ir(trace->tc,
		bt_stream_borrow_class_const(ir_stream), &stream->sc);
	if (ret) {
		goto error;
	}

	set_stream_file_name(stream);
	g_string_append_printf(path, "/%s", stream->file_name->str);
	ret = bt_ctfser_init(&stream->ctfser, path->str);
	if (ret) {
		goto error;
	}

	g_hash_table_insert(trace->streams, (gpointer) ir_stream, stream);
	goto end;

error:
	fs_sink_stream_destroy(stream);
	stream = NULL;

end:
	if (path) {
		g_string_free(path, TRUE);
	}

	return stream;
}

static
int write_field(struct fs_sink_stream *stream,
		struct fs_sink_ctf_field_class *fc, const bt_field *field);

static inline
int write_int_field(struct fs_sink_stream *stream,
		struct fs_sink_ctf_field_class_int *fc, const bt_field *field)
{
	int ret;

	if (fc->is_signed) {
		ret = bt_ctfser_write_signed_int(&stream->ctfser,
			bt_field_signed_integer_get_value(field),
			fc->base.base.alignment, fc->base.size, BYTE_ORDER);
	} else {
		ret = bt_ctfser_write_unsigned_int(&stream->ctfser,
			bt_field_unsigned_integer_get_value(field),
			fc->base.base.alignment, fc->base.size, BYTE_ORDER);
	}

	return ret;
}

static inline
int write_float_field(struct fs_sink_stream *stream,
		struct fs_sink_ctf_field_class_float *fc, const bt_field *field)
{
	int ret;
	double val = bt_field_real_get_value(field);

	if (fc->base.size == 32) {
		ret = bt_ctfser_write_float32(&stream->ctfser, val,
			fc->base.base.alignment, BYTE_ORDER);
	} else {
		ret = bt_ctfser_write_float32(&stream->ctfser, val,
			fc->base.base.alignment, BYTE_ORDER);
	}

	return ret;
}

static inline
int write_string_field(struct fs_sink_stream *stream,
		struct fs_sink_ctf_field_class_string *fc, const bt_field *field)
{
	return bt_ctfser_write_string(&stream->ctfser,
		bt_field_string_get_value(field));
}

static inline
int write_array_field_elements(struct fs_sink_stream *stream,
		struct fs_sink_ctf_field_class_array_base *fc,
		const bt_field *field)
{
	uint64_t i;
	uint64_t len = bt_field_array_get_length(field);
	int ret = 0;

	for (i = 0; i < len; i++) {
		const bt_field *elem_field =
			bt_field_array_borrow_element_field_by_index_const(
				field, i);
		ret = write_field(stream, fc->elem_fc, elem_field);
		if (unlikely(ret)) {
			goto end;
		}
	}

end:
	return ret;
}

static inline
int write_sequence_field(struct fs_sink_stream *stream,
		struct fs_sink_ctf_field_class_sequence *fc,
		const bt_field *field)
{
	int ret;

	if (fc->length_is_before) {
		ret = bt_ctfser_write_unsigned_int(&stream->ctfser,
			bt_field_array_get_length(field), 8, 32, BYTE_ORDER);
		if (unlikely(ret)) {
			goto end;
		}
	}

	ret = write_array_field_elements(stream, (void *) fc, field);

end:
	return ret;
}

static inline
int write_struct_field(struct fs_sink_stream *stream,
		struct fs_sink_ctf_field_class_struct *fc,
		const bt_field *field, bool align_struct)
{
	int ret = 0;
	uint64_t i;

	if (likely(align_struct)) {
		ret = bt_ctfser_align_offset_in_current_packet(&stream->ctfser,
			fc->base.alignment);
		if (unlikely(ret)) {
			goto end;
		}
	}

	for (i = 0; i < fc->members->len; i++) {
		const bt_field *memb_field =
			bt_field_structure_borrow_member_field_by_index_const(
				field, i);
		struct fs_sink_ctf_field_class *member_fc =
			fs_sink_ctf_field_class_struct_borrow_member_by_index(
				fc, i)->fc;

		ret = write_field(stream, member_fc, memb_field);
		if (unlikely(ret)) {
			goto end;
		}
	}

end:
	return ret;
}

static inline
int write_variant_field(struct fs_sink_stream *stream,
		struct fs_sink_ctf_field_class_variant *fc,
		const bt_field *field)
{
	uint64_t opt_index =
		bt_field_variant_get_selected_option_field_index(field);
	int ret;

	if (fc->tag_is_before) {
		ret = bt_ctfser_write_unsigned_int(&stream->ctfser,
			opt_index, 8, 16, BYTE_ORDER);
		if (unlikely(ret)) {
			goto end;
		}
	}

	ret = write_field(stream,
		fs_sink_ctf_field_class_variant_borrow_option_by_index(fc,
			opt_index)->fc,
		bt_field_variant_borrow_selected_option_field_const(field));

end:
	return ret;
}

static
int write_field(struct fs_sink_stream *stream,
		struct fs_sink_ctf_field_class *fc, const bt_field *field)
{
	int ret;

	switch (fc->type) {
	case FS_SINK_CTF_FIELD_CLASS_TYPE_INT:
		ret = write_int_field(stream, (void *) fc, field);
		break;
	case FS_SINK_CTF_FIELD_CLASS_TYPE_FLOAT:
		ret = write_float_field(stream, (void *) fc, field);
		break;
	case FS_SINK_CTF_FIELD_CLASS_TYPE_STRING:
		ret = write_string_field(stream, (void *) fc, field);
		break;
	case FS_SINK_CTF_FIELD_CLASS_TYPE_STRUCT:
		ret = write_struct_field(stream, (void *) fc, field, true);
		break;
	case FS_SINK_CTF_FIELD_CLASS_TYPE_ARRAY:
		ret = write_array_field_elements(stream, (void *) fc, field);
		break;
	case FS_SINK_CTF_FIELD_CLASS_TYPE_SEQUENCE:
		ret = write_sequence_field(stream, (void *) fc, field);
		break;
	case FS_SINK_CTF_FIELD_CLASS_TYPE_VARIANT:
		ret = write_variant_field(stream, (void *) fc, field);
		break;
	default:
		abort();
	}

	return ret;
}

static inline
int write_event_header(struct fs_sink_stream *stream,
		const bt_clock_snapshot *cs, struct fs_sink_ctf_event_class *ec)
{
	int ret;

	/* Event class ID */
	ret = bt_ctfser_write_byte_aligned_unsigned_int(&stream->ctfser,
		bt_event_class_get_id(ec->ir_ec), 8, 64, BYTE_ORDER);
	if (unlikely(ret)) {
		goto end;
	}

	/* Time */
	if (stream->sc->default_clock_class) {
		BT_ASSERT(cs);
		ret = bt_ctfser_write_byte_aligned_unsigned_int(&stream->ctfser,
			bt_clock_snapshot_get_value(cs), 8, 64, BYTE_ORDER);
		if (unlikely(ret)) {
			goto end;
		}
	}

end:
	return ret;
}

BT_HIDDEN
int fs_sink_stream_write_event(struct fs_sink_stream *stream,
		const bt_clock_snapshot *cs, const bt_event *event,
		struct fs_sink_ctf_event_class *ec)
{
	int ret;
	const bt_field *field;

	/* Header */
	ret = write_event_header(stream, cs, ec);
	if (unlikely(ret)) {
		goto end;
	}

	/* Common context */
	if (stream->sc->event_common_context_fc) {
		field = bt_event_borrow_common_context_field_const(event);
		BT_ASSERT(field);
		ret = write_struct_field(stream,
			(void *) stream->sc->event_common_context_fc,
			field, true);
		if (unlikely(ret)) {
			goto end;
		}
	}

	/* Specific context */
	if (ec->spec_context_fc) {
		field = bt_event_borrow_specific_context_field_const(event);
		BT_ASSERT(field);
		ret = write_struct_field(stream, (void *) ec->spec_context_fc,
			field, true);
		if (unlikely(ret)) {
			goto end;
		}
	}

	/* Specific context */
	if (ec->payload_fc) {
		field = bt_event_borrow_payload_field_const(event);
		BT_ASSERT(field);
		ret = write_struct_field(stream, (void *) ec->payload_fc,
			field, true);
		if (unlikely(ret)) {
			goto end;
		}
	}

end:
	return ret;
}

static
int write_packet_context(struct fs_sink_stream *stream)
{
	int ret;

	/* Packet total size */
	ret = bt_ctfser_write_byte_aligned_unsigned_int(&stream->ctfser,
		stream->packet_state.total_size, 8, 64, BYTE_ORDER);
	if (ret) {
		goto end;
	}

	/* Packet content size */
	ret = bt_ctfser_write_byte_aligned_unsigned_int(&stream->ctfser,
		stream->packet_state.content_size, 8, 64, BYTE_ORDER);
	if (ret) {
		goto end;
	}

	if (stream->sc->default_clock_class) {
		/* Beginning time */
		ret = bt_ctfser_write_byte_aligned_unsigned_int(&stream->ctfser,
			stream->packet_state.beginning_cs, 8, 64, BYTE_ORDER);
		if (ret) {
			goto end;
		}

		/* End time */
		ret = bt_ctfser_write_byte_aligned_unsigned_int(&stream->ctfser,
			stream->packet_state.end_cs, 8, 64, BYTE_ORDER);
		if (ret) {
			goto end;
		}
	}

	/* Discarded event counter */
	ret = bt_ctfser_write_byte_aligned_unsigned_int(&stream->ctfser,
		stream->packet_state.discarded_events_counter, 8, 64,
		BYTE_ORDER);
	if (ret) {
		goto end;
	}

	/* Sequence number */
	ret = bt_ctfser_write_byte_aligned_unsigned_int(&stream->ctfser,
		stream->packet_state.seq_num, 8, 64, BYTE_ORDER);
	if (ret) {
		goto end;
	}

	/* Other members */
	if (stream->sc->packet_context_fc) {
		const bt_field *packet_context_field =
			bt_packet_borrow_context_field_const(
				stream->packet_state.packet);

		BT_ASSERT(packet_context_field);
		ret = write_struct_field(stream,
			(void *) stream->sc->packet_context_fc,
			packet_context_field, false);
		if (ret) {
			goto end;
		}
	}

end:
	return ret;
}

BT_HIDDEN
int fs_sink_stream_open_packet(struct fs_sink_stream *stream,
		const bt_clock_snapshot *cs, const bt_packet *packet)
{
	int ret;
	uint64_t i;

	BT_ASSERT(!stream->packet_state.is_open);
	bt_packet_put_ref(stream->packet_state.packet);
	stream->packet_state.packet = packet;
	bt_packet_get_ref(stream->packet_state.packet);
	if (cs) {
		stream->packet_state.beginning_cs =
			bt_clock_snapshot_get_value(cs);
	}

	/* Open packet */
	ret = bt_ctfser_open_packet(&stream->ctfser);
	if (ret) {
		/* bt_ctfser_open_packet() logs errors */
		goto end;
	}

	/* Packet header: magic */
	bt_ctfser_write_byte_aligned_unsigned_int(&stream->ctfser,
		UINT64_C(0xc1fc1fc1), 8, 32, BYTE_ORDER);

	/* Packet header: UUID */
	for (i = 0; i < BABELTRACE_UUID_LEN; i++) {
		bt_ctfser_write_byte_aligned_unsigned_int(&stream->ctfser,
			(uint64_t) stream->sc->tc->uuid[i], 8, 8, BYTE_ORDER);
	}

	/* Packet header: stream class ID */
	bt_ctfser_write_byte_aligned_unsigned_int(&stream->ctfser,
		bt_stream_class_get_id(stream->sc->ir_sc), 8, 64, BYTE_ORDER);

	/* Packet header: stream ID */
	bt_ctfser_write_byte_aligned_unsigned_int(&stream->ctfser,
		bt_stream_get_id(stream->ir_stream), 8, 64, BYTE_ORDER);

	/* Save packet context's offset to rewrite it later */
	stream->packet_state.context_offset_bits =
		bt_ctfser_get_offset_in_current_packet_bits(&stream->ctfser);

	/* Write packet context just to advance to content (first event) */
	ret = write_packet_context(stream);
	if (ret) {
		goto end;
	}

	stream->packet_state.is_open = true;

end:
	return ret;
}

BT_HIDDEN
int fs_sink_stream_close_packet(struct fs_sink_stream *stream,
		const bt_clock_snapshot *cs)
{
	int ret;

	BT_ASSERT(stream->packet_state.is_open);

	if (cs) {
		stream->packet_state.end_cs = bt_clock_snapshot_get_value(cs);
	}

	stream->packet_state.content_size =
		bt_ctfser_get_offset_in_current_packet_bits(&stream->ctfser);
	stream->packet_state.total_size =
		(stream->packet_state.content_size + 7) & ~UINT64_C(7);

	/* Rewrite packet context */
	bt_ctfser_set_offset_in_current_packet_bits(&stream->ctfser,
		stream->packet_state.context_offset_bits);
	ret = write_packet_context(stream);
	if (ret) {
		goto end;
	}

	/* Close packet */
	bt_ctfser_close_current_packet(&stream->ctfser,
		stream->packet_state.total_size / 8);

	/* Partially copy current packet state to previous packet state */
	stream->prev_packet_state.end_cs = stream->packet_state.end_cs;
	stream->prev_packet_state.discarded_events_counter =
		stream->packet_state.discarded_events_counter;
	stream->prev_packet_state.seq_num =
		stream->packet_state.seq_num;

	/* Reset current packet state */
	stream->packet_state.beginning_cs = UINT64_C(-1);
	stream->packet_state.end_cs = UINT64_C(-1);
	stream->packet_state.content_size = 0;
	stream->packet_state.total_size = 0;
	stream->packet_state.seq_num += 1;
	stream->packet_state.context_offset_bits = 0;
	stream->packet_state.is_open = false;
	BT_PACKET_PUT_REF_AND_RESET(stream->packet_state.packet);

end:
	return ret;
}
