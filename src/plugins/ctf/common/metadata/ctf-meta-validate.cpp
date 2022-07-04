/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2018 Philippe Proulx <pproulx@efficios.com>
 */

#define BT_COMP_LOG_SELF_COMP       (log_cfg->self_comp)
#define BT_COMP_LOG_SELF_COMP_CLASS (log_cfg->self_comp_class)
#define BT_LOG_OUTPUT_LEVEL         (log_cfg->log_level)
#define BT_LOG_TAG                  "PLUGIN/CTF/META/VALIDATE"
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

static int validate_stream_class(struct ctf_stream_class *sc, struct meta_log_config *log_cfg)
{
    int ret = 0;
    struct ctf_field_class_int *int_fc;
    struct ctf_field_class *fc;

    if (sc->is_translated) {
        goto end;
    }

    fc = ctf_field_class_struct_borrow_member_field_class_by_name(
        ctf_field_class_as_struct(sc->packet_context_fc), "timestamp_begin");
    if (fc) {
        if (fc->type != CTF_FIELD_CLASS_TYPE_INT && fc->type != CTF_FIELD_CLASS_TYPE_ENUM) {
            _BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(
                "Invalid packet context field class: "
                "`timestamp_begin` member is not an integer field class.");
            goto invalid;
        }

        int_fc = ctf_field_class_as_int(fc);

        if (int_fc->is_signed) {
            _BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE("Invalid packet context field class: "
                                                     "`timestamp_begin` member is signed.");
            goto invalid;
        }
    }

    fc = ctf_field_class_struct_borrow_member_field_class_by_name(
        ctf_field_class_as_struct(sc->packet_context_fc), "timestamp_end");
    if (fc) {
        if (fc->type != CTF_FIELD_CLASS_TYPE_INT && fc->type != CTF_FIELD_CLASS_TYPE_ENUM) {
            _BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(
                "Invalid packet context field class: "
                "`timestamp_end` member is not an integer field class.");
            goto invalid;
        }

        int_fc = ctf_field_class_as_int(fc);

        if (int_fc->is_signed) {
            _BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE("Invalid packet context field class: "
                                                     "`timestamp_end` member is signed.");
            goto invalid;
        }
    }

    fc = ctf_field_class_struct_borrow_member_field_class_by_name(
        ctf_field_class_as_struct(sc->packet_context_fc), "events_discarded");
    if (fc) {
        if (fc->type != CTF_FIELD_CLASS_TYPE_INT && fc->type != CTF_FIELD_CLASS_TYPE_ENUM) {
            _BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(
                "Invalid packet context field class: "
                "`events_discarded` member is not an integer field class.");
            goto invalid;
        }

        int_fc = ctf_field_class_as_int(fc);

        if (int_fc->is_signed) {
            _BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE("Invalid packet context field class: "
                                                     "`events_discarded` member is signed.");
            goto invalid;
        }
    }

    fc = ctf_field_class_struct_borrow_member_field_class_by_name(
        ctf_field_class_as_struct(sc->packet_context_fc), "packet_seq_num");
    if (fc) {
        if (fc->type != CTF_FIELD_CLASS_TYPE_INT && fc->type != CTF_FIELD_CLASS_TYPE_ENUM) {
            _BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(
                "Invalid packet context field class: "
                "`packet_seq_num` member is not an integer field class.");
            goto invalid;
        }

        int_fc = ctf_field_class_as_int(fc);

        if (int_fc->is_signed) {
            _BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE("Invalid packet context field class: "
                                                     "`packet_seq_num` member is signed.");
            goto invalid;
        }
    }

    fc = ctf_field_class_struct_borrow_member_field_class_by_name(
        ctf_field_class_as_struct(sc->packet_context_fc), "packet_size");
    if (fc) {
        if (fc->type != CTF_FIELD_CLASS_TYPE_INT && fc->type != CTF_FIELD_CLASS_TYPE_ENUM) {
            _BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(
                "Invalid packet context field class: "
                "`packet_size` member is not an integer field class.");
            goto invalid;
        }

        int_fc = ctf_field_class_as_int(fc);

        if (int_fc->is_signed) {
            _BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE("Invalid packet context field class: "
                                                     "`packet_size` member is signed.");
            goto invalid;
        }
    }

    fc = ctf_field_class_struct_borrow_member_field_class_by_name(
        ctf_field_class_as_struct(sc->packet_context_fc), "content_size");
    if (fc) {
        if (fc->type != CTF_FIELD_CLASS_TYPE_INT && fc->type != CTF_FIELD_CLASS_TYPE_ENUM) {
            _BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(
                "Invalid packet context field class: "
                "`content_size` member is not an integer field class.");
            goto invalid;
        }

        int_fc = ctf_field_class_as_int(fc);

        if (int_fc->is_signed) {
            _BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE("Invalid packet context field class: "
                                                     "`content_size` member is signed.");
            goto invalid;
        }
    }

    fc = ctf_field_class_struct_borrow_member_field_class_by_name(
        ctf_field_class_as_struct(sc->event_header_fc), "id");
    if (fc) {
        if (fc->type != CTF_FIELD_CLASS_TYPE_INT && fc->type != CTF_FIELD_CLASS_TYPE_ENUM) {
            _BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE("Invalid event header field class: "
                                                     "`id` member is not an integer field class.");
            goto invalid;
        }

        int_fc = ctf_field_class_as_int(fc);

        if (int_fc->is_signed) {
            _BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE("Invalid event header field class: "
                                                     "`id` member is signed.");
            goto invalid;
        }
    } else {
        if (sc->event_classes->len > 1) {
            _BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE("Invalid event header field class: "
                                                     "missing `id` member as there's "
                                                     "more than one event class.");
            goto invalid;
        }
    }

    goto end;

invalid:
    ret = -1;

end:
    return ret;
}

