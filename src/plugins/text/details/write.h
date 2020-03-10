/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
 */

#ifndef BABELTRACE_PLUGINS_TEXT_DETAILS_WRITE_H
#define BABELTRACE_PLUGINS_TEXT_DETAILS_WRITE_H

#include <glib.h>
#include <babeltrace2/babeltrace.h>
#include <stdbool.h>

#include "details.h"

/* Writing context */
struct details_write_ctx {
	/* Weak */
	struct details_comp *details_comp;

	/* Weak (belongs to `details_comp` above) */
	GString *str;

	/* Current indentation level (number of actual spaces) */
	unsigned int indent_level;
};

/*
 * Writes the message `msg` to the component's output buffer
 * (`details_comp->str`).
 */
BT_HIDDEN
int details_write_message(struct details_comp *details_comp,
		const bt_message *msg);

#endif /* BABELTRACE_PLUGINS_TEXT_DETAILS_WRITE_H */
