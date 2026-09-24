/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#include <Python.h>

#include <cstdint>

#include <babeltrace2/babeltrace.h>

#include "cpp-common/bt2/field.hpp"

#include "../api.hpp"
#include "../runtime/exc.hpp"
#include "../runtime/py-module-builder.hpp"
#include "../runtime/utils.hpp"

namespace bt2py {
namespace {

/*
 * Returns a new Python list of strings for the the labels `labels`.
 */
PyObjUP
pyStrListFromLabels(const bt2::Labels<bt_field_class_enumeration_mapping_label_array> labels)
{
    return createPyList(labels.length(), [labels](const auto index) -> PyObject& {
        return throwIfPyErr(PyUnicode_FromString(labels[index].data()));
    });
}

PyObjUP activeBitArrayFieldFlagLabels(const bt2::ConstBitArrayField field)
{
    return pyStrListFromLabels(field.activeFlagLabels());
}

PyObjUP uEnumFieldMappingLabels(const bt2::ConstUnsignedEnumerationField field)
{
    return pyStrListFromLabels(field.labels());
}

PyObjUP sEnumFieldMappingLabels(const bt2::ConstSignedEnumerationField field)
{
    return pyStrListFromLabels(field.mappingLabels());
}

/*
 * Returns the value of `field` as a new Python string.
 *
 * Decodes with the `surrogateescape` error handler because a string
 * field holds trace data, which isn't necessarily valid UTF-8.
 */
PyObjUP getStrFieldVal(const bt2::ConstStringField field)
{
    return ownPyObjOrThrow(PyUnicode_DecodeUTF8(
        field.value().data(), static_cast<Py_ssize_t>(field.length()), "surrogateescape"));
}

/*
 * Returns a new Python memory view of the data of `field` with the
 * access `access` (`PyBUF_READ` or `PyBUF_WRITE`).
 *
 * The memory view doesn't keep `field` alive: the Python layer must not
 * use it once `field` doesn't exist anymore.
 */
PyObjUP blobFieldDataMemView(const bt2::ConstBlobField field, const int access)
{
    /* PyMemoryView_FromMemory() only reads when `access` is `PyBUF_READ` */
    return ownPyObjOrThrow(PyMemoryView_FromMemory(
        reinterpret_cast<char *>(const_cast<std::uint8_t *>(field.data().data())),
        static_cast<Py_ssize_t>(field.length()), access));
}

PyObjUP blobFieldData(const bt2::BlobField field)
{
    return blobFieldDataMemView(field.asConst(), PyBUF_WRITE);
}

PyObjUP blobFieldDataConst(const bt2::ConstBlobField field)
{
    return blobFieldDataMemView(field, PyBUF_READ);
}

void registerCommon(PyModuleBuilder& mb)
{
    mb.addLibFunc<bt_field_get_class_type>();
    mb.addLibFunc<bt_field_borrow_class>();
    mb.addLibFunc<bt_field_borrow_class_const>();
}

void registerScalar(PyModuleBuilder& mb)
{
    mb.addLibFunc<bt_field_bool_set_value>();
    mb.addLibBoolRetFunc<bt_field_bool_get_value>();

    mb.addLibFunc<bt_field_bit_array_set_value_as_integer>();
    mb.addLibFunc<bt_field_bit_array_get_value_as_integer>();
    mb.addFunc<activeBitArrayFieldFlagLabels>("field_bit_array_get_active_flag_labels");

    mb.addLibFunc<bt_field_integer_unsigned_set_value>();
    mb.addLibFunc<bt_field_integer_unsigned_get_value>();
    mb.addLibFunc<bt_field_integer_signed_set_value>();
    mb.addLibFunc<bt_field_integer_signed_get_value>();

    mb.addFunc<uEnumFieldMappingLabels>("field_enumeration_unsigned_get_mapping_labels");
    mb.addFunc<sEnumFieldMappingLabels>("field_enumeration_signed_get_mapping_labels");

    mb.addLibFunc<bt_field_real_single_precision_set_value>();
    mb.addLibFunc<bt_field_real_single_precision_get_value>();
    mb.addLibFunc<bt_field_real_double_precision_set_value>();
    mb.addLibFunc<bt_field_real_double_precision_get_value>();

    mb.addLibFunc<bt_field_string_set_value>();
    mb.addLibFunc<bt_field_string_get_length>();
    mb.addFunc<getStrFieldVal>("field_string_get_value");
    mb.addLibFunc<bt_field_string_append>();
    mb.addLibFunc<bt_field_string_append_with_length>();
    mb.addLibFunc<bt_field_string_clear>();

    mb.addLibFunc<bt_field_blob_get_length>();
    mb.addFunc<blobFieldData>("field_blob_get_data");
    mb.addFunc<blobFieldDataConst>("field_blob_get_data_const");
    mb.addLibFunc<bt_field_blob_dynamic_set_length>();
}

void registerCompound(PyModuleBuilder& mb)
{
    mb.addLibFunc<bt_field_structure_borrow_member_field_by_index>();
    mb.addLibFunc<bt_field_structure_borrow_member_field_by_index_const>();
    mb.addLibFunc<bt_field_structure_borrow_member_field_by_name>();
    mb.addLibFunc<bt_field_structure_borrow_member_field_by_name_const>();

    mb.addLibFunc<bt_field_array_get_length>();
    mb.addLibFunc<bt_field_array_borrow_element_field_by_index>();
    mb.addLibFunc<bt_field_array_borrow_element_field_by_index_const>();
    mb.addLibFunc<bt_field_array_dynamic_set_length>();

    mb.addLibFunc<bt_field_option_set_has_field>();
    mb.addLibFunc<bt_field_option_borrow_field>();
    mb.addLibFunc<bt_field_option_borrow_field_const>();

    mb.addLibFunc<bt_field_variant_get_selected_option_index>();
    mb.addLibFunc<bt_field_variant_select_option_by_index>();
    mb.addLibFunc<bt_field_variant_borrow_selected_option_field>();
    mb.addLibFunc<bt_field_variant_borrow_selected_option_field_const>();
    mb.addLibFunc<bt_field_variant_borrow_selected_option_class_const>();
    mb.addLibFunc<
        bt_field_variant_with_selector_field_integer_unsigned_borrow_selected_option_class_const>();
    mb.addLibFunc<
        bt_field_variant_with_selector_field_integer_signed_borrow_selected_option_class_const>();
}

} /* namespace */

void registerFieldApi(PyModuleBuilder& mb)
{
    registerCommon(mb);
    registerScalar(mb);
    registerCompound(mb);
}

} /* namespace bt2py */
