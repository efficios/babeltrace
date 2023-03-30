/*
 * Copyright (c) 2022 Francis Deslauriers <francis.deslauriers@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_UUID_HPP
#define BABELTRACE_CPP_COMMON_UUID_HPP

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>

#include "common/assert.h"
#include "common/uuid.h"
#include "uuid-view.hpp"

namespace bt2_common {

/*
 * A universally unique identifier.
 *
 * A `Uuid` object contains its UUID data: see `UuidView` to have a
 * UUID view on existing UUID data.
 */
class Uuid final
{
public:
    using Val = UuidView::Val;
    using ConstIter = UuidView::ConstIter;

public:
    /*
     * Builds a nil UUID.
     */
    explicit Uuid() noexcept = default;

    explicit Uuid(const Val * const uuid) noexcept
    {
        this->_setFromPtr(uuid);
    }

    explicit Uuid(const char * const str) noexcept
    {
        const auto ret = bt_uuid_from_str(str, _mUuid.data());
        BT_ASSERT(ret == 0);
    }

    explicit Uuid(const std::string& str) noexcept : Uuid {str.c_str()}
    {
    }

    explicit Uuid(const UuidView& view) noexcept : Uuid {view.data()}
    {
    }

    Uuid(const Uuid&) noexcept = default;
    Uuid& operator=(const Uuid&) noexcept = default;

    Uuid& operator=(const Val * const uuid) noexcept
    {
        this->_setFromPtr(uuid);
        return *this;
    }

    static Uuid generate() noexcept
    {
        bt_uuid_t uuidGen;

        bt_uuid_generate(uuidGen);
        return Uuid {uuidGen};
    }

    std::string str() const
    {
        return this->_view().str();
    }

    bool operator==(const Uuid& other) const noexcept
    {
        return this->_view() == other._view();
    }

    bool operator!=(const Uuid& other) const noexcept
    {
        return this->_view() != other._view();
    }

    bool operator<(const Uuid& other) const noexcept
    {
        return this->_view() < other._view();
    }

    /*
     * The returned UUID view must not outlive the UUID object.
     */
    operator UuidView() const noexcept
    {
        return this->_view();
    }

    static constexpr std::size_t size() noexcept
    {
        return UuidView::size();
    }

    const Val *data() const noexcept
    {
        return _mUuid.data();
    }

    Val operator[](const std::size_t index) const noexcept
    {
        return this->_view()[index];
    }

    ConstIter begin() const noexcept
    {
        return this->_view().begin();
    }

    ConstIter end() const noexcept
    {
        return this->_view().end();
    }

    bool isNil() const noexcept
    {
        return this->_view().isNil();
    }

private:
    /*
     * std::copy_n() won't throw when simply copying bytes below,
     * therefore this method won't throw.
     */
    void _setFromPtr(const Val * const uuid) noexcept
    {
        BT_ASSERT(uuid);
        std::copy_n(uuid, BT_UUID_LEN, std::begin(_mUuid));
    }

    UuidView _view() const noexcept
    {
        return UuidView {_mUuid.data()};
    }

    std::array<Val, UuidView::size()> _mUuid = {};
};

} /* namespace bt2_common */

#endif /* BABELTRACE_CPP_COMMON_UUID_HPP */
