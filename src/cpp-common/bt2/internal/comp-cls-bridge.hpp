/*
 * Copyright (c) 2024 EfficiOS, Inc.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_INTERNAL_COMP_CLS_BRIDGE_HPP
#define BABELTRACE_CPP_COMMON_BT2_INTERNAL_COMP_CLS_BRIDGE_HPP

#include <babeltrace2/babeltrace.h>

#include "cpp-common/bt2c/c-string-view.hpp"
#include "logging/log-api.h"

#include "../wrap.hpp"

namespace bt2 {
namespace internal {

constexpr bt2c::CStringView unhandledExcLogStr() noexcept
{
    return "Unhandled exception.";
}

constexpr bt2c::CStringView unhandledExcLogTag() noexcept
{
    return "PLUGIN-DEV-HPP";
}

/*
 * Base class of any component class bridge.
 *
 * `UserCompClsT` is the actual C++ user component class and `LibTypesT`
 * is a structure offering the following specific library types:
 *
 * `SelfCompCls`:
 *     Self component class.
 *
 * `SelfComp`:
 *     Self component.
 *
 * `SelfCompCfg`:
 *     Self component configuration.
 */
template <typename UserCompClsT, typename LibTypesT>
class CompClsBridge
{
private:
    using _LibSelfCompPtr = typename LibTypesT::SelfComp *;

public:
    static UserCompClsT& userCompFromLibSelfCompPtr(const _LibSelfCompPtr libSelfCompPtr) noexcept
    {
        return wrap(libSelfCompPtr).template data<UserCompClsT>();
    }

    static bt_component_class_initialize_method_status init(const _LibSelfCompPtr libSelfCompPtr,
                                                            typename LibTypesT::SelfCompCfg *,
                                                            const bt_value * const libParamsPtr,
                                                            void * const initData) noexcept
    {
        const auto selfComp = wrap(libSelfCompPtr);

        try {
            const auto comp =
                new UserCompClsT {selfComp, wrap(libParamsPtr).asMap(),
                                  static_cast<typename UserCompClsT::InitData *>(initData)};

            selfComp.data(*comp);
        } catch (const std::bad_alloc&) {
            return BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR;
        } catch (const Error&) {
            return BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
        } catch (...) {
            BT_LOG_WRITE_CUR_LVL(BT_LOG_WARNING, static_cast<int>(selfComp.loggingLevel()),
                                 unhandledExcLogTag(), unhandledExcLogStr());
            return BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
        }

        return BT_COMPONENT_CLASS_INITIALIZE_METHOD_STATUS_OK;
    }

    static void finalize(const _LibSelfCompPtr libSelfCompPtr) noexcept
    {
        delete &userCompFromLibSelfCompPtr(libSelfCompPtr);
    }

    static bt_component_class_get_supported_mip_versions_method_status
    getSupportedMipVersions(typename LibTypesT::SelfCompCls * const libSelfCompClsPtr,
                            const bt_value * const libParamsPtr, void *,
                            const bt_logging_level logLevel,
                            bt_integer_range_set_unsigned * const libSupportedVersionsPtr) noexcept
    {
        try {
            UserCompClsT::getSupportedMipVersions(
                wrap(libSelfCompClsPtr), wrap(libParamsPtr).asMap(),
                static_cast<LoggingLevel>(logLevel), wrap(libSupportedVersionsPtr));
            return BT_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD_STATUS_OK;
        } catch (const std::bad_alloc&) {
            return BT_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD_STATUS_MEMORY_ERROR;
        } catch (const Error&) {
            return BT_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD_STATUS_ERROR;
        } catch (...) {
            BT_LOG_WRITE_CUR_LVL(BT_LOG_WARNING, static_cast<int>(logLevel), unhandledExcLogTag(),
                                 unhandledExcLogStr());
            return BT_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD_STATUS_ERROR;
        }
    }

