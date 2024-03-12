/*
 * Copyright (c) 2024 EfficiOS, Inc.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_COMPONENT_CLASS_HPP
#define BABELTRACE_CPP_COMMON_BT2_COMPONENT_CLASS_HPP

#include <babeltrace2/babeltrace.h>

#include "cpp-common/bt2c/c-string-view.hpp"

#include "borrowed-object.hpp"
#include "component-class-dev.hpp"
#include "shared-object.hpp"

namespace bt2 {
namespace internal {

struct ComponentClassRefFuncs final
{
    static void get(const bt_component_class * const libObjPtr) noexcept
    {
        bt_component_class_get_ref(libObjPtr);
    }

    static void put(const bt_component_class * const libObjPtr) noexcept
    {
        bt_component_class_put_ref(libObjPtr);
    }
};

} /* namespace internal */

template <typename LibObjT>
class CommonSourceComponentClass;

template <typename LibObjT>
class CommonFilterComponentClass;

template <typename LibObjT>
class CommonSinkComponentClass;

template <typename LibObjT>
class CommonComponentClass : public BorrowedObject<LibObjT>
{
private:
    using _ThisBorrowedObject = BorrowedObject<LibObjT>;

public:
    using typename _ThisBorrowedObject::LibObjPtr;
    using Shared = SharedObject<CommonComponentClass, LibObjT, internal::ComponentClassRefFuncs>;

    enum class Type
    {
        SOURCE = BT_COMPONENT_CLASS_TYPE_SOURCE,
        FILTER = BT_COMPONENT_CLASS_TYPE_FILTER,
        SINK = BT_COMPONENT_CLASS_TYPE_SINK,
    };

    explicit CommonComponentClass(const LibObjPtr libObjPtr) noexcept :
        _ThisBorrowedObject {libObjPtr}
    {
    }

    template <typename OtherLibObjT>
    CommonComponentClass(const CommonComponentClass<OtherLibObjT> compCls) noexcept :
        _ThisBorrowedObject {compCls}
    {
    }

    template <typename OtherLibObjT>
    CommonComponentClass operator=(const CommonComponentClass<OtherLibObjT> compCls) noexcept
    {
        _ThisBorrowedObject::operator=(compCls);
        return *this;
    }

    /* Not `explicit` to make them behave like copy constructors */
    CommonComponentClass(
        const CommonSourceComponentClass<const bt_component_class_source> other) noexcept;
    CommonComponentClass(
        const CommonSourceComponentClass<bt_component_class_source> other) noexcept;
    CommonComponentClass(
        const CommonFilterComponentClass<const bt_component_class_filter> other) noexcept;
    CommonComponentClass(
        const CommonFilterComponentClass<bt_component_class_filter> other) noexcept;
    CommonComponentClass(
        const CommonSinkComponentClass<const bt_component_class_sink> other) noexcept;
    CommonComponentClass(const CommonSinkComponentClass<bt_component_class_sink> other) noexcept;

    CommonComponentClass
    operator=(CommonSourceComponentClass<const bt_component_class_source> other) noexcept;
    CommonComponentClass
    operator=(CommonSourceComponentClass<bt_component_class_source> other) noexcept;
    CommonComponentClass
    operator=(CommonFilterComponentClass<const bt_component_class_filter> other) noexcept;
    CommonComponentClass
    operator=(CommonFilterComponentClass<bt_component_class_filter> other) noexcept;
    CommonComponentClass
    operator=(CommonSinkComponentClass<const bt_component_class_sink> other) noexcept;
    CommonComponentClass
    operator=(CommonSinkComponentClass<bt_component_class_sink> other) noexcept;

    bool isSource() const noexcept
    {
        return static_cast<bool>(bt_component_class_is_source(this->libObjPtr()));
    }

    bool isFilter() const noexcept
    {
        return static_cast<bool>(bt_component_class_is_filter(this->libObjPtr()));
    }

    bool isSink() const noexcept
    {
        return static_cast<bool>(bt_component_class_is_sink(this->libObjPtr()));
    }

    bt2c::CStringView name() const noexcept
    {
        return bt_component_class_get_name(this->libObjPtr());
    }

    bt2c::CStringView description() const noexcept
    {
        return bt_component_class_get_description(this->libObjPtr());
    }

    bt2c::CStringView help() const noexcept
    {
        return bt_component_class_get_help(this->libObjPtr());
    }
};

using ComponentClass = CommonComponentClass<bt_component_class>;
using ConstComponentClass = CommonComponentClass<const bt_component_class>;

namespace internal {

struct SourceComponentClassRefFuncs final
{
    static void get(const bt_component_class_source * const libObjPtr) noexcept
    {
        bt_component_class_source_get_ref(libObjPtr);
    }

    static void put(const bt_component_class_source * const libObjPtr) noexcept
    {
        bt_component_class_source_put_ref(libObjPtr);
    }
};

} /* namespace internal */

template <typename LibObjT>
class CommonSourceComponentClass final : public BorrowedObject<LibObjT>
{
private:
    using _ThisBorrowedObject = BorrowedObject<LibObjT>;

public:
    using typename _ThisBorrowedObject::LibObjPtr;
    using Shared =
        SharedObject<CommonSourceComponentClass, LibObjT, internal::SourceComponentClassRefFuncs>;

