#ifndef _BABELTRACE_MMAP_ALIGN_H
#define _BABELTRACE_MMAP_ALIGN_H

/*
 * BabelTrace mmap-align.h - mmap alignment header
 *
 * Copyright 2010 - Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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
 */

#include <babeltrace/align.h>
#include <stdlib.h>
#include <sys/mman.h>

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
struct mmap_align *mmap_align(size_t length, int prot,
		int flags, int fd, off_t offset)
{
	struct mmap_align *mma;
	off_t page_aligned_offset;	/* mmap offset, aligned to floor */

	mma = malloc(sizeof(*mma));
	if (!mma)
		return MAP_FAILED;
	mma->length = length;
	page_aligned_offset = ALIGN_FLOOR(offset, PAGE_SIZE);
	/*
	 * Page aligned length needs to contain the requested range.
	 * E.g., for a small range that fits within a single page, we might
	 * require a 2 pages page_aligned_length if the range crosses a page
	 * boundary.
	 */
	mma->page_aligned_length = ALIGN(length + offset - page_aligned_offset, PAGE_SIZE);
	mma->page_aligned_addr = mmap(NULL, mma->page_aligned_length,
		prot, flags, fd, page_aligned_offset);
	if (mma->page_aligned_addr == (void *) -1UL) {
		free(mma);
		return MAP_FAILED;
	}
	mma->addr = mma->page_aligned_addr + (offset - page_aligned_offset);
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
	return munmap(page_aligned_addr, page_aligned_length);
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
