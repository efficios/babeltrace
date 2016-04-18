/*
 * so-info.c
 *
 * Babeltrace - Executable and Shared Object Debug Info Reader
 *
 * Copyright 2015 Antoine Busque <abusque@efficios.com>
 *
 * Author: Antoine Busque <abusque@efficios.com>
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

#include <fcntl.h>
#include <math.h>
#include <libgen.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dwarf.h>
#include <glib.h>
#include <babeltrace/dwarf.h>
#include <babeltrace/so-info.h>
#include <babeltrace/crc32.h>
#include <babeltrace/babeltrace-internal.h>
#include <babeltrace/utils.h>

/*
 * An address printed in hex is at most 20 bytes (16 for 64-bits +
 * leading 0x + optional leading '+' if addr is an offset + null
 * character).
 */
#define ADDR_STR_LEN 20

BT_HIDDEN
int so_info_init(void)
{
	int ret = 0;

	if (elf_version(EV_CURRENT) == EV_NONE) {
		fprintf(stderr, "ELF library initialization failed: %s\n",
			elf_errmsg(-1));
		ret = -1;
	}

	return ret;
}

BT_HIDDEN
struct so_info *so_info_create(const char *path, uint64_t low_addr,
		uint64_t memsz, bool is_pic)
{
	struct so_info *so = NULL;

	if (!path) {
		goto error;
	}

	so = g_new0(struct so_info, 1);
	if (!so) {
		goto error;
	}

	so->elf_path = strdup(path);
	if (!so->elf_path) {
		goto error;
	}

	so->is_pic = is_pic;
	so->memsz = memsz;
	so->low_addr = low_addr;
	so->high_addr = so->low_addr + so->memsz;

	return so;

error:
	so_info_destroy(so);
	return NULL;
}

BT_HIDDEN
void so_info_destroy(struct so_info *so)
{
	if (!so) {
		return;
	}

	dwarf_end(so->dwarf_info);

	free(so->elf_path);
	free(so->dwarf_path);
	free(so->build_id);
	free(so->dbg_link_filename);

	elf_end(so->elf_file);

	close(so->elf_fd);
	close(so->dwarf_fd);

	g_free(so);
}


BT_HIDDEN
int so_info_set_build_id(struct so_info *so, uint8_t *build_id,
		size_t build_id_len)
{
	if (!so || !build_id) {
		goto error;
	}

	so->build_id = malloc(build_id_len);
	if (!so->build_id) {
		goto error;
	}

	memcpy(so->build_id, build_id, build_id_len);
	so->build_id_len = build_id_len;

	/*
	 * Reset the is_elf_only flag in case it had been set
	 * previously, because we might find separate debug info using
	 * the new build id information.
	 */
	so->is_elf_only = false;

	return 0;

error:

	return -1;
}

BT_HIDDEN
int so_info_set_debug_link(struct so_info *so, char *filename, uint32_t crc)
{
	if (!so || !filename) {
		goto error;
	}

	so->dbg_link_filename = strdup(filename);
	if (!so->dbg_link_filename) {
		goto error;
	}

	so->dbg_link_crc = crc;

	/*
	 * Reset the is_elf_only flag in case it had been set
	 * previously, because we might find separate debug info using
	 * the new build id information.
	 */
	so->is_elf_only = false;

	return 0;

error:

	return -1;
}

/**
 * Tries to read DWARF info from the location given by path, and
 * attach it to the given so_info instance if it exists.
 *
 * @param so	so_info instance for which to set DWARF info
 * @param path	Presumed location of the DWARF info
 * @returns	0 on success, -1 on failure
 */
static
int so_info_set_dwarf_info_from_path(struct so_info *so, char *path)
{
	int fd = -1, ret = 0;
	struct bt_dwarf_cu *cu = NULL;
	Dwarf *dwarf_info = NULL;

	if (!so || !path) {
		goto error;
	}

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		goto error;
	}

	dwarf_info = dwarf_begin(fd, DWARF_C_READ);
	if (!dwarf_info) {
		goto error;
	}

	/*
	 * Check if the dwarf info has any CU. If not, the SO's object
	 * file contains no DWARF info.
	 */
	cu = bt_dwarf_cu_create(dwarf_info);
	if (!cu) {
		goto error;
	}

	ret = bt_dwarf_cu_next(cu);
	if (ret) {
		goto error;
	}

	so->dwarf_fd = fd;
	so->dwarf_path = strdup(path);
	if (!so->dwarf_path) {
		goto error;
	}
	so->dwarf_info = dwarf_info;
	free(cu);

	return 0;

