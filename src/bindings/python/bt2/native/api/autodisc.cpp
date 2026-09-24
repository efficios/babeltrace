/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#include <Python.h>

#include <cstdint>
#include <tuple>
#include <vector>

#include <babeltrace2/babeltrace.h>

#include "autodisc/autodisc.hpp"
#include "common/assert.h"
#include "common/common.h"
#include "cpp-common/bt2/plugin-set.hpp"
#include "cpp-common/bt2/value.hpp"
#include "cpp-common/bt2c/logging.hpp"

#include "../api.hpp"
#include "../runtime/exc.hpp"
#include "../runtime/py-conv.hpp"
#include "../runtime/py-module-builder.hpp"
#include "../runtime/utils.hpp"

namespace bt2py {
namespace {

/*
 * Automatic source component discovery structure which finalizes itself
 * on destruction.
 */
class AutoSrcDisc final
{
public:
    explicit AutoSrcDisc(const bt2c::Logger& logger)
    {
        if (auto_source_discovery_init(&_mAutoDisc) != 0) {
            BT_CPPLOGE_APPEND_CAUSE_AND_THROW_SPEC(
                logger, bt2::MemoryError,
                "Failed to initialize an automatic source discovery structure.");
        }
    }

    AutoSrcDisc(const AutoSrcDisc&) = delete;
    AutoSrcDisc& operator=(const AutoSrcDisc&) = delete;

    ~AutoSrcDisc()
    {
        auto_source_discovery_fini(&_mAutoDisc);
    }

    auto_source_discovery& autoDisc() noexcept
    {
        return _mAutoDisc;
    }

private:
    auto_source_discovery _mAutoDisc {};
};

/*
 * Returns a new Python list containing the elements of `val`, each one
 * converted with `elemToPyFunc`.
 *
 * `elemToPyFunc` receives a `bt2::ConstValue` object and returns a new
 * Python object reference, throwing `PyErrSet` on error.
 */
template <typename ElemToPyFuncT>
PyObjUP pyListFromArrayVal(const bt2::ConstArrayValue val, ElemToPyFuncT&& elemToPyFunc)
{
    return createPyList(val.length(), [val, &elemToPyFunc](const auto index) -> PyObject& {
        return elemToPyFunc(val[index]);
    });
}

using AutoSrcDiscResTuple = std::tuple<const char *, const char *, PyObjUP, PyObjUP>;

/*
 * Returns a tuple, which `PyConv` converts to a Python tuple,
 * describing the automatic source discovery result `res`:
 *
 * 0:
 *     Plugin name (`str`)
 *
 * 1:
 *     Source component class name (`str`)
 *
 * 2:
 *     Inputs (`list` of `str`)
 *
 * 3:
 *     Original input indices (`list` of `int`)
 */
AutoSrcDiscResTuple autoSrcDiscResTuple(const auto_source_discovery_result& res)
{
    return {res.plugin_name, res.source_cc_name,
            pyListFromArrayVal(bt2::ConstArrayValue {res.inputs},
                               [](const auto elem) -> PyObject& {
                                   return PyConv<bt2c::CStringView>::toPy(elem.asString().value());
                               }),
            pyListFromArrayVal(bt2::ConstArrayValue {res.original_input_indices},
                               [](const auto elem) -> PyObject& {
                                   return PyConv<std::uint64_t>::toPy(
                                       elem.asUnsignedInteger().value());
                               })};
}

/*
 * Automatically discovers the source components to instantiate to
 * handle `inputs`, querying the source component classes of the plugins
 * of `pluginSet`.
 *
 * `inputs` and `logLevels` are the parameters of the same names
 * of auto_discover_source_components().
 *
 * Returns a new Python list of which each item is a tuple describing
 * one automatic source discovery result
 * (see autoSrcDiscResTuple()).
 */
PyObjUP autoDiscSrcComps(const bt2::ConstArrayValue inputs, const bt2::ConstArrayValue logLevels,
                         const bt2::ConstPluginSet pluginSet)
{
    BT_ASSERT_DBG(inputs.length() == logLevels.length());

    const auto logger = createLogger("Automatic source discovery");
    AutoSrcDisc autoSrcDisc {logger};

    {
        /* Array of plugin pointers from the plugin set */
        std::vector<const bt_plugin *> plugins;

        plugins.reserve(pluginSet.length());

        for (const auto plugin : pluginSet) {
            plugins.push_back(plugin.libObjPtr());
        }

        switch (auto_discover_source_components(
            inputs.libObjPtr(), logLevels.libObjPtr(), plugins.data(), plugins.size(), nullptr,
            static_cast<bt_logging_level>(logger.level()), &autoSrcDisc.autoDisc(), nullptr)) {
        case AUTO_SOURCE_DISCOVERY_STATUS_OK:
            break;
        case AUTO_SOURCE_DISCOVERY_STATUS_MEMORY_ERROR:
            BT_CPPLOGE_APPEND_CAUSE_AND_THROW_SPEC(logger, bt2::MemoryError,
                                                   "Failed to auto-discover source components.");
        case AUTO_SOURCE_DISCOVERY_STATUS_ERROR:
            BT_CPPLOGE_APPEND_CAUSE_AND_THROW_SPEC(logger, bt2::Error,
                                                   "Failed to auto-discover source components.");
        default:
            /* No interrupter: can't be interrupted */
            bt_common_abort();
        }
    }

    const auto results = autoSrcDisc.autoDisc().results;

    return createPyList(results->len, [results](const auto index) -> PyObject& {
        return PyConv<AutoSrcDiscResTuple>::toPy(autoSrcDiscResTuple(
            *static_cast<const auto_source_discovery_result *>(g_ptr_array_index(results, index))));
    });
}

} /* namespace */

void registerAutodiscApi(PyModuleBuilder& mb)
{
    mb.addFunc<autoDiscSrcComps>("auto_discover_source_components");
}

} /* namespace bt2py */
