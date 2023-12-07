/*
 * Copyright (c) 2022 Francis Deslauriers <francis.deslauriers@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_COMMON_ITER_HPP
#define BABELTRACE_CPP_COMMON_BT2_COMMON_ITER_HPP

#include <iterator>

#include "cpp-common/optional.hpp"

namespace bt2 {

template <typename ContainerT, typename ElemT>
class CommonIterator
{
    friend ContainerT;

public:
    using difference_type = std::ptrdiff_t;
    using value_type = ElemT;
    using pointer = value_type *;
    using reference = value_type&;
    using iterator_category = std::input_iterator_tag;

private:
    explicit CommonIterator(const ContainerT container, const uint64_t idx) :
        _mContainer {container}, _mIdx {idx}
    {
        this->_updateCurrentValue();
    }

public:
    CommonIterator(const CommonIterator&) = default;
    CommonIterator(CommonIterator&&) = default;
    CommonIterator& operator=(const CommonIterator&) = default;
    CommonIterator& operator=(CommonIterator&&) = default;

    CommonIterator& operator++() noexcept
    {
        ++_mIdx;
        this->_updateCurrentValue();
        return *this;
    }

    CommonIterator operator++(int) noexcept
    {
        const auto tmp = *this;

        ++(*this);
        return tmp;
    }

    bool operator==(const CommonIterator& other) const noexcept
    {
        return _mIdx == other._mIdx;
    }

    bool operator!=(const CommonIterator& other) const noexcept
    {
        return !(*this == other);
    }

    reference operator*() noexcept
    {
        return *_mCurrVal;
    }

    pointer operator->() noexcept
    {
        return &(*_mCurrVal);
    }

private:
    void _updateCurrentValue() noexcept
    {
        if (_mIdx < _mContainer.size()) {
            _mCurrVal = _mContainer[_mIdx];
        } else {
            _mCurrVal = nonstd::nullopt;
        }
    }

    nonstd::optional<value_type> _mCurrVal;
    ContainerT _mContainer;
    uint64_t _mIdx;
};
} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_COMMON_ITER_HPP */
