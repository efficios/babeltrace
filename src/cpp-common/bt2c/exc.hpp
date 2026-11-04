/*
 * Copyright (c) 2022 Francis Deslauriers <francis.deslauriers@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2C_EXC_HPP
#define BABELTRACE_CPP_COMMON_BT2C_EXC_HPP

/*!
@file

@brief
    Common exception classes.

@ingroup common-cpp-bt2c

@code{.cpp}
#include "cpp-common/bt2c/exc.hpp"
@endcode
*/

#include <exception>
#include <new>
#include <stdexcept>
#include <string>

namespace bt2c {

/*! @brief %End of iteration. */
class End : public std::exception
{
public:
    explicit End() noexcept
        : std::exception {}
    {
    }
};

/*! @brief General error. */
class Error : public std::runtime_error
{
public:
    explicit Error(std::string msg = "Error")
        : std::runtime_error {std::move(msg)}
    {
    }
};

/*! @brief Overflow error. */
class OverflowError : public Error
{
public:
    /*!
    @brief
        Builds an overflow error.

    @param[in] reason
        Reason of the overflow (what() returns it).
    */
    explicit OverflowError(std::string reason = "Overflow error")
        : Error {std::move(reason)}
    {
    }
};

/*! @brief Memory error. */
class MemoryError : public std::bad_alloc
{
public:
    explicit MemoryError() noexcept
        : std::bad_alloc {}
    {
    }
};

/*! @brief Not available right now: try again later. */
class TryAgain : public std::exception
{
public:
    explicit TryAgain() noexcept
        : std::exception {}
    {
    }
};

/*! @brief No such file or directory. */
class NoSuchFileOrDirectoryError : public Error
{
public:
    explicit NoSuchFileOrDirectoryError() noexcept
        : Error {"No such file or directory"}
    {
    }
};

} /* namespace bt2c */

#endif /* BABELTRACE_CPP_COMMON_BT2C_EXC_HPP */
