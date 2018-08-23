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

#define BT_LOG_TAG "PLUGIN-CTF-METADATA-META-UPDATE-TEXT-ARRAY-SEQ"
#include "logging.h"

#include <babeltrace/babeltrace.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/assert-internal.h>
#include <glib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#include "ctf-meta-visitors.h"

static inline
int set_text_array_sequence_field_type(struct ctf_field_type *ft)
{
	int ret = 0;
	uint64_t i;

	if (!ft) {
		goto end;
	}

	switch (ft->id) {
	case CTF_FIELD_TYPE_ID_STRUCT:
	{
		struct ctf_field_type_struct *struct_ft = (void *) ft;

		for (i = 0; i < struct_ft->members->len; i++) {
			struct ctf_named_field_type *named_ft =
				ctf_field_type_struct_borrow_member_by_index(
					struct_ft, i);

			ret = set_text_array_sequence_field_type(named_ft->ft);
			if (ret) {
				goto end;
			}
		}

		break;
	}
	case CTF_FIELD_TYPE_ID_VARIANT:
	{
		struct ctf_field_type_variant *var_ft = (void *) ft;

		for (i = 0; i < var_ft->options->len; i++) {
			struct ctf_named_field_type *named_ft =
				ctf_field_type_variant_borrow_option_by_index(
					var_ft, i);

			ret = set_text_array_sequence_field_type(named_ft->ft);
			if (ret) {
				goto end;
			}
		}

		break;
	}
	case CTF_FIELD_TYPE_ID_ARRAY:
	case CTF_FIELD_TYPE_ID_SEQUENCE:
	{
		struct ctf_field_type_array_base *array_ft = (void *) ft;

		if (array_ft->elem_ft->id == CTF_FIELD_TYPE_ID_INT ||
				array_ft->elem_ft->id == CTF_FIELD_TYPE_ID_ENUM) {
			struct ctf_field_type_int *int_ft =
				(void *) array_ft->elem_ft;

			if (int_ft->base.base.alignment == 8 &&
					int_ft->base.size == 8 &&
					int_ft->encoding == CTF_ENCODING_UTF8) {
				array_ft->is_text = true;

				/*
				 * Force integer element to be unsigned;
				 * this makes the decoder enter a single
				 * path when reading a text
				 * array/sequence and we can safely
				 * decode bytes as characters anyway.
				 */
				int_ft->is_signed = false;
			}
		}

		ret = set_text_array_sequence_field_type(array_ft->elem_ft);
		if (ret) {
			goto end;
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
int ctf_trace_class_update_text_array_sequence(struct ctf_trace_class *ctf_tc)
{
	int ret = 0;
	uint64_t i;

	if (!ctf_tc->is_translated) {
		ret = set_text_array_sequence_field_type(
			ctf_tc->packet_header_ft);
		if (ret) {
			goto end;
		}
	}

	for (i = 0; i < ctf_tc->stream_classes->len; i++) {
		struct ctf_stream_class *sc = ctf_tc->stream_classes->pdata[i];
		uint64_t j;

		if (!sc->is_translated) {
			ret = set_text_array_sequence_field_type(
				sc->packet_context_ft);
			if (ret) {
				goto end;
			}

			ret = set_text_array_sequence_field_type(
				sc->event_header_ft);
			if (ret) {
				goto end;
			}

			ret = set_text_array_sequence_field_type(
				sc->event_common_context_ft);
			if (ret) {
				goto end;
			}
		}

		for (j = 0; j < sc->event_classes->len; j++) {
			struct ctf_event_class *ec =
				sc->event_classes->pdata[j];

			if (ec->is_translated) {
				continue;
			}

			ret = set_text_array_sequence_field_type(
				ec->spec_context_ft);
			if (ret) {
				goto end;
			}

			ret = set_text_array_sequence_field_type(
				ec->payload_ft);
			if (ret) {
				goto end;
			}
		}
	}

end:
	return ret;
}
