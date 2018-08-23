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

#define BT_LOG_TAG "PLUGIN-CTF-METADATA-META-UPDATE-VALUE-STORING-INDEXES"
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
int update_field_type_stored_value_index(struct ctf_field_type *ft,
		struct ctf_trace_class *tc,
		struct ctf_stream_class *sc,
		struct ctf_event_class *ec)
{
	int ret = 0;
	uint64_t i;
	struct ctf_field_path *field_path = NULL;
	struct ctf_field_type_int *tgt_ft = NULL;
	uint64_t *stored_value_index = NULL;

	if (!ft) {
		goto end;
	}

	switch (ft->id) {
	case CTF_FIELD_TYPE_ID_VARIANT:
	{
		struct ctf_field_type_variant *var_ft = (void *) ft;

		field_path = &var_ft->tag_path;
		stored_value_index = &var_ft->stored_tag_index;
		tgt_ft = (void *) var_ft->tag_ft;
		break;
	}
	case CTF_FIELD_TYPE_ID_SEQUENCE:
	{
		struct ctf_field_type_sequence *seq_ft = (void *) ft;

		field_path = &seq_ft->length_path;
		stored_value_index = &seq_ft->stored_length_index;
		tgt_ft = seq_ft->length_ft;
		break;
	}
	default:
		break;
	}

	if (field_path) {
		BT_ASSERT(tgt_ft);
		BT_ASSERT(tgt_ft->base.base.id == CTF_FIELD_TYPE_ID_INT ||
			tgt_ft->base.base.id == CTF_FIELD_TYPE_ID_ENUM);
		if (tgt_ft->storing_index >= 0) {
			/* Already storing its value */
			*stored_value_index = (uint64_t) tgt_ft->storing_index;
		} else {
			/* Not storing its value: allocate new index */
			tgt_ft->storing_index = tc->stored_value_count;
			*stored_value_index = (uint64_t) tgt_ft->storing_index;
			tc->stored_value_count++;
		}
	}

	switch (ft->id) {
	case CTF_FIELD_TYPE_ID_STRUCT:
	{
		struct ctf_field_type_struct *struct_ft = (void *) ft;

		for (i = 0; i < struct_ft->members->len; i++) {
			struct ctf_named_field_type *named_ft =
				ctf_field_type_struct_borrow_member_by_index(
					struct_ft, i);

			ret = update_field_type_stored_value_index(named_ft->ft,
				tc, sc, ec);
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

			ret = update_field_type_stored_value_index(named_ft->ft,
				tc, sc, ec);
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

		ret = update_field_type_stored_value_index(array_ft->elem_ft,
			tc, sc, ec);
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
int ctf_trace_class_update_value_storing_indexes(struct ctf_trace_class *ctf_tc)
{
	uint64_t i;

	if (!ctf_tc->is_translated) {
		update_field_type_stored_value_index(
			ctf_tc->packet_header_ft, ctf_tc, NULL, NULL);
	}

	for (i = 0; i < ctf_tc->stream_classes->len; i++) {
		uint64_t j;
		struct ctf_stream_class *sc = ctf_tc->stream_classes->pdata[i];

		if (!sc->is_translated) {
			update_field_type_stored_value_index(sc->packet_context_ft,
				ctf_tc, sc, NULL);
			update_field_type_stored_value_index(sc->event_header_ft,
				ctf_tc, sc, NULL);
			update_field_type_stored_value_index(
				sc->event_common_context_ft, ctf_tc, sc, NULL);
		}

		for (j = 0; j < sc->event_classes->len; j++) {
			struct ctf_event_class *ec =
				sc->event_classes->pdata[j];

			if (!ec->is_translated) {
				update_field_type_stored_value_index(
					ec->spec_context_ft, ctf_tc, sc, ec);
				update_field_type_stored_value_index(
					ec->payload_ft, ctf_tc, sc, ec);
			}
		}
	}

	return 0;
}
