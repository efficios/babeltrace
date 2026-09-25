/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2026 EfficiOS Inc.
 */

#include <cstdint>

#include <babeltrace2/babeltrace.h>

#include "cpp-common/bt2/clock-class.hpp"
#include "cpp-common/bt2/value.hpp"
#include "cpp-common/bt2c/uuid.hpp"

#include "conds-triggers.hpp"
#include "utils.hpp"

namespace {

/*
 * Creates a clock class and freezes it.
 *
 * Freezes the clock class by attaching it as the default clock class of
 * a stream class, which is what
 * bt_stream_class_set_default_clock_class() does
 * (see _bt_clock_class_freeze()).
 */
bt2::ClockClass::Shared createFrozenClkCls(const bt2::SelfComponent selfComp)
{
    const auto clkCls = selfComp.createClockClass();

    selfComp.createTraceClass()->createStreamClass()->defaultClockClass(*clkCls);
    return clkCls;
}

} /* namespace */

/*
 * Adds clock class API precondition failure triggers.
 */
void addClkClsPreCondsTriggers(CondTriggers& triggers)
{
    addPreTrigger(
        triggers,
        [] {
            bt_clock_class_create(nullptr);
        },
        "clock-class-create:not-null:component");

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            withCurrentThreadError([&] {
                bt_clock_class_create(selfComp.libObjPtr());
            });
        },
        CondTrigger::Type::Pre, "clock-class-create:no-error", 0));

    addPreTrigger(
        triggers,
        [] {
            bt_clock_class_get_namespace(nullptr);
        },
        "clock-class-get-namespace:not-null:clock-class");

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            bt_clock_class_get_namespace(selfComp.createClockClass()->libObjPtr());
        },
        CondTrigger::Type::Pre, "clock-class-get-namespace:mip-version-is-valid", 0));

    addPreTrigger(
        triggers,
        [] {
            bt_clock_class_set_namespace(nullptr, "ns");
        },
        "clock-class-set-namespace:not-null:clock-class");

    addRunInCompInitTriggerPerMipVersion(
        triggers,
        [](const auto selfComp) {
            const auto clkCls = selfComp.createClockClass();

            withCurrentThreadError([&] {
                bt_clock_class_set_namespace(clkCls->libObjPtr(), "ns");
            });
        },
        "clock-class-set-namespace:no-error");

    addRunInCompInitTriggerPerMipVersion(
        triggers,
        [](const auto selfComp) {
            bt_clock_class_set_namespace(selfComp.createClockClass()->libObjPtr(), nullptr);
        },
        "clock-class-set-namespace:not-null:name");

    addRunInCompInitTriggerPerMipVersion(
        triggers,
        [](const auto selfComp) {
            bt_clock_class_set_namespace(createFrozenClkCls(selfComp)->libObjPtr(), "ns");
        },
        "clock-class-set-namespace:not-frozen:clock-class");

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            bt_clock_class_set_namespace(selfComp.createClockClass()->libObjPtr(), "ns");
        },
        CondTrigger::Type::Pre, "clock-class-set-namespace:mip-version-is-valid", 0));

    addPreTrigger(
        triggers,
        [] {
            bt_clock_class_get_name(nullptr);
        },
        "clock-class-get-name:not-null:clock-class");

    addPreTrigger(
        triggers,
        [] {
            bt_clock_class_set_name(nullptr, "name");
        },
        "clock-class-set-name:not-null:clock-class");

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            const auto clkCls = selfComp.createClockClass();

            withCurrentThreadError([&] {
                bt_clock_class_set_name(clkCls->libObjPtr(), "name");
            });
        },
        CondTrigger::Type::Pre, "clock-class-set-name:no-error", 0));

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            bt_clock_class_set_name(selfComp.createClockClass()->libObjPtr(), nullptr);
        },
        CondTrigger::Type::Pre, "clock-class-set-name:not-null:name", 0));

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            bt_clock_class_set_name(createFrozenClkCls(selfComp)->libObjPtr(), "name");
        },
        CondTrigger::Type::Pre, "clock-class-set-name:not-frozen:clock-class", 0));

    addPreTrigger(
        triggers,
        [] {
            bt_clock_class_get_uid(nullptr);
        },
        "clock-class-get-uid:not-null:clock-class");

    addRunInCompInitTriggerPerMipVersion(
        triggers,
        [](const auto selfComp) {
            const auto clkCls = selfComp.createClockClass();

            withCurrentThreadError([&] {
                bt_clock_class_get_uid(clkCls->libObjPtr());
            });
        },
        "clock-class-get-uid:no-error");

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            bt_clock_class_get_uid(selfComp.createClockClass()->libObjPtr());
        },
        CondTrigger::Type::Pre, "clock-class-get-uid:mip-version-is-valid", 0));

    addPreTrigger(
        triggers,
        [] {
            bt_clock_class_set_uid(nullptr, "uid");
        },
        "clock-class-set-uid:not-null:clock-class");

    addRunInCompInitTriggerPerMipVersion(
        triggers,
        [](const auto selfComp) {
            const auto clkCls = selfComp.createClockClass();

            withCurrentThreadError([&] {
                bt_clock_class_set_uid(clkCls->libObjPtr(), "uid");
            });
        },
        "clock-class-set-uid:no-error");

    addRunInCompInitTriggerPerMipVersion(
        triggers,
        [](const auto selfComp) {
            bt_clock_class_set_uid(selfComp.createClockClass()->libObjPtr(), nullptr);
        },
        "clock-class-set-uid:not-null:name");

    addRunInCompInitTriggerPerMipVersion(
        triggers,
        [](const auto selfComp) {
            bt_clock_class_set_uid(createFrozenClkCls(selfComp)->libObjPtr(), "uid");
        },
        "clock-class-set-uid:not-frozen:clock-class");

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            bt_clock_class_set_uid(selfComp.createClockClass()->libObjPtr(), "uid");
        },
        CondTrigger::Type::Pre, "clock-class-set-uid:mip-version-is-valid", 0));

    addPreTrigger(
        triggers,
        [] {
            bt_clock_class_get_description(nullptr);
        },
        "clock-class-get-description:not-null:clock-class");

    addPreTrigger(
        triggers,
        [] {
            bt_clock_class_set_description(nullptr, "descr");
        },
        "clock-class-set-description:not-null:clock-class");

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            const auto clkCls = selfComp.createClockClass();

            withCurrentThreadError([&] {
                bt_clock_class_set_description(clkCls->libObjPtr(), "descr");
            });
        },
        CondTrigger::Type::Pre, "clock-class-set-description:no-error", 0));

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            bt_clock_class_set_description(selfComp.createClockClass()->libObjPtr(), nullptr);
        },
        CondTrigger::Type::Pre, "clock-class-set-description:not-null:description", 0));

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            bt_clock_class_set_description(createFrozenClkCls(selfComp)->libObjPtr(), "descr");
        },
        CondTrigger::Type::Pre, "clock-class-set-description:not-frozen:clock-class", 0));

    addPreTrigger(
        triggers,
        [] {
            bt_clock_class_get_frequency(nullptr);
        },
        "clock-class-get-frequency:not-null:clock-class");

    addPreTrigger(
        triggers,
        [] {
            bt_clock_class_set_frequency(nullptr, 1000);
        },
        "clock-class-set-frequency:not-null:clock-class");

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            bt_clock_class_set_frequency(createFrozenClkCls(selfComp)->libObjPtr(), 1000);
        },
        CondTrigger::Type::Pre, "clock-class-set-frequency:not-frozen:clock-class", 0));

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            bt_clock_class_set_frequency(selfComp.createClockClass()->libObjPtr(), 0);
        },
        CondTrigger::Type::Pre, "clock-class-set-frequency:valid-frequency", 0));

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            const auto clkCls = selfComp.createClockClass();

            /*
             * Default frequency is 1 GHz; set offset cycles to a value
             * larger than the new frequency we set just below.
             */
            bt_clock_class_set_offset(clkCls->libObjPtr(), 0, 100);
            bt_clock_class_set_frequency(clkCls->libObjPtr(), 50);
        },
        CondTrigger::Type::Pre, "clock-class-set-frequency:offset-cycles-lt-frequency", 0));

    addPreTrigger(
        triggers,
        [] {
            bt_clock_class_get_precision(nullptr);
        },
        "clock-class-get-precision:not-null:clock-class");

    addRunInCompInitTriggerPerMipVersion(
        triggers,
        [](const auto selfComp) {
            bt_clock_class_get_precision(selfComp.createClockClass()->libObjPtr());
        },
        "clock-class-get-precision:mip-version-is-valid");

    addPreTrigger(
        triggers,
        [] {
            std::uint64_t precision;

            bt_clock_class_get_opt_precision(nullptr, &precision);
        },
        "clock-class-get-opt-precision:not-null:clock-class");

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            const auto clkCls = selfComp.createClockClass();

            withCurrentThreadError([&] {
                std::uint64_t precision;

                bt_clock_class_get_opt_precision(clkCls->libObjPtr(), &precision);
            });
        },
        CondTrigger::Type::Pre, "clock-class-get-opt-precision:no-error", 0));

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            bt_clock_class_get_opt_precision(selfComp.createClockClass()->libObjPtr(), nullptr);
        },
        CondTrigger::Type::Pre, "clock-class-get-opt-precision:not-null:precision-output", 0));

    addPreTrigger(
        triggers,
        [] {
            bt_clock_class_set_precision(nullptr, 1);
        },
        "clock-class-set-precision:not-null:clock-class");

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            bt_clock_class_set_precision(createFrozenClkCls(selfComp)->libObjPtr(), 1);
        },
        CondTrigger::Type::Pre, "clock-class-set-precision:not-frozen:clock-class", 0));

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            bt_clock_class_set_precision(selfComp.createClockClass()->libObjPtr(), UINT64_C(-1));
        },
        CondTrigger::Type::Pre, "clock-class-set-precision:valid-precision", 0));

    addPreTrigger(
        triggers,
        [] {
            bt_clock_class_set_accuracy(nullptr, 1);
        },
        "clock-class-set-accuracy:not-null:clock-class");

    addRunInCompInitTriggerPerMipVersion(
        triggers,
        [](const auto selfComp) {
            const auto clkCls = selfComp.createClockClass();

            withCurrentThreadError([&] {
                bt_clock_class_set_accuracy(clkCls->libObjPtr(), 1);
            });
        },
        "clock-class-set-accuracy:no-error");

    addRunInCompInitTriggerPerMipVersion(
        triggers,
        [](const auto selfComp) {
            bt_clock_class_set_accuracy(createFrozenClkCls(selfComp)->libObjPtr(), 1);
        },
        "clock-class-set-accuracy:not-frozen:clock-class");

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            bt_clock_class_set_accuracy(selfComp.createClockClass()->libObjPtr(), 1);
        },
        CondTrigger::Type::Pre, "clock-class-set-accuracy:mip-version-is-valid", 0));

    addRunInCompInitTriggerPerMipVersion(
        triggers,
        [](const auto selfComp) {
            bt_clock_class_set_accuracy(selfComp.createClockClass()->libObjPtr(), UINT64_C(-1));
        },
        "clock-class-set-accuracy:valid-accuracy");

    addPreTrigger(
        triggers,
        [] {
            std::uint64_t accuracy;

            bt_clock_class_get_accuracy(nullptr, &accuracy);
        },
        "clock-class-get-accuracy:not-null:clock-class");

    addRunInCompInitTriggerPerMipVersion(
        triggers,
        [](const auto selfComp) {
            const auto clkCls = selfComp.createClockClass();

            withCurrentThreadError([&] {
                std::uint64_t accuracy;

                bt_clock_class_get_accuracy(clkCls->libObjPtr(), &accuracy);
            });
        },
        "clock-class-get-accuracy:no-error");

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            std::uint64_t accuracy;

            bt_clock_class_get_accuracy(selfComp.createClockClass()->libObjPtr(), &accuracy);
        },
        CondTrigger::Type::Pre, "clock-class-get-accuracy:mip-version-is-valid", 0));

    addRunInCompInitTriggerPerMipVersion(
        triggers,
        [](const auto selfComp) {
            bt_clock_class_get_accuracy(selfComp.createClockClass()->libObjPtr(), nullptr);
        },
        "clock-class-get-accuracy:not-null:accuracy-output");

    addPreTrigger(
        triggers,
        [] {
            std::int64_t seconds;
            std::uint64_t cycles;

            bt_clock_class_get_offset(nullptr, &seconds, &cycles);
        },
        "clock-class-get-offset:not-null:clock-class");

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            std::uint64_t cycles;

            bt_clock_class_get_offset(selfComp.createClockClass()->libObjPtr(), nullptr, &cycles);
        },
        CondTrigger::Type::Pre, "clock-class-get-offset:not-null:seconds-output", 0));

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            std::int64_t seconds;

            bt_clock_class_get_offset(selfComp.createClockClass()->libObjPtr(), &seconds, nullptr);
        },
        CondTrigger::Type::Pre, "clock-class-get-offset:not-null:cycles-output", 0));

    addPreTrigger(
        triggers,
        [] {
            bt_clock_class_set_offset(nullptr, 0, 0);
        },
        "clock-class-set-offset:not-null:clock-class");

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            bt_clock_class_set_offset(createFrozenClkCls(selfComp)->libObjPtr(), 0, 0);
        },
        CondTrigger::Type::Pre, "clock-class-set-offset:not-frozen:clock-class", 0));

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            /* Default frequency is 1 GHz; provide an equal offset (cycles) */
            bt_clock_class_set_offset(selfComp.createClockClass()->libObjPtr(), 0,
                                      UINT64_C(1000000000));
        },
        CondTrigger::Type::Pre, "clock-class-set-offset:offset-cycles-lt-frequency", 0));

    addPreTrigger(
        triggers,
        [] {
            bt_clock_class_get_origin_namespace(nullptr);
        },
        "clock-class-get-origin-namespace:not-null:clock-class");

    addRunInCompInitTriggerPerMipVersion(
        triggers,
        [](const auto selfComp) {
            const auto clkCls = selfComp.createClockClass();

            clkCls->origin("ns", "name", "uid");
            withCurrentThreadError([&] {
                bt_clock_class_get_origin_namespace(clkCls->libObjPtr());
            });
        },
        "clock-class-get-origin-namespace:no-error");

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            bt_clock_class_get_origin_namespace(selfComp.createClockClass()->libObjPtr());
        },
        CondTrigger::Type::Pre, "clock-class-get-origin-namespace:mip-version-is-valid", 0));

    addRunInCompInitTriggerPerMipVersion(
        triggers,
        [](const auto selfComp) {
            bt_clock_class_get_origin_namespace(selfComp.createClockClass()->libObjPtr());
        },
        "clock-class-get-origin-namespace:clock-class-origin-is-custom");

    addPreTrigger(
        triggers,
        [] {
            bt_clock_class_get_origin_name(nullptr);
        },
        "clock-class-get-origin-name:not-null:clock-class");

    addRunInCompInitTriggerPerMipVersion(
        triggers,
        [](const auto selfComp) {
            const auto clkCls = selfComp.createClockClass();

            clkCls->origin("ns", "name", "uid");
            withCurrentThreadError([&] {
                bt_clock_class_get_origin_name(clkCls->libObjPtr());
            });
        },
        "clock-class-get-origin-name:no-error");

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            bt_clock_class_get_origin_name(selfComp.createClockClass()->libObjPtr());
        },
        CondTrigger::Type::Pre, "clock-class-get-origin-name:mip-version-is-valid", 0));

    addRunInCompInitTriggerPerMipVersion(
        triggers,
        [](const auto selfComp) {
            bt_clock_class_get_origin_name(selfComp.createClockClass()->libObjPtr());
        },
        "clock-class-get-origin-name:clock-class-origin-is-custom");

    addPreTrigger(
        triggers,
        [] {
            bt_clock_class_get_origin_uid(nullptr);
        },
        "clock-class-get-origin-uid:not-null:clock-class");

    addRunInCompInitTriggerPerMipVersion(
        triggers,
        [](const auto selfComp) {
            const auto clkCls = selfComp.createClockClass();

            clkCls->origin("ns", "name", "uid");
            withCurrentThreadError([&] {
                bt_clock_class_get_origin_uid(clkCls->libObjPtr());
            });
        },
        "clock-class-get-origin-uid:no-error");

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            bt_clock_class_get_origin_uid(selfComp.createClockClass()->libObjPtr());
        },
        CondTrigger::Type::Pre, "clock-class-get-origin-uid:mip-version-is-valid", 0));

    addRunInCompInitTriggerPerMipVersion(
        triggers,
        [](const auto selfComp) {
            bt_clock_class_get_origin_uid(selfComp.createClockClass()->libObjPtr());
        },
        "clock-class-get-origin-uid:clock-class-origin-is-custom");

    addPreTrigger(
        triggers,
        [] {
            bt_clock_class_origin_is_unix_epoch(nullptr);
        },
        "clock-class-origin-is-unix-epoch:not-null:clock-class");

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            const auto clkCls = selfComp.createClockClass();

            withCurrentThreadError([&] {
                bt_clock_class_origin_is_unix_epoch(clkCls->libObjPtr());
            });
        },
        CondTrigger::Type::Pre, "clock-class-origin-is-unix-epoch:no-error", 0));

    addPreTrigger(
        triggers,
        [] {
            bt_clock_class_origin_is_known(nullptr);
        },
        "clock-class-origin-is-known:not-null:clock-class");

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            const auto clkCls = selfComp.createClockClass();

            withCurrentThreadError([&] {
                bt_clock_class_origin_is_known(clkCls->libObjPtr());
            });
        },
        CondTrigger::Type::Pre, "clock-class-origin-is-known:no-error", 0));

    addPreTrigger(
        triggers,
        [] {
            bt_clock_class_set_origin_is_unix_epoch(nullptr, BT_TRUE);
        },
        "clock-class-set-origin-is-unix-epoch:not-null:clock-class");

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            const auto clkCls = selfComp.createClockClass();

            withCurrentThreadError([&] {
                bt_clock_class_set_origin_is_unix_epoch(clkCls->libObjPtr(), BT_TRUE);
            });
        },
        CondTrigger::Type::Pre, "clock-class-set-origin-is-unix-epoch:no-error", 0));

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            bt_clock_class_set_origin_is_unix_epoch(createFrozenClkCls(selfComp)->libObjPtr(),
                                                    BT_TRUE);
        },
        CondTrigger::Type::Pre, "clock-class-set-origin-is-unix-epoch:not-frozen:clock-class", 0));

    addPreTrigger(
        triggers,
        [] {
            bt_clock_class_set_origin_unknown(nullptr);
        },
        "clock-class-set-origin-unknown:not-null:clock-class");

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            const auto clkCls = selfComp.createClockClass();

            withCurrentThreadError([&] {
                bt_clock_class_set_origin_unknown(clkCls->libObjPtr());
            });
        },
        CondTrigger::Type::Pre, "clock-class-set-origin-unknown:no-error", 0));

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            bt_clock_class_set_origin_unknown(createFrozenClkCls(selfComp)->libObjPtr());
        },
        CondTrigger::Type::Pre, "clock-class-set-origin-unknown:not-frozen:clock-class", 0));

    addPreTrigger(
        triggers,
        [] {
            bt_clock_class_set_origin(nullptr, "ns", "name", "uid");
        },
        "clock-class-set-origin:not-null:clock-class");

    addRunInCompInitTriggerPerMipVersion(
        triggers,
        [](const auto selfComp) {
            const auto clkCls = selfComp.createClockClass();

            withCurrentThreadError([&] {
                bt_clock_class_set_origin(clkCls->libObjPtr(), "ns", "name", "uid");
            });
        },
        "clock-class-set-origin:no-error");

    addRunInCompInitTriggerPerMipVersion(
        triggers,
        [](const auto selfComp) {
            bt_clock_class_set_origin(createFrozenClkCls(selfComp)->libObjPtr(), "ns", "name",
                                      "uid");
        },
        "clock-class-set-origin:not-frozen:clock-class");

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            bt_clock_class_set_origin(selfComp.createClockClass()->libObjPtr(), "ns", "name",
                                      "uid");
        },
        CondTrigger::Type::Pre, "clock-class-set-origin:mip-version-is-valid", 0));

    addRunInCompInitTriggerPerMipVersion(
        triggers,
        [](const auto selfComp) {
            bt_clock_class_set_origin(selfComp.createClockClass()->libObjPtr(), "ns", nullptr,
                                      "uid");
        },
        "clock-class-set-origin:not-null:name");

    addRunInCompInitTriggerPerMipVersion(
        triggers,
        [](const auto selfComp) {
            bt_clock_class_set_origin(selfComp.createClockClass()->libObjPtr(), "ns", "name",
                                      nullptr);
        },
        "clock-class-set-origin:not-null:uid");

    addPreTrigger(
        triggers,
        [] {
            bt_clock_class_set_origin_unix_epoch(nullptr);
        },
        "clock-class-set-origin-unix-epoch:not-null:clock-class");

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            const auto clkCls = selfComp.createClockClass();

            withCurrentThreadError([&] {
                bt_clock_class_set_origin_unix_epoch(clkCls->libObjPtr());
            });
        },
        CondTrigger::Type::Pre, "clock-class-set-origin-unix-epoch:no-error", 0));

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            bt_clock_class_set_origin_unix_epoch(createFrozenClkCls(selfComp)->libObjPtr());
        },
        CondTrigger::Type::Pre, "clock-class-set-origin-unix-epoch:not-frozen:clock-class", 0));

    addPreTrigger(
        triggers,
        [] {
            bt_clock_class_get_uuid(nullptr);
        },
        "clock-class-get-uuid:not-null:clock-class");

    addRunInCompInitTriggerPerMipVersion(
        triggers,
        [](const auto selfComp) {
            bt_clock_class_get_uuid(selfComp.createClockClass()->libObjPtr());
        },
        "clock-class-get-uuid:mip-version-is-valid");

    addPreTrigger(
        triggers,
        [] {
            const bt2c::Uuid uuid;

            bt_clock_class_set_uuid(nullptr, uuid.data());
        },
        "clock-class-set-uuid:not-null:clock-class");

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            bt_clock_class_set_uuid(selfComp.createClockClass()->libObjPtr(), nullptr);
        },
        CondTrigger::Type::Pre, "clock-class-set-uuid:not-null:uuid", 0));

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            const bt2c::Uuid uuid;

            bt_clock_class_set_uuid(createFrozenClkCls(selfComp)->libObjPtr(), uuid.data());
        },
        CondTrigger::Type::Pre, "clock-class-set-uuid:not-frozen:clock-class", 0));

    addRunInCompInitTriggerPerMipVersion(
        triggers,
        [](const auto selfComp) {
            const bt2c::Uuid uuid;

            bt_clock_class_set_uuid(selfComp.createClockClass()->libObjPtr(), uuid.data());
        },
        "clock-class-set-uuid:mip-version-is-valid");

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            const auto clkCls = selfComp.createClockClass();

            withCurrentThreadError([&] {
                std::int64_t ns;

                bt_clock_class_cycles_to_ns_from_origin(clkCls->libObjPtr(), 0, &ns);
            });
        },
        CondTrigger::Type::Pre, "clock-class-cycles-to-ns-from-origin:no-error", 0));

    addPreTrigger(
        triggers,
        [] {
            std::int64_t ns;

            bt_clock_class_cycles_to_ns_from_origin(nullptr, 0, &ns);
        },
        "clock-class-cycles-to-ns-from-origin:not-null:clock-class");

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            bt_clock_class_cycles_to_ns_from_origin(selfComp.createClockClass()->libObjPtr(), 0,
                                                    nullptr);
        },
        CondTrigger::Type::Pre, "clock-class-cycles-to-ns-from-origin:not-null:nanoseconds-output",
        0));

    addPreTrigger(
        triggers,
        [] {
            bt_clock_class_has_same_identity(nullptr, nullptr);
        },
        "clock-class-has-same-identity:not-null:clock-class");

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            bt_clock_class_has_same_identity(selfComp.createClockClass()->libObjPtr(),
                                             selfComp.createClockClass()->libObjPtr());
        },
        CondTrigger::Type::Pre, "clock-class-has-same-identity:mip-version-is-valid", 0));

    addPreTrigger(
        triggers,
        [] {
            bt_clock_class_borrow_user_attributes_const(nullptr);
        },
        "clock-class-borrow-user-attributes-const:not-null:clock-class");

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto) {
            bt_clock_class_set_user_attributes(nullptr, bt2::MapValue::create()->libObjPtr());
        },
        CondTrigger::Type::Pre, "clock-class-set-user-attributes:not-null:clock-class", 0));

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            bt_clock_class_set_user_attributes(selfComp.createClockClass()->libObjPtr(), nullptr);
        },
        CondTrigger::Type::Pre,
        "clock-class-set-user-attributes:not-null:user-attributes-value-object", 0));

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            bt_clock_class_set_user_attributes(selfComp.createClockClass()->libObjPtr(),
                                               bt2::ArrayValue::create()->libObjPtr());
        },
        CondTrigger::Type::Pre, "clock-class-set-user-attributes:is-map-value:user-attributes", 0));

    triggers.emplace_back(makeRunInCompInitTrigger(
        [](const auto selfComp) {
            bt_clock_class_set_user_attributes(createFrozenClkCls(selfComp)->libObjPtr(),
                                               bt2::MapValue::create()->libObjPtr());
        },
        CondTrigger::Type::Pre, "clock-class-set-user-attributes:not-frozen:clock-class", 0));

    addPreTrigger(
        triggers,
        [] {
            bt_clock_class_get_graph_mip_version(nullptr);
        },
        "clock-class-get-graph-mip-version:not-null:clock-class");
}
