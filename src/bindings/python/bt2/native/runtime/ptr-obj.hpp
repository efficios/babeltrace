/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#ifndef BABELTRACE_BINDINGS_PYTHON_BT2_NATIVE_RUNTIME_PTR_OBJ_HPP
#define BABELTRACE_BINDINGS_PYTHON_BT2_NATIVE_RUNTIME_PTR_OBJ_HPP

#include <Python.h>

#include <type_traits>

#include <babeltrace2/babeltrace.h>

#include "common/assert.h"
#include "cpp-common/bt2/wrap.hpp"

namespace bt2py {

/*
 * Type tag of a libbabeltrace2 object type.
 *
 * A `PtrObj` Python object refers to exactly one type tag. Converting a
 * `PtrObj` object back to a libbabeltrace2 object pointer checks
 * this tag.
 *
 * The tag of `T` and `const T` is the same: the Python layer freely
 * passes pointers to const objects where the C API expects
 * non-const ones.
 */
struct LibObjTypeTag final
{
    const char *name;
};

namespace internal {

/*
 * Name of the libbabeltrace2 object type `LibObjT` (without `const`).
 *
 * Specialize for each type of which a `PtrObj` object may hold a
 * pointer: `nullptr` means not supported.
 */
template <typename LibObjT>
inline constexpr const char *libObjTypeName = nullptr;

template <>
inline constexpr const char *libObjTypeName<bt_plugin_provider_set> = "bt_plugin_provider_set";

template <>
inline constexpr const char *libObjTypeName<bt_plugin_provider> = "bt_plugin_provider";

template <>
inline constexpr const char *libObjTypeName<bt_plugin> = "bt_plugin";

template <>
inline constexpr const char *libObjTypeName<bt_plugin_set> = "bt_plugin_set";

template <>
inline constexpr const char *libObjTypeName<bt_connection> = "bt_connection";

template <>
inline constexpr const char *libObjTypeName<bt_port_input> = "bt_port_input";

template <>
inline constexpr const char *libObjTypeName<bt_port_output> = "bt_port_output";

template <>
inline constexpr const char *libObjTypeName<bt_value> = "bt_value";

template <>
inline constexpr const char *libObjTypeName<bt_clock_class> = "bt_clock_class";

template <>
inline constexpr const char *libObjTypeName<bt_clock_snapshot> = "bt_clock_snapshot";

template <>
inline constexpr const char *libObjTypeName<bt_self_component> = "bt_self_component";

template <>
inline constexpr const char *libObjTypeName<bt_stream_class> = "bt_stream_class";

template <>
inline constexpr const char *libObjTypeName<bt_trace_class> = "bt_trace_class";

template <>
inline constexpr const char *libObjTypeName<bt_stream> = "bt_stream";

template <>
inline constexpr const char *libObjTypeName<bt_trace> = "bt_trace";

template <>
inline constexpr const char *libObjTypeName<bt_event_class> = "bt_event_class";

template <>
inline constexpr const char *libObjTypeName<bt_field_class> = "bt_field_class";

template <>
inline constexpr const char *libObjTypeName<bt_field> = "bt_field";

template <>
inline constexpr const char *libObjTypeName<bt_field_class_bit_array_flag> =
    "bt_field_class_bit_array_flag";

template <>
inline constexpr const char *libObjTypeName<bt_field_class_enumeration_mapping> =
    "bt_field_class_enumeration_mapping";

template <>
inline constexpr const char *libObjTypeName<bt_field_class_enumeration_unsigned_mapping> =
    "bt_field_class_enumeration_unsigned_mapping";

template <>
inline constexpr const char *libObjTypeName<bt_field_class_enumeration_signed_mapping> =
    "bt_field_class_enumeration_signed_mapping";

template <>
inline constexpr const char *libObjTypeName<bt_field_class_structure_member> =
    "bt_field_class_structure_member";

template <>
inline constexpr const char *libObjTypeName<bt_field_class_variant_option> =
    "bt_field_class_variant_option";

template <>
inline constexpr const char
    *libObjTypeName<bt_field_class_variant_with_selector_field_integer_unsigned_option> =
        "bt_field_class_variant_with_selector_field_integer_unsigned_option";

template <>
inline constexpr const char
    *libObjTypeName<bt_field_class_variant_with_selector_field_integer_signed_option> =
        "bt_field_class_variant_with_selector_field_integer_signed_option";

template <>
inline constexpr const char *libObjTypeName<bt_event> = "bt_event";

template <>
inline constexpr const char *libObjTypeName<bt_packet> = "bt_packet";

template <>
inline constexpr const char *libObjTypeName<bt_field_path> = "bt_field_path";

template <>
inline constexpr const char *libObjTypeName<bt_field_path_item> = "bt_field_path_item";

template <>
inline constexpr const char *libObjTypeName<bt_field_location> = "bt_field_location";

template <>
inline constexpr const char *libObjTypeName<bt_integer_range_set> = "bt_integer_range_set";

template <>
inline constexpr const char *libObjTypeName<bt_integer_range_set_unsigned> =
    "bt_integer_range_set_unsigned";

template <>
inline constexpr const char *libObjTypeName<bt_integer_range_set_signed> =
    "bt_integer_range_set_signed";

template <>
inline constexpr const char *libObjTypeName<bt_integer_range_unsigned> =
    "bt_integer_range_unsigned";

template <>
inline constexpr const char *libObjTypeName<bt_integer_range_signed> = "bt_integer_range_signed";

template <>
inline constexpr const char *libObjTypeName<bt_message> = "bt_message";

template <>
inline constexpr const char *libObjTypeName<bt_self_message_iterator> = "bt_self_message_iterator";

template <>
inline constexpr const char *libObjTypeName<bt_component_class> = "bt_component_class";

template <>
inline constexpr const char *libObjTypeName<bt_port> = "bt_port";

template <>
inline constexpr const char *libObjTypeName<bt_self_component_port> = "bt_self_component_port";

template <>
inline constexpr const char *libObjTypeName<bt_self_component_port_input> =
    "bt_self_component_port_input";

template <>
inline constexpr const char *libObjTypeName<bt_self_component_port_output> =
    "bt_self_component_port_output";

template <>
inline constexpr const char *libObjTypeName<bt_interrupter> = "bt_interrupter";

template <>
inline constexpr const char *libObjTypeName<bt_query_executor> = "bt_query_executor";

template <>
inline constexpr const char *libObjTypeName<bt_private_query_executor> =
    "bt_private_query_executor";

template <>
inline constexpr const char *libObjTypeName<bt_graph> = "bt_graph";

template <>
inline constexpr const char *libObjTypeName<bt_component_descriptor_set> =
    "bt_component_descriptor_set";

template <>
inline constexpr const char *libObjTypeName<bt_component_source> = "bt_component_source";

template <>
inline constexpr const char *libObjTypeName<bt_component_filter> = "bt_component_filter";

template <>
inline constexpr const char *libObjTypeName<bt_component_sink> = "bt_component_sink";

template <>
inline constexpr const char *libObjTypeName<bt_component_class_source> =
    "bt_component_class_source";

template <>
inline constexpr const char *libObjTypeName<bt_component_class_filter> =
    "bt_component_class_filter";

template <>
inline constexpr const char *libObjTypeName<bt_component_class_sink> = "bt_component_class_sink";

template <>
inline constexpr const char *libObjTypeName<bt_error> = "bt_error";

template <>
inline constexpr const char *libObjTypeName<bt_error_cause> = "bt_error_cause";

template <>
inline constexpr const char *libObjTypeName<bt_message_iterator> = "bt_message_iterator";

template <>
inline constexpr const char *libObjTypeName<bt_self_message_iterator_configuration> =
    "bt_self_message_iterator_configuration";

template <>
inline constexpr const char *libObjTypeName<bt_self_component_sink> = "bt_self_component_sink";

template <>
inline constexpr const char *libObjTypeName<bt_self_component_source> = "bt_self_component_source";

template <>
inline constexpr const char *libObjTypeName<bt_self_component_filter> = "bt_self_component_filter";

template <>
inline constexpr const char *libObjTypeName<bt_component> = "bt_component";

} /* namespace internal */

/*
 * Whether or not a `PtrObj` object may hold a pointer to `LibObjT`
 * (with or without `const`).
 */
template <typename LibObjT>
inline constexpr bool ptrObjCanHold =
    internal::libObjTypeName<std::remove_const_t<LibObjT>> != nullptr;

namespace internal {

/*
 * Unique type tag of the libbabeltrace2 object type `LibObjT`
 * (without `const`).
 */
template <typename LibObjT>
inline const LibObjTypeTag libObjTypeTag {libObjTypeName<LibObjT>};

/*
 * Returns the unique type tag of the libbabeltrace2 object type
 * `LibObjT` (with or without `const`).
 */
template <typename LibObjT>
const LibObjTypeTag& libObjTypeTagOf() noexcept
{
    using BareLibObjT = std::remove_const_t<LibObjT>;

    static_assert(ptrObjCanHold<BareLibObjT>,
                  "A `PtrObj` object can't hold a `LibObjT` pointer: specialize "
                  "`bt2py::internal::libObjTypeName` for it.");

    return libObjTypeTag<BareLibObjT>;
}

} /* namespace internal */

/*
 * Creates the `PtrObj` Python type.
 *
 * ┌─────────────────────────────────────────────────────────────────┐
 * │ IMPORTANT: Call this before any other `PtrObj` function below.  │
 * └─────────────────────────────────────────────────────────────────┘
 *
 * Throws `PyErrSet` on error.
 */
void initPtrObjType();

/*
 * Returns the `PtrObj` Python type (borrowed reference).
 */
PyTypeObject& ptrObjPyType() noexcept;

/*
 * Returns a new `PtrObj` object holding `addr` of which the type
 * is `tag`, or a new reference to `None` if `addr` is `nullptr`.
 *
 * Throws `PyErrSet` on error.
 */
PyObject& createPtrObj(void *addr, const LibObjTypeTag& tag);

/*
 * Returns the address which the `PtrObj` object `pyObj` holds, or
 * `nullptr` if `pyObj` is `None`.
 *
 * `pyObj` must be `None` or a `PtrObj` object of which the type
 * is `tag`.
 */
void *ptrObjAddr(PyObject& pyObj, const LibObjTypeTag& tag) noexcept;

/*
 * Typed version of createPtrObj(void *, const LibObjTypeTag&).
 */
template <typename LibObjT>
PyObject& createPtrObj(LibObjT * const libObjPtr)
{
    return createPtrObj(const_cast<std::remove_const_t<LibObjT> *>(libObjPtr),
                        internal::libObjTypeTagOf<LibObjT>());
}

/*
 * Typed version of ptrObjAddr().
 */
template <typename LibObjT>
LibObjT *libObjPtrFromPtrObj(PyObject& pyObj) noexcept
{
    return static_cast<LibObjT *>(ptrObjAddr(pyObj, internal::libObjTypeTagOf<LibObjT>()));
}

/*
 * Returns the libbabeltrace2 object wrapper (see bt2::wrap()) of the
 * `PtrObj` object `pyObj`.
 *
 * For example, `wrapPtrObj<const bt_value>(pyObj)` returns
 * a `bt2::ConstValue`.
 *
 * `pyObj` must be a `PtrObj` object of which the type is `LibObjT`.
 */
template <typename LibObjT>
auto wrapPtrObj(PyObject& pyObj) noexcept
{
    const auto libObjPtr = libObjPtrFromPtrObj<LibObjT>(pyObj);

    BT_ASSERT_DBG(libObjPtr);
    return bt2::wrap(libObjPtr);
}

/*
 * Like wrapPtrObj(), but returns an empty optional borrowed object if
 * `pyObj` is `None` (see bt2::wrapOptional()).
 *
 * `pyObj` must be `None` or a `PtrObj` object of which the type
 * is `LibObjT`.
 */
template <typename LibObjT>
auto wrapOptionalPtrObj(PyObject& pyObj) noexcept
{
    return bt2::wrapOptional(libObjPtrFromPtrObj<LibObjT>(pyObj));
}

} /* namespace bt2py */

#endif /* BABELTRACE_BINDINGS_PYTHON_BT2_NATIVE_RUNTIME_PTR_OBJ_HPP */
