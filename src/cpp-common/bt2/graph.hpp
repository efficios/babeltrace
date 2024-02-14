/*
 * Copyright (c) 2024 EfficiOS, Inc.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_GRAPH_HPP
#define BABELTRACE_CPP_COMMON_BT2_GRAPH_HPP

#include <cstdint>

#include <babeltrace2/babeltrace.h>

#include "borrowed-object.hpp"
#include "component-class.hpp"
#include "exc.hpp"
#include "shared-object.hpp"

namespace bt2 {
namespace internal {

struct GraphRefFuncs final
{
    static void get(const bt_graph * const libObjPtr) noexcept
    {
        bt_graph_get_ref(libObjPtr);
    }

    static void put(const bt_graph * const libObjPtr) noexcept
    {
        bt_graph_put_ref(libObjPtr);
    }
};

} /* namespace internal */

class Graph final : public BorrowedObject<bt_graph>
{
public:
    using Shared = SharedObject<Graph, bt_graph, internal::GraphRefFuncs>;

    explicit Graph(const LibObjPtr libObjPtr) noexcept : _ThisBorrowedObject {libObjPtr}
    {
    }

    static Shared create(const std::uint64_t mipVersion)
    {
        const auto libObjPtr = bt_graph_create(mipVersion);

        if (!libObjPtr) {
            throw MemoryError {};
        }

        return Shared::createWithoutRef(libObjPtr);
    }

    ConstSourceComponent addComponent(const ConstSourceComponentClass componentClass,
                                      const bt2c::CStringView name,
                                      const OptionalBorrowedObject<ConstMapValue> params = {},
                                      const LoggingLevel loggingLevel = LoggingLevel::NONE) const
    {
        return this->_addComponent<ConstSourceComponent>(
            componentClass, name, params, static_cast<void *>(nullptr), loggingLevel,
            bt_graph_add_source_component_with_initialize_method_data);
    }

    template <typename InitDataT>
    ConstSourceComponent addComponent(const ConstSourceComponentClass componentClass,
                                      const bt2c::CStringView name, InitDataT& initData,
                                      const OptionalBorrowedObject<ConstMapValue> params = {},
                                      const LoggingLevel loggingLevel = LoggingLevel::NONE) const
    {
        return this->_addComponent<ConstSourceComponent>(
            componentClass, name, params, &initData, loggingLevel,
            bt_graph_add_source_component_with_initialize_method_data);
    }

    ConstFilterComponent addComponent(const ConstFilterComponentClass componentClass,
                                      const bt2c::CStringView name,
                                      const OptionalBorrowedObject<ConstMapValue> params = {},
                                      const LoggingLevel loggingLevel = LoggingLevel::NONE) const
    {
        return this->_addComponent<ConstFilterComponent>(
            componentClass, name, params, static_cast<void *>(nullptr), loggingLevel,
            bt_graph_add_filter_component_with_initialize_method_data);
    }

    template <typename InitDataT>
    ConstFilterComponent addComponent(const ConstFilterComponentClass componentClass,
                                      const bt2c::CStringView name, InitDataT& initData,
                                      const OptionalBorrowedObject<ConstMapValue> params = {},
                                      const LoggingLevel loggingLevel = LoggingLevel::NONE) const
    {
        return this->_addComponent<ConstFilterComponent>(
            componentClass, name, params, &initData, loggingLevel,
            bt_graph_add_filter_component_with_initialize_method_data);
    }

    ConstSinkComponent addComponent(const ConstSinkComponentClass componentClass,
                                    const bt2c::CStringView name,
                                    const OptionalBorrowedObject<ConstMapValue> params = {},
                                    const LoggingLevel loggingLevel = LoggingLevel::NONE) const
    {
        return this->_addComponent<ConstSinkComponent>(
            componentClass, name, params, static_cast<void *>(nullptr), loggingLevel,
            bt_graph_add_sink_component_with_initialize_method_data);
    }

    template <typename InitDataT>
    ConstSinkComponent addComponent(const ConstSinkComponentClass componentClass,
                                    const bt2c::CStringView name, InitDataT& initData,
                                    const OptionalBorrowedObject<ConstMapValue> params = {},
                                    const LoggingLevel loggingLevel = LoggingLevel::NONE) const
    {
        return this->_addComponent<ConstSinkComponent>(
            componentClass, name, params, &initData, loggingLevel,
            bt_graph_add_sink_component_with_initialize_method_data);
    }

    void connectPorts(const ConstOutputPort outputPort, const ConstInputPort inputPort) const
    {
        const auto status = bt_graph_connect_ports(this->libObjPtr(), outputPort.libObjPtr(),
                                                   inputPort.libObjPtr(), nullptr);

        if (status == BT_GRAPH_CONNECT_PORTS_STATUS_ERROR) {
            throw Error {};
        } else if (status == BT_GRAPH_CONNECT_PORTS_STATUS_MEMORY_ERROR) {
            throw MemoryError {};
        }
    }

    void runOnce() const
    {
        const auto status = bt_graph_run_once(this->libObjPtr());

        if (status == BT_GRAPH_RUN_ONCE_STATUS_ERROR) {
            throw Error {};
        } else if (status == BT_GRAPH_RUN_ONCE_STATUS_MEMORY_ERROR) {
            throw MemoryError {};
        } else if (status == BT_GRAPH_RUN_ONCE_STATUS_AGAIN) {
            throw TryAgain {};
        }
    }

    void run() const
    {
        const auto status = bt_graph_run(this->libObjPtr());

        if (status == BT_GRAPH_RUN_STATUS_ERROR) {
            throw Error {};
        } else if (status == BT_GRAPH_RUN_STATUS_MEMORY_ERROR) {
            throw MemoryError {};
        } else if (status == BT_GRAPH_RUN_STATUS_AGAIN) {
            throw TryAgain {};
        }
    }

private:
    template <typename ConstComponentT, typename ConstComponentClassT, typename InitDataT,
              typename AddFuncT>
    ConstComponentT
    _addComponent(const ConstComponentClassT componentClass, const bt2c::CStringView name,
                  const OptionalBorrowedObject<ConstMapValue> params, InitDataT * const initData,
                  const LoggingLevel loggingLevel, AddFuncT&& addFunc) const
    {
        typename ConstComponentT::LibObjPtr libObjPtr = nullptr;

        const auto status = addFunc(this->libObjPtr(), componentClass.libObjPtr(), name,
                                    params ? params->libObjPtr() : nullptr,
                                    const_cast<void *>(static_cast<const void *>(initData)),
                                    static_cast<bt_logging_level>(loggingLevel), &libObjPtr);

        if (status == BT_GRAPH_ADD_COMPONENT_STATUS_ERROR) {
            throw Error {};
        } else if (status == BT_GRAPH_ADD_COMPONENT_STATUS_MEMORY_ERROR) {
            throw MemoryError {};
        }

        return wrap(libObjPtr);
    }
};

} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_GRAPH_HPP */
