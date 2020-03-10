/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright 2010 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 */

#ifndef _BABELTRACE_MMAP_ALIGN_H
#define _BABELTRACE_MMAP_ALIGN_H

#include "common/align.h"
#include <stdlib.h>
#include <stdint.h>
#include "compat/mman.h"
#include "common/common.h"

/*
 * This header implements a wrapper over mmap (mmap_align) that memory
 * maps a file region that is not necessarily multiple of the page size.
 * It returns a structure (instead of a pointer) that contains the mmap
 * pointer (page-aligned) and a pointer to the offset requested within
 * that page. Note: in the current implementation, the "addr" parameter
 * cannot be forced, so we allocate at an address chosen by the OS.
 */

struct mmap_align {
	void *page_aligned_addr;	/* mmap address, aligned to floor */
	size_t page_aligned_length;	/* mmap length, containing range */

	void *addr;			/* virtual mmap address */
	size_t length;			/* virtual mmap length */
};

static inline
off_t get_page_aligned_offset(off_t offset, int log_level)
{
       return ALIGN_FLOOR(offset, bt_mmap_get_offset_align_size(log_level));
}

static inline
struct mmap_align *mmap_align(size_t length, int prot,
		int flags, int fd, off_t offset, int log_level)
{
	struct mmap_align *mma;
	off_t page_aligned_offset;	/* mmap offset, aligned to floor */
	size_t page_size;

	page_size = bt_common_get_page_size(log_level);

	mma = malloc(sizeof(*mma));
	if (!mma)
		return MAP_FAILED;
	mma->length = length;
	page_aligned_offset = get_page_aligned_offset(offset, log_level);
	/*
	 * Page aligned length needs to contain the requested range.
	 * E.g., for a small range that fits within a single page, we might
	 * require a 2 pages page_aligned_length if the range crosses a page
	 * boundary.
	 */
	mma->page_aligned_length = ALIGN(length + offset - page_aligned_offset, page_size);
	mma->page_aligned_addr = bt_mmap(NULL, mma->page_aligned_length,
		prot, flags, fd, page_aligned_offset, log_level);
	if (mma->page_aligned_addr == MAP_FAILED) {
		free(mma);
		return MAP_FAILED;
	}
	mma->addr = ((uint8_t *) mma->page_aligned_addr) + (offset - page_aligned_offset);
	return mma;
}

static inline
int munmap_align(struct mmap_align *mma)
{
	void *page_aligned_addr;
	size_t page_aligned_length;

	page_aligned_addr = mma->page_aligned_addr;
	page_aligned_length = mma->page_aligned_length;
	free(mma);
	return bt_munmap(page_aligned_addr, page_aligned_length);
}

static inline
void *mmap_align_addr(struct mmap_align *mma)
{
	return mma->addr;
}

/*
 * Helper for special-cases, normally unused.
 */
static inline
void mmap_align_set_addr(struct mmap_align *mma, void *addr)
{
	mma->addr = addr;
}

#endif /* _BABELTRACE_MMAP_ALIGN_H */
