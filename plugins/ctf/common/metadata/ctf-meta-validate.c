/*
 * Copyright 2018 - Philippe Proulx <pproulx@efficios.com>
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
 */

#define BT_LOG_TAG "PLUGIN-CTF-METADATA-META-VALIDATE"
#include "logging.h"

#include <babeltrace/babeltrace.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/assert-internal.h>
#include <glib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#include "ctf-meta-visitors.h"

static
int validate_stream_class(struct ctf_stream_class *sc)
{
	int ret = 0;
	struct ctf_field_class_int *int_fc;
	struct ctf_field_class *fc;
	bool has_total_size = false;
	bool has_content_size = false;

	if (sc->is_translated) {
		goto end;
	}

	fc = ctf_field_class_struct_borrow_member_field_class_by_name(
		(void *) sc->packet_context_fc, "timestamp_begin");
	if (fc) {
		if (fc->id != CTF_FIELD_CLASS_ID_INT &&
				fc->id != CTF_FIELD_CLASS_ID_ENUM) {
			BT_LOGE_STR("Invalid packet context field class: "
				"`timestamp_begin` member is not an integer field class.");
			goto invalid;
		}

		int_fc = (void *) fc;

		if (int_fc->is_signed) {
			BT_LOGE_STR("Invalid packet context field class: "
				"`timestamp_begin` member is signed.");
			goto invalid;
		}
	}

	fc = ctf_field_class_struct_borrow_member_field_class_by_name(
		(void *) sc->packet_context_fc, "timestamp_end");
	if (fc) {
		if (fc->id != CTF_FIELD_CLASS_ID_INT &&
				fc->id != CTF_FIELD_CLASS_ID_ENUM) {
			BT_LOGE_STR("Invalid packet context field class: "
				"`timestamp_end` member is not an integer field class.");
			goto invalid;
		}

		int_fc = (void *) fc;

		if (int_fc->is_signed) {
			BT_LOGE_STR("Invalid packet context field class: "
				"`timestamp_end` member is signed.");
			goto invalid;
		}
	}

	fc = ctf_field_class_struct_borrow_member_field_class_by_name(
		(void *) sc->packet_context_fc, "events_discarded");
	if (fc) {
		if (fc->id != CTF_FIELD_CLASS_ID_INT &&
				fc->id != CTF_FIELD_CLASS_ID_ENUM) {
			BT_LOGE_STR("Invalid packet context field class: "
				"`events_discarded` member is not an integer field class.");
			goto invalid;
		}

		int_fc = (void *) fc;

		if (int_fc->is_signed) {
			BT_LOGE_STR("Invalid packet context field class: "
				"`events_discarded` member is signed.");
			goto invalid;
		}
	}

	fc = ctf_field_class_struct_borrow_member_field_class_by_name(
		(void *) sc->packet_context_fc, "packet_seq_num");
	if (fc) {
		if (fc->id != CTF_FIELD_CLASS_ID_INT &&
				fc->id != CTF_FIELD_CLASS_ID_ENUM) {
			BT_LOGE_STR("Invalid packet context field class: "
				"`packet_seq_num` member is not an integer field class.");
			goto invalid;
		}

		int_fc = (void *) fc;

		if (int_fc->is_signed) {
			BT_LOGE_STR("Invalid packet context field class: "
				"`packet_seq_num` member is signed.");
			goto invalid;
		}
	}

	fc = ctf_field_class_struct_borrow_member_field_class_by_name(
		(void *) sc->packet_context_fc, "packet_size");
	if (fc) {
		if (fc->id != CTF_FIELD_CLASS_ID_INT &&
				fc->id != CTF_FIELD_CLASS_ID_ENUM) {
			BT_LOGE_STR("Invalid packet context field class: "
				"`packet_size` member is not an integer field class.");
			goto invalid;
		}

		int_fc = (void *) fc;

		if (int_fc->is_signed) {
			BT_LOGE_STR("Invalid packet context field class: "
				"`packet_size` member is signed.");
			goto invalid;
		}

		has_total_size = true;
	}

	fc = ctf_field_class_struct_borrow_member_field_class_by_name(
		(void *) sc->packet_context_fc, "content_size");
	if (fc) {
		if (fc->id != CTF_FIELD_CLASS_ID_INT &&
				fc->id != CTF_FIELD_CLASS_ID_ENUM) {
			BT_LOGE_STR("Invalid packet context field class: "
				"`content_size` member is not an integer field class.");
			goto invalid;
		}

		int_fc = (void *) fc;

		if (int_fc->is_signed) {
			BT_LOGE_STR("Invalid packet context field class: "
				"`content_size` member is signed.");
			goto invalid;
		}

		has_content_size = true;
	}

	if (has_content_size && !has_total_size) {
			BT_LOGE_STR("Invalid packet context field class: "
				"`content_size` member exists without "
				"`packet_size` member.");
			goto invalid;
	}

	fc = ctf_field_class_struct_borrow_member_field_class_by_name(
		(void *) sc->event_header_fc, "id");
	if (fc) {
		if (fc->id != CTF_FIELD_CLASS_ID_INT &&
				fc->id != CTF_FIELD_CLASS_ID_ENUM) {
			BT_LOGE_STR("Invalid event header field class: "
				"`id` member is not an integer field class.");
			goto invalid;
		}

		int_fc = (void *) fc;

		if (int_fc->is_signed) {
			BT_LOGE_STR("Invalid event header field class: "
				"`id` member is signed.");
			goto invalid;
		}
	} else {
		if (sc->event_classes->len > 1) {
			BT_LOGE_STR("Invalid event header field class: "
				"missing `id` member as there's "
				"more than one event class.");
			goto invalid;
		}
	}

	goto end;

invalid:
	ret = -1;

end:
	return ret;
}