error:
	close(fd);
	dwarf_end(dwarf_info);
	g_free(dwarf_info);
	free(cu);

	return -1;
}

/**
 * Try to set the dwarf_info for a given so_info instance via the
 * build ID method.
 *
 * @param so		so_info instance for which to retrieve the
 *			DWARF info via build ID
 * @returns		0 on success (i.e. dwarf_info set), -1 on failure
 */
static
int so_info_set_dwarf_info_build_id(struct so_info *so)
{
	int i = 0, ret = 0, dbg_dir_trailing_slash = 0;
	char *path = NULL, *build_id_file = NULL;
	const char *dbg_dir = NULL;
	size_t build_id_file_len, path_len;

	if (!so || !so->build_id) {
		goto error;
	}

	dbg_dir = opt_debug_info_dir ? : DEFAULT_DEBUG_DIR;

	dbg_dir_trailing_slash = dbg_dir[strlen(dbg_dir) - 1] == '/';

	/* 2 characters per byte printed in hex, +2 for '/' and '\0' */
	build_id_file_len = (2 * so->build_id_len) + 2;
	build_id_file = malloc(build_id_file_len);
	if (!build_id_file) {
		goto error;
	}

	snprintf(build_id_file, 4, "%02x/", so->build_id[0]);
	for (i = 1; i < so->build_id_len; ++i) {
		int path_idx = 3 + 2 * (i - 1);

		snprintf(&build_id_file[path_idx], 3, "%02x", so->build_id[i]);
	}

	path_len = strlen(dbg_dir) + strlen(BUILD_ID_SUBDIR) +
			strlen(build_id_file) + strlen(BUILD_ID_SUFFIX) + 1;
	if (!dbg_dir_trailing_slash) {
		path_len += 1;
	}

	path = malloc(path_len);
	if (!path) {
		goto error;
	}

	strcpy(path, dbg_dir);
	if (!dbg_dir_trailing_slash) {
		strcat(path, "/");
	}
	strcat(path, BUILD_ID_SUBDIR);
	strcat(path, build_id_file);
	strcat(path, BUILD_ID_SUFFIX);

	ret = so_info_set_dwarf_info_from_path(so, path);
	if (ret) {
		goto error;
	}

	goto end;

error:
	ret = -1;
end:
	free(build_id_file);
	free(path);

	return ret;
}

/**
 * Tests whether the file located at path exists and has the expected
 * checksum.
 *
 * This predicate is used when looking up separate debug info via the
 * GNU debuglink method. The expected crc can be found .gnu_debuglink
 * section in the original ELF file, along with the filename for the
 * file containing the debug info.
 *
 * @param path	Full path at which to look for the debug file
 * @param crc	Expected checksum for the debug file
 * @returns	1 if the file exists and has the correct checksum,
 *		0 otherwise
 */
static
int is_valid_debug_file(char *path, uint32_t crc)
{
	int ret = 0, fd = -1;
	uint32_t _crc = 0;

	if (!path) {
		goto end;
	}

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		goto end;
	}

	ret = crc32(fd, &_crc);
	if (ret) {
		ret = 0;
		goto end;
	}

	ret = (crc == _crc);

end:
	close(fd);
	return ret;
}

/**
 * Try to set the dwarf_info for a given so_info instance via the
 * build ID method.
 *
 * @param so		so_info instance for which to retrieve the
 *			DWARF info via debug link
 * @returns		0 on success (i.e. dwarf_info set), -1 on failure
 */
