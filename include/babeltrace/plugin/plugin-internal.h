#ifndef BABELTRACE_PLUGIN_INTERNAL_H
#define BABELTRACE_PLUGIN_INTERNAL_H

/*
 * BabelTrace - Plug-in internal
 *
 * Copyright 2015 Jérémie Galarneau <jeremie.galarneau@efficios.com>
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

#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/ctf-writer/ref-internal.h>
#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_notification;

struct bt_plugin {
	struct bt_ctf_ref ref_count;
	GString *name;
	enum bt_plugin_type type;

	/* Plug-in implementation callbacks */
	bt_plugin_destroy_cb destroy;
	bt_plugin_set_error_stream_cb set_error_stream;
};

#ifdef __cplusplus
}
#endif

#endif /* BABELTRACE_PLUGIN_INTERNAL_H */
