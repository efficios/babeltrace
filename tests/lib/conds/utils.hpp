/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2020 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_TESTS_LIB_CONDS_UTILS_HPP
#define BABELTRACE_TESTS_LIB_CONDS_UTILS_HPP

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <babeltrace2/babeltrace.h>

#include "common/assert.h"
#include "cpp-common/bt2/message-array.hpp"
#include "cpp-common/bt2/mip.hpp"
#include "cpp-common/bt2/self-message-iterator.hpp"
#include "cpp-common/bt2c/c-string-view.hpp"
#include "cpp-common/bt2s/span.hpp"
#include "cpp-common/vendor/fmt/core.h"

#include "../utils/run-in.hpp"

/*
 * Abstract condition trigger class.
 *
 * A derived class must provide operator()() which triggers a condition
 * of which the specific type (precondition or postcondition) and ID are
 * provided at construction time.
 */
class CondTrigger
{
public:
    using UP = std::unique_ptr<CondTrigger>;

    /*
     * Condition type.
     */
    enum class Type
    {
        Pre,
        Post,
    };

protected:
    /*
     * Builds a condition trigger having the type `type`, the condition
     * ID `condId` (_without_ any `pre:` or `post:` prefix), and the
     * optional name suffix `nameSuffix`.
     *
     * The concatenation of `condId` and, if it's set, `:` and
     * `*nameSuffix`, forms the name of the condition trigger. Get the
     * name of the created condition trigger with name().
     */
    explicit CondTrigger(Type type, const std::string& condId,
                         const std::string_view nameSuffix) noexcept;

public:
    virtual ~CondTrigger() = default;
    virtual void operator()() noexcept = 0;

    Type type() const noexcept
    {
        return _mType;
    }

    /*
     * Condition ID, including any `pre:` or `post:` prefix.
     */
    const std::string& condId() const noexcept
    {
        return _mCondId;
    }

    const std::string& name() const noexcept
    {
        return _mName;
    }

private:
    Type _mType;
    std::string _mCondId;
    std::string _mName;
};

/*
 * Simple condition trigger.
 *
 * Implements a condition trigger where a function provided at
 * construction time triggers a condition.
 */
class SimpleCondTrigger : public CondTrigger
{
public:
    explicit SimpleCondTrigger(std::function<void()> func, Type type, const std::string& condId,
                               const std::string_view nameSuffix = {});

    void operator()() noexcept override
    {
        _mFunc();
    }

private:
    std::function<void()> _mFunc;
};

/*
 * Creates a simple condition trigger, calling `func`.
 */
template <typename FuncT>
CondTrigger::UP makeSimpleTrigger(FuncT&& func, const CondTrigger::Type type,
                                  const std::string& condId, const std::string_view nameSuffix = {})
{
    return std::make_unique<SimpleCondTrigger>(std::forward<FuncT>(func), type, condId, nameSuffix);
}

/*
 * Run-in condition trigger.
 *
 * Implements a condition trigger of which the triggering function
 * happens in a graph or component class query context using the
 * runIn() API.
 */
template <typename RunInT>
class RunInCondTrigger : public CondTrigger
{
public:
    explicit RunInCondTrigger(RunInT runIn, const Type type, const std::string& condId,
                              const std::uint64_t graphMipVersion,
                              const std::string_view nameSuffix = {})
        : CondTrigger {type, condId, nameSuffix},
          _mRunIn {std::move(runIn)},
          _mGraphMipVersion {graphMipVersion}
    {
    }

    void operator()() noexcept override
    {
        runIn(_mRunIn, _mGraphMipVersion);
    }

private:
    RunInT _mRunIn;
    std::uint64_t _mGraphMipVersion;
};

using OnCompInitFunc = std::function<void(bt2::SelfComponent)>;

/*
 * A "run in" class that delegates the execution to stored callables.
 *
 * Use the makeRunInCompInitTrigger*() helpers below.
 */
class RunInCompInitDelegator final : public RunIn
{
public:
    static RunInCompInitDelegator makeOnCompInit(OnCompInitFunc func)
    {
        return RunInCompInitDelegator {std::move(func)};
    }