static
int so_info_set_dwarf_info_debug_link(struct so_info *so)
{
	int ret = 0;
	const char *dbg_dir = NULL;
	char *dir_name = NULL, *so_dir = NULL, *path = NULL;
	size_t max_path_len = 0;

	if (!so || !so->dbg_link_filename) {
		goto error;
	}

	dbg_dir = opt_debug_info_dir ? : DEFAULT_DEBUG_DIR;

	dir_name = dirname(so->elf_path);
	if (!dir_name) {
		goto error;
	}

	/* so_dir is just dir_name with a trailing slash */
	so_dir = malloc(strlen(dir_name) + 2);
	if (!so_dir) {
		goto error;
	}

	strcpy(so_dir, dir_name);
	strcat(so_dir, "/");

	max_path_len = strlen(dbg_dir) + strlen(so_dir) +
			strlen(DEBUG_SUBDIR) + strlen(so->dbg_link_filename)
			+ 1;
	path = malloc(max_path_len);
	if (!path) {
		goto error;
	}

	/* First look in the SO's dir */
	strcpy(path, so_dir);
	strcat(path, so->dbg_link_filename);

	if (is_valid_debug_file(path, so->dbg_link_crc)) {
		goto found;
	}

	/* If not found, look in .debug subdir */
	strcpy(path, so_dir);
	strcat(path, DEBUG_SUBDIR);
	strcat(path, so->dbg_link_filename);

	if (is_valid_debug_file(path, so->dbg_link_crc)) {
		goto found;
	}

	/* Lastly, look under the global debug directory */
	strcpy(path, dbg_dir);
	strcat(path, so_dir);
	strcat(path, so->dbg_link_filename);

	if (is_valid_debug_file(path, so->dbg_link_crc)) {
		goto found;
	}

error:
	ret = -1;
end:
	free(path);
	free(so_dir);

	return ret;

found:
	ret = so_info_set_dwarf_info_from_path(so, path);
	if (ret) {
		goto error;
	}

	goto end;
}

/**
 * Initialize the DWARF info for a given executable.
 *
 * @param so	so_info instance
 * @returns	0 on success, -1 on failure
 */
static
int so_info_set_dwarf_info(struct so_info *so)
{
	int ret = 0;

	if (!so) {
		goto error;
	}

	/* First try to set the DWARF info from the ELF file */
	ret = so_info_set_dwarf_info_from_path(so, so->elf_path);
	if (!ret) {
		goto end;
	}

	/*
	 * If that fails, try to find separate debug info via build ID
	 * and debug link.
	 */
	ret = so_info_set_dwarf_info_build_id(so);
	if (!ret) {
		goto end;
	}

	ret = so_info_set_dwarf_info_debug_link(so);
	if (!ret) {
		goto end;
	}

error:
	ret = -1;
end:
	return ret;
}

/**
 * Initialize the ELF file for a given executable.
 *
 * @param so	so_info instance
 * @returns	0 on success, -1 on failure
 */
static
int so_info_set_elf_file(struct so_info *so)
{
	int elf_fd;
	Elf *elf_file = NULL;

	if (!so) {
		goto error;
	}

	elf_fd = open(so->elf_path, O_RDONLY);
	if (elf_fd < 0) {
		fprintf(stderr, "Failed to open %s\n", so->elf_path);
		goto error;
	}

	elf_file = elf_begin(elf_fd, ELF_C_READ, NULL);
	if (!elf_file) {
		fprintf(stderr, "elf_begin failed: %s\n", elf_errmsg(-1));
		goto error;
	}

	if (elf_kind(elf_file) != ELF_K_ELF) {
		fprintf(stderr, "Error: %s is not an ELF object\n",
				so->elf_path);
		goto error;
	}

	so->elf_fd = elf_fd;
	so->elf_file = elf_file;
	return 0;

error:
	close(elf_fd);
	elf_end(elf_file);
	return -1;
}


BT_HIDDEN
void source_location_destroy(struct source_location *src_loc)
{
	if (!src_loc) {
		return;
	}

	free(src_loc->filename);
	g_free(src_loc);
}

/**
 * Try to find the symbol closest to an address within a given ELF
 * section.
 *
 * Only function symbols are taken into account. The symbol's address
 * must precede `addr`. A symbol with a closer address might exist
 * after `addr` but is irrelevant because it cannot encompass `addr`.
 *
 * On success, if found, the out parameters `sym` and `shdr` are
 * set. On failure or if none are found, they remain unchanged.
 *
 * @param scn		ELF section in which to look for the address
 * @param addr		Virtual memory address for which to find the
 *			nearest function symbol
 * @param sym		Out parameter, the nearest function symbol
 * @param shdr		Out parameter, the section header for scn
 * @returns		0 on success, -1 on failure
 */
