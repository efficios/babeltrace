#ifndef _BABELTRACE_FORMAT_CTF_MEMSTREAM_H
#define _BABELTRACE_FORMAT_CTF_MEMSTREAM_H

/*
 * format/ctf/memstream.h
 *
 * Copyright 2012 (c) - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * memstream compatibility layer.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to use or copy this program
 * for any purpose,  provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is granted,
 * provided the above notices are retained, and a notice that the code was
 * modified is included with the above copyright notice.
 */

#define _GNU_SOURCE
#include <config.h>

#ifdef BABELTRACE_HAVE_FMEMOPEN
#include <stdio.h>

static inline
FILE *babeltrace_fmemopen(void *buf, size_t size, const char *mode)
{
	return fmemopen(buf, size, mode);
}

#else /* BABELTRACE_HAVE_FMEMOPEN */

#include <stdlib.h>
#include <stdio.h>

/*
 * Fallback for systems which don't have fmemopen. Copy buffer to a
 * temporary file, and use that file as FILE * input.
 */
static inline
FILE *babeltrace_fmemopen(void *buf, size_t size, const char *mode)
{
	char tmpname[PATH_MAX];
	size_t len;
	FILE *fp;
	int ret;

	/*
	 * Support reading only.
	 */
	if (strcmp(mode, "rb") != 0) {
		return NULL;
	}
	strncpy(tmpname, "/tmp/babeltrace-tmp-XXXXXX", PATH_MAX);
	ret = mkstemp(tmpname);
	if (ret < 0) {
		return NULL;
	}
	/*
	 * We need to write to the file.
	 */
	fp = fdopen(ret, "w+");
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
	return NULL;
}

#endif /* BABELTRACE_HAVE_FMEMOPEN */

#ifdef BABELTRACE_HAVE_OPEN_MEMSTREAM

#include <stdio.h>

static inline
FILE *babeltrace_open_memstream(char **ptr, size_t *sizeloc)
{
	return open_memstream(ptr, sizeloc);
}

static inline
int babeltrace_close_memstream(char **buf, size_t *size, FILE *fp)
{
	return fclose(fp);
}

#else /* BABELTRACE_HAVE_OPEN_MEMSTREAM */

#include <stdlib.h>
#include <stdio.h>

/*
 * Fallback for systems which don't have open_memstream. Create FILE *
 * with babeltrace_open_memstream, but require call to
 * babeltrace_close_memstream to flush all data written to the FILE *
 * into the buffer (which we allocate).
 */
static inline
FILE *babeltrace_open_memstream(char **ptr, size_t *sizeloc)
{
	char tmpname[PATH_MAX];
	int ret;
	FILE *fp;

	strncpy(tmpname, "/tmp/babeltrace-tmp-XXXXXX", PATH_MAX);
	ret = mkstemp(tmpname);
	if (ret < 0) {
		return NULL;
	}
	fp = fdopen(ret, "w+");
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
	return fp;

error_unlink:
	ret = unlink(tmpname);
	if (ret < 0) {
		perror("unlink");
	}
	return NULL;
}

/* Get file size, allocate buffer, copy. */
static inline
int babeltrace_close_memstream(char **buf, size_t *size, FILE *fp)
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
