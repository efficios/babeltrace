/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2012 Yannick Brosseau <yannick.brosseau@gmail.com>
 *
 * Lib BabelTrace - Common function to all tests
 */

#include "common.h"

#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <ftw.h>

#define RMDIR_NFDOPEN 8

static
int nftw_recursive_rmdir(const char *file,
		const struct stat *sb __attribute__((unused)),
		int flag,
		struct FTW *s __attribute__((unused)))
{
	switch (flag) {
	case FTW_F:
		unlink(file);
		break;
	case FTW_DP:
		rmdir(file);
		break;
	}

	return 0;
}

void recursive_rmdir(const char *path)
{
	int nftw_flags = FTW_PHYS | FTW_DEPTH;

	nftw(path, nftw_recursive_rmdir, RMDIR_NFDOPEN, nftw_flags);
}