    CommonSourceComponentClass(const LibObjPtr libObjPtr) noexcept : _ThisBorrowedObject {libObjPtr}
    {
    }

    template <typename OtherLibObjT>
    CommonSourceComponentClass(const CommonSourceComponentClass<OtherLibObjT> compCls) noexcept :
        _ThisBorrowedObject {compCls}
    {
    }

    template <typename OtherLibObjT>
    CommonSourceComponentClass
    operator=(const CommonSourceComponentClass<OtherLibObjT> compCls) noexcept
    {
        _ThisBorrowedObject::operator=(compCls);
        return *this;
    }

    template <typename UserComponentT>
    static CommonSourceComponentClass<LibObjT>::Shared create()
    {
        return CommonSourceComponentClass::Shared::createWithoutRef(
            internal::createSourceCompCls<UserComponentT>());
    }

    bt2c::CStringView name() const noexcept
    {
        return this->_constComponentClass().name();
    }

    bt2c::CStringView description() const noexcept
    {
        return this->_constComponentClass().description();
    }

    bt2c::CStringView help() const noexcept
    {
        return this->_constComponentClass().help();
    }

private:
    ConstComponentClass _constComponentClass() const noexcept
    {
        return ConstComponentClass {
            bt_component_class_source_as_component_class_const(this->libObjPtr())};
    }
};

template <typename LibObjT>
CommonComponentClass<LibObjT>::CommonComponentClass(
    const CommonSourceComponentClass<const bt_component_class_source> other) noexcept :
    _ThisBorrowedObject {bt_component_class_source_as_component_class_const(other.libObjPtr())}
{
}

template <typename LibObjT>
CommonComponentClass<LibObjT>::CommonComponentClass(
    const CommonSourceComponentClass<bt_component_class_source> other) noexcept :
    _ThisBorrowedObject {bt_component_class_source_as_component_class(other.libObjPtr())}
{
}

template <typename LibObjT>
CommonComponentClass<LibObjT> CommonComponentClass<LibObjT>::operator=(
    const CommonSourceComponentClass<const bt_component_class_source> other) noexcept
{
    BorrowedObject<LibObjT>::operator=(ConstComponentClass {
        bt_component_class_source_as_component_class_const(other.libObjPtr())});
    return *this;
}

template <typename LibObjT>
CommonComponentClass<LibObjT> CommonComponentClass<LibObjT>::operator=(
    const CommonSourceComponentClass<bt_component_class_source> other) noexcept
{
    BorrowedObject<LibObjT>::operator=(
        ComponentClass {bt_component_class_source_as_component_class(other.libObjPtr())});
    return *this;
}

using SourceComponentClass = CommonSourceComponentClass<bt_component_class_source>;
using ConstSourceComponentClass = CommonSourceComponentClass<const bt_component_class_source>;

namespace internal {

struct FilterComponentClassRefFuncs final
{
    static void get(const bt_component_class_filter * const libObjPtr) noexcept
    {
        bt_component_class_filter_get_ref(libObjPtr);
    }

    static void put(const bt_component_class_filter * const libObjPtr) noexcept
    {
        bt_component_class_filter_put_ref(libObjPtr);
    }
};

} /* namespace internal */

template <typename LibObjT>
class CommonFilterComponentClass final : public BorrowedObject<LibObjT>
{
private:
    using _ThisBorrowedObject = BorrowedObject<LibObjT>;

public:
    using typename _ThisBorrowedObject::LibObjPtr;
    using Shared =
        SharedObject<CommonFilterComponentClass, LibObjT, internal::FilterComponentClassRefFuncs>;

    CommonFilterComponentClass(const LibObjPtr libObjPtr) noexcept : _ThisBorrowedObject {libObjPtr}
    {
    }

    template <typename OtherLibObjT>
    CommonFilterComponentClass(const CommonFilterComponentClass<OtherLibObjT> compCls) noexcept :
        _ThisBorrowedObject {compCls}
    {
    }

    template <typename OtherLibObjT>
    CommonFilterComponentClass
    operator=(const CommonFilterComponentClass<OtherLibObjT> compCls) noexcept
    {
        _ThisBorrowedObject::operator=(compCls);
        return *this;
    }

    template <typename UserComponentT>
    static CommonFilterComponentClass<LibObjT>::Shared create()
    {
        return CommonFilterComponentClass::Shared::createWithoutRef(
            internal::createFilterCompCls<UserComponentT>());
    }

    bt2c::CStringView name() const noexcept
    {
        return this->_constComponentClass().name();
    }

    bt2c::CStringView description() const noexcept
    {
        return this->_constComponentClass().description();
    }

    bt2c::CStringView help() const noexcept
    {
        return this->_constComponentClass().help();
    }

private:
    ConstComponentClass _constComponentClass() const noexcept
    {
        return ConstComponentClass {
            bt_component_class_filter_as_component_class_const(this->libObjPtr())};
    }
};