    void onCompInit(const bt2::SelfComponent self) override
    {
        _mOnCompInitFunc(self);
    }

private:
    explicit RunInCompInitDelegator(OnCompInitFunc onCompInitFunc)
        : _mOnCompInitFunc {std::move(onCompInitFunc)}
    {
    }

    OnCompInitFunc _mOnCompInitFunc;
};

/*
 * Creates a condition trigger, calling `func` in a component
 * initialization context.
 */
inline CondTrigger::UP makeRunInCompInitTrigger(OnCompInitFunc func, const CondTrigger::Type type,
                                                const std::string& condId,
                                                const std::uint64_t graphMipVersion,
                                                const std::string_view nameSuffix = {})
{
    return std::make_unique<RunInCondTrigger<RunInCompInitDelegator>>(
        RunInCompInitDelegator::makeOnCompInit(std::move(func)), type, condId, graphMipVersion,
        nameSuffix);
}

/*
 * Appends a "trigger" cause to the error of the current thread, then
 * calls `func`.
 *
 * Use this within a condition trigger to make the next API call fail
 * its `no-error` precondition: the precondition is tripped because the
 * current thread already has an error when the call is evaluated.
 */
template <typename FuncT>
void withCurrentThreadError(FuncT&& func) noexcept
{
    BT_CURRENT_THREAD_ERROR_APPEND_CAUSE_FROM_UNKNOWN("cond-trigger", "Trigger error.");
    func();
}

/*
 * List of condition triggers.
 */
using CondTriggers = std::vector<CondTrigger::UP>;

/*
 * Appends a simple precondition trigger which calls `func`
 * to `triggers`.
 *
 * Equivalent to calling makeSimpleTrigger() with
 * `CondTrigger::Type::Pre` and pushing the result, but avoids the
 * surrounding boilerplate at call sites.
 */
template <typename FuncT>
void addPreTrigger(CondTriggers& triggers, FuncT&& func, const std::string& condId,
                   const std::string_view nameSuffix = {})
{
    triggers.emplace_back(
        makeSimpleTrigger(std::forward<FuncT>(func), CondTrigger::Type::Pre, condId, nameSuffix));
}

/*
 * Appends a simple precondition trigger which appends an error cause to
 * the error of the current thread and then calls `tripFunc`, tripping
 * the `no-error` precondition of the target API.
 *
 * Use this instead of writing `withCurrentThreadError()` inside the
 * callable yourself.
 */
template <typename FuncT>
void addPreNoErrorTrigger(CondTriggers& triggers, FuncT tripFunc, const std::string& condId,
                          const std::string_view nameSuffix = {})
{
    addPreTrigger(
        triggers,
        [call = std::move(tripFunc)]() noexcept {
            withCurrentThreadError(call);
        },
        condId, nameSuffix);
}

/*
 * A "run in" class that delegates to a stored function executed once in
 * onMsgIterInit() context.
 *
 * Use makeRunInMsgIterInitTrigger() below.
 */
class RunInMsgIterInitDelegator final : public RunIn
{
public:
    using Func = std::function<void(bt2::SelfMessageIterator)>;

    static RunInMsgIterInitDelegator make(Func func)
    {
        return RunInMsgIterInitDelegator {std::move(func)};
    }

    void onMsgIterInit(const bt2::SelfMessageIterator self) override
    {
        BT_ASSERT(!_mBeenThere);
        _mBeenThere = true;
        _mFunc(self);
    }

private:
    explicit RunInMsgIterInitDelegator(Func func)
        : _mFunc {std::move(func)}
    {
    }

    Func _mFunc;
    bool _mBeenThere = false;
};

/*
 * A "run in" class that delegates to a stored function executed once in
 * onMsgIterNext() context.
 *
 * Use makeRunInMsgIterNextTrigger() below.
 */
class RunInMsgIterNextDelegator final : public RunIn
{
public:
    using Func = std::function<void(bt2::SelfMessageIterator, bt2::ConstMessageArray&)>;

