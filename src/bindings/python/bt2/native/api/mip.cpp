/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#include <Python.h>

#include <cstdint>
#include <optional>

#include <babeltrace2/babeltrace.h>

#include "common/common.h"
#include "cpp-common/bt2/exc.hpp"
#include "cpp-common/bt2c/logging.hpp"

#include "../api.hpp"
#include "../runtime/py-conv.hpp"
#include "../runtime/py-module-builder.hpp"
#include "../runtime/utils.hpp"

namespace bt2py {
namespace {

/*
 * Adds a descriptor of a component to create from `compCls` with the
 * parameters `params` and the initialization method data
 * `pyInitMethodObj` (`None` means no method data) to `compDescrSet`.
 *
 * Doesn't get a reference to `pyInitMethodObj`: the Python layer keeps
 * it alive as long as `compDescrSet` exists.
 */
void compDescrSetAddDescr(bt_component_descriptor_set * const compDescrSet,
                          const bt_component_class * const compCls, const bt_value * const params,
                          PyObject * const pyInitMethodObj)
{
    if (bt_component_descriptor_set_add_descriptor_with_initialize_method_data(
            compDescrSet, compCls, params,
            pyInitMethodObj == Py_None ? nullptr : pyInitMethodObj) !=
        BT_COMPONENT_DESCRIPTOR_SET_ADD_DESCRIPTOR_STATUS_OK) {
        /* Only possible error status */
        BT_CPPLOGE_APPEND_CAUSE_AND_THROW_SPEC(createLogger("MIP"), bt2::MemoryError,
                                               "Failed to add a component descriptor to a set.");
    }
}

/*
 * Returns the greatest operative MIP version of the component
 * descriptors of `compDescrSet`, or `std::nullopt` if there's none.
 */
std::optional<std::uint64_t>
getGreatestOperativeMipVersion(const bt_component_descriptor_set * const compDescrSet,
                               const bt_logging_level logLevel)
{
    std::uint64_t mipVersion;

    switch (bt_get_greatest_operative_mip_version(compDescrSet, logLevel, &mipVersion)) {
    case BT_GET_GREATEST_OPERATIVE_MIP_VERSION_STATUS_OK:
        return mipVersion;
    case BT_GET_GREATEST_OPERATIVE_MIP_VERSION_STATUS_NO_MATCH:
        return std::nullopt;
    case BT_GET_GREATEST_OPERATIVE_MIP_VERSION_STATUS_MEMORY_ERROR:
        BT_CPPLOGE_APPEND_CAUSE_AND_THROW_SPEC(createLogger("MIP"), bt2::MemoryError,
                                               "Failed to get the greatest operative MIP version.");
    case BT_GET_GREATEST_OPERATIVE_MIP_VERSION_STATUS_ERROR:
        BT_CPPLOGE_APPEND_CAUSE_AND_THROW_SPEC(createLogger("MIP"), bt2::Error,
                                               "Failed to get the greatest operative MIP version.");
    }

    bt_common_abort();
}

} /* namespace */

void registerMipApi(PyModuleBuilder& mb)
{
    mb.addLibFunc<bt_component_descriptor_set_create>();
    mb.addLibFunc<bt_component_descriptor_set_get_ref>();
    mb.addLibFunc<bt_component_descriptor_set_put_ref>();
    mb.addFunc<compDescrSetAddDescr>("component_descriptor_set_add_descriptor");

    mb.addFunc<getGreatestOperativeMipVersion>("get_greatest_operative_mip_version");
    mb.addLibFunc<bt_get_maximal_mip_version>();
}

} /* namespace bt2py */