    static bt_component_class_query_method_status
    query(typename LibTypesT::SelfCompCls * const libSelfCompClsPtr,
          bt_private_query_executor * const libPrivQueryExecPtr, const char * const object,
          const bt_value * const libParamsPtr, void * const data,
          const bt_value ** const libResultPtr) noexcept
    {
        const auto privQueryExec = wrap(libPrivQueryExecPtr);

        try {
            auto result = UserCompClsT::query(
                wrap(libSelfCompClsPtr), privQueryExec, object, wrap(libParamsPtr),
                static_cast<typename UserCompClsT::QueryData *>(data));

            *libResultPtr = result.release().libObjPtr();
            return BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_OK;
        } catch (const TryAgain&) {
            return BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_AGAIN;
        } catch (const UnknownObject&) {
            return BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_UNKNOWN_OBJECT;
        } catch (const std::bad_alloc&) {
            return BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_MEMORY_ERROR;
        } catch (const Error&) {
            return BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_ERROR;
        } catch (...) {
            BT_LOG_WRITE_CUR_LVL(BT_LOG_WARNING, static_cast<int>(privQueryExec.loggingLevel()),
                                 unhandledExcLogTag(), unhandledExcLogStr());
            return BT_COMPONENT_CLASS_QUERY_METHOD_STATUS_ERROR;
        }
    }
};

template <typename SpecCompClsBridgeT, typename LibTypesT>
struct CompClsBridgeWithInputPorts
{
    static bt_component_class_port_connected_method_status
    inputPortConnected(typename LibTypesT::SelfComp * const libSelfCompPtr,
                       bt_self_component_port_input * const libSelfCompPortPtr,
                       const bt_port_output * const libOtherPortPtr) noexcept
    {
        try {
            SpecCompClsBridgeT::userCompFromLibSelfCompPtr(libSelfCompPtr)
                .inputPortConnected(wrap(libSelfCompPortPtr), wrap(libOtherPortPtr));
        } catch (const std::bad_alloc&) {
            return BT_COMPONENT_CLASS_PORT_CONNECTED_METHOD_STATUS_MEMORY_ERROR;
        } catch (const Error&) {
            return BT_COMPONENT_CLASS_PORT_CONNECTED_METHOD_STATUS_ERROR;
        } catch (...) {
            BT_LOG_WRITE_CUR_LVL(BT_LOG_WARNING,
                                 static_cast<int>(wrap(libSelfCompPtr).loggingLevel()),
                                 unhandledExcLogTag(), unhandledExcLogStr());
            return BT_COMPONENT_CLASS_PORT_CONNECTED_METHOD_STATUS_ERROR;
        }

        return BT_COMPONENT_CLASS_PORT_CONNECTED_METHOD_STATUS_OK;
    }
};

template <typename SpecCompClsBridgeT, typename LibTypesT>
struct CompClsBridgeWithOutputPorts
{
    static bt_component_class_port_connected_method_status
    outputPortConnected(typename LibTypesT::SelfComp * const libSelfCompPtr,
                        bt_self_component_port_output * const libSelfCompPortPtr,
                        const bt_port_input * const libOtherPortPtr) noexcept
    {
        try {
            SpecCompClsBridgeT::userCompFromLibSelfCompPtr(libSelfCompPtr)
                .outputPortConnected(wrap(libSelfCompPortPtr), wrap(libOtherPortPtr));
        } catch (const std::bad_alloc&) {
            return BT_COMPONENT_CLASS_PORT_CONNECTED_METHOD_STATUS_MEMORY_ERROR;
        } catch (const Error&) {
            return BT_COMPONENT_CLASS_PORT_CONNECTED_METHOD_STATUS_ERROR;
        } catch (...) {
            BT_LOG_WRITE_CUR_LVL(BT_LOG_WARNING,
                                 static_cast<int>(wrap(libSelfCompPtr).loggingLevel()),
                                 unhandledExcLogTag(), unhandledExcLogStr());
            return BT_COMPONENT_CLASS_PORT_CONNECTED_METHOD_STATUS_ERROR;
        }

        return BT_COMPONENT_CLASS_PORT_CONNECTED_METHOD_STATUS_OK;
    }
};

