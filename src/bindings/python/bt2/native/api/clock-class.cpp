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

#include "cpp-common/bt2/clock-class.hpp"
#include "cpp-common/bt2c/uuid.hpp"

#include "../api.hpp"
#include "../runtime/py-module-builder.hpp"

namespace bt2py {
namespace {

std::optional<std::uint64_t> getClkClsPrecision(const bt2::ConstClockClass clkCls) noexcept
{
    return clkCls.precision();
}

std::optional<std::uint64_t> getClkClsAccuracy(const bt2::ConstClockClass clkCls) noexcept
{
    return clkCls.accuracy();
}

std::tuple<std::int64_t, std::uint64_t> getClkClsOffset(const bt2::ConstClockClass clkCls) noexcept
{
    const auto offset = clkCls.offsetFromOrigin();

    return {offset.seconds(), offset.cycles()};
}

std::optional<bt2c::UuidView> getClkClsUuid(const bt2::ConstClockClass clkCls) noexcept
{
    return clkCls.uuid();
}

void setClkClsUuid(const bt2::ClockClass clkCls, const bt2c::UuidView uuid) noexcept
{
    clkCls.uuid(uuid);
}

std::int64_t clkClsCyclesToNsFromOrigin(const bt2::ConstClockClass clkCls,
                                        const std::uint64_t cycles)
{
    return clkCls.cyclesToNsFromOrigin(cycles);
}

} /* namespace */

void registerClockClassApi(PyModuleBuilder& mb)
{
    mb.addLibFunc<bt_clock_class_create>();
    mb.addLibFunc<bt_clock_class_get_ref>();
    mb.addLibFunc<bt_clock_class_put_ref>();
    mb.addLibFunc<bt_clock_class_get_graph_mip_version>();

    mb.addLibFunc<bt_clock_class_set_namespace>();
    mb.addLibFunc<bt_clock_class_get_namespace>();
    mb.addLibFunc<bt_clock_class_set_name>();
    mb.addLibFunc<bt_clock_class_get_name>();
    mb.addLibFunc<bt_clock_class_set_uid>();
    mb.addLibFunc<bt_clock_class_get_uid>();
    mb.addLibFunc<bt_clock_class_set_description>();
    mb.addLibFunc<bt_clock_class_get_description>();

    mb.addLibFunc<bt_clock_class_set_frequency>();
    mb.addLibFunc<bt_clock_class_get_frequency>();
    mb.addLibFunc<bt_clock_class_set_precision>();
    mb.addFunc<getClkClsPrecision>("clock_class_get_opt_precision");
    mb.addLibFunc<bt_clock_class_set_accuracy>();
    mb.addFunc<getClkClsAccuracy>("clock_class_get_accuracy");
    mb.addLibFunc<bt_clock_class_set_offset>();
    mb.addFunc<getClkClsOffset>("clock_class_get_offset");

    mb.addLibFunc<bt_clock_class_set_origin_unix_epoch>();
    mb.addLibFunc<bt_clock_class_set_origin_unknown>();
    mb.addLibFunc<bt_clock_class_set_origin>();
    mb.addLibBoolRetFunc<bt_clock_class_origin_is_known>();
    mb.addLibBoolRetFunc<bt_clock_class_origin_is_unix_epoch>();
    mb.addLibFunc<bt_clock_class_get_origin_namespace>();
    mb.addLibFunc<bt_clock_class_get_origin_name>();
    mb.addLibFunc<bt_clock_class_get_origin_uid>();

    mb.addFunc<setClkClsUuid>("clock_class_set_uuid");
    mb.addFunc<getClkClsUuid>("clock_class_get_uuid");

    mb.addLibFunc<bt_clock_class_set_user_attributes>();
    mb.addLibFunc<bt_clock_class_borrow_user_attributes>();
    mb.addLibFunc<bt_clock_class_borrow_user_attributes_const>();

    mb.addFunc<clkClsCyclesToNsFromOrigin>("clock_class_cycles_to_ns_from_origin");
}

} /* namespace bt2py */
