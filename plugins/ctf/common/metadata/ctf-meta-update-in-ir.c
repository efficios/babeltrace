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

#define BT_LOG_TAG "PLUGIN-CTF-METADATA-META-UPDATE-IN-IR"
#include "logging.h"

#include <babeltrace/babeltrace.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/assert-internal.h>
#include <babeltrace/compat/glib-internal.h>
#include <glib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>

#include "ctf-meta-visitors.h"

static
void update_field_class_in_ir(struct ctf_field_class *fc,
		GHashTable *ft_dependents)
{
	int64_t i;

	if (!fc) {
		goto end;
	}

	switch (fc->id) {
	case CTF_FIELD_CLASS_ID_INT:
	case CTF_FIELD_CLASS_ID_ENUM:
	{
		struct ctf_field_class_int *int_fc = (void *) fc;

		if (int_fc->mapped_clock_class ||
				int_fc->meaning == CTF_FIELD_CLASS_MEANING_NONE ||
				bt_g_hash_table_contains(ft_dependents, fc)) {
			/*
			 * Field class does not update a clock, has no
			 * special meaning, and no sequence/variant
			 * field class which is part of IR depends on it.
			 */
			fc->in_ir = true;
		}

		break;
	}
	case CTF_FIELD_CLASS_ID_STRUCT:
	{
		struct ctf_field_class_struct *struct_fc = (void *) fc;

		/* Reverse order */
		for (i = (int64_t) struct_fc->members->len - 1; i >= 0; i--) {
			struct ctf_named_field_class *named_fc =
				ctf_field_class_struct_borrow_member_by_index(
					struct_fc, i);

			update_field_class_in_ir(named_fc->fc, ft_dependents);

			if (named_fc->fc->in_ir) {
				/* At least one member is part of IR */
				fc->in_ir = true;
			}
		}

		break;
	}
	case CTF_FIELD_CLASS_ID_VARIANT:
	{
		struct ctf_named_field_class *named_fc;
		struct ctf_field_class_variant *var_fc = (void *) fc;

		/*
		 * Reverse order, although it is not important for this
		 * loop because a field class within a variant field
		 * type's option cannot depend on a field class in
		 * another option of the same variant field class.
		 */
		for (i = (int64_t) var_fc->options->len - 1; i >= 0; i--) {
			named_fc =
				ctf_field_class_variant_borrow_option_by_index(
					var_fc, i);

			update_field_class_in_ir(named_fc->fc, ft_dependents);

			if (named_fc->fc->in_ir) {
				/* At least one option is part of IR */
				fc->in_ir = true;
			}
		}

		if (fc->in_ir) {
			/*
			 * At least one option will make it to IR. In
			 * this case, make all options part of IR
			 * because the variant's tag could still select
			 * (dynamically) a removed option. This can mean
			 * having an empty structure as an option, for
			 * example, but at least all the options are
			 * selectable.
			 */
			for (i = 0; i < var_fc->options->len; i++) {
				ctf_field_class_variant_borrow_option_by_index(
					var_fc, i)->fc->in_ir = true;
			}

			/*
			 * This variant field class is part of IR and
			 * depends on a tag field class (which must also
			 * be part of IR).
			 */
			g_hash_table_insert(ft_dependents, var_fc->tag_fc,
				var_fc->tag_fc);
		}

		break;
	}
	case CTF_FIELD_CLASS_ID_ARRAY:
	case CTF_FIELD_CLASS_ID_SEQUENCE:
	{
		struct ctf_field_class_array_base *array_fc = (void *) fc;

		update_field_class_in_ir(array_fc->elem_fc, ft_dependents);
		fc->in_ir = array_fc->elem_fc->in_ir;

		if (fc->id == CTF_FIELD_CLASS_ID_ARRAY) {
			struct ctf_field_class_array *arr_fc = (void *) fc;

			assert(arr_fc->meaning == CTF_FIELD_CLASS_MEANING_NONE ||
				arr_fc->meaning == CTF_FIELD_CLASS_MEANING_UUID);

			/*
			 * UUID field class: nothing depends on this, so
			 * it's not part of IR.
			 */
			if (arr_fc->meaning == CTF_FIELD_CLASS_MEANING_UUID) {
				fc->in_ir = false;
				array_fc->elem_fc->in_ir = false;
			}
		} else if (fc->id == CTF_FIELD_CLASS_ID_SEQUENCE) {
			if (fc->in_ir) {
				struct ctf_field_class_sequence *seq_fc = (void *) fc;

				/*
				 * This sequence field class is part of
				 * IR and depends on a length field class
				 * (which must also be part of IR).
				 */
				g_hash_table_insert(ft_dependents,
					seq_fc->length_fc, seq_fc->length_fc);
			}
		}

		break;
	}
	default:
		fc->in_ir = true;
		break;
	}

end:
	return;
}

/*
 * Scopes and field classes are processed in reverse order because we need
 * to know if a given integer field class has dependents (sequence or
 * variant field classes) when we reach it. Dependents can only be located
 * after the length/tag field class in the metadata tree.
 */
BT_HIDDEN
int ctf_trace_class_update_in_ir(struct ctf_trace_class *ctf_tc)
{
	int ret = 0;
	uint64_t i;

	GHashTable *ft_dependents = g_hash_table_new(g_direct_hash,
		g_direct_equal);

	BT_ASSERT(ft_dependents);

	for (i = 0; i < ctf_tc->stream_classes->len; i++) {
		struct ctf_stream_class *sc = ctf_tc->stream_classes->pdata[i];
		uint64_t j;

		for (j = 0; j < sc->event_classes->len; j++) {
			struct ctf_event_class *ec = sc->event_classes->pdata[j];

			if (ec->is_translated) {
				continue;
			}

			update_field_class_in_ir(ec->payload_fc, ft_dependents);
			update_field_class_in_ir(ec->spec_context_fc,
				ft_dependents);
		}

		if (!sc->is_translated) {
			update_field_class_in_ir(sc->event_common_context_fc,
				ft_dependents);
			update_field_class_in_ir(sc->event_header_fc,
				ft_dependents);
			update_field_class_in_ir(sc->packet_context_fc,
				ft_dependents);
		}
	}

	if (!ctf_tc->is_translated) {
		update_field_class_in_ir(ctf_tc->packet_header_fc,
			ft_dependents);
	}

	g_hash_table_destroy(ft_dependents);
	return ret;
}
