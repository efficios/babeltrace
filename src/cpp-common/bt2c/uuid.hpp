/*
 * SPDX-FileCopyrightText: 2020-2023 Philippe Proulx <pproulx@efficios.com>
 * SPDX-FileCopyrightText: 2022 Francis Deslauriers <francis.deslauriers@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2C_UUID_HPP
#define BABELTRACE_CPP_COMMON_BT2C_UUID_HPP

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>

#include "common/assert.h"
#include "common/uuid.h"
#include "cpp-common/bt2c/c-string-view.hpp"

namespace bt2c {

class Uuid;

/*
 * A view on existing UUID data.
 *
 * A `UuidView` object doesn't contain its UUID data: see `Uuid` for a
 * UUID data container.
 */
class UuidView final
{
public:
    using Val = std::uint8_t;
    using ConstIter = const Val *;

public:
    explicit UuidView(const Val * const uuid) noexcept : _mUuid {uuid}
    {
        BT_ASSERT_DBG(uuid);
    }

    explicit UuidView(const Uuid& uuid) noexcept;
    UuidView(const UuidView&) noexcept = default;
    UuidView& operator=(const UuidView&) noexcept = default;

    UuidView& operator=(const Val * const uuid) noexcept
    {
        _mUuid = uuid;
        return *this;
    }

    operator Uuid() const noexcept;

    std::string str() const
    {
        std::string s;

        s.resize(BT_UUID_STR_LEN);
        bt_uuid_to_str(_mUuid, &s[0]);

        return s;
    }

    bool operator==(const UuidView& other) const noexcept
    {
        return bt_uuid_compare(_mUuid, other._mUuid) == 0;
    }

    bool operator!=(const UuidView& other) const noexcept
    {
        return !(*this == other);
    }

    bool operator<(const UuidView& other) const noexcept
    {
        return bt_uuid_compare(_mUuid, other._mUuid) < 0;
    }

    static constexpr std::size_t size() noexcept
    {
        return BT_UUID_LEN;
    }

    const Val *data() const noexcept
    {
        return _mUuid;
    }

    Val operator[](const std::size_t index) const noexcept
    {
        return _mUuid[index];
    }

    ConstIter begin() const noexcept
    {
        return _mUuid;
    }

    ConstIter end() const noexcept
    {
        return _mUuid + this->size();
    }

    bool isNil() const noexcept
    {
        return std::all_of(this->begin(), this->end(), [](const std::uint8_t byte) {
            return byte == 0;
        });
    }

private:
    const Val *_mUuid;
};

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

    explicit Uuid(const bt2c::CStringView str) noexcept
    {
        const auto ret = bt_uuid_from_str(str.data(), _mUuid.data());
        BT_ASSERT(ret == 0);
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

inline UuidView::UuidView(const Uuid& uuid) noexcept : _mUuid {uuid.data()}
{
}

inline UuidView::operator Uuid() const noexcept
{
    return Uuid {*this};
}

} /* namespace bt2c */

#endif /* BABELTRACE_CPP_COMMON_BT2C_UUID_HPP */
