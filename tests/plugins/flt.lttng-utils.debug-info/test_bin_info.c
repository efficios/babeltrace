/*
 * test_bin_info.c
 *
 * Babeltrace SO info tests
 *
 * Copyright (c) 2015 EfficiOS Inc. and Linux Foundation
 * Copyright (c) 2015 Antoine Busque <abusque@efficios.com>
 * Copyright (c) 2019 Michael Jeanson <mjeanson@efficios.com>
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

#define BT_LOG_OUTPUT_LEVEL BT_LOG_WARNING
#define BT_LOG_TAG "TEST/BIN-INFO"
#include "logging/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <glib.h>

#include "common/macros.h"
#include "common/assert.h"
#include <lttng-utils/debug-info/bin-info.h>

#include "tap/tap.h"

#define NR_TESTS 57

#define SO_NAME "libhello_so"
#define DEBUG_NAME "libhello_so.debug"
#define FUNC_FOO_FILENAME "./libhello.c"
#define FUNC_FOO_PRINTF_NAME_FMT "foo+0x%" PRIx64
#define FUNC_FOO_NAME_LEN 64

#define DWARF_DIR_NAME "dwarf_full"
#define ELF_DIR_NAME "elf_only"
#define BUILDID_DIR_NAME "build_id"
#define DEBUGLINK_DIR_NAME "debug_link"

/* Lower bound of PIC address mapping */
#define SO_LOW_ADDR 0x400000
/* Size of PIC address mapping */
#define SO_MEMSZ 0x800000
/* An address outside the PIC mapping */
#define SO_INV_ADDR 0x200000

#define BUILD_ID_HEX_LEN 20

static uint64_t opt_func_foo_addr;
static uint64_t opt_func_foo_printf_offset;
static uint64_t opt_func_foo_printf_line_no;
static uint64_t opt_func_foo_tp_offset;
static uint64_t opt_func_foo_tp_line_no;
static uint64_t opt_debug_link_crc;
static gchar *opt_build_id;
static gchar *opt_debug_info_dir;

static uint64_t func_foo_printf_addr;
static uint64_t func_foo_tp_addr;
static char func_foo_printf_name[FUNC_FOO_NAME_LEN];
static uint8_t build_id[BUILD_ID_HEX_LEN];

static GOptionEntry entries[] = {
	{"foo-addr", 0, 0, G_OPTION_ARG_INT64, &opt_func_foo_addr,
	 "Offset to printf in foo", "0xX"},
	{"printf-offset", 0, 0, G_OPTION_ARG_INT64, &opt_func_foo_printf_offset,
	 "Offset to printf in foo", "0xX"},
	{"printf-lineno", 0, 0, G_OPTION_ARG_INT64,
	 &opt_func_foo_printf_line_no, "Line number to printf in foo", "N"},
	{"tp-offset", 0, 0, G_OPTION_ARG_INT64, &opt_func_foo_tp_offset,
	 "Offset to tp in foo", "0xX"},
	{"tp-lineno", 0, 0, G_OPTION_ARG_INT64, &opt_func_foo_tp_line_no,
	 "Line number to tp in foo", "N"},
	{"debug-link-crc", 0, 0, G_OPTION_ARG_INT64, &opt_debug_link_crc,
	 "Debug link CRC", "0xX"},
	{"build-id", 0, 0, G_OPTION_ARG_STRING, &opt_build_id, "Build ID",
	 "XXXXXXXXXXXXXXX"},
	{"debug-info-dir", 0, 0, G_OPTION_ARG_STRING, &opt_debug_info_dir,
	 "Debug info directory", NULL},
	{NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL}};

static
int build_id_to_bin(void)
{
	int ret, len, i;

	if (!opt_build_id) {
		goto error;
	}

	len = strnlen(opt_build_id, BUILD_ID_HEX_LEN * 2);
	if (len != (BUILD_ID_HEX_LEN * 2)) {
		goto error;
	}

	for (i = 0; i < (len / 2); i++) {
		ret = sscanf(opt_build_id + 2 * i, "%02hhx", &build_id[i]);
		if (ret != 1) {
			goto error;
		}
	}

	if (i != BUILD_ID_HEX_LEN) {
		goto error;
	}

	return 0;
error:
	return -1;
}

