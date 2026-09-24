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

void registerComponentApi(PyModuleBuilder& mb)
{
    mb.addLibFunc<bt_component_get_name>();
    mb.addLibFunc<bt_component_get_logging_level>();

    mb.addLibFunc<bt_component_source_get_ref>();
    mb.addLibFunc<bt_component_source_put_ref>();
    mb.addLibFunc<bt_component_source_as_component_const>();
    mb.addLibFunc<bt_component_source_borrow_class_const>();
    mb.addLibFunc<bt_component_source_get_output_port_count>();
    mb.addLibFunc<bt_component_source_borrow_output_port_by_index_const>();
    mb.addLibFunc<bt_component_source_borrow_output_port_by_name_const>();

    mb.addLibFunc<bt_component_filter_get_ref>();
    mb.addLibFunc<bt_component_filter_put_ref>();
    mb.addLibFunc<bt_component_filter_as_component_const>();
    mb.addLibFunc<bt_component_filter_borrow_class_const>();
    mb.addLibFunc<bt_component_filter_get_input_port_count>();
    mb.addLibFunc<bt_component_filter_borrow_input_port_by_index_const>();
    mb.addLibFunc<bt_component_filter_borrow_input_port_by_name_const>();
    mb.addLibFunc<bt_component_filter_get_output_port_count>();
    mb.addLibFunc<bt_component_filter_borrow_output_port_by_index_const>();
    mb.addLibFunc<bt_component_filter_borrow_output_port_by_name_const>();

    mb.addLibFunc<bt_component_sink_get_ref>();
    mb.addLibFunc<bt_component_sink_put_ref>();
    mb.addLibFunc<bt_component_sink_as_component_const>();
    mb.addLibFunc<bt_component_sink_borrow_class_const>();
    mb.addLibFunc<bt_component_sink_get_input_port_count>();
    mb.addLibFunc<bt_component_sink_borrow_input_port_by_index_const>();
    mb.addLibFunc<bt_component_sink_borrow_input_port_by_name_const>();
}

} /* namespace bt2py */
