#ifndef BABELTRACE_LIB_COPY_TRACE_H
#define BABELTRACE_LIB_COPY_TRACE_H

/*
 * BabelTrace - Library to create a copy of a CTF trace
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

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Create a copy of the clock_class passed in parameter.
 *
 * Returns NULL on error.
 */
const bt_clock_class *ctf_copy_clock_class(FILE *err,
		const bt_clock_class *clock_class);

/*
 * Copy all the clock classes from the input trace and add them to the writer
 * object.
 *
 * Returns BT_COMPONENT_STATUS_OK on success, and BT_COMPONENT_STATUS_ERROR on
 * error.
 */
BT_HIDDEN
bt_component_status ctf_copy_clock_classes(FILE *err,
		const bt_trace *writer_trace,
		const bt_stream_class *writer_stream_class,
		const bt_trace *trace);

/*
 * Create a copy of the event class passed in parameter.
 *
 * Returns NULL on error.
 */
BT_HIDDEN
const bt_event_class *ctf_copy_event_class(FILE *err,
		const bt_trace *trace_copy,
		const bt_event_class *event_class);

/*
 * Copy all the event classes from the input stream class and add them to the
 * writer_stream_class.
 *
 * Returns BT_COMPONENT_STATUS_OK on success, and BT_COMPONENT_STATUS_ERROR on
 * error.
 */
BT_HIDDEN
bt_component_status ctf_copy_event_classes(FILE *err,
		const bt_stream_class *stream_class,
		const bt_stream_class *writer_stream_class);

/*
 * Create a copy of the stream class passed in parameter.
 *
 * Returns NULL or error.
 */
BT_HIDDEN
const bt_stream_class *ctf_copy_stream_class(FILE *err,
		const bt_stream_class *stream_class,
		const bt_trace *writer_trace,
		bool override_ts64);

/*
 * Copy the value of a packet context field and add it to the
 * writer_packet_context. Only supports unsigned integers for now.
 *
 * Returns BT_COMPONENT_STATUS_OK on success, and BT_COMPONENT_STATUS_ERROR on
 * error.
 */
BT_HIDDEN
bt_component_status ctf_copy_packet_context_field(FILE *err,
		const bt_field *field, const char *field_name,
		const bt_field *writer_packet_context,
		bt_field_type *writer_packet_context_type);


/*
 * Copy the packet_header from the packet passed in parameter and assign it
 * to the writer_stream.
 *
 * Returns 0 on success or -1 on error.
 */
BT_HIDDEN
int ctf_stream_copy_packet_header(FILE *err, const bt_packet *packet,
		const bt_stream *writer_stream);

/*
 * Copy the packet_header from the packet passed in parameter and assign it
 * to the writer_packet.
 *
 * Returns 0 on success or -1 on error.
 */
BT_HIDDEN
int ctf_packet_copy_header(FILE *err, const bt_packet *packet,
		const bt_packet *writer_packet);

/*
 * Copy all the field values of the packet context from the packet passed in
 * parameter and set it to the writer_stream.
 *
 * Returns 0 on success or -1 on error.
 */
BT_HIDDEN
int ctf_stream_copy_packet_context(FILE *err, const bt_packet *packet,
		const bt_stream *writer_stream);

/*
 * Copy all the field values of the packet context from the packet passed in
 * parameter and set it to the writer_packet.
 *
 * Returns 0 on success or -1 on error.
 */
BT_HIDDEN
int ctf_packet_copy_context(FILE *err, const bt_packet *packet,
		const bt_stream *writer_stream,
		const bt_packet *writer_packet);

/*
 * Create and return a copy of the event passed in parameter. The caller has to
 * append it to the writer_stream.
 *
 * Returns NULL on error.
 */
BT_HIDDEN
const bt_event *ctf_copy_event(FILE *err, const bt_event *event,
		const bt_event_class *writer_event_class,
		bool override_ts64);

/*
 * Copies the content of the event header to writer_event_header.
 *
 * Returns 0 on success, -1 on error.
 */
BT_HIDDEN
int ctf_copy_event_header(FILE *err, const bt_event *event,
		const bt_event_class *writer_event_class,
		const bt_event *writer_event,
		const bt_field *event_header);

/*
 * Copy the environment and the packet header from the input trace to the
 * writer_trace.
 *
 * Returns BT_COMPONENT_STATUS_OK on success, and BT_COMPONENT_STATUS_ERROR on
 * error.
 */
BT_HIDDEN
bt_component_status ctf_copy_trace(FILE *err, const bt_trace *trace,
		const bt_trace *writer_trace);

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_LIB_COPY_TRACE_H */
