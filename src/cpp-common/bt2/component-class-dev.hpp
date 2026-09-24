/*
 * Copyright (c) 2024 EfficiOS, Inc.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_COMPONENT_CLASS_DEV_HPP
#define BABELTRACE_CPP_COMMON_BT2_COMPONENT_CLASS_DEV_HPP

#include <cstdint>

#include <fmt/core.h>
#include <glib.h>

#include "cpp-common/bt2c/c-string-view.hpp"
#include "cpp-common/bt2c/logging.hpp"

#include "component-class.hpp"
#include "exc.hpp"
#include "internal/comp-cls-bridge.hpp"
#include "private-query-executor.hpp"
#include "self-component-port.hpp"

/*!
@file

@brief
    C++ component class development.

@ingroup common-cpp-bt2

@code{.cpp}
#include "cpp-common/bt2/component-class-dev.hpp"
@endcode

See \ref common-cpp-bt2-comp-cls-dev.
*/

namespace bt2 {

template <typename UserMessageIteratorT, typename UserComponentT>
class UserMessageIterator;

/*!
@brief
    Base class of any user component class.

See the specific #UserSourceComponent,
#UserFilterComponent, and #UserSinkComponent.

@tparam SelfCompT
    Self component wrapper type.
@tparam InitDataT
    Type of the initialization data of which your constructor
    parameter points to.
@tparam QueryDataT
    Type of the query data of which your query method
    parameter points to.
*/
template <typename SelfCompT, typename InitDataT, typename QueryDataT>
class UserComponent
{
    /* Give a related message iterator access to this logger */
    template <typename, typename>
    friend class UserMessageIterator;

public:
    /*!
    Type of the initialization data of which your constructor
    parameter points to.
    */
    using InitData = InitDataT;

    /*!
    Type of the query data of which your query method
    parameter points to.
    */
    using QueryData = QueryDataT;

    /// Default description (overridable).
    static constexpr auto description = nullptr;

    /// Default help text (overridable).
    static constexpr auto help = nullptr;

protected:
    /*!
    @brief
        Base constructor.

    @param[in] selfComp
        Self component wrapper.
    @param[in] logTag
        Tag prefix of the provided logger.
    */
    explicit UserComponent(const SelfCompT selfComp, const std::string& logTag)
        : _mLogger {selfComp, fmt::format("{}/[{}]", logTag, selfComp.name())},
          _mSelfComp {selfComp}
    {
    }

    /*!
    @brief
        Name of this component.

    @returns
        Name of this component.
    */
    bt2c::CStringView _name() const noexcept
    {
        return _mSelfComp.name();
    }

    /*!
    @brief
        Logging level of this component.

    @returns
        Logging level of this component.
    */
    LoggingLevel _loggingLevel() const noexcept
    {
        return _mSelfComp.loggingLevel();
    }

    /*!
    @brief
        Effective MIP version of the trace processing
        graph which contains this component.

    @returns
        Effective MIP version of the trace processing
        graph which contains this component.
    */
    std::uint64_t _graphMipVersion() const noexcept
    {
        return _mSelfComp.graphMipVersion();
    }

    /*!
    @brief
        Creates a trace class from this component.

    May throw #MemoryError.

    @returns
        New, default trace class.
    */
    TraceClass::Shared _createTraceClass() const
    {
        return _mSelfComp.createTraceClass();
    }

    /*!
    @brief
        Creates a clock class from this component.

    May throw #MemoryError.

    @returns
        New, default clock class.
    */
    ClockClass::Shared _createClockClass() const
    {
        return _mSelfComp.createClockClass();
    }

    /*!
    @brief
        Corresponding self component wrapper.

    @returns
        Corresponding self component wrapper.
    */
    SelfCompT _selfComp() noexcept
    {
        return _mSelfComp;
    }

    /*!
    @brief
        Dedicated logger.

    Because it's named <code>%_mLogger</code>,
    you can use the <code>BT_CPPLOG*()</code> macros within your
    methods, for example:

    @code{.cpp}
    BT_CPPLOGI("Initializing `{}` component with {} threads.",
               this->_name(), threadCount);
    @endcode
    */
    bt2c::Logger _mLogger;

private:
    SelfCompT _mSelfComp;
};

/*!
@brief
    Base class of a user source component class
    \bt_p{UserComponentT} (CRTP).

See the
\ref common-cpp-bt2-comp-cls-dev-usage "C++ component class development usage"
to learn about the requirements of this base class.

@tparam UserComponentT
    Your component class (CRTP).
@tparam UserMessageIteratorT
    Your message iterator class.
    which must inherit #UserMessageIterator.
@tparam InitDataT
    Type of the initialization data of which your constructor
    parameter points to.
@tparam QueryDataT
    Type of the query data of which your query method
    parameter points to.
*/
template <typename UserComponentT, typename UserMessageIteratorT, typename InitDataT = void,
          typename QueryDataT = void>
class UserSourceComponent : public UserComponent<SelfSourceComponent, InitDataT, QueryDataT>
{
    static_assert(std::is_base_of_v<UserMessageIterator<UserMessageIteratorT, UserComponentT>,
                                    UserMessageIteratorT>,
                  "`UserMessageIteratorT` inherits `UserMessageIterator`");

public:
    /// Your message iterator class.
    using MessageIterator = UserMessageIteratorT;

protected:
    /// Output port container type.
    using _OutputPorts = SelfSourceComponent::OutputPorts;

