/*
 * Copyright (c) 2023 Philippe Proulx <pproulx@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_COMPONENT_PORT_HPP
#define BABELTRACE_CPP_COMMON_BT2_COMPONENT_PORT_HPP

#include <cstdint>

#include <babeltrace2/babeltrace.h>

#include "logging.hpp"

#include "cpp-common/bt2c/c-string-view.hpp"

#include "borrowed-object-iterator.hpp"
#include "borrowed-object.hpp"
#include "shared-object.hpp"

namespace bt2 {
namespace internal {

struct ConstComponentRefFuncs final
{
    static void get(const bt_component * const libObjPtr) noexcept
    {
        bt_component_get_ref(libObjPtr);
    }

    static void put(const bt_component * const libObjPtr) noexcept
    {
        bt_component_put_ref(libObjPtr);
    }
};

} /* namespace internal */

class ConstSourceComponent;
class ConstFilterComponent;
class ConstSinkComponent;

class ConstComponent final : public BorrowedObject<const bt_component>
{
private:
    using typename BorrowedObject<const bt_component>::_ThisBorrowedObject;

public:
    using Shared =
        SharedObject<ConstComponent, const bt_component, internal::ConstComponentRefFuncs>;

    explicit ConstComponent(const bt_component * const libObjPtr) noexcept :
        _ThisBorrowedObject {libObjPtr}
    {
    }

    explicit ConstComponent(const bt_component_source * const libObjPtr) noexcept :
        _ThisBorrowedObject {bt_component_source_as_component_const(libObjPtr)}
    {
    }

    explicit ConstComponent(const bt_component_filter * const libObjPtr) noexcept :
        _ThisBorrowedObject {bt_component_filter_as_component_const(libObjPtr)}
    {
    }

    explicit ConstComponent(const bt_component_sink * const libObjPtr) noexcept :
        _ThisBorrowedObject {bt_component_sink_as_component_const(libObjPtr)}
    {
    }

    /* Not `explicit` to make them behave like copy constructors */
    ConstComponent(ConstSourceComponent other) noexcept;
    ConstComponent(ConstFilterComponent other) noexcept;
    ConstComponent(ConstSinkComponent other) noexcept;

    ConstComponent operator=(ConstSourceComponent other) noexcept;
    ConstComponent operator=(ConstFilterComponent other) noexcept;
    ConstComponent operator=(ConstSinkComponent other) noexcept;

    bool isSource() const noexcept
    {
        return static_cast<bool>(bt_component_is_source(this->libObjPtr()));
    }

    bool isFilter() const noexcept
    {
        return static_cast<bool>(bt_component_is_filter(this->libObjPtr()));
    }

    bool isSink() const noexcept
    {
        return static_cast<bool>(bt_component_is_sink(this->libObjPtr()));
    }

    bt2c::CStringView name() const noexcept
    {
        return bt_component_get_name(this->libObjPtr());
    }

    LoggingLevel loggingLevel() const noexcept
    {
        return static_cast<LoggingLevel>(bt_component_get_logging_level(this->libObjPtr()));
    }

    Shared shared() const noexcept
    {
        return Shared::createWithRef(*this);
    }
};

template <typename LibObjT>
class ConstSpecificComponent : public BorrowedObject<LibObjT>
{
public:
    using typename BorrowedObject<LibObjT>::LibObjPtr;

protected:
    explicit ConstSpecificComponent(const LibObjPtr libObjPtr) noexcept :
        BorrowedObject<LibObjT> {libObjPtr}
    {
    }

public:
    bt2c::CStringView name() const noexcept
    {
        return this->_constComponent().name();
    }

    LoggingLevel loggingLevel() const noexcept
    {
        return this->_constComponent().loggingLevel();
    }

    ConstComponent::Shared sharedComponent() const noexcept
    {
        return this->_constComponent().shared();
    }

private:
    ConstComponent _constComponent() const noexcept
    {
        return ConstComponent {this->libObjPtr()};
    }
};

