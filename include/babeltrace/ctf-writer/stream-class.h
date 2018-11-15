#ifndef BABELTRACE_CTF_WRITER_STREAM_CLASS_H
#define BABELTRACE_CTF_WRITER_STREAM_CLASS_H

/*
 * BabelTrace - CTF Writer: Stream Class
 *
 * Copyright 2014 EfficiOS Inc.
 *
 * Author: Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/object.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_ctf_stream_class;
struct bt_ctf_trace;
struct bt_ctf_event_class;
struct bt_ctf_field_type;
struct bt_ctf_clock;

extern struct bt_ctf_stream_class *bt_ctf_stream_class_create(
		const char *name);

extern struct bt_ctf_trace *bt_ctf_stream_class_get_trace(
		struct bt_ctf_stream_class *stream_class);

extern const char *bt_ctf_stream_class_get_name(
		struct bt_ctf_stream_class *stream_class);

extern int bt_ctf_stream_class_set_name(
		struct bt_ctf_stream_class *stream_class, const char *name);

extern int64_t bt_ctf_stream_class_get_id(
		struct bt_ctf_stream_class *stream_class);

extern int bt_ctf_stream_class_set_id(
		struct bt_ctf_stream_class *stream_class, uint64_t id);

extern struct bt_ctf_field_type *bt_ctf_stream_class_get_packet_context_type(
		struct bt_ctf_stream_class *stream_class);

extern int bt_ctf_stream_class_set_packet_context_type(
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_field_type *packet_context_type);

extern struct bt_ctf_field_type *
bt_ctf_stream_class_get_event_header_type(
		struct bt_ctf_stream_class *stream_class);

extern int bt_ctf_stream_class_set_event_header_type(
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_field_type *event_header_type);

extern struct bt_ctf_field_type *
bt_ctf_stream_class_get_event_context_type(
		struct bt_ctf_stream_class *stream_class);

extern int bt_ctf_stream_class_set_event_context_type(
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_field_type *event_context_type);

extern int64_t bt_ctf_stream_class_get_event_class_count(
		struct bt_ctf_stream_class *stream_class);

extern struct bt_ctf_event_class *bt_ctf_stream_class_get_event_class_by_index(
		struct bt_ctf_stream_class *stream_class, uint64_t index);

extern struct bt_ctf_event_class *bt_ctf_stream_class_get_event_class_by_id(
		struct bt_ctf_stream_class *stream_class, uint64_t id);

extern int bt_ctf_stream_class_add_event_class(
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_event_class *event_class);

extern int bt_ctf_stream_class_set_clock(
		struct bt_ctf_stream_class *ctf_stream_class,
		struct bt_ctf_clock *clock);

extern struct bt_ctf_clock *bt_ctf_stream_class_get_clock(
        struct bt_ctf_stream_class *stream_class);

/* Pre-2.0 CTF writer compatibility */
static inline
void bt_ctf_stream_class_get(struct bt_ctf_stream_class *stream_class)
{
	bt_object_get_ref(stream_class);
}

/* Pre-2.0 CTF writer compatibility */
static inline
void bt_ctf_stream_class_put(struct bt_ctf_stream_class *stream_class)
{
	bt_object_put_ref(stream_class);
}

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_CTF_WRITER_STREAM_CLASS_H */
