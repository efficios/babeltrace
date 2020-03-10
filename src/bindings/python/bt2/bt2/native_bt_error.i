/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2019 EfficiOS Inc. and Linux Foundation
 */

/*
 * We include current-thread.h here, because for now, it only contains
 * error-related things.
 */
%include <babeltrace2/error-reporting.h>

%{
#include "native_bt_error.i.h"
%}

static
PyObject *bt_bt2_format_bt_error_cause(const bt_error_cause *error_cause);

static
PyObject *bt_bt2_format_bt_error(const bt_error *error);