namespace internal {

template <typename LibCompT, typename LibPortPtrT>
struct ConstComponentPortsSpec;

template <>
struct ConstComponentPortsSpec<const bt_component_source, const bt_port_output> final
{
    static std::uint64_t portCount(const bt_component_source * const libCompPtr) noexcept
    {
        return bt_component_source_get_output_port_count(libCompPtr);
    }

    static const bt_port_output *portByIndex(const bt_component_source * const libCompPtr,
                                             const std::uint64_t index) noexcept
    {
        return bt_component_source_borrow_output_port_by_index_const(libCompPtr, index);
    }

    static const bt_port_output *portByName(const bt_component_source * const libCompPtr,
                                            const char * const name) noexcept
    {
        return bt_component_source_borrow_output_port_by_name_const(libCompPtr, name);
    }
};

template <>
struct ConstComponentPortsSpec<const bt_component_filter, const bt_port_output> final
{
    static std::uint64_t portCount(const bt_component_filter * const libCompPtr) noexcept
    {
        return bt_component_filter_get_output_port_count(libCompPtr);
    }

    static const bt_port_output *portByIndex(const bt_component_filter * const libCompPtr,
                                             const std::uint64_t index) noexcept
    {
        return bt_component_filter_borrow_output_port_by_index_const(libCompPtr, index);
    }

    static const bt_port_output *portByName(const bt_component_filter * const libCompPtr,
                                            const char * const name) noexcept
    {
        return bt_component_filter_borrow_output_port_by_name_const(libCompPtr, name);
    }
};

template <>
struct ConstComponentPortsSpec<const bt_component_filter, const bt_port_input> final
{
    static std::uint64_t portCount(const bt_component_filter * const libCompPtr) noexcept
    {
        return bt_component_filter_get_input_port_count(libCompPtr);
    }

    static const bt_port_input *portByIndex(const bt_component_filter * const libCompPtr,
                                            const std::uint64_t index) noexcept
    {
        return bt_component_filter_borrow_input_port_by_index_const(libCompPtr, index);
    }

    static const bt_port_input *portByName(const bt_component_filter * const libCompPtr,
                                           const char * const name) noexcept
    {
        return bt_component_filter_borrow_input_port_by_name_const(libCompPtr, name);
    }
};

template <>
struct ConstComponentPortsSpec<const bt_component_sink, const bt_port_input> final
{
    static std::uint64_t portCount(const bt_component_sink * const libCompPtr) noexcept
    {
        return bt_component_sink_get_input_port_count(libCompPtr);
    }

    static const bt_port_input *portByIndex(const bt_component_sink * const libCompPtr,
                                            const std::uint64_t index) noexcept
    {
        return bt_component_sink_borrow_input_port_by_index_const(libCompPtr, index);
    }

    static const bt_port_input *portByName(const bt_component_sink * const libCompPtr,
                                           const char * const name) noexcept
    {
        return bt_component_sink_borrow_input_port_by_name_const(libCompPtr, name);
    }
};

} /* namespace internal */

template <typename>
class ConstPort;

template <typename LibCompT, typename LibPortT>
class ConstComponentPorts final : public BorrowedObject<LibCompT>
{
private:
    using _Spec = internal::ConstComponentPortsSpec<LibCompT, LibPortT>;

public:
    using typename BorrowedObject<LibCompT>::LibObjPtr;
    using Port = ConstPort<LibPortT>;
    using Iterator = BorrowedObjectIterator<ConstComponentPorts>;

    explicit ConstComponentPorts(const LibObjPtr libObjPtr) noexcept :
        BorrowedObject<LibCompT> {libObjPtr}
    {
    }

    std::uint64_t length() const noexcept
    {
        return _Spec::portCount(this->libObjPtr());
    }

    Port operator[](std::uint64_t index) const noexcept;
    OptionalBorrowedObject<Port> operator[](bt2c::CStringView name) const noexcept;
    Iterator begin() const noexcept;
    Iterator end() const noexcept;
};

namespace internal {

struct ConstSourceComponentRefFuncs final
{
    static void get(const bt_component_source * const libObjPtr) noexcept
    {
        bt_component_source_get_ref(libObjPtr);
    }

