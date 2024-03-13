/*
 * Copyright (c) 2023 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_SELF_COMPONENT_PORT_HPP
#define BABELTRACE_CPP_COMMON_BT2_SELF_COMPONENT_PORT_HPP

#include <cstdint>

#include <babeltrace2/babeltrace.h>

#include "logging.hpp"

#include "cpp-common/bt2c/c-string-view.hpp"

#include "borrowed-object-iterator.hpp"
#include "borrowed-object.hpp"
#include "component-port.hpp"
#include "message-iterator.hpp"

namespace bt2 {

class SelfSourceComponent;
class SelfFilterComponent;
class SelfSinkComponent;

class SelfComponent final : public BorrowedObject<bt_self_component>
{
public:
    explicit SelfComponent(const LibObjPtr libObjPtr) noexcept : _ThisBorrowedObject {libObjPtr}
    {
    }

    explicit SelfComponent(bt_self_component_source * const libObjPtr) noexcept :
        _ThisBorrowedObject {bt_self_component_source_as_self_component(libObjPtr)}
    {
    }

    explicit SelfComponent(bt_self_component_filter * const libObjPtr) noexcept :
        _ThisBorrowedObject {bt_self_component_filter_as_self_component(libObjPtr)}
    {
    }

    explicit SelfComponent(bt_self_component_sink * const libObjPtr) noexcept :
        _ThisBorrowedObject {bt_self_component_sink_as_self_component(libObjPtr)}
    {
    }

    /* Not `explicit` to make them behave like copy constructors */
    SelfComponent(SelfSourceComponent other) noexcept;
    SelfComponent(SelfFilterComponent other) noexcept;
    SelfComponent(SelfSinkComponent other) noexcept;

    SelfComponent operator=(SelfSourceComponent other) noexcept;
    SelfComponent operator=(SelfFilterComponent other) noexcept;
    SelfComponent operator=(SelfSinkComponent other) noexcept;

    ConstComponent asConstComponent() const noexcept
    {
        return ConstComponent {bt_self_component_as_component(this->libObjPtr())};
    }

    bool isSource() const noexcept
    {
        return this->asConstComponent().isSource();
    }

    bool isFilter() const noexcept
    {
        return this->asConstComponent().isFilter();
    }

    bool isSink() const noexcept
    {
        return this->asConstComponent().isSink();
    }

    bt2c::CStringView name() const noexcept
    {
        return this->asConstComponent().name();
    }

    LoggingLevel loggingLevel() const noexcept
    {
        return this->asConstComponent().loggingLevel();
    }

    std::uint64_t graphMipVersion() const noexcept
    {
        return bt_self_component_get_graph_mip_version(this->libObjPtr());
    }

    template <typename T>
    T& data() const noexcept
    {
        return *static_cast<T *>(bt_self_component_get_data(this->libObjPtr()));
    }

    template <typename T>
    SelfComponent data(T& obj) const noexcept
    {
        bt_self_component_set_data(this->libObjPtr(),
                                   const_cast<void *>(static_cast<const void *>(&obj)));
        return *this;
    }

    TraceClass::Shared createTraceClass() const
    {
        const auto libObjPtr = bt_trace_class_create(this->libObjPtr());

        if (!libObjPtr) {
            throw MemoryError {};
        }

        return TraceClass::Shared::createWithoutRef(libObjPtr);
    }

    ClockClass::Shared createClockClass() const
    {
        const auto libObjPtr = bt_clock_class_create(this->libObjPtr());

        if (!libObjPtr) {
            throw MemoryError {};
        }

        return ClockClass::Shared::createWithoutRef(libObjPtr);
    }
};

namespace internal {

template <typename LibObjT>
class SelfSpecificComponent : public BorrowedObject<LibObjT>
{
private:
    using typename BorrowedObject<LibObjT>::_ThisBorrowedObject;

public:
    using typename BorrowedObject<LibObjT>::LibObjPtr;

protected:
    explicit SelfSpecificComponent(const LibObjPtr libObjPtr) noexcept :
        _ThisBorrowedObject {libObjPtr}
    {
    }

