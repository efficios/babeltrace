/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2020-2023 EfficiOS, inc.
 */

#ifndef TESTS_LIB_UTILS_RUN_IN_HPP
#define TESTS_LIB_UTILS_RUN_IN_HPP

#include <babeltrace2/babeltrace.h>

#include "cpp-common/bt2/self-component-class.hpp"
#include "cpp-common/bt2/self-component-port.hpp"
#include "cpp-common/bt2/self-message-iterator.hpp"

/*
 * Base class from which to inherit to call runIn().
 *
 * Override any of the on*() methods to get your statements executed in
 * a specific context.
 */
class RunIn
{
public:
    virtual ~RunIn() = default;

    /*
     * Called when querying the component class `self`.
     */
    virtual void onQuery(bt2::SelfComponentClass self);

    /*
     * Called when initializing the component `self`.
     */
    virtual void onCompInit(bt2::SelfComponent self);

    /*
     * Called when initializing the message iterator `self`.
     */
    virtual void onMsgIterInit(bt2::SelfMessageIterator self);
};

/*
 * Runs a simple graph (one source and one sink component), calling the
 * `on*()` methods of `runIn` along the way.
 */
void runIn(RunIn& runIn);

#endif /* TESTS_LIB_UTILS_RUN_IN_HPP */
