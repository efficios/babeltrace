/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2022 Francis Deslauriers <francis.deslauriers@efficios.com>
 * Copyright (c) 2022 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_CPP_COMMON_LOG_CFG_HPP
#define BABELTRACE_CPP_COMMON_LOG_CFG_HPP

#include <babeltrace2/babeltrace.h>

#include "common/assert.h"

namespace bt2_common {

/*
 * Logging configuration.
 *
 * A logging configuration object contains all the execution context
 * needed to:
 *
 * • Log, providing the name of some current component or component
 *   class.
 *
 * • Append a cause to the error of the current thread from the right
 *   actor.
 *
 * For a given logging configuration `L`, the guarantees are as such:
 *
 * If `L.selfMsgIter()` isn't `nullptr`:
 *     • `L.selfComp()` isn't `nullptr`.
 *     • `L.compCls()` isn't `nullptr`.
 *     • `L.compName()` isn't `nullptr`.
 *     • `L.compClsName()` isn't `nullptr`.
 *     • `L.moduleName()` is `nullptr`.
 *
 * If `L.selfComp()` isn't `nullptr`:
 *     • `L.compCls()` isn't `nullptr`.
 *     • `L.compName()` isn't `nullptr`.
 *     • `L.compClsName()` isn't `nullptr`.
 *     • `L.moduleName()` is `nullptr`.
 *
 * If `L.selfCompCls()` isn't `nullptr`:
 *     • `L.compCls()` isn't `nullptr`.
 *     • `L.compClsName()` isn't `nullptr`.
 *     • `L.moduleName()` is `nullptr`.
 *
 * If `L.compCls()` isn't `nullptr`:
 *     • `L.compClsName()` isn't `nullptr`.
 *     • `L.moduleName()` is `nullptr`.
 *
 * If `L.moduleName()` isn't `nullptr`:
 *     • `L.selfMsgIter()` is `nullptr`.
 *     • `L.selfComp()` is `nullptr`.
 *     • `L.compCls()` is `nullptr`.
 *     • `L.compName()` is `nullptr`.
 *     • `L.compClsName()` is `nullptr`.
 */
class LogCfg final
{
public:
    explicit LogCfg(const bt_logging_level logLevel, bt_self_message_iterator& selfMsgIter) :
        _mLogLevel {logLevel}, _mSelfMsgIter {&selfMsgIter},
        _mSelfComp {bt_self_message_iterator_borrow_component(&selfMsgIter)},
        _mCompCls {&this->_compClsFromSelfComp(*_mSelfComp)}
    {
    }

    explicit LogCfg(const bt_logging_level logLevel, bt_self_component& selfComp) :
        _mLogLevel {logLevel}, _mSelfComp {&selfComp}, _mCompCls {
                                                           &this->_compClsFromSelfComp(*_mSelfComp)}
    {
    }

    explicit LogCfg(const bt_logging_level logLevel, bt_self_component_class& selfCompCls) :
        _mLogLevel {logLevel}, _mSelfCompCls {&selfCompCls},
        _mCompCls {bt_self_component_class_as_component_class(&selfCompCls)}
    {
    }

    explicit LogCfg(const bt_logging_level logLevel, const char * const moduleName) :
        _mLogLevel {logLevel}, _mModuleName {moduleName}
    {
        BT_ASSERT_DBG(_mModuleName);
    }

    LogCfg(const LogCfg&) = default;
    LogCfg& operator=(const LogCfg&) = default;

    bt_logging_level logLevel() const noexcept
    {
        return _mLogLevel;
    }

    bt_self_component *selfComp() const noexcept
    {
        return _mSelfComp;
    }

    const char *compName() const noexcept
    {
        BT_ASSERT_DBG(_mSelfComp);
        return bt_component_get_name(bt_self_component_as_component(_mSelfComp));
    }

    bt_self_component_class *selfCompCls() const noexcept
    {
        return _mSelfCompCls;
    }

    const bt_component_class *compCls() const noexcept
    {
        return _mCompCls;
    }

    const char *compClsName() const noexcept
    {
        BT_ASSERT_DBG(_mCompCls);
        return bt_component_class_get_name(_mCompCls);
    }

    bt_self_message_iterator *selfMsgIter() const noexcept
    {
        return _mSelfMsgIter;
    }

    const char *moduleName() const noexcept
    {
        return _mModuleName;
    }

private:
    static const bt_component_class& _compClsFromSelfComp(bt_self_component& selfComp) noexcept
    {
        return *bt_component_borrow_class_const(bt_self_component_as_component(&selfComp));
    }

    bt_logging_level _mLogLevel;
    bt_self_message_iterator *_mSelfMsgIter = nullptr;
    bt_self_component *_mSelfComp = nullptr;
    bt_self_component_class *_mSelfCompCls = nullptr;
    const bt_component_class *_mCompCls = nullptr;
    const char *_mModuleName = nullptr;
};

} /* namespace bt2_common */

#endif /* BABELTRACE_CPP_COMMON_LOG_CFG_HPP */