    template <typename PortT, typename LibPortT, typename AddPortFuncT, typename DataT>
    PortT _addPort(const char * const name, DataT * const data, AddPortFuncT&& func) const
    {
        LibPortT *libPortPtr;

        const auto status = func(this->libObjPtr(), name,
                                 const_cast<void *>(static_cast<const void *>(data)), &libPortPtr);

        switch (status) {
        case BT_SELF_COMPONENT_ADD_PORT_STATUS_OK:
            return PortT {libPortPtr};
        case BT_SELF_COMPONENT_ADD_PORT_STATUS_MEMORY_ERROR:
            throw MemoryError {};
        case BT_SELF_COMPONENT_ADD_PORT_STATUS_ERROR:
            throw Error {};
        default:
            bt_common_abort();
        }
    }

public:
    bt2c::CStringView name() const noexcept
    {
        return this->_selfComponent().name();
    }

    LoggingLevel loggingLevel() const noexcept
    {
        return this->_selfComponent().loggingLevel();
    }

    std::uint64_t graphMipVersion() const noexcept
    {
        return this->_selfComponent().graphMipVersion();
    }

    template <typename T>
    T& data() const noexcept
    {
        return this->_selfComponent().template data<T>();
    }

protected:
    SelfComponent _selfComponent() const noexcept
    {
        return SelfComponent {this->libObjPtr()};
    }
};

template <typename LibSelfCompT, typename LibSelfCompPortPtrT>
struct SelfComponentPortsSpec;

template <>
struct SelfComponentPortsSpec<bt_self_component_source, bt_self_component_port_output> final
{
    static std::uint64_t portCount(bt_self_component_source * const libCompPtr) noexcept
    {
        return bt_component_source_get_output_port_count(
            bt_self_component_source_as_component_source(libCompPtr));
    }

    static bt_self_component_port_output *portByIndex(bt_self_component_source * const libCompPtr,
                                                      const std::uint64_t index) noexcept
    {
        return bt_self_component_source_borrow_output_port_by_index(libCompPtr, index);
    }

    static bt_self_component_port_output *portByName(bt_self_component_source * const libCompPtr,
                                                     const char * const name) noexcept
    {
        return bt_self_component_source_borrow_output_port_by_name(libCompPtr, name);
    }
};

template <>
struct SelfComponentPortsSpec<bt_self_component_filter, bt_self_component_port_output> final
{
    static std::uint64_t portCount(bt_self_component_filter * const libCompPtr) noexcept
    {
        return bt_component_filter_get_output_port_count(
            bt_self_component_filter_as_component_filter(libCompPtr));
    }

    static bt_self_component_port_output *portByIndex(bt_self_component_filter * const libCompPtr,
                                                      const std::uint64_t index) noexcept
    {
        return bt_self_component_filter_borrow_output_port_by_index(libCompPtr, index);
    }

    static bt_self_component_port_output *portByName(bt_self_component_filter * const libCompPtr,
                                                     const char * const name) noexcept
    {
        return bt_self_component_filter_borrow_output_port_by_name(libCompPtr, name);
    }
};

template <>
struct SelfComponentPortsSpec<bt_self_component_filter, bt_self_component_port_input> final
{
    static std::uint64_t portCount(bt_self_component_filter * const libCompPtr) noexcept
    {
        return bt_component_filter_get_input_port_count(
            bt_self_component_filter_as_component_filter(libCompPtr));
    }

    static bt_self_component_port_input *portByIndex(bt_self_component_filter * const libCompPtr,
                                                     const std::uint64_t index) noexcept
    {
        return bt_self_component_filter_borrow_input_port_by_index(libCompPtr, index);
    }

    static bt_self_component_port_input *portByName(bt_self_component_filter * const libCompPtr,
                                                    const char * const name) noexcept
    {
        return bt_self_component_filter_borrow_input_port_by_name(libCompPtr, name);
    }
};

template <>
struct SelfComponentPortsSpec<bt_self_component_sink, bt_self_component_port_input> final
{
    static std::uint64_t portCount(bt_self_component_sink * const libCompPtr) noexcept
    {
        return bt_component_sink_get_input_port_count(
            bt_self_component_sink_as_component_sink(libCompPtr));
    }

