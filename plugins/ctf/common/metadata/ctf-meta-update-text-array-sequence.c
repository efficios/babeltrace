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
int set_text_array_sequence_field_class(struct ctf_field_class *fc)
{
	int ret = 0;
	uint64_t i;

	if (!fc) {
		goto end;
	}

	switch (fc->id) {
	case CTF_FIELD_CLASS_ID_STRUCT:
	{
		struct ctf_field_class_struct *struct_fc = (void *) fc;

		for (i = 0; i < struct_fc->members->len; i++) {
			struct ctf_named_field_class *named_fc =
				ctf_field_class_struct_borrow_member_by_index(
					struct_fc, i);

			ret = set_text_array_sequence_field_class(named_fc->fc);
			if (ret) {
				goto end;
			}
		}

		break;
	}
	case CTF_FIELD_CLASS_ID_VARIANT:
	{
		struct ctf_field_class_variant *var_fc = (void *) fc;

		for (i = 0; i < var_fc->options->len; i++) {
			struct ctf_named_field_class *named_fc =
				ctf_field_class_variant_borrow_option_by_index(
					var_fc, i);

			ret = set_text_array_sequence_field_class(named_fc->fc);
			if (ret) {
				goto end;
			}
		}

		break;
	}
	case CTF_FIELD_CLASS_ID_ARRAY:
	case CTF_FIELD_CLASS_ID_SEQUENCE:
	{
		struct ctf_field_class_array_base *array_fc = (void *) fc;

		if (array_fc->elem_fc->id == CTF_FIELD_CLASS_ID_INT ||
				array_fc->elem_fc->id == CTF_FIELD_CLASS_ID_ENUM) {
			struct ctf_field_class_int *int_fc =
				(void *) array_fc->elem_fc;

			if (int_fc->base.base.alignment == 8 &&
					int_fc->base.size == 8 &&
					int_fc->encoding == CTF_ENCODING_UTF8) {
				array_fc->is_text = true;

				/*
				 * Force integer element to be unsigned;
				 * this makes the decoder enter a single
				 * path when reading a text
				 * array/sequence and we can safely
				 * decode bytes as characters anyway.
				 */
				int_fc->is_signed = false;
			}
		}

		ret = set_text_array_sequence_field_class(array_fc->elem_fc);
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
		ret = set_text_array_sequence_field_class(
			ctf_tc->packet_header_fc);
		if (ret) {
			goto end;
		}
	}

	for (i = 0; i < ctf_tc->stream_classes->len; i++) {
		struct ctf_stream_class *sc = ctf_tc->stream_classes->pdata[i];
		uint64_t j;

		if (!sc->is_translated) {
			ret = set_text_array_sequence_field_class(
				sc->packet_context_fc);
			if (ret) {
				goto end;
			}

			ret = set_text_array_sequence_field_class(
				sc->event_header_fc);
			if (ret) {
				goto end;
			}

			ret = set_text_array_sequence_field_class(
				sc->event_common_context_fc);
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

			ret = set_text_array_sequence_field_class(
				ec->spec_context_fc);
			if (ret) {
				goto end;
			}

			ret = set_text_array_sequence_field_class(
				ec->payload_fc);
			if (ret) {
				goto end;
			}
		}
	}

end:
	return ret;
}
