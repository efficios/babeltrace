/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2018 Philippe Proulx <pproulx@efficios.com>
 */

#define BT_COMP_LOG_SELF_COMP (log_cfg->self_comp)
#define BT_COMP_LOG_SELF_COMP_CLASS (log_cfg->self_comp_class)
#define BT_LOG_OUTPUT_LEVEL (log_cfg->log_level)
#define BT_LOG_TAG "PLUGIN/CTF/META/UPDATE-DEF-CC"
#include "logging/comp-logging.h"

#include <babeltrace2/babeltrace.h>
#include "common/macros.h"
#include "common/assert.h"
#include <glib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#include "ctf-meta-visitors.hpp"
#include "logging.hpp"

static inline
int find_mapped_clock_class(struct ctf_field_class *fc,
		struct ctf_clock_class **clock_class,
		struct meta_log_config *log_cfg)
{
	int ret = 0;
	uint64_t i;

	if (!fc) {
		goto end;
	}

	switch (fc->type) {
	case CTF_FIELD_CLASS_TYPE_INT:
	case CTF_FIELD_CLASS_TYPE_ENUM:
	{
		struct ctf_field_class_int *int_fc = ctf_field_class_as_int(fc);

		if (int_fc->mapped_clock_class) {
			if (*clock_class && *clock_class !=
					int_fc->mapped_clock_class) {
				_BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE("Stream class contains more than one "
					"clock class: expected-cc-name=\"%s\", "
					"other-cc-name=\"%s\"",
					(*clock_class)->name->str,
					int_fc->mapped_clock_class->name->str);
				ret = -1;
				goto end;
			}

			*clock_class = int_fc->mapped_clock_class;
		}

		break;
	}
	case CTF_FIELD_CLASS_TYPE_STRUCT:
	{
		struct ctf_field_class_struct *struct_fc = ctf_field_class_as_struct(fc);

		for (i = 0; i < struct_fc->members->len; i++) {
			struct ctf_named_field_class *named_fc =
				ctf_field_class_struct_borrow_member_by_index(
					struct_fc, i);

			ret = find_mapped_clock_class(named_fc->fc,
				clock_class, log_cfg);
			if (ret) {
				goto end;
			}
		}

		break;
	}
	case CTF_FIELD_CLASS_TYPE_VARIANT:
	{
		struct ctf_field_class_variant *var_fc = ctf_field_class_as_variant(fc);

		for (i = 0; i < var_fc->options->len; i++) {
			struct ctf_named_field_class *named_fc =
				ctf_field_class_variant_borrow_option_by_index(
					var_fc, i);

			ret = find_mapped_clock_class(named_fc->fc,
				clock_class, log_cfg);
			if (ret) {
				goto end;
			}
		}

		break;
	}
	case CTF_FIELD_CLASS_TYPE_ARRAY:
	case CTF_FIELD_CLASS_TYPE_SEQUENCE:
	{
		struct ctf_field_class_array_base *array_fc = ctf_field_class_as_array_base(fc);

		ret = find_mapped_clock_class(array_fc->elem_fc, clock_class,
			log_cfg);
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
		struct ctf_stream_class *stream_class,
		struct meta_log_config *log_cfg)
{
	int ret = 0;
	struct ctf_clock_class *clock_class =
		stream_class->default_clock_class;
	uint64_t i;

	ret = find_mapped_clock_class(stream_class->packet_context_fc,
		&clock_class, log_cfg);
	if (ret) {
		goto end;
	}

	ret = find_mapped_clock_class(stream_class->event_header_fc,
		&clock_class, log_cfg);
	if (ret) {
		goto end;
	}

	ret = find_mapped_clock_class(stream_class->event_common_context_fc,
		&clock_class, log_cfg);
	if (ret) {
		goto end;
	}

	for (i = 0; i < stream_class->event_classes->len; i++) {
		struct ctf_event_class *event_class =
			(ctf_event_class *) stream_class->event_classes->pdata[i];

		ret = find_mapped_clock_class(event_class->spec_context_fc,
			&clock_class, log_cfg);
		if (ret) {
			goto end;
		}

		ret = find_mapped_clock_class(event_class->payload_fc,
			&clock_class, log_cfg);
		if (ret) {
			goto end;
		}
	}

	if (!stream_class->default_clock_class) {
		stream_class->default_clock_class = clock_class;
	}

end:
	return ret;
}

BT_HIDDEN
int ctf_trace_class_update_default_clock_classes(struct ctf_trace_class *ctf_tc,
		struct meta_log_config *log_cfg)
{
	uint64_t i;
	int ret = 0;
	struct ctf_clock_class *clock_class = NULL;

	ret = find_mapped_clock_class(ctf_tc->packet_header_fc, &clock_class,
		log_cfg);
	if (ret) {
		goto end;
	}

	if (clock_class) {
		ret = -1;
		goto end;
	}

	for (i = 0; i < ctf_tc->stream_classes->len; i++) {
		struct ctf_stream_class *sc =
			(ctf_stream_class *) ctf_tc->stream_classes->pdata[i];

		ret = update_stream_class_default_clock_class(
			(ctf_stream_class *) ctf_tc->stream_classes->pdata[i], log_cfg);
		if (ret) {
			_BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE("Stream class contains more than one "
				"clock class: stream-class-id=%" PRIu64,
				sc->id);
			goto end;
		}
	}

end:
	return ret;
}
