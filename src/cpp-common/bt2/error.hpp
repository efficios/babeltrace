/*
 * Copyright (c) 2024 EfficiOS Inc.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_ERROR_HPP
#define BABELTRACE_CPP_COMMON_BT2_ERROR_HPP

#include <cstdint>
#include <memory>

#include <babeltrace2/babeltrace.h>

#include "common/assert.h"
#include "cpp-common/bt2c/c-string-view.hpp"
#include "cpp-common/vendor/fmt/format.h" /* IWYU pragma: keep */

#include "borrowed-object.hpp"
#include "component-class.hpp"

namespace bt2 {

class ConstComponentClassErrorCause;
class ConstComponentErrorCause;
class ConstMessageIteratorErrorCause;

class ConstErrorCause : public BorrowedObject<const bt_error_cause>
{
public:
    enum class ActorType
    {
        UNKNOWN = BT_ERROR_CAUSE_ACTOR_TYPE_UNKNOWN,
        COMPONENT = BT_ERROR_CAUSE_ACTOR_TYPE_COMPONENT,
        COMPONENT_CLASS = BT_ERROR_CAUSE_ACTOR_TYPE_COMPONENT_CLASS,
        MESSAGE_ITERATOR = BT_ERROR_CAUSE_ACTOR_TYPE_MESSAGE_ITERATOR,
    };

    explicit ConstErrorCause(const LibObjPtr libObjPtr) noexcept : _ThisBorrowedObject {libObjPtr}
    {
    }

    ActorType actorType() const noexcept
    {
        return static_cast<ActorType>(bt_error_cause_get_actor_type(this->libObjPtr()));
    }

    bool actorTypeIsComponentClass() const noexcept
    {
        return this->actorType() == ActorType::COMPONENT_CLASS;
    }

    bool actorTypeIsComponent() const noexcept
    {
        return this->actorType() == ActorType::COMPONENT;
    }

    bool actorTypeIsMessageIterator() const noexcept
    {
        return this->actorType() == ActorType::MESSAGE_ITERATOR;
    }

    ConstComponentClassErrorCause asComponentClass() const noexcept;
    ConstComponentErrorCause asComponent() const noexcept;
    ConstMessageIteratorErrorCause asMessageIterator() const noexcept;

    bt2c::CStringView message() const noexcept
    {
        return bt_error_cause_get_message(this->libObjPtr());
    }

    bt2c::CStringView moduleName() const noexcept
    {
        return bt_error_cause_get_module_name(this->libObjPtr());
    }

    bt2c::CStringView fileName() const noexcept
    {
        return bt_error_cause_get_file_name(this->libObjPtr());
    }

    std::uint64_t lineNumber() const noexcept
    {
        return bt_error_cause_get_line_number(this->libObjPtr());
    }
};

class ConstComponentClassErrorCause final : public ConstErrorCause
{
public:
    explicit ConstComponentClassErrorCause(const LibObjPtr libObjPtr) : ConstErrorCause {libObjPtr}
    {
        BT_ASSERT(this->actorTypeIsComponentClass());
    }

    bt2::ComponentClass::Type componentClassType() const noexcept
    {
        return static_cast<bt2::ComponentClass::Type>(
            bt_error_cause_component_class_actor_get_component_class_type(this->libObjPtr()));
    }

    bt2c::CStringView componentClassName() const noexcept
    {
        return bt_error_cause_component_class_actor_get_component_class_name(this->libObjPtr());
    }

    bt2c::CStringView pluginName() const noexcept
    {
        return bt_error_cause_component_class_actor_get_plugin_name(this->libObjPtr());
    }
};

inline ConstComponentClassErrorCause ConstErrorCause::asComponentClass() const noexcept
{
    return ConstComponentClassErrorCause {this->libObjPtr()};
}

class ConstComponentErrorCause final : public ConstErrorCause
{
public:
    explicit ConstComponentErrorCause(const LibObjPtr libObjPtr) : ConstErrorCause {libObjPtr}
    {
        BT_ASSERT(this->actorTypeIsComponent());
    }

    bt2c::CStringView componentName() const noexcept
    {
        return bt_error_cause_component_actor_get_component_name(this->libObjPtr());
    }

    bt2::ComponentClass::Type componentClassType() const noexcept
    {
        return static_cast<bt2::ComponentClass::Type>(
            bt_error_cause_component_actor_get_component_class_type(this->libObjPtr()));
    }

    bt2c::CStringView componentClassName() const noexcept
    {
        return bt_error_cause_component_actor_get_component_class_name(this->libObjPtr());
    }

