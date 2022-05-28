/*
 * Copyright (c) 2022 EfficiOS, inc.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_GLIB_UP_HPP
#define BABELTRACE_CPP_COMMON_GLIB_UP_HPP

#include <glib.h>
#include <memory>

namespace bt2_common {
namespace internal {

struct GCharDeleter final
{
    void operator()(gchar * const p) noexcept
    {
        g_free(p);
    }
};

} /* namespace internal */

using GCharUP = std::unique_ptr<gchar, internal::GCharDeleter>;

} /* namespace bt2_common */

#endif /* BABELTRACE_CPP_COMMON_GLIB_UP_HPP */
