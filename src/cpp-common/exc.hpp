/*
 * Copyright (c) 2022 Francis Deslauriers <francis.deslauriers@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_EXC_HPP
#define BABELTRACE_CPP_COMMON_EXC_HPP

#include <exception>
#include <string>
#include <new>
#include <stdexcept>

namespace bt2_common {

/*
 * End of iteration.
 */
class End : public std::exception
{
public:
    explicit End() noexcept : std::exception {}
    {
    }
};

/*
 * General error.
 */
class Error : public std::runtime_error
{
public:
    explicit Error(std::string msg = "Error") : std::runtime_error {std::move(msg)}
    {
    }
};

/*
 * Memory error.
 */
class MemoryError : public std::bad_alloc
{
public:
    explicit MemoryError() noexcept : std::bad_alloc {}
    {
    }
};

/*
 * Not available right now: try again later.
 */
class TryAgain : public std::exception
{
public:
    explicit TryAgain() noexcept : std::exception {}
    {
    }
};

} /* namespace bt2_common */

#endif /* BABELTRACE_CPP_COMMON_EXC_HPP */
