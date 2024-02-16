/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2020 Philippe Proulx <pproulx@efficios.com>
 */

#include <utility>

#include <babeltrace2/babeltrace.h>

#include "cpp-common/bt2/graph.hpp"

#include "utils.hpp"

namespace {

/*
 * Creates a simple condition trigger, calling `func`.
 */
template <typename FuncT>
CondTrigger *makeSimpleTrigger(FuncT&& func, const CondTrigger::Type type,
                               const std::string& condId,
                               const bt2s::optional<std::string>& nameSuffix = bt2s::nullopt)
{
    return new SimpleCondTrigger {std::forward<FuncT>(func), type, condId, nameSuffix};
}

using OnCompInitFunc = std::function<void(bt2::SelfComponent)>;

/*
 * A "run in" class that delegates the execution to stored callables.
 *
 * Use the makeRunIn*Trigger() helpers below.
 */
class RunInDelegator final : public RunIn
{
public:
    static RunInDelegator makeOnCompInit(OnCompInitFunc func)
    {
        return RunInDelegator {std::move(func)};
    }

    void onCompInit(const bt2::SelfComponent self) override
    {
        if (_mOnCompInitFunc) {
            _mOnCompInitFunc(self);
        }
    }

private:
    explicit RunInDelegator(OnCompInitFunc onCompInitFunc) :
        _mOnCompInitFunc {std::move(onCompInitFunc)}
    {
    }

    OnCompInitFunc _mOnCompInitFunc;
};

/*
 * Creates a condition trigger, calling `func` in a component
 * initialization context.
 */
CondTrigger *makeRunInCompInitTrigger(OnCompInitFunc func, const CondTrigger::Type type,
                                      const std::string& condId,
                                      const bt2s::optional<std::string>& nameSuffix = bt2s::nullopt)
{
    return new RunInCondTrigger<RunInDelegator> {RunInDelegator::makeOnCompInit(std::move(func)),
                                                 type, condId, nameSuffix};
}

bt2::IntegerFieldClass::Shared createUIntFc(const bt2::SelfComponent self)
{
    return self.createTraceClass()->createUnsignedIntegerFieldClass();
}

/* Our condition triggers */
CondTrigger * const triggers[] = {
    makeSimpleTrigger(
        [] {
            bt2::Graph::create(292);
        },
        CondTrigger::Type::PRE, "graph-create:valid-mip-version"),

    makeRunInCompInitTrigger(
        [](const bt2::SelfComponent self) {
            createUIntFc(self)->fieldValueRange(0);
        },
        CondTrigger::Type::PRE, "field-class-integer-set-field-value-range:valid-n", "0"),

    makeRunInCompInitTrigger(
        [](const bt2::SelfComponent self) {
            createUIntFc(self)->fieldValueRange(65);
        },
        CondTrigger::Type::PRE, "field-class-integer-set-field-value-range:valid-n", "gt-64"),

    makeSimpleTrigger(
        [] {
            bt_field_class_integer_set_field_value_range(nullptr, 23);
        },
        CondTrigger::Type::PRE, "field-class-integer-set-field-value-range:not-null:field-class"),
};

} /* namespace */

int main(const int argc, const char ** const argv)
{
    condMain(argc, argv, triggers);
}
