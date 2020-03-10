/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2016 Philippe Proulx <pproulx@efficios.com>
 */

%include <babeltrace2/value.h>

%{
#include "native_bt_value.i.h"
%}

struct bt_value *bt_value_map_get_keys(const struct bt_value *map_obj);