static
void subtest_has_address(struct bin_info *bin, uint64_t addr)
{
	int ret;

	ret = bin_info_has_address(bin, SO_LOW_ADDR - 1);
	ok(ret == 0, "bin_info_has_address - address under SO's range");

	ret = bin_info_has_address(bin, SO_LOW_ADDR);
	ok(ret == 1, "bin_info_has_address - lower bound of SO's range");

	ret = bin_info_has_address(bin, addr);
	ok(ret == 1, "bin_info_has_address - address in SO's range");

	ret = bin_info_has_address(bin, SO_LOW_ADDR + SO_MEMSZ - 1);
	ok(ret == 1, "bin_info_has_address - upper bound of SO's range");

	ret = bin_info_has_address(bin, SO_LOW_ADDR + SO_MEMSZ);
	ok(ret == 0, "bin_info_has_address - address above SO's range");
}

static
void subtest_lookup_function_name(struct bin_info *bin, uint64_t addr,
					 char *func_name)
{
	int ret;
	char *_func_name = NULL;

	ret = bin_info_lookup_function_name(bin, addr, &_func_name);
	ok(ret == 0, "bin_info_lookup_function_name successful at 0x%" PRIx64, addr);
	if (_func_name) {
		ok(strcmp(_func_name, func_name) == 0,
		   "bin_info_lookup_function_name - correct function name (%s == %s)",
		   func_name, _func_name);
		free(_func_name);
		_func_name = NULL;
	} else {
		skip(1,
		     "bin_info_lookup_function_name - function name is NULL");
	}

	/* Test function name lookup - erroneous address */
	ret = bin_info_lookup_function_name(bin, SO_INV_ADDR, &_func_name);
	ok(ret == -1 && !_func_name,
	   "bin_info_lookup_function_name - fail on invalid addr");
	free(_func_name);
}

static
void subtest_lookup_source_location(struct bin_info *bin, uint64_t addr,
					   uint64_t line_no, const char *filename)
{
	int ret;
	struct source_location *src_loc = NULL;

	ret = bin_info_lookup_source_location(bin, addr, &src_loc);
	ok(ret == 0, "bin_info_lookup_source_location successful at 0x%" PRIx64,
	   addr);
	if (src_loc) {
		ok(src_loc->line_no == line_no,
		   "bin_info_lookup_source_location - correct line_no (%" PRIu64 " == %" PRIu64 ")",
		   line_no, src_loc->line_no);
		ok(strcmp(src_loc->filename, filename) == 0,
		   "bin_info_lookup_source_location - correct filename (%s == %s)",
		   filename, src_loc->filename);
		source_location_destroy(src_loc);
		src_loc = NULL;
	} else {
		fail("bin_info_lookup_source_location - src_loc is NULL");
		fail("bin_info_lookup_source_location - src_loc is NULL");
	}

	/* Test source location lookup - erroneous address */
	ret = bin_info_lookup_source_location(bin, SO_INV_ADDR, &src_loc);
	ok(ret == -1 && !src_loc,
	   "bin_info_lookup_source_location - fail on invalid addr");
	if (src_loc) {
		source_location_destroy(src_loc);
	}
}