int ctf_trace_class_validate(struct ctf_trace_class *ctf_tc, struct meta_log_config *log_cfg)
{
    int ret = 0;
    struct ctf_field_class_int *int_fc;
    uint64_t i;

    if (!ctf_tc->is_translated) {
        struct ctf_field_class *fc;

        fc = ctf_field_class_struct_borrow_member_field_class_by_name(
            ctf_field_class_as_struct(ctf_tc->packet_header_fc), "magic");
        if (fc) {
            struct ctf_named_field_class *named_fc = ctf_field_class_struct_borrow_member_by_index(
                ctf_field_class_as_struct(ctf_tc->packet_header_fc), 0);

            if (named_fc->fc != fc) {
                _BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE("Invalid packet header field class: "
                                                         "`magic` member is not the first member.");
                goto invalid;
            }

            if (fc->type != CTF_FIELD_CLASS_TYPE_INT && fc->type != CTF_FIELD_CLASS_TYPE_ENUM) {
                _BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(
                    "Invalid packet header field class: "
                    "`magic` member is not an integer field class.");
                goto invalid;
            }

            int_fc = ctf_field_class_as_int(fc);

            if (int_fc->is_signed) {
                _BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE("Invalid packet header field class: "
                                                         "`magic` member is signed.");
                goto invalid;
            }

            if (int_fc->base.size != 32) {
                _BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE("Invalid packet header field class: "
                                                         "`magic` member is not 32-bit.");
                goto invalid;
            }
        }

        fc = ctf_field_class_struct_borrow_member_field_class_by_name(
            ctf_field_class_as_struct(ctf_tc->packet_header_fc), "stream_id");
        if (fc) {
            if (fc->type != CTF_FIELD_CLASS_TYPE_INT && fc->type != CTF_FIELD_CLASS_TYPE_ENUM) {
                _BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(
                    "Invalid packet header field class: "
                    "`stream_id` member is not an integer field class.");
                goto invalid;
            }

            int_fc = ctf_field_class_as_int(fc);

            if (int_fc->is_signed) {
                _BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE("Invalid packet header field class: "
                                                         "`stream_id` member is signed.");
                goto invalid;
            }
        } else {
            if (ctf_tc->stream_classes->len > 1) {
                _BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE("Invalid packet header field class: "
                                                         "missing `stream_id` member as there's "
                                                         "more than one stream class.");
                goto invalid;
            }
        }

        fc = ctf_field_class_struct_borrow_member_field_class_by_name(
            ctf_field_class_as_struct(ctf_tc->packet_header_fc), "stream_instance_id");
        if (fc) {
            if (fc->type != CTF_FIELD_CLASS_TYPE_INT && fc->type != CTF_FIELD_CLASS_TYPE_ENUM) {
                _BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(
                    "Invalid packet header field class: "
                    "`stream_instance_id` member is not an integer field class.");
                goto invalid;
            }

            int_fc = ctf_field_class_as_int(fc);

            if (int_fc->is_signed) {
                _BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE("Invalid packet header field class: "
                                                         "`stream_instance_id` member is signed.");
                goto invalid;
            }
        }

        fc = ctf_field_class_struct_borrow_member_field_class_by_name(
            ctf_field_class_as_struct(ctf_tc->packet_header_fc), "uuid");
        if (fc) {
            if (fc->type != CTF_FIELD_CLASS_TYPE_ARRAY) {
                _BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(
                    "Invalid packet header field class: "
                    "`uuid` member is not an array field class.");
                goto invalid;
            }

            ctf_field_class_array *array_fc = ctf_field_class_as_array(fc);

            if (array_fc->length != 16) {
                _BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(
                    "Invalid packet header field class: "
                    "`uuid` member is not a 16-element array field class.");
                goto invalid;
            }

            if (array_fc->base.elem_fc->type != CTF_FIELD_CLASS_TYPE_INT) {
                _BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE(
                    "Invalid packet header field class: "
                    "`uuid` member's element field class is not "
                    "an integer field class.");
                goto invalid;
            }

            int_fc = ctf_field_class_as_int(array_fc->base.elem_fc);

            if (int_fc->is_signed) {
                _BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE("Invalid packet header field class: "
                                                         "`uuid` member's element field class "
                                                         "is a signed integer field class.");
                goto invalid;
            }

            if (int_fc->base.size != 8) {
                _BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE("Invalid packet header field class: "
                                                         "`uuid` member's element field class "
                                                         "is not an 8-bit integer field class.");
                goto invalid;
            }

            if (int_fc->base.base.alignment != 8) {
                _BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE("Invalid packet header field class: "
                                                         "`uuid` member's element field class's "
                                                         "alignment is not 8.");
                goto invalid;
            }
        }
    }

    for (i = 0; i < ctf_tc->stream_classes->len; i++) {
        struct ctf_stream_class *sc = (ctf_stream_class *) ctf_tc->stream_classes->pdata[i];

        ret = validate_stream_class(sc, log_cfg);
        if (ret) {
            _BT_COMP_OR_COMP_CLASS_LOGE_APPEND_CAUSE("Invalid stream class: sc-id=%" PRIu64,
                                                     sc->id);
            goto invalid;
        }
    }

    goto end;

invalid:
    ret = -1;

end:
    return ret;
}
