/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * BabelTrace - CTF on File System Component
 */

#ifndef BABELTRACE_PLUGIN_CTF_FS_QUERY_H
#define BABELTRACE_PLUGIN_CTF_FS_QUERY_H

#include <babeltrace2/babeltrace.h>

#include "common/macros.h"

bt_component_class_query_method_status
metadata_info_query(bt_self_component_class_source *comp_class, const bt_value *params,
                    bt_logging_level log_level, const bt_value **result);

bt_component_class_query_method_status trace_infos_query(bt_self_component_class_source *comp_class,
                                                         const bt_value *params,
                                                         bt_logging_level log_level,
                                                         const bt_value **result);

bt_component_class_query_method_status
support_info_query(bt_self_component_class_source *comp_class, const bt_value *params,
                   bt_logging_level log_level, const bt_value **result);

#endif /* BABELTRACE_PLUGIN_CTF_FS_QUERY_H */
