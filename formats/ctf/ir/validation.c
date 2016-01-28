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
#include <babeltrace/ctf-ir/event-internal.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/ref.h>

static
int validate_event_class_types(
		struct bt_ctf_field_type *packet_header_type,
		struct bt_ctf_field_type *packet_context_type,
		struct bt_ctf_field_type *event_header_type,
		struct bt_ctf_field_type *stream_event_ctx_type,
		struct bt_ctf_field_type *event_context_type,
		struct bt_ctf_field_type *event_payload_type)
{
	int ret = 0;

	/* Resolve sequence type lengths and variant type tags first */
	ret = bt_ctf_resolve_types(packet_header_type, packet_context_type,
		event_header_type, stream_event_ctx_type, event_context_type,
		event_payload_type,
		BT_CTF_RESOLVE_FLAG_EVENT_CONTEXT |
		BT_CTF_RESOLVE_FLAG_EVENT_PAYLOAD);

	if (ret) {
		goto end;
	}

	/* Validate event field types */
	if (event_context_type) {
		ret = bt_ctf_field_type_validate_recursive(event_context_type);

		if (ret) {
			goto end;
		}
	}

	if (event_payload_type) {
		ret = bt_ctf_field_type_validate_recursive(event_payload_type);

		if (ret) {
			goto end;
		}
	}

end:
	return ret;
}

static
int validate_stream_class_types(
		struct bt_ctf_field_type *packet_header_type,
		struct bt_ctf_field_type *packet_context_type,
		struct bt_ctf_field_type *event_header_type,
		struct bt_ctf_field_type *stream_event_ctx_type)
{
	int ret = 0;

	/* Resolve sequence type lengths and variant type tags first */
	ret = bt_ctf_resolve_types(packet_header_type, packet_context_type,
		event_header_type, stream_event_ctx_type, NULL, NULL,
		BT_CTF_RESOLVE_FLAG_PACKET_CONTEXT |
		BT_CTF_RESOLVE_FLAG_EVENT_HEADER |
		BT_CTF_RESOLVE_FLAG_STREAM_EVENT_CTX);

	if (ret) {
		goto end;
	}

	/* Validate field types */
	if (packet_context_type) {
		ret = bt_ctf_field_type_validate_recursive(packet_context_type);

		if (ret) {
			goto end;
		}
	}

	if (event_header_type) {
		ret = bt_ctf_field_type_validate_recursive(event_header_type);

		if (ret) {
			goto end;
		}
	}

	if (stream_event_ctx_type) {
		ret = bt_ctf_field_type_validate_recursive(
			stream_event_ctx_type);

		if (ret) {
			goto end;
		}
	}

end:
	return ret;
}

static
int validate_trace_types(struct bt_ctf_field_type *packet_header_type)
{
	int ret = 0;

	/* Resolve sequence type lengths and variant type tags first */
	ret = bt_ctf_resolve_types(packet_header_type, NULL, NULL, NULL,
		NULL, NULL, BT_CTF_RESOLVE_FLAG_PACKET_HEADER);

	if (ret) {
		goto end;
	}

	/* Validate field types */
	if (packet_header_type) {
		ret = bt_ctf_field_type_validate_recursive(packet_header_type);

		if (ret) {
			goto end;
		}
	}

end:
	return ret;
}

BT_HIDDEN
int bt_ctf_validate_class_types(struct bt_ctf_field_type *packet_header_type,
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

	/* Clean output values */
	memset(output, 0, sizeof(*output));

	/* Validate trace */
	if ((validate_flags & BT_CTF_VALIDATION_FLAG_TRACE) && !trace_valid) {
		struct bt_ctf_field_type *packet_header_type_copy = NULL;

		/* Create field type copies */
		if (packet_header_type) {
			packet_header_type_copy =
				bt_ctf_field_type_copy(packet_header_type);

			if (!packet_header_type_copy) {
				ret = -1;
				goto error;
			}
		}

		/* Put original reference and move copy */
		BT_PUT(packet_header_type);
		BT_MOVE(packet_header_type, packet_header_type_copy);

		/* Validate trace field types */
		ret = validate_trace_types(packet_header_type);

		if (!ret) {
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
			packet_context_type_copy =
				bt_ctf_field_type_copy(packet_context_type);

			if (!packet_context_type_copy) {
				goto sc_validation_error;
			}
		}

		if (event_header_type) {
			event_header_type_copy =
				bt_ctf_field_type_copy(event_header_type);

			if (!event_header_type_copy) {
				goto sc_validation_error;
			}
		}

		if (stream_event_ctx_type) {
			stream_event_ctx_type_copy =
				bt_ctf_field_type_copy(stream_event_ctx_type);

			if (!stream_event_ctx_type_copy) {
				goto sc_validation_error;
			}
		}

		/* Put original references and move copies */
		BT_PUT(packet_context_type);
		BT_PUT(event_header_type);
		BT_PUT(stream_event_ctx_type);
		BT_MOVE(packet_context_type, packet_context_type_copy);
		BT_MOVE(event_header_type, event_header_type_copy);
		BT_MOVE(stream_event_ctx_type, stream_event_ctx_type_copy);

		/* Validate stream class field types */
		ret = validate_stream_class_types(packet_header_type,
			packet_context_type, event_header_type,
			stream_event_ctx_type);

		if (!ret) {
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
			event_context_type_copy =
				bt_ctf_field_type_copy(event_context_type);

			if (!event_context_type_copy) {
				goto ec_validation_error;
			}
		}

		if (event_payload_type) {
			event_payload_type_copy =
				bt_ctf_field_type_copy(event_payload_type);

			if (!event_payload_type_copy) {
				goto ec_validation_error;
			}
		}

		/* Put original references and move copies */
		BT_PUT(event_context_type);
		BT_PUT(event_payload_type);
		BT_MOVE(event_context_type, event_context_type_copy);
		BT_MOVE(event_payload_type, event_payload_type_copy);

		/* Validate event class field types */
		ret = validate_event_class_types(packet_header_type,
			packet_context_type, event_header_type,
			stream_event_ctx_type, event_context_type,
			event_payload_type);

		if (!ret) {
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
		BT_PUT(trace->packet_header_type);
		BT_MOVE(trace->packet_header_type, output->packet_header_type);
	}

	if ((replace_flags & BT_CTF_VALIDATION_FLAG_STREAM) && stream_class) {
		BT_PUT(stream_class->packet_context_type);
		BT_PUT(stream_class->event_header_type);
		BT_PUT(stream_class->event_context_type);
		BT_MOVE(stream_class->packet_context_type,
			output->packet_context_type);
		BT_MOVE(stream_class->event_header_type,
			output->event_header_type);
		BT_MOVE(stream_class->event_context_type,
			output->stream_event_ctx_type);
	}

	if ((replace_flags & BT_CTF_VALIDATION_FLAG_EVENT) && event_class) {
		BT_PUT(event_class->context);
		BT_PUT(event_class->fields);
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