    static bt_self_component_port_input *portByIndex(bt_self_component_sink * const libCompPtr,
                                                     const std::uint64_t index) noexcept
    {
        return bt_self_component_sink_borrow_input_port_by_index(libCompPtr, index);
    }

    static bt_self_component_port_input *portByName(bt_self_component_sink * const libCompPtr,
                                                    const char * const name) noexcept
    {
        return bt_self_component_sink_borrow_input_port_by_name(libCompPtr, name);
    }
};

} /* namespace internal */

template <typename LibSelfCompPortT, typename LibPortT>
class SelfComponentPort;

template <typename LibSelfCompT, typename LibSelfCompPortT, typename LibPortT>
class SelfComponentPorts final : public BorrowedObject<LibSelfCompT>
{
private:
    using typename BorrowedObject<LibSelfCompT>::_ThisBorrowedObject;
    using _Spec = internal::SelfComponentPortsSpec<LibSelfCompT, LibSelfCompPortT>;

public:
    using typename BorrowedObject<LibSelfCompT>::LibObjPtr;
    using Port = SelfComponentPort<LibSelfCompPortT, LibPortT>;
    using Iterator = BorrowedObjectIterator<SelfComponentPorts>;

    explicit SelfComponentPorts(const LibObjPtr libObjPtr) noexcept :
        _ThisBorrowedObject {libObjPtr}
    {
    }

    std::uint64_t length() const noexcept
    {
        return _Spec::portCount(this->libObjPtr());
    }

    Port operator[](std::uint64_t index) const noexcept;
    Port operator[](bt2c::CStringView name) const noexcept;
    Iterator begin() const noexcept;
    Iterator end() const noexcept;
    Port front() const noexcept;
    Port back() const noexcept;
};

class SelfSourceComponent final : public internal::SelfSpecificComponent<bt_self_component_source>
{
    using _ThisSelfSpecificComponent = internal::SelfSpecificComponent<bt_self_component_source>;

public:
    using OutputPorts = SelfComponentPorts<bt_self_component_source, bt_self_component_port_output,
                                           const bt_port_output>;

    explicit SelfSourceComponent(bt_self_component_source * const libObjPtr) noexcept :
        SelfSpecificComponent {libObjPtr}
    {
    }

    ConstSourceComponent asConstComponent() const noexcept
    {
        return ConstSourceComponent {
            bt_self_component_source_as_component_source(this->libObjPtr())};
    }

    using _ThisSelfSpecificComponent::data;

    template <typename T>
    SelfSourceComponent data(T& obj) const noexcept
    {
        this->_selfComponent().data(obj);
        return *this;
    }

    template <typename DataT>
    OutputPorts::Port addOutputPort(bt2c::CStringView name, DataT& data) const;

    OutputPorts::Port addOutputPort(bt2c::CStringView name) const;

    OutputPorts outputPorts() const noexcept;

private:
    template <typename DataT>
    OutputPorts::Port _addOutputPort(const char *name, DataT *data) const;
};

class SelfFilterComponent final : public internal::SelfSpecificComponent<bt_self_component_filter>
{
    using _ThisSelfSpecificComponent = internal::SelfSpecificComponent<bt_self_component_filter>;

public:
    using InputPorts = SelfComponentPorts<bt_self_component_filter, bt_self_component_port_input,
                                          const bt_port_input>;
    using OutputPorts = SelfComponentPorts<bt_self_component_filter, bt_self_component_port_output,
                                           const bt_port_output>;

    explicit SelfFilterComponent(bt_self_component_filter * const libObjPtr) noexcept :
        SelfSpecificComponent {libObjPtr}
    {
    }

    ConstFilterComponent asConstComponent() const noexcept
    {
        return ConstFilterComponent {
            bt_self_component_filter_as_component_filter(this->libObjPtr())};
    }

    using _ThisSelfSpecificComponent::data;

    template <typename T>
    SelfFilterComponent data(T& obj) const noexcept
    {
        this->_selfComponent().data(obj);
        return *this;
    }

    template <typename DataT>
    InputPorts::Port addInputPort(bt2c::CStringView name, DataT& data) const;

    InputPorts::Port addInputPort(bt2c::CStringView name) const;

    InputPorts inputPorts() const noexcept;

