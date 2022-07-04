/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2016 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_CTF_WRITER_VALIDATION_INTERNAL_H
#define BABELTRACE_CTF_WRITER_VALIDATION_INTERNAL_H

#include "common/macros.h"

#include "values.h"

struct bt_ctf_trace_common;
struct bt_ctf_stream_class_common;
struct bt_ctf_event_class_common;
struct bt_ctf_field_type_common;

typedef struct bt_ctf_field_type_common *(*bt_ctf_validation_flag_copy_field_type_func)(
		struct bt_ctf_field_type_common *);

enum bt_ctf_validation_flag {
	BT_CTF_VALIDATION_FLAG_TRACE	= 1,
	BT_CTF_VALIDATION_FLAG_STREAM	= 2,
	BT_CTF_VALIDATION_FLAG_EVENT	= 4,
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
struct bt_ctf_validation_output {
	struct bt_ctf_field_type_common *packet_header_type;
	struct bt_ctf_field_type_common *packet_context_type;
	struct bt_ctf_field_type_common *event_header_type;
	struct bt_ctf_field_type_common *stream_event_ctx_type;
	struct bt_ctf_field_type_common *event_context_type;
	struct bt_ctf_field_type_common *event_payload_type;
	enum bt_ctf_validation_flag valid_flags;
};

/*
 * This function resolves and validates the field types of an event
 * class, a stream class, and a trace. Copies are created if needed
 * and the resulting field types to use are placed in the `output`
 * validation structure, which also contains the results of the
 * validation. Copies can replace the original field types of a trace,
 * a stream class, and an event class using
 * bt_ctf_validation_replace_types().
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
int bt_ctf_validate_class_types(struct bt_ctf_private_value *environment,
		struct bt_ctf_field_type_common *packet_header_type,
		struct bt_ctf_field_type_common *packet_context_type,
		struct bt_ctf_field_type_common *event_header_type,
		struct bt_ctf_field_type_common *stream_event_ctx_type,
		struct bt_ctf_field_type_common *event_context_type,
		struct bt_ctf_field_type_common *event_payload_type,
		int trace_valid, int stream_class_valid, int event_class_valid,
		struct bt_ctf_validation_output *output,
		enum bt_ctf_validation_flag validate_flags,
		bt_ctf_validation_flag_copy_field_type_func copy_field_type_func);

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
 * bt_ctf_validation_output_put_types() should be called to clean the
 * structure.
 *
 * All parameters are owned by the caller.
 */
void bt_ctf_validation_replace_types(struct bt_ctf_trace_common *trace,
		struct bt_ctf_stream_class_common *stream_class,
		struct bt_ctf_event_class_common *event_class,
		struct bt_ctf_validation_output *output,
		enum bt_ctf_validation_flag replace_flags);

/*
 * This function puts all the field types contained in a given
 * validation output structure.
 *
 * `output` is owned by the caller and is not freed here.
 */
void bt_ctf_validation_output_put_types(
		struct bt_ctf_validation_output *output);

#endif /* BABELTRACE_CTF_WRITER_VALIDATION_INTERNAL_H */
