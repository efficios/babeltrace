#ifndef _BABELTRACE_COMPAT_FCNTL_H
#define _BABELTRACE_COMPAT_FCNTL_H

/*
 * babeltrace/compat/fcntl.h
 *
 * Copyright 2015 (c) - Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * fcntl compatibility layer.
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

#ifdef BABELTRACE_HAVE_POSIX_FALLOCATE

#include <fcntl.h>

static inline
int bt_posix_fallocate(int fd, off_t offset, off_t len)
{
	return posix_fallocate(fd, offset, len);
}

#else /* #ifdef BABELTRACE_HAVE_POSIX_FALLOCATE */

#include <sys/types.h>
#include <unistd.h>
#include <string.h>

#define BABELTRACE_FALLOCATE_BUFLEN	256

#ifndef min_t
#define min_t(type, a, b)	\
	((type) (a) < (type) (b) ? (type) (a) : (type) (b))
#endif

static inline
int bt_posix_fallocate(int fd, off_t offset, off_t len)
{
	int ret = 0;
	ssize_t copy_len;
	char buf[BABELTRACE_FALLOCATE_BUFLEN];
	off_t i, file_pos, orig_end_offset, range_end;

	if (offset < 0 || len < 0) {
		ret = EINVAL;
		goto end;
	}

	range_end = offset + len;
	if (range_end < 0) {
		ret = EFBIG;
		goto end;
	}

	file_pos = lseek(fd, 0, SEEK_CUR);
	if (file_pos < 0) {
		ret = errno;
		goto end;
	}

	orig_end_offset = lseek(fd, 0, SEEK_END);
	if (orig_end_offset < 0) {
		ret = errno;
		goto end;
	}

	/* Seek back to original position. */
	ret = lseek(fd, file_pos, SEEK_SET);
	if (ret) {
		ret = errno;
		goto end;
	}

	/*
	 * The file may not need to grow, but we want to ensure the
	 * space has actually been reserved by the file system. First, copy
	 * the "existing" region of the file, then grow the file if needed.
	 */
	for (i = file_pos; i < min_t(off_t, range_end, orig_end_offset);
			i += copy_len) {
		ssize_t copy_ret;

		copy_len = min_t(size_t, BABELTRACE_FALLOCATE_BUFLEN,
				min_t(off_t, range_end - i,
					orig_end_offset - i));
		copy_ret = pread(fd, &buf, copy_len, i);
		if (copy_ret < copy_len) {
			/*
			 * The caller must handle any EINTR.
			 * POSIX_FALLOCATE(3) does not mention EINTR.
			 * However, glibc does forward to fallocate()
			 * directly on Linux, which may be interrupted.
			 */
			ret = errno;
			goto end;
		}

		copy_ret = pwrite(fd, &buf, copy_len, i);
		if (copy_ret < copy_len) {
			/* Same caveat as noted at pread() */
			ret = errno;
			goto end;
		}
	}

	/* Grow file, as necessary. */
	memset(&buf, 0, BABELTRACE_FALLOCATE_BUFLEN);
	for (i = orig_end_offset; i < range_end; i += copy_len) {
		ssize_t write_ret;

		copy_len = min_t(size_t, BABELTRACE_FALLOCATE_BUFLEN,
				range_end - i);
		write_ret = pwrite(fd, &buf, copy_len, i);
		if (write_ret < copy_len) {
			ret = errno;
			goto end;
		}
	}
end:
	return ret;
}
#endif /* #else #ifdef BABELTRACE_HAVE_POSIX_FALLOCATE */


#ifdef BABELTRACE_HAVE_FACCESSAT

#include <fcntl.h>
#include <unistd.h>

static inline
int bt_faccessat(int dirfd, const char *dirname,
		const char *pathname, int mode, int flags)
{
	return faccessat(dirfd, pathname, mode, flags);
}

#else /* #ifdef BABELTRACE_HAVE_FACCESSAT */

#include <string.h>
#include <unistd.h>

static inline
int bt_faccessat(int dirfd, const char *dirname,
		const char *pathname, int mode, int flags)
{
	char cpath[PATH_MAX];

	if (flags != 0) {
		errno = EINVAL;
		return -1;
	}
	/* Includes middle / and final \0. */
	if (strlen(dirname) + strlen(pathname) + 2 > PATH_MAX) {
		return -1;
	}
	strcpy(cpath, dirname);
	strcat(cpath, "/");
	strcat(cpath, pathname);
	return access(cpath, mode);
}

#endif /* #else #ifdef BABELTRACE_HAVE_FACCESSAT */

#endif /* _BABELTRACE_COMPAT_FCNTL_H */
