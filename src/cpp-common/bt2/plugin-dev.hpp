/*
 * Copyright (c) 2023 Simon Marchi <simon.marchi@efficios.com>
 * Copyright (c) 2023 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_PLUGIN_DEV_HPP
#define BABELTRACE_CPP_COMMON_BT2_PLUGIN_DEV_HPP

#include <babeltrace2/babeltrace.h>

#include "internal/comp-cls-bridge.hpp" /* IWYU pragma: keep */

#define BT_CPP_PLUGIN_SOURCE_COMPONENT_CLASS_WITH_ID(_pluginId, _componentClassId, _name,          \
                                                     _userComponentClass)                          \
    BT_PLUGIN_SOURCE_COMPONENT_CLASS_WITH_ID(                                                      \
        _pluginId, _componentClassId, _name,                                                       \
        bt2::internal::MsgIterClsBridge<_userComponentClass::MessageIterator>::next);              \
    BT_PLUGIN_SOURCE_COMPONENT_CLASS_INITIALIZE_METHOD_WITH_ID(                                    \
        _pluginId, _componentClassId, bt2::internal::SrcCompClsBridge<_userComponentClass>::init); \
    BT_PLUGIN_SOURCE_COMPONENT_CLASS_FINALIZE_METHOD_WITH_ID(                                      \
        _pluginId, _componentClassId,                                                              \
        bt2::internal::SrcCompClsBridge<_userComponentClass>::finalize);                           \
    BT_PLUGIN_SOURCE_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD_WITH_ID(                    \
        _pluginId, _componentClassId,                                                              \
        bt2::internal::SrcCompClsBridge<_userComponentClass>::getSupportedMipVersions);            \
    BT_PLUGIN_SOURCE_COMPONENT_CLASS_OUTPUT_PORT_CONNECTED_METHOD_WITH_ID(                         \
        _pluginId, _componentClassId,                                                              \
        bt2::internal::SrcCompClsBridge<_userComponentClass>::outputPortConnected);                \
    BT_PLUGIN_SOURCE_COMPONENT_CLASS_QUERY_METHOD_WITH_ID(                                         \
        _pluginId, _componentClassId,                                                              \
        bt2::internal::SrcCompClsBridge<_userComponentClass>::query);                              \
    BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_WITH_ID(             \
        _pluginId, _componentClassId,                                                              \
        bt2::internal::MsgIterClsBridge<_userComponentClass::MessageIterator>::init);              \
    BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_FINALIZE_METHOD_WITH_ID(               \
        _pluginId, _componentClassId,                                                              \
        bt2::internal::MsgIterClsBridge<_userComponentClass::MessageIterator>::finalize);          \
    BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHODS_WITH_ID(        \
        _pluginId, _componentClassId,                                                              \
        bt2::internal::MsgIterClsBridge<_userComponentClass::MessageIterator>::seekBeginning,      \
        bt2::internal::MsgIterClsBridge<_userComponentClass::MessageIterator>::canSeekBeginning);  \
    BT_PLUGIN_SOURCE_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_SEEK_NS_FROM_ORIGIN_METHODS_WITH_ID(   \
        _pluginId, _componentClassId,                                                              \
        bt2::internal::MsgIterClsBridge<_userComponentClass::MessageIterator>::seekNsFromOrigin,   \
        bt2::internal::MsgIterClsBridge<                                                           \
            _userComponentClass::MessageIterator>::canSeekNsFromOrigin);

