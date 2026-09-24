/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#include <Python.h>

#include <cstdint>

#include <babeltrace2/babeltrace.h>

#include "cpp-common/bt2/exc.hpp"
#include "cpp-common/bt2/field-class.hpp"
#include "cpp-common/bt2c/logging.hpp"

#include "../api.hpp"
#include "../runtime/py-module-builder.hpp"
#include "../runtime/utils.hpp"

namespace bt2py {
namespace {

/*
 * Returns a new Python list of the labels of the active flags of `fc`
 * for the value `valAsInt`.
 */
PyObjUP activeBitArrayFcFlagLabelsForValAsInt(const bt2::ConstBitArrayFieldClass fc,
                                              const std::uint64_t valAsInt)
{
    bt_field_class_bit_array_flag_label_array labels;
    std::uint64_t count;

    if (bt_field_class_bit_array_get_active_flag_labels_for_value_as_integer(
            fc.libObjPtr(), valAsInt, &labels, &count) !=
        BT_FIELD_CLASS_BIT_ARRAY_GET_ACTIVE_FLAG_LABELS_FOR_VALUE_AS_INTEGER_STATUS_OK) {
        /* Only possible error status */
        BT_CPPLOGE_APPEND_CAUSE_AND_THROW_SPEC(
            createLogger("Field class"), bt2::MemoryError,
            "Failed to get the active flag labels of a bit array field class: value={}", valAsInt);
    }

    return pyStrListFromCStrs(labels, count);
}

/*
 * Returns a new Python list of the labels of the mappings of `fc` which
 * contain `val`.
 */
PyObjUP uEnumFcMappingLabelsForVal(const bt2::ConstUnsignedEnumerationFieldClass fc,
                                   const std::uint64_t val)
{
    bt_field_class_enumeration_mapping_label_array labels;
    std::uint64_t count;

    if (bt_field_class_enumeration_unsigned_get_mapping_labels_for_value(fc.libObjPtr(), val,
                                                                         &labels, &count) !=
        BT_FIELD_CLASS_ENUMERATION_GET_MAPPING_LABELS_BY_VALUE_STATUS_OK) {
        /* Only possible error status */
        BT_CPPLOGE_APPEND_CAUSE_AND_THROW_SPEC(
            createLogger("Field class"), bt2::MemoryError,
            "Failed to get the mapping labels of an unsigned enumeration field class: value={}",
            val);
    }

    return pyStrListFromCStrs(labels, count);
}

/*
 * Returns a new Python list of the labels of the mappings of `fc` which
 * contain `val`.
 */
PyObjUP sEnumFcMappingLabelsForVal(const bt2::ConstSignedEnumerationFieldClass fc,
                                   const std::int64_t val)
{
    bt_field_class_enumeration_mapping_label_array labels;
    std::uint64_t count;

    if (bt_field_class_enumeration_signed_get_mapping_labels_for_value(fc.libObjPtr(), val, &labels,
                                                                       &count) !=
        BT_FIELD_CLASS_ENUMERATION_GET_MAPPING_LABELS_BY_VALUE_STATUS_OK) {
        /* Only possible error status */
        BT_CPPLOGE_APPEND_CAUSE_AND_THROW_SPEC(
            createLogger("Field class"), bt2::MemoryError,
            "Failed to get the mapping labels of a signed enumeration field class: value={}", val);
    }

    return pyStrListFromCStrs(labels, count);
}

void registerTypeConstants(PyModuleBuilder& mb)
{
    mb.addWiseEnumConstants<bt2::FieldClassType>("FIELD_CLASS_TYPE_");
}

void registerCommon(PyModuleBuilder& mb)
{
    mb.addLibFunc<bt_field_class_get_type>();
    mb.addLibBoolRetFunc<bt_field_class_type_is>();
    mb.addLibFunc<bt_field_class_get_graph_mip_version>();
    mb.addLibFunc<bt_field_class_get_ref>();
    mb.addLibFunc<bt_field_class_put_ref>();

    mb.addLibFunc<bt_field_class_set_user_attributes>();
    mb.addLibFunc<bt_field_class_borrow_user_attributes>();
    mb.addLibFunc<bt_field_class_borrow_user_attributes_const>();
}

void registerBoolAndBitArray(PyModuleBuilder& mb)
{
    mb.addLibFunc<bt_field_class_bool_create>();

    mb.addLibFunc<bt_field_class_bit_array_create>();
    mb.addLibFunc<bt_field_class_bit_array_get_length>();
    mb.addLibFunc<bt_field_class_bit_array_add_flag>();
    mb.addLibFunc<bt_field_class_bit_array_get_flag_count>();
    mb.addLibFunc<bt_field_class_bit_array_borrow_flag_by_index_const>();
    mb.addLibFunc<bt_field_class_bit_array_borrow_flag_by_label_const>();
    mb.addFunc<activeBitArrayFcFlagLabelsForValAsInt>(
        "field_class_bit_array_get_active_flag_labels_for_value_as_integer");
    mb.addLibFunc<bt_field_class_bit_array_flag_get_label>();
    mb.addLibFunc<bt_field_class_bit_array_flag_borrow_index_ranges_const>();
}

void registerIntAndEnum(PyModuleBuilder& mb)
{
    mb.addWiseEnumConstants<bt2::DisplayBase>("FIELD_CLASS_INTEGER_PREFERRED_DISPLAY_BASE_");
    mb.addConstant("FIELD_CLASS_INTEGER_FIELD_VALUE_HINT_SMALL",
                   BT_FIELD_CLASS_INTEGER_FIELD_VALUE_HINT_SMALL);

    mb.addLibFunc<bt_field_class_integer_unsigned_create>();
    mb.addLibFunc<bt_field_class_integer_signed_create>();
    mb.addLibFunc<bt_field_class_integer_set_field_value_range>();
    mb.addLibFunc<bt_field_class_integer_get_field_value_range>();
    mb.addLibFunc<bt_field_class_integer_set_preferred_display_base>();
    mb.addLibFunc<bt_field_class_integer_get_preferred_display_base>();
    mb.addLibFunc<bt_field_class_integer_set_field_value_hints>();
    mb.addLibFunc<bt_field_class_integer_get_field_value_hints>();
    mb.addLibBoolRetFunc<bt_field_class_integer_has_field_value_hint>();

    mb.addLibFunc<bt_field_class_enumeration_get_mapping_count>();
    mb.addLibFunc<bt_field_class_enumeration_mapping_get_label>();

    mb.addLibFunc<bt_field_class_enumeration_unsigned_create>();
    mb.addLibFunc<bt_field_class_enumeration_unsigned_add_mapping>();
    mb.addLibFunc<bt_field_class_enumeration_unsigned_borrow_mapping_by_index_const>();
    mb.addLibFunc<bt_field_class_enumeration_unsigned_borrow_mapping_by_label_const>();
    mb.addFunc<uEnumFcMappingLabelsForVal>(
        "field_class_enumeration_unsigned_get_mapping_labels_for_value");
    mb.addLibFunc<bt_field_class_enumeration_unsigned_mapping_as_mapping_const>();
    mb.addLibFunc<bt_field_class_enumeration_unsigned_mapping_borrow_ranges_const>();

    mb.addLibFunc<bt_field_class_enumeration_signed_create>();
    mb.addLibFunc<bt_field_class_enumeration_signed_add_mapping>();
    mb.addLibFunc<bt_field_class_enumeration_signed_borrow_mapping_by_index_const>();
    mb.addLibFunc<bt_field_class_enumeration_signed_borrow_mapping_by_label_const>();
    mb.addFunc<sEnumFcMappingLabelsForVal>(
        "field_class_enumeration_signed_get_mapping_labels_for_value");
    mb.addLibFunc<bt_field_class_enumeration_signed_mapping_as_mapping_const>();
    mb.addLibFunc<bt_field_class_enumeration_signed_mapping_borrow_ranges_const>();
}

void registerRealStrAndBlob(PyModuleBuilder& mb)
{
    mb.addLibFunc<bt_field_class_real_single_precision_create>();
    mb.addLibFunc<bt_field_class_real_double_precision_create>();

    mb.addLibFunc<bt_field_class_string_create>();

    mb.addLibFunc<bt_field_class_blob_set_media_type>();
    mb.addLibFunc<bt_field_class_blob_get_media_type>();
    mb.addLibFunc<bt_field_class_blob_static_create>();
    mb.addLibFunc<bt_field_class_blob_static_get_length>();
    mb.addLibFunc<bt_field_class_blob_dynamic_without_length_field_location_create>();
    mb.addLibFunc<bt_field_class_blob_dynamic_with_length_field_location_create>();
    mb.addLibFunc<
        bt_field_class_blob_dynamic_with_length_field_borrow_length_field_location_const>();
}

void registerStruct(PyModuleBuilder& mb)
{
    mb.addLibFunc<bt_field_class_structure_create>();
    mb.addLibFunc<bt_field_class_structure_append_member>();
    mb.addLibFunc<bt_field_class_structure_get_member_count>();
    mb.addLibFunc<bt_field_class_structure_borrow_member_by_index>();
    mb.addLibFunc<bt_field_class_structure_borrow_member_by_index_const>();
    mb.addLibFunc<bt_field_class_structure_borrow_member_by_name>();
    mb.addLibFunc<bt_field_class_structure_borrow_member_by_name_const>();

    mb.addLibFunc<bt_field_class_structure_member_get_name>();
    mb.addLibFunc<bt_field_class_structure_member_borrow_field_class>();
    mb.addLibFunc<bt_field_class_structure_member_borrow_field_class_const>();
    mb.addLibFunc<bt_field_class_structure_member_set_user_attributes>();
    mb.addLibFunc<bt_field_class_structure_member_borrow_user_attributes>();
    mb.addLibFunc<bt_field_class_structure_member_borrow_user_attributes_const>();
}

void registerArray(PyModuleBuilder& mb)
{
    mb.addLibFunc<bt_field_class_array_borrow_element_field_class>();
    mb.addLibFunc<bt_field_class_array_borrow_element_field_class_const>();
    mb.addLibFunc<bt_field_class_array_static_create>();
    mb.addLibFunc<bt_field_class_array_static_get_length>();
    mb.addLibFunc<bt_field_class_array_dynamic_create>();
    mb.addLibFunc<bt_field_class_array_dynamic_without_length_field_location_create>();
    mb.addLibFunc<bt_field_class_array_dynamic_with_length_field_location_create>();
    mb.addLibFunc<bt_field_class_array_dynamic_with_length_field_borrow_length_field_path_const>();
    mb.addLibFunc<
        bt_field_class_array_dynamic_with_length_field_borrow_length_field_location_const>();
}

void registerOption(PyModuleBuilder& mb)
{
    mb.addLibFunc<bt_field_class_option_borrow_field_class>();
    mb.addLibFunc<bt_field_class_option_borrow_field_class_const>();

    mb.addLibFunc<bt_field_class_option_without_selector_create>();
    mb.addLibFunc<bt_field_class_option_without_selector_field_location_create>();

    mb.addLibFunc<bt_field_class_option_with_selector_field_borrow_selector_field_path_const>();
    mb.addLibFunc<bt_field_class_option_with_selector_field_borrow_selector_field_location_const>();

    mb.addLibFunc<bt_field_class_option_with_selector_field_bool_create>();
    mb.addLibFunc<bt_field_class_option_with_selector_field_location_bool_create>();
    mb.addLibFunc<bt_field_class_option_with_selector_field_bool_set_selector_is_reversed>();
    mb.addLibBoolRetFunc<bt_field_class_option_with_selector_field_bool_selector_is_reversed>();

    mb.addLibFunc<bt_field_class_option_with_selector_field_integer_unsigned_create>();
    mb.addLibFunc<bt_field_class_option_with_selector_field_location_integer_unsigned_create>();
    mb.addLibFunc<
        bt_field_class_option_with_selector_field_integer_unsigned_borrow_selector_ranges_const>();

    mb.addLibFunc<bt_field_class_option_with_selector_field_integer_signed_create>();
    mb.addLibFunc<bt_field_class_option_with_selector_field_location_integer_signed_create>();
    mb.addLibFunc<
        bt_field_class_option_with_selector_field_integer_signed_borrow_selector_ranges_const>();
}

void registerVariant(PyModuleBuilder& mb)
{
    mb.addLibFunc<bt_field_class_variant_create>();
    mb.addLibFunc<bt_field_class_variant_get_option_count>();
    mb.addLibFunc<bt_field_class_variant_borrow_option_by_index>();
    mb.addLibFunc<bt_field_class_variant_borrow_option_by_index_const>();
    mb.addLibFunc<bt_field_class_variant_borrow_option_by_name>();
    mb.addLibFunc<bt_field_class_variant_borrow_option_by_name_const>();

    mb.addLibFunc<bt_field_class_variant_option_get_name>();
    mb.addLibFunc<bt_field_class_variant_option_borrow_field_class>();
    mb.addLibFunc<bt_field_class_variant_option_borrow_field_class_const>();
    mb.addLibFunc<bt_field_class_variant_option_set_user_attributes>();
    mb.addLibFunc<bt_field_class_variant_option_borrow_user_attributes>();
    mb.addLibFunc<bt_field_class_variant_option_borrow_user_attributes_const>();

    mb.addLibFunc<bt_field_class_variant_without_selector_field_location_create>();
    mb.addLibFunc<bt_field_class_variant_without_selector_append_option>();

    mb.addLibFunc<bt_field_class_variant_with_selector_field_borrow_selector_field_path_const>();
    mb.addLibFunc<
        bt_field_class_variant_with_selector_field_borrow_selector_field_location_const>();

    mb.addLibFunc<bt_field_class_variant_with_selector_field_location_integer_unsigned_create>();
    mb.addLibFunc<bt_field_class_variant_with_selector_field_integer_unsigned_append_option>();
    mb.addLibFunc<
        bt_field_class_variant_with_selector_field_integer_unsigned_borrow_option_by_index_const>();
    mb.addLibFunc<
        bt_field_class_variant_with_selector_field_integer_unsigned_borrow_option_by_name_const>();
    mb.addLibFunc<
        bt_field_class_variant_with_selector_field_integer_unsigned_option_borrow_ranges_const>();
    mb.addLibFunc<
        bt_field_class_variant_with_selector_field_integer_unsigned_option_as_option_const>();

    mb.addLibFunc<bt_field_class_variant_with_selector_field_location_integer_signed_create>();
    mb.addLibFunc<bt_field_class_variant_with_selector_field_integer_signed_append_option>();
    mb.addLibFunc<
        bt_field_class_variant_with_selector_field_integer_signed_borrow_option_by_index_const>();
    mb.addLibFunc<
        bt_field_class_variant_with_selector_field_integer_signed_borrow_option_by_name_const>();
    mb.addLibFunc<
        bt_field_class_variant_with_selector_field_integer_signed_option_borrow_ranges_const>();
    mb.addLibFunc<
        bt_field_class_variant_with_selector_field_integer_signed_option_as_option_const>();
}

} /* namespace */

void registerFieldClassApi(PyModuleBuilder& mb)
{
    registerTypeConstants(mb);
    registerCommon(mb);
    registerBoolAndBitArray(mb);
    registerIntAndEnum(mb);
    registerRealStrAndBlob(mb);
    registerStruct(mb);
    registerArray(mb);
    registerOption(mb);
    registerVariant(mb);
}

} /* namespace bt2py */
