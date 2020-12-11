/*
 * Copyright (c) 2020 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_LIB_ERROR_HPP
#define BABELTRACE_CPP_COMMON_BT2_LIB_ERROR_HPP

#include <string>
#include <stdexcept>

namespace bt2 {

/*
 * Any library error.
 */
class LibError : public std::runtime_error
{
public:
    explicit LibError(const std::string& msg = "Error") : std::runtime_error {msg}
    {
    }
};

/*
 * Memory error.
 */
class LibMemoryError : public LibError
{
public:
    LibMemoryError() : LibError {"Memory error"}
    {
    }
};

/*
 * Overflow error.
 */
class LibOverflowError : public LibError
{
public:
    LibOverflowError() : LibError {"Overflow error"}
    {
    }
};

} // namespace bt2

#endif // BABELTRACE_CPP_COMMON_BT2_LIB_ERROR_HPP
