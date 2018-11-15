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
int set_int_field_class_meaning_by_name(struct ctf_field_class *fc,
		const char *field_name, const char *id_name,
		enum ctf_field_class_meaning meaning)
{
	int ret = 0;
	uint64_t i;

	if (!fc) {
		goto end;
	}

	switch (fc->id) {
	case CTF_FIELD_CLASS_ID_INT:
	case CTF_FIELD_CLASS_ID_ENUM:
	{
		struct ctf_field_class_int *int_fc = (void *) fc;

		if (field_name && strcmp(field_name, id_name) == 0) {
			int_fc->meaning = meaning;
		}

		break;
	}
	case CTF_FIELD_CLASS_ID_STRUCT:
	{
		struct ctf_field_class_struct *struct_fc = (void *) fc;

		for (i = 0; i < struct_fc->members->len; i++) {
			struct ctf_named_field_class *named_fc =
				ctf_field_class_struct_borrow_member_by_index(
					struct_fc, i);

			ret = set_int_field_class_meaning_by_name(named_fc->fc,
				named_fc->name->str, id_name, meaning);
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

			ret = set_int_field_class_meaning_by_name(named_fc->fc,
				NULL, id_name, meaning);
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

		ret = set_int_field_class_meaning_by_name(array_fc->elem_fc,
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
	struct ctf_field_class_int *int_fc;
	uint64_t i;

	if (!sc->is_translated) {
		int_fc = ctf_field_class_struct_borrow_member_int_field_class_by_name(
			(void *) sc->packet_context_fc, "timestamp_begin");
		if (int_fc) {
			int_fc->meaning = CTF_FIELD_CLASS_MEANING_PACKET_BEGINNING_TIME;
		}

		int_fc = ctf_field_class_struct_borrow_member_int_field_class_by_name(
			(void *) sc->packet_context_fc, "timestamp_end");
		if (int_fc) {
			int_fc->meaning = CTF_FIELD_CLASS_MEANING_PACKET_END_TIME;

			/*
			 * Remove mapped clock class to avoid updating
			 * the clock immediately when decoding.
			 */
			int_fc->mapped_clock_class = NULL;
		}

		int_fc = ctf_field_class_struct_borrow_member_int_field_class_by_name(
			(void *) sc->packet_context_fc, "events_discarded");
		if (int_fc) {
			int_fc->meaning = CTF_FIELD_CLASS_MEANING_DISC_EV_REC_COUNTER_SNAPSHOT;
		}

		int_fc = ctf_field_class_struct_borrow_member_int_field_class_by_name(
			(void *) sc->packet_context_fc, "packet_seq_num");
		if (int_fc) {
			int_fc->meaning = CTF_FIELD_CLASS_MEANING_PACKET_COUNTER_SNAPSHOT;

		}

		int_fc = ctf_field_class_struct_borrow_member_int_field_class_by_name(
			(void *) sc->packet_context_fc, "packet_size");
		if (int_fc) {
			int_fc->meaning = CTF_FIELD_CLASS_MEANING_EXP_PACKET_TOTAL_SIZE;
		}

		int_fc = ctf_field_class_struct_borrow_member_int_field_class_by_name(
			(void *) sc->packet_context_fc, "content_size");
		if (int_fc) {
			int_fc->meaning = CTF_FIELD_CLASS_MEANING_EXP_PACKET_CONTENT_SIZE;
		}

		ret = set_int_field_class_meaning_by_name(
			sc->event_header_fc, NULL, "id",
			CTF_FIELD_CLASS_MEANING_EVENT_CLASS_ID);
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
	struct ctf_field_class_int *int_fc;
	struct ctf_named_field_class *named_fc;
	uint64_t i;

	if (!ctf_tc->is_translated) {
		int_fc = ctf_field_class_struct_borrow_member_int_field_class_by_name(
			(void *) ctf_tc->packet_header_fc, "magic");
		if (int_fc) {
			int_fc->meaning = CTF_FIELD_CLASS_MEANING_MAGIC;
		}

		int_fc = ctf_field_class_struct_borrow_member_int_field_class_by_name(
			(void *) ctf_tc->packet_header_fc, "stream_id");
		if (int_fc) {
			int_fc->meaning = CTF_FIELD_CLASS_MEANING_STREAM_CLASS_ID;
		}

		int_fc = ctf_field_class_struct_borrow_member_int_field_class_by_name(
			(void *) ctf_tc->packet_header_fc,
			"stream_instance_id");
		if (int_fc) {
			int_fc->meaning = CTF_FIELD_CLASS_MEANING_DATA_STREAM_ID;
		}

		named_fc = ctf_field_class_struct_borrow_member_by_name(
			(void *) ctf_tc->packet_header_fc, "uuid");
		if (named_fc && named_fc->fc->id == CTF_FIELD_CLASS_ID_ARRAY) {
			struct ctf_field_class_array *array_fc =
				(void *) named_fc->fc;

			array_fc->meaning = CTF_FIELD_CLASS_MEANING_UUID;
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
