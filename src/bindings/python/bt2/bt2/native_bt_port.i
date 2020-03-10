/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2017 Philippe Proulx <pproulx@efficios.com>
 */

/*
 * Typemap for the user data attached to (and owned by) a self component port.
 * The pointer saved as the port's user data is directly the PyObject *.
 *
 * As per the CPython calling convention, we need to return a new reference to
 * the returned object, which will be transferred to the caller.
 */

%typemap(out) void * {
	Py_INCREF($1);
	$result = $1;
}

%include <babeltrace2/graph/port.h>
%include <babeltrace2/graph/self-component-port.h>

/*
 * Clear this typemap, since it is a bit broad and could apply to something we
 * don't want.
 */
%clear void *;
