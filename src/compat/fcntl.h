#ifndef _BABELTRACE_COMPAT_FCNTL_H
#define _BABELTRACE_COMPAT_FCNTL_H

/*
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

#elif defined(__MINGW32__) /* #ifdef BABELTRACE_HAVE_POSIX_FALLOCATE */

#include <stdlib.h>
#include <windows.h>
#include <fcntl.h>

static inline
int bt_posix_fallocate(int fd, off_t offset, off_t len)
{
	HANDLE handle;
	LARGE_INTEGER pos, file_pos, orig_end_offset, range_end;
	int ret = 0;
	char zero = 0;
	DWORD byteswritten;

	if (offset < 0 || len <= 0) {
		ret = EINVAL;
		goto end;
	}

	range_end.QuadPart = (__int64) offset + (__int64) len;

	/* Get a handle from the fd */
	handle = (HANDLE) _get_osfhandle(fd);
	if (handle == INVALID_HANDLE_VALUE) {
		ret = EBADF;
		goto end;
	}

	/* Get the file original end offset */
	ret = GetFileSizeEx(handle, &orig_end_offset);
	if (ret == 0) {
		ret = EBADF;
		goto end;
	}

	/* Make sure we don't truncate the file */
	if (orig_end_offset.QuadPart >= range_end.QuadPart) {
		ret = 0;
		goto end;
	}

	/* Get the current file pointer position */
	pos.QuadPart = 0;
	ret = SetFilePointerEx(handle, pos, &file_pos, FILE_CURRENT);
	if (ret == 0) {
		ret = EBADF;
		goto end;
	}

	/* Move the file pointer to the new end offset */
	ret = SetFilePointerEx(handle, range_end, NULL, FILE_BEGIN);
	if (ret == 0) {
		ret = EBADF;
		goto end;
	}

	/* Sets the physical file size to the current position */
	ret = SetEndOfFile(handle);
	if (ret == 0) {
		ret = EINVAL;
		goto restore;
	}

	/*
	 * Move the file pointer back 1 byte, and write a single 0 at the
	 * last byte of the new end offset, the operating system will zero
	 * fill the file.
	 */
	pos.QuadPart = -1;
	ret = SetFilePointerEx(handle, pos, NULL, FILE_END);
	if (ret == 0) {
		ret = EBADF;
		goto end;
	}

	ret = WriteFile(handle, &zero, 1, &byteswritten, NULL);
	if (ret == 0 || byteswritten != 1) {
		ret = ENOSPC;
	} else {
		ret = 0;
	}

restore:
	/* Restore the original file pointer position */
	if (!SetFilePointerEx(handle, file_pos, NULL, FILE_BEGIN)) {
		/* We moved the file pointer but failed to restore it. */
		abort();
	}

end:
	return ret;
}

#else

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

#endif /* _BABELTRACE_COMPAT_FCNTL_H */
