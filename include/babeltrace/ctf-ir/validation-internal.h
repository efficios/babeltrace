#ifndef BABELTRACE_CTF_IR_VALIDATION_INTERNAL_H
#define BABELTRACE_CTF_IR_VALIDATION_INTERNAL_H

/*
 * Babeltrace - CTF IR: Validation of trace, stream class, and event class
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

#include <babeltrace/ctf-ir/event-types.h>
#include <babeltrace/ctf-ir/event.h>
#include <babeltrace/ctf-ir/stream-class.h>
#include <babeltrace/ctf-ir/trace.h>
#include <babeltrace/babeltrace-internal.h>

enum bt_ctf_validation_flag {
	BT_CTF_VALIDATION_FLAG_TRACE	= 1,
	BT_CTF_VALIDATION_FLAG_STREAM	= 2,
	BT_CTF_VALIDATION_FLAG_EVENT	= 4,
};

struct bt_ctf_validation_output {
	struct bt_ctf_field_type *packet_header_type;
	struct bt_ctf_field_type *packet_context_type;
	struct bt_ctf_field_type *event_header_type;
	struct bt_ctf_field_type *stream_event_ctx_type;
	struct bt_ctf_field_type *event_context_type;
	struct bt_ctf_field_type *event_payload_type;
	enum bt_ctf_validation_flag valid_flags;
};

/*
 * bt_ctf_validate_class_types() validates the types of a trace, of a
 * stream class which should be part of this trace, and of an event
 * class which should be part of this stream class.
 *
 * The caller can control what exactly is validated thanks to the
 * validation flags `validate_flags`.
 *
 * When sucessful (returns 0), the validation results are placed in
 * `output->valid_flags`. The field type copies that were created for
 * the validation process are also placed in `output` (the caller
 * becomes their owner).
 *
 * This function does not replace any field types, nor does it mark
 * any object as valid.
 */
BT_HIDDEN
int bt_ctf_validate_class_types(struct bt_ctf_field_type *packet_header_type,
		struct bt_ctf_field_type *packet_context_type,
		struct bt_ctf_field_type *event_header_type,
		struct bt_ctf_field_type *stream_event_ctx_type,
		struct bt_ctf_field_type *event_context_type,
		struct bt_ctf_field_type *event_payload_type,
		int trace_valid, int stream_class_valid, int event_class_valid,
		struct bt_ctf_validation_output *output,
		enum bt_ctf_validation_flag validate_flags);

BT_HIDDEN
void bt_ctf_validation_replace_types(struct bt_ctf_trace *trace,
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_event_class *event_class,
		struct bt_ctf_validation_output *output,
		enum bt_ctf_validation_flag replace_flags);

BT_HIDDEN
void bt_ctf_validation_output_put_types(
		struct bt_ctf_validation_output *output);

#endif /* BABELTRACE_CTF_IR_VALIDATION_INTERNAL_H */
