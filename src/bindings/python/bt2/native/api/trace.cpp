/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#include <Python.h>

#include <cstdint>
#include <optional>
#include <tuple>

#include <babeltrace2/babeltrace.h>

#include "cpp-common/bt2/trace-ir.hpp"
#include "cpp-common/bt2/value.hpp"
#include "cpp-common/bt2c/c-string-view.hpp"
#include "cpp-common/bt2c/uuid.hpp"

#include "../api.hpp"
#include "../runtime/py-module-builder.hpp"

namespace bt2py {
namespace {

std::optional<bt2c::UuidView> getTraceUuid(const bt2::ConstTrace trace) noexcept
{
    return trace.uuid();
}

void setTraceUuid(const bt2::Trace trace, const bt2c::UuidView uuid) noexcept
{
    trace.uuid(uuid);
}

std::tuple<bt2c::CStringView, bt2::ConstValue>
borrowTraceEnvEntryByIndexConst(const bt2::ConstTrace trace, const std::uint64_t index) noexcept
{
    const auto entry = trace.environmentEntry(index);

    return {entry.name, entry.value};
}

} /* namespace */

void registerTraceApi(PyModuleBuilder& mb)
{
    mb.addLibFunc<bt_trace_create>();
    mb.addLibFunc<bt_trace_get_ref>();
    mb.addLibFunc<bt_trace_put_ref>();
    mb.addLibFunc<bt_trace_borrow_class>();
    mb.addLibFunc<bt_trace_borrow_class_const>();

    mb.addLibFunc<bt_trace_get_stream_count>();
    mb.addLibFunc<bt_trace_borrow_stream_by_index>();
    mb.addLibFunc<bt_trace_borrow_stream_by_index_const>();
    mb.addLibFunc<bt_trace_borrow_stream_by_id>();
    mb.addLibFunc<bt_trace_borrow_stream_by_id_const>();

    mb.addLibFunc<bt_trace_set_namespace>();
    mb.addLibFunc<bt_trace_get_namespace>();
    mb.addLibFunc<bt_trace_set_name>();
    mb.addLibFunc<bt_trace_get_name>();
    mb.addLibFunc<bt_trace_set_uid>();
    mb.addLibFunc<bt_trace_get_uid>();
    mb.addFunc<setTraceUuid>("trace_set_uuid");
    mb.addFunc<getTraceUuid>("trace_get_uuid");

    mb.addLibFunc<bt_trace_set_environment_entry_integer>();
    mb.addLibFunc<bt_trace_set_environment_entry_string>();
    mb.addLibFunc<bt_trace_get_environment_entry_count>();
    mb.addFunc<borrowTraceEnvEntryByIndexConst>("trace_borrow_environment_entry_by_index_const");
    mb.addLibFunc<bt_trace_borrow_environment_entry_value_by_name_const>();

    mb.addLibFunc<bt_trace_set_user_attributes>();
    mb.addLibFunc<bt_trace_borrow_user_attributes>();
    mb.addLibFunc<bt_trace_borrow_user_attributes_const>();

    mb.addLibAddDestructionListenerFunc<bt_trace_add_destruction_listener>();
    mb.addLibRemoveDestructionListenerFunc<bt_trace_remove_destruction_listener>();
}

} /* namespace bt2py */