    static RunInMsgIterNextDelegator make(Func func)
    {
        return RunInMsgIterNextDelegator {std::move(func)};
    }

    void onMsgIterNext(const bt2::SelfMessageIterator self, bt2::ConstMessageArray& msgs) override
    {
        BT_ASSERT(!_mBeenThere);
        _mBeenThere = true;
        _mFunc(self, msgs);
    }

private:
    explicit RunInMsgIterNextDelegator(Func func)
        : _mFunc {std::move(func)}
    {
    }

    Func _mFunc;
    bool _mBeenThere = false;
};

/*
 * Creates a condition trigger which runs `func` once when the message
 * iterator of the underlying graph is initialized.
 */
inline CondTrigger::UP makeRunInMsgIterInitTrigger(RunInMsgIterInitDelegator::Func func,
                                                   const CondTrigger::Type type,
                                                   const std::string& condId,
                                                   const std::uint64_t graphMipVersion,
                                                   const std::string_view nameSuffix = {})
{
    return std::make_unique<RunInCondTrigger<RunInMsgIterInitDelegator>>(
        RunInMsgIterInitDelegator::make(std::move(func)), type, condId, graphMipVersion,
        nameSuffix);
}

/*
 * Creates a condition trigger which runs `func` once during a "next"
 * call on the message iterator of the underlying graph.
 */
inline CondTrigger::UP makeRunInMsgIterNextTrigger(RunInMsgIterNextDelegator::Func func,
                                                   const CondTrigger::Type type,
                                                   const std::string& condId,
                                                   const std::uint64_t graphMipVersion,
                                                   const std::string_view nameSuffix = {})
{
    return std::make_unique<RunInCondTrigger<RunInMsgIterNextDelegator>>(
        RunInMsgIterNextDelegator::make(std::move(func)), type, condId, graphMipVersion,
        nameSuffix);
}

/*
 * Adds, for each supported MIP version starting at `firstMipVersion`
 * (MIP 1 by default), a precondition trigger which calls `func` in a
 * component initialization context.
 *
 * The condition ID of each trigger is `condId` and the name suffix is
 * `mipN` where `N` is the MIP version, ensuring a unique trigger name
 * per MIP version.
 */
inline void addRunInCompInitTriggerPerMipVersion(CondTriggers& triggers, const OnCompInitFunc& func,
                                                 const std::string& condId,
                                                 const std::uint64_t firstMipVersion = 1)
{
    for (auto mipVersion = firstMipVersion; mipVersion <= bt2::getMaximalMipVersion();
         ++mipVersion) {
        triggers.emplace_back(makeRunInCompInitTrigger(
            func, CondTrigger::Type::Pre, condId, mipVersion, fmt::format("mip{}", mipVersion)));
    }
}

/*
 * The entry point of a condition trigger program.
 *
 * Call this from your own main() with your list of condition triggers
 * `triggers`.
 *
 * Each condition trigger of `triggers` must have a unique name, as
 * returned by CondTrigger::name().
 *
 * This function uses `argc` and `argv` to respond to one of the
 * following commands:
 *
 * `list`:
 *     Prints a list of condition triggers as a JSON array of objects.
 *
 *     Each JSON object has:
 *
 *     `cond-id`:
 *         The condition ID of the trigger, as returned by
 *         CondTrigger:condId().
 *
 *     `name`:
 *         The condition ID name, as returned by CondTrigger::name().
 *
 * `run`:
 *     Runs the triggering function of the condition trigger at the
 *     index specified by the next command-line argument.
 *
 *     For example,
 *
 *         $ my-cond-trigger-program run 45
 *
 *     would run the function of the condition trigger `triggers[45]`.
 *
 *     The program is expected to abort through a libbabeltrace2
 *     condition failure.
 */
void condMain(const bt2s::span<const char * const> argv, const CondTriggers& triggers) noexcept;

#endif /* BABELTRACE_TESTS_LIB_CONDS_UTILS_HPP */
