/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
 */


/* Output argument typemap for clock value output (always appends) */
%typemap(in, numinputs=0)
	(const bt_clock_snapshot **)
	(bt_clock_snapshot *temp_clock_snapshot = NULL) {
	$1 = &temp_clock_snapshot;
}

%typemap(argout)
	(const bt_clock_snapshot **) {
	if (*$1) {
		/* SWIG_Python_AppendOutput() steals the created object */
		$result = SWIG_Python_AppendOutput($result,
				SWIG_NewPointerObj(SWIG_as_voidptr(*$1),
					SWIGTYPE_p_bt_clock_snapshot, 0));
	} else {
		/* SWIG_Python_AppendOutput() steals Py_None */
		Py_INCREF(Py_None);
		$result = SWIG_Python_AppendOutput($result, Py_None);
	}
}

%include <babeltrace2/graph/message.h>
