/*
 * Copyright (c) 2022 EfficiOS, Inc.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2C_LIB_STR_HPP
#define BABELTRACE_CPP_COMMON_BT2C_LIB_STR_HPP

#include "cpp-common/bt2/message.hpp"

namespace bt2c {

static inline const char *messageTypeStr(const bt2::MessageType type)
{
    return bt_common_message_type_string(static_cast<bt_message_type>(type));
}

} /* namespace bt2c */

#endif /* BABELTRACE_CPP_COMMON_BT2C_LIB_STR_HPP */
