/*
 * Copyright (c) 2022 Francis Deslauriers <francis.deslauriers@efficios.com>
 * Copyright (c) 2023 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_BORROWED_OBJECT_ITERATOR_HPP
#define BABELTRACE_CPP_COMMON_BT2_BORROWED_OBJECT_ITERATOR_HPP

#include <cstdint>
#include <type_traits>
#include <utility>

#include "common/assert.h"

#include "borrowed-object-proxy.hpp"

namespace bt2 {

/*
 * An iterator class to iterate an instance of a borrowed object
 * container of type `ContainerT`.
 *
 * `ContainerT` must implement:
 *
 *     // Returns the number of contained borrowed objects.
 *     std::uint64_t length() const noexcept;
 *
 *     // Returns the borrowed object at index `i`.
 *     SomeObject operator[](std::uint64_t i) const noexcept;
 */
template <typename ContainerT>
class BorrowedObjectIterator final
{
    friend ContainerT;

public:
    using Object = typename std::remove_reference<
        decltype(std::declval<ContainerT>()[std::declval<std::uint64_t>()])>::type;

private:
    explicit BorrowedObjectIterator(const ContainerT container, const uint64_t idx) :
        _mContainer {container}, _mIdx {idx}
    {
    }

public:
    BorrowedObjectIterator& operator++() noexcept
    {
        ++_mIdx;
        return *this;
    }

    BorrowedObjectIterator operator++(int) noexcept
    {
        const auto tmp = *this;

        ++(*this);
        return tmp;
    }

    bool operator==(const BorrowedObjectIterator& other) const noexcept
    {
        BT_ASSERT_DBG(_mContainer.isSame(other._mContainer));
        return _mIdx == other._mIdx;
    }

    bool operator!=(const BorrowedObjectIterator& other) const noexcept
    {
        return !(*this == other);
    }

    Object operator*() const noexcept
    {
        BT_ASSERT_DBG(_mIdx < _mContainer.length());
        return _mContainer[_mIdx];
    }

    BorrowedObjectProxy<Object> operator->() const noexcept
    {
        return BorrowedObjectProxy<Object> {(**this).libObjPtr()};
    }

private:
    ContainerT _mContainer;
    std::uint64_t _mIdx;
};

} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_BORROWED_OBJECT_ITERATOR_HPP */