    static void put(const bt_component_source * const libObjPtr) noexcept
    {
        bt_component_source_put_ref(libObjPtr);
    }
};

} /* namespace internal */

class ConstSourceComponent final : public ConstSpecificComponent<const bt_component_source>
{
public:
    using Shared = SharedObject<ConstSourceComponent, const bt_component_source,
                                internal::ConstSourceComponentRefFuncs>;

    using OutputPorts = ConstComponentPorts<const bt_component_source, const bt_port_output>;

    explicit ConstSourceComponent(const bt_component_source * const libObjPtr) noexcept :
        ConstSpecificComponent {libObjPtr}
    {
    }

    OutputPorts outputPorts() const noexcept;

    Shared shared() const noexcept
    {
        return Shared::createWithRef(*this);
    }
};

namespace internal {

struct ConstFilterComponentRefFuncs final
{
    static void get(const bt_component_filter * const libObjPtr) noexcept
    {
        bt_component_filter_get_ref(libObjPtr);
    }

    static void put(const bt_component_filter * const libObjPtr) noexcept
    {
        bt_component_filter_put_ref(libObjPtr);
    }
};

} /* namespace internal */

class ConstFilterComponent final : public ConstSpecificComponent<const bt_component_filter>
{
public:
    using Shared = SharedObject<ConstFilterComponent, const bt_component_filter,
                                internal::ConstFilterComponentRefFuncs>;

    using InputPorts = ConstComponentPorts<const bt_component_filter, const bt_port_input>;
    using OutputPorts = ConstComponentPorts<const bt_component_filter, const bt_port_output>;

    explicit ConstFilterComponent(const bt_component_filter * const libObjPtr) noexcept :
        ConstSpecificComponent {libObjPtr}
    {
    }

    InputPorts inputPorts() const noexcept;
    OutputPorts outputPorts() const noexcept;

    Shared shared() const noexcept
    {
        return Shared::createWithRef(*this);
    }
};

namespace internal {

struct ConstSinkComponentRefFuncs final
{
    static void get(const bt_component_sink * const libObjPtr) noexcept
    {
        bt_component_sink_get_ref(libObjPtr);
    }

    static void put(const bt_component_sink * const libObjPtr) noexcept
    {
        bt_component_sink_put_ref(libObjPtr);
    }
};

} /* namespace internal */

class ConstSinkComponent final : public ConstSpecificComponent<const bt_component_sink>
{
public:
    using Shared = SharedObject<ConstSinkComponent, const bt_component_sink,
                                internal::ConstSinkComponentRefFuncs>;

    using InputPorts = ConstComponentPorts<const bt_component_sink, const bt_port_input>;

    explicit ConstSinkComponent(const bt_component_sink * const libObjPtr) noexcept :
        ConstSpecificComponent {libObjPtr}
    {
    }

    InputPorts inputPorts() const noexcept;

    Shared shared() const noexcept
    {
        return Shared::createWithRef(*this);
    }
};

inline ConstComponent::ConstComponent(const ConstSourceComponent other) noexcept :
    ConstComponent {other.libObjPtr()}
{
}

inline ConstComponent::ConstComponent(const ConstFilterComponent other) noexcept :
    ConstComponent {other.libObjPtr()}
{
}

inline ConstComponent::ConstComponent(const ConstSinkComponent other) noexcept :
    ConstComponent {other.libObjPtr()}
{
}

inline ConstComponent ConstComponent::operator=(const ConstSourceComponent other) noexcept
{
    *this = ConstComponent {other.libObjPtr()};
    return *this;
}

inline ConstComponent ConstComponent::operator=(const ConstFilterComponent other) noexcept
{
    *this = ConstComponent {other.libObjPtr()};
    return *this;
}

inline ConstComponent ConstComponent::operator=(const ConstSinkComponent other) noexcept
{
    *this = ConstComponent {other.libObjPtr()};
    return *this;
}