    /*!
    @brief
        Protected constructor.

    @param[in] selfComp
        Self source component wrapper.
    @param[in] logTag
        Tag prefix of the provided logger.
    */
    explicit UserSourceComponent(const SelfSourceComponent selfComp, const std::string& logTag)
        : UserComponent<SelfSourceComponent, InitDataT, QueryDataT> {selfComp, logTag}
    {
    }

public:
    /*!
    @brief
        Type enumerator of this component class.

    @returns
        Type enumerator of this component class.
    */
    static constexpr ComponentClassType type() noexcept
    {
        return ComponentClassType::Source;
    }

    static Value::Shared query(const SelfComponentClass selfCompCls,
                               const PrivateQueryExecutor privQueryExec,
                               const bt2c::CStringView obj, const ConstValue params,
                               QueryDataT * const data)
    {
        return UserComponentT::_query(selfCompCls, privQueryExec, obj, params, data);
    }

    static void getSupportedMipVersions(const SelfComponentClass selfCompCls,
                                        const ConstMapValue params, InitDataT * const initData,
                                        const LoggingLevel loggingLevel,
                                        const UnsignedIntegerRangeSet ranges)
    {
        UserComponentT::_getSupportedMipVersions(selfCompCls, params, initData, loggingLevel,
                                                 ranges);
    }

    void outputPortConnected(const SelfComponentOutputPort outputPort,
                             const ConstInputPort inputPort)
    {
        static_cast<UserComponentT&>(*this)._outputPortConnected(outputPort, inputPort);
    }

protected:
    /*!
    @brief
        Override to implement the query method of this component class.

    This default version throws #UnknownObject.

    Yours may throw:

    - #Error
    - #MemoryError
    - #TryAgain
    - #UnknownObject

    @param[in] selfCompCls
        Self component class wrapper.
    @param[in] privQueryExec
        Private query executor.
    @param[in] object
        Query object.
    @param[in] params
        Query parameters.
    @param[in] queryData
        Custom query data.

    @returns
        Query results.
    */
    [[noreturn]] static Value::Shared _query(SelfComponentClass selfCompCls __attribute__((unused)),
                                             PrivateQueryExecutor privQueryExec
                                             __attribute__((unused)),
                                             bt2c::CStringView object __attribute__((unused)),
                                             ConstValue params __attribute__((unused)),
                                             QueryDataT *queryData __attribute__((unused)))
    {
        throw UnknownObject {};
    }

    /*!
    @brief
        Override to implement the
        “get supported Message Interchange Protocol versions”
        method of this component class.

    This default version adds the [0,&nbsp;0] range to \bt_p{ranges}.

    Yours may throw:

    - #Error
    - #MemoryError

    @param[in] selfCompCls
        Self component class wrapper.
    @param[in] params
        Initialization parameters.
    @param[in] initData
        Custom initialization data.
    @param[in] loggingLevel
        Logging level.
    @param[in] ranges
        Supported version ranges.
    */
    static void _getSupportedMipVersions(SelfComponentClass selfCompCls __attribute__((unused)),
                                         ConstMapValue params __attribute__((unused)),
                                         InitDataT *initData __attribute__((unused)),
                                         LoggingLevel loggingLevel __attribute__((unused)),
                                         const UnsignedIntegerRangeSet ranges)
    {
        ranges.addRange(0, 0);
    }

    /*!
    @brief
        Override to implement the “output port connected”
        method of this component class.

    Yours may throw:

    - #Error
    - #MemoryError

    @param[in] outputPort
        Output port.
    @param[in] inputPort
        Input port.
    */
    void _outputPortConnected(SelfComponentOutputPort outputPort __attribute__((unused)),
                              ConstInputPort inputPort __attribute__((unused)))
    {
    }

    /*!
    @brief
        Forwards to
        <code>bt_self_component_source_add_output_port()</code>
        for this component.

    @param[in] name
        Output port name.
    @param[in] data
        Custom port data.

    @returns
        Created port.
    */
    template <typename DataT>
    _OutputPorts::Port _addOutputPort(const bt2c::CStringView name, DataT& data)
    {
        return this->_selfComp().addOutputPort(name, data);
    }

    /*!
    @brief
        Forwards to
        <code>bt_self_component_source_add_output_port()</code>
        for this component.

    @param[in] name
        Output port name.

    @returns
        Created port.
    */
    _OutputPorts::Port _addOutputPort(const bt2c::CStringView name)
    {
        return this->_selfComp().addOutputPort(name);
    }

    /*!
    @brief
        Output ports.

    @returns
        Output ports.
    */
    _OutputPorts _outputPorts() noexcept
    {
        return this->_selfComp().outputPorts();
    }
};

/*!
@brief
    Base class of a user filter component class
    \bt_p{UserComponentT} (CRTP).

See the
\ref common-cpp-bt2-comp-cls-dev-usage "C++ component class development usage"
to learn about the requirements of this base class.

@tparam UserComponentT
    Your component class (CRTP).
@tparam UserMessageIteratorT
    Your message iterator class.
    which must inherit #UserMessageIterator.
@tparam InitDataT
    Type of the initialization data of which your constructor
    parameter points to.
@tparam QueryDataT
    Type of the query data of which your query method
    parameter points to.
*/
template <typename UserComponentT, typename UserMessageIteratorT, typename InitDataT = void,
          typename QueryDataT = void>
class UserFilterComponent : public UserComponent<SelfFilterComponent, InitDataT, QueryDataT>
{
    static_assert(std::is_base_of_v<UserMessageIterator<UserMessageIteratorT, UserComponentT>,
                                    UserMessageIteratorT>,
                  "`UserMessageIteratorT` inherits `UserMessageIterator`");

public:
    /// Your message iterator class.
    using MessageIterator = UserMessageIteratorT;

protected:
    /// Input port container type.
    using _InputPorts = SelfFilterComponent::InputPorts;

    /// Output port container type.
    using _OutputPorts = SelfFilterComponent::OutputPorts;

