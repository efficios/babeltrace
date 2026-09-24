/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 EfficiOS Inc.
 */

#include <Python.h>

#include <babeltrace2/babeltrace.h>

#include "../api.hpp"
#include "../runtime/py-module-builder.hpp"

namespace bt2py {

void registerVersionApi(PyModuleBuilder& mb)
{
    mb.addLibFunc<bt_version_get_major>();
    mb.addLibFunc<bt_version_get_minor>();
    mb.addLibFunc<bt_version_get_patch>();
    mb.addLibFunc<bt_version_get_development_stage>();
    mb.addLibFunc<bt_version_get_vcs_revision_description>();
    mb.addLibFunc<bt_version_get_name>();
    mb.addLibFunc<bt_version_get_name_description>();
    mb.addLibFunc<bt_version_get_extra_name>();
    mb.addLibFunc<bt_version_get_extra_description>();
    mb.addLibFunc<bt_version_get_extra_patch_names>();
}

} /* namespace bt2py */
