/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#include <Python.h>

#include <babeltrace2/babeltrace.h>

#include "cpp-common/bt2/error.hpp"

#include "../api.hpp"
#include "../runtime/py-module-builder.hpp"

namespace bt2py {

void registerErrorApi(PyModuleBuilder& mb)
{
    mb.addLibFunc<bt_current_thread_take_error>();
    mb.addLibFunc<bt_current_thread_move_error>();
    mb.addLibFunc<bt_current_thread_clear_error>();

    mb.addLibFunc<bt_error_get_cause_count>();
    mb.addLibFunc<bt_error_borrow_cause_by_index>();
    mb.addLibFunc<bt_error_release>();

    mb.addWiseEnumConstants<bt2::ErrorCauseActorType>("ERROR_CAUSE_ACTOR_TYPE_");

    mb.addLibFunc<bt_error_cause_get_actor_type>();
    mb.addLibFunc<bt_error_cause_get_message>();
    mb.addLibFunc<bt_error_cause_get_module_name>();
    mb.addLibFunc<bt_error_cause_get_file_name>();
    mb.addLibFunc<bt_error_cause_get_line_number>();

    mb.addLibFunc<bt_error_cause_component_actor_get_component_name>();
    mb.addLibFunc<bt_error_cause_component_actor_get_component_class_type>();
    mb.addLibFunc<bt_error_cause_component_actor_get_component_class_name>();
    mb.addLibFunc<bt_error_cause_component_actor_get_plugin_name>();

    mb.addLibFunc<bt_error_cause_component_class_actor_get_component_class_type>();
    mb.addLibFunc<bt_error_cause_component_class_actor_get_component_class_name>();
    mb.addLibFunc<bt_error_cause_component_class_actor_get_plugin_name>();

    mb.addLibFunc<bt_error_cause_message_iterator_actor_get_component_name>();
    mb.addLibFunc<bt_error_cause_message_iterator_actor_get_component_output_port_name>();
    mb.addLibFunc<bt_error_cause_message_iterator_actor_get_component_class_type>();
    mb.addLibFunc<bt_error_cause_message_iterator_actor_get_component_class_name>();
    mb.addLibFunc<bt_error_cause_message_iterator_actor_get_plugin_name>();
}

} /* namespace bt2py */
