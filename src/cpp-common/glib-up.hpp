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

namespace internal {

struct GStringDeleter final
{
    void operator()(GString * const str)
    {
        g_string_free(str, TRUE);
    }
};

} /* namespace internal */

using GStringUP = std::unique_ptr<GString, internal::GStringDeleter>;

namespace internal {

struct GDirDeleter final
{
    void operator()(GDir * const dir)
    {
        g_dir_close(dir);
    }
};

} /* namespace internal */

using GDirUP = std::unique_ptr<GDir, internal::GDirDeleter>;

} /* namespace bt2_common */

#endif /* BABELTRACE_CPP_COMMON_GLIB_UP_HPP */