struct SrcCompClsLibTypes final
{
    using SelfCompCls = bt_self_component_class_source;
    using SelfComp = bt_self_component_source;
    using SelfCompCfg = bt_self_component_source_configuration;
};

template <typename UserCompClsT>
class SrcCompClsBridge final :
    public CompClsBridge<UserCompClsT, SrcCompClsLibTypes>,
    public CompClsBridgeWithOutputPorts<SrcCompClsBridge<UserCompClsT>, SrcCompClsLibTypes>
{
};

struct FltCompClsLibTypes final
{
    using SelfCompCls = bt_self_component_class_filter;
    using SelfComp = bt_self_component_filter;
    using SelfCompCfg = bt_self_component_filter_configuration;
};

template <typename UserCompClsT>
class FltCompClsBridge final :
    public CompClsBridge<UserCompClsT, FltCompClsLibTypes>,
    public CompClsBridgeWithInputPorts<FltCompClsBridge<UserCompClsT>, FltCompClsLibTypes>,
    public CompClsBridgeWithOutputPorts<FltCompClsBridge<UserCompClsT>, FltCompClsLibTypes>
{
};

struct SinkCompClsLibTypes final
{
    using SelfCompCls = bt_self_component_class_sink;
    using SelfComp = bt_self_component_sink;
    using SelfCompCfg = bt_self_component_sink_configuration;
};

template <typename UserCompClsT>
class SinkCompClsBridge final :
    public CompClsBridge<UserCompClsT, SinkCompClsLibTypes>,
    public CompClsBridgeWithInputPorts<SinkCompClsBridge<UserCompClsT>, SinkCompClsLibTypes>
{
public:
    using CompClsBridge<UserCompClsT, SinkCompClsLibTypes>::userCompFromLibSelfCompPtr;

    static bt_component_class_sink_consume_method_status
    consume(bt_self_component_sink * const libSelfCompPtr) noexcept
    {
        try {
            if (userCompFromLibSelfCompPtr(libSelfCompPtr).consume()) {
                return BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_OK;
            } else {
                return BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_END;
            }
        } catch (const TryAgain&) {
            return BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_AGAIN;
        } catch (const std::bad_alloc&) {
            return BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_MEMORY_ERROR;
        } catch (const Error&) {
            return BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_ERROR;
        } catch (...) {
            BT_LOG_WRITE_CUR_LVL(BT_LOG_WARNING,
                                 static_cast<int>(wrap(libSelfCompPtr).loggingLevel()),
                                 unhandledExcLogTag(), unhandledExcLogStr());
            return BT_COMPONENT_CLASS_SINK_CONSUME_METHOD_STATUS_ERROR;
        }
    }

    static bt_component_class_sink_graph_is_configured_method_status
    graphIsConfigured(bt_self_component_sink * const libSelfCompPtr) noexcept
    {
        try {
            userCompFromLibSelfCompPtr(libSelfCompPtr).graphIsConfigured();
            return BT_COMPONENT_CLASS_SINK_GRAPH_IS_CONFIGURED_METHOD_STATUS_OK;
        } catch (const std::bad_alloc&) {
            return BT_COMPONENT_CLASS_SINK_GRAPH_IS_CONFIGURED_METHOD_STATUS_MEMORY_ERROR;
        } catch (const Error&) {
            return BT_COMPONENT_CLASS_SINK_GRAPH_IS_CONFIGURED_METHOD_STATUS_ERROR;
        } catch (...) {
            BT_LOG_WRITE_CUR_LVL(BT_LOG_WARNING,
                                 static_cast<int>(wrap(libSelfCompPtr).loggingLevel()),
                                 unhandledExcLogTag(), unhandledExcLogStr());
            return BT_COMPONENT_CLASS_SINK_GRAPH_IS_CONFIGURED_METHOD_STATUS_ERROR;
        }
    }
};

