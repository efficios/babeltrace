/*
 * Common Trace Format
 *
 * Types registry.
 *
 * Copyright 2010 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * Dual LGPL v2.1/GPL v2 license.
 */

#include <ctf/ctf-types.h>
#include <glib.h>

struct type {
	GQuark name;
	size_t len;	/* type length, in bits */
};