static
void test_bin_info_build_id(const char *bin_info_dir)
{
	int ret;
	char *data_dir, *bin_path;
	struct bin_info *bin = NULL;
	struct bt_fd_cache fdc;
	uint8_t invalid_build_id[BUILD_ID_HEX_LEN] = {
		0xa3, 0xfd, 0x8b, 0xff, 0x45, 0xe1, 0xa9, 0x32, 0x15, 0xdd,
		0x6d, 0xaa, 0xd5, 0x53, 0x98, 0x7e, 0xaf, 0xd4, 0x0c, 0xbb
	};

	diag("bin-info tests - separate DWARF via build ID");

	data_dir = g_build_filename(bin_info_dir, BUILDID_DIR_NAME, NULL);
	bin_path =
		g_build_filename(bin_info_dir, BUILDID_DIR_NAME, SO_NAME, NULL);

	if (!data_dir || !bin_path) {
		exit(EXIT_FAILURE);
	}

	ret = bt_fd_cache_init(&fdc, BT_LOG_OUTPUT_LEVEL);
	if (ret != 0) {
		diag("Failed to initialize FD cache");
		exit(EXIT_FAILURE);
	}

	bin = bin_info_create(&fdc, bin_path, SO_LOW_ADDR, SO_MEMSZ, true,
			      data_dir, NULL, BT_LOG_OUTPUT_LEVEL, NULL);
	ok(bin, "bin_info_create successful (%s)", bin_path);

	/* Test setting invalid build_id */
	ret = bin_info_set_build_id(bin, invalid_build_id, BUILD_ID_HEX_LEN);
	ok(ret == -1, "bin_info_set_build_id fail on invalid build_id");

	/* Test setting correct build_id */
	ret = bin_info_set_build_id(bin, build_id, BUILD_ID_HEX_LEN);
	ok(ret == 0, "bin_info_set_build_id successful");

	/* Test bin_info_has_address */
	subtest_has_address(bin, func_foo_printf_addr);

	/* Test function name lookup (with DWARF) */
	subtest_lookup_function_name(bin, func_foo_printf_addr,
				     func_foo_printf_name);

	/* Test source location lookup */
	subtest_lookup_source_location(bin, func_foo_printf_addr,
				       opt_func_foo_printf_line_no,
				       FUNC_FOO_FILENAME);

	bin_info_destroy(bin);
	bt_fd_cache_fini(&fdc);
	g_free(data_dir);
	g_free(bin_path);
}

static
void test_bin_info_debug_link(const char *bin_info_dir)
{
	int ret;
	char *data_dir, *bin_path;
	struct bin_info *bin = NULL;
	struct bt_fd_cache fdc;

	diag("bin-info tests - separate DWARF via debug link");

	data_dir = g_build_filename(bin_info_dir, DEBUGLINK_DIR_NAME, NULL);
	bin_path = g_build_filename(bin_info_dir, DEBUGLINK_DIR_NAME, SO_NAME,
				    NULL);

	if (!data_dir || !bin_path) {
		exit(EXIT_FAILURE);
	}

	ret = bt_fd_cache_init(&fdc, BT_LOG_OUTPUT_LEVEL);
	if (ret != 0) {
		diag("Failed to initialize FD cache");
		exit(EXIT_FAILURE);
	}

	bin = bin_info_create(&fdc, bin_path, SO_LOW_ADDR, SO_MEMSZ, true,
		data_dir, NULL, BT_LOG_OUTPUT_LEVEL, NULL);
	ok(bin, "bin_info_create successful (%s)", bin_path);

	/* Test setting debug link */
	ret = bin_info_set_debug_link(bin, DEBUG_NAME, opt_debug_link_crc);
	ok(ret == 0, "bin_info_set_debug_link successful");

	/* Test bin_info_has_address */
	subtest_has_address(bin, func_foo_printf_addr);

	/* Test function name lookup (with DWARF) */
	subtest_lookup_function_name(bin, func_foo_printf_addr,
				     func_foo_printf_name);

	/* Test source location lookup */
	subtest_lookup_source_location(bin, func_foo_printf_addr,
				       opt_func_foo_printf_line_no,
				       FUNC_FOO_FILENAME);

	bin_info_destroy(bin);
	bt_fd_cache_fini(&fdc);
	g_free(data_dir);
	g_free(bin_path);
}

static
void test_bin_info_elf(const char *bin_info_dir)
{
	int ret;
	char *data_dir, *bin_path;
	struct bin_info *bin = NULL;
	struct source_location *src_loc = NULL;
	struct bt_fd_cache fdc;

	diag("bin-info tests - ELF only");

	data_dir = g_build_filename(bin_info_dir, ELF_DIR_NAME, NULL);
	bin_path = g_build_filename(bin_info_dir, ELF_DIR_NAME, SO_NAME, NULL);

	if (!data_dir || !bin_path) {
		exit(EXIT_FAILURE);
	}

	ret = bt_fd_cache_init(&fdc, BT_LOG_OUTPUT_LEVEL);
	if (ret != 0) {
		diag("Failed to initialize FD cache");
		exit(EXIT_FAILURE);
	}

	bin = bin_info_create(&fdc, bin_path, SO_LOW_ADDR, SO_MEMSZ, true,
		data_dir, NULL, BT_LOG_OUTPUT_LEVEL, NULL);
	ok(bin, "bin_info_create successful (%s)", bin_path);

	/* Test bin_info_has_address */
	subtest_has_address(bin, func_foo_printf_addr);

	/* Test function name lookup (with ELF) */
	subtest_lookup_function_name(bin, func_foo_printf_addr,
				     func_foo_printf_name);

	/* Test source location location - should fail on ELF only file  */
	ret = bin_info_lookup_source_location(bin, func_foo_printf_addr,
					      &src_loc);
	ok(ret == -1,
	   "bin_info_lookup_source_location - fail on ELF only file");

	source_location_destroy(src_loc);
	bin_info_destroy(bin);
	bt_fd_cache_fini(&fdc);
	g_free(data_dir);
	g_free(bin_path);
}

