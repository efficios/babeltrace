/*
 * Copyright (c) 2024 EfficiOS Inc.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef BABELTRACE_CPP_COMMON_BT2_ERROR_HPP
#define BABELTRACE_CPP_COMMON_BT2_ERROR_HPP

#include <cstdint>
#include <memory>

#include <fmt/format.h> /* IWYU pragma: keep */

#include <babeltrace2/babeltrace.h>

#include "common/assert.h"
#include "cpp-common/bt2c/c-string-view.hpp"
#include "cpp-common/vendor/wise-enum/wise_enum.h"

#include "borrowed-object.hpp"
#include "component-class.hpp"

/*!
@file

@brief
    Error management.

@ingroup common-cpp-bt2

@code{.cpp}
#include "cpp-common/bt2/error.hpp"
@endcode

This file offers a C++ version of the error management system of
libbabeltrace2.
*/

namespace bt2 {

class ConstComponentClassErrorCause;
class ConstComponentErrorCause;
class ConstMessageIteratorErrorCause;

/* Avoid `-Wshadow` error on GCC, conflicting with `bt2::ComponentClass` */
BT_DIAG_PUSH
BT_DIAG_IGNORE_SHADOW

/* clang-format off */

/*!
@brief
    Error cause actor type.
*/
WISE_ENUM_CLASS(ErrorCauseActorType,
    /// Unknown.
    (Unknown,           BT_ERROR_CAUSE_ACTOR_TYPE_UNKNOWN),

    /// Component.
    (Component,         BT_ERROR_CAUSE_ACTOR_TYPE_COMPONENT),

    /// Component class.
    (ComponentClass,    BT_ERROR_CAUSE_ACTOR_TYPE_COMPONENT_CLASS),

    /// Message iterator.
    (MessageIterator,   BT_ERROR_CAUSE_ACTOR_TYPE_MESSAGE_ITERATOR))

/* clang-format on */

BT_DIAG_POP

/*!
@brief
    Error cause wrapper.

Wraps a <code>const bt_error_cause *</code> pointer and offers
the corresponding general <code>bt_error_cause_*</code> API.

@note
    @parblock
    The individual methods of this class aren't documented yet, but
    they're straightforward to understand if you already know
    the libbabeltrace2 C&nbsp;API.

    Please see <code>%src/cpp-common/bt2/error.hpp</code>.
    @endparblock
*/
class ConstErrorCause : public BorrowedObject<const bt_error_cause>
{
public:
    explicit ConstErrorCause(const LibObjPtr libObjPtr) noexcept
        : _ThisBorrowedObject {libObjPtr}
    {
    }

    ErrorCauseActorType actorType() const noexcept
    {
        return static_cast<ErrorCauseActorType>(bt_error_cause_get_actor_type(this->libObjPtr()));
    }

    bool actorTypeIsComponentClass() const noexcept
    {
        return this->actorType() == ErrorCauseActorType::ComponentClass;
    }

    bool actorTypeIsComponent() const noexcept
    {
        return this->actorType() == ErrorCauseActorType::Component;
    }

