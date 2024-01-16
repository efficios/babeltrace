/*
 * Copyright (c) 2023 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_RAW_VALUE_PROXY_HPP
#define BABELTRACE_CPP_COMMON_BT2_RAW_VALUE_PROXY_HPP

#include <string>

#include "cpp-common/bt2c/c-string-view.hpp"

namespace bt2 {

template <typename ObjT>
class RawValueProxy
{
private:
    using _RawVal = typename ObjT::Value;

public:
    explicit RawValueProxy(const ObjT obj) : _mObj {obj}
    {
    }

    RawValueProxy& operator=(const _RawVal& rawVal)
    {
        _mObj.value(rawVal);
        return *this;
    }

    operator _RawVal() const noexcept
    {
        return _mObj.value();
    }

private:
    ObjT _mObj;
};

} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_RAW_VALUE_PROXY_HPP */
