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
void update_field_type_in_ir(struct ctf_field_type *ft,
		GHashTable *ft_dependents)
{
	int64_t i;

	if (!ft) {
		goto end;
	}

	switch (ft->id) {
	case CTF_FIELD_TYPE_ID_INT:
	case CTF_FIELD_TYPE_ID_ENUM:
	{
		struct ctf_field_type_int *int_ft = (void *) ft;

		if (int_ft->mapped_clock_class ||
				int_ft->meaning == CTF_FIELD_TYPE_MEANING_NONE ||
				bt_g_hash_table_contains(ft_dependents, ft)) {
			/*
			 * Field type does not update a clock, has no
			 * special meaning, and no sequence/variant
			 * field type which is part of IR depends on it.
			 */
			ft->in_ir = true;
		}

		break;
	}
	case CTF_FIELD_TYPE_ID_STRUCT:
	{
		struct ctf_field_type_struct *struct_ft = (void *) ft;

		/* Reverse order */
		for (i = (int64_t) struct_ft->members->len - 1; i >= 0; i--) {
			struct ctf_named_field_type *named_ft =
				ctf_field_type_struct_borrow_member_by_index(
					struct_ft, i);

			update_field_type_in_ir(named_ft->ft, ft_dependents);

			if (named_ft->ft->in_ir) {
				/* At least one member is part of IR */
				ft->in_ir = true;
			}
		}

		break;
	}
	case CTF_FIELD_TYPE_ID_VARIANT:
	{
		struct ctf_named_field_type *named_ft;
		struct ctf_field_type_variant *var_ft = (void *) ft;

		/*
		 * Reverse order, although it is not important for this
		 * loop because a field type within a variant field
		 * type's option cannot depend on a field type in
		 * another option of the same variant field type.
		 */
		for (i = (int64_t) var_ft->options->len - 1; i >= 0; i--) {
			named_ft =
				ctf_field_type_variant_borrow_option_by_index(
					var_ft, i);

			update_field_type_in_ir(named_ft->ft, ft_dependents);

			if (named_ft->ft->in_ir) {
				/* At least one option is part of IR */
				ft->in_ir = true;
			}
		}

		if (ft->in_ir) {
			/*
			 * At least one option will make it to IR. In
			 * this case, make all options part of IR
			 * because the variant's tag could still select
			 * (dynamically) a removed option. This can mean
			 * having an empty structure as an option, for
			 * example, but at least all the options are
			 * selectable.
			 */
			for (i = 0; i < var_ft->options->len; i++) {
				ctf_field_type_variant_borrow_option_by_index(
					var_ft, i)->ft->in_ir = true;
			}

			/*
			 * This variant field type is part of IR and
			 * depends on a tag field type (which must also
			 * be part of IR).
			 */
			g_hash_table_insert(ft_dependents, var_ft->tag_ft,
				var_ft->tag_ft);
		}

		break;
	}
	case CTF_FIELD_TYPE_ID_ARRAY:
	case CTF_FIELD_TYPE_ID_SEQUENCE:
	{
		struct ctf_field_type_array_base *array_ft = (void *) ft;

		update_field_type_in_ir(array_ft->elem_ft, ft_dependents);
		ft->in_ir = array_ft->elem_ft->in_ir;

		if (ft->id == CTF_FIELD_TYPE_ID_ARRAY) {
			struct ctf_field_type_array *arr_ft = (void *) ft;

			assert(arr_ft->meaning == CTF_FIELD_TYPE_MEANING_NONE ||
				arr_ft->meaning == CTF_FIELD_TYPE_MEANING_UUID);

			/*
			 * UUID field type: nothing depends on this, so
			 * it's not part of IR.
			 */
			if (arr_ft->meaning == CTF_FIELD_TYPE_MEANING_UUID) {
				ft->in_ir = false;
				array_ft->elem_ft->in_ir = false;
			}
		} else if (ft->id == CTF_FIELD_TYPE_ID_SEQUENCE) {
			if (ft->in_ir) {
				struct ctf_field_type_sequence *seq_ft = (void *) ft;

				/*
				 * This sequence field type is part of
				 * IR and depends on a length field type
				 * (which must also be part of IR).
				 */
				g_hash_table_insert(ft_dependents,
					seq_ft->length_ft, seq_ft->length_ft);
			}
		}

		break;
	}
	default:
		ft->in_ir = true;
		break;
	}

end:
	return;
}

/*
 * Scopes and field types are processed in reverse order because we need
 * to know if a given integer field type has dependents (sequence or
 * variant field types) when we reach it. Dependents can only be located
 * after the length/tag field type in the metadata tree.
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

			update_field_type_in_ir(ec->payload_ft, ft_dependents);
			update_field_type_in_ir(ec->spec_context_ft,
				ft_dependents);
		}

		if (!sc->is_translated) {
			update_field_type_in_ir(sc->event_common_context_ft,
				ft_dependents);
			update_field_type_in_ir(sc->event_header_ft,
				ft_dependents);
			update_field_type_in_ir(sc->packet_context_ft,
				ft_dependents);
		}
	}

	if (!ctf_tc->is_translated) {
		update_field_type_in_ir(ctf_tc->packet_header_ft,
			ft_dependents);
	}

	g_hash_table_destroy(ft_dependents);
	return ret;
}
