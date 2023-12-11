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

template <typename ObjT>
class RawStringValueProxy final : public RawValueProxy<ObjT>
{
public:
    explicit RawStringValueProxy(const ObjT obj) : RawValueProxy<ObjT> {obj}
    {
    }

    RawStringValueProxy& operator=(const char * const rawVal)
    {
        RawValueProxy<ObjT>::operator=(bt2c::CStringView {rawVal});
        return *this;
    }

    RawStringValueProxy& operator=(const std::string& rawVal)
    {
        RawValueProxy<ObjT>::operator=(bt2c::CStringView {rawVal.data()});
        return *this;
    }
};

} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_RAW_VALUE_PROXY_HPP */
