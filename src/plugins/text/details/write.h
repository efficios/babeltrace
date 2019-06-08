#ifndef BABELTRACE_PLUGINS_TEXT_DETAILS_WRITE_H
#define BABELTRACE_PLUGINS_TEXT_DETAILS_WRITE_H

/*
 * Copyright 2019 Philippe Proulx <pproulx@efficios.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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
