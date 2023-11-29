/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2018 Philippe Proulx <pproulx@efficios.com>
 */

#include <glib.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include <babeltrace2/babeltrace.h>

#include "common/assert.h"
#include "common/macros.h"

#include "ctf-meta-visitors.hpp"

static int update_field_class_stored_value_index(struct ctf_field_class *fc,
                                                 struct ctf_trace_class *tc,
                                                 struct ctf_stream_class *sc,
                                                 struct ctf_event_class *ec)
{
    int ret = 0;
    uint64_t i;
    struct ctf_field_path *field_path = NULL;
    struct ctf_field_class_int *tgt_fc = NULL;
    uint64_t *stored_value_index = NULL;

    if (!fc) {
        goto end;
    }

    switch (fc->type) {
    case CTF_FIELD_CLASS_TYPE_VARIANT:
    {
        ctf_field_class_variant *var_fc = ctf_field_class_as_variant(fc);

        field_path = &var_fc->tag_path;
        stored_value_index = &var_fc->stored_tag_index;
        tgt_fc = &var_fc->tag_fc->base;
        break;
    }
    case CTF_FIELD_CLASS_TYPE_SEQUENCE:
    {
        struct ctf_field_class_sequence *seq_fc = ctf_field_class_as_sequence(fc);

        field_path = &seq_fc->length_path;
        stored_value_index = &seq_fc->stored_length_index;
        tgt_fc = seq_fc->length_fc;
        break;
    }
    default:
        break;
    }

    if (field_path) {
        BT_ASSERT(tgt_fc);
        BT_ASSERT(tgt_fc->base.base.type == CTF_FIELD_CLASS_TYPE_INT ||
                  tgt_fc->base.base.type == CTF_FIELD_CLASS_TYPE_ENUM);
        if (tgt_fc->storing_index >= 0) {
            /* Already storing its value */
            *stored_value_index = (uint64_t) tgt_fc->storing_index;
        } else {
            /* Not storing its value: allocate new index */
            tgt_fc->storing_index = tc->stored_value_count;
            *stored_value_index = (uint64_t) tgt_fc->storing_index;
            tc->stored_value_count++;
        }
    }

    switch (fc->type) {
    case CTF_FIELD_CLASS_TYPE_STRUCT:
    {
        struct ctf_field_class_struct *struct_fc = ctf_field_class_as_struct(fc);

        for (i = 0; i < struct_fc->members->len; i++) {
            struct ctf_named_field_class *named_fc =
                ctf_field_class_struct_borrow_member_by_index(struct_fc, i);

            ret = update_field_class_stored_value_index(named_fc->fc, tc, sc, ec);
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
                ctf_field_class_variant_borrow_option_by_index(var_fc, i);

            ret = update_field_class_stored_value_index(named_fc->fc, tc, sc, ec);
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

        ret = update_field_class_stored_value_index(array_fc->elem_fc, tc, sc, ec);
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

int ctf_trace_class_update_value_storing_indexes(struct ctf_trace_class *ctf_tc)
{
    uint64_t i;

    if (!ctf_tc->is_translated) {
        update_field_class_stored_value_index(ctf_tc->packet_header_fc, ctf_tc, NULL, NULL);
    }

    for (i = 0; i < ctf_tc->stream_classes->len; i++) {
        uint64_t j;
        ctf_stream_class *sc = (ctf_stream_class *) ctf_tc->stream_classes->pdata[i];

        if (!sc->is_translated) {
            update_field_class_stored_value_index(sc->packet_context_fc, ctf_tc, sc, NULL);
            update_field_class_stored_value_index(sc->event_header_fc, ctf_tc, sc, NULL);
            update_field_class_stored_value_index(sc->event_common_context_fc, ctf_tc, sc, NULL);
        }

        for (j = 0; j < sc->event_classes->len; j++) {
            struct ctf_event_class *ec = (ctf_event_class *) sc->event_classes->pdata[j];

            if (!ec->is_translated) {
                update_field_class_stored_value_index(ec->spec_context_fc, ctf_tc, sc, ec);
                update_field_class_stored_value_index(ec->payload_fc, ctf_tc, sc, ec);
            }
        }
    }

    return 0;
}
