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

void registerTraceClassApi(PyModuleBuilder& mb)
{
    mb.addLibFunc<bt_trace_class_create>();
    mb.addLibFunc<bt_trace_class_get_ref>();
    mb.addLibFunc<bt_trace_class_put_ref>();
    mb.addLibFunc<bt_trace_class_get_graph_mip_version>();

    mb.addLibFunc<bt_trace_class_get_stream_class_count>();
    mb.addLibFunc<bt_trace_class_borrow_stream_class_by_index>();
    mb.addLibFunc<bt_trace_class_borrow_stream_class_by_index_const>();
    mb.addLibFunc<bt_trace_class_borrow_stream_class_by_id>();
    mb.addLibFunc<bt_trace_class_borrow_stream_class_by_id_const>();

    mb.addLibFunc<bt_trace_class_set_assigns_automatic_stream_class_id>();
    mb.addLibBoolRetFunc<bt_trace_class_assigns_automatic_stream_class_id>();

    mb.addLibFunc<bt_trace_class_set_user_attributes>();
    mb.addLibFunc<bt_trace_class_borrow_user_attributes>();
    mb.addLibFunc<bt_trace_class_borrow_user_attributes_const>();

    mb.addLibAddDestructionListenerFunc<bt_trace_class_add_destruction_listener>();
    mb.addLibRemoveDestructionListenerFunc<bt_trace_class_remove_destruction_listener>();
}

} /* namespace bt2py */
