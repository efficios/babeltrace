/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2020 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef TESTS_LIB_CONDS_UTILS_H
#define TESTS_LIB_CONDS_UTILS_H

#include <babeltrace2/babeltrace.h>

#include "cpp-common/bt2/self-component-port.hpp"
#include "cpp-common/bt2s/span.hpp"

enum cond_trigger_func_type
{
    COND_TRIGGER_FUNC_TYPE_BASIC,
    COND_TRIGGER_FUNC_TYPE_RUN_IN_COMP_CLS_INIT,
};

enum cond_trigger_type
{
    COND_TRIGGER_TYPE_PRE,
    COND_TRIGGER_TYPE_POST,
};

typedef void (*cond_trigger_basic_func)(void);
typedef void (*cond_trigger_run_in_comp_cls_init_func)(bt2::SelfComponent);

struct cond_trigger
{
    enum cond_trigger_type type;
    enum cond_trigger_func_type func_type;
    const char *cond_id;
    const char *suffix;
    union
    {
        cond_trigger_basic_func basic;
        cond_trigger_run_in_comp_cls_init_func run_in_comp_cls_init;
    } func;
};

#define COND_TRIGGER_PRE_BASIC(_cond_id, _suffix, _func)                                           \
    {                                                                                              \
        .type = COND_TRIGGER_TYPE_PRE, .func_type = COND_TRIGGER_FUNC_TYPE_BASIC,                  \
        .cond_id = _cond_id, .suffix = _suffix, .func = {                                          \
            .basic = _func,                                                                        \
        }                                                                                          \
    }

#define COND_TRIGGER_POST_BASIC(_cond_id, _suffix, _func)                                          \
    {                                                                                              \
        .type = COND_TRIGGER_TYPE_POST, .func_type = COND_TRIGGER_FUNC_TYPE_BASIC,                 \
        .cond_id = _cond_id, .suffix = _suffix, .func = {                                          \
            .basic = _func,                                                                        \
        }                                                                                          \
    }

#define COND_TRIGGER_PRE_RUN_IN_COMP_CLS_INIT(_cond_id, _suffix, _func)                            \
    {                                                                                              \
        .type = COND_TRIGGER_TYPE_PRE, .func_type = COND_TRIGGER_FUNC_TYPE_RUN_IN_COMP_CLS_INIT,   \
        .cond_id = _cond_id, .suffix = _suffix, .func = {                                          \
            .run_in_comp_cls_init = _func,                                                         \
        }                                                                                          \
    }

#define COND_TRIGGER_POST_RUN_IN_COMP_CLS_INIT(_cond_id, _suffix, _func)                           \
    {                                                                                              \
        .type = COND_TRIGGER_TYPE_POST, .func_type = COND_TRIGGER_FUNC_TYPE_RUN_IN_COMP_CLS_INIT,  \
        .cond_id = _cond_id, .suffix = _suffix, .func = {                                          \
            .run_in_comp_cls_init = _func,                                                         \
        }                                                                                          \
    }

void condMain(int argc, const char **argv, bt2s::span<const cond_trigger> triggers) noexcept;

#endif /* TESTS_LIB_CONDS_UTILS_H */
