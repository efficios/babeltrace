/*
 * symbols.c
 *
 * Babeltrace CTF Plug-in Registration Symbols
 *
 * Copyright 2016 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * Author: Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/plugin/plugin-macros.h>
#include "reader/reader-internal.h"
#include "lttng-live/lttng-live-internal.h"

/* Initialize plug-in entry points. */
BT_PLUGIN_NAME("ctf");
BT_PLUGIN_DESCRIPTION("Babeltrace CTF plug-in.");
BT_PLUGIN_AUTHOR("Jérémie Galarneau");
BT_PLUGIN_LICENSE("MIT");

BT_PLUGIN_COMPONENT_CLASSES_BEGIN
BT_PLUGIN_SOURCE_COMPONENT_CLASS_ENTRY(READER_COMPONENT_NAME,
		READER_COMPONENT_DESCRIPTION, ctf_init);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_ENTRY(LTTNG_LIVE_COMPONENT_NAME,
		LTTNG_LIVE_COMPONENT_DESCRIPTION, lttng_live_init);
BT_PLUGIN_COMPONENT_CLASSES_END