    template <typename DataT>
    OutputPorts::Port addOutputPort(bt2c::CStringView name, DataT& data) const;

    OutputPorts::Port addOutputPort(bt2c::CStringView name) const;

    OutputPorts outputPorts() const noexcept;

private:
    template <typename DataT>
    InputPorts::Port _addInputPort(const char *name, DataT *data) const;

    template <typename DataT>
    OutputPorts::Port _addOutputPort(const char *name, DataT *data) const;
};

class SelfSinkComponent final : public internal::SelfSpecificComponent<bt_self_component_sink>
{
    using _ThisSelfSpecificComponent = internal::SelfSpecificComponent<bt_self_component_sink>;

public:
    using InputPorts = SelfComponentPorts<bt_self_component_sink, bt_self_component_port_input,
                                          const bt_port_input>;

    explicit SelfSinkComponent(bt_self_component_sink * const libObjPtr) noexcept :
        SelfSpecificComponent {libObjPtr}
    {
    }

    ConstSinkComponent asConstComponent() const noexcept
    {
        return ConstSinkComponent {bt_self_component_sink_as_component_sink(this->libObjPtr())};
    }

    using _ThisSelfSpecificComponent::data;

    template <typename T>
    SelfSinkComponent data(T& obj) const noexcept
    {
        this->_selfComponent().data(obj);
        return *this;
    }

    MessageIterator::Shared createMessageIterator(InputPorts::Port port) const;

    bool isInterrupted() const noexcept
    {
        return static_cast<bool>(bt_self_component_sink_is_interrupted(this->libObjPtr()));
    }

    template <typename DataT>
    InputPorts::Port addInputPort(bt2c::CStringView name, DataT& data) const;

    InputPorts::Port addInputPort(bt2c::CStringView name) const;

    InputPorts inputPorts() const noexcept;

private:
    template <typename DataT>
    InputPorts::Port _addInputPort(const char *name, DataT *data) const;
};

inline SelfComponent::SelfComponent(const SelfSourceComponent other) noexcept :
    SelfComponent {other.libObjPtr()}
{
}

inline SelfComponent::SelfComponent(const SelfFilterComponent other) noexcept :
    SelfComponent {other.libObjPtr()}
{
}

inline SelfComponent::SelfComponent(const SelfSinkComponent other) noexcept :
    SelfComponent {other.libObjPtr()}
{
}

inline SelfComponent SelfComponent::operator=(const SelfSourceComponent other) noexcept
{
    *this = SelfComponent {other.libObjPtr()};
    return *this;
}

inline SelfComponent SelfComponent::operator=(const SelfFilterComponent other) noexcept
{
    *this = SelfComponent {other.libObjPtr()};
    return *this;
}

inline SelfComponent SelfComponent::operator=(const SelfSinkComponent other) noexcept
{
    *this = SelfComponent {other.libObjPtr()};
    return *this;
}

namespace internal {

template <typename LibObjT>
struct SelfComponentPortSpec;

/* Functions specific to self component input ports */
template <>
struct SelfComponentPortSpec<bt_self_component_port_input> final
{
    static bt_self_component_port *
    asSelfCompPort(bt_self_component_port_input * const libObjPtr) noexcept
    {
        return bt_self_component_port_input_as_self_component_port(libObjPtr);
    }

    static const bt_port_input *
    asConstPort(const bt_self_component_port_input * const libObjPtr) noexcept
    {
        return bt_self_component_port_input_as_port_input(libObjPtr);
    }
};

/* Functions specific to self component output ports */
template <>
struct SelfComponentPortSpec<bt_self_component_port_output> final
{
    static bt_self_component_port *
    asSelfCompPort(bt_self_component_port_output * const libObjPtr) noexcept
    {
        return bt_self_component_port_output_as_self_component_port(libObjPtr);
    }

    static const bt_port_output *
    asConstPort(bt_self_component_port_output * const libObjPtr) noexcept
    {
        return bt_self_component_port_output_as_port_output(libObjPtr);
    }
};

} /* namespace internal */

template <typename LibSelfCompPortT, typename LibPortT>
class SelfComponentPort final : public BorrowedObject<LibSelfCompPortT>
{
public:
    using typename BorrowedObject<LibSelfCompPortT>::LibObjPtr;