static
int so_info_get_nearest_symbol_from_section(Elf_Scn *scn, uint64_t addr,
		GElf_Sym **sym, GElf_Shdr **shdr)
{
	int i;
	size_t symbol_count;
	Elf_Data *data = NULL;
	GElf_Shdr *_shdr = NULL;
	GElf_Sym *nearest_sym = NULL;

	if (!scn || !sym || !shdr) {
		goto error;
	}

	_shdr = g_new0(GElf_Shdr, 1);
	if (!_shdr) {
		goto error;
	}

	_shdr = gelf_getshdr(scn, _shdr);
	if (!_shdr) {
		goto error;
	}

	if (_shdr->sh_type != SHT_SYMTAB) {
		/*
		 * We are only interested in symbol table (symtab)
		 * sections, skip this one.
		 */
		goto end;
	}

	data = elf_getdata(scn, NULL);
	if (!data) {
		goto error;
	}

	symbol_count = _shdr->sh_size / _shdr->sh_entsize;

	for (i = 0; i < symbol_count; ++i) {
		GElf_Sym *cur_sym = NULL;

		cur_sym = g_new0(GElf_Sym, 1);
		if (!cur_sym) {
			goto error;
		}
		cur_sym = gelf_getsym(data, i, cur_sym);
		if (!cur_sym) {
			goto error;
		}
		if (GELF_ST_TYPE(cur_sym->st_info) != STT_FUNC) {
			/* We're only interested in the functions. */
			g_free(cur_sym);
			continue;
		}

		if (cur_sym->st_value <= addr &&
				(!nearest_sym ||
				cur_sym->st_value > nearest_sym->st_value)) {
			g_free(nearest_sym);
			nearest_sym = cur_sym;
		} else {
			g_free(cur_sym);
		}
	}

end:
	if (nearest_sym) {
		*sym = nearest_sym;
		*shdr = _shdr;
	} else {
		g_free(_shdr);
	}

	return 0;

error:
	g_free(nearest_sym);
	g_free(_shdr);
	return -1;
}

/**
 * Get the name of the function containing a given address within an
 * executable using ELF symbols.
 *
 * The function name is in fact the name of the nearest ELF symbol,
 * followed by the offset in bytes between the address and the symbol
 * (in hex), separated by a '+' character.
 *
 * If found, the out parameter `func_name` is set on success. On failure,
 * it remains unchanged.
 *
 * @param so		so_info instance for the executable containing
 *			the address
 * @param addr		Virtual memory address for which to find the
 *			function name
 * @param func_name	Out parameter, the function name
 * @returns		0 on success, -1 on failure
 */
static
int so_info_lookup_elf_function_name(struct so_info *so, uint64_t addr,
		char **func_name)
{
	/*
	 * TODO (possible optimisation): if an ELF has no symtab
	 * section, it has been stripped. Therefore, it would be wise
	 * to store a flag indicating the stripped status after the
	 * first iteration to prevent subsequent ones.
	 */
	int ret = 0;
	Elf_Scn *scn = NULL;
	GElf_Sym *sym = NULL;
	GElf_Shdr *shdr = NULL;
	char *sym_name = NULL;
	char *_func_name = NULL;
	char offset_str[ADDR_STR_LEN];

	/* Set ELF file if it hasn't been accessed yet. */
	if (!so->elf_file) {
		ret = so_info_set_elf_file(so);
		if (ret) {
			/* Failed to set ELF file. */
			goto error;
		}
	}

	scn = elf_nextscn(so->elf_file, scn);
	if (!scn) {
		goto error;
	}

	while (scn && !sym) {
		ret = so_info_get_nearest_symbol_from_section(
				scn, addr, &sym, &shdr);
		if (ret) {
			goto error;
		}

		scn = elf_nextscn(so->elf_file, scn);
	}

	if (sym) {
		sym_name = elf_strptr(so->elf_file, shdr->sh_link,
				sym->st_name);
		if (!sym_name) {
			goto error;
		}

		snprintf(offset_str, ADDR_STR_LEN, "+%#0" PRIx64,
				addr - sym->st_value);
		_func_name = malloc(strlen(sym_name) + ADDR_STR_LEN);
		if (!_func_name) {
			goto error;
		}

		strcpy(_func_name, sym_name);
		strcat(_func_name, offset_str);
		*func_name = _func_name;
	}

	g_free(shdr);
	g_free(sym);
	return 0;

error:
	g_free(shdr);
	g_free(sym);
	free(_func_name);
	return -1;
}

