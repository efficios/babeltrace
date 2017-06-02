/*
 * compat/compat_mman.h
 *
 * Copyright (C) 2013   JP Ikaheimonen <jp_ikaheimonen@mentor.com>
 *               2016   Michael Jeanson <mjeanson@efficios.com>
 *
 * These sources are based on ftp://g.oswego.edu/pub/misc/malloc.c
 * file by Doug Lea, released to the public domain.
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

#define BT_LOG_TAG "COMPAT-MMAN"
#include "logging.h"

#ifdef __MINGW32__

#include <assert.h>
#include <errno.h>
#include <io.h>
#include <pthread.h>
#include <stdlib.h>
#include <windows.h>
#include <babeltrace/compat/mman-internal.h>

struct mmap_info {
	HANDLE file_handle; /* the duplicated handle */
	HANDLE map_handle;  /* handle returned by CreateFileMapping */
	void *start;        /* ptr returned by MapViewOfFile */
};

/*
 * This mutex protects the array of memory mappings and its associated
 * counters. (mmap_infos, mmap_infos_cur mmap_infos_max)
 */
static pthread_mutex_t mmap_mutex = PTHREAD_MUTEX_INITIALIZER;

static int mmap_infos_cur = 0;
static int mmap_infos_max = 0;
static struct mmap_info *mmap_infos = NULL;

#define NEW_MMAP_STRUCT_CNT 10

static
void mmap_lock(void)
{
	if (pthread_mutex_lock(&mmap_mutex)) {
		abort();
	}
}

static
void mmap_unlock(void)
{
	if (pthread_mutex_unlock(&mmap_mutex)) {
		abort();
	}
}

/*
 * Convert mmap memory protection flags to CreateFileMapping page protection
 * flag and MapViewOfFile desired access flag.
 */
static
DWORD map_prot_flags(int prot, DWORD *dwDesiredAccess)
{
	if (prot & PROT_READ) {
		if (prot & PROT_WRITE) {
			*dwDesiredAccess = FILE_MAP_WRITE;
			if (prot & PROT_EXEC) {
				return PAGE_EXECUTE_READWRITE;
			}
			return PAGE_READWRITE;
		}
		if (prot & PROT_EXEC) {
			*dwDesiredAccess = FILE_MAP_EXECUTE;
			return PAGE_EXECUTE_READ;
		}
		*dwDesiredAccess = FILE_MAP_READ;
		return PAGE_READONLY;
	}
	if (prot & PROT_WRITE) {
		*dwDesiredAccess = FILE_MAP_COPY;
		return PAGE_WRITECOPY;
	}
	if (prot & PROT_EXEC) {
		*dwDesiredAccess = FILE_MAP_EXECUTE;
		return PAGE_EXECUTE_READ;
	}

	/* Mapping failed */
	*dwDesiredAccess = 0;
	return 0;
}

