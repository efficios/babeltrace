/*
 * plugin.c
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

#include <babeltrace/plugin/plugin-dev.h>
#include "fs/fs.h"
#include "lttng-live/lttng-live-internal.h"

/* Initialize plug-in description. */
BT_PLUGIN(ctf);
BT_PLUGIN_DESCRIPTION("Built-in Babeltrace plug-in providing CTF read support.");
BT_PLUGIN_AUTHOR("Jérémie Galarneau");
BT_PLUGIN_LICENSE("MIT");

/* Declare component classes implemented by this plug-in. */
BT_PLUGIN_SOURCE_COMPONENT_CLASS(fs, ctf_fs_iterator_get, ctf_fs_iterator_next);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_DESCRIPTION(fs, CTF_FS_COMPONENT_DESCRIPTION);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_INIT_METHOD(fs, ctf_fs_init);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_QUERY_INFO_METHOD(fs, ctf_fs_query_info);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_DESTROY_METHOD(fs, ctf_fs_destroy);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_NOTIFICATION_ITERATOR_INIT_METHOD(fs,
	ctf_fs_iterator_init);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_NOTIFICATION_ITERATOR_DESTROY_METHOD(fs,
	ctf_fs_iterator_destroy);

BT_PLUGIN_SOURCE_COMPONENT_CLASS_WITH_ID(auto, lttng_live, "lttng-live",
	lttng_live_iterator_get, lttng_live_iterator_next);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_INIT_METHOD_WITH_ID(auto, lttng_live,
	lttng_live_init);
BT_PLUGIN_SOURCE_COMPONENT_CLASS_DESCRIPTION_WITH_ID(auto, lttng_live,
        LTTNG_LIVE_COMPONENT_DESCRIPTION);
