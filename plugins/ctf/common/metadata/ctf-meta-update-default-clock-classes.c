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

#define BT_LOG_TAG "PLUGIN-CTF-METADATA-META-UPDATE-DEF-CC"
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
int find_mapped_clock_class(struct ctf_field_type *ft,
		struct bt_clock_class **clock_class)
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

		if (int_ft->mapped_clock_class) {
			if (*clock_class && *clock_class !=
					int_ft->mapped_clock_class) {
				BT_LOGE("Stream class contains more than one "
					"clock class: expected-cc-name=\"%s\", "
					"other-cc-name=\"%s\"",
					bt_clock_class_get_name(*clock_class),
					bt_clock_class_get_name(int_ft->mapped_clock_class));
				ret = -1;
				goto end;
			}

			*clock_class = int_ft->mapped_clock_class;
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

			ret = find_mapped_clock_class(named_ft->ft,
				clock_class);
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

			ret = find_mapped_clock_class(named_ft->ft,
				clock_class);
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

		ret = find_mapped_clock_class(array_ft->elem_ft, clock_class);
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

static inline
int update_stream_class_default_clock_class(
		struct ctf_stream_class *stream_class)
{
	int ret = 0;
	struct bt_clock_class *clock_class = stream_class->default_clock_class;
	uint64_t i;

	ret = find_mapped_clock_class(stream_class->packet_context_ft,
		&clock_class);
	if (ret) {
		goto end;
	}

	ret = find_mapped_clock_class(stream_class->event_header_ft,
		&clock_class);
	if (ret) {
		goto end;
	}

	ret = find_mapped_clock_class(stream_class->event_common_context_ft,
		&clock_class);
	if (ret) {
		goto end;
	}

	for (i = 0; i < stream_class->event_classes->len; i++) {
		struct ctf_event_class *event_class =
			stream_class->event_classes->pdata[i];

		ret = find_mapped_clock_class(event_class->spec_context_ft,
			&clock_class);
		if (ret) {
			goto end;
		}

		ret = find_mapped_clock_class(event_class->payload_ft,
			&clock_class);
		if (ret) {
			goto end;
		}
	}

	if (!stream_class->default_clock_class) {
		stream_class->default_clock_class = bt_get(clock_class);
	}

end:
	return ret;
}

BT_HIDDEN
int ctf_trace_class_update_default_clock_classes(struct ctf_trace_class *ctf_tc)
{
	uint64_t i;
	int ret = 0;
	struct bt_clock_class *clock_class = NULL;

	ret = find_mapped_clock_class(ctf_tc->packet_header_ft,
		&clock_class);
	if (ret) {
		goto end;
	}

	if (clock_class) {
		ret = -1;
		goto end;
	}

	for (i = 0; i < ctf_tc->stream_classes->len; i++) {
		struct ctf_stream_class *sc =
			ctf_tc->stream_classes->pdata[i];

		ret = update_stream_class_default_clock_class(
			ctf_tc->stream_classes->pdata[i]);
		if (ret) {
			BT_LOGE("Stream class contains more than one "
				"clock class: stream-class-id=%" PRIu64,
				sc->id);
			goto end;
		}
	}

end:
	return ret;
}
