/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2016 Philippe Proulx <pproulx@efficios.com>
 */

/* Output argument typemap for initialized event class log level output
 * parameter (always appends).
 */
%typemap(in, numinputs=0)
	(bt_event_class_log_level *)
	(bt_event_class_log_level temp = -1) {
	$1 = &temp;
}

%typemap(argout) bt_event_class_log_level * {
	/* SWIG_Python_AppendOutput() steals the created object */
	$result = SWIG_Python_AppendOutput($result, SWIG_From_int(*$1));
}

%include <babeltrace2/trace-ir/event-class.h>
