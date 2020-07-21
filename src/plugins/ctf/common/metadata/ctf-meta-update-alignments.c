/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2020 Philippe Proulx <pproulx@efficios.com>
 */

#include <babeltrace2/babeltrace.h>
#include "common/macros.h"
#include "common/assert.h"
#include <glib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#include "ctf-meta-visitors.h"

static inline
int set_alignments(struct ctf_field_class *fc)
{
	int ret = 0;
	uint64_t i;

	if (!fc) {
		goto end;
	}

	switch (fc->type) {
	case CTF_FIELD_CLASS_TYPE_STRUCT:
	{
		struct ctf_field_class_struct *struct_fc = (void *) fc;

		for (i = 0; i < struct_fc->members->len; i++) {
			struct ctf_named_field_class *named_fc =
				ctf_field_class_struct_borrow_member_by_index(
					struct_fc, i);

			ret = set_alignments(named_fc->fc);
			if (ret) {
				goto end;
			}

			if (named_fc->fc->alignment > fc->alignment) {
				fc->alignment = named_fc->fc->alignment;
			}
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

			ret = set_alignments(named_fc->fc);
			if (ret) {
				goto end;
			}
		}

		break;
	}
	case CTF_FIELD_CLASS_TYPE_ARRAY:
	case CTF_FIELD_CLASS_TYPE_SEQUENCE:
	{
		struct ctf_field_class_array_base *array_fc = (void *) fc;

		ret = set_alignments(array_fc->elem_fc);
		if (ret) {
			goto end;
		}

		/*
		 * Use the alignment of the array/sequence field class's
		 * element FC as its own alignment.
		 *
		 * This is especially important when the array/sequence
		 * field's effective length is zero: as per CTF 1.8, the
		 * stream data decoding process still needs to align the
		 * cursor using the element's alignment [1]:
		 *
		 * > Arrays are always aligned on their element
		 * > alignment requirement.
		 *
		 * For example:
		 *
		 *     struct {
		 *         integer { size = 8; } a;
		 *         integer { size = 8; align = 16; } b[0];
		 *         integer { size = 8; } c;
		 *     };
		 *
		 * When using this to decode the bytes 1, 2, and 3, then
		 * the decoded values are:
		 *
		 * `a`: 1
		 * `b`: []
		 * `c`: 3
		 *
		 * [1]: https://diamon.org/ctf/#spec4.2.3
		 */
		array_fc->base.alignment = array_fc->elem_fc->alignment;
		break;
	}
	default:
		break;
	}

end:
	return ret;
}

BT_HIDDEN
int ctf_trace_class_update_alignments(
		struct ctf_trace_class *ctf_tc)
{
	int ret = 0;
	uint64_t i;

	if (!ctf_tc->is_translated) {
		ret = set_alignments(ctf_tc->packet_header_fc);
		if (ret) {
			goto end;
		}
	}

	for (i = 0; i < ctf_tc->stream_classes->len; i++) {
		struct ctf_stream_class *sc = ctf_tc->stream_classes->pdata[i];
		uint64_t j;

		if (!sc->is_translated) {
			ret = set_alignments(sc->packet_context_fc);
			if (ret) {
				goto end;
			}

			ret = set_alignments(sc->event_header_fc);
			if (ret) {
				goto end;
			}

			ret = set_alignments(sc->event_common_context_fc);
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

			ret = set_alignments(ec->spec_context_fc);
			if (ret) {
				goto end;
			}

			ret = set_alignments(ec->payload_fc);
			if (ret) {
				goto end;
			}
		}
	}

end:
	return ret;
}
