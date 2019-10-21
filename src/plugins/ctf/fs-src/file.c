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

#define BT_COMP_LOG_SELF_COMP (file->self_comp)
#define BT_LOG_OUTPUT_LEVEL (file->log_level)
#define BT_LOG_TAG "PLUGIN/SRC.CTF.FS/FILE"
#include "logging/comp-logging.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <glib.h>
#include "file.h"

BT_HIDDEN
void ctf_fs_file_destroy(struct ctf_fs_file *file)
{
	if (!file) {
		return;
	}

	if (file->fp) {
		BT_COMP_LOGD("Closing file \"%s\" (%p)",
				file->path ? file->path->str : NULL, file->fp);

		if (fclose(file->fp)) {
			BT_COMP_LOGE("Cannot close file \"%s\": %s",
					file->path ? file->path->str : "NULL",
					strerror(errno));
		}
	}

	if (file->path) {
		g_string_free(file->path, TRUE);
	}

	g_free(file);
}

BT_HIDDEN
struct ctf_fs_file *ctf_fs_file_create(bt_logging_level log_level,
		bt_self_component *self_comp)
{
	struct ctf_fs_file *file = g_new0(struct ctf_fs_file, 1);

	if (!file) {
		goto error;
	}

	file->log_level = log_level;
	file->self_comp = self_comp;
	file->path = g_string_new(NULL);
	if (!file->path) {
		goto error;
	}

	goto end;

error:
	ctf_fs_file_destroy(file);
	file = NULL;

end:
	return file;
}

BT_HIDDEN
int ctf_fs_file_open(struct ctf_fs_file *file, const char *mode)
{
	int ret = 0;
	struct stat stat;

	BT_COMP_LOGI("Opening file \"%s\" with mode \"%s\"", file->path->str, mode);
	file->fp = fopen(file->path->str, mode);
	if (!file->fp) {
		BT_COMP_LOGE_APPEND_CAUSE_ERRNO(file->self_comp,
			"Cannot open file", ": path=%s, mode=%s",
			file->path->str, mode);
		goto error;
	}

	BT_COMP_LOGI("Opened file: %p", file->fp);

	if (fstat(fileno(file->fp), &stat)) {
		BT_COMP_LOGE_APPEND_CAUSE_ERRNO(file->self_comp,
			"Cannot get file information",
			": path=%s", file->path->str);
		goto error;
	}

	file->size = stat.st_size;
	BT_COMP_LOGI("File is %jd bytes", (intmax_t) file->size);
	goto end;

error:
	ret = -1;

	if (file->fp) {
		if (fclose(file->fp)) {
			BT_COMP_LOGE("Cannot close file \"%s\": %s", file->path->str,
				strerror(errno));
		}
	}

end:
	return ret;
}