    /*!
    @brief
        Protected constructor.

    @param[in] selfComp
        Self filter component wrapper.
    @param[in] logTag
        Tag prefix of the provided logger.
    */
    explicit UserFilterComponent(const SelfFilterComponent selfComp, const std::string& logTag)
        : UserComponent<SelfFilterComponent, InitDataT, QueryDataT> {selfComp, logTag}
    {
    }

public:
    /*!
    @brief
        Type enumerator of this component class.

    @returns
        Type enumerator of this component class.
    */
    static constexpr ComponentClassType type() noexcept
    {
        return ComponentClassType::Filter;
    }

    static Value::Shared query(const SelfComponentClass selfCompCls,
                               const PrivateQueryExecutor privQueryExec,
                               const bt2c::CStringView obj, const ConstValue params,
                               QueryDataT * const data)
    {
        return UserComponentT::_query(selfCompCls, privQueryExec, obj, params, data);
    }

    static void getSupportedMipVersions(const SelfComponentClass selfCompCls,
                                        const ConstMapValue params, InitDataT * const initData,
                                        const LoggingLevel loggingLevel,
                                        const UnsignedIntegerRangeSet ranges)
    {
        UserComponentT::_getSupportedMipVersions(selfCompCls, params, initData, loggingLevel,
                                                 ranges);
    }

    void inputPortConnected(const SelfComponentInputPort inputPort,
                            const ConstOutputPort outputPort)
    {
        static_cast<UserComponentT&>(*this)._inputPortConnected(inputPort, outputPort);
    }

    void outputPortConnected(const SelfComponentOutputPort outputPort,
                             const ConstInputPort inputPort)
    {
        static_cast<UserComponentT&>(*this)._outputPortConnected(outputPort, inputPort);
    }

protected:
    /*!
    @brief
        Override to implement the query method of this component class.

    This default version throws #UnknownObject.

    Yours may throw:

    - #Error
    - #MemoryError
    - #TryAgain
    - #UnknownObject

    @param[in] selfCompCls
        Self component class wrapper.
    @param[in] privQueryExec
        Private query executor.
    @param[in] object
        Query object.
    @param[in] params
        Query parameters.
    @param[in] queryData
        Custom query data.

    @returns
        Query results.
    */
    [[noreturn]] static Value::Shared _query(SelfComponentClass selfCompCls __attribute__((unused)),
                                             PrivateQueryExecutor privQueryExec
                                             __attribute__((unused)),
                                             bt2c::CStringView object __attribute__((unused)),
                                             ConstValue params __attribute__((unused)),
                                             QueryDataT *queryData __attribute__((unused)))
    {
        throw UnknownObject {};
    }

    /*!
    @brief
        Override to implement the
        “get supported Message Interchange Protocol versions”
        method of this component class.

    This default version adds the [0,&nbsp;0] range to \bt_p{ranges}.

    Yours may throw:

    - #Error
    - #MemoryError

    @param[in] selfCompCls
        Self component class wrapper.
    @param[in] params
        Initialization parameters.
    @param[in] initData
        Custom initialization data.
    @param[in] loggingLevel
        Logging level.
    @param[in] ranges
        Supported version ranges.
    */
    static void _getSupportedMipVersions(SelfComponentClass selfCompCls __attribute__((unused)),
                                         ConstMapValue params __attribute__((unused)),
                                         InitDataT *initData __attribute__((unused)),
                                         LoggingLevel loggingLevel __attribute__((unused)),
                                         const UnsignedIntegerRangeSet ranges)
    {
        ranges.addRange(0, 0);
    }

    /*!
    @brief
        Override to implement the “input port connected”
        method of this component class.

    Yours may throw:

    - #Error
    - #MemoryError

    @param[in] inputPort
        Input port.
    @param[in] outputPort
        Output port.
    */
    void _inputPortConnected(SelfComponentInputPort inputPort __attribute__((unused)),
                             ConstOutputPort outputPort __attribute__((unused)))
    {
    }

    /*!
    @brief
        Override to implement the “output port connected”
        method of this component class.

    Yours may throw:

    - #Error
    - #MemoryError

    @param[in] outputPort
        Output port.
    @param[in] inputPort
        Input port.
    */
    void _outputPortConnected(SelfComponentOutputPort outputPort __attribute__((unused)),
                              ConstInputPort inputPort __attribute__((unused)))
    {
    }

    /*!
    @brief
        Forwards to
        <code>bt_self_component_filter_add_intput_port()</code>
        for this component.

    @param[in] name
        Output port name.
    @param[in] data
        Custom port data.

    @returns
        Created port.
    */
    template <typename DataT>
    _OutputPorts::Port _addInputPort(const bt2c::CStringView name, DataT& data)
    {
        return this->_selfComp().addInputPort(name, data);
    }

    /*!
    @brief
        Forwards to
        <code>bt_self_component_filter_add_intput_port()</code>
        for this component.

    @param[in] name
        Output port name.

    @returns
        Created port.
    */
    _InputPorts::Port _addInputPort(const bt2c::CStringView name)
    {
        return this->_selfComp().addInputPort(name);
    }

    /*!
    @brief
        Input ports.

    @returns
        Input ports.
    */
    _InputPorts _inputPorts() noexcept
    {
        return this->_selfComp().inputPorts();
    }

    /*!
    @brief
        Forwards to
        <code>bt_self_component_filter_add_output_port()</code>
        for this component.

    @param[in] name
        Output port name.
    @param[in] data
        Custom port data.

    @returns
        Created port.
    */
    template <typename DataT>
    _OutputPorts::Port _addOutputPort(const bt2c::CStringView name, DataT& data)
    {
        return this->_selfComp().addOutputPort(name, data);
    }