void *mmap(void *start, size_t length, int prot, int flags, int fd,
		off_t offset)
{
	struct mmap_info mapping;
	DWORD dwDesiredAccess;
	DWORD flProtect;
	HANDLE handle;

	/* Check for a valid fd */
	if (fd == -1) {
		_set_errno(EBADF);
		return MAP_FAILED;
	}

	/* we don't support this atm */
	if (flags == MAP_FIXED) {
		_set_errno(ENOTSUP);
		return MAP_FAILED;
	}

	/* Map mmap flags to windows API */
	flProtect = map_prot_flags(prot, &dwDesiredAccess);
	if (flProtect == 0) {
		_set_errno(EINVAL);
		return MAP_FAILED;
	}

	/* Get a handle from the fd */
	handle = (HANDLE) _get_osfhandle(fd);

	/* Duplicate the handle and store it in 'mapping.file_handle' */
	if (!DuplicateHandle(GetCurrentProcess(), handle, GetCurrentProcess(),
			&mapping.file_handle, 0, FALSE,
			DUPLICATE_SAME_ACCESS)) {
		return MAP_FAILED;
	}

	/*
	 * Create a file mapping object with a maximum size
	 * of 'offset' + 'length'
	 */
	mapping.map_handle = CreateFileMapping(mapping.file_handle, NULL,
			flProtect, 0, offset + length, NULL);
	if (mapping.map_handle == 0) {
		if (!CloseHandle(mapping.file_handle)) {
			abort();
		}
		_set_errno(EACCES);
		return MAP_FAILED;
	}

	/* Map the requested block starting at 'offset' for 'length' bytes */
	mapping.start = MapViewOfFile(mapping.map_handle, dwDesiredAccess, 0,
			offset, length);
	if (mapping.start == 0) {
		DWORD dwLastErr = GetLastError();
		if (!CloseHandle(mapping.map_handle)) {
			abort();
		}
		if (!CloseHandle(mapping.file_handle)) {
			abort();
		}

		if (dwLastErr == ERROR_MAPPED_ALIGNMENT) {
			_set_errno(EINVAL);
		} else {
			_set_errno(EACCES);
		}

		return MAP_FAILED;
	}

	mmap_lock();

	/* If we have never done any mappings, allocate the array */
	if (mmap_infos == NULL) {
		mmap_infos_max = NEW_MMAP_STRUCT_CNT;
		mmap_infos = (struct mmap_info *) calloc(mmap_infos_max,
				sizeof(struct mmap_info));
		if (mmap_infos == NULL) {
			mmap_infos_max = 0;
			goto error_mutex_unlock;
		}
	}

	/* if we have reached our current mapping limit, expand it */
	if (mmap_infos_cur == mmap_infos_max) {
		struct mmap_info *realloc_mmap_infos = NULL;

		mmap_infos_max += NEW_MMAP_STRUCT_CNT;
		realloc_mmap_infos = (struct mmap_info *) realloc(mmap_infos,
				mmap_infos_max * sizeof(struct mmap_info));
		if (realloc_mmap_infos == NULL) {
			mmap_infos_max -= NEW_MMAP_STRUCT_CNT;
			goto error_mutex_unlock;
		}
		mmap_infos = realloc_mmap_infos;
	}

	/* Add the new mapping to the array */
	memcpy(&mmap_infos[mmap_infos_cur], &mapping,
			sizeof(struct mmap_info));
	mmap_infos_cur++;

	mmap_unlock();

	return mapping.start;

error_mutex_unlock:
	mmap_unlock();

	if (!CloseHandle(mapping.map_handle)) {
		abort();
	}
	if (!CloseHandle(mapping.file_handle)) {
		abort();
	}

	_set_errno(ENOMEM);
	return MAP_FAILED;
}

int munmap(void *start, size_t length)
{
	int i, j;

	/* Find the mapping to unmap */
	for (i = 0; i < mmap_infos_cur; i++) {
		if (mmap_infos[i].start == start)
			break;
	}

	/* Mapping was not found */
	if (i == mmap_infos_cur) {
		_set_errno(EINVAL);
		return -1;
	}

	/* Cleanup of handles should never fail */
	if (!UnmapViewOfFile(mmap_infos[i].start)) {
		abort();
	}
	if (!CloseHandle(mmap_infos[i].map_handle)) {
		abort();
	}
	if (!CloseHandle(mmap_infos[i].file_handle)) {
		abort();
	}

	mmap_lock();

	/* Clean the mapping list */
	for (j = i + 1; j < mmap_infos_cur; j++) {
		memcpy(&mmap_infos[j - 1], &mmap_infos[j],
				sizeof(struct mmap_info));
	}
	mmap_infos_cur--;

	/* If the mapping list is now empty, free it */
	if (mmap_infos_cur == 0) {
		free(mmap_infos);
		mmap_infos = NULL;
		mmap_infos_max = 0;
	}

	mmap_unlock();

	return 0;
}

#endif
