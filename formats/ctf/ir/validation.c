/*
 * validation.c
 *
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

#include <babeltrace/ctf-ir/validation-internal.h>
#include <babeltrace/ctf-ir/resolve-internal.h>
#include <babeltrace/ctf-ir/trace-internal.h>
#include <babeltrace/ctf-ir/stream-class-internal.h>
#include <babeltrace/ctf-ir/field-types-internal.h>
#include <babeltrace/ctf-ir/event-class-internal.h>
#include <babeltrace/values.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/ref.h>

#define _printf_error(fmt, args...) \
	printf_verbose("[validation] " fmt, ## args)

/*
 * This function resolves and validates the field types of an event
 * class. Only `event_context_type` and `event_payload_type` are
 * resolved and validated; the other field types are used as eventual
 * resolving targets.
 *
 * All parameters are owned by the caller.
 */
static
int validate_event_class_types(struct bt_value *environment,
		struct bt_ctf_field_type *packet_header_type,
		struct bt_ctf_field_type *packet_context_type,
		struct bt_ctf_field_type *event_header_type,
		struct bt_ctf_field_type *stream_event_ctx_type,
		struct bt_ctf_field_type *event_context_type,
		struct bt_ctf_field_type *event_payload_type)
{
	int ret = 0;

	/* Resolve sequence type lengths and variant type tags first */
	ret = bt_ctf_resolve_types(environment, packet_header_type,
		packet_context_type, event_header_type, stream_event_ctx_type,
		event_context_type, event_payload_type,
		BT_CTF_RESOLVE_FLAG_EVENT_CONTEXT |
		BT_CTF_RESOLVE_FLAG_EVENT_PAYLOAD);
	if (ret) {
		_printf_error("Cannot resolve event class types\n");
		goto end;
	}

	/* Validate field types individually */
	if (event_context_type) {
		ret = bt_ctf_field_type_validate(event_context_type);
		if (ret) {
			_printf_error("Invalid event context type\n");
			goto end;
		}
	}

	if (event_payload_type) {
		ret = bt_ctf_field_type_validate(event_payload_type);
		if (ret) {
			_printf_error("Invalid event payload type\n");
			goto end;
		}
	}

end:
	return ret;
}

/*
 * This function resolves and validates the field types of a stream
 * class. Only `packet_context_type`, `event_header_type`, and
 * `stream_event_ctx_type` are resolved and validated; the other field
 * type is used as an eventual resolving target.
 *
 * All parameters are owned by the caller.
 */
static
int validate_stream_class_types(struct bt_value *environment,
		struct bt_ctf_field_type *packet_header_type,
		struct bt_ctf_field_type *packet_context_type,
		struct bt_ctf_field_type *event_header_type,
		struct bt_ctf_field_type *stream_event_ctx_type)
{
	int ret = 0;

	/* Resolve sequence type lengths and variant type tags first */
	ret = bt_ctf_resolve_types(environment, packet_header_type,
		packet_context_type, event_header_type, stream_event_ctx_type,
		NULL, NULL,
		BT_CTF_RESOLVE_FLAG_PACKET_CONTEXT |
		BT_CTF_RESOLVE_FLAG_EVENT_HEADER |
		BT_CTF_RESOLVE_FLAG_STREAM_EVENT_CTX);
	if (ret) {
		_printf_error("Cannot resolve stream class types\n");
		goto end;
	}

	/* Validate field types individually */
	if (packet_context_type) {
		ret = bt_ctf_field_type_validate(packet_context_type);
		if (ret) {
			_printf_error("Invalid stream packet context type\n");
			goto end;
		}
	}

	if (event_header_type) {
		ret = bt_ctf_field_type_validate(event_header_type);
		if (ret) {
			_printf_error("Invalid stream event header type\n");
			goto end;
		}
	}

	if (stream_event_ctx_type) {
		ret = bt_ctf_field_type_validate(
			stream_event_ctx_type);
		if (ret) {
			_printf_error("Invalid stream event context type\n");
			goto end;
		}
	}

end:
	return ret;
}

/*
 * This function resolves and validates the field types of a trace.
 *
 * All parameters are owned by the caller.
 */
static
int validate_trace_types(struct bt_value *environment,
		struct bt_ctf_field_type *packet_header_type)
{
	int ret = 0;

	/* Resolve sequence type lengths and variant type tags first */
	ret = bt_ctf_resolve_types(environment, packet_header_type,
		NULL, NULL, NULL, NULL, NULL,
		BT_CTF_RESOLVE_FLAG_PACKET_HEADER);
	if (ret) {
		_printf_error("Cannot resolve trace types\n");
		goto end;
	}

