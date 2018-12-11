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

#define BT_LOG_TAG "PLUGIN-CTF-METADATA-META-WARN-MEANINGLESS-HEADER-FIELDS"
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
void warn_meaningless_field(const char *name, const char *scope_name)
{
	BT_LOGW("User field found in %s: ignoring: name=\"%s\"",
		scope_name, name);
}

static inline
void warn_meaningless_fields(struct ctf_field_class *fc, const char *name,
		const char *scope_name)
{
	uint64_t i;

	if (!fc) {
		goto end;
	}

	switch (fc->type) {
	case CTF_FIELD_CLASS_TYPE_FLOAT:
	case CTF_FIELD_CLASS_TYPE_STRING:
		warn_meaningless_field(name, scope_name);
		break;
	case CTF_FIELD_CLASS_TYPE_INT:
	case CTF_FIELD_CLASS_TYPE_ENUM:
	{
		struct ctf_field_class_int *int_fc = (void *) fc;

		if (int_fc->meaning == CTF_FIELD_CLASS_MEANING_NONE &&
				!int_fc->mapped_clock_class) {
			warn_meaningless_field(name, scope_name);
		}

		break;
	}
	case CTF_FIELD_CLASS_TYPE_STRUCT:
	{
		struct ctf_field_class_struct *struct_fc = (void *) fc;

		for (i = 0; i < struct_fc->members->len; i++) {
			struct ctf_named_field_class *named_fc =
				ctf_field_class_struct_borrow_member_by_index(
					struct_fc, i);

			warn_meaningless_fields(named_fc->fc,
				named_fc->name->str, scope_name);
		}

		break;
	}
	case CTF_FIELD_CLASS_TYPE_VARIANT:
	{
		struct ctf_field_class_variant *var_fc = (void *) fc;

		for (i = 0; i < var_fc->options->len; i++) {
			struct ctf_named_field_class *named_fc =
				ctf_field_class_variant_borrow_option_by_index(
					var_fc, i);

			warn_meaningless_fields(named_fc->fc,
				named_fc->name->str, scope_name);
		}

		break;
	}
	case CTF_FIELD_CLASS_TYPE_ARRAY:
	{
		struct ctf_field_class_array *array_fc = (void *) fc;

		if (array_fc->meaning != CTF_FIELD_CLASS_MEANING_NONE) {
			goto end;
		}

		/* Fallthrough */
	}
	case CTF_FIELD_CLASS_TYPE_SEQUENCE:
	{
		struct ctf_field_class_array_base *array_fc = (void *) fc;

		warn_meaningless_fields(array_fc->elem_fc, name, scope_name);
		break;
	}
	default:
		abort();
	}

end:
	return;
}

BT_HIDDEN
void ctf_trace_class_warn_meaningless_header_fields(
		struct ctf_trace_class *ctf_tc)
{
	uint64_t i;

	if (!ctf_tc->is_translated) {
		warn_meaningless_fields(
			ctf_tc->packet_header_fc, NULL, "packet header");
	}

	for (i = 0; i < ctf_tc->stream_classes->len; i++) {
		struct ctf_stream_class *sc = ctf_tc->stream_classes->pdata[i];

		if (!sc->is_translated) {
			warn_meaningless_fields(sc->event_header_fc, NULL,
				"event header");
		}
	}
}