    /*!
    @brief
        Forwards to
        <code>bt_self_component_filter_add_output_port()</code>
        for this component.

    @param[in] name
        Output port name.

    @returns
        Created port.
    */
    _OutputPorts::Port _addOutputPort(const bt2c::CStringView name)
    {
        return this->_selfComp().addOutputPort(name);
    }

    /*!
    @brief
        Output ports.

    @returns
        Output ports.
    */
    _OutputPorts _outputPorts() noexcept
    {
        return this->_selfComp().outputPorts();
    }
};

/*!
@brief
    Base class of a user sink component class
    \bt_p{UserComponentT} (CRTP).

See the
\ref common-cpp-bt2-comp-cls-dev-usage "C++ component class development usage"
to learn about the requirements of this base class.

@tparam UserComponentT
    Your component class (CRTP).
@tparam InitDataT
    Type of the initialization data of which your constructor
    parameter points to.
@tparam QueryDataT
    Type of the query data of which your query method
    parameter points to.
*/
template <typename UserComponentT, typename InitDataT = void, typename QueryDataT = void>
class UserSinkComponent : public UserComponent<SelfSinkComponent, InitDataT, QueryDataT>
{
protected:
    /// Input port container type.
    using _InputPorts = SelfSinkComponent::InputPorts;

    /*!
    @brief
        Protected constructor.

    @param[in] selfComp
        Self sink component wrapper.
    @param[in] logTag
        Tag prefix of the provided logger.
    */
    explicit UserSinkComponent(const SelfSinkComponent selfComp, const std::string& logTag)
        : UserComponent<SelfSinkComponent, InitDataT, QueryDataT> {selfComp, logTag}
    {
    }

public:
    /*!
    @brief
        Type enumerator of this component class.

    @returns
        Type enumerator of this component class.
    */
    static constexpr ComponentClassType type() noexcept
    {
        return ComponentClassType::Sink;
    }

    static Value::Shared query(const SelfComponentClass selfCompCls,
                               const PrivateQueryExecutor privQueryExec,
                               const bt2c::CStringView obj, const ConstValue params,
                               QueryDataT * const data)
    {
        return UserComponentT::_query(selfCompCls, privQueryExec, obj, params, data);
    }

    static void getSupportedMipVersions(const SelfComponentClass selfCompCls,
                                        const ConstMapValue params, InitDataT * const initData,
                                        const LoggingLevel loggingLevel,
                                        const UnsignedIntegerRangeSet ranges)
    {
        UserComponentT::_getSupportedMipVersions(selfCompCls, params, initData, loggingLevel,
                                                 ranges);
    }

    void graphIsConfigured()
    {
        static_cast<UserComponentT&>(*this)._graphIsConfigured();
    }

    void inputPortConnected(const SelfComponentInputPort inputPort,
                            const ConstOutputPort outputPort)
    {
        static_cast<UserComponentT&>(*this)._inputPortConnected(inputPort, outputPort);
    }

    bool consume()
    {
        return static_cast<UserComponentT&>(*this)._consume();
    }

protected:
    /*!
    @brief
        Override to implement the query method of this component class.

    This default version throws #UnknownObject.

    Yours may throw:

    - #Error
    - #MemoryError
    - #TryAgain
    - #UnknownObject

    @param[in] selfCompCls
        Self component class wrapper.
    @param[in] privQueryExec
        Private query executor.
    @param[in] object
        Query object.
    @param[in] params
        Query parameters.
    @param[in] queryData
        Custom query data.

    @returns
        Query results.
    */
    [[noreturn]] static Value::Shared _query(SelfComponentClass selfCompCls __attribute__((unused)),
                                             PrivateQueryExecutor privQueryExec
                                             __attribute__((unused)),
                                             bt2c::CStringView object __attribute__((unused)),
                                             ConstValue params __attribute__((unused)),
                                             QueryDataT *queryData __attribute__((unused)))
    {
        throw UnknownObject {};
    }

    /*!
    @brief
        Override to implement the
        “get supported Message Interchange Protocol versions”
        method of this component class.

    This default version adds the [0,&nbsp;0] range to \bt_p{ranges}.

    Yours may throw:

    - #Error
    - #MemoryError

    @param[in] selfCompCls
        Self component class wrapper.
    @param[in] params
        Initialization parameters.
    @param[in] initData
        Custom initialization data.
    @param[in] loggingLevel
        Logging level.
    @param[in] ranges
        Supported version ranges.
    */
    static void _getSupportedMipVersions(SelfComponentClass selfCompCls __attribute__((unused)),
                                         ConstMapValue params __attribute__((unused)),
                                         InitDataT *initData __attribute__((unused)),
                                         LoggingLevel loggingLevel __attribute__((unused)),
                                         const UnsignedIntegerRangeSet ranges)
    {
        ranges.addRange(0, 0);
    }

    /*!
    @brief
        Override to implement the “graph is configured”
        method of this component class.

    You may now throw anything.
    */
    void _graphIsConfigured()
    {
    }

    /*!
    @brief
        Override to implement the “input port connected”
        method of this component class.

    Yours may throw:

    - #Error
    - #MemoryError

    @param[in] inputPort
        Input port.
    @param[in] outputPort
        Output port.
    */
    void _inputPortConnected(SelfComponentInputPort inputPort __attribute__((unused)),
                             ConstOutputPort outputPort __attribute__((unused)))
    {
    }

    /*!
    @brief
        Creates a message iterator on the input port \bt_p{port}
        from this component.

    @param[in] port
        Input port on which to create the message iterator.

    @returns
        Created message iterator.
    */
    MessageIterator::Shared _createMessageIterator(const _InputPorts::Port port)
    {
        return this->_selfComp().createMessageIterator(port);
    }