/**
 * Get the name of the function containing a given address within a
 * given compile unit (CU).
 *
 * If found, the out parameter `func_name` is set on success. On
 * failure, it remains unchanged.
 *
 * @param cu		bt_dwarf_cu instance which may contain the address
 * @param addr		Virtual memory address for which to find the
 *			function name
 * @param func_name	Out parameter, the function name
 * @returns		0 on success, -1 on failure
 */
static
int so_info_lookup_cu_function_name(struct bt_dwarf_cu *cu, uint64_t addr,
		char **func_name)
{
	int ret = 0, found = 0;
	char *_func_name = NULL;
	struct bt_dwarf_die *die = NULL;

	if (!cu || !func_name) {
		goto error;
	}

	die = bt_dwarf_die_create(cu);
	if (!die) {
		goto error;
	}

	while (bt_dwarf_die_next(die) == 0) {
		int tag;

		ret = bt_dwarf_die_get_tag(die, &tag);
		if (ret) {
			goto error;
		}

		if (tag == DW_TAG_subprogram) {
			ret = bt_dwarf_die_contains_addr(die, addr, &found);
			if (ret) {
				goto error;
			}

			if (found) {
				break;
			}
		}
	}

	if (found) {
		ret = bt_dwarf_die_get_name(die, &_func_name);
		if (ret) {
			goto error;
		}

		*func_name = _func_name;
	}

	bt_dwarf_die_destroy(die);
	return 0;

error:
	bt_dwarf_die_destroy(die);
	return -1;
}

/**
 * Get the name of the function containing a given address within an
 * executable using DWARF debug info.
 *
 * If found, the out parameter `func_name` is set on success. On
 * failure, it remains unchanged.
 *
 * @param so		so_info instance for the executable containing
 *			the address
 * @param addr		Virtual memory address for which to find the
 *			function name
 * @param func_name	Out parameter, the function name
 * @returns		0 on success, -1 on failure
 */
static
int so_info_lookup_dwarf_function_name(struct so_info *so, uint64_t addr,
		char **func_name)
{
	int ret = 0;
	char *_func_name = NULL;
	struct bt_dwarf_cu *cu = NULL;

	if (!so || !func_name) {
		goto error;
	}

	cu = bt_dwarf_cu_create(so->dwarf_info);
	if (!cu) {
		goto error;
	}

	while (bt_dwarf_cu_next(cu) == 0) {
		ret = so_info_lookup_cu_function_name(cu, addr, &_func_name);
		if (ret) {
			goto error;
		}

		if (_func_name) {
			break;
		}
	}

	if (_func_name) {
		*func_name = _func_name;
	}

	bt_dwarf_cu_destroy(cu);
	return 0;

error:
	bt_dwarf_cu_destroy(cu);
	return -1;
}

BT_HIDDEN
int so_info_lookup_function_name(struct so_info *so, uint64_t addr,
		char **func_name)
{
	int ret = 0;
	char *_func_name = NULL;

	if (!so || !func_name) {
		goto error;
	}

	/* Set DWARF info if it hasn't been accessed yet. */
	if (!so->dwarf_info && !so->is_elf_only) {
		ret = so_info_set_dwarf_info(so);
		if (ret) {
			/* Failed to set DWARF info, fallback to ELF. */
			so->is_elf_only = true;
		}
	}

	if (!so_info_has_address(so, addr)) {
		goto error;
	}

	/*
	 * Addresses in ELF and DWARF are relative to base address for
	 * PIC, so make the address argument relative too if needed.
	 */
	if (so->is_pic) {
		addr -= so->low_addr;
	}

	if (so->is_elf_only) {
		ret = so_info_lookup_elf_function_name(so, addr, &_func_name);
	} else {
		ret = so_info_lookup_dwarf_function_name(so, addr, &_func_name);
	}

	if (ret || !_func_name) {
		goto error;
	}

	*func_name = _func_name;
	return 0;

error:
	return -1;
}

BT_HIDDEN
int so_info_get_bin_loc(struct so_info *so, uint64_t addr, char **bin_loc)
{
	int ret = 0;
	char *_bin_loc = NULL;

	if (!so || !bin_loc) {
		goto error;
	}

	if (so->is_pic) {
		addr -= so->low_addr;
		ret = asprintf(&_bin_loc, "+%#0" PRIx64, addr);
	} else {
		ret = asprintf(&_bin_loc, "@%#0" PRIx64, addr);
	}

	if (ret == -1 || !_bin_loc) {
		goto error;
	}

	*bin_loc = _bin_loc;
	return 0;

error:
	return -1;
}