    explicit SelfComponentPort(const LibObjPtr libObjPtr) noexcept :
        BorrowedObject<LibSelfCompPortT> {libObjPtr}
    {
    }

    ConstPort<LibPortT> asConstPort() const noexcept
    {
        return ConstPort<LibPortT> {
            internal::SelfComponentPortSpec<LibSelfCompPortT>::asConstPort(this->libObjPtr())};
    }

    bt2c::CStringView name() const noexcept
    {
        return this->asConstPort().name();
    }

    bool isConnected() const noexcept
    {
        return this->asConstPort().isConnected();
    }

    SelfComponent component() const noexcept
    {
        return SelfComponent {bt_self_component_port_borrow_component(this->_libSelfCompPortPtr())};
    }

    template <typename T>
    T& data() const noexcept
    {
        return *static_cast<T *>(bt_self_component_port_get_data(this->_libSelfCompPortPtr()));
    }

private:
    bt_self_component_port *_libSelfCompPortPtr() const noexcept
    {
        return internal::SelfComponentPortSpec<LibSelfCompPortT>::asSelfCompPort(this->libObjPtr());
    }
};

template <typename LibSelfCompT, typename LibSelfCompPortT, typename LibPortT>
typename SelfComponentPorts<LibSelfCompT, LibSelfCompPortT, LibPortT>::Port
SelfComponentPorts<LibSelfCompT, LibSelfCompPortT, LibPortT>::operator[](
    const std::uint64_t index) const noexcept
{
    return Port {_Spec::portByIndex(this->libObjPtr(), index)};
}

template <typename LibSelfCompT, typename LibSelfCompPortT, typename LibPortT>
typename SelfComponentPorts<LibSelfCompT, LibSelfCompPortT, LibPortT>::Port
SelfComponentPorts<LibSelfCompT, LibSelfCompPortT, LibPortT>::operator[](
    const bt2c::CStringView name) const noexcept
{
    return Port {_Spec::portByName(this->libObjPtr(), name)};
}

template <typename LibSelfCompT, typename LibSelfCompPortT, typename LibPortT>
typename SelfComponentPorts<LibSelfCompT, LibSelfCompPortT, LibPortT>::Iterator
SelfComponentPorts<LibSelfCompT, LibSelfCompPortT, LibPortT>::begin() const noexcept
{
    return Iterator {*this, 0};
}

template <typename LibSelfCompT, typename LibSelfCompPortT, typename LibPortT>
typename SelfComponentPorts<LibSelfCompT, LibSelfCompPortT, LibPortT>::Iterator
SelfComponentPorts<LibSelfCompT, LibSelfCompPortT, LibPortT>::end() const noexcept
{
    return Iterator {*this, this->length()};
}

template <typename LibSelfCompT, typename LibSelfCompPortT, typename LibPortT>
typename SelfComponentPorts<LibSelfCompT, LibSelfCompPortT, LibPortT>::Port
SelfComponentPorts<LibSelfCompT, LibSelfCompPortT, LibPortT>::front() const noexcept
{
    return (*this)[0];
}

template <typename LibSelfCompT, typename LibSelfCompPortT, typename LibPortT>
typename SelfComponentPorts<LibSelfCompT, LibSelfCompPortT, LibPortT>::Port
SelfComponentPorts<LibSelfCompT, LibSelfCompPortT, LibPortT>::back() const noexcept
{
    return (*this)[this->length() - 1];
}

using SelfComponentInputPort = SelfComponentPort<bt_self_component_port_input, const bt_port_input>;

using SelfComponentOutputPort =
    SelfComponentPort<bt_self_component_port_output, const bt_port_output>;

template <typename DataT>
SelfSourceComponent::OutputPorts::Port SelfSourceComponent::_addOutputPort(const char * const name,
                                                                           DataT * const data) const
{
    return this->_addPort<SelfSourceComponent::OutputPorts::Port, bt_self_component_port_output>(
        name, data, bt_self_component_source_add_output_port);
}