    /*!
    @brief
        Forwards to
        <code>bt_self_component_sink_add_intput_port()</code>
        for this component.

    @param[in] name
        Output port name.
    @param[in] data
        Custom port data.

    @returns
        Created port.
    */
    template <typename DataT>
    _InputPorts::Port _addInputPort(const bt2c::CStringView name, DataT& data)
    {
        return this->_selfComp().addInputPort(name, data);
    }

    /*!
    @brief
        Forwards to
        <code>bt_self_component_filter_add_intput_port()</code>
        for this component.

    @param[in] name
        Output port name.

    @returns
        Created port.
    */
    _InputPorts::Port _addInputPort(const bt2c::CStringView name)
    {
        return this->_selfComp().addInputPort(name);
    }

    /*!
    @brief
        Input ports.

    @returns
        Input ports.
    */
    _InputPorts _inputPorts() noexcept
    {
        return this->_selfComp().inputPorts();
    }
};

/*!
@brief
    Base class of a user message iterator class
    \bt_p{UserMessageIteratorT} (CRTP).

See the
\ref common-cpp-bt2-comp-cls-dev-usage "C++ component class development usage"
to learn about the requirements of this base class.

@tparam UserMessageIteratorT
    Your message iterator class (CRTP).
@tparam UserComponentT
    Your component class.

@note
    @parblock
    The individual <code>_create_*()</code> methods of this class aren't
    documented yet, but they're straightforward to understand if you
    already know the libbabeltrace2 C&nbsp;API.

    Please see <code>%src/cpp-common/bt2/component-class-dev.hpp</code>.
    @endparblock
*/
template <typename UserMessageIteratorT, typename UserComponentT>
class UserMessageIterator
{
private:
    /* Avoid `-Wshadow` error on GCC, conflicting with `bt2::Error` */
    BT_DIAG_PUSH
    BT_DIAG_IGNORE_SHADOW

    /* Type of `_mExcToThrowType` */
    enum class _ExcToThrowType
    {
        None,
        Error,
        MemError,
    };

    BT_DIAG_POP

protected:
    /*!
    @brief
        Protected constructor.

    @param[in] selfMsgIter
        Self message iterator wrapper.
    @param[in] logTagSuffix
        Tag suffix of the provided logger.
    */
    explicit UserMessageIterator(const SelfMessageIterator selfMsgIter,
                                 const std::string& logTagSuffix)
        : _mSelfMsgIter {selfMsgIter},
          _mLogger {selfMsgIter,
                    fmt::format("{}/{}", this->_component()._mLogger.tag(), logTagSuffix)}
    {
    }

public:
    void next(ConstMessageArray& messages)
    {
        /* Any saved error? Now is the time to throw */
        if (G_UNLIKELY(_mExcToThrowType != _ExcToThrowType::None)) {
            /* Move `_mSavedLibError`, if any, as current thread error */
            if (_mSavedLibError) {
                bt_current_thread_move_error(_mSavedLibError.release());
            }

            /* Throw the corresponding exception */
            if (_mExcToThrowType == _ExcToThrowType::Error) {
                throw Error {};
            } else {
                BT_ASSERT(_mExcToThrowType == _ExcToThrowType::MemError);
                throw MemoryError {};
            }
        }

        /*
         * When catching some exception below, if our message array
         * isn't empty, then return immediately before throwing to
         * provide those messages to downstream.
         *
         * When catching an error, also save the current thread error,
         * if any, so that we can restore it later (see the beginning of
         * this method).
         */
        BT_ASSERT_DBG(_mExcToThrowType == _ExcToThrowType::None);

        try {
            this->_userObj()._next(messages);

            /* We're done: everything below is exception handling */
            return;
        } catch (const TryAgain&) {
            if (messages.isEmpty()) {
                throw;
            }
        } catch (const std::bad_alloc&) {
            if (messages.isEmpty()) {
                throw;
            }

            _mExcToThrowType = _ExcToThrowType::MemError;
        } catch (const Error&) {
            if (messages.isEmpty()) {
                throw;
            }

            _mExcToThrowType = _ExcToThrowType::Error;
        }

        if (_mExcToThrowType != _ExcToThrowType::None) {
            BT_CPPLOGE(
                "An error occurred, but there are {} messages to return: delaying the error reporting.",
                messages.length());
            BT_ASSERT(!_mSavedLibError);
            _mSavedLibError.reset(bt_current_thread_take_error());
        }
    }

    bool canSeekBeginning()
    {
        this->_resetError();
        return this->_userObj()._canSeekBeginning();
    }

    void seekBeginning()
    {
        this->_resetError();
        return this->_userObj()._seekBeginning();
    }

    bool canSeekNsFromOrigin(const std::int64_t nsFromOrigin)
    {
        this->_resetError();
        return this->_userObj()._canSeekNsFromOrigin(nsFromOrigin);
    }

    void seekNsFromOrigin(const std::int64_t nsFromOrigin)
    {
        this->_resetError();
        this->_userObj()._seekNsFromOrigin(nsFromOrigin);
    }

protected:
    /*!
    @brief
        Override to implement the
        “can seek beginning?” method of this message iterator class.

    This default version returns <code>false</code>.

    Yours may throw:

    - #Error
    - #MemoryError
    - #TryAgain

    @returns
        \c true if this message iterator can seek its beginning.
    */
    bool _canSeekBeginning() noexcept
    {
        return false;
    }

    /*!
    @brief
        Override to implement the
        “seek beginning” method of this message iterator class.

    You may throw:

    - #Error
    - #MemoryError
    - #TryAgain
    */
    void _seekBeginning() noexcept
    {
    }

