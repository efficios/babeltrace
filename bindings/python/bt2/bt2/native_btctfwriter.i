/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Philippe Proulx <pproulx@efficios.com>
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
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/* Types */
struct bt_ctf_clock;
struct bt_ctf_writer;

/* Clock functions */
struct bt_ctf_clock *bt_ctf_clock_create(const char *name);
const char *bt_ctf_clock_get_name(struct bt_ctf_clock *clock);
const char *bt_ctf_clock_get_description(struct bt_ctf_clock *clock);
int bt_ctf_clock_set_description(struct bt_ctf_clock *clock,
		const char *desc);
uint64_t bt_ctf_clock_get_frequency(struct bt_ctf_clock *clock);
int bt_ctf_clock_set_frequency(struct bt_ctf_clock *clock,
		uint64_t freq);
uint64_t bt_ctf_clock_get_precision(struct bt_ctf_clock *clock);
int bt_ctf_clock_set_precision(struct bt_ctf_clock *clock,
		uint64_t precision);
int bt_ctf_clock_get_offset_s(struct bt_ctf_clock *clock,
		int64_t *OUTPUT);
int bt_ctf_clock_set_offset_s(struct bt_ctf_clock *clock,
		int64_t offset_s);
int bt_ctf_clock_get_offset(struct bt_ctf_clock *clock,
		int64_t *OUTPUT);
int bt_ctf_clock_set_offset(struct bt_ctf_clock *clock,
		int64_t offset);
int bt_ctf_clock_get_is_absolute(struct bt_ctf_clock *clock);
int bt_ctf_clock_set_is_absolute(struct bt_ctf_clock *clock,
		int is_absolute);
BTUUID bt_ctf_clock_get_uuid(struct bt_ctf_clock *clock);
int bt_ctf_clock_set_uuid(struct bt_ctf_clock *clock,
		BTUUID uuid);
int bt_ctf_clock_set_time(struct bt_ctf_clock *clock,
		int64_t time);

/* Stream functions */
int bt_stream_get_discarded_events_count(
		struct bt_stream *stream, uint64_t *OUTPUT);
void bt_stream_append_discarded_events(struct bt_stream *stream,
		uint64_t event_count);
int bt_stream_append_event(struct bt_stream *stream,
		struct bt_event *event);
struct bt_field *bt_stream_get_packet_header(
		struct bt_stream *stream);
int bt_stream_set_packet_header(
		struct bt_stream *stream,
		struct bt_field *packet_header);
struct bt_field *bt_stream_get_packet_context(
		struct bt_stream *stream);
int bt_stream_set_packet_context(
		struct bt_stream *stream,
		struct bt_field *packet_context);
int bt_stream_flush(struct bt_stream *stream);
int bt_stream_is_writer(struct bt_stream *stream);

/* Stream class functions */
int bt_stream_class_set_clock(
		struct bt_stream_class *stream_class,
		struct bt_ctf_clock *clock);
struct bt_ctf_clock *bt_stream_class_get_clock(
		struct bt_stream_class *stream_class);

/* Writer functions */
struct bt_ctf_writer *bt_ctf_writer_create(const char *path);
struct bt_trace *bt_ctf_writer_get_trace(
		struct bt_ctf_writer *writer);
int bt_ctf_writer_add_clock(struct bt_ctf_writer *writer,
		struct bt_ctf_clock *clock);
char *bt_ctf_writer_get_metadata_string(struct bt_ctf_writer *writer);
void bt_ctf_writer_flush_metadata(struct bt_ctf_writer *writer);
