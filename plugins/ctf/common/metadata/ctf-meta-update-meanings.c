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

#define BT_LOG_TAG "PLUGIN-CTF-METADATA-META-UPDATE-MEANINGS"
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
int set_int_field_type_meaning_by_name(struct ctf_field_type *ft,
		const char *field_name, const char *id_name,
		enum ctf_field_type_meaning meaning)
{
	int ret = 0;
	uint64_t i;

	if (!ft) {
		goto end;
	}

	switch (ft->id) {
	case CTF_FIELD_TYPE_ID_INT:
	case CTF_FIELD_TYPE_ID_ENUM:
	{
		struct ctf_field_type_int *int_ft = (void *) ft;

		if (field_name && strcmp(field_name, id_name) == 0) {
			int_ft->meaning = meaning;
		}

		break;
	}
	case CTF_FIELD_TYPE_ID_STRUCT:
	{
		struct ctf_field_type_struct *struct_ft = (void *) ft;

		for (i = 0; i < struct_ft->members->len; i++) {
			struct ctf_named_field_type *named_ft =
				ctf_field_type_struct_borrow_member_by_index(
					struct_ft, i);

			ret = set_int_field_type_meaning_by_name(named_ft->ft,
				named_ft->name->str, id_name, meaning);
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

			ret = set_int_field_type_meaning_by_name(named_ft->ft,
				NULL, id_name, meaning);
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

		ret = set_int_field_type_meaning_by_name(array_ft->elem_ft,
			NULL, id_name, meaning);
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

static
int update_stream_class_meanings(struct ctf_stream_class *sc)
{
	int ret = 0;
	struct ctf_field_type_int *int_ft;
	uint64_t i;

	if (!sc->is_translated) {
		int_ft = ctf_field_type_struct_borrow_member_int_field_type_by_name(
			(void *) sc->packet_context_ft, "timestamp_begin");
		if (int_ft) {
			int_ft->meaning = CTF_FIELD_TYPE_MEANING_PACKET_BEGINNING_TIME;
		}

		int_ft = ctf_field_type_struct_borrow_member_int_field_type_by_name(
			(void *) sc->packet_context_ft, "timestamp_end");
		if (int_ft) {
			int_ft->meaning = CTF_FIELD_TYPE_MEANING_PACKET_END_TIME;

			/*
			 * Remove mapped clock class to avoid updating
			 * the clock immediately when decoding.
			 */
			int_ft->mapped_clock_class = NULL;
		}

		int_ft = ctf_field_type_struct_borrow_member_int_field_type_by_name(
			(void *) sc->packet_context_ft, "events_discarded");
		if (int_ft) {
			int_ft->meaning = CTF_FIELD_TYPE_MEANING_DISC_EV_REC_COUNTER_SNAPSHOT;
		}

		int_ft = ctf_field_type_struct_borrow_member_int_field_type_by_name(
			(void *) sc->packet_context_ft, "packet_seq_num");
		if (int_ft) {
			int_ft->meaning = CTF_FIELD_TYPE_MEANING_PACKET_COUNTER_SNAPSHOT;

		}

		int_ft = ctf_field_type_struct_borrow_member_int_field_type_by_name(
			(void *) sc->packet_context_ft, "packet_size");
		if (int_ft) {
			int_ft->meaning = CTF_FIELD_TYPE_MEANING_EXP_PACKET_TOTAL_SIZE;
		}

		int_ft = ctf_field_type_struct_borrow_member_int_field_type_by_name(
			(void *) sc->packet_context_ft, "content_size");
		if (int_ft) {
			int_ft->meaning = CTF_FIELD_TYPE_MEANING_EXP_PACKET_CONTENT_SIZE;
		}

		ret = set_int_field_type_meaning_by_name(
			sc->event_header_ft, NULL, "id",
			CTF_FIELD_TYPE_MEANING_EVENT_CLASS_ID);
		if (ret) {
			goto end;
		}
	}

	for (i = 0; i < sc->event_classes->len; i++) {
		struct ctf_event_class *ec = sc->event_classes->pdata[i];

		if (ec->is_translated) {
			continue;
		}
	}

end:
	return ret;
}

BT_HIDDEN
int ctf_trace_class_update_meanings(struct ctf_trace_class *ctf_tc)
{
	int ret = 0;
	struct ctf_field_type_int *int_ft;
	struct ctf_named_field_type *named_ft;
	uint64_t i;

	if (!ctf_tc->is_translated) {
		int_ft = ctf_field_type_struct_borrow_member_int_field_type_by_name(
			(void *) ctf_tc->packet_header_ft, "magic");
		if (int_ft) {
			int_ft->meaning = CTF_FIELD_TYPE_MEANING_MAGIC;
		}

		int_ft = ctf_field_type_struct_borrow_member_int_field_type_by_name(
			(void *) ctf_tc->packet_header_ft, "stream_id");
		if (int_ft) {
			int_ft->meaning = CTF_FIELD_TYPE_MEANING_STREAM_CLASS_ID;
		}

		int_ft = ctf_field_type_struct_borrow_member_int_field_type_by_name(
			(void *) ctf_tc->packet_header_ft,
			"stream_instance_id");
		if (int_ft) {
			int_ft->meaning = CTF_FIELD_TYPE_MEANING_DATA_STREAM_ID;
		}

		named_ft = ctf_field_type_struct_borrow_member_by_name(
			(void *) ctf_tc->packet_header_ft, "uuid");
		if (named_ft && named_ft->ft->id == CTF_FIELD_TYPE_ID_ARRAY) {
			struct ctf_field_type_array *array_ft =
				(void *) named_ft->ft;

			array_ft->meaning = CTF_FIELD_TYPE_MEANING_UUID;
		}
	}

	for (i = 0; i < ctf_tc->stream_classes->len; i++) {
		ret = update_stream_class_meanings(
			ctf_tc->stream_classes->pdata[i]);
		if (ret) {
			goto end;
		}
	}

end:
	return ret;
}