    /*!
    @brief
        Override to implement the
        “can seek ns from origin?” method of
        this message iterator class.

    This default version returns <code>false</code>.

    Yours may throw:

    - #Error
    - #MemoryError
    - #TryAgain

    @param[in] nsFromOrigin
        Nanoseconds from clock origin.

    @returns
        \c true if this message iterator can seek
        \bt_p{nsFromOrigin}&nbsp;ns from its clock origin.
    */
    bool _canSeekNsFromOrigin(std::int64_t nsFromOrigin __attribute__((unused))) noexcept
    {
        return false;
    }

    /*!
    @brief
        Override to implement the
        “seek ns from origin” method of this message iterator class.

    You may throw:

    - #Error
    - #MemoryError
    - #TryAgain

    @param[in] nsFromOrigin
        Nanoseconds from clock origin.
    */
    void _seekNsFromOrigin(std::int64_t nsFromOrigin __attribute__((unused))) noexcept
    {
    }

    MessageIterator::Shared _createMessageIterator(const SelfComponentInputPort port)
    {
        return _mSelfMsgIter.createMessageIterator(port);
    }

    StreamBeginningMessage::Shared _createStreamBeginningMessage(const ConstStream stream) const
    {
        return _mSelfMsgIter.createStreamBeginningMessage(stream);
    }

    StreamEndMessage::Shared _createStreamEndMessage(const ConstStream stream) const
    {
        return _mSelfMsgIter.createStreamEndMessage(stream);
    }

    EventMessage::Shared _createEventMessage(const ConstEventClass eventCls,
                                             const ConstStream stream) const
    {
        return _mSelfMsgIter.createEventMessage(eventCls, stream);
    }

    EventMessage::Shared _createEventMessage(const ConstEventClass eventCls,
                                             const ConstStream stream,
                                             const std::uint64_t clockSnapshotValue) const
    {
        return _mSelfMsgIter.createEventMessage(eventCls, stream, clockSnapshotValue);
    }

    EventMessage::Shared _createEventMessage(const ConstEventClass eventCls,
                                             const ConstPacket packet) const
    {
        return _mSelfMsgIter.createEventMessage(eventCls, packet);
    }

    EventMessage::Shared _createEventMessage(const ConstEventClass eventCls,
                                             const ConstPacket packet,
                                             const std::uint64_t clockSnapshotValue) const
    {
        return _mSelfMsgIter.createEventMessage(eventCls, packet, clockSnapshotValue);
    }

    PacketBeginningMessage::Shared _createPacketBeginningMessage(const ConstPacket packet) const
    {
        return _mSelfMsgIter.createPacketBeginningMessage(packet);
    }

    PacketBeginningMessage::Shared
    _createPacketBeginningMessage(const ConstPacket packet,
                                  const std::uint64_t clockSnapshotValue) const
    {
        return _mSelfMsgIter.createPacketBeginningMessage(packet, clockSnapshotValue);
    }

    PacketEndMessage::Shared _createPacketEndMessage(const ConstPacket packet) const
    {
        return _mSelfMsgIter.createPacketEndMessage(packet);
    }

    PacketEndMessage::Shared _createPacketEndMessage(const ConstPacket packet,
                                                     const std::uint64_t clockSnapshotValue) const
    {
        return _mSelfMsgIter.createPacketEndMessage(packet, clockSnapshotValue);
    }

    DiscardedEventsMessage::Shared _createDiscardedEventsMessage(const ConstStream stream)
    {
        return _mSelfMsgIter.createDiscardedEventsMessage(stream);
    }

    DiscardedEventsMessage::Shared
    _createDiscardedEventsMessage(const ConstStream stream,
                                  const std::uint64_t beginningClockSnapshotValue,
                                  const std::uint64_t endClockSnapshotValue)
    {
        return _mSelfMsgIter.createDiscardedEventsMessage(stream, beginningClockSnapshotValue,
                                                          endClockSnapshotValue);
    }

    DiscardedPacketsMessage::Shared _createDiscardedPacketsMessage(const ConstStream stream)
    {
        return _mSelfMsgIter.createDiscardedPacketsMessage(stream);
    }

    DiscardedPacketsMessage::Shared
    _createDiscardedPacketsMessage(const ConstStream stream,
                                   const std::uint64_t beginningClockSnapshotValue,
                                   const std::uint64_t endClockSnapshotValue)
    {
        return _mSelfMsgIter.createDiscardedPacketsMessage(stream, beginningClockSnapshotValue,
                                                           endClockSnapshotValue);
    }

    MessageIteratorInactivityMessage::Shared
    _createMessageIteratorInactivityMessage(const ConstClockClass clockClass,
                                            const std::uint64_t clockSnapshotValue)
    {
        return _mSelfMsgIter.createMessageIteratorInactivityMessage(clockClass, clockSnapshotValue);
    }

    /*!
    @brief
        Component (an instance of your own C++ component class) of
        this message iterator.

    @returns
        Component of this message iterator.
    */
    UserComponentT& _component() noexcept
    {
        return _mSelfMsgIter.component().template data<UserComponentT>();
    }

    /*!
    @brief
        Output port.

    @returns
        Output port.
    */
    SelfComponentOutputPort _port() noexcept
    {
        return _mSelfMsgIter.port();
    }

    /*!
    @brief
        Whether or not this message iterator is interrupted.

    @returns
        \c true if this message iterator is interrupted.
    */
    bool _isInterrupted() const noexcept
    {
        return _mSelfMsgIter.isInterrupted();
    }

private:
    UserMessageIteratorT& _userObj() noexcept
    {
        return static_cast<UserMessageIteratorT&>(*this);
    }

