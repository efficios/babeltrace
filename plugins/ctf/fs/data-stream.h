#ifndef CTF_FS_DATA_STREAM_H
#define CTF_FS_DATA_STREAM_H

/*
 * Copyright 2016 - Philippe Proulx <pproulx@efficios.com>
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

#include <stdio.h>
#include <glib.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/ctf-ir/trace.h>

#include "../common/notif-iter/notif-iter.h"

BT_HIDDEN
int ctf_fs_data_stream_init(struct ctf_fs_component *ctf_fs,
		struct ctf_fs_data_stream *data_stream);

BT_HIDDEN
void ctf_fs_data_stream_fini(struct ctf_fs_data_stream *data_stream);

BT_HIDDEN
int ctf_fs_data_stream_open_streams(struct ctf_fs_component *ctf_fs);

BT_HIDDEN
int ctf_fs_data_stream_get_next_notification(
		struct ctf_fs_component *ctf_fs,
		struct bt_ctf_notif_iter_notif **notification);

#endif /* CTF_FS_DATA_STREAM_H */
