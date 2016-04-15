/*
 * test_so_info.c
 *
 * Babeltrace SO info tests
 *
 * Copyright (c) 2015 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2015 Antoine Busque <abusque@efficios.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <babeltrace/so-info.h>
#include "tap/tap.h"

#define NR_TESTS 36
#define SO_NAME "libhello.so"
#define SO_NAME_ELF "libhello_elf.so"
#define SO_NAME_BUILD_ID "libhello_build_id.so"
#define SO_NAME_DEBUG_LINK "libhello_debug_link.so"
#define SO_LOW_ADDR 0x400000
#define SO_MEMSZ 0x400000
#define FUNC_FOO_ADDR 0x4014ee
#define FUNC_FOO_LINE_NO 8
#define FUNC_FOO_FILENAME "/efficios/libhello.c"
#define FUNC_FOO_TP_ADDR 0x4014d3
#define FUNC_FOO_TP_LINE_NO 7
#define FUNC_FOO_TP_FILENAME "/efficios/libhello.c"
#define FUNC_FOO_ADDR_ELF 0x4013ef
#define FUNC_FOO_ADDR_DBG_LINK 0x40148e
#define FUNC_FOO_NAME "foo"
#define FUNC_FOO_NAME_ELF "foo+0x24"
#define BUILD_ID_LEN 20

char *opt_debug_info_dir;

static
void test_so_info_build_id(const char *data_dir)
{
	int ret;
	char path[PATH_MAX];
	char *func_name = NULL;
	struct so_info *so = NULL;
	struct source_location *src_loc = NULL;
	uint8_t build_id[BUILD_ID_LEN] = {
		0xcd, 0xd9, 0x8c, 0xdd, 0x87, 0xf7, 0xfe, 0x64, 0xc1, 0x3b,
		0x6d, 0xaa, 0xd5, 0x53, 0x98, 0x7e, 0xaf, 0xd4, 0x0c, 0xbb
	};

	diag("so-info tests - separate DWARF via build ID");

	snprintf(path, PATH_MAX, "%s/%s", data_dir, SO_NAME_BUILD_ID);

	so = so_info_create(path, SO_LOW_ADDR, SO_MEMSZ);
	ok(so != NULL, "so_info_create succesful");

	/* Test setting build_id */
	ret = so_info_set_build_id(so, build_id, BUILD_ID_LEN);
	ok(ret == 0, "so_info_set_build_id succesful");

	/* Test function name lookup (with DWARF) */
	ret = so_info_lookup_function_name(so, FUNC_FOO_ADDR, &func_name);
	ok(ret == 0, "so_info_lookup_function_name succesful");
	ok(strcmp(func_name, FUNC_FOO_NAME) == 0,
		"so_info_lookup_function_name - correct func_name value");
	free(func_name);

	/* Test source location lookup */
	ret = so_info_lookup_source_location(so, FUNC_FOO_ADDR, &src_loc);
	ok(ret == 0, "so_info_lookup_source_location succesful");
	ok(src_loc->line_no == FUNC_FOO_LINE_NO,
		"so_info_lookup_source_location - correct line_no");
	ok(strcmp(src_loc->filename, FUNC_FOO_FILENAME) == 0,
		"so_info_lookup_source_location - correct filename");
	source_location_destroy(src_loc);

	so_info_destroy(so);
}

static
void test_so_info_debug_link(const char *data_dir)
{
	int ret;
	char path[PATH_MAX];
	char *func_name = NULL;
	struct so_info *so = NULL;
	struct source_location *src_loc = NULL;
	char *dbg_filename = "libhello_debug_link.so.debug";
	uint32_t crc = 0xe55c2b98;

	diag("so-info tests - separate DWARF via debug link");

	snprintf(path, PATH_MAX, "%s/%s", data_dir, SO_NAME_DEBUG_LINK);

	so = so_info_create(path, SO_LOW_ADDR, SO_MEMSZ);
	ok(so != NULL, "so_info_create succesful");

	/* Test setting debug link */
	ret = so_info_set_debug_link(so, dbg_filename, crc);
	ok(ret == 0, "so_info_set_debug_link succesful");

	/* Test function name lookup (with DWARF) */
	ret = so_info_lookup_function_name(so, FUNC_FOO_ADDR_DBG_LINK,
					&func_name);
	ok(ret == 0, "so_info_lookup_function_name succesful");
	ok(strcmp(func_name, FUNC_FOO_NAME) == 0,
		"so_info_lookup_function_name - correct func_name value");
	free(func_name);

	/* Test source location lookup */
	ret = so_info_lookup_source_location(so, FUNC_FOO_ADDR_DBG_LINK,
					&src_loc);
	ok(ret == 0, "so_info_lookup_source_location succesful");
	ok(src_loc->line_no == FUNC_FOO_LINE_NO,
		"so_info_lookup_source_location - correct line_no");
	ok(strcmp(src_loc->filename, FUNC_FOO_FILENAME) == 0,
		"so_info_lookup_source_location - correct filename");
	source_location_destroy(src_loc);

	so_info_destroy(so);
}