    void _resetError() noexcept
    {
        _mExcToThrowType = _ExcToThrowType::None;
        _mSavedLibError.reset();
    }

    SelfMessageIterator _mSelfMsgIter;

    /*
     * next() may accumulate messages, and then catch an error before
     * returning. In that case, it saves the error of the current thread
     * here so that it can return its accumulated messages and throw the
     * next time.
     *
     * It also saves the type of the exception to throw the next time.
     */
    _ExcToThrowType _mExcToThrowType = _ExcToThrowType::None;

    struct LibErrorDeleter final
    {
        void operator()(const bt_error * const error) const noexcept
        {
            bt_error_release(error);
        }
    };

    std::unique_ptr<const bt_error, LibErrorDeleter> _mSavedLibError;

protected:
    /*!
    @brief
        Dedicated logger.

    Because it's named <code>%_mLogger</code>,
    you can use the <code>BT_CPPLOG*()</code> macros within your
    methods, for example:

    @code{.cpp}
    BT_CPPLOGE_APPEND_CAUSE_AND_THROW(bt2::Error,
                                      "Failed to connect to the database @ `{}`.",
                                      dbUri);
    @endcode
    */
    bt2c::Logger _mLogger;
};

namespace internal {

template <typename UserComponentT, typename CompClsBridgeT, typename LibSpecCompClsPtrT,
          typename AsCompClsFuncT, typename SetInitMethodFuncT, typename SetFinalizeMethodFuncT,
          typename SetGetSupportedMipVersionsMethodFuncT, typename SetQueryMethodFuncT>
void setCompClsCommonProps(
    LibSpecCompClsPtrT * const libSpecCompClsPtr, AsCompClsFuncT&& asCompClsFunc,
    SetInitMethodFuncT&& setInitMethodFunc, SetFinalizeMethodFuncT&& setFinalizeMethodFunc,
    SetGetSupportedMipVersionsMethodFuncT&& setGetSupportedMipVersionsMethodFunc,
    SetQueryMethodFuncT&& setQueryMethodFunc)
{
    const auto libCompClsPtr = asCompClsFunc(libSpecCompClsPtr);

    if (UserComponentT::description != nullptr) {
        const auto status =
            bt_component_class_set_description(libCompClsPtr, UserComponentT::description);

        if (status == BT_COMPONENT_CLASS_SET_DESCRIPTION_STATUS_MEMORY_ERROR) {
            throw MemoryError {};
        }
    }

    if (UserComponentT::help != nullptr) {
        if (bt_component_class_set_help(libCompClsPtr, UserComponentT::help) ==
            BT_COMPONENT_CLASS_SET_HELP_STATUS_MEMORY_ERROR) {
            throw MemoryError {};
        }
    }

    {
        const auto status = setInitMethodFunc(libSpecCompClsPtr, CompClsBridgeT::init);

        BT_ASSERT(status == BT_COMPONENT_CLASS_SET_METHOD_STATUS_OK);
    }

    {
        const auto status = setFinalizeMethodFunc(libSpecCompClsPtr, CompClsBridgeT::finalize);

        BT_ASSERT(status == BT_COMPONENT_CLASS_SET_METHOD_STATUS_OK);
    }

    {
        const auto status = setGetSupportedMipVersionsMethodFunc(
            libSpecCompClsPtr, CompClsBridgeT::getSupportedMipVersions);

        BT_ASSERT(status == BT_COMPONENT_CLASS_SET_METHOD_STATUS_OK);
    }

    {
        const auto status = setQueryMethodFunc(libSpecCompClsPtr, CompClsBridgeT::query);

        BT_ASSERT(status == BT_COMPONENT_CLASS_SET_METHOD_STATUS_OK);
    }
}

template <typename MsgIterClsBridgeT>
bt_message_iterator_class *createLibMsgIterCls()
{
    const auto libMsgIterClsPtr = bt_message_iterator_class_create(MsgIterClsBridgeT::next);

    if (!libMsgIterClsPtr) {
        throw MemoryError {};
    }

    {
        const auto status = bt_message_iterator_class_set_initialize_method(
            libMsgIterClsPtr, MsgIterClsBridgeT::init);

        BT_ASSERT(status == BT_MESSAGE_ITERATOR_CLASS_SET_METHOD_STATUS_OK);
    }

    {
        const auto status = bt_message_iterator_class_set_finalize_method(
            libMsgIterClsPtr, MsgIterClsBridgeT::finalize);

        BT_ASSERT(status == BT_MESSAGE_ITERATOR_CLASS_SET_METHOD_STATUS_OK);
    }

    {
        const auto status = bt_message_iterator_class_set_seek_beginning_methods(
            libMsgIterClsPtr, MsgIterClsBridgeT::seekBeginning,
            MsgIterClsBridgeT::canSeekBeginning);

        BT_ASSERT(status == BT_MESSAGE_ITERATOR_CLASS_SET_METHOD_STATUS_OK);
    }

    {
        const auto status = bt_message_iterator_class_set_seek_ns_from_origin_methods(
            libMsgIterClsPtr, MsgIterClsBridgeT::seekNsFromOrigin,
            MsgIterClsBridgeT::canSeekNsFromOrigin);

        BT_ASSERT(status == BT_MESSAGE_ITERATOR_CLASS_SET_METHOD_STATUS_OK);
    }

    return libMsgIterClsPtr;
}

template <typename UserComponentT>
bt_component_class_source *createSourceCompCls()
{
    static_assert(std::is_base_of_v<
                      UserSourceComponent<UserComponentT, typename UserComponentT::MessageIterator,
                                          typename UserComponentT::InitData,
                                          typename UserComponentT::QueryData>,
                      UserComponentT>,
                  "`UserComponentT` inherits `UserSourceComponent`");

    using CompClsBridge = internal::SrcCompClsBridge<UserComponentT>;
    using MsgIterClsBridge = internal::MsgIterClsBridge<typename UserComponentT::MessageIterator>;

    const auto libMsgIterClsPtr = createLibMsgIterCls<MsgIterClsBridge>();
    const auto libCompClsPtr =
        bt_component_class_source_create(UserComponentT::name, libMsgIterClsPtr);

    bt_message_iterator_class_put_ref(libMsgIterClsPtr);

    if (!libCompClsPtr) {
        throw MemoryError {};
    }

    setCompClsCommonProps<UserComponentT, CompClsBridge>(
        libCompClsPtr, bt_component_class_source_as_component_class,
        bt_component_class_source_set_initialize_method,
        bt_component_class_source_set_finalize_method,
        bt_component_class_source_set_get_supported_mip_versions_method,
        bt_component_class_source_set_query_method);

    {
        const auto status = bt_component_class_source_set_output_port_connected_method(
            libCompClsPtr, CompClsBridge::outputPortConnected);

        BT_ASSERT(status == BT_COMPONENT_CLASS_SET_METHOD_STATUS_OK);
    }

    return libCompClsPtr;
}

template <typename UserComponentT>
bt_component_class_filter *createFilterCompCls()
{
    static_assert(std::is_base_of_v<
                      UserFilterComponent<UserComponentT, typename UserComponentT::MessageIterator,
                                          typename UserComponentT::InitData,
                                          typename UserComponentT::QueryData>,
                      UserComponentT>,
                  "`UserComponentT` inherits `UserFilterComponent`");

    using CompClsBridge = internal::FltCompClsBridge<UserComponentT>;
    using MsgIterClsBridge = internal::MsgIterClsBridge<typename UserComponentT::MessageIterator>;

    const auto libMsgIterClsPtr = createLibMsgIterCls<MsgIterClsBridge>();
    const auto libCompClsPtr =
        bt_component_class_filter_create(UserComponentT::name, libMsgIterClsPtr);

    bt_message_iterator_class_put_ref(libMsgIterClsPtr);

    if (!libCompClsPtr) {
        throw MemoryError {};
    }

    setCompClsCommonProps<UserComponentT, CompClsBridge>(
        libCompClsPtr, bt_component_class_filter_as_component_class,
        bt_component_class_filter_set_initialize_method,
        bt_component_class_filter_set_finalize_method,
        bt_component_class_filter_set_get_supported_mip_versions_method,
        bt_component_class_filter_set_query_method);

    {
        const auto status = bt_component_class_filter_set_input_port_connected_method(
            libCompClsPtr, CompClsBridge::inputPortConnected);

        BT_ASSERT(status == BT_COMPONENT_CLASS_SET_METHOD_STATUS_OK);
    }

    {
        const auto status = bt_component_class_filter_set_output_port_connected_method(
            libCompClsPtr, CompClsBridge::outputPortConnected);

        BT_ASSERT(status == BT_COMPONENT_CLASS_SET_METHOD_STATUS_OK);
    }

    return libCompClsPtr;
}

template <typename UserComponentT>
bt_component_class_sink *createSinkCompCls()
{
    static_assert(
        std::is_base_of_v<UserSinkComponent<UserComponentT, typename UserComponentT::InitData,
                                            typename UserComponentT::QueryData>,
                          UserComponentT>,
        "`UserComponentT` inherits `UserSinkComponent`");

    using CompClsBridge = internal::SinkCompClsBridge<UserComponentT>;

    const auto libCompClsPtr =
        bt_component_class_sink_create(UserComponentT::name, CompClsBridge::consume);

    if (!libCompClsPtr) {
        throw MemoryError {};
    }

    setCompClsCommonProps<UserComponentT, CompClsBridge>(
        libCompClsPtr, bt_component_class_sink_as_component_class,
        bt_component_class_sink_set_initialize_method, bt_component_class_sink_set_finalize_method,
        bt_component_class_sink_set_get_supported_mip_versions_method,
        bt_component_class_sink_set_query_method);

    {
        const auto status = bt_component_class_sink_set_graph_is_configured_method(
            libCompClsPtr, CompClsBridge::graphIsConfigured);

        BT_ASSERT(status == BT_COMPONENT_CLASS_SET_METHOD_STATUS_OK);
    }

    {
        const auto status = bt_component_class_sink_set_input_port_connected_method(
            libCompClsPtr, CompClsBridge::inputPortConnected);

        BT_ASSERT(status == BT_COMPONENT_CLASS_SET_METHOD_STATUS_OK);
    }

    return libCompClsPtr;
}

} /* namespace internal */

/*!
@brief
    Creates a libbabeltrace2 component class
    from the C++ component class \bt_p{UserComponentT}.

@tparam UserComponentT
    Component class which inherits bt2::UserSourceComponent,
    bt2::UserFilterComponent, or bt2::UserSinkComponent.

@returns
    Component class wrapper.
*/
template <typename UserComponentT>
auto createComponentClass()
{
    if constexpr (UserComponentT::type() == ComponentClassType::Source) {
        return SourceComponentClass::Shared::createWithoutRef(
            internal::createSourceCompCls<UserComponentT>());
    } else if constexpr (UserComponentT::type() == ComponentClassType::Filter) {
        return FilterComponentClass::Shared::createWithoutRef(
            internal::createFilterCompCls<UserComponentT>());
    } else {
        static_assert(UserComponentT::type() == ComponentClassType::Sink);
        return SinkComponentClass::Shared::createWithoutRef(
            internal::createSinkCompCls<UserComponentT>());
    }
}

} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_COMPONENT_CLASS_DEV_HPP */