	/* Validate field types individually */
	if (packet_header_type) {
		ret = bt_ctf_field_type_validate(packet_header_type);
		if (ret) {
			_printf_error("Invalid trace packet header type\n");
			goto end;
		}
	}

end:
	return ret;
}

/*
 * Checks whether or not `field_type` contains a variant or a sequence
 * field type, recursively. Returns 1 if it's the case.
 *
 * `field_type` is owned by the caller.
 */
static
int field_type_contains_sequence_or_variant_ft(struct bt_ctf_field_type *type)
{
	int ret = 0;
	enum ctf_type_id type_id = bt_ctf_field_type_get_type_id(type);

	switch (type_id) {
	case CTF_TYPE_SEQUENCE:
	case CTF_TYPE_VARIANT:
		ret = 1;
		goto end;
	case CTF_TYPE_ARRAY:
	case CTF_TYPE_STRUCT:
	{
		int i;
		int field_count = bt_ctf_field_type_get_field_count(type);

		if (field_count < 0) {
			ret = -1;
			goto end;
		}

		for (i = 0; i < field_count; ++i) {
			struct bt_ctf_field_type *child_type =
				bt_ctf_field_type_get_field_at_index(type, i);

			ret = field_type_contains_sequence_or_variant_ft(
				child_type);
			BT_PUT(child_type);
			if (ret != 0) {
				goto end;
			}
		}
		break;
	}
	default:
		break;
	}

end:
	return ret;
}

BT_HIDDEN
int bt_ctf_validate_class_types(struct bt_value *environment,
		struct bt_ctf_field_type *packet_header_type,
		struct bt_ctf_field_type *packet_context_type,
		struct bt_ctf_field_type *event_header_type,
		struct bt_ctf_field_type *stream_event_ctx_type,
		struct bt_ctf_field_type *event_context_type,
		struct bt_ctf_field_type *event_payload_type,
		int trace_valid, int stream_class_valid, int event_class_valid,
		struct bt_ctf_validation_output *output,
		enum bt_ctf_validation_flag validate_flags)
{
	int ret = 0;
	int contains_seq_var;
	int valid_ret;

	/* Clean output values */
	memset(output, 0, sizeof(*output));

	/* Set initial valid flags according to valid parameters */
	if (trace_valid) {
		output->valid_flags |= BT_CTF_VALIDATION_FLAG_TRACE;
	}

	if (stream_class_valid) {
		output->valid_flags |= BT_CTF_VALIDATION_FLAG_STREAM;
	}

	if (event_class_valid) {
		output->valid_flags |= BT_CTF_VALIDATION_FLAG_EVENT;
	}

	/* Own the type parameters */
	bt_get(packet_header_type);
	bt_get(packet_context_type);
	bt_get(event_header_type);
	bt_get(stream_event_ctx_type);
	bt_get(event_context_type);
	bt_get(event_payload_type);

	/* Validate trace */
	if ((validate_flags & BT_CTF_VALIDATION_FLAG_TRACE) && !trace_valid) {
		struct bt_ctf_field_type *packet_header_type_copy = NULL;

		/* Create field type copies */
		if (packet_header_type) {
			contains_seq_var =
				field_type_contains_sequence_or_variant_ft(
					packet_header_type);
			if (contains_seq_var < 0) {
				ret = contains_seq_var;
				goto error;
			} else if (!contains_seq_var) {
				/* No copy is needed */
				packet_header_type_copy = packet_header_type;
				bt_get(packet_header_type_copy);
				goto skip_packet_header_type_copy;
			}

			packet_header_type_copy =
				bt_ctf_field_type_copy(packet_header_type);
			if (!packet_header_type_copy) {
				ret = -1;
				_printf_error("Cannot copy packet header type\n");
				goto error;
			}

			/*
			 * Freeze this copy: if it's returned to the
			 * caller, it cannot be modified any way since
			 * it will be resolved.
			 */
			bt_ctf_field_type_freeze(packet_header_type_copy);
		}

skip_packet_header_type_copy:
		/* Put original reference and move copy */
		BT_MOVE(packet_header_type, packet_header_type_copy);

		/* Validate trace field types */
		valid_ret = validate_trace_types(environment,
			packet_header_type);
		if (!valid_ret) {
			/* Trace is valid */
			output->valid_flags |= BT_CTF_VALIDATION_FLAG_TRACE;
		}
	}

