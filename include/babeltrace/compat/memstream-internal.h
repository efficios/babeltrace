#ifndef _BABELTRACE_FORMAT_CTF_MEMSTREAM_H
#define _BABELTRACE_FORMAT_CTF_MEMSTREAM_H

/*
 * Copyright 2012 (c) - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * memstream compatibility layer.
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

#ifdef BABELTRACE_HAVE_FMEMOPEN
#include <stdio.h>

static inline
FILE *bt_fmemopen(void *buf, size_t size, const char *mode)
{
	return fmemopen(buf, size, mode);
}

#else /* BABELTRACE_HAVE_FMEMOPEN */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <babeltrace/endian-internal.h>

#ifdef __MINGW32__

#include <io.h>
#include <glib.h>

/*
 * Fallback for systems which don't have fmemopen. Copy buffer to a
 * temporary file, and use that file as FILE * input.
 */
static inline
FILE *bt_fmemopen(void *buf, size_t size, const char *mode)
{
	char *tmpname;
	size_t len;
	FILE *fp;
	int ret;

	/*
	 * Support reading only.
	 */
	if (strcmp(mode, "rb") != 0) {
		return NULL;
	}

	/* Build a temporary filename */
	tmpname = g_build_filename(g_get_tmp_dir(), "babeltrace-tmp-XXXXXX", NULL);
	if (_mktemp(tmpname) == NULL) {
		goto error_free;
	}

	/*
	 * Open as a read/write binary temporary deleted on close file.
	 * Will be deleted when the last file pointer is closed.
	 */
	fp = fopen(tmpname, "w+bTD");
	if (!fp) {
		goto error_free;
	}

	/* Copy the entire buffer to the file */
	len = fwrite(buf, sizeof(char), size, fp);
	if (len != size) {
		goto error_close;
	}

	/* Set the file pointer to the start of file */
	ret = fseek(fp, 0L, SEEK_SET);
	if (ret < 0) {
		perror("fseek");
		goto error_close;
	}

	g_free(tmpname);
	return fp;

error_close:
	ret = fclose(fp);
	if (ret < 0) {
		perror("close");
	}
error_free:
	g_free(tmpname);
	return NULL;
}

#else /* __MINGW32__ */

/*
 * Fallback for systems which don't have fmemopen. Copy buffer to a
 * temporary file, and use that file as FILE * input.
 */
static inline
FILE *bt_fmemopen(void *buf, size_t size, const char *mode)
{
	char *tmpname;
	size_t len;
	FILE *fp;
	int ret;

	/*
	 * Support reading only.
	 */
	if (strcmp(mode, "rb") != 0) {
		return NULL;
	}

	tmpname = g_build_filename(g_get_tmp_dir(), "babeltrace-tmp-XXXXXX", NULL);
	ret = mkstemp(tmpname);
	if (ret < 0) {
		g_free(tmpname);
		return NULL;
	}
	/*
	 * We need to write to the file.
	 */
	fp = fdopen(ret, "wb+");
	if (!fp) {
		goto error_unlink;
	}
	/* Copy the entire buffer to the file */
	len = fwrite(buf, sizeof(char), size, fp);
	if (len != size) {
		goto error_close;
	}
	ret = fseek(fp, 0L, SEEK_SET);
	if (ret < 0) {
		perror("fseek");
		goto error_close;
	}
	/* We keep the handle open, but can unlink the file on the VFS. */
	ret = unlink(tmpname);
	if (ret < 0) {
		perror("unlink");
	}
	g_free(tmpname);
	return fp;

error_close:
	ret = fclose(fp);
	if (ret < 0) {
		perror("close");
	}
error_unlink:
	ret = unlink(tmpname);
	if (ret < 0) {
		perror("unlink");
	}
	g_free(tmpname);
	return NULL;
}

#endif /* __MINGW32__ */

#endif /* BABELTRACE_HAVE_FMEMOPEN */


#ifdef BABELTRACE_HAVE_OPEN_MEMSTREAM

#include <stdio.h>

static inline
FILE *bt_open_memstream(char **ptr, size_t *sizeloc)
{
	return open_memstream(ptr, sizeloc);
}

