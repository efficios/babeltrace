/*
 * common.c
 *
 * Lib BabelTrace - Common function to all tests
 *
 * Copyright 2012 - Yannick Brosseau <yannick.brosseau@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <babeltrace/compat/dirent.h>
#include <babeltrace/compat/limits.h>
#include <sys/stat.h>

void recursive_rmdir(const char *path)
{
	struct dirent *entry;
	DIR *dir = opendir(path);

	if (!dir) {
		perror("# opendir");
		return;
	}

	while ((entry = readdir(dir))) {
		struct stat st;
		char filename[PATH_MAX];

		if (snprintf(filename, sizeof(filename), "%s/%s",
				path, entry->d_name) <= 0) {
			continue;
		}

		if (stat(filename, &st)) {
			continue;
		}

		if (S_ISREG(st.st_mode)) {
			unlinkat(bt_dirfd(dir), entry->d_name, 0);
		}
	}

	rmdir(path);
	closedir(dir);
}
