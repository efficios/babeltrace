/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2020 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef TESTS_LIB_CONDS_UTILS_HPP
#define TESTS_LIB_CONDS_UTILS_HPP

#include <functional>
#include <string>
#include <utility>

#include <babeltrace2/babeltrace.h>

#include "cpp-common/bt2c/c-string-view.hpp"
#include "cpp-common/bt2s/span.hpp"

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
    /*
     * Condition type.
     */
    enum class Type
    {
        PRE,
        POST,
    };

protected:
    /*
     * Builds a condition trigger having the type `type`, the condition
     * ID `condId` (_without_ any `pre:` or `post:` prefix), and the
     * optional name suffix `nameSuffix`.
     *
     * The concatenation of `condId` and, if it's set, `-` and
     * `*nameSuffix`, forms the name of the condition trigger. Get the
     * name of the created condition trigger with name().
     */
    explicit CondTrigger(Type type, const std::string& condId,
                         const bt2c::CStringView nameSuffix) noexcept;

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
                               const bt2c::CStringView nameSuffix = {});

    void operator()() noexcept override
    {
        _mFunc();
    }

private:
    std::function<void()> _mFunc;
};

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
                              const bt2c::CStringView nameSuffix = {}) :
        CondTrigger {type, condId, nameSuffix},
        _mRunIn {std::move(runIn)}
    {
    }

    explicit RunInCondTrigger(const Type type, const std::string& condId,
                              const bt2c::CStringView nameSuffix = {}) :
        RunInCondTrigger {RunInT {}, type, condId, nameSuffix}
    {
    }

    void operator()() noexcept override
    {
        runIn(_mRunIn);
    }

private:
    RunInT _mRunIn;
};

/*
 * List of condition triggers.
 */
using CondTriggers = bt2s::span<CondTrigger * const>;

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
void condMain(int argc, const char **argv, CondTriggers triggers) noexcept;

#endif /* TESTS_LIB_CONDS_UTILS_HPP */