template <typename UserMsgIterT>
class MsgIterClsBridge final
{
public:
    static UserMsgIterT&
    userMsgIterFromLibSelfMsgIterPtr(bt_self_message_iterator * const libSelfMsgIterPtr) noexcept
    {
        return wrap(libSelfMsgIterPtr).data<UserMsgIterT>();
    }

    static bt_message_iterator_class_initialize_method_status
    init(bt_self_message_iterator * const libSelfMsgIterPtr,
         bt_self_message_iterator_configuration * const libSelfMsgIterConfigPtr,
         bt_self_component_port_output * const libSelfCompPortPtr) noexcept
    {
        const auto selfMsgIter = wrap(libSelfMsgIterPtr);

        try {
            const auto msgIter = new UserMsgIterT {selfMsgIter, wrap(libSelfMsgIterConfigPtr),
                                                   wrap(libSelfCompPortPtr)};

            selfMsgIter.data(*msgIter);
        } catch (const std::bad_alloc&) {
            return BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_MEMORY_ERROR;
        } catch (const Error&) {
            return BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
        } catch (...) {
            BT_LOG_WRITE_CUR_LVL(
                BT_LOG_WARNING,
                static_cast<int>(wrap(libSelfMsgIterPtr).component().loggingLevel()),
                unhandledExcLogTag(), unhandledExcLogStr());
            return BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_ERROR;
        }

        return BT_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_STATUS_OK;
    }

    static void finalize(bt_self_message_iterator * const libSelfMsgIterPtr) noexcept
    {
        delete &userMsgIterFromLibSelfMsgIterPtr(libSelfMsgIterPtr);
    }

    static bt_message_iterator_class_next_method_status
    next(bt_self_message_iterator * const libSelfMsgIterPtr, bt_message_array_const libMsgsPtr,
         const uint64_t capacity, uint64_t * const count) noexcept
    {
        try {
            auto msgArray = ConstMessageArray::wrapEmpty(libMsgsPtr, capacity);
            auto& msgIter = userMsgIterFromLibSelfMsgIterPtr(libSelfMsgIterPtr);

            msgIter.next(msgArray);
            *count = msgArray.release();

            if (G_LIKELY(*count > 0)) {
                return BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_OK;
            } else {
                return BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_END;
            }
        } catch (const TryAgain&) {
            return BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_AGAIN;
        } catch (const std::bad_alloc&) {
            return BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_MEMORY_ERROR;
        } catch (const Error&) {
            return BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR;
        } catch (...) {
            BT_LOG_WRITE_CUR_LVL(
                BT_LOG_WARNING,
                static_cast<int>(wrap(libSelfMsgIterPtr).component().loggingLevel()),
                unhandledExcLogTag(), unhandledExcLogStr());
            return BT_MESSAGE_ITERATOR_CLASS_NEXT_METHOD_STATUS_ERROR;
        }
    }

    static bt_message_iterator_class_can_seek_beginning_method_status
    canSeekBeginning(bt_self_message_iterator * const libSelfMsgIterPtr,
                     bt_bool * const canSeek) noexcept
    {
        try {
            *canSeek = static_cast<bt_bool>(
                userMsgIterFromLibSelfMsgIterPtr(libSelfMsgIterPtr).canSeekBeginning());
        } catch (const TryAgain&) {
            return BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_BEGINNING_METHOD_STATUS_AGAIN;
        } catch (const std::bad_alloc&) {
            return BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_BEGINNING_METHOD_STATUS_MEMORY_ERROR;
        } catch (const Error&) {
            return BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_BEGINNING_METHOD_STATUS_ERROR;
        } catch (...) {
            BT_LOG_WRITE_CUR_LVL(
                BT_LOG_WARNING,
                static_cast<int>(wrap(libSelfMsgIterPtr).component().loggingLevel()),
                unhandledExcLogTag(), unhandledExcLogStr());
            return BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_BEGINNING_METHOD_STATUS_ERROR;
        }

        return BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_BEGINNING_METHOD_STATUS_OK;
    }