/**
 * Predicate used to determine whether the children of a given DIE
 * contain a specific address.
 *
 * More specifically, the parameter `die` is expected to be a
 * subprogram (function) DIE, and this predicate tells whether any
 * subroutines are inlined within this function and would contain
 * `addr`.
 *
 * Do note that this function advances the position of `die`. If the
 * address is found within one of its children, `die` will be pointing
 * to that child upon returning from the function, allowing to extract
 * the information deemed necessary.
 *
 * @param die	The parent DIE in whose children the address will be
 *		looked for
 * @param addr	The address for which to look for in the DIEs
 * @returns	Returns 1 if the address was found, 0 if not
 */
static
int so_info_child_die_has_address(struct bt_dwarf_die *die, uint64_t addr)
{
	int ret = 0, contains = 0;

	if (!die) {
		goto error;
	}

	ret = bt_dwarf_die_child(die);
	if (ret) {
		goto error;
	}

	do {
		int tag;

		ret = bt_dwarf_die_get_tag(die, &tag);
		if (ret) {
			goto error;
		}

		if (tag == DW_TAG_inlined_subroutine) {
			ret = bt_dwarf_die_contains_addr(die, addr, &contains);
			if (ret) {
				goto error;
			}

			if (contains) {
				ret = 1;
				goto end;
			}
		}
	} while (bt_dwarf_die_next(die) == 0);

end:
	return ret;

error:
	ret = 0;
	goto end;
}

/**
 * Lookup the source location for a given address within a CU, making
 * the assumption that it is contained within an inline routine in a
 * function.
 *
 * @param cu		bt_dwarf_cu instance in which to look for the address
 * @param addr		The address for which to look for
 * @param src_loc	Out parameter, the source location (filename and
 *			line number) for the address
 * @returns		0 on success, -1 on failure
 */
static
int so_info_lookup_cu_src_loc_inl(struct bt_dwarf_cu *cu, uint64_t addr,
		struct source_location **src_loc)
{
	int ret = 0, found = 0;
	struct bt_dwarf_die *die = NULL;
	struct source_location *_src_loc = NULL;

	if (!cu || !src_loc) {
		goto error;
	}

	die = bt_dwarf_die_create(cu);
	if (!die) {
		goto error;
	}

	while (bt_dwarf_die_next(die) == 0) {
		int tag;

		ret = bt_dwarf_die_get_tag(die, &tag);
		if (ret) {
			goto error;
		}

		if (tag == DW_TAG_subprogram) {
			int contains = 0;

			ret = bt_dwarf_die_contains_addr(die, addr, &contains);
			if (ret) {
				goto error;
			}

			if (contains) {
				/*
				 * Try to find an inlined subroutine
				 * child of this DIE containing addr.
				 */
				found = so_info_child_die_has_address(
						die, addr);
				goto end;
			}
		}
	}

end:
	if (found) {
		char *filename = NULL;
		uint64_t line_no;

		_src_loc = g_new0(struct source_location, 1);
		if (!_src_loc) {
			goto error;
		}

		ret = bt_dwarf_die_get_call_file(die, &filename);
		if (ret) {
			goto error;
		}
		ret = bt_dwarf_die_get_call_line(die, &line_no);
		if (ret) {
			free(filename);
			goto error;
		}

		_src_loc->filename = filename;
		_src_loc->line_no = line_no;
		*src_loc = _src_loc;
	}

	bt_dwarf_die_destroy(die);
	return 0;

error:
	source_location_destroy(_src_loc);
	bt_dwarf_die_destroy(die);
	return -1;
}

/**
 * Lookup the source location for a given address within a CU,
 * assuming that it is contained within an inlined function.
 *
 * A source location can be found regardless of inlining status for
 * this method, but in the case of an inlined function, the returned
 * source location will point not to the callsite but rather to the
 * definition site of the inline function.
 *
 * @param cu		bt_dwarf_cu instance in which to look for the address
 * @param addr		The address for which to look for
 * @param src_loc	Out parameter, the source location (filename and
 *			line number) for the address
 * @returns		0 on success, -1 on failure
 */
