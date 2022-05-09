/*
 * Copyright (c) 2020 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_TYPE_TRAITS_HPP
#define BABELTRACE_CPP_COMMON_BT2_TYPE_TRAITS_HPP

#include "internal/utils.hpp"

namespace bt2 {

template <typename ObjT>
struct AddConst
{
    using Type = typename internal::TypeDescr<ObjT>::Const;
};

template <typename ObjT>
struct RemoveConst
{
    using Type = typename internal::TypeDescr<ObjT>::NonConst;
};

} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_TYPE_TRAITS_HPP */