    static bt_message_iterator_class_seek_beginning_method_status
    seekBeginning(bt_self_message_iterator * const libSelfMsgIterPtr) noexcept
    {
        try {
            userMsgIterFromLibSelfMsgIterPtr(libSelfMsgIterPtr).seekBeginning();
        } catch (const TryAgain&) {
            return BT_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHOD_STATUS_AGAIN;
        } catch (const std::bad_alloc&) {
            return BT_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHOD_STATUS_MEMORY_ERROR;
        } catch (const Error&) {
            return BT_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHOD_STATUS_ERROR;
        } catch (...) {
            BT_LOG_WRITE_CUR_LVL(
                BT_LOG_WARNING,
                static_cast<int>(wrap(libSelfMsgIterPtr).component().loggingLevel()),
                unhandledExcLogTag(), unhandledExcLogStr());
            return BT_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHOD_STATUS_ERROR;
        }

        return BT_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHOD_STATUS_OK;
    }

    static bt_message_iterator_class_can_seek_ns_from_origin_method_status
    canSeekNsFromOrigin(bt_self_message_iterator * const libSelfMsgIterPtr,
                        const std::int64_t nsFromOrigin, bt_bool * const canSeek) noexcept
    {
        try {
            *canSeek = static_cast<bt_bool>(userMsgIterFromLibSelfMsgIterPtr(libSelfMsgIterPtr)
                                                .canSeekNsFromOrigin(nsFromOrigin));
        } catch (const TryAgain&) {
            return BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_AGAIN;
        } catch (const std::bad_alloc&) {
            return BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_MEMORY_ERROR;
        } catch (const Error&) {
            return BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_ERROR;
        } catch (...) {
            BT_LOG_WRITE_CUR_LVL(
                BT_LOG_WARNING,
                static_cast<int>(wrap(libSelfMsgIterPtr).component().loggingLevel()),
                unhandledExcLogTag(), unhandledExcLogStr());
            return BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_ERROR;
        }

        return BT_MESSAGE_ITERATOR_CLASS_CAN_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_OK;
    }

    static bt_message_iterator_class_seek_ns_from_origin_method_status
    seekNsFromOrigin(bt_self_message_iterator * const libSelfMsgIterPtr,
                     const std::int64_t nsFromOrigin) noexcept
    {
        try {
            userMsgIterFromLibSelfMsgIterPtr(libSelfMsgIterPtr).seekNsFromOrigin(nsFromOrigin);
        } catch (const TryAgain&) {
            return BT_MESSAGE_ITERATOR_CLASS_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_AGAIN;
        } catch (const std::bad_alloc&) {
            return BT_MESSAGE_ITERATOR_CLASS_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_MEMORY_ERROR;
        } catch (const Error&) {
            return BT_MESSAGE_ITERATOR_CLASS_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_ERROR;
        } catch (...) {
            BT_LOG_WRITE_CUR_LVL(
                BT_LOG_WARNING,
                static_cast<int>(wrap(libSelfMsgIterPtr).component().loggingLevel()),
                unhandledExcLogTag(), unhandledExcLogStr());
            return BT_MESSAGE_ITERATOR_CLASS_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_ERROR;
        }

        return BT_MESSAGE_ITERATOR_CLASS_SEEK_NS_FROM_ORIGIN_METHOD_STATUS_OK;
    }
};

} /* namespace internal */
} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_INTERNAL_COMP_CLS_BRIDGE_HPP */
