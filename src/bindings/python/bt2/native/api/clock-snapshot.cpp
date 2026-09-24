/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#include <Python.h>

#include <cstdint>

#include <babeltrace2/babeltrace.h>

#include "cpp-common/bt2/clock-snapshot.hpp"

#include "../api.hpp"
#include "../runtime/py-module-builder.hpp"

namespace bt2py {
namespace {

std::int64_t getClkSnapshotNsFromOrigin(const bt2::ConstClockSnapshot clkSnapshot)
{
    return clkSnapshot.nsFromOrigin();
}

} /* namespace */

void registerClockSnapshotApi(PyModuleBuilder& mb)
{
    mb.addLibFunc<bt_clock_snapshot_borrow_clock_class_const>();
    mb.addLibFunc<bt_clock_snapshot_get_value>();
    mb.addFunc<getClkSnapshotNsFromOrigin>("clock_snapshot_get_ns_from_origin");
}

} /* namespace bt2py */