    bool actorTypeIsMessageIterator() const noexcept
    {
        return this->actorType() == ErrorCauseActorType::MessageIterator;
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

/*!
@brief
    Error cause with component class actor wrapper.

Wraps a <code>const bt_error_cause *</code> pointer and offers
the corresponding specific
<code>bt_error_cause_component_class_actor_*</code> API.

@note
    @parblock
    The individual methods of this class aren't documented yet, but
    they're straightforward to understand if you already know
    the libbabeltrace2 C&nbsp;API.

    Please see <code>%src/cpp-common/bt2/error.hpp</code>.
    @endparblock
*/
class ConstComponentClassErrorCause final : public ConstErrorCause
{
public:
    explicit ConstComponentClassErrorCause(const LibObjPtr libObjPtr)
        : ConstErrorCause {libObjPtr}
    {
        BT_ASSERT(this->actorTypeIsComponentClass());
    }

    bt2::ComponentClassType componentClassType() const noexcept
    {
        return static_cast<bt2::ComponentClassType>(
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

/*!
@brief
    Error cause with component actor wrapper.

Wraps a <code>const bt_error_cause *</code> pointer and offers
the corresponding specific
<code>bt_error_cause_component_actor_*</code> API.

@note
    @parblock
    The individual methods of this class aren't documented yet, but
    they're straightforward to understand if you already know
    the libbabeltrace2 C&nbsp;API.

    Please see <code>%src/cpp-common/bt2/error.hpp</code>.
    @endparblock
*/
class ConstComponentErrorCause final : public ConstErrorCause
{
public:
    explicit ConstComponentErrorCause(const LibObjPtr libObjPtr)
        : ConstErrorCause {libObjPtr}
    {
        BT_ASSERT(this->actorTypeIsComponent());
    }

    bt2c::CStringView componentName() const noexcept
    {
        return bt_error_cause_component_actor_get_component_name(this->libObjPtr());
    }

    bt2::ComponentClassType componentClassType() const noexcept
    {
        return static_cast<bt2::ComponentClassType>(
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

/*!
@brief
    Error cause with message iterator actor wrapper.

Wraps a <code>const bt_error_cause *</code> pointer and offers
the corresponding specific
<code>bt_error_cause_message_iterator_actor_*</code> API.

@note
    @parblock
    The individual methods of this class aren't documented yet, but
    they're straightforward to understand if you already know
    the libbabeltrace2 C&nbsp;API.

    Please see <code>%src/cpp-common/bt2/error.hpp</code>.
    @endparblock
*/
class ConstMessageIteratorErrorCause final : public ConstErrorCause
{
public:
    explicit ConstMessageIteratorErrorCause(const LibObjPtr libObjPtr)
        : ConstErrorCause {libObjPtr}
    {
        BT_ASSERT(this->actorTypeIsMessageIterator());
    }

    bt2c::CStringView componentOutputPortName() const noexcept
    {
        return bt_error_cause_message_iterator_actor_get_component_output_port_name(
            this->libObjPtr());
    }

    bt2c::CStringView componentName() const noexcept
    {
        return bt_error_cause_message_iterator_actor_get_component_name(this->libObjPtr());
    }

    bt2::ComponentClassType componentClassType() const noexcept
    {
        return static_cast<bt2::ComponentClassType>(
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
    explicit ConstErrorCauseProxy(const ConstErrorCause cause) noexcept
        : _mCause {cause}
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

namespace internal {

/*
 * Functionality common between `ConstError` and `UniqueConstError`.
 */
template <typename ErrorType>
class ErrorMixin
{
public:
    /*!
    @brief
        Number of causes of this error.

    @returns
        Number of causes of this error.
    */
    std::uint64_t length() const noexcept
    {
        return bt_error_get_cause_count(_libObjPtr());
    }

    /*!
    @brief
        Cause of this error at the index \bt_p{index}.

    @param[in] index
        Index of the cause to borrow.

    @returns
        Borrowed cause at the index \bt_p{index} of this error.

    @pre
        \bt_p{index} is less than what length() returns.
    */
    ConstErrorCause operator[](const std::uint64_t index) const noexcept
    {
        return ConstErrorCause {bt_error_borrow_cause_by_index(_libObjPtr(), index)};
    }

    /*!
    @brief
        Iterator at the first cause of this error.

    @returns
        Iterator at the first cause of this error.
    */
    ConstErrorIterator begin() const noexcept;

    /*!
    @brief
        Iterator \em after the last cause of this error.

    @returns
        Iterator \em after the last cause of this error.
    */
    ConstErrorIterator end() const noexcept;

private:
    const bt_error *_libObjPtr() const noexcept
    {
        return static_cast<const ErrorType *>(this)->libObjPtr();
    }
};

}; /* namespace internal */

/*!
@brief
    Unique error wrapper.

This class wraps a <code>const bt_error *</code> pointer, as returned
by <code>bt_current_thread_take_error()</code>.

You may not copy an instance of #UniqueConstError.

Get the error of the current thread with takeCurrentThreadError().

A #UniqueConstError instance may be empty: use its
operator bool() to check its existence.

Destroying a #UniqueConstError instance releases (frees) the wrapped
thread error with <code>bt_error_release()</code>.

Move back the wrapped thread error to the current thread
with moveErrorToCurrentThread().
*/
class UniqueConstError final : public internal::ErrorMixin<UniqueConstError>
{
public:
    /// libbabeltrace2 raw object pointer type.
    using LibObjPtr = const bt_error *;

    /*!
    @brief
        Builds a unique error to wrap the libbabeltrace2 thread error
        pointer \bt_p{libError}.

    @param[in] libError
        libbabeltrace2 thread error pointer to wrap.

    @bt_pre_not_null{libObjPtr}
    */
    explicit UniqueConstError(const LibObjPtr libError) noexcept
        : _mLibError {libError}
    {
    }

    /*!
    @brief
        Whether or not this error exists.

    @returns
        \c true if this error exists.
    */
    explicit operator bool() const noexcept
    {
        return this->libObjPtr();
    }

    /*!
    @brief
        Wrapped libbabeltrace2 thread error pointer.

    @attention
        Do \em not call <code>bt_current_thread_move_error()</code>
        or <code>bt_error_release()</code> with this pointer as this
        wrapper won't know.

    @returns
        Wrapped libbabeltrace2 thread error pointer.
    */
    LibObjPtr libObjPtr() const noexcept
    {
        return _mLibError.get();
    }

    /*!
    @brief
        Makes this wrapper stop managing the thread error pointer,
        returning it.

    After this call, operator bool() returns <code>false</code>.

    @returns
        Raw thread error pointer.
    */
    LibObjPtr release() noexcept
    {
        return _mLibError.release();
    }

private:
    struct _LibErrorDeleter final
    {
        void operator()(const LibObjPtr libError) const noexcept
        {
            bt_error_release(libError);
        }
    };

    std::unique_ptr<std::remove_pointer_t<LibObjPtr>, _LibErrorDeleter> _mLibError;
};

/*!
@brief
    Error wrapper.

This class wraps a <code>const bt_error *</code> pointer without
managing it: unlike #UniqueConstError, destroying a #ConstError instance
does \em not release the wrapped error.

Use this class to wrap a <code>const bt_error *</code> pointer which you
don't own.

Unlike #UniqueConstError, a #ConstError instance may never be empty: it
always wraps a valid error.

You may build a #ConstError from a #UniqueConstError, borrowing its
wrapped pointer.
*/
class ConstError final : public internal::ErrorMixin<ConstError>
{
public:
    /// libbabeltrace2 raw object pointer type.
    using LibObjPtr = const bt_error *;

    /*!
    @brief
        Builds an error to wrap the libbabeltrace2 error pointer
        \bt_p{libError}.

    @param[in] libError
        libbabeltrace2 error pointer to wrap.

    @bt_pre_not_null{libError}
    */
    explicit ConstError(const LibObjPtr libError) noexcept
        : _mLibError {libError}
    {
        BT_ASSERT(libError);
    }

    /*!
    @brief
        Builds an error to wrap the libbabeltrace2 error pointer of the
        unique error \bt_p{unique}.

    @param[in] unique
        Unique error of which to borrow the wrapped pointer.

    @pre
        \bt_p{unique} isn't empty.

    @note
        Intentionally not explicit.
    */
    ConstError(const UniqueConstError& unique) noexcept
        : _mLibError {unique.libObjPtr()}
    {
        BT_ASSERT(_mLibError);
    }

    /*!
    @brief
        Wrapped libbabeltrace2 error pointer.

    @returns
        Wrapped libbabeltrace2 error pointer.
    */
    LibObjPtr libObjPtr() const noexcept
    {
        return _mLibError;
    }

private:
    LibObjPtr _mLibError;
};

/*!
@brief
    Error iterator (provides causes).

@note
    @parblock
    The individual methods of this class aren't documented yet, but
    they're straightforward to understand if you know the iterator
    concept.

    Please see <code>%src/cpp-common/bt2/error.hpp</code>.
    @endparblock
*/
class ConstErrorIterator final
{
    friend internal::ErrorMixin<ConstError>;
    friend internal::ErrorMixin<UniqueConstError>;

private:
    explicit ConstErrorIterator(ConstError error, const std::uint64_t index) noexcept
        : _mError {error},
          _mIndex {index}
    {
    }

public:
    bool operator==(const ConstErrorIterator& other) const noexcept
    {
        BT_ASSERT(other._mError.libObjPtr() == _mError.libObjPtr());
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

    ConstErrorCause operator*() const noexcept
    {
        return _mError[_mIndex];
    }

    ConstErrorCauseProxy operator->() const noexcept
    {
        return ConstErrorCauseProxy {**this};
    }

private:
    ConstError _mError;
    std::uint64_t _mIndex;
};

namespace internal {

template <typename ErrorType>
ConstErrorIterator ErrorMixin<ErrorType>::begin() const noexcept
{
    BT_ASSERT(_libObjPtr());
    return ConstErrorIterator {ConstError {*static_cast<const ErrorType *>(this)}, 0};
}

template <typename ErrorType>
ConstErrorIterator ErrorMixin<ErrorType>::end() const noexcept
{
    BT_ASSERT(_libObjPtr());
    return ConstErrorIterator {ConstError {*static_cast<const ErrorType *>(this)}, this->length()};
}

} /* namespace internal */

/*!
@brief
    Returns a unique error using
    <code>bt_current_thread_take_error()</code>.

@returns
    Current thread error which can be empty.
*/
inline UniqueConstError takeCurrentThreadError() noexcept
{
    return UniqueConstError {bt_current_thread_take_error()};
}

/*!
@brief
    Moves the error \bt_p{error} back to the current thread using
    <code>bt_current_thread_move_error()</code>.

After calling this function, \bt_p{error} is empty.

@param[in] error
    Error to move back to the current thread.
*/
inline void moveErrorToCurrentThread(UniqueConstError error) noexcept
{
    bt_current_thread_move_error(error.release());
}

} /* namespace bt2 */

#endif /* BABELTRACE_CPP_COMMON_BT2_ERROR_HPP */