    bt2c::CStringView pluginName() const noexcept
    {
        return bt_error_cause_component_actor_get_plugin_name(this->libObjPtr());
    }
};

inline ConstComponentErrorCause ConstErrorCause::asComponent() const noexcept
{
    return ConstComponentErrorCause {this->libObjPtr()};
}

class ConstMessageIteratorErrorCause final : public ConstErrorCause
{
public:
    explicit ConstMessageIteratorErrorCause(const LibObjPtr libObjPtr) : ConstErrorCause {libObjPtr}
    {
        BT_ASSERT(this->actorTypeIsMessageIterator());
    }

    bt2c::CStringView componentOutputPortName() const noexcept
    {
        return bt_error_cause_message_iterator_actor_get_component_name(this->libObjPtr());
    }

    bt2c::CStringView componentName() const noexcept
    {
        return bt_error_cause_message_iterator_actor_get_component_name(this->libObjPtr());
    }

    bt2::ComponentClass::Type componentClassType() const noexcept
    {
        return static_cast<bt2::ComponentClass::Type>(
            bt_error_cause_message_iterator_actor_get_component_class_type(this->libObjPtr()));
    }

    bt2c::CStringView componentClassName() const noexcept
    {
        return bt_error_cause_message_iterator_actor_get_component_class_name(this->libObjPtr());
    }

    bt2c::CStringView pluginName() const noexcept
    {
        return bt_error_cause_message_iterator_actor_get_plugin_name(this->libObjPtr());
    }
};

inline ConstMessageIteratorErrorCause ConstErrorCause::asMessageIterator() const noexcept
{
    return ConstMessageIteratorErrorCause {this->libObjPtr()};
}

class ConstErrorIterator;

class ConstErrorCauseProxy final
{
    friend ConstErrorIterator;

private:
    explicit ConstErrorCauseProxy(const ConstErrorCause cause) noexcept : _mCause {cause}
    {
    }

public:
    const ConstErrorCause *operator->() const noexcept
    {
        return &_mCause;
    }

private:
    ConstErrorCause _mCause;
};

class UniqueConstError;

class ConstErrorIterator final
{
    friend UniqueConstError;

private:
    explicit ConstErrorIterator(const UniqueConstError& error, const std::uint64_t index) noexcept :
        _mError {error}, _mIndex {index}
    {
    }

public:
    bool operator==(const ConstErrorIterator& other) const noexcept
    {
        BT_ASSERT(&other._mError == &_mError);
        return other._mIndex == _mIndex;
    }

    bool operator!=(const ConstErrorIterator& other) const noexcept
    {
        return !(*this == other);
    }

    ConstErrorIterator& operator++() noexcept
    {
        ++_mIndex;
        return *this;
    }

    ConstErrorIterator operator++(int) noexcept
    {
        const auto ret = *this;

        ++_mIndex;
        return ret;
    }

    ConstErrorCause operator*() const noexcept;

    ConstErrorCauseProxy operator->() const noexcept
    {
        return ConstErrorCauseProxy {**this};
    }

private:
    const UniqueConstError& _mError;
    std::uint64_t _mIndex;
};

class UniqueConstError final
{
public:
    using LibObjPtr = const bt_error *;

    explicit UniqueConstError(const LibObjPtr libError) noexcept : _mLibError {libError}
    {
    }

    explicit operator bool() const noexcept
    {
        return this->libObjPtr();
    }

    LibObjPtr libObjPtr() const noexcept
    {
        return _mLibError.get();
    }

    LibObjPtr release() noexcept
    {
        return _mLibError.release();
    }

    std::uint64_t length() const noexcept
    {
        return bt_error_get_cause_count(this->libObjPtr());
    }

    ConstErrorCause operator[](const std::uint64_t index) const noexcept
    {
        return ConstErrorCause {bt_error_borrow_cause_by_index(this->libObjPtr(), index)};
    }

    ConstErrorIterator begin() const noexcept
    {
        BT_ASSERT(_mLibError);
        return ConstErrorIterator {*this, 0};
    }

    ConstErrorIterator end() const noexcept
    {
        BT_ASSERT(_mLibError);
        return ConstErrorIterator {*this, this->length()};
    }

private:
    struct _LibErrorDeleter final
    {
        void operator()(const LibObjPtr libError) const noexcept
        {
            bt_error_release(libError);
        }
    };

    std::unique_ptr<std::remove_pointer<LibObjPtr>::type, _LibErrorDeleter> _mLibError;
};

inline ConstErrorCause ConstErrorIterator::operator*() const noexcept
{
    return _mError[_mIndex];
}

inline UniqueConstError takeCurrentThreadError() noexcept
{
    return UniqueConstError {bt_current_thread_take_error()};
}

inline void moveErrorToCurrentThread(UniqueConstError error) noexcept
{
    bt_current_thread_move_error(error.release());
}

} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_ERROR_HPP */
