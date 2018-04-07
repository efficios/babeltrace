#ifndef BABELTRACE_CTF_WRITER_STREAM_CLASS_INTERNAL_H
#define BABELTRACE_CTF_WRITER_STREAM_CLASS_INTERNAL_H

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

#include <babeltrace/ctf-ir/stream-class-internal.h>
#include <babeltrace/ctf-writer/field-types.h>
#include <babeltrace/ctf-writer/clock-internal.h>

struct bt_ctf_stream_class {
	struct bt_stream_class_common common;
	struct bt_ctf_clock *clock;
	int64_t next_stream_id;
};

struct metadata_context;

BT_HIDDEN
int bt_ctf_stream_class_serialize(struct bt_ctf_stream_class *stream_class,
		struct metadata_context *context);

BT_HIDDEN
int bt_ctf_stream_class_map_clock_class(
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_field_type *packet_context_type,
		struct bt_ctf_field_type *event_header_type);

#endif /* BABELTRACE_CTF_WRITER_STREAM_CLASS_INTERNAL_H */
