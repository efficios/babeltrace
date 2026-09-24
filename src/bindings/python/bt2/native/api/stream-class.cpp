/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#include <Python.h>

#include <babeltrace2/babeltrace.h>

#include "../api.hpp"
#include "../runtime/py-module-builder.hpp"

namespace bt2py {

void registerStreamClassApi(PyModuleBuilder& mb)
{
    mb.addLibFunc<bt_stream_class_create>();
    mb.addLibFunc<bt_stream_class_create_with_id>();
    mb.addLibFunc<bt_stream_class_get_ref>();
    mb.addLibFunc<bt_stream_class_put_ref>();
    mb.addLibFunc<bt_stream_class_borrow_trace_class>();
    mb.addLibFunc<bt_stream_class_borrow_trace_class_const>();
    mb.addLibFunc<bt_stream_class_get_id>();

    mb.addLibFunc<bt_stream_class_get_event_class_count>();
    mb.addLibFunc<bt_stream_class_borrow_event_class_by_index>();
    mb.addLibFunc<bt_stream_class_borrow_event_class_by_index_const>();
    mb.addLibFunc<bt_stream_class_borrow_event_class_by_id>();
    mb.addLibFunc<bt_stream_class_borrow_event_class_by_id_const>();

    mb.addLibFunc<bt_stream_class_set_namespace>();
    mb.addLibFunc<bt_stream_class_get_namespace>();
    mb.addLibFunc<bt_stream_class_set_name>();
    mb.addLibFunc<bt_stream_class_get_name>();
    mb.addLibFunc<bt_stream_class_set_uid>();
    mb.addLibFunc<bt_stream_class_get_uid>();

    mb.addLibFunc<bt_stream_class_set_assigns_automatic_event_class_id>();
    mb.addLibBoolRetFunc<bt_stream_class_assigns_automatic_event_class_id>();
    mb.addLibFunc<bt_stream_class_set_assigns_automatic_stream_id>();
    mb.addLibBoolRetFunc<bt_stream_class_assigns_automatic_stream_id>();

    mb.addLibFunc<bt_stream_class_set_default_clock_class>();
    mb.addLibFunc<bt_stream_class_borrow_default_clock_class>();
    mb.addLibFunc<bt_stream_class_borrow_default_clock_class_const>();

    mb.addLibFunc<bt_stream_class_set_supports_packets>();
    mb.addLibBoolRetFunc<bt_stream_class_supports_packets>();
    mb.addLibBoolRetFunc<bt_stream_class_packets_have_beginning_default_clock_snapshot>();
    mb.addLibBoolRetFunc<bt_stream_class_packets_have_end_default_clock_snapshot>();

    mb.addLibFunc<bt_stream_class_set_supports_discarded_events>();
    mb.addLibBoolRetFunc<bt_stream_class_supports_discarded_events>();
    mb.addLibBoolRetFunc<bt_stream_class_discarded_events_have_default_clock_snapshots>();
    mb.addLibFunc<bt_stream_class_set_supports_discarded_packets>();
    mb.addLibBoolRetFunc<bt_stream_class_supports_discarded_packets>();
    mb.addLibBoolRetFunc<bt_stream_class_discarded_packets_have_default_clock_snapshots>();

    mb.addLibFunc<bt_stream_class_set_packet_context_field_class>();
    mb.addLibFunc<bt_stream_class_borrow_packet_context_field_class>();
    mb.addLibFunc<bt_stream_class_borrow_packet_context_field_class_const>();
    mb.addLibFunc<bt_stream_class_set_event_common_context_field_class>();
    mb.addLibFunc<bt_stream_class_borrow_event_common_context_field_class>();
    mb.addLibFunc<bt_stream_class_borrow_event_common_context_field_class_const>();

    mb.addLibFunc<bt_stream_class_set_user_attributes>();
    mb.addLibFunc<bt_stream_class_borrow_user_attributes>();
    mb.addLibFunc<bt_stream_class_borrow_user_attributes_const>();
}

} /* namespace bt2py */