static
int so_info_lookup_cu_src_loc_no_inl(struct bt_dwarf_cu *cu, uint64_t addr,
		struct source_location **src_loc)
{
	struct source_location *_src_loc = NULL;
	struct bt_dwarf_die *die = NULL;
	const char *filename = NULL;
	Dwarf_Line *line = NULL;
	Dwarf_Addr line_addr;
	int ret, line_no;

	if (!cu || !src_loc) {
		goto error;
	}

	die = bt_dwarf_die_create(cu);
	if (!die) {
		goto error;
	}

	line = dwarf_getsrc_die(die->dwarf_die, addr);
	if (!line) {
		goto error;
	}

	ret = dwarf_lineaddr(line, &line_addr);
	if (ret) {
		goto error;
	}

	filename = dwarf_linesrc(line, NULL, NULL);
	if (!filename) {
		goto error;
	}

	if (addr == line_addr) {
		_src_loc = g_new0(struct source_location, 1);
		if (!_src_loc) {
			goto error;
		}

		ret = dwarf_lineno(line, &line_no);
		if (ret) {
			goto error;
		}

		_src_loc->line_no = line_no;
		_src_loc->filename = strdup(filename);
	}

	bt_dwarf_die_destroy(die);

	if (_src_loc) {
		*src_loc = _src_loc;
	}

	return 0;

error:
	source_location_destroy(_src_loc);
	bt_dwarf_die_destroy(die);
	return -1;
}

/**
 * Get the source location (file name and line number) for a given
 * address within a compile unit (CU).
 *
 * On success, the out parameter `src_loc` is set if found. On
 * failure, it remains unchanged.
 *
 * @param so		bt_dwarf_cu instance for the compile unit which
 *			may contain the address
 * @param addr		Virtual memory address for which to find the
 *			source location
 * @param src_loc	Out parameter, the source location
 * @returns		0 on success, -1 on failure
 */
static
int so_info_lookup_cu_src_loc(struct bt_dwarf_cu *cu, uint64_t addr,
		struct source_location **src_loc)
{
	int ret = 0;
	struct source_location *_src_loc = NULL;

	if (!cu || !src_loc) {
		goto error;
	}

	ret = so_info_lookup_cu_src_loc_inl(cu, addr, &_src_loc);
	if (ret) {
		goto error;
	}

	if (_src_loc) {
		goto end;
	}

	ret = so_info_lookup_cu_src_loc_no_inl(cu, addr, &_src_loc);
	if (ret) {
		goto error;
	}

	if (_src_loc) {
		goto end;
	}

end:
	if (_src_loc) {
		*src_loc = _src_loc;
	}

	return 0;

error:
	source_location_destroy(_src_loc);
	return -1;
}

BT_HIDDEN
int so_info_lookup_source_location(struct so_info *so, uint64_t addr,
		struct source_location **src_loc)
{
	struct bt_dwarf_cu *cu = NULL;
	struct source_location *_src_loc = NULL;

	if (!so || !src_loc) {
		goto error;
	}

	/* Set DWARF info if it hasn't been accessed yet. */
	if (!so->dwarf_info && !so->is_elf_only) {
		if (so_info_set_dwarf_info(so)) {
			/* Failed to set DWARF info. */
			so->is_elf_only = true;
		}
	}

	if (so->is_elf_only) {
		/* We cannot lookup source location without DWARF info. */
		goto error;
	}

	if (!so_info_has_address(so, addr)) {
		goto error;
	}

	/*
	 * Addresses in ELF and DWARF are relative to base address for
	 * PIC, so make the address argument relative too if needed.
	 */
	if (so->is_pic) {
		addr -= so->low_addr;
	}

	cu = bt_dwarf_cu_create(so->dwarf_info);
	if (!cu) {
		goto error;
	}

	while (bt_dwarf_cu_next(cu) == 0) {
		int ret;

		ret = so_info_lookup_cu_src_loc(cu, addr, &_src_loc);
		if (ret) {
			goto error;
		}

		if (_src_loc) {
			break;
		}
	}

	bt_dwarf_cu_destroy(cu);
	if (_src_loc) {
		*src_loc = _src_loc;
	}

	return 0;

error:
	source_location_destroy(_src_loc);
	bt_dwarf_cu_destroy(cu);
	return -1;
}
