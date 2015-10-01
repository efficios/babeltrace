#ifndef _BABELTRACE_COMPAT_FCNTL_H
#define _BABELTRACE_COMPAT_FCNTL_H

/*
 * babeltrace/compat/fcntl.h
 *
 * Copyright 2015 (c) - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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

#include <config.h>

#ifdef BABELTRACE_HAVE_POSIX_FALLOCATE

#include <fcntl.h>

static inline
int babeltrace_posix_fallocate_overwrite(int fd, off_t offset, off_t len)
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
int babeltrace_posix_fallocate_overwrite(int fd, off_t offset, off_t len)
{
	off_t orig_off;
	ssize_t wlen = 0;
	char buf[BABELTRACE_FALLOCATE_BUFLEN];
	off_t i;

	memset(buf, 0, sizeof(buf));
	orig_off = lseek(fd, 0, SEEK_CUR);
	if (orig_off < 0) {
		return -1;
	}
	if (lseek(fd, offset, SEEK_SET) != offset) {
		return -1;
	}
	for (i = 0; i < len; i += wlen) {
		wlen = write(fd, buf, min_t(size_t, len - i,
				BABELTRACE_FALLOCATE_BUFLEN));
		if (wlen < 0) {
			return -1;
		}
	}
	if (lseek(fd, orig_off, SEEK_SET)) {
		return -1;
	}
	return 0;
}

#endif /* #else #ifdef BABELTRACE_HAVE_POSIX_FALLOCATE */

#endif /* _BABELTRACE_COMPAT_FCNTL_H */
