#ifndef BABELTRACE_PLUGIN_TRIMMER_COPY_H
#define BABELTRACE_PLUGIN_TRIMMER_COPY_H

/*
 * BabelTrace - Copy Trace Structure
 *
 * Copyright 2017 Julien Desfossez <jdesfossez@efficios.com>
 *
 * Author: Julien Desfossez <jdesfossez@efficios.com>
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
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/babeltrace.h>

BT_HIDDEN
const bt_event *debug_info_output_event(struct debug_info_iterator *debug_it,
		const bt_event *event);
BT_HIDDEN
const bt_packet *debug_info_new_packet(struct debug_info_iterator *debug_it,
		const bt_packet *packet);
BT_HIDDEN
const bt_packet *debug_info_close_packet(struct debug_info_iterator *debug_it,
		const bt_packet *packet);
BT_HIDDEN
const bt_stream *debug_info_stream_begin(
		struct debug_info_iterator *debug_it,
		const bt_stream *stream);
BT_HIDDEN
const bt_stream *debug_info_stream_end(struct debug_info_iterator *debug_it,
		const bt_stream *stream);

BT_HIDDEN
int get_stream_event_context_unsigned_int_field_value(FILE *err,
		const bt_event *event, const char *field_name,
		uint64_t *value);
BT_HIDDEN
int get_stream_event_context_int_field_value(FILE *err, const bt_event *event,
		const char *field_name, int64_t *value);
BT_HIDDEN
int get_payload_unsigned_int_field_value(FILE *err,
		const bt_event *event, const char *field_name,
		uint64_t *value);
BT_HIDDEN
int get_payload_int_field_value(FILE *err, const bt_event *event,
		const char *field_name, int64_t *value);
BT_HIDDEN
int get_payload_string_field_value(FILE *err,
		const bt_event *event, const char *field_name,
		const char **value);
BT_HIDDEN
int get_payload_build_id_field_value(FILE *err,
		const bt_event *event, const char *field_name,
		uint8_t **build_id, uint64_t *build_id_len);

#endif /* BABELTRACE_PLUGIN_TRIMMER_COPY_H */