template <typename LibObjT>
CommonComponentClass<LibObjT>::CommonComponentClass(
    const CommonFilterComponentClass<const bt_component_class_filter> other) noexcept :
    _ThisBorrowedObject {bt_component_class_filter_as_component_class_const(other.libObjPtr())}
{
}

template <typename LibObjT>
CommonComponentClass<LibObjT>::CommonComponentClass(
    const CommonFilterComponentClass<bt_component_class_filter> other) noexcept :
    _ThisBorrowedObject {bt_component_class_filter_as_component_class(other.libObjPtr())}
{
}

template <typename LibObjT>
CommonComponentClass<LibObjT> CommonComponentClass<LibObjT>::operator=(
    const CommonFilterComponentClass<const bt_component_class_filter> other) noexcept
{
    BorrowedObject<LibObjT>::operator=(ConstComponentClass {
        bt_component_class_filter_as_component_class_const(other.libObjPtr())});
    return *this;
}

template <typename LibObjT>
CommonComponentClass<LibObjT> CommonComponentClass<LibObjT>::operator=(
    const CommonFilterComponentClass<bt_component_class_filter> other) noexcept
{
    BorrowedObject<LibObjT>::operator=(
        ComponentClass {bt_component_class_filter_as_component_class(other.libObjPtr())});
    return *this;
}

using FilterComponentClass = CommonFilterComponentClass<bt_component_class_filter>;
using ConstFilterComponentClass = CommonFilterComponentClass<const bt_component_class_filter>;

namespace internal {

struct SinkComponentClassRefFuncs final
{
    static void get(const bt_component_class_sink * const libObjPtr) noexcept
    {
        bt_component_class_sink_get_ref(libObjPtr);
    }

    static void put(const bt_component_class_sink * const libObjPtr) noexcept
    {
        bt_component_class_sink_put_ref(libObjPtr);
    }
};

} /* namespace internal */

template <typename LibObjT>
class CommonSinkComponentClass final : public BorrowedObject<LibObjT>
{
private:
    using _ThisBorrowedObject = BorrowedObject<LibObjT>;

public:
    using typename _ThisBorrowedObject::LibObjPtr;
    using Shared =
        SharedObject<CommonSinkComponentClass, LibObjT, internal::SinkComponentClassRefFuncs>;

    CommonSinkComponentClass(const LibObjPtr libObjPtr) noexcept : _ThisBorrowedObject {libObjPtr}
    {
    }

    template <typename OtherLibObjT>
    CommonSinkComponentClass(const CommonSinkComponentClass<OtherLibObjT> compCls) noexcept :
        _ThisBorrowedObject {compCls}
    {
    }

    template <typename OtherLibObjT>
    CommonSinkComponentClass
    operator=(const CommonSinkComponentClass<OtherLibObjT> compCls) noexcept
    {
        _ThisBorrowedObject::operator=(compCls);
        return *this;
    }

    template <typename UserComponentT>
    static CommonSinkComponentClass<LibObjT>::Shared create()
    {
        return CommonSinkComponentClass::Shared::createWithoutRef(
            internal::createSinkCompCls<UserComponentT>());
    }

    bt2c::CStringView name() const noexcept
    {
        return this->_constComponentClass().name();
    }

    bt2c::CStringView description() const noexcept
    {
        return this->_constComponentClass().description();
    }

    bt2c::CStringView help() const noexcept
    {
        return this->_constComponentClass().help();
    }

private:
    ConstComponentClass _constComponentClass() const noexcept
    {
        return ConstComponentClass {
            bt_component_class_sink_as_component_class_const(this->libObjPtr())};
    }
};

template <typename LibObjT>
CommonComponentClass<LibObjT>::CommonComponentClass(
    const CommonSinkComponentClass<const bt_component_class_sink> other) noexcept :
    _ThisBorrowedObject {bt_component_class_sink_as_component_class_const(other.libObjPtr())}
{
}

template <typename LibObjT>
CommonComponentClass<LibObjT>::CommonComponentClass(
    const CommonSinkComponentClass<bt_component_class_sink> other) noexcept :
    _ThisBorrowedObject {bt_component_class_sink_as_component_class(other.libObjPtr())}
{
}

template <typename LibObjT>
CommonComponentClass<LibObjT> CommonComponentClass<LibObjT>::operator=(
    const CommonSinkComponentClass<const bt_component_class_sink> other) noexcept
{
    BorrowedObject<LibObjT>::operator=(
        ConstComponentClass {bt_component_class_sink_as_component_class_const(other.libObjPtr())});
    return *this;
}

template <typename LibObjT>
CommonComponentClass<LibObjT> CommonComponentClass<LibObjT>::operator=(
    const CommonSinkComponentClass<bt_component_class_sink> other) noexcept
{
    BorrowedObject<LibObjT>::operator=(
        ComponentClass {bt_component_class_sink_as_component_class(other.libObjPtr())});
    return *this;
}

using SinkComponentClass = CommonSinkComponentClass<bt_component_class_sink>;
using ConstSinkComponentClass = CommonSinkComponentClass<const bt_component_class_sink>;

} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_COMPONENT_CLASS_HPP */
