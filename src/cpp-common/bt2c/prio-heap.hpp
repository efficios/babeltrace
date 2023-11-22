/*
 * Copyright (c) 2011 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 * Copyright (c) 2023 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2C_PRIO_HEAP_HPP
#define BABELTRACE_CPP_COMMON_BT2C_PRIO_HEAP_HPP

#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

#include "common/assert.h"

namespace bt2c {

/*
 * A templated C++ version of what used to be the `bt_heap_` C API,
 * written by Mathieu Desnoyers, which implements an efficient heap data
 * structure.
 *
 * This implements a static-sized priority heap based on CLRS,
 * chapter 6.
 *
 * This version copies instances of `T` during its operations, so it's
 * best to use with small objects such as pointers, integers, and small
 * PODs.
 *
 * `T` must be default-constructible, copy-constructible, and
 * copy-assignable.
 *
 * `CompT` is the type of the callable comparator. It must be possible
 * to call an instance `comp` of `CompT` as such:
 *
 *     comp(a, b)
 *
 * `comp` accepts two different `const T&` values and returns a value
 * contextually convertible to `bool` which must be true if `a` appears
 * _after_ `b`.
 *
 * The benefit of this version over `std::priority_queue` is the
 * replaceTop() method which you can call to remove the top (greatest)
 * element and then insert a new one immediately afterwards with a
 * single heap rebalance.
 */
template <typename T, typename CompT = std::greater<T>>
class PrioHeap final
{
    static_assert(std::is_default_constructible<T>::value, "`T` is default-constructible.");
    static_assert(std::is_copy_constructible<T>::value, "`T` is copy-constructible.");
    static_assert(std::is_copy_assignable<T>::value, "`T` is copy-assignable.");

public:
    /*
     * Builds a priority heap using the comparator `comp` and with an
     * initial capacity of `cap` elements.
     */
    explicit PrioHeap(CompT comp, const std::size_t cap) : _mComp {std::move(comp)}
    {
        _mElems.reserve(cap);
    }

    /*
     * Builds a priority heap using the comparator `comp` and with an
     * initial capacity of zero.
     */
    explicit PrioHeap(CompT comp) : PrioHeap {std::move(comp), 0}
    {
    }

    /*
     * Builds a priority heap using a default comparator and with an
     * initial capacity of zero.
     */
    explicit PrioHeap() : PrioHeap {CompT {}, 0}
    {
    }

    /*
     * Number of contained elements.
     */
    std::size_t len() const noexcept
    {
        return _mElems.size();
    }

    /*
     * Whether or not this heap is empty.
     */
    bool isEmpty() const noexcept
    {
        return _mElems.empty();
    }

    /*
     * Removes all the elements.
     */
    void clear()
    {
        _mElems.clear();
    }

    /*
     * Current top (greatest) element (`const` version).
     */
    const T& top() const noexcept
    {
        BT_ASSERT_DBG(!this->isEmpty());
        this->_validate();
        return _mElems.front();
    }

    /*
     * Current top (greatest) element.
     */
    T& top() noexcept
    {
        BT_ASSERT_DBG(!this->isEmpty());
        this->_validate();
        return _mElems.front();
    }

    /*
     * Inserts a copy of the element `elem`.
     */
    void insert(const T& elem)
    {
        /* Default-construct the new one */
        _mElems.resize(_mElems.size() + 1);

        auto pos = this->len() - 1;

        while (pos > 0 && this->_gt(elem, this->_parentElem(pos))) {
            /* Move parent down until we find the right spot */
            _mElems[pos] = this->_parentElem(pos);
            pos = this->_parentPos(pos);
        }

        _mElems[pos] = elem;
        this->_validate();
    }

    /*
     * Removes the top (greatest) element.
     *
     * This heap must not be empty.
     */
    void removeTop()
    {
        BT_ASSERT_DBG(!this->isEmpty());

        if (_mElems.size() == 1) {
            /* Fast path for a single element */
            _mElems.clear();
            return;
        }

        /*
         * Shrink, replace the current top by the previous last element,
         * and heapify.
         */
        const auto lastElem = _mElems.back();

        _mElems.resize(_mElems.size() - 1);
        return this->replaceTop(lastElem);
    }

    /*
     * Removes the top (greatest) element, and inserts a copy of `elem`.
     *
     * Equivalent to using removeTop() and then insert(), but more
     * efficient (single rebalance).
     *
     * This heap must not be empty.
     */
    void replaceTop(const T& elem)
    {
        BT_ASSERT_DBG(!this->isEmpty());

        /* Replace the current top and heapify */
        _mElems[0] = elem;
        this->_heapify(0);
    }

private:
    static std::size_t _parentPos(const std::size_t pos) noexcept
    {
        return (pos - 1) >> 1;
    }

    void _heapify(std::size_t pos)
    {
        while (true) {
            std::size_t largestPos;
            const auto leftPos = (pos << 1) + 1;

            if (leftPos < this->len() && this->_gt(_mElems[leftPos], _mElems[pos])) {
                largestPos = leftPos;
            } else {
                largestPos = pos;
            }

            const auto rightPos = (pos << 1) + 2;

            if (rightPos < this->len() && this->_gt(_mElems[rightPos], _mElems[largestPos])) {
                largestPos = rightPos;
            }

            if (G_UNLIKELY(largestPos == pos)) {
                break;
            }

            const auto tmpElem = _mElems[pos];

            _mElems[pos] = _mElems[largestPos];
            _mElems[largestPos] = tmpElem;
            pos = largestPos;
        }

        this->_validate();
    }

    T& _parentElem(const std::size_t pos) noexcept
    {
        return _mElems[this->_parentPos(pos)];
    }

    bool _gt(const T& a, const T& b) const
    {
        /* Forward to user comparator */
        return _mComp(a, b);
    }

    void _validate() const noexcept
    {
#ifdef BT_DEBUG_MODE
        if (_mElems.empty()) {
            return;
        }

        for (std::size_t i = 1; i < _mElems.size(); ++i) {
            BT_ASSERT_DBG(!this->_gt(_mElems[i], _mElems.front()));
        }
#endif /* BT_DEBUG_MODE */
    }

    CompT _mComp;
    std::vector<T> _mElems;
};

} /* namespace bt2c */

#endif /* BABELTRACE_CPP_COMMON_BT2C_PRIO_HEAP_HPP */