template <typename DataT>
SelfSourceComponent::OutputPorts::Port
SelfSourceComponent::addOutputPort(const bt2c::CStringView name, DataT& data) const
{
    return this->_addOutputPort(name, &data);
}

inline SelfSourceComponent::OutputPorts::Port
SelfSourceComponent::addOutputPort(const bt2c::CStringView name) const
{
    return this->_addOutputPort<void>(name, nullptr);
}

inline SelfSourceComponent::OutputPorts SelfSourceComponent::outputPorts() const noexcept
{
    return OutputPorts {this->libObjPtr()};
}

template <typename DataT>
SelfFilterComponent::OutputPorts::Port SelfFilterComponent::_addOutputPort(const char * const name,
                                                                           DataT * const data) const
{
    return this->_addPort<SelfFilterComponent::OutputPorts::Port, bt_self_component_port_output>(
        name, data, bt_self_component_filter_add_output_port);
}

template <typename DataT>
SelfFilterComponent::OutputPorts::Port
SelfFilterComponent::addOutputPort(const bt2c::CStringView name, DataT& data) const
{
    return this->_addOutputPort(name, &data);
}

inline SelfFilterComponent::OutputPorts::Port
SelfFilterComponent::addOutputPort(const bt2c::CStringView name) const
{
    return this->_addOutputPort<void>(name, nullptr);
}

inline SelfFilterComponent::OutputPorts SelfFilterComponent::outputPorts() const noexcept
{
    return OutputPorts {this->libObjPtr()};
}

template <typename DataT>
SelfFilterComponent::InputPorts::Port SelfFilterComponent::_addInputPort(const char * const name,
                                                                         DataT * const data) const
{
    return this->_addPort<SelfFilterComponent::InputPorts::Port, bt_self_component_port_input>(
        name, data, bt_self_component_filter_add_input_port);
}

template <typename DataT>
SelfFilterComponent::InputPorts::Port
SelfFilterComponent::addInputPort(const bt2c::CStringView name, DataT& data) const
{
    return this->_addInputPort(name, &data);
}

inline SelfFilterComponent::InputPorts::Port
SelfFilterComponent::addInputPort(const bt2c::CStringView name) const
{
    return this->_addInputPort<void>(name, nullptr);
}

inline SelfFilterComponent::InputPorts SelfFilterComponent::inputPorts() const noexcept
{
    return InputPorts {this->libObjPtr()};
}

inline MessageIterator::Shared
SelfSinkComponent::createMessageIterator(const InputPorts::Port port) const
{
    bt_message_iterator *libMsgIterPtr = nullptr;

    const auto status = bt_message_iterator_create_from_sink_component(
        this->libObjPtr(), port.libObjPtr(), &libMsgIterPtr);

    switch (status) {
    case BT_MESSAGE_ITERATOR_CREATE_FROM_SINK_COMPONENT_STATUS_OK:
        return MessageIterator::Shared::createWithoutRef(libMsgIterPtr);
    case BT_MESSAGE_ITERATOR_CREATE_FROM_SINK_COMPONENT_STATUS_MEMORY_ERROR:
        throw MemoryError {};
    case BT_MESSAGE_ITERATOR_CREATE_FROM_SINK_COMPONENT_STATUS_ERROR:
        throw Error {};
    default:
        bt_common_abort();
    }
}

template <typename DataT>
SelfSinkComponent::InputPorts::Port SelfSinkComponent::_addInputPort(const char * const name,
                                                                     DataT * const data) const
{
    return this->_addPort<SelfSinkComponent::InputPorts::Port, bt_self_component_port_input>(
        name, data, bt_self_component_sink_add_input_port);
}

template <typename DataT>
SelfSinkComponent::InputPorts::Port SelfSinkComponent::addInputPort(const bt2c::CStringView name,
                                                                    DataT& data) const
{
    return this->_addInputPort(name, &data);
}

inline SelfSinkComponent::InputPorts::Port
SelfSinkComponent::addInputPort(const bt2c::CStringView name) const
{
    return this->_addInputPort<void>(name, nullptr);
}

inline SelfSinkComponent::InputPorts SelfSinkComponent::inputPorts() const noexcept
{
    return InputPorts {this->libObjPtr()};
}

} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_SELF_COMPONENT_PORT_HPP */