static
void test_bin_info_bundled(const char *bin_info_dir)
{
	int ret;
	char *data_dir, *bin_path;
	struct bin_info *bin = NULL;
	struct bt_fd_cache fdc;

	diag("bin-info tests - DWARF bundled in SO file");

	data_dir = g_build_filename(bin_info_dir, DWARF_DIR_NAME, NULL);
	bin_path =
		g_build_filename(bin_info_dir, DWARF_DIR_NAME, SO_NAME, NULL);

	if (!data_dir || !bin_path) {
		exit(EXIT_FAILURE);
	}

	ret = bt_fd_cache_init(&fdc, BT_LOG_OUTPUT_LEVEL);
	if (ret != 0) {
		diag("Failed to initialize FD cache");
		exit(EXIT_FAILURE);
	}

	bin = bin_info_create(&fdc, bin_path, SO_LOW_ADDR, SO_MEMSZ, true,
		data_dir, NULL, BT_LOG_OUTPUT_LEVEL, NULL);
	ok(bin, "bin_info_create successful (%s)", bin_path);

	/* Test bin_info_has_address */
	subtest_has_address(bin, func_foo_printf_addr);

	/* Test function name lookup (with DWARF) */
	subtest_lookup_function_name(bin, func_foo_printf_addr,
				     func_foo_printf_name);

	/* Test source location lookup */
	subtest_lookup_source_location(bin, func_foo_printf_addr,
				       opt_func_foo_printf_line_no,
				       FUNC_FOO_FILENAME);

	/* Test source location lookup - inlined function */
	subtest_lookup_source_location(bin, func_foo_tp_addr,
				       opt_func_foo_tp_line_no,
				       FUNC_FOO_FILENAME);

	bin_info_destroy(bin);
	bt_fd_cache_fini(&fdc);
	g_free(data_dir);
	g_free(bin_path);
}

int main(int argc, char **argv)
{
	int ret;
	GError *error = NULL;
	GOptionContext *context;
	int status;

	context = g_option_context_new("- bin info test");
	g_option_context_add_main_entries(context, entries, NULL);
	if (!g_option_context_parse(context, &argc, &argv, &error)) {
		fprintf(stderr, "option parsing failed: %s\n", error->message);
		status = EXIT_FAILURE;
		goto end;
	}

	g_snprintf(func_foo_printf_name, FUNC_FOO_NAME_LEN,
		   FUNC_FOO_PRINTF_NAME_FMT, opt_func_foo_printf_offset);
	func_foo_printf_addr =
		SO_LOW_ADDR + opt_func_foo_addr + opt_func_foo_printf_offset;
	func_foo_tp_addr =
		SO_LOW_ADDR + opt_func_foo_addr + opt_func_foo_tp_offset;

	if (build_id_to_bin()) {
		fprintf(stderr, "Failed to parse / missing build id\n");
		status = EXIT_FAILURE;
		goto end;
	}

	plan_tests(NR_TESTS);

	ret = bin_info_init(BT_LOG_OUTPUT_LEVEL, NULL);
	ok(ret == 0, "bin_info_init successful");

	test_bin_info_elf(opt_debug_info_dir);
	test_bin_info_bundled(opt_debug_info_dir);
	test_bin_info_build_id(opt_debug_info_dir);
	test_bin_info_debug_link(opt_debug_info_dir);

	status = EXIT_SUCCESS;

end:
	g_option_context_free(context);

	return status;
}