static inline
int bt_close_memstream(char **buf, size_t *size, FILE *fp)
{
	return fclose(fp);
}

#else /* BABELTRACE_HAVE_OPEN_MEMSTREAM */

#include <stdlib.h>
#include <stdio.h>
#include <glib.h>

#ifdef __MINGW32__

/*
 * Fallback for systems which don't have open_memstream. Create FILE *
 * with bt_open_memstream, but require call to
 * bt_close_memstream to flush all data written to the FILE *
 * into the buffer (which we allocate).
 */
static inline
FILE *bt_open_memstream(char **ptr, size_t *sizeloc)
{
	char *tmpname;
	FILE *fp;

	tmpname = g_build_filename(g_get_tmp_dir(), "babeltrace-tmp-XXXXXX", NULL);

	if (_mktemp(tmpname) == NULL) {
		goto error_free;
	}

	/*
	 * Open as a read/write binary temporary deleted on close file.
	 * Will be deleted when the last file pointer is closed.
	 */
	fp = fopen(tmpname, "w+bTD");
	if (!fp) {
		goto error_free;
	}

	g_free(tmpname);
	return fp;

error_free:
	g_free(tmpname);
	return NULL;
}

#else /* __MINGW32__ */

/*
 * Fallback for systems which don't have open_memstream. Create FILE *
 * with bt_open_memstream, but require call to
 * bt_close_memstream to flush all data written to the FILE *
 * into the buffer (which we allocate).
 */
static inline
FILE *bt_open_memstream(char **ptr, size_t *sizeloc)
{
	char *tmpname;
	int ret;
	FILE *fp;

	tmpname = g_build_filename(g_get_tmp_dir(), "babeltrace-tmp-XXXXXX", NULL);

	ret = mkstemp(tmpname);
	if (ret < 0) {
		perror("mkstemp");
		g_free(tmpname);
		return NULL;
	}
	fp = fdopen(ret, "wb+");
	if (!fp) {
		goto error_unlink;
	}
	/*
	 * babeltrace_flush_memstream will update the buffer content
	 * with read from fp. No need to keep the file around, just the
	 * handle.
	 */
	ret = unlink(tmpname);
	if (ret < 0) {
		perror("unlink");
	}
	g_free(tmpname);
	return fp;

error_unlink:
	ret = unlink(tmpname);
	if (ret < 0) {
		perror("unlink");
	}
	g_free(tmpname);
	return NULL;
}

#endif /* __MINGW32__ */

/* Get file size, allocate buffer, copy. */
static inline
int bt_close_memstream(char **buf, size_t *size, FILE *fp)
{
	size_t len, n;
	long pos;
	int ret;

	ret = fflush(fp);
	if (ret < 0) {
		perror("fflush");
		return ret;
	}
	ret = fseek(fp, 0L, SEEK_END);
	if (ret < 0) {
		perror("fseek");
		return ret;
	}
	pos = ftell(fp);
	if (ret < 0) {
		perror("ftell");
		return ret;
	}
	*size = pos;
	/* add final \0 */
	*buf = calloc(pos + 1, sizeof(char));
	if (!*buf) {
		return -ENOMEM;
	}
	ret = fseek(fp, 0L, SEEK_SET);
	if (ret < 0) {
		perror("fseek");
		goto error_free;
	}
	/* Copy the entire file into the buffer */
	n = 0;
	clearerr(fp);
	while (!feof(fp) && !ferror(fp) && (*size - n > 0)) {
		len = fread(*buf, sizeof(char), *size - n, fp);
		n += len;
	}
	if (n != *size) {
		ret = -1;
		goto error_close;
	}
	ret = fclose(fp);
	if (ret < 0) {
		perror("fclose");
		return ret;
	}
	return 0;

error_close:
	ret = fclose(fp);
	if (ret < 0) {
		perror("fclose");
	}
error_free:
	free(*buf);
	*buf = NULL;
	return ret;
}

#endif /* BABELTRACE_HAVE_OPEN_MEMSTREAM */

#endif /* _BABELTRACE_FORMAT_CTF_MEMSTREAM_H */