	/* Validate stream class */
	if ((validate_flags & BT_CTF_VALIDATION_FLAG_STREAM) &&
			!stream_class_valid) {
		struct bt_ctf_field_type *packet_context_type_copy = NULL;
		struct bt_ctf_field_type *event_header_type_copy = NULL;
		struct bt_ctf_field_type *stream_event_ctx_type_copy = NULL;

		if (packet_context_type) {
			contains_seq_var =
				field_type_contains_sequence_or_variant_ft(
					packet_context_type);
			if (contains_seq_var < 0) {
				ret = contains_seq_var;
				goto error;
			} else if (!contains_seq_var) {
				/* No copy is needed */
				packet_context_type_copy = packet_context_type;
				bt_get(packet_context_type_copy);
				goto skip_packet_context_type_copy;
			}

			packet_context_type_copy =
				bt_ctf_field_type_copy(packet_context_type);
			if (!packet_context_type_copy) {
				_printf_error("Cannot copy packet context type\n");
				goto sc_validation_error;
			}

			/*
			 * Freeze this copy: if it's returned to the
			 * caller, it cannot be modified any way since
			 * it will be resolved.
			 */
			bt_ctf_field_type_freeze(packet_context_type_copy);
		}

skip_packet_context_type_copy:
		if (event_header_type) {
			contains_seq_var =
				field_type_contains_sequence_or_variant_ft(
					event_header_type);
			if (contains_seq_var < 0) {
				ret = contains_seq_var;
				goto error;
			} else if (!contains_seq_var) {
				/* No copy is needed */
				event_header_type_copy = event_header_type;
				bt_get(event_header_type_copy);
				goto skip_event_header_type_copy;
			}

			event_header_type_copy =
				bt_ctf_field_type_copy(event_header_type);
			if (!event_header_type_copy) {
				_printf_error("Cannot copy event header type\n");
				goto sc_validation_error;
			}

			/*
			 * Freeze this copy: if it's returned to the
			 * caller, it cannot be modified any way since
			 * it will be resolved.
			 */
			bt_ctf_field_type_freeze(event_header_type_copy);
		}

skip_event_header_type_copy:
		if (stream_event_ctx_type) {
			contains_seq_var =
				field_type_contains_sequence_or_variant_ft(
					stream_event_ctx_type);
			if (contains_seq_var < 0) {
				ret = contains_seq_var;
				goto error;
			} else if (!contains_seq_var) {
				/* No copy is needed */
				stream_event_ctx_type_copy =
					stream_event_ctx_type;
				bt_get(stream_event_ctx_type_copy);
				goto skip_stream_event_ctx_type_copy;
			}

			stream_event_ctx_type_copy =
				bt_ctf_field_type_copy(stream_event_ctx_type);
			if (!stream_event_ctx_type_copy) {
				_printf_error("Cannot copy stream event context type\n");
				goto sc_validation_error;
			}

			/*
			 * Freeze this copy: if it's returned to the
			 * caller, it cannot be modified any way since
			 * it will be resolved.
			 */
			bt_ctf_field_type_freeze(stream_event_ctx_type_copy);
		}

skip_stream_event_ctx_type_copy:
		/* Put original references and move copies */
		BT_MOVE(packet_context_type, packet_context_type_copy);
		BT_MOVE(event_header_type, event_header_type_copy);
		BT_MOVE(stream_event_ctx_type, stream_event_ctx_type_copy);

		/* Validate stream class field types */
		valid_ret = validate_stream_class_types(environment,
			packet_header_type, packet_context_type,
			event_header_type, stream_event_ctx_type);
		if (!valid_ret) {
			/* Stream class is valid */
			output->valid_flags |= BT_CTF_VALIDATION_FLAG_STREAM;
		}

		goto sc_validation_done;

sc_validation_error:
		BT_PUT(packet_context_type_copy);
		BT_PUT(event_header_type_copy);
		BT_PUT(stream_event_ctx_type_copy);
		ret = -1;
		goto error;
	}

sc_validation_done:
	/* Validate event class */
	if ((validate_flags & BT_CTF_VALIDATION_FLAG_EVENT) &&
			!event_class_valid) {
		struct bt_ctf_field_type *event_context_type_copy = NULL;
		struct bt_ctf_field_type *event_payload_type_copy = NULL;

		if (event_context_type) {
			contains_seq_var =
				field_type_contains_sequence_or_variant_ft(
					event_context_type);
			if (contains_seq_var < 0) {
				ret = contains_seq_var;
				goto error;
			} else if (!contains_seq_var) {
				/* No copy is needed */
				event_context_type_copy = event_context_type;
				bt_get(event_context_type_copy);
				goto skip_event_context_type_copy;
			}

			event_context_type_copy =
				bt_ctf_field_type_copy(event_context_type);
			if (!event_context_type_copy) {
				_printf_error("Cannot copy event context type\n");
				goto ec_validation_error;
			}

			/*
			 * Freeze this copy: if it's returned to the
			 * caller, it cannot be modified any way since
			 * it will be resolved.
			 */
			bt_ctf_field_type_freeze(event_context_type_copy);
		}

skip_event_context_type_copy:
		if (event_payload_type) {
			contains_seq_var =
				field_type_contains_sequence_or_variant_ft(
					event_payload_type);
			if (contains_seq_var < 0) {
				ret = contains_seq_var;
				goto error;
			} else if (!contains_seq_var) {
				/* No copy is needed */
				event_payload_type_copy = event_payload_type;
				bt_get(event_payload_type_copy);
				goto skip_event_payload_type_copy;
			}

			event_payload_type_copy =
				bt_ctf_field_type_copy(event_payload_type);
			if (!event_payload_type_copy) {
				_printf_error("Cannot copy event payload type\n");
				goto ec_validation_error;
			}

			/*
			 * Freeze this copy: if it's returned to the
			 * caller, it cannot be modified any way since
			 * it will be resolved.
			 */
			bt_ctf_field_type_freeze(event_payload_type_copy);
		}

skip_event_payload_type_copy:
		/* Put original references and move copies */
		BT_MOVE(event_context_type, event_context_type_copy);
		BT_MOVE(event_payload_type, event_payload_type_copy);

		/* Validate event class field types */
		valid_ret = validate_event_class_types(environment,
			packet_header_type, packet_context_type,
			event_header_type, stream_event_ctx_type,
			event_context_type, event_payload_type);
		if (!valid_ret) {
			/* Event class is valid */
			output->valid_flags |= BT_CTF_VALIDATION_FLAG_EVENT;
		}

		goto ec_validation_done;

ec_validation_error:
		BT_PUT(event_context_type_copy);
		BT_PUT(event_payload_type_copy);
		ret = -1;
		goto error;
	}

ec_validation_done:
	/*
	 * Validation is complete. Move the field types that were used
	 * to validate (and that were possibly altered by the validation
	 * process) to the output values.
	 */
	BT_MOVE(output->packet_header_type, packet_header_type);
	BT_MOVE(output->packet_context_type, packet_context_type);
	BT_MOVE(output->event_header_type, event_header_type);
	BT_MOVE(output->stream_event_ctx_type, stream_event_ctx_type);
	BT_MOVE(output->event_context_type, event_context_type);
	BT_MOVE(output->event_payload_type, event_payload_type);

