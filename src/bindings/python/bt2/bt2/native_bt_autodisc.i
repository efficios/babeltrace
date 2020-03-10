/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
 */

bt_value *bt_bt2_auto_discover_source_components(const bt_value *inputs,
		const bt_plugin_set *plugin_set);

%{
#include "native_bt_autodisc.i.h"
%}