namespace internal {

template <typename LibObjT>
struct ConstPortSpec;

/* Functions specific to constant input ports */
template <>
struct ConstPortSpec<const bt_port_input> final
{
    static const bt_port *asPort(const bt_port_input * const libObjPtr) noexcept
    {
        return bt_port_input_as_port_const(libObjPtr);
    }
};

/* Functions specific to constant output ports */
template <>
struct ConstPortSpec<const bt_port_output> final
{
    static const bt_port *asPort(const bt_port_output * const libObjPtr) noexcept
    {
        return bt_port_output_as_port_const(libObjPtr);
    }
};

template <typename LibObjT>
struct ConstPortRefFuncs final
{
    static void get(LibObjT * const libObjPtr) noexcept
    {
        bt_port_get_ref(ConstPortSpec<LibObjT>::port(libObjPtr));
    }

    static void put(LibObjT * const libObjPtr) noexcept
    {
        bt_port_put_ref(ConstPortSpec<LibObjT>::port(libObjPtr));
    }
};

} /* namespace internal */

template <typename LibObjT>
class ConstPort final : public BorrowedObject<LibObjT>
{
public:
    using typename BorrowedObject<LibObjT>::LibObjPtr;
    using Shared = SharedObject<ConstPort, LibObjT, internal::ConstPortRefFuncs<LibObjT>>;

    explicit ConstPort(const LibObjPtr libObjPtr) noexcept : BorrowedObject<LibObjT> {libObjPtr}
    {
    }

    bt2c::CStringView name() const noexcept
    {
        return bt_port_get_name(this->_libConstPortPtr());
    }

    bool isConnected() const noexcept
    {
        return static_cast<bool>(bt_port_is_connected(this->_libConstPortPtr()));
    }

    ConstComponent component() const noexcept
    {
        return ConstComponent {bt_port_borrow_component_const(this->_libConstPortPtr())};
    }

    Shared shared() const noexcept
    {
        return Shared::createWithRef(*this);
    }

private:
    const bt_port *_libConstPortPtr() const noexcept
    {
        return internal::ConstPortSpec<LibObjT>::asPort(this->libObjPtr());
    }
};

template <typename LibCompT, typename LibPortT>
typename ConstComponentPorts<LibCompT, LibPortT>::Port
ConstComponentPorts<LibCompT, LibPortT>::operator[](const std::uint64_t index) const noexcept
{
    return Port {_Spec::portByIndex(this->libObjPtr(), index)};
}

template <typename LibCompT, typename LibPortT>
OptionalBorrowedObject<typename ConstComponentPorts<LibCompT, LibPortT>::Port>
ConstComponentPorts<LibCompT, LibPortT>::operator[](const bt2c::CStringView name) const noexcept
{
    return _Spec::portByName(this->libObjPtr(), name);
}

template <typename LibCompT, typename LibPortT>
typename ConstComponentPorts<LibCompT, LibPortT>::Iterator
ConstComponentPorts<LibCompT, LibPortT>::begin() const noexcept
{
    return Iterator {*this, 0};
}

template <typename LibCompT, typename LibPortT>
typename ConstComponentPorts<LibCompT, LibPortT>::Iterator
ConstComponentPorts<LibCompT, LibPortT>::end() const noexcept
{
    return Iterator {*this, this->length()};
}

using ConstInputPort = ConstPort<const bt_port_input>;
using ConstOutputPort = ConstPort<const bt_port_output>;

inline ConstSourceComponent::OutputPorts ConstSourceComponent::outputPorts() const noexcept
{
    return OutputPorts {this->libObjPtr()};
}

inline ConstFilterComponent::OutputPorts ConstFilterComponent::outputPorts() const noexcept
{
    return OutputPorts {this->libObjPtr()};
}

inline ConstFilterComponent::InputPorts ConstFilterComponent::inputPorts() const noexcept
{
    return InputPorts {this->libObjPtr()};
}

inline ConstSinkComponent::InputPorts ConstSinkComponent::inputPorts() const noexcept
{
    return InputPorts {this->libObjPtr()};
}

} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_COMPONENT_PORT_HPP */