	return ret;

error:
	BT_PUT(packet_header_type);
	BT_PUT(packet_context_type);
	BT_PUT(event_header_type);
	BT_PUT(stream_event_ctx_type);
	BT_PUT(event_context_type);
	BT_PUT(event_payload_type);

	return ret;
}

BT_HIDDEN
void bt_ctf_validation_replace_types(struct bt_ctf_trace *trace,
		struct bt_ctf_stream_class *stream_class,
		struct bt_ctf_event_class *event_class,
		struct bt_ctf_validation_output *output,
		enum bt_ctf_validation_flag replace_flags)
{
	if ((replace_flags & BT_CTF_VALIDATION_FLAG_TRACE) && trace) {
		BT_MOVE(trace->packet_header_type, output->packet_header_type);
	}

	if ((replace_flags & BT_CTF_VALIDATION_FLAG_STREAM) && stream_class) {
		BT_MOVE(stream_class->packet_context_type,
			output->packet_context_type);
		BT_MOVE(stream_class->event_header_type,
			output->event_header_type);
		BT_MOVE(stream_class->event_context_type,
			output->stream_event_ctx_type);
	}

	if ((replace_flags & BT_CTF_VALIDATION_FLAG_EVENT) && event_class) {
		BT_MOVE(event_class->context, output->event_context_type);
		BT_MOVE(event_class->fields, output->event_payload_type);
	}
}

BT_HIDDEN
void bt_ctf_validation_output_put_types(
		struct bt_ctf_validation_output *output)
{
	BT_PUT(output->packet_header_type);
	BT_PUT(output->packet_context_type);
	BT_PUT(output->event_header_type);
	BT_PUT(output->stream_event_ctx_type);
	BT_PUT(output->event_context_type);
	BT_PUT(output->event_payload_type);
}
