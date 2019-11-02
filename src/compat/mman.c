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

#define BT_LOG_OUTPUT_LEVEL (mapping->log_level)
#define BT_LOG_TAG "COMPAT/MMAN"
#include "logging/log.h"

#include "common/macros.h"
#include "common/common.h"

#ifdef __APPLE__
/*
 * On macOS, we need a dummy symbol so that the linker won't
 * complain of an empty table of contents.
 */
BT_HIDDEN
int bt_mman_dummy_symbol;
#endif /* __APPLE__ */

#ifdef __MINGW32__

#include <errno.h>
#include <glib.h>
#include <io.h>
#include <pthread.h>
#include <stdlib.h>
#include <windows.h>

#include "compat/mman.h"

struct mmap_mapping {
	int log_level;

	/* The duplicated handle. */
	HANDLE file_handle;
	/* Handle returned by CreateFileMapping. */
	HANDLE map_handle;
};

static
GHashTable *mmap_mappings = NULL;

/*
 * This mutex protects the hashtable of memory mappings.
 */
static pthread_mutex_t mmap_mutex = PTHREAD_MUTEX_INITIALIZER;

static
struct mmap_mapping *mapping_create(int log_level)
{
	struct mmap_mapping *mapping;

	mapping = malloc(sizeof(struct mmap_mapping));
	if (mapping) {
		mapping->file_handle = NULL;
		mapping->map_handle = NULL;
		mapping->log_level = log_level;
	}

	return mapping;
}

static
void mapping_clean(struct mmap_mapping *mapping)
{
	if (mapping) {
		if (!CloseHandle(mapping->map_handle)) {
			BT_LOGF_STR("Failed to close mmap map_handle.");
			bt_common_abort();
		}
		if (!CloseHandle(mapping->file_handle)) {
			BT_LOGF_STR("Failed to close mmap file_handle.");
			bt_common_abort();
		}
		free(mapping);
		mapping = NULL;
	}
}

static
void addr_clean(void *addr)
{
	/* Cleanup of handles should never fail. */
	if (!UnmapViewOfFile(addr)) {
		/*
		 * FIXME: We don't have access to the mapping's log
		 * level here, so force a FATAL level.
		 */
		BT_LOG_WRITE_CUR_LVL(BT_LOG_FATAL, BT_LOG_FATAL, BT_LOG_TAG,
			"Failed to unmap mmap mapping.");
		bt_common_abort();
	}
}

static
void mmap_lock(int log_level)
{
	if (pthread_mutex_lock(&mmap_mutex)) {
		BT_LOG_WRITE_CUR_LVL(BT_LOG_FATAL, log_level, BT_LOG_TAG, "Failed to acquire mmap_mutex.");
		bt_common_abort();
	}
}

static
void mmap_unlock(int log_level)
{
	if (pthread_mutex_unlock(&mmap_mutex)) {
		BT_LOG_WRITE_CUR_LVL(BT_LOG_FATAL, log_level, BT_LOG_TAG, "Failed to release mmap_mutex.");
		bt_common_abort();
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

	/* Mapping failed. */
	*dwDesiredAccess = 0;
	return 0;
}

BT_HIDDEN
void *bt_mmap(void *addr, size_t length, int prot, int flags, int fd,
		off_t offset, int log_level)
{
	struct mmap_mapping *mapping = NULL;
	void *mapping_addr;
	DWORD dwDesiredAccess;
	DWORD flProtect;
	HANDLE handle;

	/* Check for a valid fd. */
	if (fd == -1) {
		_set_errno(EBADF);
		goto error;
	}

	/* We don't support this at the moment. */
	if (flags == MAP_FIXED) {
		_set_errno(ENOTSUP);
		goto error;
	}

	/* Map mmap flags to those of the Windows API. */
	flProtect = map_prot_flags(prot, &dwDesiredAccess);
	if (flProtect == 0) {
		_set_errno(EINVAL);
		goto error;
	}

	/* Allocate the mapping struct. */
	mapping = mapping_create(log_level);
	if (!mapping) {
		BT_LOG_WRITE_CUR_LVL(BT_LOG_ERROR, log_level, BT_LOG_TAG,
			"Failed to allocate mmap mapping.");
		_set_errno(ENOMEM);
		goto error;
	}

	/* Get a handle from the fd. */
	handle = (HANDLE) _get_osfhandle(fd);

	/* Duplicate the handle and store it in 'mapping.file_handle'. */
	if (!DuplicateHandle(GetCurrentProcess(), handle, GetCurrentProcess(),
			&mapping->file_handle, 0, FALSE,
			DUPLICATE_SAME_ACCESS)) {
		_set_errno(ENOMEM);
		goto error;
	}

	/*
	 * Create a file mapping object with a maximum size
	 * of 'offset' + 'length'.
	 */
	mapping->map_handle = CreateFileMapping(mapping->file_handle, NULL,
			flProtect, 0, offset + length, NULL);
	if (mapping->map_handle == 0) {
		_set_errno(EACCES);
		goto error;
	}

	/* Map the requested block starting at 'offset' for 'length' bytes. */
	mapping_addr = MapViewOfFile(mapping->map_handle, dwDesiredAccess, 0,
			offset, length);
	if (mapping_addr == 0) {
		DWORD dwLastErr = GetLastError();
		if (dwLastErr == ERROR_MAPPED_ALIGNMENT) {
			_set_errno(EINVAL);
		} else {
			_set_errno(EACCES);
		}
		goto error;
	}

	mmap_lock(log_level);

	/* If we have never done any mappings, allocate the hashtable. */
	if (!mmap_mappings) {
		mmap_mappings = g_hash_table_new_full(g_direct_hash,
			g_direct_equal, (GDestroyNotify) addr_clean,
			(GDestroyNotify) mapping_clean);
		if (!mmap_mappings) {
			BT_LOGE_STR("Failed to allocate mmap hashtable.");
			_set_errno(ENOMEM);
			goto error_mutex_unlock;
		}
	}

	/* Add the new mapping to the hashtable. */
	g_hash_table_insert(mmap_mappings, mapping_addr, mapping);

	mmap_unlock(log_level);

	return mapping_addr;

error_mutex_unlock:
	mmap_unlock(log_level);
error:
	mapping_clean(mapping);
	return MAP_FAILED;
}

BT_HIDDEN
int bt_munmap(void *addr, size_t length)
{
	int ret = 0;
	struct mmap_mapping *mapping = addr;
	int log_level;

	BT_ASSERT(mapping);
	log_level = mapping->log_level;
	mmap_lock(log_level);

	/* Check if the mapping exists in the hashtable. */
	if (!g_hash_table_lookup(mmap_mappings, addr)) {
		_set_errno(EINVAL);
		ret = -1;
		goto end;
	}

	/* Remove it. */
	if (!g_hash_table_remove(mmap_mappings, addr)) {
		BT_LOGF_STR("Failed to remove mapping from hashtable.");
		bt_common_abort();
	}

end:
	mmap_unlock(log_level);
	return ret;
}

BT_HIDDEN
size_t bt_mmap_get_offset_align_size(int log_level)
{
	SYSTEM_INFO sysinfo;

	GetNativeSystemInfo(&sysinfo);
	BT_LOG_WRITE_CUR_LVL(BT_LOG_DEBUG, log_level, BT_LOG_TAG,
		"Allocator granularity is %lu.",
		sysinfo.dwAllocationGranularity);

	return sysinfo.dwAllocationGranularity;
}

#endif
