#ifndef _BABELTRACE_COMPAT_MMAN_H
#define _BABELTRACE_COMPAT_MMAN_H

/*
 * Copyright (C) 2015-2016  Michael Jeanson <mjeanson@efficios.com>
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

#ifdef __MINGW32__

#include <sys/types.h>

#define PROT_NONE	0x0
#define PROT_READ	0x1
#define PROT_WRITE	0x2
#define PROT_EXEC	0x4

#define MAP_FILE	0
#define MAP_SHARED	1
#define MAP_PRIVATE	2
#define MAP_TYPE	0xF
#define MAP_FIXED	0x10
#define MAP_ANONYMOUS	0x20
#define MAP_ANON	MAP_ANONYMOUS
#define MAP_FAILED	((void *) -1)

/*
 * Note that some platforms (e.g. Windows) do not allow read-only
 * mappings to exceed the file's size (even within a page).
 */
void *bt_mmap(void *addr, size_t length, int prot, int flags, int fd,
	off_t offset);

int bt_munmap(void *addr, size_t length);

#else /* __MINGW32__ */

#include <sys/mman.h>

static inline
void *bt_mmap(void *addr, size_t length, int prot, int flags, int fd,
	off_t offset)
{
	return (void *) mmap(addr, length, prot, flags, fd, offset);
}

static inline
int bt_munmap(void *addr, size_t length)
{
	return munmap(addr, length);
}
#endif /* __MINGW32__ */

#ifndef MAP_ANONYMOUS
# ifdef MAP_ANON
#   define MAP_ANONYMOUS MAP_ANON
# endif
#endif

#endif /* _BABELTRACE_COMPAT_MMAN_H */