static
void test_so_info_elf(const char *data_dir)
{
	int ret;
	char path[PATH_MAX];
	char *func_name = NULL;
	struct so_info *so = NULL;
	struct source_location *src_loc = NULL;

	diag("so-info tests - ELF only");

	snprintf(path, PATH_MAX, "%s/%s", data_dir, SO_NAME_ELF);

	so = so_info_create(path, SO_LOW_ADDR, SO_MEMSZ);
	ok(so != NULL, "so_info_create succesful");

	/* Test function name lookup (with ELF) */
	ret = so_info_lookup_function_name(so, FUNC_FOO_ADDR_ELF, &func_name);
	ok(ret == 0, "so_info_lookup_function_name succesful");
	ok(strcmp(func_name, FUNC_FOO_NAME_ELF) == 0,
		"so_info_lookup_function_name - correct func_name value");
	free(func_name);
	func_name = NULL;

	/* Test function name lookup - erroneous address */
	ret = so_info_lookup_function_name(so, 0, &func_name);
	ok(ret == -1 && func_name == NULL,
		"so_info_lookup_function_name - fail on addr not found");

	/* Test source location location - should fail on ELF only file  */
	ret = so_info_lookup_source_location(so, FUNC_FOO_ADDR_ELF, &src_loc);
	ok(ret == -1, "so_info_lookup_source_location - fail on ELF only file");

	source_location_destroy(src_loc);
	so_info_destroy(so);
}

static
void test_so_info(const char *data_dir)
{
	int ret;
	char path[PATH_MAX];
	char *func_name = NULL;
	struct so_info *so = NULL;
	struct source_location *src_loc = NULL;

	diag("so-info tests - DWARF bundled with SO file");

	snprintf(path, PATH_MAX, "%s/%s", data_dir, SO_NAME);

	so = so_info_create(path, SO_LOW_ADDR, SO_MEMSZ);
	ok(so != NULL, "so_info_create succesful");

	/* Test so_info_has_address */
	ret = so_info_has_address(so, 0);
	ok(ret == 0, "so_info_has_address - address under so's range");
	ret = so_info_has_address(so, SO_LOW_ADDR);
	ok(ret == 1, "so_info_has_address - lower bound of so's range");
	ret = so_info_has_address(so, FUNC_FOO_ADDR);
	ok(ret == 1, "so_info_has_address - address in so's range");
	ret = so_info_has_address(so, SO_LOW_ADDR + SO_MEMSZ - 1);
	ok(ret == 1, "so_info_has_address - upper bound of so's range");
	ret = so_info_has_address(so, SO_LOW_ADDR + SO_MEMSZ);
	ok(ret == 0, "so_info_has_address - address above so's range");

	/* Test function name lookup (with DWARF) */
	ret = so_info_lookup_function_name(so, FUNC_FOO_ADDR, &func_name);
	ok(ret == 0, "so_info_lookup_function_name succesful");
	ok(strcmp(func_name, FUNC_FOO_NAME) == 0,
		"so_info_lookup_function_name - correct func_name value");
	free(func_name);
	func_name = NULL;

	/* Test function name lookup - erroneous address */
	ret = so_info_lookup_function_name(so, 0, &func_name);
	ok(ret == -1 && func_name == NULL,
		"so_info_lookup_function_name - fail on addr not found");

	/* Test source location lookup */
	ret = so_info_lookup_source_location(so, FUNC_FOO_ADDR, &src_loc);
	ok(ret == 0, "so_info_lookup_source_location succesful");
	ok(src_loc->line_no == FUNC_FOO_LINE_NO,
		"so_info_lookup_source_location - correct line_no");
	ok(strcmp(src_loc->filename, FUNC_FOO_FILENAME) == 0,
		"so_info_lookup_source_location - correct filename");
	source_location_destroy(src_loc);
	src_loc = NULL;

	/* Test source location lookup - inlined function */
	ret = so_info_lookup_source_location(so, FUNC_FOO_TP_ADDR, &src_loc);
	ok(ret == 0,
		"so_info_lookup_source_location (inlined func) succesful");
	ok(src_loc->line_no == FUNC_FOO_TP_LINE_NO,
		"so_info_lookup_source_location (inlined func) - correct line_no");
	ok(strcmp(src_loc->filename, FUNC_FOO_TP_FILENAME) == 0,
		"so_info_lookup_source_location (inlined func) - correct filename");
	source_location_destroy(src_loc);
	src_loc = NULL;

	/* Test source location lookup - erroneous address */
	ret = so_info_lookup_source_location(so, 0, &src_loc);
	ok(ret == -1 && src_loc == NULL,
		"so_info_lookup_source_location - fail on addr not found");

	so_info_destroy(so);
}

int main(int argc, char **argv)
{
	int ret;

	plan_tests(NR_TESTS);

	if (argc != 2) {
		return EXIT_FAILURE;
	} else {
		opt_debug_info_dir = argv[1];
	}

	ret = so_info_init();
	ok(ret == 0, "so_info_init succesful");

	test_so_info(opt_debug_info_dir);
	test_so_info_elf(opt_debug_info_dir);
	test_so_info_build_id(opt_debug_info_dir);
	test_so_info_debug_link(opt_debug_info_dir);

	return EXIT_SUCCESS;
}
