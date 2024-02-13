/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2020-2023 EfficiOS, inc.
 */

#ifndef TESTS_LIB_UTILS_H
#define TESTS_LIB_UTILS_H

#include <functional>

#include <babeltrace2/babeltrace.h>

#include "cpp-common/bt2/self-component-class.hpp"
#include "cpp-common/bt2/self-component-port.hpp"
#include "cpp-common/bt2/self-message-iterator.hpp"

using RunInCompClsQueryFunc = std::function<void(bt2::SelfComponentClass)>;
using RunInCompClsInitFunc = std::function<void(bt2::SelfComponent)>;
using RunInMsgIterClsInitFunc = std::function<void(bt2::SelfMessageIterator)>;

/*
 * Runs:
 *
 * • `compClsCtxFunc` in the context of a component class method,
 *   if not `nullptr`.
 *
 * • `compCtxFunc` in the context of a component method, if not
 *   `nullptr`.
 *
 * • `msgIterCtxFunc` in the context of a message iterator method, if
 *   not `nullptr`.
 */
void runIn(RunInCompClsQueryFunc compClsCtxFunc, RunInCompClsInitFunc compCtxFunc,
           RunInMsgIterClsInitFunc msgIterCtxFunc);

/*
 * Runs `func` in the context of a component class method.
 */
void runInCompClsQuery(RunInCompClsQueryFunc func);

/*
 * Runs `func` in the context of a component method.
 */
void runInCompClsInit(RunInCompClsInitFunc func);

/*
 * Runs `func` in the context of a message iterator method.
 */
void runInMsgIterClsInit(RunInMsgIterClsInitFunc func);

#endif /* TESTS_LIB_UTILS_H */
