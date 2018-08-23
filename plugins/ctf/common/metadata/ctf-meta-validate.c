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
	struct ctf_field_type_int *int_ft;
	struct ctf_field_type *ft;
	bool has_total_size = false;
	bool has_content_size = false;

	if (sc->is_translated) {
		goto end;
	}

	ft = ctf_field_type_struct_borrow_member_field_type_by_name(
		(void *) sc->packet_context_ft, "timestamp_begin");
	if (ft) {
		if (ft->id != CTF_FIELD_TYPE_ID_INT &&
				ft->id != CTF_FIELD_TYPE_ID_ENUM) {
			BT_LOGE_STR("Invalid packet context field type: "
				"`timestamp_begin` member is not an integer field type.");
			goto invalid;
		}

		int_ft = (void *) ft;

		if (int_ft->is_signed) {
			BT_LOGE_STR("Invalid packet context field type: "
				"`timestamp_begin` member is signed.");
			goto invalid;
		}
	}

	ft = ctf_field_type_struct_borrow_member_field_type_by_name(
		(void *) sc->packet_context_ft, "timestamp_end");
	if (ft) {
		if (ft->id != CTF_FIELD_TYPE_ID_INT &&
				ft->id != CTF_FIELD_TYPE_ID_ENUM) {
			BT_LOGE_STR("Invalid packet context field type: "
				"`timestamp_end` member is not an integer field type.");
			goto invalid;
		}

		int_ft = (void *) ft;

		if (int_ft->is_signed) {
			BT_LOGE_STR("Invalid packet context field type: "
				"`timestamp_end` member is signed.");
			goto invalid;
		}
	}

	ft = ctf_field_type_struct_borrow_member_field_type_by_name(
		(void *) sc->packet_context_ft, "events_discarded");
	if (ft) {
		if (ft->id != CTF_FIELD_TYPE_ID_INT &&
				ft->id != CTF_FIELD_TYPE_ID_ENUM) {
			BT_LOGE_STR("Invalid packet context field type: "
				"`events_discarded` member is not an integer field type.");
			goto invalid;
		}

		int_ft = (void *) ft;

		if (int_ft->is_signed) {
			BT_LOGE_STR("Invalid packet context field type: "
				"`events_discarded` member is signed.");
			goto invalid;
		}
	}

	ft = ctf_field_type_struct_borrow_member_field_type_by_name(
		(void *) sc->packet_context_ft, "packet_seq_num");
	if (ft) {
		if (ft->id != CTF_FIELD_TYPE_ID_INT &&
				ft->id != CTF_FIELD_TYPE_ID_ENUM) {
			BT_LOGE_STR("Invalid packet context field type: "
				"`packet_seq_num` member is not an integer field type.");
			goto invalid;
		}

		int_ft = (void *) ft;

		if (int_ft->is_signed) {
			BT_LOGE_STR("Invalid packet context field type: "
				"`packet_seq_num` member is signed.");
			goto invalid;
		}
	}

	ft = ctf_field_type_struct_borrow_member_field_type_by_name(
		(void *) sc->packet_context_ft, "packet_size");
	if (ft) {
		if (ft->id != CTF_FIELD_TYPE_ID_INT &&
				ft->id != CTF_FIELD_TYPE_ID_ENUM) {
			BT_LOGE_STR("Invalid packet context field type: "
				"`packet_size` member is not an integer field type.");
			goto invalid;
		}

		int_ft = (void *) ft;

		if (int_ft->is_signed) {
			BT_LOGE_STR("Invalid packet context field type: "
				"`packet_size` member is signed.");
			goto invalid;
		}

		has_total_size = true;
	}

	ft = ctf_field_type_struct_borrow_member_field_type_by_name(
		(void *) sc->packet_context_ft, "content_size");
	if (ft) {
		if (ft->id != CTF_FIELD_TYPE_ID_INT &&
				ft->id != CTF_FIELD_TYPE_ID_ENUM) {
			BT_LOGE_STR("Invalid packet context field type: "
				"`content_size` member is not an integer field type.");
			goto invalid;
		}

		int_ft = (void *) ft;

		if (int_ft->is_signed) {
			BT_LOGE_STR("Invalid packet context field type: "
				"`content_size` member is signed.");
			goto invalid;
		}

		has_content_size = true;
	}

	if (has_content_size && !has_total_size) {
			BT_LOGE_STR("Invalid packet context field type: "
				"`content_size` member exists without "
				"`packet_size` member.");
			goto invalid;
	}

	ft = ctf_field_type_struct_borrow_member_field_type_by_name(
		(void *) sc->event_header_ft, "id");
	if (ft) {
		if (ft->id != CTF_FIELD_TYPE_ID_INT &&
				ft->id != CTF_FIELD_TYPE_ID_ENUM) {
			BT_LOGE_STR("Invalid event header field type: "
				"`id` member is not an integer field type.");
			goto invalid;
		}

		int_ft = (void *) ft;

		if (int_ft->is_signed) {
			BT_LOGE_STR("Invalid event header field type: "
				"`id` member is signed.");
			goto invalid;
		}
	} else {
		if (sc->event_classes->len > 1) {
			BT_LOGE_STR("Invalid event header field type: "
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
	struct ctf_field_type_int *int_ft;
	uint64_t i;

	if (!ctf_tc->is_translated) {
		struct ctf_field_type *ft;

		ft = ctf_field_type_struct_borrow_member_field_type_by_name(
			(void *) ctf_tc->packet_header_ft, "magic");
		if (ft) {
			struct ctf_named_field_type *named_ft =
				ctf_field_type_struct_borrow_member_by_index(
					(void *) ctf_tc->packet_header_ft,
					0);

			if (named_ft->ft != ft) {
				BT_LOGE_STR("Invalid packet header field type: "
					"`magic` member is not the first member.");
				goto invalid;
			}

			if (ft->id != CTF_FIELD_TYPE_ID_INT &&
					ft->id != CTF_FIELD_TYPE_ID_ENUM) {
				BT_LOGE_STR("Invalid packet header field type: "
					"`magic` member is not an integer field type.");
				goto invalid;
			}

			int_ft = (void *) ft;

			if (int_ft->is_signed) {
				BT_LOGE_STR("Invalid packet header field type: "
					"`magic` member is signed.");
				goto invalid;
			}

			if (int_ft->base.size != 32) {
				BT_LOGE_STR("Invalid packet header field type: "
					"`magic` member is not 32-bit.");
				goto invalid;
			}
		}

		ft = ctf_field_type_struct_borrow_member_field_type_by_name(
			(void *) ctf_tc->packet_header_ft, "stream_id");
		if (ft) {
			if (ft->id != CTF_FIELD_TYPE_ID_INT &&
					ft->id != CTF_FIELD_TYPE_ID_ENUM) {
				BT_LOGE_STR("Invalid packet header field type: "
					"`stream_id` member is not an integer field type.");
				goto invalid;
			}

			int_ft = (void *) ft;

			if (int_ft->is_signed) {
				BT_LOGE_STR("Invalid packet header field type: "
					"`stream_id` member is signed.");
				goto invalid;
			}
		} else {
			if (ctf_tc->stream_classes->len > 1) {
				BT_LOGE_STR("Invalid packet header field type: "
					"missing `stream_id` member as there's "
					"more than one stream class.");
				goto invalid;
			}
		}

		ft = ctf_field_type_struct_borrow_member_field_type_by_name(
			(void *) ctf_tc->packet_header_ft,
			"stream_instance_id");
		if (ft) {
			if (ft->id != CTF_FIELD_TYPE_ID_INT &&
					ft->id != CTF_FIELD_TYPE_ID_ENUM) {
				BT_LOGE_STR("Invalid packet header field type: "
					"`stream_instance_id` member is not an integer field type.");
				goto invalid;
			}

			int_ft = (void *) ft;

			if (int_ft->is_signed) {
				BT_LOGE_STR("Invalid packet header field type: "
					"`stream_instance_id` member is signed.");
				goto invalid;
			}
		}

		ft = ctf_field_type_struct_borrow_member_field_type_by_name(
			(void *) ctf_tc->packet_header_ft, "uuid");
		if (ft) {
			struct ctf_field_type_array *array_ft = (void *) ft;

			if (ft->id != CTF_FIELD_TYPE_ID_ARRAY) {
				BT_LOGE_STR("Invalid packet header field type: "
					"`uuid` member is not an array field type.");
				goto invalid;
			}

			array_ft = (void *) ft;

			if (array_ft->length != 16) {
				BT_LOGE_STR("Invalid packet header field type: "
					"`uuid` member is not a 16-element array field type.");
				goto invalid;
			}

			if (array_ft->base.elem_ft->id != CTF_FIELD_TYPE_ID_INT) {
				BT_LOGE_STR("Invalid packet header field type: "
					"`uuid` member's element field type is not "
					"an integer field type.");
				goto invalid;
			}

			int_ft = (void *) array_ft->base.elem_ft;

			if (int_ft->is_signed) {
				BT_LOGE_STR("Invalid packet header field type: "
					"`uuid` member's element field type "
					"is a signed integer field type.");
				goto invalid;
			}

			if (int_ft->base.size != 8) {
				BT_LOGE_STR("Invalid packet header field type: "
					"`uuid` member's element field type "
					"is not an 8-bit integer field type.");
				goto invalid;
			}

			if (int_ft->base.base.alignment != 8) {
				BT_LOGE_STR("Invalid packet header field type: "
					"`uuid` member's element field type's "
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
