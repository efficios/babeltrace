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
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <glib.h>

#define PRINT_ERR_STREAM	ctf_fs->error_fp
#define PRINT_PREFIX		"ctf-fs"
#include "print.h"

#include "ctf-fs.h"
#include "ctf-fs-file.h"
#include "ctf-fs-metadata.h"
#include "ctf-fs-data-stream.h"

bool ctf_fs_debug = false;

static void ctf_fs_destroy(struct ctf_fs *ctf_fs)
{
	if (!ctf_fs) {
		return;
	}

	if (ctf_fs->trace_path) {
		g_string_free(ctf_fs->trace_path, TRUE);
	}

	ctf_fs_metadata_deinit(&ctf_fs->metadata);
	ctf_fs_data_stream_deinit(&ctf_fs->data_stream);
	g_free(ctf_fs);
}

static struct ctf_fs *ctf_fs_create(const char *trace_path)
{
	struct ctf_fs *ctf_fs = g_new0(struct ctf_fs, 1);

	if (!ctf_fs) {
		goto error;
	}

	ctf_fs->trace_path = g_string_new(trace_path);
	if (!ctf_fs->trace_path) {
		goto error;
	}

	ctf_fs->error_fp = stderr;
	ctf_fs->page_size = getpagesize();

	if (ctf_fs_metadata_init(&ctf_fs->metadata)) {
		PERR("Cannot initialize metadata structure\n");
		goto error;
	}

	if (ctf_fs_data_stream_init(ctf_fs, &ctf_fs->data_stream)) {
		PERR("Cannot initialize data stream structure\n");
		goto error;
	}

	goto end;

error:
	ctf_fs_destroy(ctf_fs);
	ctf_fs = NULL;

end:
	return ctf_fs;
}

void ctf_fs_init(void)
{
	if (g_strcmp0(getenv("CTF_FS_DEBUG"), "1") == 0) {
		ctf_fs_debug = true;
	}
}

void ctf_fs_test(const char *trace_path)
{
	ctf_fs_init();

	struct ctf_fs *ctf_fs = ctf_fs_create(trace_path);
	struct bt_ctf_notif_iter_notif *notification;

	if (!ctf_fs) {
		return;
	}

	/* Set trace from metadata file */
	ctf_fs_metadata_set_trace(ctf_fs);
	ctf_fs_data_stream_open_streams(ctf_fs);

	while (true) {
		int ret = ctf_fs_data_stream_get_next_notification(ctf_fs, &notification);
		assert(ret == 0);

		if (!notification) {
			goto end;
		}

		switch (notification->type) {
		case BT_CTF_NOTIF_ITER_NOTIF_NEW_PACKET:
		{
			struct bt_ctf_notif_iter_notif_new_packet *notif =
				(struct bt_ctf_notif_iter_notif_new_packet *) notification;
			break;
		}
		case BT_CTF_NOTIF_ITER_NOTIF_EVENT:
		{
			struct bt_ctf_notif_iter_notif_event *notif =
				(struct bt_ctf_notif_iter_notif_event *) notification;
			break;
		}
		case BT_CTF_NOTIF_ITER_NOTIF_END_OF_PACKET:
		{
			struct bt_ctf_notif_iter_notif_end_of_packet *notif =
				(struct bt_ctf_notif_iter_notif_end_of_packet *) notification;
			break;
		}
		default:
			break;
		}

		bt_ctf_notif_iter_notif_destroy(notification);
	}

end:
	ctf_fs_destroy(ctf_fs);
}
