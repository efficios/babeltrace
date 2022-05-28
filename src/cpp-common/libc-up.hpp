/*
 * Copyright (c) 2022 EfficiOS, inc.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_LIBC_UP_HPP
#define BABELTRACE_CPP_COMMON_LIBC_UP_HPP

#include <cstdio>
#include <memory>

namespace bt2_common {
namespace internal {

struct FileCloserDeleter
{
    void operator()(std::FILE * const f) noexcept
    {
        std::fclose(f);
    }
};

} /* namespace internal */

using FileUP = std::unique_ptr<std::FILE, internal::FileCloserDeleter>;

} /* namespace bt2_common */

#endif /* BABELTRACE_CPP_COMMON_LIBC_UP_HPP */
