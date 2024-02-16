/*
 * Copyright (c) 2024 EfficiOS, Inc.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_PLUGIN_HPP
#define BABELTRACE_CPP_COMMON_BT2_PLUGIN_HPP

#include <cstdint>

#include <babeltrace2/babeltrace.h>

#include "cpp-common/bt2c/c-string-view.hpp"
#include "cpp-common/bt2s/optional.hpp"

#include "borrowed-object-iterator.hpp"
#include "borrowed-object.hpp"
#include "component-class.hpp"
#include "shared-object.hpp"

namespace bt2 {

template <typename PluginSpecCompClsT>
class ConstPluginComponentClasses final : public BorrowedObject<const bt_plugin>
{
public:
    using Iterator = BorrowedObjectIterator<ConstPluginComponentClasses<PluginSpecCompClsT>>;

    explicit ConstPluginComponentClasses(const LibObjPtr libPluginPtr) noexcept :
        _ThisBorrowedObject {libPluginPtr}
    {
    }

    std::uint64_t length() const noexcept
    {
        return PluginSpecCompClsT::getCompClsCount(this->libObjPtr());
    }

    Iterator begin() const noexcept
    {
        return Iterator {*this, 0};
    }

    Iterator end() const noexcept
    {
        return Iterator {*this, this->length()};
    }

    typename PluginSpecCompClsT::CompCls operator[](const std::uint64_t index) const noexcept
    {
        return PluginSpecCompClsT::borrowCompClsByIndex(this->libObjPtr(), index);
    }

    OptionalBorrowedObject<typename PluginSpecCompClsT::CompCls>
    operator[](const bt2c::CStringView name) const noexcept
    {
        return PluginSpecCompClsT::borrowCompClsByName(this->libObjPtr(), name);
    }
};

namespace internal {

struct PluginRefFuncs final
{
    static void get(const bt_plugin * const libObjPtr) noexcept
    {
        bt_plugin_get_ref(libObjPtr);
    }

    static void put(const bt_plugin * const libObjPtr) noexcept
    {
        bt_plugin_put_ref(libObjPtr);
    }
};

struct PluginSourceCompClsFuncs final
{
    using CompCls = ConstSourceComponentClass;

    static std::uint64_t getCompClsCount(const bt_plugin * const plugin) noexcept
    {
        return bt_plugin_get_source_component_class_count(plugin);
    }

    static const bt_component_class_source *borrowCompClsByIndex(const bt_plugin * const plugin,
                                                                 const std::uint64_t index) noexcept
    {
        return bt_plugin_borrow_source_component_class_by_index_const(plugin, index);
    }

    static const bt_component_class_source *borrowCompClsByName(const bt_plugin * const plugin,
                                                                const char *name) noexcept
    {
        return bt_plugin_borrow_source_component_class_by_name_const(plugin, name);
    }
};

struct PluginFilterCompClsFuncs final
{
    using CompCls = ConstFilterComponentClass;

    static std::uint64_t getCompClsCount(const bt_plugin * const plugin) noexcept
    {
        return bt_plugin_get_filter_component_class_count(plugin);
    }

    static const bt_component_class_filter *borrowCompClsByIndex(const bt_plugin * const plugin,
                                                                 const std::uint64_t index) noexcept
    {
        return bt_plugin_borrow_filter_component_class_by_index_const(plugin, index);
    }

    static const bt_component_class_filter *borrowCompClsByName(const bt_plugin * const plugin,
                                                                const char *name) noexcept
    {
        return bt_plugin_borrow_filter_component_class_by_name_const(plugin, name);
    }
};

struct PluginSinkCompClsFuncs final
{
    using CompCls = ConstSinkComponentClass;

    static std::uint64_t getCompClsCount(const bt_plugin * const plugin) noexcept
    {
        return bt_plugin_get_sink_component_class_count(plugin);
    }

    static const bt_component_class_sink *borrowCompClsByIndex(const bt_plugin * const plugin,
                                                               const std::uint64_t index) noexcept
    {
        return bt_plugin_borrow_sink_component_class_by_index_const(plugin, index);
    }

    static const bt_component_class_sink *borrowCompClsByName(const bt_plugin * const plugin,
                                                              const char *name) noexcept
    {
        return bt_plugin_borrow_sink_component_class_by_name_const(plugin, name);
    }
};

} /* namespace internal */

class ConstPlugin final : public BorrowedObject<const bt_plugin>
{
public:
    using SourceComponementClasses =
        ConstPluginComponentClasses<internal::PluginSourceCompClsFuncs>;
    using FilterComponementClasses =
        ConstPluginComponentClasses<internal::PluginFilterCompClsFuncs>;
    using SinkComponementClasses = ConstPluginComponentClasses<internal::PluginSinkCompClsFuncs>;

    class Version final
    {
    public:
        explicit Version(const unsigned int major, const unsigned int minor,
                         const unsigned int patch, const bt2c::CStringView extra) noexcept :
            _mMajor {major},
            _mMinor {minor}, _mPatch {patch}, _mExtra {extra}
        {
        }

        unsigned int major() const noexcept
        {
            return _mMajor;
        }

        unsigned int minor() const noexcept
        {
            return _mMinor;
        }

        unsigned int patch() const noexcept
        {
            return _mPatch;
        }

        bt2c::CStringView extra() const noexcept
        {
            return _mExtra;
        }

    private:
        unsigned int _mMajor, _mMinor, _mPatch;
        bt2c::CStringView _mExtra;
    };

    using Shared = SharedObject<ConstPlugin, const bt_plugin, internal::PluginRefFuncs>;

    explicit ConstPlugin(const LibObjPtr plugin) : _ThisBorrowedObject {plugin}
    {
    }

    bt2c::CStringView name() const noexcept
    {
        return bt_plugin_get_name(this->libObjPtr());
    }

    bt2c::CStringView description() const noexcept
    {
        return bt_plugin_get_description(this->libObjPtr());
    }

    bt2c::CStringView author() const noexcept
    {
        return bt_plugin_get_author(this->libObjPtr());
    }

    bt2c::CStringView license() const noexcept
    {
        return bt_plugin_get_license(this->libObjPtr());
    }

    bt2c::CStringView path() const noexcept
    {
        return bt_plugin_get_path(this->libObjPtr());
    }

    bt2s::optional<Version> version() const noexcept
    {
        unsigned int major, minor, patch;
        const char *extra;

        if (bt_plugin_get_version(this->libObjPtr(), &major, &minor, &patch, &extra) ==
            BT_PROPERTY_AVAILABILITY_NOT_AVAILABLE) {
            return bt2s::nullopt;
        }

        return Version {major, minor, patch, extra};
    }

    SourceComponementClasses sourceComponentClasses() const noexcept
    {
        return SourceComponementClasses {this->libObjPtr()};
    }

    FilterComponementClasses filterComponentClasses() const noexcept
    {
        return FilterComponementClasses {this->libObjPtr()};
    }

    SinkComponementClasses sinkComponentClasses() const noexcept
    {
        return SinkComponementClasses {this->libObjPtr()};
    }
};

} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_PLUGIN_HPP */