BT_HIDDEN
int ctf_trace_class_validate(struct ctf_trace_class *ctf_tc)
{
	int ret = 0;
	struct ctf_field_class_int *int_fc;
	uint64_t i;

	if (!ctf_tc->is_translated) {
		struct ctf_field_class *fc;

		fc = ctf_field_class_struct_borrow_member_field_class_by_name(
			(void *) ctf_tc->packet_header_fc, "magic");
		if (fc) {
			struct ctf_named_field_class *named_fc =
				ctf_field_class_struct_borrow_member_by_index(
					(void *) ctf_tc->packet_header_fc,
					0);

			if (named_fc->fc != fc) {
				BT_LOGE_STR("Invalid packet header field class: "
					"`magic` member is not the first member.");
				goto invalid;
			}

			if (fc->id != CTF_FIELD_CLASS_ID_INT &&
					fc->id != CTF_FIELD_CLASS_ID_ENUM) {
				BT_LOGE_STR("Invalid packet header field class: "
					"`magic` member is not an integer field class.");
				goto invalid;
			}

			int_fc = (void *) fc;

			if (int_fc->is_signed) {
				BT_LOGE_STR("Invalid packet header field class: "
					"`magic` member is signed.");
				goto invalid;
			}

			if (int_fc->base.size != 32) {
				BT_LOGE_STR("Invalid packet header field class: "
					"`magic` member is not 32-bit.");
				goto invalid;
			}
		}

		fc = ctf_field_class_struct_borrow_member_field_class_by_name(
			(void *) ctf_tc->packet_header_fc, "stream_id");
		if (fc) {
			if (fc->id != CTF_FIELD_CLASS_ID_INT &&
					fc->id != CTF_FIELD_CLASS_ID_ENUM) {
				BT_LOGE_STR("Invalid packet header field class: "
					"`stream_id` member is not an integer field class.");
				goto invalid;
			}

			int_fc = (void *) fc;

			if (int_fc->is_signed) {
				BT_LOGE_STR("Invalid packet header field class: "
					"`stream_id` member is signed.");
				goto invalid;
			}
		} else {
			if (ctf_tc->stream_classes->len > 1) {
				BT_LOGE_STR("Invalid packet header field class: "
					"missing `stream_id` member as there's "
					"more than one stream class.");
				goto invalid;
			}
		}

		fc = ctf_field_class_struct_borrow_member_field_class_by_name(
			(void *) ctf_tc->packet_header_fc,
			"stream_instance_id");
		if (fc) {
			if (fc->id != CTF_FIELD_CLASS_ID_INT &&
					fc->id != CTF_FIELD_CLASS_ID_ENUM) {
				BT_LOGE_STR("Invalid packet header field class: "
					"`stream_instance_id` member is not an integer field class.");
				goto invalid;
			}

			int_fc = (void *) fc;

			if (int_fc->is_signed) {
				BT_LOGE_STR("Invalid packet header field class: "
					"`stream_instance_id` member is signed.");
				goto invalid;
			}
		}

		fc = ctf_field_class_struct_borrow_member_field_class_by_name(
			(void *) ctf_tc->packet_header_fc, "uuid");
		if (fc) {
			struct ctf_field_class_array *array_fc = (void *) fc;

			if (fc->id != CTF_FIELD_CLASS_ID_ARRAY) {
				BT_LOGE_STR("Invalid packet header field class: "
					"`uuid` member is not an array field class.");
				goto invalid;
			}

			array_fc = (void *) fc;

			if (array_fc->length != 16) {
				BT_LOGE_STR("Invalid packet header field class: "
					"`uuid` member is not a 16-element array field class.");
				goto invalid;
			}

			if (array_fc->base.elem_fc->id != CTF_FIELD_CLASS_ID_INT) {
				BT_LOGE_STR("Invalid packet header field class: "
					"`uuid` member's element field class is not "
					"an integer field class.");
				goto invalid;
			}

			int_fc = (void *) array_fc->base.elem_fc;

			if (int_fc->is_signed) {
				BT_LOGE_STR("Invalid packet header field class: "
					"`uuid` member's element field class "
					"is a signed integer field class.");
				goto invalid;
			}

			if (int_fc->base.size != 8) {
				BT_LOGE_STR("Invalid packet header field class: "
					"`uuid` member's element field class "
					"is not an 8-bit integer field class.");
				goto invalid;
			}

			if (int_fc->base.base.alignment != 8) {
				BT_LOGE_STR("Invalid packet header field class: "
					"`uuid` member's element field class's "
					"alignment is not 8.");
				goto invalid;
			}
		}
	}

	for (i = 0; i < ctf_tc->stream_classes->len; i++) {
		struct ctf_stream_class *sc =
			ctf_tc->stream_classes->pdata[i];

		ret = validate_stream_class(sc);
		if (ret) {
			BT_LOGE("Invalid stream class: sc-id=%" PRIu64, sc->id);
			goto invalid;
		}
	}

	goto end;

invalid:
	ret = -1;

end:
	return ret;
}
