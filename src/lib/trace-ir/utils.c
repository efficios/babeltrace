/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2017-2018 Philippe Proulx <pproulx@efficios.com>
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 */

#define BT_LOG_TAG "LIB/TRACE-IR-UTILS"
#include "lib/logging.h"

#include <stdlib.h>
#include <glib.h>
#include <babeltrace2/trace-ir/clock-class.h>
#include "common/assert.h"

#include "field-class.h"
