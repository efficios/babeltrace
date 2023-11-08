/*
 * Copyright (c) 2023 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_PRIVATE_QUERY_EXECUTOR_HPP
#define BABELTRACE_CPP_COMMON_BT2_PRIVATE_QUERY_EXECUTOR_HPP

#include <babeltrace2/babeltrace.h>

#include "logging.hpp"

#include "borrowed-object.hpp"

namespace bt2 {

class PrivateQueryExecutor final : public BorrowedObject<bt_private_query_executor>
{
public:
    explicit PrivateQueryExecutor(const _LibObjPtr libObjPtr) noexcept :
        _ThisBorrowedObject {libObjPtr}
    {
    }

    bt2::LoggingLevel loggingLevel() const noexcept
    {
        return static_cast<bt2::LoggingLevel>(bt_query_executor_get_logging_level(
            bt_private_query_executor_as_query_executor_const(this->libObjPtr())));
    }

    bool isInterrupted() const noexcept
    {
        return static_cast<bool>(bt_query_executor_is_interrupted(
            bt_private_query_executor_as_query_executor_const(this->libObjPtr())));
    }
};

} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_PRIVATE_QUERY_EXECUTOR_HPP */
