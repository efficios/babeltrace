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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <glib.h>
#include "file.h"

#define BT_LOG_TAG "PLUGIN-CTF-FS-FILE-SRC"
#include "logging.h"

BT_HIDDEN
void ctf_fs_file_destroy(struct ctf_fs_file *file)
{
	if (!file) {
		return;
	}

	if (file->fp) {
		BT_LOGD("Closing file \"%s\" (%p)",
				file->path ? file->path->str : NULL, file->fp);

		if (fclose(file->fp)) {
			BT_LOGE("Cannot close file \"%s\": %s",
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
struct ctf_fs_file *ctf_fs_file_create(void)
{
	struct ctf_fs_file *file = g_new0(struct ctf_fs_file, 1);

	if (!file) {
		goto error;
	}

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

	BT_LOGD("Opening file \"%s\" with mode \"%s\"", file->path->str, mode);
	file->fp = fopen(file->path->str, mode);
	if (!file->fp) {
		BT_LOGE("Cannot open file \"%s\" with mode \"%s\": %s",
			file->path->str, mode, strerror(errno));
		goto error;
	}

	BT_LOGD("Opened file: %p", file->fp);

	if (fstat(fileno(file->fp), &stat)) {
		BT_LOGE("Cannot get file informations: %s", strerror(errno));
		goto error;
	}

	file->size = stat.st_size;
	BT_LOGD("File is %jd bytes", (intmax_t) file->size);
	goto end;

error:
	ret = -1;

	if (file->fp) {
		if (fclose(file->fp)) {
			BT_LOGE("Cannot close file \"%s\": %s", file->path->str,
				strerror(errno));
		}
	}

end:
	return ret;
}
