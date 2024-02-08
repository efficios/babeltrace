/*
 * Copyright (c) 2024 EfficiOS, inc.
 *
 * SPDX-License-Identifier: MIT
 */

#include "common/common.h"
#include "cpp-common/bt2/message.hpp"
#include "cpp-common/vendor/fmt/format.h" /* IWYU pragma: keep */

namespace bt2 {

inline const char *format_as(const MessageType type)
{
    return bt_common_message_type_string(static_cast<bt_message_type>(type));
}

} /* namespace bt2 */
