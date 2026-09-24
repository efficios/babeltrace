/*
 * Copyright (c) 2026 EfficiOS, Inc.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_CONNECTION_HPP
#define BABELTRACE_CPP_COMMON_BT2_CONNECTION_HPP

#include <babeltrace2/babeltrace.h>

#include "borrowed-object.hpp"
#include "component-port.hpp"
#include "shared-object.hpp"

namespace bt2 {
namespace internal {

struct ConstConnectionRefFuncs final
{
    static void get(const bt_connection * const libObjPtr) noexcept
    {
        bt_connection_get_ref(libObjPtr);
    }

    static void put(const bt_connection * const libObjPtr) noexcept
    {
        bt_connection_put_ref(libObjPtr);
    }
};

} /* namespace internal */

class ConstConnection final : public BorrowedObject<const bt_connection>
{
public:
    using Shared =
        SharedObject<ConstConnection, const bt_connection, internal::ConstConnectionRefFuncs>;

    explicit ConstConnection(const LibObjPtr libObjPtr) noexcept
        : _ThisBorrowedObject {libObjPtr}
    {
    }

    ConstOutputPort upstreamPort() const noexcept
    {
        return ConstOutputPort {bt_connection_borrow_upstream_port_const(this->libObjPtr())};
    }

    ConstInputPort downstreamPort() const noexcept
    {
        return ConstInputPort {bt_connection_borrow_downstream_port_const(this->libObjPtr())};
    }

    Shared shared() const noexcept
    {
        return Shared::createWithRef(*this);
    }
};

} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_CONNECTION_HPP */
