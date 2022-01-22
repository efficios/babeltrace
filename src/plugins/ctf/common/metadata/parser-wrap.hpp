/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2019 EfficiOS Inc.
 */

#ifndef BABELTRACE_PLUGINS_CTF_COMMON_METADATA_PARSER_WRAP_H
#define BABELTRACE_PLUGINS_CTF_COMMON_METADATA_PARSER_WRAP_H

/*
 * Small wrapper around the bison-generated parser.h to conditionally define
 * YYDEBUG (and therefore the yydebug declaration).
 */

#include "logging/log.h"

#if BT_LOG_ENABLED_TRACE
#    define YYDEBUG                           1
#    define YYFPRINTF(_stream, _fmt, args...) BT_LOGT(_fmt, ##args)
#else
#    define YYDEBUG 0
#endif

#define ALLOW_INCLUDE_PARSER_H
#include "parser.hpp"
#undef ALLOW_INCLUDE_PARSER_H

#endif
