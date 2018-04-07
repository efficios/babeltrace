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

#include <babeltrace/values.h>
#include <babeltrace/babeltrace-internal.h>

struct bt_trace_common;
struct bt_stream_class_common;
struct bt_event_class_common;
struct bt_field_type_common;

typedef struct bt_field_type_common *(*bt_validation_flag_copy_field_type_func)(
		struct bt_field_type_common *);

enum bt_validation_flag {
	BT_VALIDATION_FLAG_TRACE	= 1,
	BT_VALIDATION_FLAG_STREAM	= 2,
	BT_VALIDATION_FLAG_EVENT	= 4,
};

/*
 * Validation output structure.
 *
 * This is where the results of the validation function go. The field
 * types are the validated ones which should replace the original field
 * types of a trace, a stream class, and an event class.
 *
 * `valid_flags` contains the results of the validation.
 */
struct bt_validation_output {
	struct bt_field_type_common *packet_header_type;
	struct bt_field_type_common *packet_context_type;
	struct bt_field_type_common *event_header_type;
	struct bt_field_type_common *stream_event_ctx_type;
	struct bt_field_type_common *event_context_type;
	struct bt_field_type_common *event_payload_type;
	enum bt_validation_flag valid_flags;
};

/*
 * This function resolves and validates the field types of an event
 * class, a stream class, and a trace. Copies are created if needed
 * and the resulting field types to use are placed in the `output`
 * validation structure, which also contains the results of the
 * validation. Copies can replace the original field types of a trace,
 * a stream class, and an event class using
 * bt_validation_replace_types().
 *
 * The current known validity of the field types of the trace,
 * stream class, and event class must be indicated with the
 * `trace_valid`, `stream_class_valid`, and `event_class_valid`
 * parameters. If a class is valid, its field types are not copied,
 * validated, or resolved during this call.
 *
 * The validation flags `validate_flags` indicate which classes should
 * have their field types validated.
 *
 * All parameters are owned by the caller.
 */
BT_HIDDEN
int bt_validate_class_types(struct bt_value *environment,
		struct bt_field_type_common *packet_header_type,
		struct bt_field_type_common *packet_context_type,
		struct bt_field_type_common *event_header_type,
		struct bt_field_type_common *stream_event_ctx_type,
		struct bt_field_type_common *event_context_type,
		struct bt_field_type_common *event_payload_type,
		int trace_valid, int stream_class_valid, int event_class_valid,
		struct bt_validation_output *output,
		enum bt_validation_flag validate_flags,
		bt_validation_flag_copy_field_type_func copy_field_type_func);

/*
 * This function replaces the actual field types of a trace, a stream
 * class, and an event class with the appropriate field types contained
 * in a validation output structure.
 *
 * The replace flags `replace_flags` indicate which classes should have
 * their field types replaced.
 *
 * Note that the field types that are not used in the validation output
 * structure are still owned by it at the end of this call.
 * bt_validation_output_put_types() should be called to clean the
 * structure.
 *
 * All parameters are owned by the caller.
 */
BT_HIDDEN
void bt_validation_replace_types(struct bt_trace_common *trace,
		struct bt_stream_class_common *stream_class,
		struct bt_event_class_common *event_class,
		struct bt_validation_output *output,
		enum bt_validation_flag replace_flags);

/*
 * This function puts all the field types contained in a given
 * validation output structure.
 *
 * `output` is owned by the caller and is not freed here.
 */
BT_HIDDEN
void bt_validation_output_put_types(
		struct bt_validation_output *output);

#endif /* BABELTRACE_CTF_IR_VALIDATION_INTERNAL_H */
