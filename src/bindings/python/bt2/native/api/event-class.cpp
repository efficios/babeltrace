/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#include <Python.h>

#include <optional>

#include <babeltrace2/babeltrace.h>

#include "cpp-common/bt2/trace-ir.hpp"

#include "../api.hpp"
#include "../runtime/py-module-builder.hpp"

namespace bt2py {
namespace {

std::optional<bt2::EventClassLogLevel>
getEventClsLogLevel(const bt2::ConstEventClass eventCls) noexcept
{
    return eventCls.logLevel();
}

} /* namespace */

void registerEventClassApi(PyModuleBuilder& mb)
{
    mb.addLibFunc<bt_event_class_create>();
    mb.addLibFunc<bt_event_class_create_with_id>();
    mb.addLibFunc<bt_event_class_get_ref>();
    mb.addLibFunc<bt_event_class_put_ref>();
    mb.addLibFunc<bt_event_class_borrow_stream_class>();
    mb.addLibFunc<bt_event_class_borrow_stream_class_const>();
    mb.addLibFunc<bt_event_class_get_id>();

    mb.addLibFunc<bt_event_class_set_namespace>();
    mb.addLibFunc<bt_event_class_get_namespace>();
    mb.addLibFunc<bt_event_class_set_name>();
    mb.addLibFunc<bt_event_class_get_name>();
    mb.addLibFunc<bt_event_class_set_uid>();
    mb.addLibFunc<bt_event_class_get_uid>();

    mb.addLibFunc<bt_event_class_set_log_level>();
    mb.addFunc<getEventClsLogLevel>("event_class_get_log_level");

    mb.addWiseEnumConstants<bt2::EventClassLogLevel>("EVENT_CLASS_LOG_LEVEL_");

    mb.addLibFunc<bt_event_class_set_emf_uri>();
    mb.addLibFunc<bt_event_class_get_emf_uri>();

    mb.addLibFunc<bt_event_class_set_specific_context_field_class>();
    mb.addLibFunc<bt_event_class_borrow_specific_context_field_class>();
    mb.addLibFunc<bt_event_class_borrow_specific_context_field_class_const>();
    mb.addLibFunc<bt_event_class_set_payload_field_class>();
    mb.addLibFunc<bt_event_class_borrow_payload_field_class>();
    mb.addLibFunc<bt_event_class_borrow_payload_field_class_const>();

    mb.addLibFunc<bt_event_class_set_user_attributes>();
    mb.addLibFunc<bt_event_class_borrow_user_attributes>();
    mb.addLibFunc<bt_event_class_borrow_user_attributes_const>();
}

} /* namespace bt2py */