#define BT_CPP_PLUGIN_FILTER_COMPONENT_CLASS_WITH_ID(_pluginId, _componentClassId, _name,          \
                                                     _userComponentClass)                          \
    BT_PLUGIN_FILTER_COMPONENT_CLASS_WITH_ID(                                                      \
        _pluginId, _componentClassId, _name,                                                       \
        bt2::internal::MsgIterClsBridge<_userComponentClass::MessageIterator>::next);              \
    BT_PLUGIN_FILTER_COMPONENT_CLASS_INITIALIZE_METHOD_WITH_ID(                                    \
        _pluginId, _componentClassId, bt2::internal::FltCompClsBridge<_userComponentClass>::init); \
    BT_PLUGIN_FILTER_COMPONENT_CLASS_FINALIZE_METHOD_WITH_ID(                                      \
        _pluginId, _componentClassId,                                                              \
        bt2::internal::FltCompClsBridge<_userComponentClass>::finalize);                           \
    BT_PLUGIN_FILTER_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD_WITH_ID(                    \
        _pluginId, _componentClassId,                                                              \
        bt2::internal::FltCompClsBridge<_userComponentClass>::getSupportedMipVersions);            \
    BT_PLUGIN_FILTER_COMPONENT_CLASS_INPUT_PORT_CONNECTED_METHOD_WITH_ID(                          \
        _pluginId, _componentClassId,                                                              \
        bt2::internal::FltCompClsBridge<_userComponentClass>::inputPortConnected);                 \
    BT_PLUGIN_FILTER_COMPONENT_CLASS_OUTPUT_PORT_CONNECTED_METHOD_WITH_ID(                         \
        _pluginId, _componentClassId,                                                              \
        bt2::internal::FltCompClsBridge<_userComponentClass>::outputPortConnected);                \
    BT_PLUGIN_FILTER_COMPONENT_CLASS_QUERY_METHOD_WITH_ID(                                         \
        _pluginId, _componentClassId,                                                              \
        bt2::internal::FltCompClsBridge<_userComponentClass>::query);                              \
    BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_INITIALIZE_METHOD_WITH_ID(             \
        _pluginId, _componentClassId,                                                              \
        bt2::internal::MsgIterClsBridge<_userComponentClass::MessageIterator>::init);              \
    BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_FINALIZE_METHOD_WITH_ID(               \
        _pluginId, _componentClassId,                                                              \
        bt2::internal::MsgIterClsBridge<_userComponentClass::MessageIterator>::finalize);          \
    BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_SEEK_BEGINNING_METHODS_WITH_ID(        \
        _pluginId, _componentClassId,                                                              \
        bt2::internal::MsgIterClsBridge<_userComponentClass::MessageIterator>::seekBeginning,      \
        bt2::internal::MsgIterClsBridge<_userComponentClass::MessageIterator>::canSeekBeginning);  \
    BT_PLUGIN_FILTER_COMPONENT_CLASS_MESSAGE_ITERATOR_CLASS_SEEK_NS_FROM_ORIGIN_METHODS_WITH_ID(   \
        _pluginId, _componentClassId,                                                              \
        bt2::internal::MsgIterClsBridge<_userComponentClass::MessageIterator>::seekNsFromOrigin,   \
        bt2::internal::MsgIterClsBridge<                                                           \
            _userComponentClass::MessageIterator>::canSeekNsFromOrigin);

#define BT_CPP_PLUGIN_SINK_COMPONENT_CLASS_WITH_ID(_pluginId, _componentClassId, _name,            \
                                                   _userComponentClass)                            \
    BT_PLUGIN_SINK_COMPONENT_CLASS_WITH_ID(                                                        \
        _pluginId, _componentClassId, _name,                                                       \
        bt2::internal::SinkCompClsBridge<_userComponentClass>::consume);                           \
    BT_PLUGIN_SINK_COMPONENT_CLASS_INITIALIZE_METHOD_WITH_ID(                                      \
        _pluginId, _componentClassId,                                                              \
        bt2::internal::SinkCompClsBridge<_userComponentClass>::init);                              \
    BT_PLUGIN_SINK_COMPONENT_CLASS_FINALIZE_METHOD_WITH_ID(                                        \
        _pluginId, _componentClassId,                                                              \
        bt2::internal::SinkCompClsBridge<_userComponentClass>::finalize);                          \
    BT_PLUGIN_SINK_COMPONENT_CLASS_GET_SUPPORTED_MIP_VERSIONS_METHOD_WITH_ID(                      \
        _pluginId, _componentClassId,                                                              \
        bt2::internal::SinkCompClsBridge<_userComponentClass>::getSupportedMipVersions);           \
    BT_PLUGIN_SINK_COMPONENT_CLASS_INPUT_PORT_CONNECTED_METHOD_WITH_ID(                            \
        _pluginId, _componentClassId,                                                              \
        bt2::internal::SinkCompClsBridge<_userComponentClass>::inputPortConnected);                \
    BT_PLUGIN_SINK_COMPONENT_CLASS_GRAPH_IS_CONFIGURED_METHOD_WITH_ID(                             \
        _pluginId, _componentClassId,                                                              \
        bt2::internal::SinkCompClsBridge<_userComponentClass>::graphIsConfigured);                 \
    BT_PLUGIN_SINK_COMPONENT_CLASS_QUERY_METHOD_WITH_ID(                                           \
        _pluginId, _componentClassId,                                                              \
        bt2::internal::SinkCompClsBridge<_userComponentClass>::query);

#define BT_CPP_PLUGIN_SOURCE_COMPONENT_CLASS(_name, _userComponentClass)                           \
    BT_CPP_PLUGIN_SOURCE_COMPONENT_CLASS_WITH_ID(auto, _name, #_name, _userComponentClass)

#define BT_CPP_PLUGIN_FILTER_COMPONENT_CLASS(_name, _userComponentClass)                           \
    BT_CPP_PLUGIN_FILTER_COMPONENT_CLASS_WITH_ID(auto, _name, #_name, _userComponentClass)

#define BT_CPP_PLUGIN_SINK_COMPONENT_CLASS(_name, _userComponentClass)                             \
    BT_CPP_PLUGIN_SINK_COMPONENT_CLASS_WITH_ID(auto, _name, #_name, _userComponentClass)

#endif /* BABELTRACE_CPP_COMMON_BT2_PLUGIN_DEV_HPP */
