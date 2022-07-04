/*
 * SPDX-License-Identifier: BSD-4-Clause
 *
 * Copyright (c) 1991, 1993 The Regents of the University of California.
 */

#ifndef _BABELTRACE_CRC32_H
#define _BABELTRACE_CRC32_H

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include "common/macros.h"

/**
 * Compute a 32-bit cyclic redundancy checksum for a given file.
 *
 * On success, the out parameter crc is set with the computed checksum
 * value,
 *
 * @param fd	File descriptor for the file for which to compute the CRC
 * @param crc	Out parameter, the computed checksum
 * @returns	0 on success, -1 on failure.
 */
int crc32(int fd, uint32_t *crc);

#endif	/* _BABELTRACE_CRC32_H */
